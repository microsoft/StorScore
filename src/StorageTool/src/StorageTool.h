// StorScore
//
// Copyright (c) Microsoft Corporation
//
// All rights reserved. 
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

// SSD/HDD feature access utility.  Michael Xing 11/2016.

#pragma once

#include "stdafx.h"

//
// ATA related definitions
//

#define MAX_SECTORS                 (4)

#define ATA_BLOCK_SIZE              (512)

typedef struct _ATA_ACS_COMMAND {
    union {
        struct {
            UCHAR   Bits7_0;
            UCHAR   Bits15_8;
        };
        USHORT      AsUshort;
    } Feature;

    union {
        struct {
            UCHAR   Bits7_0;
            UCHAR   Bits15_8;
        };
        USHORT      AsUshort;
    } Count;

    ULONG   Reserved0;                          // padding ulong.

    union {
        struct {
            UCHAR   Bits7_0;
            UCHAR   Bits15_8;
            UCHAR   Bits23_16;
            UCHAR   Bits31_24;
            UCHAR   Bits39_32;
            UCHAR   Bits47_40;

            UCHAR   Reserved0;                  // padding byte.
            UCHAR   Reserved1;                  // padding byte.
        };
        ULONGLONG   AsUlonglong;
    } LBA;

    union {
        struct {
            UCHAR   Bits7_0;
            UCHAR   Bits15_8;
            UCHAR   Bits23_16;
            UCHAR   Bits31_24;
        };
        ULONG       AsULONG;
    } Auxiliary;

    UCHAR   ICC;

    union {
        struct {
            UCHAR   Lba27_24 : 4;      // only used for 28bits command
            UCHAR   DEV : 1;      // always be 0 for SATA device
            UCHAR   Obsolete0 : 1;
            UCHAR   LBA : 1;
            UCHAR   FUA : 1;      // FUA bit for Read/Write command.
        };
        UCHAR   AsByte;                 // Device register
    } Device;

    UCHAR   Command;

    UCHAR   Reserved1;                  // padding bytes.
} ATA_ACS_COMMAND, *PATA_ACS_COMMAND;


#pragma pack(push, ata_command, 1)
typedef struct _ATA_COMMAND {
    union {
        BYTE    bFeaturesReg;           // Current: Features7:0; Previous: Features15:8
        BYTE    Features;
    };

    union {
        BYTE    bSectorCountReg;        // Current: SectorCount7:0; Previous: SectorCount15:8
        BYTE    SectorCount;
    };

    union {
        BYTE    bSectorNumberReg;       // Current: LBA7:0;   Previous: LBA31:24
        BYTE    LbaLow;
    };

    union {
        BYTE    bCylLowReg;             // Current: LBA15:8;  Previous: LBA39:32
        BYTE    LbaMid;
    };

    union {
        BYTE    bCylHighReg;            // Current: LBA23:16; Previous: LBA47:40
        BYTE    LbaHigh;
    };

    union {
        BYTE    bDriveHeadReg;          // Device register
        struct {
            BYTE    Lba27_24 : 4;      // only used for 28bits command
            BYTE    DEV : 1;      // always be 0 for SATA device
            BYTE    Obsolete0 : 1;
            BYTE    LBA : 1;
            BYTE    Obsolete1 : 1;
        } Device;
    };

    union {
        BYTE    bCommandReg;            // Actual ATA command op code.
        BYTE    Command;
    };

    BYTE    Reserved;                   // reserved for future use.  Must be zero.
} ATA_COMMAND, *PATA_COMMAND;
#pragma pack (pop, ata_command)

typedef struct _ATA_PT {
    ATA_PASS_THROUGH_EX AtaPassThrough;
    UCHAR               Buffer[MAX_SECTORS * ATA_BLOCK_SIZE];
} ATA_PT, *PATA_PT;


//
// SCSI related definitions
//

#define SPT_SENSE_LENGTH        32
#define SPTWB_DATA_LENGTH       512

typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH ScsiPassThrough;
    UCHAR             SenseBuffer[SPT_SENSE_LENGTH];
    UCHAR             DataBuffer[SPTWB_DATA_LENGTH];
} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;

