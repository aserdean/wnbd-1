/*
 * Copyright (c) 2019 SUSE LLC
 *
 * Licensed under LGPL-2.1 (see LICENSE)
 */

#include "common.h"
#include "scsi_trace.h"

PCHAR SRB_FUNCTION_STRINGS[] = {
    (PCHAR)"SRB_FUNCTION_EXECUTE_SCSI           0x00",
    (PCHAR)"SRB_FUNCTION_CLAIM_DEVICE           0x01",
    (PCHAR)"SRB_FUNCTION_IO_CONTROL             0x02",
    (PCHAR)"SRB_FUNCTION_RECEIVE_EVENT          0x03",
    (PCHAR)"SRB_FUNCTION_RELEASE_QUEUE          0x04",
    (PCHAR)"SRB_FUNCTION_ATTACH_DEVICE          0x05",
    (PCHAR)"SRB_FUNCTION_RELEASE_DEVICE         0x06",
    (PCHAR)"SRB_FUNCTION_SHUTDOWN               0x07",
    (PCHAR)"SRB_FUNCTION_FLUSH                  0x08",
    (PCHAR)"SRB_FUNCTION_PROTOCOL_COMMAND       0x09",
    (PCHAR)"SRB_FUNCTION_ABORT_COMMAND          0x10",
    (PCHAR)"SRB_FUNCTION_RELEASE_RECOVERY       0x11",
    (PCHAR)"SRB_FUNCTION_RESET_BUS              0x12",
    (PCHAR)"SRB_FUNCTION_RESET_DEVICE           0x13",
    (PCHAR)"SRB_FUNCTION_TERMINATE_IO           0x14",
    (PCHAR)"SRB_FUNCTION_FLUSH_QUEUE            0x15",
    (PCHAR)"SRB_FUNCTION_REMOVE_DEVICE          0x16",
    (PCHAR)"SRB_FUNCTION_WMI                    0x17",
    (PCHAR)"SRB_FUNCTION_LOCK_QUEUE             0x18",
    (PCHAR)"SRB_FUNCTION_UNLOCK_QUEUE           0x19",
    (PCHAR)"SRB_FUNCTION_QUIESCE_DEVICE         0x1a",
    (PCHAR)"SRB_FUNCTION_RESET_LOGICAL_UNIT     0x20",
    (PCHAR)"SRB_FUNCTION_SET_LINK_TIMEOUT       0x21",
    (PCHAR)"SRB_FUNCTION_LINK_TIMEOUT_OCCURRED  0x22",
    (PCHAR)"SRB_FUNCTION_LINK_TIMEOUT_COMPLETE  0x23",
    (PCHAR)"SRB_FUNCTION_POWER                  0x24",
    (PCHAR)"SRB_FUNCTION_PNP                    0x25",
    (PCHAR)"SRB_FUNCTION_DUMP_POINTERS          0x26",
    (PCHAR)"SRB_FUNCTION_FREE_DUMP_POINTERS     0x27",
    (PCHAR)"SRB_FUNCTION_STORAGE_REQUEST_BLOCK  0x28"
};

