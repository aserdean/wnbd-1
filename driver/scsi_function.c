/*
 * Copyright (c) 2019 SUSE LLC
 *
 * Licensed under LGPL-2.1 (see LICENSE)
 */

#include "common.h"
#include "debug.h"
#include "scsi_driver_extensions.h"
#include "scsi_operation.h"
#include "scsi_trace.h"
#include "srb_helper.h"
#include "scsi_function.h"
#include "util.h"
#include "userspace.h"

VOID DrainDeviceQueue(PWNBD_SCSI_DEVICE Device, PLIST_ENTRY ListHead,
                      PKSPIN_LOCK ListLock, PSCSI_DEVICE_INFORMATION DeviceInformation)
{
    WNBD_LOG_LOUD(": Enter");

    PLIST_ENTRY Request;
    PSRB_QUEUE_ELEMENT Element;

    while ((Request = ExInterlockedRemoveHeadList(ListHead, ListLock)) != NULL) {
        Element = CONTAINING_RECORD(Request, SRB_QUEUE_ELEMENT, Link);

        Element->Srb->DataTransferLength = 0;
        Element->Srb->SrbStatus = SRB_STATUS_ABORTED;
        Element->Aborted = 1;

        InterlockedDecrement(&Device->OutstandingIoCount);
        WNBD_LOG_INFO("Notifying StorPort of completion of %p 0x%llx status: 0x%x(%s)",
            Element->Srb, Element->Tag, Element->Srb->SrbStatus,
            WnbdToStringSrbStatus(Element->Srb->SrbStatus));
        StorPortNotification(RequestComplete, Element->DeviceExtension,
                             Element->Srb);
        ExFreePool(Element);

        KIRQL Irql = { 0 };
        KeAcquireSpinLock(&DeviceInformation->StatsLock, &Irql);
        DeviceInformation->Stats.AbortedUnsubmittedIORequests += 1;
        KeReleaseSpinLock(&DeviceInformation->StatsLock, Irql);
    }
}


VOID SendAbortFailedForQueue(PLIST_ENTRY ListHead, PKSPIN_LOCK ListLock,
                             PSCSI_DEVICE_INFORMATION DeviceInformation)
{
    WNBD_LOG_LOUD(": Enter");

    PSRB_QUEUE_ELEMENT Element;
    PLIST_ENTRY ItemLink, ItemNext;
    KIRQL Irql = { 0 };

    KeAcquireSpinLock(ListLock, &Irql);
    LIST_FORALL_SAFE(ListHead, ItemLink, ItemNext) {
        Element = CONTAINING_RECORD(ItemLink, SRB_QUEUE_ELEMENT, Link);

        Element->Srb->DataTransferLength = 0;
        Element->Srb->SrbStatus = SRB_STATUS_ABORTED;
        Element->Aborted = 1;

        WNBD_LOG_INFO("Notifying StorPort of completion of %p 0x%llx status: 0x%x(%s)",
            Element->Srb, Element->Tag, Element->Srb->SrbStatus,
            WnbdToStringSrbStatus(Element->Srb->SrbStatus));
        StorPortNotification(RequestComplete, Element->DeviceExtension,
                             Element->Srb);

        KIRQL Irql2 = { 0 };
        KeAcquireSpinLock(&DeviceInformation->StatsLock, &Irql2);
        DeviceInformation->Stats.AbortedSubmittedIORequests += 1;
        KeReleaseSpinLock(&DeviceInformation->StatsLock, Irql2);
    }
    KeReleaseSpinLock(ListLock, Irql);
}

UCHAR DrainDeviceQueues(PVOID DeviceExtension,
                        PSCSI_REQUEST_BLOCK Srb)