#pragma pack(push, vpd_extended_inquiry_data, 1)
typedef struct _VPD_EXTENDED_INQUIRY_DATA_PAGE {
    UCHAR DeviceType : 5;
    UCHAR DeviceTypeQualifier : 3;

    UCHAR PageCode;         // 86h
    UCHAR PageLength[2];    // [0] - 00h, [1] - 3Ch

    UCHAR RefChk : 1;       // byte 4 bit 0
    UCHAR AppChk : 1;
    UCHAR GrdChk : 1;
    UCHAR Spt : 3;
    UCHAR ActivateMicrocode : 2;

    UCHAR SimpSup : 1;      // byte 5 bit 0
    UCHAR OrdSup : 1;
    UCHAR HeadSup : 1;
    UCHAR PriorSup : 1;
    UCHAR GroupSup : 1;
    UCHAR UaskSup : 1;
    UCHAR Reserved0 : 2;

    UCHAR VSup : 1;         // byte 6 bit 0
    UCHAR NvSup : 1;
    UCHAR CrdSup : 1;
    UCHAR WuSup : 1;
    UCHAR Reserved1 : 4;

    UCHAR LuiClr : 1;       // byte 7 bit 0
    UCHAR Reserved2 : 3;
    UCHAR PiiSup : 1;
    UCHAR Reserved3 : 3;

    UCHAR Cbcs : 1;         // byte 8 bit 0
    UCHAR Reserved4 : 3;
    UCHAR RSup : 1;
    UCHAR Reserved5 : 3;

    UCHAR Multi_i_t_Nexus_Microcode_Download : 4;   // byte 9 bit 0
    UCHAR Reserved6 : 4;

    UCHAR ExtendedSelfTestCompletionMinutes[2];

    UCHAR Reserved7 : 5;    // byte 12 bit 0
    UCHAR VsaSup : 1;
    UCHAR HraSup : 1;
    UCHAR PoaSup : 1;

    UCHAR MaxSupportedSenseDataLength;

    UCHAR Reserved8[50];
} VPD_EXTENDED_INQUIRY_DATA_PAGE, *PVPD_EXTENDED_INQUIRY_DATA_PAGE;
#pragma pack(pop, vpd_extended_inquiry_data)


#define READ_BUFFER_MODE_DATA           0x02
#define READ_BUFFER_MODE_DESCRIPTOR     0x03

typedef struct _READ_BUFFER_CDB {
    UCHAR OperationCode;        // 0x3C SCSIOP_READ_DATA_BUFF
    UCHAR Mode : 5;
    UCHAR Reserved0 : 3;
    UCHAR BufferID;
    UCHAR BufferOffset[3];
    UCHAR AllocationLength[3];
    UCHAR Control;
} READ_BUFFER_CDB, *PREAD_BUFFER_CDB;

typedef struct _READ_BUFFER_DESCRIPTOR_DATA {
    UCHAR OffsetBoundary;
    UCHAR BufferCapacity[3];
} READ_BUFFER_DESCRIPTOR_DATA, *PREAD_BUFFER_DESCRIPTOR_DATA;


#define SERVICE_ACTION_REPORT_SUPPORTED_OPERATION_CODES         0x0C

#define REPORT_SUPPORTED_OPERATION_CODES_REPORTING_OPTIONS_ALL              0x0
#define REPORT_SUPPORTED_OPERATION_CODES_REPORTING_OPTIONS_OP_ALL           0x1
#define REPORT_SUPPORTED_OPERATION_CODES_REPORTING_OPTIONS_OP_SA            0x2
#define REPORT_SUPPORTED_OPERATION_CODES_REPORTING_OPTIONS_OP_SA_OVERWRITE  0x3

typedef struct _REPORT_SUPPORTED_OPERATION_CODES_CDB {
    UCHAR OperationCode;        // 0xA3 SCSIOP_MAINTENANCE_IN
    UCHAR ServiceAction : 5;    // 0x0C SERVICE_ACTION_REPORT_SUPPORTED_OPERATION_CODES
    UCHAR Reserved0 : 3;
    UCHAR ReportOptions : 3;
    UCHAR Reserved1 : 4;
    UCHAR RCTD : 1;
    UCHAR RequestedOperationCode;
    UCHAR RequestedServiceAction[2];
    UCHAR AllocationLength[4];
    UCHAR Reserved2;
    UCHAR Control;
} REPORT_SUPPORTED_OPERATION_CODES_CDB, *PREPORT_SUPPORTED_OPERATION_CODES_CDB;

