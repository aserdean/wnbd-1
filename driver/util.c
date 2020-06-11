/*
 * Copyright (c) 2019 SUSE LLC
 *
 * Licensed under LGPL-2.1 (see LICENSE)
 */

#include <berkeley.h>
#include "common.h"
#include "debug.h"
#include "rbd_protocol.h"
#include "scsi_driver_extensions.h"
#include "scsi_trace.h"
#include "srb_helper.h"
#include "userspace.h"
#include "util.h"

VOID
WnbdDeleteScsiInformation(_In_ PVOID ScsiInformation)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(ScsiInformation);
    PSCSI_DEVICE_INFORMATION ScsiInfo = (PSCSI_DEVICE_INFORMATION)ScsiInformation;
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(&ScsiInfo->GlobalInformation->ConnectionMutex, TRUE);

    PLIST_ENTRY Request;
    PSRB_QUEUE_ELEMENT Element;

    while ((Request = ExInterlockedRemoveHeadList(&ScsiInfo->RequestListHead, &ScsiInfo->RequestListLock)) != NULL) {
        Element = CONTAINING_RECORD(Request, SRB_QUEUE_ELEMENT, Link);
        Element->Srb->DataTransferLength = 0;
        Element->Srb->SrbStatus = SRB_STATUS_ABORTED;
        PWNBD_SCSI_DEVICE Device = (PWNBD_SCSI_DEVICE)ScsiInfo->Device;
        InterlockedDecrement(&Device->OutstandingIoCount);
        WNBD_LOG_INFO("Notifying StorPort of completion of %p status: 0x%x(%s)",
            Element->Srb, Element->Srb->SrbStatus, WnbdToStringSrbStatus(Element->Srb->SrbStatus));
        StorPortNotification(RequestComplete, Element->DeviceExtension, Element->Srb);
        ExFreePool(Element);
    }

    if(ScsiInfo->InquiryData) {
        ExFreePool(ScsiInfo->InquiryData);
        ScsiInfo->InquiryData = NULL;
    }

    if(ScsiInfo->UserEntry) {
        ExFreePool(ScsiInfo->UserEntry);
        ScsiInfo->UserEntry = NULL;
    }

    if (ScsiInfo->ReadPreallocatedBuffer) {
        ExFreePool(ScsiInfo->ReadPreallocatedBuffer);
        ScsiInfo->ReadPreallocatedBuffer = NULL;
    }

    if (ScsiInfo->WritePreallocatedBuffer) {
        ExFreePool(ScsiInfo->WritePreallocatedBuffer);
        ScsiInfo->WritePreallocatedBuffer = NULL;
    }

    if (-1 != ScsiInfo->Socket) {
        WNBD_LOG_INFO("Closing socket FD: %d", ScsiInfo->Socket);
        Close(ScsiInfo->Socket);
        ScsiInfo->Socket = -1;
    }

    ExReleaseResourceLite(&ScsiInfo->GlobalInformation->ConnectionMutex);
    KeLeaveCriticalRegion();

    ExFreePool(ScsiInfo);
    ScsiInfo = NULL;

    WNBD_LOG_LOUD(": Exit");
}

VOID
WnbdDeleteDevices(_In_ PWNBD_EXTENSION Ext,
                  _In_ BOOLEAN All)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(Ext);
    PWNBD_SCSI_DEVICE Device = NULL;
    PLIST_ENTRY Link, Next;
    KeEnterCriticalRegion();
    ExAcquireResourceSharedLite(&Ext->DeviceResourceLock, TRUE);
    ExAcquireResourceExclusiveLite(&((PGLOBAL_INFORMATION)Ext->GlobalInformation)->ConnectionMutex, TRUE);
    LIST_FORALL_SAFE(&Ext->DeviceList, Link, Next) {
        Device = (PWNBD_SCSI_DEVICE)CONTAINING_RECORD(Link, WNBD_SCSI_DEVICE, ListEntry);
        if (Device->ReportedMissing || All) {
            WNBD_LOG_INFO("Deleting device %p with %d:%d:%d",
                Device, Device->PathId, Device->TargetId, Device->Lun);
            PSCSI_DEVICE_INFORMATION Info = (PSCSI_DEVICE_INFORMATION)Device->ScsiDeviceExtension;
            WnbdDeleteConnection((PGLOBAL_INFORMATION)Ext->GlobalInformation,
                                 &Info->UserEntry->UserInformation);
            RemoveEntryList(&Device->ListEntry);
            WnbdDeleteScsiInformation(Device->ScsiDeviceExtension);
            ExFreePool(Device);
            Device = NULL;
            if (FALSE == All) {
                break;
            }
        }
    }
    ExReleaseResourceLite(&((PGLOBAL_INFORMATION)Ext->GlobalInformation)->ConnectionMutex);
    KeLeaveCriticalRegion();
    ExReleaseResourceLite(&Ext->DeviceResourceLock);

    WNBD_LOG_INFO("Request to exit DeleteDevicesThreadStart");

    WNBD_LOG_LOUD(": Exit");
}

