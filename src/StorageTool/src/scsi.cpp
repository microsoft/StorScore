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


//
// SCSI pass-through functions
//
ULONG
ScsiSendPassThrough(
    _In_ HANDLE               Handle,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt
)
/*++

Routine Description:

Sends IOCTL_SCSI_PASS_THROUGH with the parameters provided

Arguments:

Handle - Handle to the device
ScsiPt - The pass through structure

Return Value:

The number of bytes returned.

--*/
{
    BOOL  status;
    DWORD bytesReturned = 0;
    DWORD length = 0;

    length = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, DataBuffer) + ScsiPt->ScsiPassThrough.DataTransferLength;

    status = DeviceIoControl(Handle,
                             IOCTL_SCSI_PASS_THROUGH,
                             ScsiPt,
                             sizeof(SCSI_PASS_THROUGH),
                             ScsiPt,
                             length,
                             &bytesReturned,
                             FALSE);
    if (status && (ScsiPt->ScsiPassThrough.ScsiStatus == SCSISTAT_GOOD)) {
        _tprintf(_T("Send SCSI PASS THROUGH command succeeded.\n"));
    } else {
        if (!status) {
            _tprintf(_T("Send SCSI PASS THROUGH command failed. Error : %d\n"), GetLastError());
        } else {
            PSENSE_DATA senseData = (PSENSE_DATA)ScsiPt->SenseBuffer;
            _tprintf(_T("Send SCSI PASS THROUGH command failed. SCSI Error : %02x/%02x/%02x \n"), senseData->SenseKey, senseData->AdditionalSenseCode, senseData->AdditionalSenseCodeQualifier);
        }

        bytesReturned = 0;
    }

    return bytesReturned;
}

VOID
BuildInquiryCommand(
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR VpdPage
)
{
    PCDB inquiryCdb = (PCDB)ScsiPt->ScsiPassThrough.Cdb;

    ZeroMemory(ScsiPt, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    ScsiPt->ScsiPassThrough.Length = sizeof(SCSI_PASS_THROUGH);
    ScsiPt->ScsiPassThrough.PathId = ScsiAddress->PathId;
    ScsiPt->ScsiPassThrough.TargetId = ScsiAddress->TargetId;
    ScsiPt->ScsiPassThrough.Lun = ScsiAddress->Lun;
    ScsiPt->ScsiPassThrough.CdbLength = CDB6GENERIC_LENGTH;
    ScsiPt->ScsiPassThrough.SenseInfoLength = SPT_SENSE_LENGTH;
    ScsiPt->ScsiPassThrough.DataIn = SCSI_IOCTL_DATA_IN;
    ScsiPt->ScsiPassThrough.DataTransferLength = 512;
    ScsiPt->ScsiPassThrough.TimeOutValue = 10;
    ScsiPt->ScsiPassThrough.DataBufferOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, DataBuffer);
    ScsiPt->ScsiPassThrough.SenseInfoOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, SenseBuffer);

    inquiryCdb->CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    inquiryCdb->CDB6INQUIRY3.EnableVitalProductData = (VpdPage != 0) ? 1 : 0;
    inquiryCdb->CDB6INQUIRY.PageCode = VpdPage;
    inquiryCdb->CDB6INQUIRY.AllocationLength = 0xFF;

    return;
}

VOID
BuildReadBufferCommand(
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR Mode,
    _In_    UCHAR BufferId
)
{
    PREAD_BUFFER_CDB readBufferCdb = (PREAD_BUFFER_CDB)ScsiPt->ScsiPassThrough.Cdb;

    ZeroMemory(ScsiPt, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    ScsiPt->ScsiPassThrough.Length = sizeof(SCSI_PASS_THROUGH);
    ScsiPt->ScsiPassThrough.PathId = ScsiAddress->PathId;
    ScsiPt->ScsiPassThrough.TargetId = ScsiAddress->TargetId;
    ScsiPt->ScsiPassThrough.Lun = ScsiAddress->Lun;
    ScsiPt->ScsiPassThrough.CdbLength = CDB10GENERIC_LENGTH;
    ScsiPt->ScsiPassThrough.SenseInfoLength = SPT_SENSE_LENGTH;
    ScsiPt->ScsiPassThrough.DataIn = SCSI_IOCTL_DATA_IN;
    ScsiPt->ScsiPassThrough.DataTransferLength = 512;
    ScsiPt->ScsiPassThrough.TimeOutValue = 10;
    ScsiPt->ScsiPassThrough.DataBufferOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, DataBuffer);
    ScsiPt->ScsiPassThrough.SenseInfoOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, SenseBuffer);

    readBufferCdb->OperationCode = SCSIOP_READ_DATA_BUFF;
    readBufferCdb->Mode = Mode;
    readBufferCdb->BufferID = BufferId;
    readBufferCdb->AllocationLength[2] = 0xFF;

    return;
}

VOID
BuildReportSupportedOpCommand(
    _In_ PSCSI_ADDRESS ScsiAddress,
    _Inout_ PSCSI_PASS_THROUGH_WITH_BUFFERS ScsiPt,
    _In_    UCHAR ReportOptions,
    _In_    UCHAR Rctd,
    _In_    UCHAR RequestedOperationCode,
    _In_    UCHAR RequestedServiceAction
)
{
    PREPORT_SUPPORTED_OPERATION_CODES_CDB reportSupportedOpCdb = (PREPORT_SUPPORTED_OPERATION_CODES_CDB)ScsiPt->ScsiPassThrough.Cdb;

    ZeroMemory(ScsiPt, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    ScsiPt->ScsiPassThrough.Length = sizeof(SCSI_PASS_THROUGH);
    ScsiPt->ScsiPassThrough.PathId = ScsiAddress->PathId;
    ScsiPt->ScsiPassThrough.TargetId = ScsiAddress->TargetId;
    ScsiPt->ScsiPassThrough.Lun = ScsiAddress->Lun;
    ScsiPt->ScsiPassThrough.CdbLength = CDB10GENERIC_LENGTH;
    ScsiPt->ScsiPassThrough.SenseInfoLength = SPT_SENSE_LENGTH;
    ScsiPt->ScsiPassThrough.DataIn = SCSI_IOCTL_DATA_IN;
    ScsiPt->ScsiPassThrough.DataTransferLength = 512;
    ScsiPt->ScsiPassThrough.TimeOutValue = 10;
    ScsiPt->ScsiPassThrough.DataBufferOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, DataBuffer);
    ScsiPt->ScsiPassThrough.SenseInfoOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_WITH_BUFFERS, SenseBuffer);

    reportSupportedOpCdb->OperationCode = SCSIOP_MAINTENANCE_IN;
    reportSupportedOpCdb->ServiceAction = SERVICE_ACTION_REPORT_SUPPORTED_OPERATION_CODES;
    reportSupportedOpCdb->ReportOptions = ReportOptions;
    reportSupportedOpCdb->RCTD = Rctd;
    reportSupportedOpCdb->RequestedOperationCode = RequestedOperationCode;
    reportSupportedOpCdb->RequestedServiceAction[1] = RequestedServiceAction;

    reportSupportedOpCdb->AllocationLength[3] = 0xFF;

    return;
}