PCHAR SRB_CDB_OPERATION_STRINGS[] = {
    // 6-byte commands:
    (PCHAR)"SCSIOP_TEST_UNIT_READY               0x00",
    (PCHAR)"SCSIOP_REZERO_UNIT                   0x01",
    (PCHAR)"SCSIOP_REWIND                        0x01",
    (PCHAR)"SCSIOP_REQUEST_BLOCK_ADDR            0x02",
    (PCHAR)"SCSIOP_REQUEST_SENSE                 0x03",
    (PCHAR)"SCSIOP_FORMAT_UNIT                   0x04",
    (PCHAR)"SCSIOP_READ_BLOCK_LIMITS             0x05",
    (PCHAR)"SCSIOP_REASSIGN_BLOCKS               0x07",
    (PCHAR)"SCSIOP_INIT_ELEMENT_STATUS           0x07",
    (PCHAR)"SCSIOP_READ6                         0x08",
    (PCHAR)"SCSIOP_RECEIVE                       0x08",
    (PCHAR)"SCSIOP_WRITE6                        0x0A",
    (PCHAR)"SCSIOP_PRINT                         0x0A",
    (PCHAR)"SCSIOP_SEND                          0x0A",
    (PCHAR)"SCSIOP_SEEK6                         0x0B",
    (PCHAR)"SCSIOP_TRACK_SELECT                  0x0B",
    (PCHAR)"SCSIOP_SLEW_PRINT                    0x0B",
    (PCHAR)"SCSIOP_SET_CAPACITY                  0x0B",
    (PCHAR)"SCSIOP_SEEK_BLOCK                    0x0C",
    (PCHAR)"SCSIOP_PARTITION                     0x0D",
    (PCHAR)"SCSIOP_READ_REVERSE                  0x0F",
    (PCHAR)"SCSIOP_WRITE_FILEMARKS               0x10",
    (PCHAR)"SCSIOP_FLUSH_BUFFER                  0x10",
    (PCHAR)"SCSIOP_SPACE                         0x11",
    (PCHAR)"SCSIOP_INQUIRY                       0x12",
    (PCHAR)"SCSIOP_VERIFY6                       0x13",
    (PCHAR)"SCSIOP_RECOVER_BUF_DATA              0x14",
    (PCHAR)"SCSIOP_MODE_SELECT                   0x15",
    (PCHAR)"SCSIOP_RESERVE_UNIT                  0x16",
    (PCHAR)"SCSIOP_RELEASE_UNIT                  0x17",
    (PCHAR)"SCSIOP_COPY                          0x18",
    (PCHAR)"SCSIOP_ERASE                         0x19",
    (PCHAR)"SCSIOP_MODE_SENSE                    0x1A",
    (PCHAR)"SCSIOP_START_STOP_UNIT               0x1B",
    (PCHAR)"SCSIOP_STOP_PRINT                    0x1B",
    (PCHAR)"SCSIOP_LOAD_UNLOAD                   0x1B",
    (PCHAR)"SCSIOP_RECEIVE_DIAGNOSTIC            0x1C",
    (PCHAR)"SCSIOP_SEND_DIAGNOSTIC               0x1D",
    (PCHAR)"SCSIOP_MEDIUM_REMOVAL                0x1E",
    // 10-byte commands
    (PCHAR)"SCSIOP_READ_FORMATTED_CAPACITY       0x23",
    (PCHAR)"SCSIOP_READ_CAPACITY                 0x25",
    (PCHAR)"SCSIOP_READ                          0x28",
    (PCHAR)"SCSIOP_WRITE                         0x2A",
    (PCHAR)"SCSIOP_SEEK                          0x2B",
    (PCHAR)"SCSIOP_LOCATE                        0x2B",
    (PCHAR)"SCSIOP_POSITION_TO_ELEMENT           0x2B",
    (PCHAR)"SCSIOP_WRITE_VERIFY                  0x2E",
    (PCHAR)"SCSIOP_VERIFY                        0x2F",
    (PCHAR)"SCSIOP_SEARCH_DATA_HIGH              0x30",
    (PCHAR)"SCSIOP_SEARCH_DATA_EQUAL             0x31",
    (PCHAR)"SCSIOP_SEARCH_DATA_LOW               0x32",
    (PCHAR)"SCSIOP_SET_LIMITS                    0x33",
    (PCHAR)"SCSIOP_READ_POSITION                 0x34",
    (PCHAR)"SCSIOP_SYNCHRONIZE_CACHE             0x35",
    (PCHAR)"SCSIOP_COMPARE                       0x39",
    (PCHAR)"SCSIOP_COPY_COMPARE                  0x3A",
    (PCHAR)"SCSIOP_WRITE_DATA_BUFF               0x3B",
    (PCHAR)"SCSIOP_READ_DATA_BUFF                0x3C",
    (PCHAR)"SCSIOP_WRITE_LONG                    0x3F",
    (PCHAR)"SCSIOP_CHANGE_DEFINITION             0x40",
    (PCHAR)"SCSIOP_WRITE_SAME                    0x41",
    (PCHAR)"SCSIOP_READ_SUB_CHANNEL              0x42",
    (PCHAR)"SCSIOP_UNMAP                         0x42",
    (PCHAR)"SCSIOP_READ_TOC                      0x43",
    (PCHAR)"SCSIOP_READ_HEADER                   0x44",
    (PCHAR)"SCSIOP_REPORT_DENSITY_SUPPORT        0x44",
    (PCHAR)"SCSIOP_PLAY_AUDIO                    0x45",
    (PCHAR)"SCSIOP_GET_CONFIGURATION             0x46",
    (PCHAR)"SCSIOP_PLAY_AUDIO_MSF                0x47",
    (PCHAR)"SCSIOP_PLAY_TRACK_INDEX              0x48",
    (PCHAR)"SCSIOP_SANITIZE                      0x48",
    (PCHAR)"SCSIOP_PLAY_TRACK_RELATIVE           0x49",
    (PCHAR)"SCSIOP_GET_EVENT_STATUS              0x4A",
    (PCHAR)"SCSIOP_PAUSE_RESUME                  0x4B",
    (PCHAR)"SCSIOP_LOG_SELECT                    0x4C",
    (PCHAR)"SCSIOP_LOG_SENSE                     0x4D",
    (PCHAR)"SCSIOP_STOP_PLAY_SCAN                0x4E",
    (PCHAR)"SCSIOP_XDWRITE                       0x50",
    (PCHAR)"SCSIOP_XPWRITE                       0x51",
    (PCHAR)"SCSIOP_READ_DISK_INFORMATION         0x51",
    (PCHAR)"SCSIOP_READ_DISC_INFORMATION         0x51",
    (PCHAR)"SCSIOP_READ_TRACK_INFORMATION        0x52",
    (PCHAR)"SCSIOP_XDWRITE_READ                  0x53",
    (PCHAR)"SCSIOP_RESERVE_TRACK_RZONE           0x53",
    (PCHAR)"SCSIOP_SEND_OPC_INFORMATION          0x54",
    (PCHAR)"SCSIOP_MODE_SELECT10                 0x55",
    (PCHAR)"SCSIOP_RESERVE_UNIT10                0x56",
    (PCHAR)"SCSIOP_RESERVE_ELEMENT               0x56",
    (PCHAR)"SCSIOP_RELEASE_UNIT10                0x57",
    (PCHAR)"SCSIOP_RELEASE_ELEMENT               0x57",
    (PCHAR)"SCSIOP_REPAIR_TRACK                  0x58",
    (PCHAR)"SCSIOP_MODE_SENSE10                  0x5A",
    (PCHAR)"SCSIOP_CLOSE_TRACK_SESSION           0x5B",
    (PCHAR)"SCSIOP_READ_BUFFER_CAPACITY          0x5C",
    (PCHAR)"SCSIOP_SEND_CUE_SHEET                0x5D",
    (PCHAR)"SCSIOP_PERSISTENT_RESERVE_IN         0x5E",
    (PCHAR)"SCSIOP_PERSISTENT_RESERVE_OUT        0x5F",
    // 12-byte commands
    (PCHAR)"SCSIOP_REPORT_LUNS                   0xA0",
    (PCHAR)"SCSIOP_BLANK                         0xA1",
    (PCHAR)"SCSIOP_ATA_PASSTHROUGH12             0xA1",
    (PCHAR)"SCSIOP_SEND_EVENT                    0xA2",
    (PCHAR)"SCSIOP_SECURITY_PROTOCOL_IN          0xA2",
    (PCHAR)"SCSIOP_SEND_KEY                      0xA3",
    (PCHAR)"SCSIOP_MAINTENANCE_IN                0xA3",
    (PCHAR)"SCSIOP_REPORT_KEY                    0xA4",
    (PCHAR)"SCSIOP_MAINTENANCE_OUT               0xA4",
    (PCHAR)"SCSIOP_MOVE_MEDIUM                   0xA5",
    (PCHAR)"SCSIOP_LOAD_UNLOAD_SLOT              0xA6",
    (PCHAR)"SCSIOP_EXCHANGE_MEDIUM               0xA6",
    (PCHAR)"SCSIOP_SET_READ_AHEAD                0xA7",
    (PCHAR)"SCSIOP_MOVE_MEDIUM_ATTACHED          0xA7",
    (PCHAR)"SCSIOP_READ12                        0xA8",
    (PCHAR)"SCSIOP_GET_MESSAGE                   0xA8",
    (PCHAR)"SCSIOP_SERVICE_ACTION_OUT12          0xA9",
    (PCHAR)"SCSIOP_WRITE12                       0xAA",
    (PCHAR)"SCSIOP_SEND_MESSAGE                  0xAB",
    (PCHAR)"SCSIOP_SERVICE_ACTION_IN12           0xAB",
    (PCHAR)"SCSIOP_GET_PERFORMANCE               0xAC",
    (PCHAR)"SCSIOP_READ_DVD_STRUCTURE            0xAD",
    (PCHAR)"SCSIOP_WRITE_VERIFY12                0xAE",
    (PCHAR)"SCSIOP_VERIFY12                      0xAF",
    (PCHAR)"SCSIOP_SEARCH_DATA_HIGH12            0xB0",
    (PCHAR)"SCSIOP_SEARCH_DATA_EQUAL12           0xB1",
    (PCHAR)"SCSIOP_SEARCH_DATA_LOW12             0xB2",
    (PCHAR)"SCSIOP_SET_LIMITS12                  0xB3",
    (PCHAR)"SCSIOP_READ_ELEMENT_STATUS_ATTACHED  0xB4",
    (PCHAR)"SCSIOP_REQUEST_VOL_ELEMENT           0xB5",
    (PCHAR)"SCSIOP_SECURITY_PROTOCOL_OUT         0xB5",
    (PCHAR)"SCSIOP_SEND_VOLUME_TAG               0xB6",
    (PCHAR)"SCSIOP_SET_STREAMING                 0xB6",
    (PCHAR)"SCSIOP_READ_DEFECT_DATA              0xB7",
    (PCHAR)"SCSIOP_READ_ELEMENT_STATUS           0xB8",
    (PCHAR)"SCSIOP_READ_CD_MSF                   0xB9",
    (PCHAR)"SCSIOP_SCAN_CD                       0xBA",
    (PCHAR)"SCSIOP_REDUNDANCY_GROUP_IN           0xBA",
    (PCHAR)"SCSIOP_SET_CD_SPEED                  0xBB",
    (PCHAR)"SCSIOP_REDUNDANCY_GROUP_OUT          0xBB",
    (PCHAR)"SCSIOP_PLAY_CD                       0xBC",
    (PCHAR)"SCSIOP_SPARE_IN                      0xBC",
    (PCHAR)"SCSIOP_MECHANISM_STATUS              0xBD",
    (PCHAR)"SCSIOP_SPARE_OUT                     0xBD",
    (PCHAR)"SCSIOP_READ_CD                       0xBE",
    (PCHAR)"SCSIOP_VOLUME_SET_IN                 0xBE",
    (PCHAR)"SCSIOP_SEND_DVD_STRUCTURE            0xBF",
    (PCHAR)"SCSIOP_VOLUME_SET_OUT                0xBF",
    (PCHAR)"SCSIOP_INIT_ELEMENT_RANGE            0xE7",
    // 16-byte commands
    (PCHAR)"SCSIOP_XDWRITE_EXTENDED16            0x80",
    (PCHAR)"SCSIOP_WRITE_FILEMARKS16             0x80",
    (PCHAR)"SCSIOP_REBUILD16                     0x81",
    (PCHAR)"SCSIOP_READ_REVERSE16                0x81",
    (PCHAR)"SCSIOP_REGENERATE16                  0x82",
    (PCHAR)"SCSIOP_EXTENDED_COPY                 0x83",
    (PCHAR)"SCSIOP_POPULATE_TOKEN                0x83",
    (PCHAR)"SCSIOP_WRITE_USING_TOKEN             0x83",
    (PCHAR)"SCSIOP_RECEIVE_COPY_RESULTS          0x84",
    (PCHAR)"SCSIOP_RECEIVE_ROD_TOKEN_INFORMATION 0x84",
    (PCHAR)"SCSIOP_ATA_PASSTHROUGH16             0x85",
    (PCHAR)"SCSIOP_ACCESS_CONTROL_IN             0x86",
    (PCHAR)"SCSIOP_ACCESS_CONTROL_OUT            0x87",
    (PCHAR)"SCSIOP_READ16                        0x88",
    (PCHAR)"SCSIOP_COMPARE_AND_WRITE             0x89",
    (PCHAR)"SCSIOP_WRITE16                       0x8A",
    (PCHAR)"SCSIOP_READ_ATTRIBUTES               0x8C",
    (PCHAR)"SCSIOP_WRITE_ATTRIBUTES              0x8D",
    (PCHAR)"SCSIOP_WRITE_VERIFY16                0x8E",
    (PCHAR)"SCSIOP_VERIFY16                      0x8F",
    (PCHAR)"SCSIOP_PREFETCH16                    0x90",
    (PCHAR)"SCSIOP_SYNCHRONIZE_CACHE16           0x91",
    (PCHAR)"SCSIOP_SPACE16                       0x91",
    (PCHAR)"SCSIOP_LOCK_UNLOCK_CACHE16           0x92",
    (PCHAR)"SCSIOP_LOCATE16                      0x92",
    (PCHAR)"SCSIOP_WRITE_SAME16                  0x93",
    (PCHAR)"SCSIOP_ERASE16                       0x93",
    (PCHAR)"SCSIOP_ZBC_OUT                       0x94",
    (PCHAR)"SCSIOP_ZBC_IN                        0x95",
    (PCHAR)"SCSIOP_READ_DATA_BUFF16              0x9B",
    (PCHAR)"SCSIOP_READ_CAPACITY16               0x9E",
    (PCHAR)"SCSIOP_GET_LBA_STATUS                0x9E",
    (PCHAR)"SCSIOP_GET_PHYSICAL_ELEMENT_STATUS   0x9E",
    (PCHAR)"SCSIOP_REMOVE_ELEMENT_AND_TRUNCATE   0x9E",
    (PCHAR)"SCSIOP_SERVICE_ACTION_IN16           0x9E",
    (PCHAR)"SCSIOP_SERVICE_ACTION_OUT16          0x9F",
    // 32-byte commands
    (PCHAR)"SCSIOP_OPERATION32                   0x7F"
};

