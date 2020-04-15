/*
 * Copyright (c) 2019 SUSE LLC
 *
 * Licensed under LGPL-2.1 (see LICENSE)
 */

#ifndef DRIVER_H
#define DRIVER_H 1

#define MAX_NUMBER_OF_SCSI_TARGETS       128
#define MAX_NUMBER_OF_SCSI_LOGICAL_UNITS 1
#define MAX_NUMBER_OF_SCSI_BUSES         1

#define LOGICAL_BLOCK_SIZE 512

#define WNBD_INQUIRY_VENDOR_ID           "WNBD"
#define WNBD_INQUIRY_PRODUCT_ID          "v0.1"
#define WNBD_INQUIRY_PRODUCT_REVISION    "v0.1"
#define WNBD_INQUIRY_VENDOR_SPECIFIC     "WNBD_DISK_SPECIFIC_VENDOR_STRING"

_Use_decl_annotations_
BOOLEAN
WNBDReadRegistryValue(_In_ PUNICODE_STRING RegistryPath,
                      _In_ PWSTR Key,
                      _In_ ULONG Type,
                      _Out_ PVOID Value);

#endif