#define REPORT_SUPPORTED_OPERATION_CODES_SUPPORT_NOT_AVAILABLE      0x0
#define REPORT_SUPPORTED_OPERATION_CODES_SUPPORT_NONE               0x1
#define REPORT_SUPPORTED_OPERATION_CODES_SUPPORT_SUPPORT_STANDARD   0x3
#define REPORT_SUPPORTED_OPERATION_CODES_SUPPORT_SUPPORT_VENDOR     0x5

typedef struct _ONE_COMMAND_PARAMATER_DATA {
    UCHAR Reserved0;
    UCHAR Support : 3;
    UCHAR Reserved1 : 4;
    UCHAR CTDP : 1;
    UCHAR CdbSize[2];
    UCHAR CdbUsageData[ANYSIZE_ARRAY];
    // Command timeouts descriptor, if any.
} ONE_COMMAND_PARAMATER_DATA, *PONE_COMMAND_PARAMATER_DATA;


//
// NVMe related definitions
//
#define NVME_MAX_LOG_SIZE           4096

#define NVME_LOG_PAGE_MSFT_HEALTH       0xC0
#define NVME_LOG_PAGE_MSFT_DEBUGGING1   0xC1
#define NVME_LOG_PAGE_MSFT_DEBUGGING2   0xC2

#define NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0       0x0000
#define NVME_LOG_PAGE_MSFT_HEALTH_VERSION_1       0x0001

//
// Information of log: NVME_LOG_PAGE_MSFT_HEALTH. Size: 512 bytes
//
typedef struct {

    UCHAR   MediaUnitsWritten[16];          // Contains the number of 512 byte data units written to the media; this value in-cludes metadata written to the non-out-of-band area in the media. This value is reported in thousands (i.e., a value of 1 corresponds to 1000 units of 512 bytes written) and is rounded up. When the LBA size is a value other than 512 bytes, the controller shall convert the amount of data written to 512 byte units.  
    UCHAR   CapacitorHealth;                // An indicator of the health of the capaci-tors (if present).  This shall be expressed as a percentage of charge the capacitor is able to hold.  If no capacitor exists, value shall be 255.
    UCHAR   ECCIterations[16];              // The number of reads performed on the flash as a result of error correction (e.g. Read retries, LDPC iterations, etc.)

    union {

        struct {
            UCHAR   SuperCapacitorExists : 1;
            UCHAR   Reserved : 7;
        } DUMMYSTRUCTNAME;

        UCHAR AsUchar;

    } SupportedFeatures;


    UCHAR   TemperatureThrottling[7];       // Tracks how much performance is throt-tled to prevent overheating.  The attrib-ute reports the number of dies multiplied by how long the dies are turned off (in minutes).  Resets when the drive power cycles.  Saturates and does not wrap.
    UCHAR   PowerConsumption;               // Current power consumption of NAND, Controller and other SSD components in Watts.  If the SSD does not have a mech-anism to measure power, it should re-turn 255.
    UCHAR   WearRangeDelta;                 // Returns the difference between the per-centage of used endurance of the most-worn block and the least worn block: (% used of most - worn) –(% used of least - worn)
    UCHAR   UnalignedIO[6];                 // Count of the number of unaligned IOs performed by the host.  This counter should be resettable and should not wrap.
    UCHAR   MappedLBAs[4];                  // Number of LBAs the map is tracking
    UCHAR   ProgramFailCount;               // The total number of error events on program
    UCHAR   EraseFailCount;                 // The total number of error events on erase.

    UCHAR   Reserved1[455];

    UCHAR   Version[2];

} NVME_HEALTH_INFO_MSFT_LOG_V0, *PNVME_HEALTH_INFO_MSFT_LOG_V0;