VOID
WnbdDeviceCleanerThread(_In_ PVOID Context)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(Context);
    PWNBD_EXTENSION Ext = (PWNBD_EXTENSION)Context;

    while (TRUE) {
        KeWaitForSingleObject(&Ext->DeviceCleanerEvent, Executive, KernelMode, FALSE, NULL);

        if (Ext->StopDeviceCleaner) {
            WNBD_LOG_INFO("Terminating Device Cleaner");
            WnbdDeleteDevices(Ext, TRUE);
            break;
        }

        WnbdDeleteDevices(Ext, FALSE);
    }

    WNBD_LOG_LOUD(": Exit");

    (void)PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID
WnbdReportMissingDevice(_In_ PWNBD_EXTENSION DeviceExtension,
                        _In_ PWNBD_SCSI_DEVICE Device,
                        _In_ PWNBD_LU_EXTENSION LuExtension)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(Device);
    ASSERT(DeviceExtension);
    ASSERT(LuExtension);

    if (!Device->Missing) {
        LuExtension->WnbdScsiDevice = Device;
    } else {
        if (!Device->ReportedMissing) {
            WNBD_LOG_INFO(": Scheduling %p to be deleted and waking DeviceCleaner",
                          Device);
            Device->ReportedMissing = TRUE;
            KeSetEvent(&DeviceExtension->DeviceCleanerEvent, IO_DISK_INCREMENT, FALSE);
        }
        Device = NULL;
    }

    WNBD_LOG_LOUD(": Exit");
}

PWNBD_SCSI_DEVICE
WnbdFindDevice(_In_ PWNBD_LU_EXTENSION LuExtension,
               _In_ PWNBD_EXTENSION DeviceExtension,
               _In_ PSCSI_REQUEST_BLOCK Srb)
{
    WNBD_LOG_LOUD(": Entered");
    ASSERT(LuExtension);
    ASSERT(DeviceExtension);
    ASSERT(Srb);

    PWNBD_SCSI_DEVICE Device = NULL;

    for (PLIST_ENTRY Entry = DeviceExtension->DeviceList.Flink;
        Entry != &DeviceExtension->DeviceList; Entry = Entry->Flink) {

        Device = (PWNBD_SCSI_DEVICE) CONTAINING_RECORD(Entry, WNBD_SCSI_DEVICE, ListEntry);

        if (Device->PathId == Srb->PathId
            && Device->TargetId == Srb->TargetId
            && Device->Lun == Srb->Lun) {
            WnbdReportMissingDevice(DeviceExtension, Device, LuExtension);
            break;
        }
        Device = NULL;
    }

    WNBD_LOG_LOUD(": Exit");

    return Device;
}

NTSTATUS
WnbdProcessDeviceThreadRequestsReads(_In_ PSCSI_DEVICE_INFORMATION DeviceInformation,
                                     _In_ PSRB_QUEUE_ELEMENT Element)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(DeviceInformation);
    ASSERT(Element);
    NTSTATUS Status = STATUS_SUCCESS;

    NbdReadStat(DeviceInformation->Socket,
                Element->StartingLbn,
                Element->ReadLength,
                &Status,
                Element->Tag);

    KIRQL Irql = { 0 };
    KeAcquireSpinLock(&DeviceInformation->StatsLock, &Irql);
    DeviceInformation->Stats.UnsubmittedIORequests -= 1;
    DeviceInformation->Stats.TotalSubmittedIORequests += 1;
    DeviceInformation->Stats.PendingSubmittedIORequests += 1;
    KeReleaseSpinLock(&DeviceInformation->StatsLock, Irql);

    WNBD_LOG_LOUD(": Exit");
    return Status;
}