PCHAR SRB_STATUS_STRINGS[] = {
    (PCHAR)"SRB_STATUS_PENDING                  0x00",
    (PCHAR)"SRB_STATUS_SUCCESS                  0x01",
    (PCHAR)"SRB_STATUS_ABORTED                  0x02",
    (PCHAR)"SRB_STATUS_ABORT_FAILED             0x03",
    (PCHAR)"SRB_STATUS_ERROR                    0x04",
    (PCHAR)"SRB_STATUS_BUSY                     0x05",
    (PCHAR)"SRB_STATUS_INVALID_REQUEST          0x06",
    (PCHAR)"SRB_STATUS_INVALID_PATH_ID          0x07",
    (PCHAR)"SRB_STATUS_NO_DEVICE                0x08",
    (PCHAR)"SRB_STATUS_TIMEOUT                  0x09",
    (PCHAR)"SRB_STATUS_SELECTION_TIMEOUT        0x0A",
    (PCHAR)"SRB_STATUS_COMMAND_TIMEOUT          0x0B",
    (PCHAR)"SRB_STATUS_MESSAGE_REJECTED         0x0D",
    (PCHAR)"SRB_STATUS_BUS_RESET                0x0E",
    (PCHAR)"SRB_STATUS_PARITY_ERROR             0x0F",
    (PCHAR)"SRB_STATUS_REQUEST_SENSE_FAILED     0x10",
    (PCHAR)"SRB_STATUS_NO_HBA                   0x11",
    (PCHAR)"SRB_STATUS_DATA_OVERRUN             0x12",
    (PCHAR)"SRB_STATUS_UNEXPECTED_BUS_FREE      0x13",
    (PCHAR)"SRB_STATUS_PHASE_SEQUENCE_FAILURE   0x14",
    (PCHAR)"SRB_STATUS_BAD_SRB_BLOCK_LENGTH     0x15",
    (PCHAR)"SRB_STATUS_REQUEST_FLUSHED          0x16",
    (PCHAR)"SRB_STATUS_INVALID_LUN              0x20",
    (PCHAR)"SRB_STATUS_INVALID_TARGET_ID        0x21",
    (PCHAR)"SRB_STATUS_BAD_FUNCTION             0x22",
    (PCHAR)"SRB_STATUS_ERROR_RECOVERY           0x23",
    (PCHAR)"SRB_STATUS_NOT_POWERED              0x24",
    (PCHAR)"SRB_STATUS_LINK_DOWN                0x25"
};

_Use_decl_annotations_
PCHAR
WnbdToStringSrbFunction(UCHAR SrbFunction)
{
    return SRB_FUNCTION_STRINGS[SrbFunction];
}

_Use_decl_annotations_
PCHAR
WnbdToStringSrbCdbOperation(UCHAR SrbOperation)
{
    return SRB_CDB_OPERATION_STRINGS[SrbOperation];
}

_Use_decl_annotations_
PCHAR
WnbdToStringSrbStatus(UCHAR SrbStatus)
{
    return SRB_STATUS_STRINGS[SrbStatus];
}