typedef struct {

    UCHAR   MediaUnitsWritten[16];          // Contains the number of 512 byte data units written to the media; this value in-cludes metadata written to the non-out-of-band area in the media. This value is reported in thousands (i.e., a value of 1 corresponds to 1000 units of 512 bytes written) and is rounded up. When the LBA size is a value other than 512 bytes, the controller shall convert the amount of data written to 512 byte units.  
    UCHAR   ECCIterations[16];              // The number of reads performed on the flash as a result of error correction (e.g. Read retries, LDPC iterations, etc.)

    UCHAR   CapacitorHealth;                // An indicator of the health of the capaci-tors (if present).  This shall be expressed as a percentage of charge the capacitor is able to hold.  If no capacitor exists, value shall be 255.

    union {

        struct {
            UCHAR   SuperCapacitorExists : 1;
            UCHAR   Reserved : 7;
        } DUMMYSTRUCTNAME;

        UCHAR AsUchar;

    } SupportedFeatures;

    UCHAR   PowerConsumption;               // Current power consumption of NAND, Controller and other SSD components in Watts.  If the SSD does not have a mech-anism to measure power, it should re-turn 255.
    UCHAR   WearRangeDelta;                 // Returns the difference between the per-centage of used endurance of the most-worn block and the least worn block: (% used of most - worn) –(% used of least - worn)

    UCHAR   Reserved0[4];

    UCHAR   TemperatureThrottling[8];       // Tracks how much performance is throt-tled to prevent overheating.  The attrib-ute reports the number of dies multiplied by how long the dies are turned off (in minutes).  Resets when the drive power cycles.  Saturates and does not wrap.
    UCHAR   UnalignedIO[8];                 // Count of the number of unaligned IOs performed by the host.  This counter should be resettable and should not wrap.
    UCHAR   MappedLBAs[4];                  // Number of LBAs the map is tracking
    UCHAR   ProgramFailCount[4];            // The total number of error events on program
    UCHAR   EraseFailCount[4];              // The total number of error events on erase.

    UCHAR   Reserved1[442];

    UCHAR   Version[2];

} NVME_HEALTH_INFO_MSFT_LOG_V1, *PNVME_HEALTH_INFO_MSFT_LOG_V1;


//
// Applicaiton related definitions
//

static const ULONG LogicalSectorSize = 512;

typedef union _ULONG128 {
    struct {
        ULONGLONG Low;
        ULONGLONG High;
    };
    UCHAR Byte[16];
} ULONG128, *PULONG128;

__inline
VOID
Uint128ShiftRight(
    PULONG128 Value,
    ULONG     Bits
)
{
    if (Bits < 64) {
        UINT64 tmp;
        tmp = Value->High << (64 - Bits);
        Value->High >>= Bits;
        Value->Low >>= Bits;
        Value->Low |= tmp;
    } else if (Bits < 128) {
        Value->Low = Value->High >> (Bits - 64);
        Value->High = 0;
    } else {
        Value->High = Value->Low = 0;
    }

    return;
}


#define ALIGN_DOWN_BY(length, alignment) \
            ((ULONG_PTR)(length) & ~(alignment - 1))


// Bus Type String. Corresponding to data structure STORAGE_BUS_TYPE.
static TCHAR* BusType[] = {
    _T("UNKNOWN"),  // 0x00
    _T("SCSI   "),
    _T("ATAPI  "),
    _T("ATA    "),
    _T("1394   "),
    _T("SSA    "),
    _T("FIBRE  "),
    _T("USB    "),
    _T("RAID   "),
    _T("iSCSI  "),
    _T("SAS    "),
    _T("SATA   "),
    _T("SD     "),
    _T("MMC    "),
    _T("VIRTUAL"),
    _T("FB-VIRT"),
    _T("SPACES "),
    _T("NVME   ")
};

static const GUID * DeviceGuids[] = {
    &GUID_DEVINTERFACE_DISK,
    &GUID_DEVINTERFACE_CDROM,
    &GUID_DEVCLASS_SCSIADAPTER,
    &GUID_DEVCLASS_HDC,
    &GUID_DEVCLASS_SBP2,
    &GUID_DEVCLASS_USB
};

typedef enum {
    DiskGuidIndex = 0,
    CdromGuidIndex = 1,
    ScsiAdapterGuidIndex = 2,
    HdcGuidIndex = 3,
    Sbp2GuidIndex = 4,
    UsbGuidIndex = 5,
} DEVICE_GUID_INDEX;

