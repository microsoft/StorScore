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

BOOLEAN
PrintSector(
    _In_ HANDLE     DeviceHandle,
    _In_ ULONGLONG  StartingSector,
    _In_ ULONGLONG  SectorCount
)
{
    BOOLEAN   success;
    ULONGLONG offset;
    DWORD     filePointer;
    DWORD     errorCode;

    offset = (ULONGLONG)StartingSector * LogicalSectorSize;

    // Try to move file pointer to reading position
    filePointer = SetFilePointer(DeviceHandle, (ULONG)offset, NULL, FILE_BEGIN);

    if (filePointer == INVALID_SET_FILE_POINTER) {
        // Obtain the error code.
        errorCode = GetLastError();

        _tprintf(_T("  Set file pointer failed, error: 0x%X.\n"), errorCode);

        success = FALSE;
    } else {
        BOOL    result;
        DWORD   bytesRead = 0;
        PUCHAR  readBuffer = NULL;

        readBuffer = (PUCHAR)malloc((ULONG)SectorCount * LogicalSectorSize);

        if (readBuffer == NULL) {
            // Obtain the error code.
            errorCode = GetLastError();

            _tprintf(_T("  allocate buffer failed, error: 0x%X.\n"), errorCode);

            success = FALSE;
        } else {
            // Attempt an asynchronous read operation.
            result = ReadFile(DeviceHandle,
                              readBuffer,
                              (ULONG)SectorCount * LogicalSectorSize,
                              &bytesRead,
                              NULL);
            if (result) {
                if (bytesRead == SectorCount * LogicalSectorSize) {
                    ULONG   checkSum = 0;
                    ULONG   limit = (ULONG)SectorCount * LogicalSectorSize / sizeof(ULONG);
                    PULONG  buffer = (PULONG)readBuffer;
                    ULONG   i;

                    for (i = 0; i < limit; i++) {
                        checkSum += buffer[i];
                    }

                    _tprintf(_T("  checksum for sector 0x%I64X, sector count 0x%I64X: %x\n"), StartingSector, SectorCount, checkSum);
                    success = TRUE;
                } else {
                    _tprintf(_T("  Bytes read: 0x%X, smaller than 0x%I64X sector(s).\n"), bytesRead, SectorCount);
                    success = FALSE;
                }
            } else {
                errorCode = GetLastError();

                _tprintf(_T("  Read file failed, error: 0x%X.\n"), errorCode);

                success = FALSE;
            }

            //release buffer
            free(readBuffer);
        }
    }

    return success;
}

VOID
PrintDetailedError(
    _In_ ULONG ErrorCode
)
{
    LPVOID  errorBuffer = NULL;
    ULONG   count = 0;

    count = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL,
                          ErrorCode,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPTSTR)&errorBuffer,
                          0,
                          NULL);

    if (count != 0) {
        _tprintf(_T("\tDetailed error message: %s"), (TCHAR*)errorBuffer);
    } else {
        _tprintf(_T("\tCould not print. Error code: %d"), GetLastError());
    }

    if (errorBuffer != NULL) {
        LocalFree(errorBuffer);
    }

    return;
}

BOOL
DeviceGetMiniportFirmwareInfo(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index,
    _Out_ PUCHAR*     Buffer,
    _Out_ DWORD*      BufferLength,
    _In_ BOOLEAN      DisplayResult
    )
/*++

Routine Description:

    Issue IOCTL_SCSI_MINIPORT_FIRMWARE, function FIRMWARE_FUNCTION_GET_INFO to retrieve firmware information.
    NOTE: This is known supported by StorNVMe since win8.1
          Caller should free the returned memory.

Arguments:

    DeviceList - 
    Index - 
    Buffer - 
    BufferLength - 
    DisplayResult - 

Return Value:

    BOOL - TRUE or FALSE

--*/
{
    BOOL    result = FALSE;
    ULONG   returnedLength = 0;
    ULONG   firmwareInfoOffset = 0;

    PSRB_IO_CONTROL         srbControl = NULL;
    PFIRMWARE_REQUEST_BLOCK firmwareRequest = NULL;
    PSTORAGE_FIRMWARE_INFO  firmwareInfo = NULL;

    PUCHAR  buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   retryCount = 0;

    *Buffer = NULL;
    *BufferLength = 0;

    //
    // Start with buffer size that can hold 8 firmware slots.
    // This should be good for most of devices.
    //
    bufferLength = ((sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK) - 1) / sizeof(PVOID) + 1) * sizeof(PVOID) +
                    FIELD_OFFSET(STORAGE_FIRMWARE_INFO, Slot) + sizeof(STORAGE_FIRMWARE_SLOT_INFO) * 8;

