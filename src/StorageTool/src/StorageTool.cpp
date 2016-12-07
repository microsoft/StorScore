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


// StorageTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "StorageTool.h"
#include "util.h"


__inline
VOID
PrintHelp(
    VOID
    )
{
    _tprintf(_T("\nUsage:\n")
             _T("\tStorageTool -<function> <Disk|Cdrom> <Device#> [<Additional Parameter>...] \n\n")

             _T("\tStorageTool -Detail <Disk|Cdrom> <device#>\n")
             _T("\tStorageTool -HealthInfo <Disk|Cdrom> <device#>\n")
             _T("\tStorageTool -LogPageInfo <Disk|Cdrom> <device#>\n")
             // Device Map
    );

    return;
}

BOOLEAN
ParseCommandLine(
    _In_ DWORD              ArgumentCount,
    _In_ TCHAR*             Arguments[],
    _Out_ PCOMMAND_OPTIONS  Options
)
/*++

Routine Description:

    Take and save parameters, validate if they are valid options.

Arguments:

    ArgumentCount -
    Arguments -
    Options -

Return Value:

    TRUE or FALSE

--*/
{
    BOOLEAN validArgument = TRUE;

    //
    // Parameter should at least have "Command", "Device Type" and "Device Number".
    //
    if (ArgumentCount < 3) {
        return FALSE;
    }

    //
    // Tolerate both '-' and '/'.
    //
    if ((Arguments[0][0] == _T('/')) || (Arguments[0][0] == _T('-'))) {
        Arguments[0][0] = _T('-');
    }

    if (CompareArgs(Arguments[1], _T("Disk"))) {
        Options->Target.ForDisk = TRUE;
    } else if (CompareArgs(Arguments[1], _T("Cdrom"))) {
        Options->Target.ForCdrom = TRUE;
    } else {
        validArgument = FALSE;
        return validArgument;
    }

    if (_stscanf_s(Arguments[2], _T("%d"), &Options->Target.DeviceNumber) != 1) {
        validArgument = FALSE;
        return validArgument;
    }

    if (CompareArgs(Arguments[0], _T("-Detail"))) {
        Options->Operation.Detail = TRUE;
    } else if (CompareArgs(Arguments[0], _T("-HealthInfo"))) {
        Options->Operation.HealthInfo = TRUE;
    } else if (CompareArgs(Arguments[0], _T("-LogPageInfo"))) {
        Options->Operation.LogPageInfo = TRUE;
    } else if (CompareArgs(Arguments[0], _T("-AtaCommand"))) {
        PATA_ACS_COMMAND ataAcsCommand = (PATA_ACS_COMMAND)&Options->Parameters.u.AtaCmd.AtaAcsCommand;

        Options->Operation.AtaCommand = TRUE;

        if (ArgumentCount < 10) {
            validArgument = FALSE;
        } else {
            if (_stscanf_s(Arguments[3], _T("%x"), (PULONG)&ataAcsCommand->Feature.AsUshort) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[4], _T("%x"), (PULONG)&ataAcsCommand->Count.AsUshort) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[5], _T("%x"), (PULONG)&ataAcsCommand->LBA.AsUlonglong) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[6], _T("%x"), (PULONG)&ataAcsCommand->Auxiliary.AsULONG) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[7], _T("%x"), (PULONG)&ataAcsCommand->ICC) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[8], _T("%x"), (PULONG)&ataAcsCommand->Device.AsByte) != 1) {
                validArgument = FALSE;
            } else if (_stscanf_s(Arguments[9], _T("%x"), (PULONG)&ataAcsCommand->Command) != 1) {
                validArgument = FALSE;
            }
        }
    } else if (CompareArgs(Arguments[0], _T("-FirmwareInfo"))) {
        Options->Operation.FirmwareInfo = TRUE;

        if (ArgumentCount >= 4) {
            if (CompareArgs(Arguments[3], _T("scsiminiport"))) {
                Options->Parameters.u.Firmware.ScsiMiniportIoctl = TRUE;
            }
        }
    } else if (CompareArgs(Arguments[0], _T("-FirmwareUpgrade"))) {
        Options->Operation.FirmwareUpdate = TRUE;

        if (_stscanf_s(Arguments[3], _T("%x"), (PULONG)&Options->Parameters.u.Firmware.SlotId) != 1) {
            validArgument = FALSE;
        } else {
            if (ArgumentCount >= 5) {
                Options->Parameters.FileName = Arguments[4];

                if (ArgumentCount >= 6) {
                    if (CompareArgs(Arguments[5], _T("scsiminiport"))) {
                        Options->Parameters.u.Firmware.ScsiMiniportIoctl = TRUE;
                    }
                }
            } else {
                Options->Parameters.FileName = NULL;
            }
        }
    } else {
        validArgument = FALSE;
    }

    return validArgument;
}