NTSTATUS
WnbdProcessDeviceThreadRequestsWrites(_In_ PSCSI_DEVICE_INFORMATION DeviceInformation,
                                      _In_ PSRB_QUEUE_ELEMENT Element)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(DeviceInformation);
    ASSERT(Element);
    ULONG StorResult;
    PVOID Buffer;
    NTSTATUS Status = STATUS_SUCCESS;

    StorResult = StorPortGetSystemAddress(Element->DeviceExtension, Element->Srb, &Buffer);
    if (STOR_STATUS_SUCCESS != StorResult) {
        Status = SRB_STATUS_INTERNAL_ERROR;
    } else {
        NbdWriteStat(DeviceInformation->Socket,
                     Element->StartingLbn,
                     Element->ReadLength,
                     &Status,
                     Buffer,
                     DeviceInformation->WritePreallocatedBuffer,
                     &DeviceInformation->WritePreallocatedBufferLength,
                     Element->Tag);
    }

    KIRQL Irql = { 0 };
    KeAcquireSpinLock(&DeviceInformation->StatsLock, &Irql);
    DeviceInformation->Stats.UnsubmittedIORequests -= 1;
    DeviceInformation->Stats.TotalSubmittedIORequests += 1;
    DeviceInformation->Stats.PendingSubmittedIORequests += 1;
    KeReleaseSpinLock(&DeviceInformation->StatsLock, Irql);

    WNBD_LOG_LOUD(": Exit");
    return Status;
}

VOID CloseConnection(_In_ PSCSI_DEVICE_INFORMATION DeviceInformation) {
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(
        &DeviceInformation->GlobalInformation->ConnectionMutex, TRUE);
    if (-1 != DeviceInformation->Socket) {
        WNBD_LOG_INFO("Closing socket FD: %d", DeviceInformation->Socket);
        Close(DeviceInformation->Socket);
        DeviceInformation->Socket = -1;
        DeviceInformation->Device->Missing = TRUE;
    }
    ExReleaseResourceLite(&DeviceInformation->GlobalInformation->ConnectionMutex);
    KeLeaveCriticalRegion();
}

VOID
WnbdProcessDeviceThreadRequests(_In_ PSCSI_DEVICE_INFORMATION DeviceInformation)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(DeviceInformation);

    PLIST_ENTRY Request;
    PSRB_QUEUE_ELEMENT Element;
    NTSTATUS Status = STATUS_SUCCESS;
    static UINT64 RequestTag = 0;

    while ((Request = ExInterlockedRemoveHeadList(
            &DeviceInformation->RequestListHead,
            &DeviceInformation->RequestListLock)) != NULL) {
        RequestTag += 1;
        Element = CONTAINING_RECORD(Request, SRB_QUEUE_ELEMENT, Link);
        Element->Tag = RequestTag;
        Element->Srb->DataTransferLength = 0;
        PCDB Cdb = (PCDB)&Element->Srb->Cdb;
        WNBD_LOG_INFO("Processing request. Address: %p Tag: 0x%llx",
                      Status, Element->Srb, Element->Tag);

        switch (Cdb->AsByte[0]) {
        case SCSIOP_READ6:
        case SCSIOP_READ:
        case SCSIOP_READ12:
        case SCSIOP_READ16:
            KeWaitForSingleObject(&DeviceInformation->RequestSemaphore, Executive, KernelMode, FALSE, NULL);
            if(DeviceInformation->SoftTerminateDevice || DeviceInformation->HardTerminateDevice) {
                return;
            }
            Status = WnbdProcessDeviceThreadRequestsReads(DeviceInformation, Element);
            break;

        case SCSIOP_WRITE6:
        case SCSIOP_WRITE:
        case SCSIOP_WRITE12:
        case SCSIOP_WRITE16:
            KeWaitForSingleObject(&DeviceInformation->RequestSemaphore, Executive, KernelMode, FALSE, NULL);
            if(DeviceInformation->SoftTerminateDevice || DeviceInformation->HardTerminateDevice) {
                return;
            }
            Status = WnbdProcessDeviceThreadRequestsWrites(DeviceInformation, Element);
            break;

        case SCSIOP_SYNCHRONIZE_CACHE:
        case SCSIOP_SYNCHRONIZE_CACHE16:
            /*
             * We just want to mark synchronize as been successful
             */
            Element->Srb->DataTransferLength = 0;
            Element->Srb->SrbStatus = SRB_STATUS_SUCCESS;
            Status = STATUS_SUCCESS;
            goto skip;
            break;

        default:
            Status = STATUS_DRIVER_INTERNAL_ERROR;
            break;
        }

        if (STATUS_SUCCESS == Status) {
            ExInterlockedInsertTailList(
                &DeviceInformation->ReplyListHead,
                &Element->Link, &DeviceInformation->ReplyListLock);
            WNBD_LOG_LOUD("Pending request. Address: %p Tag: 0x%llx",
                          Element->Srb, Element->Tag);
            KeReleaseSemaphore(&DeviceInformation->DeviceEventReply, 0, 1, FALSE);
        } else {
            Element->Srb->DataTransferLength = 0;
            Element->Srb->SrbStatus = SRB_STATUS_TIMEOUT;
            WNBD_LOG_INFO("FD failed with: %x. Address: %p Tag: 0x%llx",
                          Status, Element->Srb, Element->Tag);
            if (STATUS_CONNECTION_RESET == Status ||
                STATUS_CONNECTION_DISCONNECTED == Status ||
                STATUS_CONNECTION_ABORTED == Status) {
                CloseConnection(DeviceInformation);
            }
// FIXME: ugly
skip:
            StorPortNotification(RequestComplete, Element->DeviceExtension, Element->Srb);
            ExFreePool(Element);
        }
    }

    WNBD_LOG_LOUD(": Exit");
}