RetryOnce:
    buffer = (PUCHAR)malloc(bufferLength);

    if (buffer == NULL) {
        //
        // Exit if memory allocation failed.
        //
        if (DisplayResult) {
            _tprintf(_T("Allocate Firmware Information Buffer Failed.\n"));
        }

        bufferLength = 0;

        goto Exit;
    }

    RtlZeroMemory(buffer, bufferLength);

    srbControl = (PSRB_IO_CONTROL)buffer;
    firmwareRequest = (PFIRMWARE_REQUEST_BLOCK)(srbControl + 1);

    firmwareInfoOffset = ((sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK) - 1) / sizeof(PVOID) + 1) * sizeof(PVOID);

    srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
    srbControl->ControlCode = IOCTL_SCSI_MINIPORT_FIRMWARE;
    RtlMoveMemory(srbControl->Signature, IOCTL_MINIPORT_SIGNATURE_FIRMWARE, 8);
    srbControl->Timeout = 30;
    srbControl->Length = bufferLength - sizeof(SRB_IO_CONTROL);

    firmwareRequest->Version = FIRMWARE_REQUEST_BLOCK_STRUCTURE_VERSION;
    firmwareRequest->Size = sizeof(FIRMWARE_REQUEST_BLOCK);
    firmwareRequest->Function = FIRMWARE_FUNCTION_GET_INFO;
    firmwareRequest->Flags = FIRMWARE_REQUEST_FLAG_CONTROLLER;
    firmwareRequest->DataBufferOffset = firmwareInfoOffset;
    firmwareRequest->DataBufferLength = bufferLength - firmwareInfoOffset;

    //
    // get device firmware info
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_SCSI_MINIPORT,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    firmwareInfo = (PSTORAGE_FIRMWARE_INFO)((PUCHAR)srbControl + firmwareRequest->DataBufferOffset);

    if (result && (retryCount == 0)) {
        //
        // Check if bigger buffer is needed. If yes, re-allocate buffer and retry the IOCTL request.
        //
        if (firmwareInfo->SlotCount > 8) {
            bufferLength = ((sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK) - 1) / sizeof(PVOID) + 1) * sizeof(PVOID) +
                            FIELD_OFFSET(STORAGE_FIRMWARE_INFO, Slot) + sizeof(STORAGE_FIRMWARE_SLOT_INFO) * firmwareInfo->SlotCount;
            retryCount++;

            free(buffer);
            buffer = NULL;

            goto RetryOnce;
        }
    }

    if (result && (retryCount > 0)) {
        //
        // Check if bigger buffer is still needed. This should be a driver issue.
        //
        ULONG   tempLength = 0;

        tempLength = ((sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK) - 1) / sizeof(PVOID) + 1) * sizeof(PVOID) +
                        FIELD_OFFSET(STORAGE_FIRMWARE_INFO, Slot) + sizeof(STORAGE_FIRMWARE_SLOT_INFO) * firmwareInfo->SlotCount;

        if (tempLength > bufferLength) {

            if (DisplayResult) {
                _tprintf(_T("\t Driver reports slot count %d after retried.\n"), firmwareInfo->SlotCount);
            }

            free(buffer);
            buffer = NULL;
            bufferLength = 0;

            goto Exit;
        }
    }

    if (!result) {
        if (DisplayResult) {
            _tprintf(_T("\t Get Firmware Information using IOCTL_SCSI_MINIPORT_FIRMWARE (function FIRMWARE_FUNCTION_GET_INFO) Failed. Error code: %d\n"), GetLastError());
        }

        free(buffer);
        buffer = NULL;
        bufferLength = 0;
    } else {
        UCHAR   i;
        TCHAR   revision[32] = {0};

        //
        // Update cached firmware information if the request succeeded.
        //
        RtlZeroMemory(DeviceList[Index].FirmwareRevision, 17);

        for (i = 0; i < firmwareInfo->SlotCount; i++) {
            if (firmwareInfo->Slot[i].SlotNumber == firmwareInfo->ActiveSlot) {
                RtlCopyMemory(DeviceList[Index].FirmwareRevision, &firmwareInfo->Slot[i].Revision.AsUlonglong, 8);
                break;
            }
        }

        if (DisplayResult) {
            _tprintf(_T("\n\t ----Firmware Information----\n"));
            _tprintf(_T("\t Support upgrade command: %s\n"), firmwareInfo->UpgradeSupport ? _T("Yes") : _T("No"));
            _tprintf(_T("\t Slot Count: %d\n"), firmwareInfo->SlotCount);
            _tprintf(_T("\t Current Active Slot: %d\n"), firmwareInfo->ActiveSlot);

            if (firmwareInfo->PendingActivateSlot == STORAGE_FIRMWARE_INFO_INVALID_SLOT) {
                _tprintf(_T("\t Pending Active Slot: %s\n"),  _T("No"));
            } else {
                _tprintf(_T("\t Pending Active Slot: %d\n"), firmwareInfo->PendingActivateSlot);
            }

            for (i = 0; i < firmwareInfo->SlotCount; i++) {
                RtlZeroMemory(revision, sizeof(revision));

                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)&firmwareInfo->Slot[i].Revision.AsUlonglong,
                                    -1,
                                    revision,
                                    8
                                    );

                _tprintf(_T("\t\t Slot ID: %d\n"), firmwareInfo->Slot[i].SlotNumber);
                _tprintf(_T("\t\t Slot Read Only: %s\n"), firmwareInfo->Slot[i].ReadOnly ? _T("Yes") : _T("No"));
                _tprintf(_T("\t\t Revision: %s\n"), revision);
            }
        }
    }