int
_tmain(
    _In_              int          argc,
    _In_reads_bytes_(argc) _TCHAR* argv[]
)
{
    BOOLEAN         result = FALSE;
    COMMAND_OPTIONS commandOptions = {0};

    //
    // Validate input parameters.
    //
    result = ParseCommandLine(argc - 1, argv + 1, &commandOptions);

    if (!result) {
        PDEVICE_LIST    diskList = NULL;
        PDEVICE_LIST    cdromList = NULL;
        ULONG           diskCount = 0;
        ULONG           cdromCount = 0;
        ULONG           i;

        //
        // Get device count and build list.
        //
        diskCount = DeviceGetCount(DiskGuidIndex);
        cdromCount = DeviceGetCount(CdromGuidIndex);

        if (diskCount > 0) {
            diskList = (PDEVICE_LIST)malloc(sizeof(DEVICE_LIST) * diskCount);

            if (diskList == NULL) {
                _tprintf(_T("Build disk list - Could not allocate buffer for Device List."));
                return ERROR_SUCCESS;
            }

            ZeroMemory(diskList, sizeof(DEVICE_LIST) * diskCount);
            DeviceListBuild(DiskGuidIndex, diskList, &commandOptions, &diskCount);
        }

        if (cdromCount > 0) {
            cdromList = (PDEVICE_LIST)malloc(sizeof(DEVICE_LIST) * cdromCount);

            if (cdromList == NULL) {
                _tprintf(_T("Build cdrom list - Could not allocate buffer for Device List."));
                return ERROR_SUCCESS;
            }

            ZeroMemory(cdromList, sizeof(DEVICE_LIST) * cdromCount);
            DeviceListBuild(CdromGuidIndex, cdromList, &commandOptions, &cdromCount);
        }

        //
        // Display all devices
        //
        _tprintf(_T("\n\tDisk Count: %d\n"), diskCount);

        for (i = 0; i < diskCount; i++) {
            DeviceListGeneralInfo(DiskGuidIndex, diskList, i);
        }

        _tprintf(_T("\n\tCD/DVD Count: %d\n"), cdromCount);

        for (i = 0; i < cdromCount; i++) {
            DeviceListGeneralInfo(CdromGuidIndex, cdromList, i);
        }

        //
        // Print help message.
        //
        PrintHelp();

        //
        // Release resources.
        //
        DeviceListFree(diskList, diskCount);
        free(diskList);

        DeviceListFree(cdromList, cdromCount);
        free(cdromList);

    } else {
        DEVICE_LIST deviceList = {0};
        ULONG       guidIndex = DiskGuidIndex;

        //
        // Get specified device information.
        //
        if (commandOptions.Target.ForDisk) {
            guidIndex = DiskGuidIndex;
        } else if (commandOptions.Target.ForCdrom) {
            guidIndex = CdromGuidIndex;
        }

        deviceList.Handle = DeviceGetHandle(guidIndex, commandOptions.Target.DeviceNumber);

        if (deviceList.Handle != INVALID_HANDLE_VALUE) {
            deviceList.DeviceNumber = commandOptions.Target.DeviceNumber;
            DeviceGetGeneralInfo(&deviceList, 0, &commandOptions, FALSE);
        } else {
            _tprintf(_T("Cannot open handle for the device."));
        }

        //
        // Display device basic information.
        //
        if (commandOptions.Target.ForDisk) {
            DeviceListGeneralInfo(DiskGuidIndex, &deviceList, 0);
        } else {
            DeviceListGeneralInfo(CdromGuidIndex, &deviceList, 0);
        }

        //
        // Run requested operation.
        //
        if (commandOptions.Operation.Detail) {
            //DeviceIdentify(&deviceList, 0);
        } else if (commandOptions.Operation.HealthInfo) {
            DeviceHealthInfo(&deviceList, 0);
        } else if (commandOptions.Operation.LogPageInfo) {
            DeviceLogPageInfo(&deviceList, 0);
        } else if (commandOptions.Operation.AtaCommand) {
            //DeviceAtaCommandPassThrough(&deviceList, 0, &commandOptions);
        } else if (commandOptions.Operation.FirmwareInfo) {
            BOOL    result = FALSE;
            PUCHAR  buffer = NULL;
            ULONG   bufferLength = 0;

            if (commandOptions.Parameters.u.Firmware.ScsiMiniportIoctl) {
                result = DeviceGetMiniportFirmwareInfo(&deviceList, 0, &buffer, &bufferLength, TRUE);
            } else {
                result = DeviceGetStorageFirmwareInfo(&deviceList, 0, &commandOptions, &buffer, &bufferLength, TRUE);
            }

            if (buffer != NULL) {
                free(buffer);
                buffer = NULL;
            }
        } else if (commandOptions.Operation.FirmwareUpdate) {
            if (commandOptions.Parameters.u.Firmware.ScsiMiniportIoctl) {
                //DeviceMiniportFirmwareUpgrade(&deviceList, 0, &commandOptions);
            } else {
                //DeviceStorageFirmwareUpgrade(&deviceList, 0, &commandOptions, FALSE);
            }
        } 
    }

    return ERROR_SUCCESS;
}


