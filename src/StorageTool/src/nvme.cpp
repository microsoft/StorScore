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

#include "stdafx.h"
#include "StorageTool.h"
#include "util.h"

ULONG
DeviceNVMeHealthInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    )
{
    ULONG   status = ERROR_SUCCESS;
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;

    PSTORAGE_PROPERTY_QUERY query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = NULL;

    //
    // Allocate buffer for use.
    //
    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + NVME_MAX_LOG_SIZE;
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("\tDeviceHealthInfo: allocate buffer failed, exit.\n"));
        status = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }

    //
    // Initialize query data structure to get SMART/Health Information Log.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_HEALTH_INFO_LOG);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
    );

    if (!result || (returnedLength == 0)) {
        status = GetLastError();
        _tprintf(_T("\tDeviceHealthInfo: SMART/Health Information Log failed. Error Code %d.\n"), status);
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {

        status = ERROR_INVALID_DATA;
        _tprintf(_T("\tDeviceHealthInfo: SMART/Health Information Log - data descriptor header not valid.\n"));
        goto exit;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_LOG))) {

        status = ERROR_INVALID_DATA;
        _tprintf(_T("\tDeviceHealthInfo: SMART/Health Information Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // SMART/Health Information Log Data 
    //
    {
        PNVME_HEALTH_INFO_LOG smartInfo = (PNVME_HEALTH_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        ULONG temperature = 0;
        ULONG128 temp = {0};

        _tprintf(_T("\n\tNVMe - Info from Health/Smart Log:\n\n"));

        if (smartInfo->CriticalWarning.AvailableSpaceLow == 1) {
            _tprintf(_T("\t Critical Warning: Available Space is Low!\n\n"));
        }

        if (smartInfo->CriticalWarning.TemperatureThreshold == 1) {
            _tprintf(_T("\t Critical Warning: Temperature Threshold is crossed!\n\n"));
        }

        if (smartInfo->CriticalWarning.ReliabilityDegraded == 1) {
            _tprintf(_T("\t Critical Warning: Reliability is degraded!\n\n"));
        }

        if (smartInfo->CriticalWarning.ReadOnly == 1) {
            _tprintf(_T("\t Critical Warning: Media has been placed in READ-ONLY mode!\n\n"));
        }

        if (smartInfo->CriticalWarning.VolatileMemoryBackupDeviceFailed == 1) {
            _tprintf(_T("\t Critical Warning: Volatile Memory Backup Device has failed!\n\n"));
        }

        temperature = (ULONG)smartInfo->Temperature[1] << 8 | smartInfo->Temperature[0];

        if (temperature >= 273) {
            _tprintf(_T("\t Composite Temperature: %d\n\n"), temperature - 273);
        } else {
            _tprintf(_T("\t Composite Temperature: NOT reported\n\n"));
        }

        _tprintf(_T("\t Available Spare: %d\n"), smartInfo->AvailableSpare);
        _tprintf(_T("\t Available Spare Threshold: %d\n\n"), smartInfo->AvailableSpareThreshold);

        _tprintf(_T("\t Endurance Percentage Used: %d\n\n"), smartInfo->PercentageUsed);

        temp.Low = ((PULONG128)smartInfo->DataUnitRead)->Low;
        temp.High = ((PULONG128)smartInfo->DataUnitRead)->High;

        // Convert to MB from 512 bytes, shift right 11 bits.
        Uint128ShiftRight(&temp, 11);
        if (temp.High > 0) {
            _tprintf(_T("\t Data Read (MB): %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Data Read (MB): %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->DataUnitWritten)->Low;
        temp.High = ((PULONG128)smartInfo->DataUnitWritten)->High;

        // Convert to MB from 512 bytes, shift right 11 bits.
        Uint128ShiftRight(&temp, 11);
        if (temp.High > 0) {
            _tprintf(_T("\t Data Written (MB): %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Data Written (MB): %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->HostReadCommands)->Low;
        temp.High = ((PULONG128)smartInfo->HostReadCommands)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Host READ command completed: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Host READ command completed: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->HostWrittenCommands)->Low;
        temp.High = ((PULONG128)smartInfo->HostWrittenCommands)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Host WRITE command completed: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Host WRITE command completed: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->ControllerBusyTime)->Low;
        temp.High = ((PULONG128)smartInfo->ControllerBusyTime)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Controller has been busy servicing IO (Minutes): %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Controller has been busy servicing IO (Minutes): %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->PowerCycle)->Low;
        temp.High = ((PULONG128)smartInfo->PowerCycle)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Power Cycle Count: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Power Cycle Count: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->PowerOnHours)->Low;
        temp.High = ((PULONG128)smartInfo->PowerOnHours)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Power On Hours: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Power On Hours: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->UnsafeShutdowns)->Low;
        temp.High = ((PULONG128)smartInfo->UnsafeShutdowns)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Unsafe Shutdown Count: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Unsafe Shutdown Count: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->MediaErrors)->Low;
        temp.High = ((PULONG128)smartInfo->MediaErrors)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Media Error Count: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Media Error Count: %I64d\n"), temp.Low);
        }

        temp.Low = ((PULONG128)smartInfo->ErrorInfoLogEntryCount)->Low;
        temp.High = ((PULONG128)smartInfo->ErrorInfoLogEntryCount)->High;

        if (temp.High > 0) {
            _tprintf(_T("\t Error Info Log Entry Count: %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Error Info Log Entry Count: %I64d\n"), temp.Low);
        }

        _tprintf(_T("\n"));

        _tprintf(_T("\t Controller has been operating over warning composite temperature (Minutes): %d\n"), smartInfo->WarningCompositeTemperatureTime);
        _tprintf(_T("\t Controller has been operating over critical composite temperature (Minutes): %d\n"), smartInfo->CriticalCompositeTemperatureTime);

        if (smartInfo->TemperatureSensor1 >= 273) {
            _tprintf(_T("\t Temperature Sensor #1: %d\n"), smartInfo->TemperatureSensor1 - 273);
        }

        if (smartInfo->TemperatureSensor2 >= 273) {
            _tprintf(_T("\t Temperature Sensor #2: %d\n"), smartInfo->TemperatureSensor2 - 273);
        }

        if (smartInfo->TemperatureSensor3 >= 273) {
            _tprintf(_T("\t Temperature Sensor #3: %d\n"), smartInfo->TemperatureSensor3 - 273);
        }

        if (smartInfo->TemperatureSensor4 >= 273) {
            _tprintf(_T("\t Temperature Sensor #4: %d\n"), smartInfo->TemperatureSensor4 - 273);
        }

        if (smartInfo->TemperatureSensor5 >= 273) {
            _tprintf(_T("\t Temperature Sensor #5: %d\n"), smartInfo->TemperatureSensor5 - 273);
        }

        if (smartInfo->TemperatureSensor6 >= 273) {
            _tprintf(_T("\t Temperature Sensor #6: %d\n"), smartInfo->TemperatureSensor6 - 273);
        }

        if (smartInfo->TemperatureSensor7 >= 273) {
            _tprintf(_T("\t Temperature Sensor #7: %d\n"), smartInfo->TemperatureSensor7 - 273);
        }

        if (smartInfo->TemperatureSensor8 >= 273) {
            _tprintf(_T("\t Temperature Sensor #8: %d\n"), smartInfo->TemperatureSensor8 - 273);
        }
    }

    //
    // Initialize query data structure to get Microsoft SMART/Health Information Log.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_MSFT_HEALTH;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_HEALTH_INFO_MSFT_LOG_V0);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
    );

    _tprintf(_T("\n\tNVMe - Info from Microsoft's Vendor-Specific Log Page:\n\n"));

    if (!result || (returnedLength == 0)) {
        status = GetLastError();
        _tprintf(_T("\t Error: Reading the log failed. Error Code %d.\n"), status);
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {

        status = ERROR_INVALID_DATA;
        _tprintf(_T("\t Error: The data descriptor header of the log page is not valid.\n"));
        goto exit;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_MSFT_LOG_V0))) {

        status = ERROR_INVALID_DATA;
        _tprintf(_T("\t Error: The ProtocolData Offset/Length of the log page is not valid.\n"));
        goto exit;
    }

    //
    // Microsoft SMART/Health Information Log Data 
    //
    {
        PNVME_HEALTH_INFO_MSFT_LOG_V0 msftInfo0 = (PNVME_HEALTH_INFO_MSFT_LOG_V0)((PCHAR)protocolData + protocolData->ProtocolDataOffset);
        PNVME_HEALTH_INFO_MSFT_LOG_V1 msftInfo1 = (PNVME_HEALTH_INFO_MSFT_LOG_V1)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        USHORT      version = *((PUSHORT)msftInfo0->Version);
        ULONG128    temp = {0};
        ULONGLONG   temp8 = 0;
        ULONG       temp4 = 0;

        _tprintf(_T("\t Version %d\n"), version);

        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp.Low = ((PULONG128)msftInfo0->MediaUnitsWritten)->Low;
            temp.High = ((PULONG128)msftInfo0->MediaUnitsWritten)->High;
        } else {
            temp.Low = ((PULONG128)msftInfo1->MediaUnitsWritten)->Low;
            temp.High = ((PULONG128)msftInfo1->MediaUnitsWritten)->High;
        }

        // Convert to MB from 512 bytes, shift right 11 bits.
        Uint128ShiftRight(&temp, 11);
        if (temp.High > 0) {
            _tprintf(_T("\t Media Units Written (MB): %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t Media Units Written (MB): %I64d\n"), temp.Low);
        }

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->SupportedFeatures.SuperCapacitorExists;
        } else {
            temp4 = msftInfo1->SupportedFeatures.SuperCapacitorExists;
        }

        _tprintf(_T("\t Super Capacitor Exists: %s\n"), (temp4 == 1) ? _T("Yes") : _T("No"));

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->CapacitorHealth;
        } else {
            temp4 = msftInfo1->CapacitorHealth;
        }

        if (temp4 < 255) {
            _tprintf(_T("\t Percentage of Charge Capacitor can hold: %d\n"), temp4);
        } else {
            _tprintf(_T("\t Percentage of Charge Capacitor can hold: no capacitor exists.\n"));
        }

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp.Low = ((PULONG128)msftInfo0->ECCIterations)->Low;
            temp.High = ((PULONG128)msftInfo0->ECCIterations)->High;
        } else {
            temp.Low = ((PULONG128)msftInfo1->ECCIterations)->Low;
            temp.High = ((PULONG128)msftInfo1->ECCIterations)->High;
        }

        if (temp.High > 0) {
            _tprintf(_T("\t MECC Iterations (READ performed for error correction): %I64d %I64d\n"), temp.High, temp.Low);
        } else {
            _tprintf(_T("\t MECC Iterations (READ performed for error correction): %I64d\n"), temp.Low);
        }

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp8 = (ULONGLONG)msftInfo0->TemperatureThrottling[0] |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[1] << 8) |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[2] << 16) |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[3] << 24) |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[4] << 32) |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[5] << 40) |
                    ((ULONGLONG)msftInfo0->TemperatureThrottling[6] << 48);
        } else {
            temp8 = *((PULONGLONG)msftInfo1->TemperatureThrottling);
        }

        _tprintf(_T("\t Dies off minutes to prevent overheating: %I64d\n"), temp8);

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->PowerConsumption;
        } else {
            temp4 = msftInfo1->PowerConsumption;
        }

        if (temp4 != 255) {
            _tprintf(_T("\t Current power consumption (watts): %d\n"), temp4);
        } else {
            _tprintf(_T("\t Current power consumption (watts): not measured\n"));
        }

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->WearRangeDelta;
        } else {
            temp4 = msftInfo1->WearRangeDelta;
        }

        _tprintf(_T("\t Wear Range Delta: %d\n"), temp4);

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp8 = (ULONGLONG)msftInfo0->UnalignedIO[0] |
                    ((ULONGLONG)msftInfo0->UnalignedIO[1] << 8) |
                    ((ULONGLONG)msftInfo0->UnalignedIO[2] << 16) |
                    ((ULONGLONG)msftInfo0->UnalignedIO[3] << 24) |
                    ((ULONGLONG)msftInfo0->UnalignedIO[4] << 32) |
                    ((ULONGLONG)msftInfo0->UnalignedIO[5] << 40);
        } else {
            temp8 = *((PULONGLONG)msftInfo1->UnalignedIO);
        }

        _tprintf(_T("\t Number of IOs Not Aligned to 4kB Boundaries: %I64d\n"), temp8);

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = *((PULONG)msftInfo0->MappedLBAs);
        } else {
            temp4 = *((PULONG)msftInfo1->MappedLBAs);
        }

        _tprintf(_T("\t Mapped LBA Count: %d\n"), temp4);


        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->ProgramFailCount;
        } else {
            temp4 = *((PULONG)msftInfo1->ProgramFailCount);
        }

        _tprintf(_T("\t Program Fail Count: %d\n"), temp4);

        //
        if (version == NVME_LOG_PAGE_MSFT_HEALTH_VERSION_0) {
            temp4 = msftInfo0->EraseFailCount;
        } else {
            temp4 = *((PULONG)msftInfo1->EraseFailCount);
        }

        _tprintf(_T("\t Erase Fail Count: %d\n"), temp4);

        _tprintf(_T("\n\t RAW Data: \n\t "));
        int i = 0;
        PUCHAR tempPointer = (PUCHAR)protocolData + protocolData->ProtocolDataOffset;

        for (i = 0; i < sizeof(NVME_HEALTH_INFO_MSFT_LOG_V0); i++) {
            _tprintf(_T("%02X "), tempPointer[i]);

            if (((i + 1) % 16) == 0) {
                _tprintf(_T("\n\t "));
            }
        }
    }

exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return status;
}

BOOLEAN DeviceNVMeLogPages(PDEVICE_LIST DeviceList, ULONG DeviceIndex)
{
    _tprintf(_T("\n\t Log pages not yet implemented for NVMe. \n\t "));
    return true;
}