Exit:

    *Buffer = buffer;
    *BufferLength = bufferLength;

    return result;
}

BOOL
DeviceGetStorageFirmwareInfo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _Out_ PUCHAR*           Buffer,
    _Out_ DWORD*            BufferLength,
    _In_ BOOLEAN            DisplayResult
    )
/*++

Routine Description:

    Issue IOCTL_STORAGE_FIRMWARE_GET_INFO to retrieve firmware information.
    NOTE: This is supported since win10
          Caller should free the returned memory.

Arguments:

    DeviceList - 
    Index - 
    Buffer - 
    CommandOptions - 
    BufferLength - 
    DisplayResult - 

Return Value:

    BOOL - TRUE or FALSE

--*/
{
    BOOL    result = FALSE;
    ULONG   returnedLength = 0;
    PUCHAR  buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   retryCount = 0;

    STORAGE_HW_FIRMWARE_INFO_QUERY  query = {0};
    PSTORAGE_HW_FIRMWARE_INFO       firmwareInfo = NULL;
    PSTORAGE_DEVICE_DESCRIPTOR      deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;

    UNREFERENCED_PARAMETER(CommandOptions);

    *Buffer = NULL;
    *BufferLength = 0;

    if (DisplayResult) {
        _tprintf(_T("Get firmware information by issuing IOCTL_STORAGE_FIRMWARE_GET_INFO.\n"));
    }

    //
    // Start with buffer size that can hold 8 firmware slots.
    // This should be good for most of devices.
    //
    bufferLength = FIELD_OFFSET(STORAGE_HW_FIRMWARE_INFO, Slot) + sizeof(STORAGE_HW_FIRMWARE_SLOT_INFO) * 8;

RetryOnce:
    buffer = (PUCHAR)malloc(bufferLength);

    if (buffer == NULL) {
        //
        // Exit if memory allocation failed.
        //
        if (DisplayResult) {
            _tprintf(_T("Allocate Firmware Information Buffer Failed.\n"));
        }

        bufferLength = 0;

        result = FALSE;
        goto Exit;
    }

    RtlZeroMemory(buffer, bufferLength);
    firmwareInfo = (PSTORAGE_HW_FIRMWARE_INFO)buffer;

    query.Version = sizeof(STORAGE_HW_FIRMWARE_INFO_QUERY);
    query.Size = sizeof(STORAGE_HW_FIRMWARE_INFO_QUERY);

    if (deviceDesc->BusType == BusTypeNvme) {
        query.Flags = STORAGE_HW_FIRMWARE_REQUEST_FLAG_CONTROLLER;
    }
    else {
        query.Flags = 0;
    }

    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_FIRMWARE_GET_INFO,
                             &query,
                             sizeof(STORAGE_HW_FIRMWARE_INFO_QUERY),
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (result && (retryCount == 0)) {
        //
        // Check if bigger buffer is needed. If yes, re-allocate buffer and retry the IOCTL request.
        //
        if (firmwareInfo->SlotCount > 8) {
            if (DisplayResult) {
                _tprintf(_T("Slot Count %d is more than 8, allocate a bigger buffer and retry the request.\n"), firmwareInfo->SlotCount);
            }

            bufferLength = FIELD_OFFSET(STORAGE_HW_FIRMWARE_INFO, Slot) + sizeof(STORAGE_HW_FIRMWARE_SLOT_INFO) * firmwareInfo->SlotCount;
            retryCount++;

            free(buffer);
            buffer = NULL;

            goto RetryOnce;
        }
    }

    if (result && (retryCount > 0)) {
        //
        // Check if bigger buffer is still needed. This should be a driver issue.
        //
        ULONG   tempLength = 0;

        tempLength = FIELD_OFFSET(STORAGE_HW_FIRMWARE_INFO, Slot) + sizeof(STORAGE_HW_FIRMWARE_SLOT_INFO) * firmwareInfo->SlotCount;

        if (tempLength > bufferLength) {

            if (DisplayResult) {
                _tprintf(_T("Driver reports slot count %d (unexpected) after retried.\n"), firmwareInfo->SlotCount);
            }

            result = FALSE;
            goto Exit;
        }
    }

    if (!result) {
        if (DisplayResult) {
            _tprintf(_T("Get Firmware Information - IOCTL_STORAGE_FIRMWARE_GET_INFO Failed. Error code: %d\n"), GetLastError());
        }

        goto Exit;
    } else {
        UCHAR   i;
        TCHAR   revision[32] = {0};

        //
        // Update cached firmware information if the request succeeded.
        //
        RtlZeroMemory(DeviceList[Index].FirmwareRevision, 17);

        for (i = 0; i < firmwareInfo->SlotCount; i++) {
            if (firmwareInfo->Slot[i].SlotNumber == firmwareInfo->ActiveSlot) {
                RtlCopyMemory(DeviceList[Index].FirmwareRevision, &firmwareInfo->Slot[i].Revision, 16);
                break;
            }
        }

        if (DisplayResult) {
            _tprintf(_T("\n\tSupport upgrade command: %s\n"), firmwareInfo->SupportUpgrade ? _T("Yes") : _T("No"));
            _tprintf(_T("\tSlot Count: %d\n"), firmwareInfo->SlotCount);
            _tprintf(_T("\tCurrent Active Slot: %d\n"), firmwareInfo->ActiveSlot);

            if (firmwareInfo->PendingActivateSlot == STORAGE_FIRMWARE_INFO_INVALID_SLOT) {
                _tprintf(_T("\tPending Active Slot: %s\n"), _T("N/A"));
            }
            else {
                _tprintf(_T("\tPending Active Slot: %d\n"), firmwareInfo->PendingActivateSlot);
            }

            _tprintf(_T("\tFirmware applies to both controller and device: %s\n"), firmwareInfo->FirmwareShared ? _T("Yes") : _T("No"));
            _tprintf(_T("\tFirmware payload alignment: %d\n"), firmwareInfo->ImagePayloadAlignment);
            _tprintf(_T("\tFirmware payload max size: %d\n"), firmwareInfo->ImagePayloadMaxSize);

            for (i = 0; i < firmwareInfo->SlotCount; i++) {
                RtlZeroMemory(revision, sizeof(revision));

                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)firmwareInfo->Slot[i].Revision,
                                    -1,
                                    revision,
                                    sizeof(revision) / sizeof(revision[0]) - 1
                                    );

                _tprintf(_T("\tSlot ID: %d\n"), firmwareInfo->Slot[i].SlotNumber);
                _tprintf(_T("\tSlot Read Only: %s\n"), firmwareInfo->Slot[i].ReadOnly ? _T("Yes") : _T("No"));
                _tprintf(_T("\tRevision: %s\n"), revision);
            }
        }

        result = TRUE;
    }

Exit:

    if (!result) {
        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
        }

        bufferLength = 0;
    }

    *Buffer = buffer;
    *BufferLength = bufferLength;

    return result;
}