ULONG
DeviceGetCount(
    _In_ ULONG DeviceGuidIndex
    )
/*++

Routine Description:

   Use SetupAPI to get the device count of a specific type.

Arguments:

    DeviceGuidIndex - 

Return Value:

    ULONG - count value

--*/
{
    ULONG       count = 0;
    HDEVINFO    deviceInfo = NULL;

    //
    // Open the device using device interface registered by the driver
    //
    deviceInfo = SetupDiGetClassDevs(DeviceGuids[DeviceGuidIndex],
                                     NULL,
                                     NULL,
                                     (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE)
                                     );

    if(deviceInfo == INVALID_HANDLE_VALUE) {
        _tprintf(_T("DeviceGetCount - Getting Device Class through SetupDiGetClassDevs failed with error. Error code: %d\n"), GetLastError());
    } else {
        BOOL                        result = FALSE;
        SP_DEVICE_INTERFACE_DATA    interfaceData = {0};
        ULONG                       i = 0;

        for (i = 0; ; i++) {
            interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

            result = SetupDiEnumDeviceInterfaces(deviceInfo,
                                                 NULL,
                                                 DeviceGuids[DeviceGuidIndex],
                                                 i,
                                                 &interfaceData
                                                 );

            if (result) {
                count++;
            } else {
                ULONG   errorCode = GetLastError();

                if (errorCode == ERROR_NO_MORE_ITEMS) {
                    //
                    // end of device list
                    //
                    break;
                } else {
                    _tprintf(_T("DeviceGetCount - SetupDiEnumDeviceInterfaces failed with error. Error code: %d\n"), errorCode);
                }
            }
        }
    }

    return count;
}

VOID
DeviceListBuild(
    _In_    ULONG           DeviceGuidIndex,
    _Inout_ PDEVICE_LIST    DeviceList,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_    PULONG          DeviceCount
    )
/*++

Routine Description:

   Build a list of device of a specific type.

Arguments:

    DeviceGuidIndex - 
    DeviceList - 
    CommandOptions - 
    DeviceCount - 

Return Value:

    None

--*/
{
    ULONG   deviceNumber = 0;
    ULONG   index = 0;

    //
    //  Enumerate all the devices
    //
    while (index < *DeviceCount) {
        DeviceList[index].Handle = DeviceGetHandle(DeviceGuidIndex, deviceNumber);

        if (DeviceList[index].Handle != INVALID_HANDLE_VALUE) {
            DeviceList[index].DeviceNumber = deviceNumber;
            DeviceGetGeneralInfo(DeviceList, index, CommandOptions, FALSE);
            index++;
        }

        deviceNumber++;

        if (deviceNumber >= 0x10000) {
            //
            // Bail out if device# reaches 64K, not realistic.
            // The situation can occur if a device was hot-removed after we got the count.
            //
            _tprintf(_T("DeviceListBuild - Device Count doesn't match the value before entering this function. Device Hot-Removal Happened. Original Count: %d, Found Count: %d\n"),
                     *DeviceCount, index);

            *DeviceCount = index;
            break;
        }
    }

    return;
}

