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
DeviceSecureErase(
    _In_ PDEVICE_LIST   DeviceList,
    _In_ ULONG          DeviceIndex
    )
{
    ULONG   status = ERROR_SUCCESS;
    BOOL    result;
    DWORD   junk = 0; // discard results

    if (!IsNVMeDevice(DeviceList, DeviceIndex) &&
        !IsAtaDevice(DeviceList, DeviceIndex)) {
        status = ERROR_NOT_SUPPORTED;
        _tprintf(_T(" NOT supported Device Type.  This function is only for NVMe and ATA devices.\n"));
        goto exit;
    }

    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
        IOCTL_STORAGE_REINITIALIZE_MEDIA, 
        NULL, 0, // no input buffer
        NULL, 0, // no output
        &junk,   // # bytes returned
        (LPOVERLAPPED)NULL);

    if (!result)
    {
        status = GetLastError();
        _tprintf(_T(" Secure erase failed. Error %ld.\n"), status);
        goto exit;
    }

    _tprintf(_T(" Secure erase was successful\n"));

exit:
    return status;
}

ULONG
DeviceHealthInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    )
{
    ULONG   status = ERROR_SUCCESS;

    if (IsNVMeDevice(DeviceList, DeviceIndex)) {
        status = DeviceNVMeHealthInfo(DeviceList, DeviceIndex);
    } else if (IsAtaDevice(DeviceList, DeviceIndex)) {
        status = DeviceAtaHealthInfo(DeviceList, DeviceIndex);
    }  else {
        status = ERROR_NOT_SUPPORTED;
        _tprintf(_T(" NOT Supported Device Type. This function is only for NVMe device as now. \n"));
    }

    return status;
}

ULONG
DeviceLogPageInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    )
{
    ULONG   status = ERROR_SUCCESS;

    if (IsAtaDevice(DeviceList, DeviceIndex)) {
        status = DeviceAtaReadLogDirectory(DeviceList, DeviceIndex, TRUE);
    }  else if (IsNVMeDevice(DeviceList, DeviceIndex)) {
        status = DeviceNVMeLogPages(DeviceList, DeviceIndex);
    }  else {
        status = ERROR_NOT_SUPPORTED;
        _tprintf(_T(" NOT Supported Device Type. This function is only for ATA device as now. \n"));
    }

    return status;
}