VOID
WnbdDeviceRequestThread(_In_ PVOID Context)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(Context);
    
    PSCSI_DEVICE_INFORMATION DeviceInformation;
    PAGED_CODE();

    DeviceInformation = (PSCSI_DEVICE_INFORMATION) Context;

    KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

    while (TRUE) {
        KeWaitForSingleObject(&DeviceInformation->DeviceEvent, Executive, KernelMode, FALSE, NULL);

        if (DeviceInformation->HardTerminateDevice) {
            WNBD_LOG_INFO("Hard terminate thread: %p", DeviceInformation);
            PsTerminateSystemThread(STATUS_SUCCESS);
        }

        WnbdProcessDeviceThreadRequests(DeviceInformation);

        // TODO: why do we have two separate flags? Those seem to be the same.
        if (DeviceInformation->SoftTerminateDevice) {
            WNBD_LOG_INFO("Soft terminate thread: %p", DeviceInformation);
            PsTerminateSystemThread(STATUS_SUCCESS);
        }
    }
}

VOID
WnbdDeviceReplyThread(_In_ PVOID Context)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(Context);

    PSCSI_DEVICE_INFORMATION DeviceInformation;
    PAGED_CODE();

    DeviceInformation = (PSCSI_DEVICE_INFORMATION) Context;

    KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

    while (TRUE) {

        if (DeviceInformation->HardTerminateDevice) {
            WNBD_LOG_INFO("Hard terminate thread: %p", DeviceInformation);
            PsTerminateSystemThread(STATUS_SUCCESS);
        }

        WnbdProcessDeviceThreadReplies(DeviceInformation);

        if (DeviceInformation->SoftTerminateDevice) {
            WNBD_LOG_INFO("Soft terminate thread: %p", DeviceInformation);
            PsTerminateSystemThread(STATUS_SUCCESS);
        }
    }
}

inline BOOLEAN
IsReadSrb(PSCSI_REQUEST_BLOCK Srb)
{
    PCDB Cdb = SrbGetCdb(Srb);
    if(!Cdb) {
        return FALSE;
    }
    
    switch (Cdb->AsByte[0]) {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        return TRUE;
    default:
        return FALSE;
    }
}