{
    WNBD_LOG_LOUD(": Enter");
    ASSERT(Srb);
    ASSERT(DeviceExtension);

    UCHAR SrbStatus = SRB_STATUS_NO_DEVICE;
    PWNBD_SCSI_DEVICE Device;
    PWNBD_LU_EXTENSION LuExtension;
    KIRQL Irql;
    KeAcquireSpinLock(&((PWNBD_EXTENSION)DeviceExtension)->DeviceListLock, &Irql);

    WNBD_LOG_INFO(": Received %s command. SRB = 0x%p. CDB = 0x%x. PathId: %d TargetId: %d LUN: %d",
        WnbdToStringSrbCdbOperation(SrbGetCdb(Srb)->AsByte[0]),
        Srb, SrbGetCdb(Srb)->AsByte[0], Srb->PathId, Srb->TargetId, Srb->Lun);

    LuExtension = (PWNBD_LU_EXTENSION)
        StorPortGetLogicalUnit(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun);

    if (!LuExtension) {
        WNBD_LOG_ERROR(": Unable to get LUN extension for device PathId: %d TargetId: %d LUN: %d",
            Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }

    Device = WnbdFindDevice(LuExtension, DeviceExtension, Srb);
    if (NULL == Device) {
        WNBD_LOG_INFO("Could not find device PathId: %d TargetId: %d LUN: %d",
            Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }

    if (NULL == Device->ScsiDeviceExtension) {
        WNBD_LOG_ERROR("%p has no ScsiDeviceExtension. PathId = %d. TargetId = %d. LUN = %d",
            Device, Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }
    if (Device->Missing) {
        WNBD_LOG_WARN("%p is marked for deletion. PathId = %d. TargetId = %d. LUN = %d",
            Device, Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }
    PSCSI_DEVICE_INFORMATION Info = (PSCSI_DEVICE_INFORMATION)Device->ScsiDeviceExtension;

    DrainDeviceQueue(Device, &Info->RequestListHead, &Info->RequestListLock, Info);
    // Should we set those in-flight requests to SRB_STATUS_ABORT_FAILED?
    // We can't set them to SRB_STATUS_ABORTED because those requests have been
    // submitted and will most probably complete.
    // DrainDeviceQueue(Device, &Info->ReplyListHead, &Info->ReplyListLock);
    SendAbortFailedForQueue(&Info->ReplyListHead, &Info->ReplyListLock, Info);

    SrbStatus = SRB_STATUS_SUCCESS;

Exit:
    KeReleaseSpinLock(&((PWNBD_EXTENSION)DeviceExtension)->DeviceListLock, Irql);

    WNBD_LOG_LOUD(": Exit");
    return SrbStatus;
}

_Use_decl_annotations_
UCHAR
WnbdAbortFunction(_In_ PVOID DeviceExtension,
                  _In_ PSCSI_REQUEST_BLOCK Srb)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(Srb);
    ASSERT(DeviceExtension);

    UCHAR SrbStatus = DrainDeviceQueues(DeviceExtension, Srb);

    WNBD_LOG_LOUD(": Exit");

    return SrbStatus;
}

_Use_decl_annotations_
UCHAR
WnbdResetLogicalUnitFunction(PVOID DeviceExtension,
                             PSCSI_REQUEST_BLOCK Srb)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(Srb);
    ASSERT(DeviceExtension);

    UCHAR SrbStatus = DrainDeviceQueues(DeviceExtension, Srb);

    WNBD_LOG_LOUD(": Exit");

    return SrbStatus;
}

_Use_decl_annotations_
UCHAR
WnbdResetDeviceFunction(PVOID DeviceExtension,
                        PSCSI_REQUEST_BLOCK  Srb)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(Srb);
    ASSERT(DeviceExtension);

    StorPortCompleteRequest(DeviceExtension,
                            Srb->PathId,
                            Srb->TargetId,
                            SP_UNTAGGED,
                            SRB_STATUS_TIMEOUT);

    WNBD_LOG_LOUD(": Exit");

    return SRB_STATUS_SUCCESS;
}

_Use_decl_annotations_
UCHAR
WnbdExecuteScsiFunction(PVOID DeviceExtension,
                        PSCSI_REQUEST_BLOCK Srb,
                        PBOOLEAN Complete)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(DeviceExtension);
    ASSERT(Srb);
    ASSERT(Complete);

    UCHAR SrbStatus = SRB_STATUS_NO_DEVICE;
    PWNBD_SCSI_DEVICE Device;
    PWNBD_LU_EXTENSION LuExtension;
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL Irql;
    KeAcquireSpinLock(&((PWNBD_EXTENSION)DeviceExtension)->DeviceListLock, &Irql);

    *Complete = TRUE;

    WNBD_LOG_INFO(": Received %s command. SRB = 0x%p. CDB = 0x%x. PathId: %d TargetId: %d LUN: %d",
                  WnbdToStringSrbCdbOperation(SrbGetCdb(Srb)->AsByte[0]),
                  Srb, SrbGetCdb(Srb)->AsByte[0], Srb->PathId, Srb->TargetId, Srb->Lun);

    LuExtension = (PWNBD_LU_EXTENSION)
        StorPortGetLogicalUnit(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun );

    if(!LuExtension) {
        WNBD_LOG_ERROR(": Unable to get LUN extension for device PathId: %d TargetId: %d LUN: %d",
                       Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }

    Device = WnbdFindDevice(LuExtension, DeviceExtension, Srb);
    if (NULL == Device) {
        WNBD_LOG_INFO("Could not find device PathId: %d TargetId: %d LUN: %d",
                      Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }

    if (NULL == Device->ScsiDeviceExtension) {
        WNBD_LOG_ERROR("%p has no ScsiDeviceExtension. PathId = %d. TargetId = %d. LUN = %d",
                       Device, Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }
    if (Device->Missing) {
        WNBD_LOG_WARN("%p is marked for deletion. PathId = %d. TargetId = %d. LUN = %d",
                      Device, Srb->PathId, Srb->TargetId, Srb->Lun);
        goto Exit;
    }

    InterlockedIncrement(&Device->OutstandingIoCount);
    Status = WnbdHandleSrbOperation(DeviceExtension, Device->ScsiDeviceExtension, Srb);

    if(STATUS_PENDING == Status) {
        *Complete = FALSE;
        SrbStatus = SRB_STATUS_PENDING;
    } else {
        InterlockedDecrement(&Device->OutstandingIoCount);
        SrbStatus = Srb->SrbStatus;
    }

Exit:
    KeReleaseSpinLock(&((PWNBD_EXTENSION)DeviceExtension)->DeviceListLock, Irql);
    WNBD_LOG_LOUD(": Exit");

    return SrbStatus;
}

_Use_decl_annotations_
UCHAR
WnbdPNPFunction(PSCSI_REQUEST_BLOCK Srb)
{
    WNBD_LOG_LOUD(": Enter");

    ASSERT(Srb);
    PSCSI_PNP_REQUEST_BLOCK PNP = (PSCSI_PNP_REQUEST_BLOCK) Srb;
    UCHAR SrbStatus = SRB_STATUS_INVALID_REQUEST;

    switch (PNP->PnPAction)
    {
    case StorQueryCapabilities:
        if (!(PNP->SrbPnPFlags & SRB_PNP_FLAGS_ADAPTER_REQUEST)) {
            PVOID DataBuffer = SrbGetDataBuffer(Srb);
            ASSERT(DataBuffer);

            PSTOR_DEVICE_CAPABILITIES_EX DeviceCapabilitiesEx = DataBuffer;
            RtlZeroMemory(DeviceCapabilitiesEx, sizeof(PSTOR_DEVICE_CAPABILITIES_EX));
            DeviceCapabilitiesEx->DefaultWriteCacheEnabled = 1;
            DeviceCapabilitiesEx->SilentInstall = 1;
            DeviceCapabilitiesEx->SurpriseRemovalOK = 1;
            DeviceCapabilitiesEx->Removable = 1;

            SrbStatus = SRB_STATUS_SUCCESS;
        }
        break;

    default:
        WNBD_LOG_WARN("Untreated SCSI Request PNP Flag: %x", PNP->SrbPnPFlags);
        break;
    }

    WNBD_LOG_LOUD(": Exit with SrbStatus: %s",
                  WnbdToStringSrbStatus(SrbStatus));

    return SrbStatus;
}
