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

/*++

Copyright (C) Microsoft Corporation, 2008 - 2012

Module Name:

    util.h

Abstract:


Revision History:

    Michael Xing (xiaoxing),  Oct. 2012

Notes:

--*/

#pragma once

#include "stdafx.h"
#include "StorageTool.h"

//
// Inline functions
//

__inline
BOOLEAN
CompareArgs (
    _In_ const TCHAR *arg1,
    _In_ const TCHAR *arg2
    )
{
    if ((arg1 == NULL) || (arg2 == NULL)) {
        return FALSE;
    }

    return  (_tcsnicmp(arg1, arg2, _tcslen(arg2) + 1) == 0);
}


BOOL
__inline
IsAtaDevice(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    )
{
    PSTORAGE_DEVICE_DESCRIPTOR deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[DeviceIndex].DeviceDescriptor;

    return ((deviceDesc->BusType == BusTypeAtapi) || (deviceDesc->BusType == BusTypeAta) || (deviceDesc->BusType == BusTypeSata));
}

BOOL
__inline
IsNVMeDevice(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
)
{
    PSTORAGE_DEVICE_DESCRIPTOR deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[DeviceIndex].DeviceDescriptor;

    return (deviceDesc->BusType == BusTypeNvme);
}

__inline
const TCHAR*
GetAtaCommandSetMajorVersionString(
    _In_ USHORT MajorVersion
    )
{
    if ((MajorVersion == 0x0000) ||
        (MajorVersion == 0xFFFF)) {
        return _T("Not Reported");
    } else if ((MajorVersion & (1 << 11)) != 0) {
        return _T("ACS-4");
    } else if ((MajorVersion & (1 << 10)) != 0) {
        return _T("ACS-3");
    } else if ((MajorVersion & (1 << 9)) != 0) {
        return _T("ACS-2");
    } else if ((MajorVersion & (1 << 8)) != 0) {
        return _T("ACS-1");
    } else if ((MajorVersion & (1 << 7)) != 0) {
        return _T("ATA-7");
    } else if ((MajorVersion & (1 << 6)) != 0) {
        return _T("ATA-6");
    } else if ((MajorVersion & (1 << 5)) != 0) {
        return _T("ATA-5");
    } else {
        return _T("Not Reported");
    }
}

__inline
const TCHAR*
GetAtaTransportMajorVersionString(
    _In_ USHORT MajorVersion,
    _In_ USHORT Type
    )
{
    if (((MajorVersion == 0x0000) && (Type == 0x0)) ||
        ((MajorVersion == 0xFFF) && (Type == 0xF))) {
        return _T("Not Reported");
    } else if ((MajorVersion & (1 << 7)) != 0) {
        return _T("SATA 3.2");
    } else if ((MajorVersion & (1 << 6)) != 0) {
        return _T("SATA 3.1");
    } else if ((MajorVersion & (1 << 5)) != 0) {
        return _T("SATA 3.0");
    } else if ((MajorVersion & (1 << 4)) != 0) {
        return _T("SATA 2.6");
    } else if ((MajorVersion & (1 << 3)) != 0) {
        return _T("SATA 2.5");
    } else if ((MajorVersion & (1 << 2)) != 0) {
        return _T("SATA 2");
    } else if ((MajorVersion & (1 << 1)) != 0) {
        return _T("SATA 1.0a");
    } else if ((MajorVersion & (1 << 0)) != 0) {
        return _T("PATA");
    } else {
        return _T("Not Reported");
    }
}

__inline
const TCHAR*
GetAtaTransportTypeString(
    _In_ USHORT MajorVersion,
    _In_ USHORT Type
    )
{
    if (((MajorVersion == 0x0000) && (Type == 0x0)) ||
        ((MajorVersion == 0xFFF) && (Type == 0xF))) {
        return _T("Not Reported");
    } else if (Type == 0x0) {
        return _T("PATA");
    } else if (Type == 0x1) {
        return _T("SATA");
    } else if (Type == 0xE) {
        return _T("PCIe");
    } else {
        return _T("Not Reported");
    }
}

__inline
const TCHAR*
GetAtaFormFactorString(
    _In_ USHORT FormFactor
    )
{
    switch (FormFactor) {
    case 0x1:
        return _T("5.25 inch");
    case 0x2:
        return _T("3.5 inch");
    case 0x3:
        return _T("2.5 inch");
    case 0x4:
        return _T("1.8 inch");
    case 0x5:
        return _T("< 1.8 inch");
    case 0x6:
        return _T("mSATA");
    case 0x7:
        return _T("M.2");
    case 0x8:
        return _T("MicroSSD");
    case 0x9:
        return _T("CFast");
    case 0x0:
    default:
        return _T("Not Reported");
    };
}


//
// Helper functions
//

VOID
PrintDetailedError(
    _In_ ULONG ErrorCode
    );

BOOLEAN
PrintSector(
    _In_ HANDLE     DeviceHandle,
    _In_ ULONGLONG  StartingSector,
    _In_ ULONGLONG  SectorCount
    );


//
// ATA functions
//

ULONG
AtaSendPassThrough(
    _In_ HANDLE               Handle,
    _Inout_ PATA_PASS_THROUGH_EX AtaPassThrough,
    _In_ ULONG                InputLength,
    _In_ ULONG                OutputLength,
    _In_ BOOLEAN              DisplayResult
    );

VOID
BuildIdentifyDeviceCommand (
    _In_ PATA_PASS_THROUGH_EX PassThru
    );

VOID
BuildReadLogExCommand (
    _In_ PATA_PASS_THROUGH_EX PassThru,
    _In_ UCHAR  LogAddress,
    _In_ USHORT PageNumber,
    _In_ USHORT BlockCount,
    _In_ USHORT FeatureField
    );

BOOLEAN
DeviceReadLogDirectory (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex,
    _In_ BOOLEAN      DisplayResult
    );


BOOL
DeviceGetAtaFirmwareInfo(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    );

ULONG
DeviceAtaHealthInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
);

BOOLEAN
DeviceAtaReadLogDirectory(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex,
    _In_ BOOLEAN      DisplayResult
    );



//
// SCSI functions
//

ULONG
ScsiSendPassThrough (
    _In_ HANDLE               Handle,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt
    );

VOID
BuildInquiryCommand (
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR VpdPage
    );

VOID
BuildReadBufferCommand (
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR Mode,
    _In_    UCHAR BufferId
    );

VOID
BuildReportSupportedOpCommand (
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR ReportOptions,
    _In_    UCHAR Rctd,
    _In_    UCHAR RequestedOperationCode,
    _In_    UCHAR RequestedServiceAction
    );



//
// NVMe function
//

ULONG
DeviceNVMeHealthInfo(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    );

BOOLEAN
DeviceNVMeLogPages(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    );