VOID
WnbdProcessDeviceThreadReplies(_In_ PSCSI_DEVICE_INFORMATION DeviceInformation)
{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(DeviceInformation);

    PSRB_QUEUE_ELEMENT Element = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    NBD_REPLY Reply = { 0 };
    PVOID SrbBuff = NULL, TempBuff = NULL;
    NTSTATUS error = STATUS_SUCCESS;
    /* Check if the reply list is empty before trying anything else */

    /*if (-1 == DeviceInformation->Socket || IsListEmpty(&DeviceInformation->ReplyListHead)) {
        return;
    }*/
    Status = NbdReadReply(DeviceInformation->Socket, &Reply);
    if (Status) {
        CloseConnection(DeviceInformation);
        return;
    }
    // TODO: check how can we avoid the situation in which we're looping over
    // list entries at the same time as the "Drain" function. Should we use
    // KeEnterCriticalRegion or an additional spin lock or mutex?
    // Other elements can be inserted, we need a lock to perform the lookup
    PLIST_ENTRY ItemLink, ItemNext;
    KIRQL Irql = { 0 };
    /* We need this event to make sure we have something in the queue */
    /* */
    // TODO: is it ok if this blocks while we're trying to reset the device?
    KeWaitForSingleObject(&DeviceInformation->DeviceEventReply, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&DeviceInformation->ReplyListLock, &Irql);
    LIST_FORALL_SAFE(&DeviceInformation->ReplyListHead, ItemLink, ItemNext) {
        Element = CONTAINING_RECORD(ItemLink, SRB_QUEUE_ELEMENT, Link);
        if (Element->Tag == Reply.Handle) {
            /* Remove the element from the list once found*/
            RemoveEntryList(&Element->Link);
            break;
        }
        Element = NULL;
    }
    KeReleaseSpinLock(&DeviceInformation->ReplyListLock, Irql);
    if(!Element) {
        WNBD_LOG_ERROR("Received reply with no matching request tag: 0x%llx",
            Reply.Handle);
        CloseConnection(DeviceInformation);
        goto Exit;
    }
    WNBD_LOG_LOUD("Received reply header for %p 0x%llx.", Element->Srb, Element->Tag);

    // TODO: can we use this buffer directly?
    // No, because of STOR_MAP_NON_READ_WRITE_BUFFERS
    ULONG StorResult;
    StorResult = StorPortGetSystemAddress(Element->DeviceExtension, Element->Srb, &SrbBuff);
    if (STOR_STATUS_SUCCESS != StorResult) {
        WNBD_LOG_ERROR("Could not get SRB %p 0x%llx data buffer. Error: %d.",
                       Element->Srb, Element->Tag, error);
        Element->Srb->SrbStatus = SRB_STATUS_INTERNAL_ERROR;
        CloseConnection(DeviceInformation);
        goto Exit;
    }

    if(IsReadSrb(Element->Srb)) {
        // Preallocate the buffer?
        if (Element->ReadLength > DeviceInformation->ReadPreallocatedBufferLength) {
            TempBuff = NbdMalloc(Element->ReadLength);
            if (!TempBuff) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                CloseConnection(DeviceInformation);
                goto Exit;
            }
            DeviceInformation->ReadPreallocatedBufferLength = Element->ReadLength;
            ExFreePool(DeviceInformation->ReadPreallocatedBuffer);
            DeviceInformation->ReadPreallocatedBuffer = TempBuff;
        } else {
            TempBuff = DeviceInformation->ReadPreallocatedBuffer;
        }

        if (-1 == RbdReadExact(DeviceInformation->Socket, TempBuff, Element->ReadLength, &error)) {
            WNBD_LOG_ERROR("Failed receiving reply %p 0x%llx. Error: %d",
                           Element->Srb, Element->Tag, error);
            Element->Srb->DataTransferLength = 0;
            Element->Srb->SrbStatus = SRB_STATUS_INTERNAL_ERROR;
            CloseConnection(DeviceInformation);
            goto Exit;
        } else {
            RtlCopyMemory(SrbBuff, TempBuff, Element->ReadLength);
        }
    }
    // TODO: rename ReadLength to DataLength
    // TODO: check NBD error code.
    Element->Srb->DataTransferLength = Element->ReadLength;
    Element->Srb->SrbStatus = SRB_STATUS_SUCCESS;

    KeAcquireSpinLock(&DeviceInformation->StatsLock, &Irql);
    DeviceInformation->Stats.TotalReceivedIOReplies += 1;
    DeviceInformation->Stats.PendingSubmittedIORequests -= 1;
    if(Element->Aborted) {
        WNBD_LOG_WARN("Got reply for aborted request: %p 0x%llx.",
                      Element->Srb, Element->Tag);
        DeviceInformation->Stats.CompletedAbortedIORequests += 1;
    }
    else {
        WNBD_LOG_LOUD("Successfully completed request %p 0x%llx.",
                      Element->Srb, Element->Tag);
        KeReleaseSemaphore(&DeviceInformation->RequestSemaphore, 0, 1, FALSE);
    }
    KeReleaseSpinLock(&DeviceInformation->StatsLock, Irql);

Exit:
    /*if(TempBuff) {
        NbdFree(TempBuff);
    }*/
    InterlockedDecrement(&DeviceInformation->Device->OutstandingIoCount);
    if (Element) {
        if(!Element->Aborted) {
            WNBD_LOG_INFO("Notifying StorPort of completion of %p status: 0x%x(%s)",
                Element->Srb, Element->Srb->SrbStatus,
                WnbdToStringSrbStatus(Element->Srb->SrbStatus));
            StorPortNotification(RequestComplete, Element->DeviceExtension,
                                 Element->Srb);
        }
        ExFreePool(Element);
    }
}