HANDLE
DeviceGetHandle(
    _In_ ULONG  DeviceGuidIndex,
    _In_ ULONG  DeviceNumber
    )
/*++

Routine Description:

   Open a handle for a device

Arguments:

    DeviceGuidIndex - 
    DeviceNumber - 

Return Value:

    Handle

--*/
{
    HANDLE  deviceHandle = INVALID_HANDLE_VALUE;
    HRESULT result;
    TCHAR   deviceName[128] = {0};

    if (DeviceGuidIndex == 0) {
        result = StringCbPrintf(deviceName,
                                sizeof(deviceName) / sizeof(deviceName[0]) - 1,
                                _T("\\\\.\\Physicaldrive%d"),
                                DeviceNumber
                                );
    } else if (DeviceGuidIndex == 1) {
        result = StringCbPrintf(deviceName,
                                sizeof(deviceName) / sizeof(deviceName[0]) - 1,
                                _T("\\\\.\\Cdrom%d"),
                                DeviceNumber
                                );
    } else {
        return deviceHandle;
    }

    if (FAILED(result)) {
        return deviceHandle;
    }

    deviceHandle = CreateFile(deviceName,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_NO_BUFFERING,
                              NULL
                              );

    return deviceHandle;
}


VOID
DeviceGetGeneralInfo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_ BOOLEAN            GetDeviceNumber
    )