typedef struct _DEVICE_LIST {
    HANDLE                      Handle;
    ULONG                       DeviceNumber;
    SCSI_ADDRESS                ScsiId;
    STORAGE_ADAPTER_DESCRIPTOR  AdapterDescriptor;
    UCHAR                       DeviceDescriptor[1024];     // STORAGE_DEVICE_DESCRIPTOR is variable length structure.
    ULONG                       DeviceDescLength;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR DeviceAccessAlignmentDescriptor;
    //PSTORAGE_ZONED_DEVICE_DESCRIPTOR ZonedDeviceDescriptor; // STORAGE_ZONED_DEVICE_DESCRIPTOR is a variable length structure.
    ULONG                       ZonedDeviceDescriptorLength;
    BOOLEAN                     NoSeekPenalty;
    BOOLEAN                     SupportStorageFWIoctl;      // introduced in win10
    BOOLEAN                     SupportMiniportFWIoctl;     // introduced in win8.1
    UCHAR                       FirmwareRevision[17];

    struct {
        USHORT  Version;
        USHORT  DeviceStatisticsPageCount;
        USHORT  NcqCommandErrorPageCount;
        USHORT  PhyEventCounterPageCount;
        USHORT  NcqNonDataPageCount;
        USHORT  NcqSendReceivePageCount;
        USHORT  HybridInfoPageCount;
        USHORT  CurrentDeviceInteralPageCount;
        USHORT  SavedDeviceInternalPageCount;
        USHORT  IdentifyDeviceDataPageCount;
    } LogPage;

} DEVICE_LIST, *PDEVICE_LIST;

typedef struct _COMMAND_OPTIONS {

    struct {
        ULONG   LoggerEnabled : 1;
        ULONG   Reserved : 31;
    } State;

    struct {
        //
        // Utility Functions
        //
        ULONG   Detail : 1;
        ULONG   HealthInfo : 1;
        ULONG   LogPageInfo : 1;
        ULONG   AtaCommand : 1;
        ULONG   FirmwareInfo : 1;
        ULONG   FirmwareUpdate : 1;

        //ULONG   Reserved0 : 24;

        ULONG   Reserved;
    } Operation;

    struct {
        BOOLEAN     ForDisk;
        BOOLEAN     ForCdrom;
        UCHAR       Reserved0[2];

        ULONG       DeviceNumber;
        TCHAR*      DeviceInstance;
    } Target;

    struct {
        //
        // Common Parameters
        //
        LONGLONG    StartingOffset;   //must align to sector
        ULONGLONG   LengthInBytes;    //must be multiple of logical sector size

        TCHAR*      FileName;

        //
        // Command Specific Parameters
        //
        union {
            struct {
                UCHAR   SlotId;
                BOOLEAN ScsiMiniportIoctl;
            } Firmware;

            struct {
                ATA_ACS_COMMAND AtaAcsCommand;
            } AtaCmd;

            struct {
                UCHAR   Index;
                BOOLEAN UpdateOverThreshold;
                SHORT   Threshold;
            } Temperature;

            struct {
                BOOLEAN AdapterTopology;
            } Topology;

            struct {
                ULONG   LogValue;
                ULONG   LogSubValue;
            } ReadLog;
        } u;

    } Parameters;

} COMMAND_OPTIONS, *PCOMMAND_OPTIONS;


//
// Other functions
//

ULONG
DeviceGetCount(
    _In_ ULONG DeviceGuidIndex
    );

VOID
DeviceListBuild(
    _In_    ULONG           DeviceGuidIndex,
    _Inout_ PDEVICE_LIST    DeviceList,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_    PULONG          DeviceCount
    );

HANDLE
DeviceGetHandle(
    _In_ ULONG  DeviceGuidIndex,
    _In_ ULONG  DeviceNumber
    );

VOID
DeviceGetGeneralInfo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ ULONG              DeviceIndex,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_ BOOLEAN            GetDeviceNumber
    );

VOID
DeviceListGeneralInfo(
    _In_ ULONG        DeviceGuidIndex,
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    );

VOID
DeviceListFree(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceCount
    );


BOOL
DeviceGetMiniportFirmwareInfo(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        DeviceIndex,
    _Out_ PUCHAR*     Buffer,
    _Out_ DWORD*      BufferLength,
    _In_ BOOLEAN      DisplayResult
    );

BOOL
DeviceGetStorageFirmwareInfo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ ULONG              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _Out_ PUCHAR*           Buffer,
    _Out_ DWORD*            BufferLength,
    _In_ BOOLEAN            DisplayResult
    );

ULONG
DeviceHealthInfo(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    );

ULONG
DeviceLogPageInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    );

BOOL
DeviceValidateFirmwareUpgradeSupport(
    _In_ PSTORAGE_HW_FIRMWARE_INFO  FirmwareInfo,
    _In_ PCOMMAND_OPTIONS           CommandOptions
);

VOID
DeviceMiniportFirmwareUpgrade(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ ULONG              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions
    );

VOID
DeviceStorageFirmwareUpgrade(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ ULONG              DeviceIndex,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_ BOOLEAN            VerboseDisplay
    );