/*++

Routine Description:

   Send IOCTLs to retrieve SCSI Address, Device and Adapter Descriptor, 
   Device Number, Firmware Revision etc.

Arguments:

    DeviceList - 
    Index - 
    CommandOptions -
    GetDeviceNumber -

Return Value:

    None

--*/
{
    BOOL                            succeed = FALSE;
    STORAGE_PROPERTY_QUERY          query;
    ULONG                           returnedLength = 0;
    DEVICE_SEEK_PENALTY_DESCRIPTOR  seekPenaltyDesc = {0};

    PUCHAR                          buffer = NULL;
    ULONG                           bufferLength = 0;

    //
    // get device scsi id.
    //
    succeed = DeviceIoControl(DeviceList[Index].Handle,
                              IOCTL_SCSI_GET_ADDRESS,
                              NULL,
                              0,
                              &DeviceList[Index].ScsiId,
                              sizeof(SCSI_ADDRESS),
                              &returnedLength,
                              NULL
                              );

    if (!succeed) {
        //
        // some port drivers do not support this IOCTL.
        //
        DeviceList[Index].ScsiId.PortNumber = 0xff;
        DeviceList[Index].ScsiId.PathId = 0xff;
        DeviceList[Index].ScsiId.TargetId = 0xff;
        DeviceList[Index].ScsiId.Lun = 0xff;
    }

    //
    // get adapter descriptor
    //
    RtlZeroMemory(&query, sizeof(STORAGE_PROPERTY_QUERY));
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;

    succeed = DeviceIoControl(DeviceList[Index].Handle,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              &query,
                              sizeof( STORAGE_PROPERTY_QUERY ),
                              &DeviceList[Index].AdapterDescriptor,
                              sizeof(STORAGE_ADAPTER_DESCRIPTOR),
                              &returnedLength,
                              NULL
                              );

    if (!succeed) {
        _tprintf(_T("Retrieve Adapter Descriptor failed for device %d, Handle: %p. Error code: %d\n"),
                DeviceList[Index].DeviceNumber,
                DeviceList[Index].Handle,
                GetLastError());
    }

    //
    // get device descriptor;
    //
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    succeed = DeviceIoControl(DeviceList[Index].Handle,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              &query,
                              sizeof(STORAGE_PROPERTY_QUERY),
                              &DeviceList[Index].DeviceDescriptor,
                              1000,
                              &DeviceList[Index].DeviceDescLength,
                              NULL
                              );

    if (!succeed) {
        _tprintf(_T("Retrieve Device Descriptor failed for device %d, Handle: %p. Error code: %d\n"),
                DeviceList[Index].DeviceNumber,
                DeviceList[Index].Handle,
                GetLastError());
    }

    //
    // get seek penalty descriptor;
    //
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    ZeroMemory(&seekPenaltyDesc, sizeof(DEVICE_SEEK_PENALTY_DESCRIPTOR));

    succeed = DeviceIoControl(DeviceList[Index].Handle,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              &query,
                              sizeof(STORAGE_PROPERTY_QUERY),
                              &seekPenaltyDesc,
                              sizeof(DEVICE_SEEK_PENALTY_DESCRIPTOR),
                              &returnedLength,
                              NULL
                              );

    if (succeed) {
        DeviceList[Index].NoSeekPenalty = !(seekPenaltyDesc.IncursSeekPenalty);
    }

    //
    // get access alignment descriptor;
    //
    query.PropertyId = StorageAccessAlignmentProperty;
    query.QueryType = PropertyStandardQuery;

    succeed = DeviceIoControl(DeviceList[Index].Handle,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              &query,
                              sizeof(STORAGE_PROPERTY_QUERY),
                              &DeviceList[Index].DeviceAccessAlignmentDescriptor,
                              sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
                              &returnedLength,
                              NULL
                              );

    if (!succeed) {
        _tprintf(_T("Retrieve Access Alignment Descriptor failed for device %d, Handle: %p. Error code: %d\n"),
                 DeviceList[Index].DeviceNumber,
                 DeviceList[Index].Handle,
                 GetLastError());
    }


    //
    // Get DeviceNumber if needed.
    //
    if (GetDeviceNumber) {

        STORAGE_DEVICE_NUMBER storageDeviceNumber = {0};

        succeed = DeviceIoControl(DeviceList[Index].Handle,
                                  IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                  NULL,
                                  0,
                                  &storageDeviceNumber,
                                  sizeof(STORAGE_DEVICE_NUMBER),
                                  &returnedLength,
                                  NULL
                                  );

        if (succeed) {
            DeviceList[Index].DeviceNumber = storageDeviceNumber.DeviceNumber;

            if (storageDeviceNumber.DeviceType == FILE_DEVICE_DISK) {
                CommandOptions->Target.ForDisk = TRUE;
            } else if ((storageDeviceNumber.DeviceType == FILE_DEVICE_CD_ROM) || 
                       (storageDeviceNumber.DeviceType == FILE_DEVICE_DVD)) {
                CommandOptions->Target.ForCdrom = TRUE;
            }
        }
    }

    //
    // Get detailed firmware information.
    //
    succeed = DeviceGetStorageFirmwareInfo(DeviceList, Index, CommandOptions, &buffer, &bufferLength, FALSE);
    
    if (succeed) {
        DeviceList[Index].SupportStorageFWIoctl = TRUE;
    } else {
        succeed = DeviceGetMiniportFirmwareInfo(DeviceList, Index, &buffer, &bufferLength, FALSE);
    }

    if (succeed) {
        DeviceList[Index].SupportMiniportFWIoctl = TRUE;
    } else {
        //
        // If firmware information is not retrieved, try IDENTIFY DEVICE command for ATA devices.
        //
        DeviceGetAtaFirmwareInfo(DeviceList, Index);
    }

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}


VOID
DeviceListGeneralInfo(
    _In_ ULONG        DeviceGuidIndex,
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
{
    HRESULT                    result = 0;
    PSTORAGE_DEVICE_DESCRIPTOR deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;
    PUCHAR                     tempBuffer = NULL;

    TCHAR   deviceNumber[64] = {0};
    TCHAR   deviceType[16] = {0};
    TCHAR   scsiAddress[32] = {0};
    TCHAR   vendorId[64] = {0};
    TCHAR   productId[64] = {0};
    TCHAR   firmwareRevision[32] = {0};

    //
    // Get device number and bus type string.
    //
    result = StringCbPrintf(deviceNumber,
                            sizeof(deviceNumber) / sizeof(deviceNumber[0]) - 1,
                            _T("\t%s #%d : [%s]"),
                            (DeviceGuidIndex == 0) ? _T("Disk") : _T("Cdrom"),
                            DeviceList[Index].DeviceNumber,
                            BusType[DeviceList[Index].AdapterDescriptor.BusType]
                            );

    if (FAILED(result)) {
        _tprintf(_T("failed to generate Device Number string. Error code: %d\n"), GetLastError());
        return;
    }

    //
    // Get scsi address string.
    //
    if ((DeviceList[Index].ScsiId.PortNumber == 0xff) &&
        (DeviceList[Index].ScsiId.PathId == 0xff) &&
        (DeviceList[Index].ScsiId.TargetId == 0xff) &&
        (DeviceList[Index].ScsiId.Lun == 0xff) ) {

        result = StringCbPrintf(scsiAddress,
                                sizeof(scsiAddress) / sizeof(scsiAddress[0]) - 1,
                                _T("[SCSI_ID N/A]")
                                );
    } else {
        result = StringCbPrintf(scsiAddress,
                                sizeof(scsiAddress) / sizeof(scsiAddress[0]) - 1,
                                _T("[%02x %02x %02x %02x]"),
                                DeviceList[Index].ScsiId.PortNumber,
                                DeviceList[Index].ScsiId.PathId,
                                DeviceList[Index].ScsiId.TargetId,
                                DeviceList[Index].ScsiId.Lun
                                );
    }

    if (FAILED(result)) {
        _tprintf(_T("failed to generate SCSI Address string\n"));
        return;
    }

    //
    // Get device vendor and product string.
    //
    tempBuffer = DeviceList[Index].DeviceDescriptor;

    if (deviceDesc->VendorIdOffset && tempBuffer[deviceDesc->VendorIdOffset]) {

        MultiByteToWideChar(CP_ACP,
                            0,
                            (LPCCH)&tempBuffer[deviceDesc->VendorIdOffset],
                            -1,
                            vendorId,
                            sizeof(vendorId) / sizeof(vendorId[0]) - 1
                            );
    }

    if (deviceDesc->ProductIdOffset && tempBuffer[deviceDesc->ProductIdOffset]) {

        MultiByteToWideChar(CP_ACP,
                            0,
                            (LPCCH)&tempBuffer[deviceDesc->ProductIdOffset],
                            -1,
                            productId,
                            sizeof(productId) / sizeof(productId[0]) - 1
                            );
    }

    if ((DeviceList[Index].FirmwareRevision[0] != 0) || (DeviceList[Index].FirmwareRevision[1] != 0)) {

        MultiByteToWideChar(CP_ACP,
                            0,
                            (LPCCH)DeviceList[Index].FirmwareRevision,
                            -1,
                            firmwareRevision,
                            sizeof(firmwareRevision) / sizeof(firmwareRevision[0]) - 1
                            );

    } else if (deviceDesc->ProductRevisionOffset && tempBuffer[deviceDesc->ProductRevisionOffset]) {

        MultiByteToWideChar(CP_ACP,
                            0,
                            (LPCCH)&tempBuffer[deviceDesc->ProductRevisionOffset],
                            -1,
                            firmwareRevision,
                            sizeof(firmwareRevision) / sizeof(firmwareRevision[0]) - 1
                            );
    }

    if (DeviceList[Index].NoSeekPenalty) {
        RtlCopyMemory(deviceType, _T("[SSD]"), sizeof(_T("[SSD]")));
    } else {
        RtlCopyMemory(deviceType, _T("[HDD]"), sizeof(_T("[HDD]")));
    }

    //
    // print all information on the same line.
    //
    if (vendorId[0] != 0) {
        _tprintf(_T("%s %s %s %s %s \t%s\n"),
                deviceNumber,
                deviceType,
                scsiAddress,
                vendorId,
                productId,
                firmwareRevision
                );
    } else {
        _tprintf(_T("%s %s %s %s \t%s\n"),
                deviceNumber,
                deviceType,
                scsiAddress,
                productId,
                firmwareRevision
                );
    }

    return;

}

VOID
DeviceListFree(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceCount
    )
{
    ULONG   i;

    for (i = 0; i < DeviceCount; i++) {
        if ((DeviceList[i].Handle != 0) &&
            (DeviceList[i].Handle != INVALID_HANDLE_VALUE)) {
            CloseHandle(DeviceList[i].Handle);
            DeviceList[i].Handle = 0;
        }
/*
        if (DeviceList[i].ZonedDeviceDescriptor != 0) {
            free(DeviceList[i].ZonedDeviceDescriptor);
            DeviceList[i].ZonedDeviceDescriptor = NULL;
            DeviceList[i].ZonedDeviceDescriptorLength = 0;
        }
*/
    }

}


