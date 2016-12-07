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

__inline
const TCHAR*
GetSmrDeviceTypeString(
    _In_ STORAGE_ZONED_DEVICE_TYPES Type
    )
{
    switch(Type) {
        case ZonedDeviceTypeDeviceManaged:
            return _T("Device Managed SMR Disk");
        case ZonedDeviceTypeHostManaged:
            return _T("Host Managed SMR Disk");
        case ZonedDeviceTypeHostAware:
            return _T("Host Aware SMR Disk");
        case ZonedDeviceTypeUnknown:
        default:
            return _T("Not a SMR Disk");
    }
}

__inline
const TCHAR*
GetZoneTypeString(
    _In_ STORAGE_ZONE_TYPES ZoneType
    )
{
    switch(ZoneType) {
        case ZoneTypeUnknown:
            return _T("Unknown");
        case ZoneTypeConventional:
            return _T("Conventional");
        case ZoneTypeSequentialWritePreferred:
            return _T("SequentialWritePreferred");
        case ZoneTypeSequentialWriteRequired:
            return _T("SequentialWriteRequired");
        default:
            return _T("Invalid");
    }
}

__inline
const TCHAR*
GetZoneConditionString(
    _In_ STORAGE_ZONE_CONDITION ZoneCondition
    )
{
    switch(ZoneCondition) {
        case ZoneConditionConventional:
            return _T("Conventional");
        case ZoneConditionEmpty:
            return _T("Empty");
        case ZoneConditionImplicitlyOpened:
            return _T("ImplicitlyOpened");
        case ZoneConditionExplicitlyOpened:
            return _T("ExplicitlyOpened");
        case ZoneConditionClosed:
            return _T("Closed");
        case ZoneConditionReadOnly:
            return _T("ReadOnly");
        case ZoneConditionFull:
            return _T("Full");
        case ZoneConditionOffline:
            return _T("Offline");
        case ZoneConditionResetWritePointerRecommended:
            return _T("ResetWritePointerRecommended");

        default:
            return _T("Invalid");
    }
}

__inline
const TCHAR*
GetZoneActionString(
    _In_ DEVICE_DATA_MANAGEMENT_SET_ACTION Action
    )
{
    switch (Action) {
    case DeviceDsmAction_OpenZone:
        return _T("Open-Zone");
    case DeviceDsmAction_FinishZone:
        return _T("Finish-Zone");
    case DeviceDsmAction_CloseZone:
        return _T("Close-Zone");
    default:
        return _T("InvalidZoneAction");
    }
}

__inline
const TCHAR*
GetAtaZoneActionString(
    _In_ UCHAR Action
    )
{
    switch (Action) {
    case ZM_ACTION_OPEN_ZONE:
        return _T("Open-Zone");
    case ZM_ACTION_FINISH_ZONE:
        return _T("Finish-Zone");
    case ZM_ACTION_CLOSE_ZONE:
        return _T("Close-Zone");
    default:
        return _T("InvalidZoneAction");
    }
}

VOID
DeviceGetZonedDeviceProperty(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index
    )
/*++

Routine Description:

    Issuing IOCTL to get Zoned Device Property.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    BOOL    result;
    ULONG   returnedLength = 0;
    ULONG   i = 0;
    ULONG   zoneGroupCount = 64;

    STORAGE_PROPERTY_QUERY query = {0};

    //
    // Initialize query data structure to get Zoned Device Descriptor.
    //
    query.PropertyId = StorageDeviceZonedDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    //
    // Allocate ZonedDeviceDescriptor buffer if it doesn't exist yet.
    //
    if (DeviceList[Index].ZonedDeviceDescriptorLength == 0) {
        //
        // Default size is able to hold 64 zone groups.
        //
        DeviceList[Index].ZonedDeviceDescriptorLength = FIELD_OFFSET(STORAGE_ZONED_DEVICE_DESCRIPTOR, ZoneGroup) + sizeof(STORAGE_ZONE_GROUP) * zoneGroupCount;
    }

Retry:

    if (DeviceList[Index].ZonedDeviceDescriptor == NULL) {

        DeviceList[Index].ZonedDeviceDescriptor = (PSTORAGE_ZONED_DEVICE_DESCRIPTOR)malloc(DeviceList[Index].ZonedDeviceDescriptorLength);

        if (DeviceList[Index].ZonedDeviceDescriptor == NULL) {

            _tprintf(_T("\tDeviceSmrTest: Cannot allocate memory for ZonedDeviceDescriptor, required size: %d. Error Code %d.\n"),
                     DeviceList[Index].ZonedDeviceDescriptorLength, GetLastError());
            goto exit;
        }
    }

    RtlZeroMemory(DeviceList[Index].ZonedDeviceDescriptor, DeviceList[Index].ZonedDeviceDescriptorLength);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             &query,
                             sizeof(STORAGE_PROPERTY_QUERY),
                             DeviceList[Index].ZonedDeviceDescriptor,
                             DeviceList[Index].ZonedDeviceDescriptorLength,
                             &returnedLength,
                             NULL
                             );
    _tprintf(_T("\n"));

    if (!result || (returnedLength == 0)) {

        _tprintf(_T("\tDeviceSmrTest: Get Zoned Device Descriptor using IOCTL_STORAGE_QUERY_PROPERTY failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((DeviceList[Index].ZonedDeviceDescriptor->Version != sizeof(STORAGE_ZONED_DEVICE_DESCRIPTOR)) ||
        (DeviceList[Index].ZonedDeviceDescriptor->Size < sizeof(STORAGE_ZONED_DEVICE_DESCRIPTOR))) {

        _tprintf(_T("\tDeviceSmrTest: Get Zoned Device Descriptor using IOCTL_STORAGE_QUERY_PROPERTY failed. - data descriptor header not valid.\n"));
        goto exit;
    }

    if ((DeviceList[Index].ZonedDeviceDescriptor->Size > DeviceList[Index].ZonedDeviceDescriptorLength) ||
        (DeviceList[Index].ZonedDeviceDescriptor->ZoneGroupCount > zoneGroupCount)) {
        //
        // Buffer is too small. Calculate needed size and retry.
        //
        free(DeviceList[Index].ZonedDeviceDescriptor);
        DeviceList[Index].ZonedDeviceDescriptor = NULL;

        DeviceList[Index].ZonedDeviceDescriptorLength = max(DeviceList[Index].ZonedDeviceDescriptor->Size,
                                                             (FIELD_OFFSET(STORAGE_ZONED_DEVICE_DESCRIPTOR, ZoneGroup) +
                                                              sizeof(STORAGE_ZONE_GROUP) * DeviceList[Index].ZonedDeviceDescriptor->ZoneGroupCount));
        
        goto Retry;
    }

    _tprintf(_T("\tSMR Device Information from IOCTL_STORAGE_QUERY_PROPERTY:\n"));

    _tprintf(_T("\tSMR Device Type: %s.\n"), GetSmrDeviceTypeString(DeviceList[Index].ZonedDeviceDescriptor->DeviceType));
    _tprintf(_T("\t\tDisk Logical/Physical Sector Size: %d / %d.\n"), DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector, DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerPhysicalSector);

    if ((DeviceList[Index].ZonedDeviceDescriptor->DeviceType != ZonedDeviceTypeHostManaged) &&
        (DeviceList[Index].ZonedDeviceDescriptor->DeviceType != ZonedDeviceTypeHostAware)) {

        goto exit;
    }

    _tprintf(_T("\t\tSMR Total Zones Count: %I64d.\n"), DeviceList[Index].ZonedDeviceDescriptor->ZoneCount);

    if (DeviceList[Index].ZonedDeviceDescriptor->DeviceType == ZonedDeviceTypeHostManaged) {

        _tprintf(_T("\t\tMax Open Zone Count: %d.\n"), DeviceList[Index].ZonedDeviceDescriptor->ZoneAttributes.SequentialRequiredZone.MaxOpenZoneCount);
        _tprintf(_T("\t\tUnrestricted Read in Write Required Zone: %s.\n"), DeviceList[Index].ZonedDeviceDescriptor->ZoneAttributes.SequentialRequiredZone.UnrestrictedRead ? _T("Yes") : _T("No"));
    } else if (DeviceList[Index].ZonedDeviceDescriptor->DeviceType == ZonedDeviceTypeHostAware) {

        _tprintf(_T("\t\tOptimal Open Zone Count: %d.\n"), DeviceList[Index].ZonedDeviceDescriptor->ZoneAttributes.SequentialPreferredZone.OptimalOpenZoneCount);
    }

    _tprintf(_T("\n\tZones Layout:\n"));

    for (i = 0; i < DeviceList[Index].ZonedDeviceDescriptor->ZoneGroupCount; i++) {
        _tprintf(_T("\t\tCount: %d, Size: %I64d, Type: %s.\n"), 
                 DeviceList[Index].ZonedDeviceDescriptor->ZoneGroup[i].ZoneCount,
                 DeviceList[Index].ZonedDeviceDescriptor->ZoneGroup[i].ZoneSize,
                 GetZoneTypeString(DeviceList[Index].ZonedDeviceDescriptor->ZoneGroup[i].ZoneType));
    }

exit:

    _tprintf(_T("\n"));

    return;
}

VOID
DeviceGetZonesInformation(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA,
    _In_ BOOLEAN            All,
    _Inout_updates_(*BufferLength) PUCHAR Buffer,
    _In_ PULONG             BufferLength
    )
/*++

Routine Description:

    Issuing IOCTL to get Zoned Device Property.

Arguments:

    DeviceList - 
    Index - 
    StartingLBA - 
    All - 
    Buffer - 
    BufferLength - 

Return Value:

    None

--*/
{
    BOOL    result;
    ULONG   returnedLength = 0;

    ULONG   inputBufferLength = 0;
    PUCHAR  inputBuffer = NULL;

    //
    // Input buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES, DEVICE_DSM_REPORT_ZONES_PARAMETERS,
    // and a DEVICE_DATA_SET_RANGE.
    //
    inputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES) + sizeof(DEVICE_DSM_REPORT_ZONES_PARAMETERS) + sizeof(DEVICE_DATA_SET_RANGE);

    inputBuffer = (PUCHAR)malloc(inputBufferLength);

    if (inputBuffer == NULL) {

        _tprintf(_T("DeviceGetZonesInformation: Allocate InputBuffer Failed.\n"));

        goto exit;
    }

    RtlZeroMemory(inputBuffer, inputBufferLength);
    RtlZeroMemory(Buffer, *BufferLength);

    //
    // Initialize input data structure for ReportZones.
    //
    {
        PDEVICE_MANAGE_DATA_SET_ATTRIBUTES dsmAttributes = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES)inputBuffer;
        PDEVICE_DSM_REPORT_ZONES_PARAMETERS reportZonesParameters = NULL;

        dsmAttributes->Size = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
        dsmAttributes->Action = DeviceDsmAction_ReportZones;
        dsmAttributes->ParameterBlockOffset = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
        dsmAttributes->ParameterBlockLength = sizeof(DEVICE_DSM_REPORT_ZONES_PARAMETERS);

        reportZonesParameters = (PDEVICE_DSM_REPORT_ZONES_PARAMETERS)((PUCHAR)dsmAttributes + dsmAttributes->ParameterBlockOffset);
        reportZonesParameters->Size = sizeof(DEVICE_DSM_REPORT_ZONES_PARAMETERS);
        reportZonesParameters->ReportOption = ZoneConditionAny;

        if (All) {

            dsmAttributes->Flags = DEVICE_DSM_FLAG_ENTIRE_DATA_SET_RANGE;
            dsmAttributes->DataSetRangesOffset = 0;
            dsmAttributes->DataSetRangesLength = 0;
        } else {

            PDEVICE_DATA_SET_RANGE lbaRange = NULL;

            dsmAttributes->Flags = 0;
            dsmAttributes->DataSetRangesOffset = dsmAttributes->ParameterBlockOffset + dsmAttributes->ParameterBlockLength;
            dsmAttributes->DataSetRangesLength = sizeof(DEVICE_DATA_SET_RANGE);

            lbaRange = (PDEVICE_DATA_SET_RANGE)((PUCHAR)dsmAttributes + dsmAttributes->DataSetRangesOffset);
            lbaRange->StartingOffset = StartingLBA;
            lbaRange->LengthInBytes = 256 * 1024 * 1024;
        }
    }

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES,
                             inputBuffer,
                             inputBufferLength,
                             Buffer,
                             *BufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
    
        DWORD LastErr = GetLastError();
        if (LastErr == ERROR_MORE_DATA && 
            (!All)) {
            _tprintf(_T("\tDeviceGetZonesInformation: Get Zone information succeeded. (Ignoring ERROR_MORE_DATA error since we just needed single zone info.)\n"));
        } else {
            _tprintf(_T("\tDeviceGetZonesInformation: Get Zone information failed. Error Code %d.\n"),LastErr );
        }
    }

exit:

    if (inputBuffer != NULL) {
        free(inputBuffer);
        inputBuffer = NULL;
    }

    *BufferLength = returnedLength;

    return;
}

VOID
DeviceGetAllZonesInformation(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index
    )
/*++

Routine Description:

    Issuing IOCTL to get Zoned Device Property.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    ULONG   returnedLength = 0;

    ULONG   outputBufferLength = 0;
    PUCHAR  outputBuffer = NULL;

    _tprintf(_T("\tDeviceDsmAction_ReportZones Test to retrieve all zones:\n"));

    //
    // Output buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT, DEVICE_DSM_REPORT_ZONES_DATA and one or more STORAGE_ZONE_DESCRIPTIOR.
    // A 8TB disk may have 32K zones with 256MB zone size. Thus needs at least 32K * sizeof(STORAGE_ZONE_DESCRIPTIOR) to contain zones information.
    // Allocate 64K zones as initial value.
    //
    outputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT) + FIELD_OFFSET(DEVICE_DSM_REPORT_ZONES_DATA, ZoneDescriptors) + sizeof(STORAGE_ZONE_DESCRIPTIOR) * 64 * 1024;

    outputBuffer = (PUCHAR)malloc(outputBufferLength);

    if (outputBuffer == NULL) {

        _tprintf(_T("DeviceGetZonesInformation: Allocate OutputBuffer Failed.\n"));

        goto exit;
    }

    RtlZeroMemory(outputBuffer, outputBufferLength);
    returnedLength = outputBufferLength;

    DeviceGetZonesInformation(DeviceList, Index, 0, TRUE, outputBuffer, &returnedLength);

    if (returnedLength == 0) {

        _tprintf(_T("\tDeviceGetZonesInformation: Get Zone information failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    {
        PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT dsmOutput = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)outputBuffer;
        PDEVICE_DSM_REPORT_ZONES_DATA reportZonesData = (PDEVICE_DSM_REPORT_ZONES_DATA)((PUCHAR)dsmOutput + dsmOutput->OutputBlockOffset);
        ULONG startZone = 0;
        ULONG i;

        if ((dsmOutput->Size != sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)) ||
            (dsmOutput->OutputBlockOffset < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT))) {

            _tprintf(_T("\tDeviceGetZonesInformation: DSM Output buffer header value invalid.\n"));
            goto exit;
        }

        _tprintf(_T("\t\tSMR Total Zones Count: %I64d.\n"), reportZonesData->ZoneCount);

        switch (reportZonesData->Attributes) {

        case ZonesAttributeTypeAndLengthMayDifferent:
            _tprintf(_T("\t\tSMR Zones's type and size can be different. \n"));
            break;

        case ZonesAttributeTypeSameLengthSame:
            _tprintf(_T("\t\tSMR Zones's type and size are same. \n"));
            break;

        case ZonesAttributeTypeSameLastZoneLengthDifferent:
            _tprintf(_T("\t\tSMR Zones's type are same and the last zone size is different from others. \n"));
            break;

        case ZonesAttributeTypeMayDifferentLengthSame:
            _tprintf(_T("\t\tSMR Zones's type may be different but the size are same. \n"));
            break;

        default:
            _tprintf(_T("\t\tSMR Zones's type and size attribute is not valid. \n"));
            break;

        }

        for (i = 0; i < reportZonesData->ZoneCount; i++) {

            if ((sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT) + FIELD_OFFSET(DEVICE_DSM_REPORT_ZONES_DATA, ZoneDescriptors) + sizeof(STORAGE_ZONE_DESCRIPTIOR) * (i + 1)) > returnedLength) {
                //
                // The next entry will exceed the returned buffer. Print current entry and bail out.
                //
                _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"), 
                         startZone,
                         i,
                         (i - startZone + 1),
                         reportZonesData->ZoneDescriptors[i].ZoneSize,
                         GetZoneTypeString(reportZonesData->ZoneDescriptors[i].ZoneType),
                         GetZoneConditionString(reportZonesData->ZoneDescriptors[i].ZoneCondition)
                         );
                break;
            }

            if ((reportZonesData->ZoneDescriptors[i].ZoneSize != reportZonesData->ZoneDescriptors[i + 1].ZoneSize) ||
                (reportZonesData->ZoneDescriptors[i].ZoneType != reportZonesData->ZoneDescriptors[i + 1].ZoneType) ||
                (reportZonesData->ZoneDescriptors[i].ZoneCondition != reportZonesData->ZoneDescriptors[i + 1].ZoneCondition)) {

                _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"), 
                         startZone,
                         i,
                         (i - startZone + 1),
                         reportZonesData->ZoneDescriptors[i].ZoneSize,
                         GetZoneTypeString(reportZonesData->ZoneDescriptors[i].ZoneType),
                         GetZoneConditionString(reportZonesData->ZoneDescriptors[i].ZoneCondition)
                         );

                startZone = i + 1;
            }
        }

        if ((i == reportZonesData->ZoneCount) &&
            (startZone < reportZonesData->ZoneCount)) {

            _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"), 
                        startZone,
                        i - 1,
                        (i - startZone),
                        reportZonesData->ZoneDescriptors[i - 1].ZoneSize,
                        GetZoneTypeString(reportZonesData->ZoneDescriptors[i - 1].ZoneType),
                        GetZoneConditionString(reportZonesData->ZoneDescriptors[i - 1].ZoneCondition)
                        );
        }

    }

exit:

    _tprintf(_T("\n"));

    if (outputBuffer != NULL) {
        free(outputBuffer);
        outputBuffer = NULL;
    }

    return;
}

VOID
DeviceResetWritePointer(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA,
    _In_ BOOLEAN            All
    )
/*++

Routine Description:

    Issuing IOCTL to get Zoned Device Property.

Arguments:

    DeviceList - 
    Index - 
    StartingLBA- 
    All - 

Return Value:

    None

--*/
{
    BOOL    result;
    ULONG   returnedLength = 0;

    ULONG   inputBufferLength = 0;
    PUCHAR  inputBuffer = NULL;

    ULONG   outputBufferLength = 0;
    PUCHAR  outputBuffer = NULL;

    //
    // Input buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES, and a DEVICE_DATA_SET_RANGE.
    //
    inputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES) + sizeof(DEVICE_DATA_SET_RANGE);

    inputBuffer = (PUCHAR)malloc(inputBufferLength);

    if (inputBuffer == NULL) {

        _tprintf(_T("DeviceResetWritePointer: Allocate InputBuffer Failed.\n"));

        goto exit;
    }

    RtlZeroMemory(inputBuffer, inputBufferLength);

    //
    // Initialize input data structure for reseting write pointer.
    //
    {
        PDEVICE_MANAGE_DATA_SET_ATTRIBUTES dsmAttributes = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES)inputBuffer;

        dsmAttributes->Size = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
        dsmAttributes->Action = DeviceDsmAction_ResetWritePointer;

        if (All) {

            dsmAttributes->Flags = DEVICE_DSM_FLAG_ENTIRE_DATA_SET_RANGE;
            dsmAttributes->DataSetRangesOffset = 0;
            dsmAttributes->DataSetRangesLength = 0;
        } else {

            PDEVICE_DATA_SET_RANGE lbaRange = NULL;

            dsmAttributes->Flags = 0;
            dsmAttributes->DataSetRangesOffset = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
            dsmAttributes->DataSetRangesLength = sizeof(DEVICE_DATA_SET_RANGE);

            lbaRange = (PDEVICE_DATA_SET_RANGE)((PUCHAR)dsmAttributes + dsmAttributes->DataSetRangesOffset);
            lbaRange->StartingOffset = StartingLBA;
        }
    }

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES,
                             inputBuffer,
                             inputBufferLength,
                             NULL,
                             0,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength != 0)) {

        _tprintf(_T("\tDeviceResetWritePointer: Reset Write Pointer failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    if (All) {

        _tprintf(_T("\tReset Write Pointers for whole disk succeeded.\n"));
    }
    else {

        _tprintf(_T("\tReset Write Pointers for starting LBA - 0x%I64X succeeded.\n"), StartingLBA);
    }

    //
    // Read the zone and check if the write pointer is at the desired place.
    //
    {
        //
        // Output buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT, DEVICE_DSM_REPORT_ZONES_DATA including one STORAGE_ZONE_DESCRIPTIOR.
        //
        outputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT) + sizeof(DEVICE_DSM_REPORT_ZONES_DATA);

        outputBuffer = (PUCHAR)malloc(outputBufferLength);

        if (outputBuffer == NULL) {

            _tprintf(_T("\tDeviceResetWritePointer: Allocate zone information buffer Failed.\n"));

            goto exit;
        }

        RtlZeroMemory(outputBuffer, outputBufferLength);
        returnedLength = outputBufferLength;
        if (StartingLBA != 0) {
            StartingLBA = ALIGN_DOWN_BY(StartingLBA, (256*1024*1024));
        }

        DeviceGetZonesInformation(DeviceList, Index, StartingLBA, FALSE, outputBuffer, &returnedLength);

        if (returnedLength == 0) {

            _tprintf(_T("\tDeviceResetWritePointer: Get Zone information failed. Error Code %d.\n"), GetLastError());
            goto exit;
        }

        //
        // Validate the returned data.
        //
        {
            PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT dsmOutput = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)outputBuffer;
            PDEVICE_DSM_REPORT_ZONES_DATA reportZonesData = (PDEVICE_DSM_REPORT_ZONES_DATA)((PUCHAR)dsmOutput + dsmOutput->OutputBlockOffset);

            if ((dsmOutput->Size != sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)) ||
                (dsmOutput->OutputBlockOffset < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT))) {

                _tprintf(_T("\t\tDeviceResetWritePointer: DSM Output buffer header value invalid.\n"));
                goto exit;
            }

            _tprintf(_T("\t\tZone information: SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s, \n\t\tSTART Offset - 0x%I64X, WRITE POINTER OFFSET - 0x%I64X, Reset Recommended: %s.\n"), 
                        reportZonesData->ZoneDescriptors[0].ZoneSize,
                        GetZoneTypeString(reportZonesData->ZoneDescriptors[0].ZoneType),
                        GetZoneConditionString(reportZonesData->ZoneDescriptors[0].ZoneCondition),
                        StartingLBA,
                        reportZonesData->ZoneDescriptors[0].WritePointerOffset,
                        reportZonesData->ZoneDescriptors[0].ResetWritePointerRecommend ? _T("Yes") : _T("No")
                        );
        }
    }

exit:

    _tprintf(_T("\n"));

    if (inputBuffer != NULL) {
        free(inputBuffer);
        inputBuffer = NULL;
    }

    if (outputBuffer != NULL) {
        free(outputBuffer);
        outputBuffer = NULL;
    }

    return;
}

VOID
DeviceZoneOperation(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ DEVICE_DATA_MANAGEMENT_SET_ACTION Action,
    _In_ ULONGLONG          StartingLBA,
    _Inout_updates_(InputBufferLength) PUCHAR InputBuffer,
    _In_ ULONG              InputBufferLength,
    _Inout_updates_(OutputBufferLength) PUCHAR OutputBuffer,
    _In_ ULONG              OutputBufferLength
    )
/*++

Routine Description:

    Issuing IOCTL to perform zone action and validate the zone condition afterwards.

Arguments:

    DeviceList -
    Index -
    Action -
    StartingLBA -
    InputBuffer - 
    InputBufferLength - 
    OutputBuffer -
    OutputBufferLength -

Return Value:

    None

--*/
{
    BOOL    result;
    ULONG   returnedLength = 0;
    PDEVICE_MANAGE_DATA_SET_ATTRIBUTES dsmAttributes = NULL;
    PDEVICE_DATA_SET_RANGE lbaRange = NULL;

    PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT dsmOutput = NULL;
    PDEVICE_DSM_REPORT_ZONES_DATA reportZonesData = NULL;

    dsmAttributes = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES)InputBuffer;
    dsmOutput = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)OutputBuffer;

    //
    // Set up input parameters.
    //
    RtlZeroMemory(InputBuffer, InputBufferLength);

    dsmAttributes->Size = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
    dsmAttributes->Action = Action;

    dsmAttributes->Flags = 0;
    dsmAttributes->DataSetRangesOffset = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
    dsmAttributes->DataSetRangesLength = sizeof(DEVICE_DATA_SET_RANGE);

    lbaRange = (PDEVICE_DATA_SET_RANGE)((PUCHAR)dsmAttributes + dsmAttributes->DataSetRangesOffset);
    lbaRange->StartingOffset = StartingLBA;

    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES,
                             InputBuffer,
                             InputBufferLength,
                             NULL,
                             0,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength != 0)) {

        _tprintf(_T("\tDeviceZoneOperation: %s failed. Error Code %d.\n"), GetZoneActionString(Action), GetLastError());
        goto exit;
    }

    //
    // Get current zone status
    //
    RtlZeroMemory(OutputBuffer, OutputBufferLength);
    returnedLength = OutputBufferLength;
    
    if (StartingLBA != 0) {
        StartingLBA = ALIGN_DOWN_BY(StartingLBA, (256*1024*1024));
    }

    DeviceGetZonesInformation(DeviceList, Index, StartingLBA, FALSE, OutputBuffer, &returnedLength);

    if (returnedLength == 0) {

        _tprintf(_T("\t\tDeviceZoneOperation: Get Zone information after %S failed. Error Code %d.\n"), GetZoneActionString(Action), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    reportZonesData = (PDEVICE_DSM_REPORT_ZONES_DATA)((PUCHAR)dsmOutput + dsmOutput->OutputBlockOffset);

    if ((dsmOutput->Size != sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)) ||
        (dsmOutput->OutputBlockOffset < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT))) {

        _tprintf(_T("\t\tDeviceZoneOperation: DSM Output buffer header value invalid.\n"));
        goto exit;
    }

    if (((Action == DeviceDsmAction_OpenZone) && (reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionExplicitlyOpened)) ||
        ((Action == DeviceDsmAction_FinishZone) && (reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionFull)) ||
        ((Action == DeviceDsmAction_CloseZone) && ((reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionClosed) ||
                                                   (reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionEmpty)))) {

        _tprintf(_T("\tDeviceZoneOperation: %s succeeded. \n"), GetZoneActionString(Action));
    } else {

        _tprintf(_T("\tDeviceZoneOperation: %s failed as zone condition is not expected.\n"), GetZoneActionString(Action));
    }

    _tprintf(_T("\t\tAfter %s: Zone CONDITION - %s.\n"), GetZoneActionString(Action), GetZoneConditionString(reportZonesData->ZoneDescriptors[0].ZoneCondition));

exit:

    _tprintf(_T("\n"));

    return;
}

VOID
DeviceZoneOperationTest(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA
    )
/*++

Routine Description:

    Issuing IOCTL to open zone, close zone, finish zone.

Arguments:

    DeviceList -
    Index -
    StartingLBA-

Return Value:

    None

--*/
{
    ULONG   returnedLength = 0;

    ULONG   inputBufferLength = 0;
    PUCHAR  inputBuffer = NULL;

    ULONG   outputBufferLength = 0;
    PUCHAR  outputBuffer = NULL;

    PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT dsmOutput = NULL;
    PDEVICE_DSM_REPORT_ZONES_DATA reportZonesData = NULL;

    //
    // Input buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES, and a DEVICE_DATA_SET_RANGE.
    //
    inputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES) + sizeof(DEVICE_DATA_SET_RANGE);

    inputBuffer = (PUCHAR)malloc(inputBufferLength);

    if (inputBuffer == NULL) {

        _tprintf(_T("\tDeviceZoneOperationTest: Allocate InputBuffer Failed.\n"));

        goto exit;
    }

    //
    // Output buffer consists of DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT, DEVICE_DSM_REPORT_ZONES_DATA including one STORAGE_ZONE_DESCRIPTIOR.
    //
    outputBufferLength = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT) + sizeof(DEVICE_DSM_REPORT_ZONES_DATA);

    outputBuffer = (PUCHAR)malloc(outputBufferLength);

    if (outputBuffer == NULL) {

        _tprintf(_T("\tDeviceZoneOperationTest: Allocate zone information buffer Failed.\n"));

        goto exit;
    }

    dsmOutput = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)outputBuffer;

    //
    // Get current zone status
    //
    RtlZeroMemory(outputBuffer, outputBufferLength);
    returnedLength = outputBufferLength;

    if (StartingLBA != 0) {
        StartingLBA = ALIGN_DOWN_BY(StartingLBA, (256*1024*1024));
    }

    DeviceGetZonesInformation(DeviceList, Index, StartingLBA, FALSE, outputBuffer, &returnedLength);

    if (returnedLength == 0) {

        _tprintf(_T("\tDeviceZoneOperationTest: Get Zone information failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    _tprintf(_T("\tDeviceZoneOperationTest: Started on zone below:\n"));

    //
    // Validate the returned data.
    //
    reportZonesData = (PDEVICE_DSM_REPORT_ZONES_DATA)((PUCHAR)dsmOutput + dsmOutput->OutputBlockOffset);

    if ((dsmOutput->Size != sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT)) ||
        (dsmOutput->OutputBlockOffset < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES_OUTPUT))) {

        _tprintf(_T("\tDeviceZoneOperationTest: DSM Output buffer header value invalid.\n"));
        goto exit;
    }

    _tprintf(_T("\t\tZone information: SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s, \n\t\tSTART LBA - 0x%I64X, WRITE POINTER offset - 0x%I64X, Reset Recommended: %s.\n\n"),
                reportZonesData->ZoneDescriptors[0].ZoneSize,
                GetZoneTypeString(reportZonesData->ZoneDescriptors[0].ZoneType),
                GetZoneConditionString(reportZonesData->ZoneDescriptors[0].ZoneCondition),
                StartingLBA,
                reportZonesData->ZoneDescriptors[0].WritePointerOffset,
                reportZonesData->ZoneDescriptors[0].ResetWritePointerRecommend ? _T("Yes") : _T("No")
                );


    //
    // If the zone is opened, close it and validate the close condition.
    //
    if ((reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionImplicitlyOpened) ||
        (reportZonesData->ZoneDescriptors[0].ZoneCondition == ZoneConditionExplicitlyOpened)) {

        //
        // Close the zone
        //
        DeviceZoneOperation(DeviceList,
                            Index,
                            DeviceDsmAction_CloseZone,
                            StartingLBA,
                            inputBuffer,
                            inputBufferLength,
                            outputBuffer,
                            outputBufferLength
                            );
    }

    //
    // Now open the zone and validate the zone status
    //
    DeviceZoneOperation(DeviceList,
                        Index,
                        DeviceDsmAction_OpenZone,
                        StartingLBA,
                        inputBuffer,
                        inputBufferLength,
                        outputBuffer,
                        outputBufferLength
                        );

    //
    // Now close the zone and validate the closed condition.
    //
    DeviceZoneOperation(DeviceList,
                        Index,
                        DeviceDsmAction_CloseZone,
                        StartingLBA,
                        inputBuffer,
                        inputBufferLength,
                        outputBuffer,
                        outputBufferLength
                        );

    //
    // Now open the zone and validate the zone status
    //
    DeviceZoneOperation(DeviceList,
                        Index,
                        DeviceDsmAction_OpenZone,
                        StartingLBA,
                        inputBuffer,
                        inputBufferLength,
                        outputBuffer,
                        outputBufferLength
                        );

    //
    // Now finish the zone and validate the Finished condition.
    //
    DeviceZoneOperation(DeviceList,
                        Index,
                        DeviceDsmAction_FinishZone,
                        StartingLBA,
                        inputBuffer,
                        inputBufferLength,
                        outputBuffer,
                        outputBufferLength
                        );

exit:

    _tprintf(_T("\n"));

    if (inputBuffer != NULL) {
        free(inputBuffer);
        inputBuffer = NULL;
    }

    if (outputBuffer != NULL) {
        free(outputBuffer);
        outputBuffer = NULL;
    }

    return;
}

VOID
DeviceAtaGetZonesInformation(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA,
    _In_ BOOLEAN            All,
    _Inout_ PULONG          ErrorCount
    )
/*++

Routine Description:

    Issuing REPORT ZONES EXT command to get zones information.

Arguments:

    DeviceList -
    Index -
    StartingLBA -
    All -
    ErrorCount -

Return Value:

    None

--*/
{
    ULONG   bytesReturned = 0;
    BOOLEAN queryCompleted = FALSE;

    ULONG   reportZonesDataLength = 0;  // transfer buffer to allocate for getting current piece of zones information.
    PUCHAR  reportZonesDataBuffer = NULL;

    ULONG   outputBufferLength = 0;
    PUCHAR  outputBuffer = NULL;        // data buffer to allocate for holding all zones information.

    PATA_PT ataPt = NULL;
    PATA_PASS_THROUGH_EX passThru = NULL;

    PREPORT_ZONES_EXT_DATA reportZonesData = NULL;
    PATA_ZONE_DESCRIPTOR zoneDescriptor = NULL;
    ULONG zoneDescriptorIndex = 0;

    PREPORT_ZONES_EXT_DATA currentReportZonesData = NULL;
    PATA_ZONE_DESCRIPTOR currentZoneDescriptor = NULL;

    ULONGLONG queryStartLBA = StartingLBA;

    if (All) {
        //
        // Output buffer consists of ATA_PASS_THROUGH_EX, REPORT_ZONES_EXT_DATA and one or more ATA_ZONE_DESCRIPTOR.
        // A 8TB disk may have 32K zones with 256MB zone size. Thus needs at least 32K * sizeof(STORAGE_ZONE_DESCRIPTIOR) to contain zones information.
        // Allocate 64K zones as initial value.
        //
        reportZonesDataLength = sizeof(REPORT_ZONES_EXT_DATA) + sizeof(ATA_ZONE_DESCRIPTOR) * 64 * 1024;
        outputBufferLength = sizeof(ATA_PASS_THROUGH_EX) + DeviceList[Index].AdapterDescriptor.MaximumTransferLength;
    } else {
        reportZonesDataLength = sizeof(REPORT_ZONES_EXT_DATA) + sizeof(ATA_ZONE_DESCRIPTOR);
        outputBufferLength = sizeof(ATA_PASS_THROUGH_EX) + ATA_BLOCK_SIZE;
    }

    reportZonesDataBuffer = (PUCHAR)malloc(reportZonesDataLength);
    outputBuffer = (PUCHAR)malloc(outputBufferLength);

    if ((reportZonesDataBuffer == NULL) || (outputBuffer == NULL)) {

        _tprintf(_T("DeviceGetAtaZonesInformation: Allocate buffer failed.\n"));

        goto exit;
    }

    RtlZeroMemory(reportZonesDataBuffer, reportZonesDataLength);

    reportZonesData = (PREPORT_ZONES_EXT_DATA)(reportZonesDataBuffer + sizeof(ATA_PASS_THROUGH_EX));
    zoneDescriptor = (PATA_ZONE_DESCRIPTOR)(reportZonesDataBuffer + sizeof(ATA_PASS_THROUGH_EX) + sizeof(REPORT_ZONES_EXT_DATA));

    ataPt = (PATA_PT)outputBuffer;
    passThru = &ataPt->AtaPassThrough;

    currentReportZonesData = (PREPORT_ZONES_EXT_DATA)(outputBuffer + sizeof(ATA_PASS_THROUGH_EX));
    currentZoneDescriptor = (PATA_ZONE_DESCRIPTOR)(outputBuffer + sizeof(ATA_PASS_THROUGH_EX) + sizeof(REPORT_ZONES_EXT_DATA));

    //
    // In case of getting all zones, we need to issue multiple commands,
    // as the typical max adapter transfer size is only 128KB.
    //
    _tprintf(_T("\tIssue REPORT ZONES EXT command to retrieve zone information.\n"));
    
    for (; !queryCompleted; ) {

        RtlZeroMemory(outputBuffer, outputBufferLength);

        BuildReportZonesExtCommand(passThru,
                                   ATA_REPORT_ZONES_OPTION_LIST_ALL_ZONES,
                                   queryStartLBA,
                                   ((outputBufferLength - sizeof(ATA_PASS_THROUGH_EX)) / ATA_BLOCK_SIZE));

        bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                           passThru,
                                           outputBufferLength,
                                           outputBufferLength,
                                           FALSE);

        if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
            _tprintf(_T("\t\t Error %d: REPORT ZONES EXT command failed to retrieve data. ErrorCode: %d\n"), ++(*ErrorCount), GetLastError());

            queryCompleted = TRUE;
        } else {
            ULONG currentDescriptorIndex;

            if (!All) {
                queryCompleted = TRUE;
            }

            //
            // Copy data into reportZonesDataBuffer, and find the Starting LBA for next command.
            //
            if (reportZonesData->ZoneListLength == 0) {
                RtlCopyMemory(reportZonesData, currentReportZonesData, sizeof(REPORT_ZONES_EXT_DATA));
            }

            for (currentDescriptorIndex = 0; (sizeof(REPORT_ZONES_EXT_DATA) + (currentDescriptorIndex + 1) * sizeof(ATA_ZONE_DESCRIPTOR)) <= bytesReturned; currentDescriptorIndex++) {

                if (currentZoneDescriptor[currentDescriptorIndex].ZoneLength == 0) {
                    //
                    // End of information, bail out the loop.
                    //
                    queryCompleted = TRUE;
                    break;
                }

                if ((currentDescriptorIndex > 0) && 
                    (currentZoneDescriptor[currentDescriptorIndex].ZoneStartLBA <= currentZoneDescriptor[currentDescriptorIndex - 1].ZoneStartLBA)) {
                    //
                    // End of information, bail out the loop.
                    //
                    queryCompleted = TRUE;
                    break;
                }

                if ((sizeof(REPORT_ZONES_EXT_DATA) + (zoneDescriptorIndex + 1) * sizeof(ATA_ZONE_DESCRIPTOR)) <= reportZonesDataLength) {
                    RtlCopyMemory(&zoneDescriptor[zoneDescriptorIndex], &currentZoneDescriptor[currentDescriptorIndex], sizeof(ATA_ZONE_DESCRIPTOR));

                    queryStartLBA = currentZoneDescriptor[currentDescriptorIndex].ZoneStartLBA + currentZoneDescriptor[currentDescriptorIndex].ZoneLength;
                    zoneDescriptorIndex++;
                } else {
                    //
                    // End of buffer, bail out the loop.
                    // NOTE: may consider to have a free/re-allocate logic in next version, in case the default size is not big enough.
                    //
                    queryCompleted = TRUE;
                    break;
                }
            }
        }
    }

    //
    // Validate the returned data.
    //
    {
        ULONG startZone = 0;
        ULONG i;


        _tprintf(_T("\t\tSMR Total Zones Count: %I64d.\n"), reportZonesData->ZoneListLength / sizeof(ATA_ZONE_DESCRIPTOR));

        switch (reportZonesData->SAME) {

        case ATA_ZONES_TYPE_AND_LENGTH_MAY_DIFFERENT:
            _tprintf(_T("\t\tSMR Zones's type and size can be different. \n"));
            break;

        case ATA_ZONES_TYPE_SAME_LENGTH_SAME:
            _tprintf(_T("\t\tSMR Zones's type and size are same. \n"));
            break;

        case ATA_ZONES_TYPE_SAME_LAST_ZONE_LENGTH_DIFFERENT:
            _tprintf(_T("\t\tSMR Zones's type are same and the last zone size is different from others. \n"));
            break;

        case ATA_ZONES_TYPE_MAY_DIFFERENT_LENGTH_SAME:
            _tprintf(_T("\t\tSMR Zones's type may be different but the size are same. \n"));
            break;

        default:
            _tprintf(_T("\t\tSMR Zones's type and size attribute is not valid. \n"));
            break;

        }

        for (i = 0; i < (reportZonesData->ZoneListLength / sizeof(ATA_ZONE_DESCRIPTOR)); i++) {

            if ((sizeof(ATA_PASS_THROUGH_EX) + sizeof(REPORT_ZONES_EXT_DATA) + sizeof(ATA_ZONE_DESCRIPTOR) * (i + 1)) > reportZonesDataLength) {
                //
                // The next entry will exceed the returned buffer. Print current entry and bail out.
                //
                _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"),
                         startZone,
                         i,
                         (i - startZone + 1),
                         zoneDescriptor[i].ZoneLength * DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector,
                         GetZoneTypeString(zoneDescriptor[i].ZoneType),
                         GetZoneConditionString(zoneDescriptor[i].ZoneCondition)
                         );
                break;
            }

            if ((zoneDescriptor[i].ZoneLength != zoneDescriptor[i + 1].ZoneLength) ||
                (zoneDescriptor[i].ZoneType != zoneDescriptor[i + 1].ZoneType) ||
                (zoneDescriptor[i].ZoneCondition != zoneDescriptor[i + 1].ZoneCondition)) {

                _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"),
                         startZone,
                         i,
                         (i - startZone + 1),
                         zoneDescriptor[i].ZoneLength * DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector,
                         GetZoneTypeString(zoneDescriptor[i].ZoneType),
                         GetZoneConditionString(zoneDescriptor[i].ZoneCondition)
                         );

                startZone = i + 1;
            }
        }

        if ((i == (reportZonesData->ZoneListLength / sizeof(ATA_ZONE_DESCRIPTOR))) &&
            (startZone < (reportZonesData->ZoneListLength / sizeof(ATA_ZONE_DESCRIPTOR)))) {

            _tprintf(_T("\t\tZone %d to Zone %d (count: %d): SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s.\n"),
                     startZone,
                     i - 1,
                     (i - startZone),
                     zoneDescriptor[i].ZoneLength * DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector,
                     GetZoneTypeString(zoneDescriptor[i].ZoneType),
                     GetZoneConditionString(zoneDescriptor[i].ZoneCondition)
                     );
        }

    }

exit:

    _tprintf(_T("\n"));

    if (outputBuffer != NULL) {
        free(outputBuffer);
        outputBuffer = NULL;
    }

    if (reportZonesDataBuffer != NULL) {
        free(reportZonesDataBuffer);
        reportZonesDataBuffer = NULL;
    }

    return;
}

VOID
DeviceAtaResetWritePointer(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA,
    _In_ BOOLEAN            All,
    _Inout_ PULONG          ErrorCount
    )
/*++

Routine Description:

    Issuing Reset Write Pointer Ext command to device.

Arguments:

    DeviceList - 
    Index - 
    StartingLBA- 
    All - 
    ErrorCount - 

Return Value:

    None

--*/
{
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned;
    ULONGLONG            zoneStartLBA = 0;

    if (!All && (StartingLBA != 0)) {
        //
        // Get zone start LBA.
        //

        BuildReportZonesExtCommand(passThru,
                                   ATA_REPORT_ZONES_OPTION_LIST_ALL_ZONES,
                                   StartingLBA,
                                   1);

        bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE);

        if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
            _tprintf(_T("\tError %d: REPORT ZONES EXT command failed to retrieve data.\n"), ++(*ErrorCount));
        } else {
            PREPORT_ZONES_EXT_DATA reportZonesData = NULL;
            PATA_ZONE_DESCRIPTOR zoneDescriptor = NULL;

            reportZonesData = (PREPORT_ZONES_EXT_DATA)(ataPt.Buffer);
            zoneDescriptor = (PATA_ZONE_DESCRIPTOR)(ataPt.Buffer + sizeof(REPORT_ZONES_EXT_DATA));

            zoneStartLBA = zoneDescriptor->ZoneStartLBA;
        }
    }

    RtlZeroMemory(&ataPt, sizeof(ATA_PT));

    BuildZoneOperationCommand(passThru, ZM_ACTION_RESET_WRITE_POINTER, zoneStartLBA, All);

    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );

    if (bytesReturned < FIELD_OFFSET(ATA_PT, Buffer)) {
        _tprintf(_T("\tError %d: RESET WRITE POINTER EXT command failed.\n"), ++(*ErrorCount));
    } else {

        if (All) {
            _tprintf(_T("\tRESET WRITE POINTER EXT command for whole disk succeeded.\n"));
        } else {
            _tprintf(_T("\tRESET WRITE POINTER EXT command for zone contains Starting LBA: %I64d succeeded.\n"), StartingLBA);
        }

        if (!All) {
            //
            // Get zone condition. Should be EMPTY after reset.
            //
            RtlZeroMemory(&ataPt, sizeof(ATA_PT));

            BuildReportZonesExtCommand(passThru,
                                       ATA_REPORT_ZONES_OPTION_LIST_ALL_ZONES,
                                       StartingLBA,
                                       1);

            bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                               passThru,
                                               sizeof(ATA_PT),
                                               sizeof(ATA_PT),
                                               FALSE);

            if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
                _tprintf(_T("\tError %d: REPORT ZONES EXT command failed to retrieve data.\n"), ++(*ErrorCount));
            } else {
                PREPORT_ZONES_EXT_DATA reportZonesData = NULL;
                PATA_ZONE_DESCRIPTOR zoneDescriptor = NULL;

                reportZonesData = (PREPORT_ZONES_EXT_DATA)(ataPt.Buffer);
                zoneDescriptor = (PATA_ZONE_DESCRIPTOR)(ataPt.Buffer + sizeof(REPORT_ZONES_EXT_DATA));

                if ((zoneDescriptor->ZoneType == ATA_ZONE_TYPE_SEQUENTIAL_WRITE_REQUIRED) ||
                    (zoneDescriptor->ZoneType == ATA_ZONE_TYPE_SEQUENTIAL_WRITE_PREFERRED)) {

                    if ((zoneDescriptor->ZoneCondition != ATA_ZONE_CONDITION_EMPTY) ||
                        (zoneDescriptor->ZoneStartLBA != zoneDescriptor->WritePointerLBA)) {

                        _tprintf(_T("\tError %d: Zone state not expected after RESET WRITE POINTER EXT command. Condition: %s, Zone Start LBA: %I64d, Write Pointer LBA:  %I64d, Zone Length: %I64d\n"),
                                 ++(*ErrorCount),
                                 GetZoneConditionString(zoneDescriptor->ZoneCondition),
                                 zoneDescriptor->ZoneStartLBA,
                                 zoneDescriptor->WritePointerLBA,
                                 zoneDescriptor->ZoneLength
                                 );
                    }
                }
            }
        }
    }

    _tprintf(_T("\n"));

    return;
}

VOID
DeviceAtaZoneOperation(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ UCHAR              Action,
    _In_ ULONGLONG          StartingLBA,
    _Inout_ PATA_PT         AtaPt
)
/*++

Routine Description:

    Issuing ATA Zone Management to perform zone action and validate the zone condition afterwards.

Arguments:

    DeviceList -
    Index -
    Action -
    StartingLBA -
    AtaPt -

Return Value:

    None

--*/
{
    ULONG   bytesReturned = 0;
    PATA_PASS_THROUGH_EX passThru = &AtaPt->AtaPassThrough;
    PREPORT_ZONES_EXT_DATA reportZonesData = NULL;
    PATA_ZONE_DESCRIPTOR zoneDescriptor = NULL;

    reportZonesData = (PREPORT_ZONES_EXT_DATA)(AtaPt->Buffer);
    zoneDescriptor = (PATA_ZONE_DESCRIPTOR)(AtaPt->Buffer + sizeof(REPORT_ZONES_EXT_DATA));

    RtlZeroMemory(AtaPt, sizeof(ATA_PT));

    BuildZoneOperationCommand(passThru, Action, StartingLBA, FALSE);

    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );


    if (bytesReturned < FIELD_OFFSET(ATA_PT, Buffer)) {

        _tprintf(_T("\tDeviceAtaZoneOperation: %s failed. Error Code %d.\n"), GetAtaZoneActionString(Action), GetLastError());
        goto exit;
    }

    //
    // Get current zone status
    //
    RtlZeroMemory(AtaPt, sizeof(ATA_PT));

    BuildReportZonesExtCommand(passThru,
                               ATA_REPORT_ZONES_OPTION_LIST_ALL_ZONES,
                               StartingLBA,
                               1);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE);

    if (bytesReturned < FIELD_OFFSET(ATA_PT, Buffer)) {

        _tprintf(_T("\t\tDeviceAtaZoneOperation: Get Zone information after %S failed. Error Code %d.\n"), GetAtaZoneActionString(Action), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if (((Action == ZM_ACTION_OPEN_ZONE) && (zoneDescriptor->ZoneCondition == ATA_ZONE_CONDITION_EXPLICITLY_OPENED)) ||
        ((Action == ZM_ACTION_FINISH_ZONE) && (zoneDescriptor->ZoneCondition == ATA_ZONE_CONDITION_FULL)) ||
        ((Action == ZM_ACTION_CLOSE_ZONE) && ((zoneDescriptor->ZoneCondition == ATA_ZONE_CONDITION_CLOSED) ||
                                                   (zoneDescriptor->ZoneCondition == ATA_ZONE_CONDITION_EMPTY)))) {

        _tprintf(_T("\tDeviceAtaZoneOperation: %s succeeded. \n"), GetAtaZoneActionString(Action));
    } else {

        _tprintf(_T("\tDeviceAtaZoneOperation: %s failed as zone condition is not expected.\n"), GetAtaZoneActionString(Action));
    }

    _tprintf(_T("\t\tAfter %s: Zone CONDITION - %s.\n"), GetAtaZoneActionString(Action), GetZoneConditionString(zoneDescriptor->ZoneCondition));

exit:

    _tprintf(_T("\n"));

    return;
}

VOID
DeviceAtaZoneOperationTest(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ ULONGLONG          StartingLBA
    )
/*++

Routine Description:

    Issuing ATA command to open zone, close zone, finish zone and validate the result.

Arguments:

    DeviceList -
    Index -
    StartingLBA-

Return Value:

    None

--*/
{
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    PREPORT_ZONES_EXT_DATA reportZonesData = NULL;
    PATA_ZONE_DESCRIPTOR zoneDescriptor = NULL;
    ULONG                bytesReturned = 0;
    ULONGLONG            zoneStartLBA = 0;
    
    reportZonesData = (PREPORT_ZONES_EXT_DATA)(ataPt.Buffer);
    zoneDescriptor = (PATA_ZONE_DESCRIPTOR)(ataPt.Buffer + sizeof(REPORT_ZONES_EXT_DATA));

    //
    // Convert from bytes to sectors.
    //
    StartingLBA /= DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector;

    //
    // Get zone information
    //
    BuildReportZonesExtCommand(passThru,
                               ATA_REPORT_ZONES_OPTION_LIST_ALL_ZONES,
                               StartingLBA,
                               1);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE);

    if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {

        _tprintf(_T("\tDeviceAtaZoneOperationTest: Get Zone information failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    _tprintf(_T("\tDeviceAtaZoneOperationTest: Started on zone below:\n"));

    //
    // Validate the returned data.
    //
    _tprintf(_T("\t\tZone information: SIZE: %I64d (bytes), TYPE - %s, CONDITION - %s, \n\t\tSTART LBA - 0x%I64X, WRITE POINTER - 0x%I64X, Reset Recommended: %s.\n\n"),
                zoneDescriptor->ZoneLength * DeviceList[Index].DeviceAccessAlignmentDescriptor.BytesPerLogicalSector,
                GetZoneTypeString(zoneDescriptor->ZoneType),
                GetZoneConditionString(zoneDescriptor->ZoneCondition),
                zoneDescriptor->ZoneStartLBA,
                zoneDescriptor->WritePointerLBA,
                zoneDescriptor->Reset ? _T("Yes") : _T("No")
                );

    zoneStartLBA = zoneDescriptor->ZoneStartLBA;

    //
    // If the zone is opened, close it and validate the close condition.
    //
    if ((zoneDescriptor->ZoneCondition == ZoneConditionImplicitlyOpened) ||
        (zoneDescriptor->ZoneCondition == ZoneConditionExplicitlyOpened)) {

        //
        // Close the zone
        //
        DeviceAtaZoneOperation(DeviceList,
                               Index,
                               ZM_ACTION_CLOSE_ZONE,
                               zoneStartLBA,
                               &ataPt
                               );
    }

    //
    // Now open the zone and validate the zone status
    //
    DeviceAtaZoneOperation(DeviceList,
                           Index,
                           ZM_ACTION_OPEN_ZONE,
                           zoneStartLBA,
                           &ataPt
                           );

    //
    // Now close the zone and validate the closed condition.
    //
    DeviceAtaZoneOperation(DeviceList,
                           Index,
                           ZM_ACTION_CLOSE_ZONE,
                           zoneStartLBA,
                           &ataPt
                           );

    //
    // Now open the zone and validate the zone status
    //
    DeviceAtaZoneOperation(DeviceList,
                           Index,
                           ZM_ACTION_OPEN_ZONE,
                           zoneStartLBA,
                           &ataPt
                           );

    //
    // Now finish the zone and validate the Finished condition.
    //
    DeviceAtaZoneOperation(DeviceList,
                           Index,
                           ZM_ACTION_FINISH_ZONE,
                           zoneStartLBA,
                           &ataPt
                           );

exit:

    _tprintf(_T("\n"));

    return;
}


VOID
DeviceSmrAtaLogo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index
    )
/*++

Routine Description:

    NOTE: This test applies to win10 and up.

    The test will send ATA pass-through commands to disk to validate the SMR disk requirements.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned = 0;
    PIDENTIFY_DEVICE_DATA identifyData = NULL;
    ULONG                errorCount = 0;
    BOOLEAN              result = FALSE;

    //
    // Get Identify Device Data.
    //
    BuildIdentifyDeviceCommand(passThru);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );

    identifyData = (PIDENTIFY_DEVICE_DATA)(ataPt.Buffer);

    if (bytesReturned < (FIELD_OFFSET(ATA_PT, Buffer) + ATA_BLOCK_SIZE)) {
        _tprintf(_T("\tDeviceSmrAtaLogo: Error - Couldn't get Identify Device Data.\n\n"));
    } else {

        _tprintf(_T("\nSMR Disk ATA command tests.\n"));

        _tprintf(_T("\t Disk General Information:\n"));
        _tprintf(_T("\t\t CommandSet Spec Support: %s\n"), GetAtaCommandSetMajorVersionString(identifyData->MajorRevision));
        _tprintf(_T("\t\t Transport Spec Support:  %s\n"), GetAtaTransportMajorVersionString(identifyData->TransportMajorVersion.MajorVersion, identifyData->TransportMajorVersion.TransportType));

        _tprintf(_T("\t\t Transport Type: %s, Form Factor: %s, "),
                 GetAtaTransportTypeString(identifyData->TransportMajorVersion.MajorVersion, identifyData->TransportMajorVersion.TransportType),
                 GetAtaFormFactorString(identifyData->NominalFormFactor));

        if (identifyData->NominalMediaRotationRate == 0x1) {
            _tprintf(_T("SSD \n"));
        } else if ((identifyData->NominalMediaRotationRate >= 0x401) && (identifyData->NominalMediaRotationRate <= 0xFFFE)) {
            _tprintf(_T("RPM: %d\n"), identifyData->NominalMediaRotationRate);
        } else {
            _tprintf(_T("RPM: Not Reported\n"), identifyData->NominalMediaRotationRate);
        }

        _tprintf(_T("\n"));

        //
        // Common requirements from Identify Device Data fields.
        //
        _tprintf(_T("\t\t Disk supports General feature set.\n"));

        if ((identifyData->CommandSetSupport.WordValid == 1) &&
            (identifyData->CommandSetSupport.GpLogging == 1)) {

            _tprintf(_T("\t\t Disk supports General Purpose Logging feature set.\n"));

            if (identifyData->CommandSetActive.GpLogging == 0) {
                _tprintf(_T("\t\t\t Warning: General Purpose Logging feature set is not enabled.\n"));
            }
        } else {
            _tprintf(_T("\t\t Error %d: Disk doesn't support General Purpose Logging feature set.\n"), ++errorCount);
        }

        if ((identifyData->CommandSetSupport.WordValid83 == 1) &&
            (identifyData->CommandSetActive.BigLba == 1)) {

            _tprintf(_T("\t\t Disk supports 48bit Address feature set.\n"));
        } else {
            _tprintf(_T("\t\t Error %d: Disk doesn't support 48bit Address feature set.\n"), ++errorCount);
        }

        if (identifyData->SerialAtaCapabilities.NCQ == 1) {
            _tprintf(_T("\t\t Disk supports NCQ feature set.\n"));
        } else {
            _tprintf(_T("\t\t Error %d: Disk doesn't support NCQ feature set.\n"), ++errorCount);
        }

        if (identifyData->SerialAtaFeaturesSupported.NCQAutosense == 1) {
            _tprintf(_T("\t\t Disk supports NCQ AutoSense.\n"));
        } else {
            _tprintf(_T("\t\t Error %d: Disk doesn't support NCQ AutoSense.\n"), ++errorCount);
        }

        if ((identifyData->SCTCommandTransport.Supported == 1) &&
            (identifyData->SCTCommandTransport.WriteSameSuported == 1)) {
            _tprintf(_T("\t\t Disk supports SCT Write Same command.\n"));
        } else {
            _tprintf(_T("\t\t Error %d: Disk doesn't support SCT Write Same command.\n"), ++errorCount);
        }

        //
        // Host Managed disk requirements.
        //
        if (DeviceList[Index].ZonedDeviceDescriptor->DeviceType == ZonedDeviceTypeHostManaged) {

            if (identifyData->AdditionalSupported.ZonedCapabilities != 0) {
                _tprintf(_T("\t\t Error %d: Zoned field value is not zero: %d.\n"), ++errorCount, identifyData->AdditionalSupported.ZonedCapabilities);
            }
        } else if (DeviceList[Index].ZonedDeviceDescriptor->DeviceType == ZonedDeviceTypeHostAware) {

            if (identifyData->AdditionalSupported.ZonedCapabilities != 1) {
                _tprintf(_T("\t\t Error %d: Zoned field value is not 01h: %d.\n"), ++errorCount, identifyData->AdditionalSupported.ZonedCapabilities);
            }

            //identifyData->CommandSetActive.Words119_120Valid;   //CommandSet Extension
            //identifyData->CommandSetSupportExt.SenseDataReporting;
            //identifyData->CommandSetActiveExt.SenseDataReporting;
        }
    }

    //
    // Validate the support of Zoned Device Information page in Identify Device Data log.
    //
    result = DeviceReadLogDirectory(DeviceList, Index, FALSE);

    if (!result) {
        _tprintf(_T("\t\t Error %d: READ LOG EXT command failed to retrieve Log Directory or Log Directory doesn't have correct version.\n"), ++errorCount);
    }

    //
    // Read Log - IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS
    // This is to see if the Capabilities page is supported or not.
    //
    RtlZeroMemory(passThru, sizeof(ATA_PT));

    BuildReadLogExCommand(passThru, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS, 0, 1, 0);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                        passThru,
                                        sizeof(ATA_PT),
                                        sizeof(ATA_PT),
                                        FALSE);

    if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
        _tprintf(_T("\t\t Error %d: READ LOG EXT command failed to retrieve IDENTIFY DEVICE DATA log.\n"), ++errorCount);

        result = FALSE;
    } else {
        _tprintf(_T("\t\t READ LOG EXT command to retrieve IDENTIFY DEVICE DATA log succeeded.\n"));
    }

    //
    // Read Log - IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS & IDE_GP_LOG_IDENTIFY_DEVICE_DATA_SUPPORTED_CAPABILITIES_PAGE
    //
    RtlZeroMemory(passThru, sizeof(ATA_PT));

    BuildReadLogExCommand(passThru, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ZONED_DEVICE_INFORMATION_PAGE, 1, 0);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                        passThru,
                                        sizeof(ATA_PT),
                                        sizeof(ATA_PT),
                                        FALSE);

    if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
        _tprintf(_T("\t\t Error %d: READ LOG - IDENTIFY DEVICE DATA log - Zone Device Information page failed.\n"), ++errorCount);
        result = FALSE;
    } else {
        PIDENTIFY_DEVICE_DATA_LOG_PAGE_ZONED_DEVICE_INFO page = (PIDENTIFY_DEVICE_DATA_LOG_PAGE_ZONED_DEVICE_INFO)ataPt.Buffer;

        if (page->Header.RevisionNumber != IDE_GP_LOG_VERSION) {
            _tprintf(_T("\t\t Error %d: READ LOG - IDENTIFY DEVICE DATA log - Zone Device Information page: Revision (should be 1): %d\n"), ++errorCount, page->Header.RevisionNumber);
            result = FALSE;
        }
        
        if (page->Header.PageNumber != IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ZONED_DEVICE_INFORMATION_PAGE) {
            _tprintf(_T("\t\t Error %d: READ LOG - IDENTIFY DEVICE DATA log - Zone Device Information page: Page Number (should be 9): %d\n"), ++errorCount, page->Header.PageNumber);
            result = FALSE;
        } else {
            _tprintf(_T("\t\t READ LOG EXT command to retrieve IDENTIFY DEVICE DATA log - Zoned Device Information page succeeded.\n"));

            if (page->Version.Valid == 1) {
                if (page->Version.ZacMinorVersion == 0x05CF) {
                    _tprintf(_T("\t\t ZAC revision 05\n"));
                } else if (page->Version.ZacMinorVersion == 0xA36C) {
                    _tprintf(_T("\t\t ZAC revision 04\n"));
                } else if (page->Version.ZacMinorVersion == 0xB6E8) {
                    _tprintf(_T("\t\t ZAC revision 01\n"));
                } else {
                    _tprintf(_T("\t\t ZAC revision value invalid: 0x%X\n"), page->Version.ZacMinorVersion);
                }
            } else {
                _tprintf(_T("\t\t Warning: Disk does not report ZAC revision value.\n"));
            }

            if (DeviceList[Index].ZonedDeviceDescriptor->DeviceType == ZonedDeviceTypeHostManaged) {
                if (page->ZonedDeviceCapabilities.Valid == 1) {
                    _tprintf(_T("\t\t URSWRZ (Unrestricted read in Sequential Write Required Zone) value: %d.\n"), page->ZonedDeviceCapabilities.URSWRZ);
                }

                if ((page->MaxNumberOfOpenSequentialWriteRequiredZones.Valid == 1) &&
                    (page->MaxNumberOfOpenSequentialWriteRequiredZones.Number > 0)) {
                    _tprintf(_T("\t\t Max Number of Open Sequential Write Required Zones: %d.\n"), page->MaxNumberOfOpenSequentialWriteRequiredZones.Number);
                } else {
                    _tprintf(_T("\t\t Error %d: Host Managed disk does not report Max Number of Open Sequential Write Required Zones value.\n"), ++errorCount);
                }
            } else {
                if ((page->ZonedDeviceCapabilities.Valid == 1) &&
                    (page->ZonedDeviceCapabilities.URSWRZ == 1)) {
                    _tprintf(_T("\t\t Error %d: Host Aware disk should not report URSWRZ (Unrestricted read in Sequential Write Required Zone) value: %d.\n"), ++errorCount, page->ZonedDeviceCapabilities.URSWRZ);
                }

                if ((page->OptimalNumberOfOpenSequentialWritePreferredZones.Valid == 1) &&
                    (page->OptimalNumberOfOpenSequentialWritePreferredZones.Number > 0)) {
                    _tprintf(_T("\t\t Optimal Number of Open Sequential Write Preferred Zones: %d.\n"), page->OptimalNumberOfOpenSequentialWritePreferredZones.Number);
                } else {
                    _tprintf(_T("\t\t Error %d: Host Aware disk does not report Optimal Number of Open Sequential Write Preferred Zones value.\n"), ++errorCount);
                }

                if ((page->OptimalNumberOfNonSequentiallyWrittenSequentialWritePreferredZones.Valid == 1) &&
                    (page->OptimalNumberOfNonSequentiallyWrittenSequentialWritePreferredZones.Number > 0)) {
                    _tprintf(_T("\t\t Optimal Number of non-sequentially written Sequential Write Preferred Zones: %d.\n"), page->OptimalNumberOfNonSequentiallyWrittenSequentialWritePreferredZones.Number);
                }
            }
        }
    }

    _tprintf(_T("\n"));

    //
    // Report Zones
    //
    DeviceAtaGetZonesInformation(DeviceList, Index, 0, TRUE, &errorCount);

    //
    // Reset Write Pointer tests
    //
    DeviceAtaResetWritePointer(DeviceList, Index, 0, TRUE, &errorCount);                // Reset Write Pointers for how disk.

    DeviceAtaResetWritePointer(DeviceList, Index, 0x8000000000, FALSE, &errorCount);    // Reset Write Pointer for a zone at ~550G.

    //
    // Report Zones
    //
    DeviceAtaGetZonesInformation(DeviceList, Index, 0, TRUE, &errorCount);

    //
    // Zone operations test (open, close, finish)
    //
    DeviceAtaZoneOperationTest(DeviceList, Index, 0x10000000000);                       // Test zone operations for zone at ~1T.

    return;
}

VOID
DeviceSmrScsiLogo(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index
    )
/*++

Routine Description:

    NOTE: This test applies to win10 and up.

    The test will send SCSI pass-through commands to disk to validate the SMR disk requirements.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DeviceList);
    UNREFERENCED_PARAMETER(Index);

//exit:

    return;
}

VOID
DeviceSmrTest(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index
    )
    /*++

    Routine Description:

    NOTE: This test applies to win10 and up.

    The test includes following portions:
    1. Test IOCTL interfaces for: Get Zoned Device Property, Get Zones Information, Reset Write Pointer.
    2. Test ATA pass-through interface.
    3. Test SCSI pass-through interface, also SCSI to ATA translation in case of an ATA device underneath.

    Arguments:

    DeviceList -
    Index -

    Return Value:

    None

    --*/
{
    PSTORAGE_DEVICE_DESCRIPTOR deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;

    //
    // Get SMR disk property
    //
    DeviceGetZonedDeviceProperty(DeviceList, Index);

    if ((DeviceList[Index].ZonedDeviceDescriptor->DeviceType != ZonedDeviceTypeHostManaged) &&
        (DeviceList[Index].ZonedDeviceDescriptor->DeviceType != ZonedDeviceTypeHostAware)) {

        return;
    }

    //
    // Test the IOCTL pipes.
    //
    {
        _tprintf(_T("SMR IOCTL Tests:\n"));

        DeviceGetAllZonesInformation(DeviceList, Index);

        DeviceResetWritePointer(DeviceList, Index, 0, TRUE);

        DeviceResetWritePointer(DeviceList, Index, 0x8000001000, FALSE);    // Reset Write Pointer somewhere at ~550G.

        DeviceZoneOperationTest(DeviceList, Index, 0x10000000000);           // Test zone operations for zone at ~1T.
    }

    //
    // Test ATA commands.
    //
    if (IsAtaDevice(deviceDesc->BusType)) {

        DeviceSmrAtaLogo(DeviceList, Index);
    }

    //
    // Test SCSI commands.
    //
    DeviceSmrScsiLogo(DeviceList, Index);

    return;
}


//
// topology
//


__inline
TCHAR*
GetHealthStatusString(
    STORAGE_COMPONENT_HEALTH_STATUS HealthStatus
)
{
    switch (HealthStatus) {
    case HealthStatusUnknown:
        return _T("Unknown");
    case HealthStatusNormal:
        return _T("Normal");
    case HealthStatusThrottled:
        return _T("Throttled");
    case HealthStatusWarning:
        return _T("Warning");
    case HealthStatusDisabled:
        return _T("Disabled");
    case HealthStatusFailed:
        return _T("Failed");
    default:
        return _T("Unknown");
    };
}

__inline
TCHAR*
GetCommandProtocolString(
    STORAGE_PROTOCOL_TYPE Protocol
)
{
    switch (Protocol) {
    case ProtocolTypeUnknown:
        return _T("Unknown");
    case ProtocolTypeScsi:
        return _T("SCSI");
    case ProtocolTypeAta:
        return _T("ATA");
    case ProtocolTypeNvme:
        return _T("NVMe");
    case ProtocolTypeSd:
        return _T("SD");
    case ProtocolTypeProprietary:
        return _T("Vendor");
    default:
        return _T("Unknown");
    };
}

__inline
TCHAR*
GetFormFactorString(
    STORAGE_DEVICE_FORM_FACTOR FormFactor
)
{
    switch (FormFactor) {
    case FormFactorUnknown:
        return _T("Unknown");
    case FormFactor3_5:
        return _T("3.5inch");
    case FormFactor2_5:
        return _T("2.5inch");
    case FormFactor1_8:
        return _T("1.8inch");
    case FormFactor1_8Less:
        return _T("<1.8inch");
    case FormFactorEmbedded:
        return _T("Embedded");
    case FormFactorMemoryCard:
        return _T("Memory Card");
    case FormFactormSata:
        return _T("mSATA");
    case FormFactorM_2:
        return _T("M.2");
    case FormFactorPCIeBoard:
        return _T("PCIe Card");
    case FormFactorDimm:
        return _T("DIMM");
    default:
        return _T("Unknown");
    };
}

VOID
DeviceTopologyTest(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex,
    _In_ BOOLEAN      AdapterTopology
)
/*
Issue IOCTL_STORAGE_QUERY_PROPERTY with StorageDevicePhysicalTopologyProperty or StorageAdapterPhysicalTopologyProperty.

*/
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;
    ULONG   retryCount = 0;

    PSTORAGE_PROPERTY_QUERY query = NULL;

    PSTORAGE_PHYSICAL_TOPOLOGY_DESCRIPTOR topologyDescr = NULL;
    PSTORAGE_PHYSICAL_NODE_DATA nodeData = NULL;
    PSTORAGE_PHYSICAL_ADAPTER_DATA adapterData = NULL;
    PSTORAGE_PHYSICAL_DEVICE_DATA deviceData = NULL;

    ULONG   i, j;

    //
    // Allocate buffer for use.
    //
    bufferLength = 0x1000;

RetryOnce:
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DeviceTopologyTest: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    topologyDescr = (PSTORAGE_PHYSICAL_TOPOLOGY_DESCRIPTOR)buffer;

    query->PropertyId = AdapterTopology ? StorageAdapterPhysicalTopologyProperty : StorageDevicePhysicalTopologyProperty;
    query->QueryType = PropertyStandardQuery;

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
        _tprintf(_T("DeviceTopologyTest: IOCTL failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if (topologyDescr->Version != sizeof(STORAGE_PHYSICAL_TOPOLOGY_DESCRIPTOR)) {
        _tprintf(_T("DeviceTopologyTest: data descriptor version not valid.\n"));
        goto exit;
    }

    if ((topologyDescr->Size > bufferLength) && (retryCount == 0)) {
        free(buffer);
        buffer = NULL;
        bufferLength = topologyDescr->Size;

        retryCount++;

        goto RetryOnce;
    }

    if (topologyDescr->Size > bufferLength) {
        _tprintf(_T("DeviceTopologyTest: data descriptor size changed after first retry.\n"));
        goto exit;
    }

    //
    // Print result.
    //
    _tprintf(_T("Node Count: %d \n\n"), topologyDescr->NodeCount);

    for (i = 0; i < topologyDescr->NodeCount; i++) {
        nodeData = &topologyDescr->Node[i];

        _tprintf(_T("\t Node ID: %d \n"), nodeData->NodeId);
        _tprintf(_T("\t Adapter Count: %d \n"), nodeData->AdapterCount);
        _tprintf(_T("\t Disk Count: %d \n\n"), nodeData->DeviceCount);

        //
        // The Adapter Count should be 0 or 1.
        //
        if (nodeData->AdapterCount > 0) {
            if ((nodeData->AdapterDataOffset < sizeof(STORAGE_PHYSICAL_NODE_DATA)) ||
                (nodeData->AdapterDataLength < sizeof(STORAGE_PHYSICAL_ADAPTER_DATA))) {

                _tprintf(_T("\t\t Adapter data offset/length is not set correctly. Offset:%d, Length %d \n"), nodeData->AdapterDataOffset, nodeData->AdapterDataLength);
            } else {
                TCHAR tempChar[64] = {0};

                adapterData = (PSTORAGE_PHYSICAL_ADAPTER_DATA)((PUCHAR)nodeData + nodeData->AdapterDataOffset);

                _tprintf(_T("\t\t Adapter ID: %d \n"), adapterData->AdapterId);
                _tprintf(_T("\t\t Health Status: %s \n"), GetHealthStatusString(adapterData->HealthStatus));
                _tprintf(_T("\t\t Command Protocol: %s \n"), GetCommandProtocolString(adapterData->CommandProtocol));
                _tprintf(_T("\t\t Spec Version: %X \n"), adapterData->SpecVersion.AsUlong);

                //
                // Vendor
                //
                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)adapterData->Vendor,
                                    -1,
                                    tempChar,
                                    sizeof(tempChar) / sizeof(tempChar[0]) - 1
                );

                _tprintf(_T("\t\t Vendor: %s \n"), tempChar);

                //
                // Model
                //
                RtlZeroMemory(tempChar, sizeof(tempChar));

                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)adapterData->Model,
                                    -1,
                                    tempChar,
                                    sizeof(tempChar) / sizeof(tempChar[0]) - 1
                );

                _tprintf(_T("\t\t Model: %s \n"), tempChar);

                //
                // Firmware Revision
                //
                RtlZeroMemory(tempChar, sizeof(tempChar));

                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)adapterData->FirmwareRevision,
                                    -1,
                                    tempChar,
                                    sizeof(tempChar) / sizeof(tempChar[0]) - 1
                );

                _tprintf(_T("\t\t Firmware: %s \n"), tempChar);

                //
                // Physical Location
                //
                RtlZeroMemory(tempChar, sizeof(tempChar));

                MultiByteToWideChar(CP_ACP,
                                    0,
                                    (LPCCH)adapterData->PhysicalLocation,
                                    -1,
                                    tempChar,
                                    sizeof(tempChar) / sizeof(tempChar[0]) - 1
                );

                _tprintf(_T("\t\t Physical Location: %s \n"), tempChar);

                _tprintf(_T("\t\t Enclosure Connected: %s \n\n"), adapterData->ExpanderConnected ? _T("Yes") : _T("No"));
            }
        }

        //
        // The Device Count should be at least 1.
        //
        if (nodeData->DeviceCount == 0) {
            _tprintf(_T("\t\t Data Error - Disk Count is ZERO \n"));
        } else {
            if ((nodeData->DeviceDataOffset < (sizeof(STORAGE_PHYSICAL_NODE_DATA) + nodeData->AdapterCount * sizeof(STORAGE_PHYSICAL_ADAPTER_DATA))) ||
                (nodeData->DeviceDataLength < sizeof(STORAGE_PHYSICAL_DEVICE_DATA))) {

                _tprintf(_T("\t\t Disk data offset/length is not set correctly. Offset:%d, Length.\n"), nodeData->DeviceDataOffset, nodeData->DeviceDataLength);
            } else {
                for (j = 0; j < nodeData->DeviceCount; j++) {
                    TCHAR tempChar[64] = {0};

                    deviceData = (PSTORAGE_PHYSICAL_DEVICE_DATA)((PUCHAR)nodeData + nodeData->DeviceDataOffset + j * sizeof(STORAGE_PHYSICAL_DEVICE_DATA));

                    _tprintf(_T("\t\t Device ID: %d \n"), deviceData->DeviceId);
                    _tprintf(_T("\t\t Device Role: %X (Bit 0 - Cache; Bit 1 - Tiering; Bit 2 - Data) \n"), deviceData->Role);
                    _tprintf(_T("\t\t Health Status: %s \n"), GetHealthStatusString(deviceData->HealthStatus));
                    _tprintf(_T("\t\t Command Protocol: %s \n"), GetCommandProtocolString(deviceData->CommandProtocol));
                    _tprintf(_T("\t\t Spec Version: %X \n"), deviceData->SpecVersion.AsUlong);
                    _tprintf(_T("\t\t Form Factor: %s \n"), GetFormFactorString(deviceData->FormFactor));

                    //
                    // Vendor
                    //
                    MultiByteToWideChar(CP_ACP,
                                        0,
                                        (LPCCH)deviceData->Vendor,
                                        -1,
                                        tempChar,
                                        sizeof(tempChar) / sizeof(tempChar[0]) - 1
                    );

                    _tprintf(_T("\t\t Vendor: %s \n"), tempChar);

                    //
                    // Model
                    //
                    RtlZeroMemory(tempChar, sizeof(tempChar));

                    MultiByteToWideChar(CP_ACP,
                                        0,
                                        (LPCCH)deviceData->Model,
                                        -1,
                                        tempChar,
                                        sizeof(tempChar) / sizeof(tempChar[0]) - 1
                    );

                    _tprintf(_T("\t\t Model: %s \n"), tempChar);

                    //
                    // Firmware Revision
                    //
                    RtlZeroMemory(tempChar, sizeof(tempChar));

                    MultiByteToWideChar(CP_ACP,
                                        0,
                                        (LPCCH)deviceData->FirmwareRevision,
                                        -1,
                                        tempChar,
                                        sizeof(tempChar) / sizeof(tempChar[0]) - 1
                    );

                    _tprintf(_T("\t\t Firmware: %s \n"), tempChar);

                    //
                    // Physical Location
                    //
                    RtlZeroMemory(tempChar, sizeof(tempChar));

                    MultiByteToWideChar(CP_ACP,
                                        0,
                                        (LPCCH)deviceData->PhysicalLocation,
                                        -1,
                                        tempChar,
                                        sizeof(tempChar) / sizeof(tempChar[0]) - 1
                    );

                    _tprintf(_T("\t\t Physical Location: %s \n"), tempChar);

                    _tprintf(_T("\t\t Capacity: %d GB \n\n"), (deviceData->Capacity / 1024) / 1024);
                }
            }

        }
    }

exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}


//
// trim
//

BOOLEAN
TrimTest(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex,
    _In_ PCOMMAND_OPTIONS CommandOptions
)
{
    BOOL        succeed;
    UCHAR       inputBuffer[128] = {0};
    UCHAR       outputBuffer[512] = {0};
    ULONG       returnedLength;

    PSTORAGE_PROPERTY_QUERY query;

    BOOLEAN supportsTrim = FALSE;

    // get trim descriptor
    query = (PSTORAGE_PROPERTY_QUERY)inputBuffer;
    query->PropertyId = StorageDeviceTrimProperty;
    query->QueryType = PropertyStandardQuery;

    succeed = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              query,
                              128,
                              outputBuffer,
                              512,
                              &returnedLength,
                              NULL
    );

    // get retrieved information for seek penalty and trim support.
    if (!succeed) {
        _tprintf(_T("Retrieved attribute - StorageDeviceTrimProperty failed, error: %d\n\n"), GetLastError());
    } else {
        PDEVICE_TRIM_DESCRIPTOR trimDescr;
        trimDescr = (PDEVICE_TRIM_DESCRIPTOR)outputBuffer;

        supportsTrim = trimDescr->TrimEnabled;

        _tprintf(_T("  Device %s TRIM attribute.\n"), supportsTrim ? _T("supports") : _T("does not support"));
    }

    // test TRIM commands.
    if (supportsTrim) {
        PDEVICE_MANAGE_DATA_SET_ATTRIBUTES dsmAttributes = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES)outputBuffer;
        PDEVICE_DATA_SET_RANGE dataSetRange = (PDEVICE_DATA_SET_RANGE)(outputBuffer + sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES));

        ZeroMemory(outputBuffer, 512);

        dsmAttributes->Size = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
        dsmAttributes->Action = DeviceDsmAction_Trim;
        dsmAttributes->ParameterBlockOffset = 0;
        dsmAttributes->ParameterBlockLength = 0;
        dsmAttributes->DataSetRangesOffset = sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES);
        // only one range
        dsmAttributes->DataSetRangesLength = sizeof(DEVICE_DATA_SET_RANGE);


        dataSetRange->StartingOffset = CommandOptions->Parameters.StartingOffset;
        dataSetRange->LengthInBytes = CommandOptions->Parameters.LengthInBytes;

        succeed = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                                  IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES,
                                  outputBuffer,
                                  512,
                                  outputBuffer,
                                  512,
                                  &returnedLength,
                                  NULL
        );
        if (succeed) {
            _tprintf(_T("  Device TRIM succeeded.\n"));
        } else {
            _tprintf(_T("  Device TRIM test failed. IOCTL error:0n%d.\n"), GetLastError());
        }

    }

    _tprintf(_T("\n"));

    return (BOOLEAN)succeed;
}

VOID
DeviceReadLog (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        Index,
    _In_ ULONG        LogValue,
    _In_ ULONG        LogSubValue
    )
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;

    PSTORAGE_PROPERTY_QUERY             query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA     protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR   protocolDataDescr = NULL;

    PSTORAGE_DEVICE_DESCRIPTOR          deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;

    if (!IsAtaDevice(deviceDesc->BusType) && !IsNVMeDevice(deviceDesc->BusType)) {
        _tprintf(_T("Only support for ATA or NVMe devices. \n"));
        goto exit;
    }

    //
    // Allocate buffer for use.
    //
    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    bufferLength += (IsAtaDevice(deviceDesc->BusType) ? IDE_GP_LOG_SECTOR_SIZE : NVME_MAX_LOG_SIZE);

    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DeviceReadLog: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = IsNVMeDevice(deviceDesc->BusType) ? StorageAdapterProtocolSpecificProperty : StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = IsNVMeDevice(deviceDesc->BusType) ? ProtocolTypeNvme : ProtocolTypeAta;
    protocolData->DataType = IsNVMeDevice(deviceDesc->BusType) ? NVMeDataTypeLogPage : AtaDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = LogValue;
    protocolData->ProtocolDataRequestSubValue = LogSubValue;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = (IsAtaDevice(deviceDesc->BusType) ? IDE_GP_LOG_SECTOR_SIZE : NVME_MAX_LOG_SIZE);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceReadLog: query property for log page failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceReadLog: data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if (protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) {
        _tprintf(_T("DeviceReadLog: ProtocolData Offset not valid.\n"));
        goto exit;
    }

    //
    // Print out log data
    //
    {
        ULONG   i = 0, j = 0;
        PUCHAR  logData = (PUCHAR)protocolData + protocolData->ProtocolDataOffset;

        for (i = 0; (i * 16) < protocolData->ProtocolDataLength; i++) {
            for (j = 0; j < 16; j++) {
                if ((i * 16 + j) >= protocolData->ProtocolDataLength) {
                    break;
                } else {
                    _tprintf(_T("%02X "), logData[i * 16 + j]);
                }
            }

            _tprintf(_T("\n"));

            if ((i * 16 + j) >= protocolData->ProtocolDataLength) {
                break;
            }
        }
    }

exit:

    return;
}

VOID
BuildReportZonesExtCommand(
    _In_ PATA_PASS_THROUGH_EX PassThru,
    _In_ UCHAR ReportingOptions,
    _In_ ULONGLONG StartingLBA,
    _In_ ULONG BlockCount
)
/*++
    Build REPORT ZONES EXT command

Parameters:
    PassThru            - should have been zero-ed.
    ReportingOptions    - Report Zones option value.
    StartingLBA         - Starting LBA
    BlockCount          - How many blocks (in 512 bytes)

Return Values:
    None

--*/
{
    PATA_COMMAND    currentCmdRegister = (PATA_COMMAND)PassThru->CurrentTaskFile;
    PATA_COMMAND    previousCmdRegister = (PATA_COMMAND)PassThru->PreviousTaskFile;

    //
    // Fills data fields in ATA_PASS_THROUGH_EX structure.
    //
    PassThru->Length = sizeof(ATA_PASS_THROUGH_EX);
    PassThru->AtaFlags = ATA_FLAGS_DRDY_REQUIRED;
    PassThru->AtaFlags |= ATA_FLAGS_DATA_IN;
    PassThru->AtaFlags |= ATA_FLAGS_48BIT_COMMAND;
    PassThru->TimeOutValue = 10;
    PassThru->DataTransferLength = BlockCount * ATA_BLOCK_SIZE;
    PassThru->DataBufferOffset = FIELD_OFFSET(ATA_PT, Buffer);

    //
    // Set up ATA command
    //
    //currentCmdRegister->Features = ZM_ACTION_REPORT_ZONES;
    previousCmdRegister->Features = ReportingOptions;

    //if (Partial) {
    //    previousCmdRegister->Features |= (1 << 7);  // Feature Bit 15 is PARTIAL bit.
    //}

    currentCmdRegister->SectorCount = (UCHAR)(BlockCount & 0xFF);
    previousCmdRegister->SectorCount = (UCHAR)((BlockCount >> 8) & 0xFF);

    currentCmdRegister->LbaLow = (UCHAR)((StartingLBA & 0x0000000000ff) >> 0);
    currentCmdRegister->LbaMid = (UCHAR)((StartingLBA & 0x00000000ff00) >> 8);
    currentCmdRegister->LbaHigh = (UCHAR)((StartingLBA & 0x000000ff0000) >> 16);
    previousCmdRegister->LbaLow = (UCHAR)((StartingLBA & 0x0000ff000000) >> 24);
    previousCmdRegister->LbaMid = (UCHAR)((StartingLBA & 0x00ff00000000) >> 32);
    previousCmdRegister->LbaHigh = (UCHAR)((StartingLBA & 0xff0000000000) >> 40);

    currentCmdRegister->Device.LBA = 1;
    //currentCmdRegister->Command = IDE_COMMAND_ZAC_MANAGEMENT_IN;

    return;
}

VOID
BuildZoneOperationCommand(
    _In_ PATA_PASS_THROUGH_EX PassThru,
    _In_ UCHAR Operation,
    _In_ ULONGLONG StartingLBA,
    _In_ BOOLEAN AllZones
)
/*++
    Build OPEN ZONE, CLOSE ZONE, FINISH ZONE or RESET WRITE POINTER command

Parameters:
    PassThru            - should have been zero-ed.
    Operation           - Open, Close, Finish Zone; or Reset Zone Pointer
    StartingLBA         - Zone Starting LBA.
    AllZones            - The operation targets to all write pointer zones of a disk.

Return Values:
    None

--*/
{
    PATA_COMMAND    currentCmdRegister = (PATA_COMMAND)PassThru->CurrentTaskFile;
    PATA_COMMAND    previousCmdRegister = (PATA_COMMAND)PassThru->PreviousTaskFile;

    //
    // Fills data fields in ATA_PASS_THROUGH_EX structure.
    //
    PassThru->Length = sizeof(ATA_PASS_THROUGH_EX);
    PassThru->AtaFlags = ATA_FLAGS_DRDY_REQUIRED;
    PassThru->AtaFlags |= ATA_FLAGS_48BIT_COMMAND;
    PassThru->TimeOutValue = 10;
    PassThru->DataTransferLength = 0;
    PassThru->DataBufferOffset = 0;

    //
    // Set up ATA command
    //
    currentCmdRegister->Features = Operation;

    if (AllZones) {
        previousCmdRegister->Features |= (1 << 0);      // Feature Bit 8 is ALL bit.
    }

    currentCmdRegister->LbaLow = (UCHAR)((StartingLBA & 0x0000000000ff) >> 0);
    currentCmdRegister->LbaMid = (UCHAR)((StartingLBA & 0x00000000ff00) >> 8);
    currentCmdRegister->LbaHigh = (UCHAR)((StartingLBA & 0x000000ff0000) >> 16);
    previousCmdRegister->LbaLow = (UCHAR)((StartingLBA & 0x0000ff000000) >> 24);
    previousCmdRegister->LbaMid = (UCHAR)((StartingLBA & 0x00ff00000000) >> 32);
    previousCmdRegister->LbaHigh = (UCHAR)((StartingLBA & 0xff0000000000) >> 40);

    currentCmdRegister->Device.LBA = 1;
    //currentCmdRegister->Command = IDE_COMMAND_ZAC_MANAGEMENT_OUT;

    return;
}

VOID
AtaDisplayDataBuffer(
    _In_ PATA_PT AtaPt,
    _In_ ULONG   BytesReturned,
    _In_ BOOLEAN IdentifyDeviceData
    )
{
    PIDENTIFY_DEVICE_DATA identifyData;

    ULONG   i;
    LONG    len;
    ULONG   count;
    PUCHAR  buffer;

    len = min(BytesReturned - FIELD_OFFSET(ATA_PT, Buffer), 512);

    if (len <= 0) {
        _tprintf(_T("No Data\n\n"));
        return;
    }

    buffer = AtaPt->Buffer;

    if (IdentifyDeviceData) {
        identifyData = (PIDENTIFY_DEVICE_DATA)(buffer);

        _tprintf(_T("\n"));
        _tprintf(_T("\t CommandSet Spec Support: %s\n"), GetAtaCommandSetMajorVersionString(identifyData->MajorRevision));
        _tprintf(_T("\t Transport Spec Support:  %s\n"), GetAtaTransportMajorVersionString(identifyData->TransportMajorVersion.MajorVersion, identifyData->TransportMajorVersion.TransportType));

        _tprintf(_T("\t Transport Type: %s, Form Factor: %s, "),
                 GetAtaTransportTypeString(identifyData->TransportMajorVersion.MajorVersion, identifyData->TransportMajorVersion.TransportType),
                 GetAtaFormFactorString(identifyData->NominalFormFactor));

        if (identifyData->NominalMediaRotationRate == 0x1) {
            _tprintf(_T("SSD \n"));
        } else if ((identifyData->NominalMediaRotationRate >= 0x401) && (identifyData->NominalMediaRotationRate <= 0xFFFE)) {
            _tprintf(_T("RPM: %d\n"), identifyData->NominalMediaRotationRate);
        } else {
            _tprintf(_T("RPM: Not Reported\n"));
        }

        _tprintf(_T("\t Trim Support: %s\n"), (identifyData->DataSetManagementFeature.SupportsTrim == 1) ? _T("Yes") : _T("No"));

        _tprintf(_T("\n"));
    }

    if (IdentifyDeviceData) {
        _tprintf(_T("IDENTIFY DEVICE DATA:\n"));
    }

    count = 0;

    for (i = 0; i< (ULONG)len; i++) {

        _tprintf(_T("%02x "), buffer[i]);

        count++;

        if ((count % 16) == 0) {
            _tprintf(_T("\n"));
        }
    }

    _tprintf(_T("\n"));

    return;
}

BOOL
DeviceFirmwareTestNvme(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
/*++

Routine Description:

    Query NVMe protocol specific information to determine if device supports firmware upgrade.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    BOOL    result = FALSE;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;

    //
    // Device should be able to return Log Page - Firmware Slot Information (03h)
    //
    PSTORAGE_PROPERTY_QUERY             query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA     protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR   protocolDataDescr = NULL;

    _tprintf(_T("NVMe - Check controller support of Firmware Slot Info Log.\n"));

    //
    // Allocate buffer for use.
    //
    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + sizeof(NVME_FIRMWARE_SLOT_INFO_LOG);
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("ERROR: allocate buffer for Firmware Slot Information (03h) Page failed.\n"));
        goto exit;
    }

    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageAdapterProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_FIRMWARE_SLOT_INFO;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_FIRMWARE_SLOT_INFO_LOG);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("ERROR: query property for Firmware Slot Information (03h) Page failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("ERROR: query property for Firmware Slot Information (03h) Page failed. Data descriptor header not valid.\n"));
        goto exit;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if (protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) {
        _tprintf(_T("ERROR: query property for Firmware Slot Information (03h) Page failed. ProtocolData Offset not valid.\n"));
        goto exit;
    }

    _tprintf(_T("NVMe - Query property for Firmware Slot Information (03h) Page succeeded.\n"));

exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return result;
}

VOID
DeviceNVMeQueryProtocolDataTest (
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
/*
    Issue IOCTL_STORAGE_QUERY_PROPERTY with StorageAdapterProtocolSpecificProperty and StorageDeviceProtocolSpecificProperty.

    STORAGE_PROTOCOL_TYPE - ProtocolTypeNvme

    NVMeDataTypeIdentify,       // Retrieved by command - IDENTIFY CONTROLLER or IDENTIFY NAMESPACE
    NVMeDataTypeLogPage,        // Retrieved by command - GET LOG PAGE
    NVMeDataTypeFeature,        // Retrieved by command - GET FEATURES

*/
{
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
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageAdapterProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_CONTROLLER;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = NVME_MAX_LOG_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < NVME_MAX_LOG_SIZE)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Identify Controller Data 
    //
    {
        PNVME_IDENTIFY_CONTROLLER_DATA identifyControllerData = (PNVME_IDENTIFY_CONTROLLER_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if ((identifyControllerData->VID == 0) ||
            (identifyControllerData->NN == 0)) {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Identify Controller Data not valid.\n"));
            goto exit;
        } else {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Identify Controller Data succeeded***.\n"));
        }
    }

    //
    // Initialize query data structure to get Identify Namespace Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;      // not actually needed.
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = NVME_MAX_LOG_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < NVME_MAX_LOG_SIZE)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Identify Namespace Data 
    //
    {
        PNVME_IDENTIFY_NAMESPACE_DATA identifyNamespaceData = (PNVME_IDENTIFY_NAMESPACE_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if ((identifyNamespaceData->NSZE == 0) ||
            (identifyNamespaceData->NCAP == 0)) {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Identify Namespace Data not valid.\n"));
            goto exit;
        } else {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Identify Namespace Data succeeded***.\n"));
        }
    }

    //
    // get Identify Namespace Data for namespace 1 through controller.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageAdapterProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;
    protocolData->ProtocolDataRequestSubValue = 1;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = NVME_MAX_LOG_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < NVME_MAX_LOG_SIZE)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Namespace Data - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Identify Namespace Data 
    //
    {
        PNVME_IDENTIFY_NAMESPACE_DATA identifyNamespaceData = (PNVME_IDENTIFY_NAMESPACE_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if ((identifyNamespaceData->NSZE == 0) ||
            (identifyNamespaceData->NCAP == 0)) {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Identify Namespace Data not valid.\n"));
            goto exit;
        } else {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Identify Namespace Data succeeded***.\n"));
        }
    }


    //
    // Initialize query data structure to get Error Information Log.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_ERROR_INFO;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_ERROR_INFO_LOG);

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Error Information Log failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Error Information Log - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_ERROR_INFO_LOG))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Error Information Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Error Information Log Data 
    //
    {
        PNVME_ERROR_INFO_LOG erroInfo = (PNVME_ERROR_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Error Information Log Data - ErrorCount %I64d.\n"), erroInfo->ErrorCount);
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Error Information Log Data - Status %d.\n"), erroInfo->Status.AsUshort);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Error Information Log succeeded***.\n"));
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
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_LOG))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // SMART/Health Information Log Data 
    //
    {
        PNVME_HEALTH_INFO_LOG smartInfo = (PNVME_HEALTH_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log Data - Temperature %d.\n"), ((ULONG)smartInfo->Temperature[1] << 8 | smartInfo->Temperature[0]) - 273);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***SMART/Health Information Log succeeded***.\n"));
    }


    //
    // Initialize query data structure to get Arbitration feature.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeFeature;
    protocolData->ProtocolDataRequestValue = NVME_FEATURE_ARBITRATION;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = 0;
    protocolData->ProtocolDataLength = 0;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Feature - Arbitration failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Feature - Arbitration - data descriptor header not valid.\n"));
        return;
    }

    //
    // Arbitration Data 
    //
    {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Arbitration Data - %x.\n"), protocolDataDescr->ProtocolSpecificData.FixedProtocolReturnData);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Get Feature - Arbitration succeeded***.\n"));
    }


    //
    // Initialize query data structure to Volatile Cache feature.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeFeature;
    protocolData->ProtocolDataRequestValue = NVME_FEATURE_VOLATILE_WRITE_CACHE;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = 0;
    protocolData->ProtocolDataLength = 0;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Feature - Volatile Cache failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Feature - Volatile Cache  - data descriptor header not valid.\n"));
        return;
    }

    //
    // Arbitration Data 
    //
    {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Feature - Volatile Cache - %x.\n"), protocolDataDescr->ProtocolSpecificData.FixedProtocolReturnData);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Get Feature - Volatile Cache succeeded***.\n"));
    }



exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}

BOOL
DeviceValidateFirmwareUpgradeSupport(
    _In_ PSTORAGE_HW_FIRMWARE_INFO  FirmwareInfo,
    _In_ PCOMMAND_OPTIONS           CommandOptions
    )
/*++

Routine Description:

    Check the retrieved data to see if the device supports firmware upgrade.

Arguments:

    FirmwareInfo - 
    CommandOptions - 

Return Value:

    BOOL - TRUE or FALSE

--*/
{
    BOOL    result = TRUE;
    UCHAR   slotId = STORAGE_HW_FIRMWARE_INVALID_SLOT;
    int     i;

    UNREFERENCED_PARAMETER(CommandOptions);

    _tprintf(_T("Validate firmware upgrade support information.\n"));

    if (FirmwareInfo->SupportUpgrade == FALSE) {
        _tprintf(_T("IOCTL_STORAGE_FIRMWARE_GET_INFO - The device does not support firmware upgrade.\n"));
        result = FALSE;
    } else if (FirmwareInfo->SlotCount == 0) {
        _tprintf(_T("IOCTL_STORAGE_FIRMWARE_GET_INFO - reported slot count is 0.\n"));
        result = FALSE;
    } else if (FirmwareInfo->ImagePayloadAlignment == 0) {
        _tprintf(_T("IOCTL_STORAGE_FIRMWARE_GET_INFO - reported ImagePayloadAlignment is 0.\n"));
        result = FALSE;
    } else if (FirmwareInfo->ImagePayloadMaxSize == 0) {
        _tprintf(_T("IOCTL_STORAGE_FIRMWARE_GET_INFO - reported ImagePayloadMaxSize is 0.\n"));
        result = FALSE;
    }

    //
    // Check if there is writable slot available.
    //
    if (result) {
        for (i = 0; i < FirmwareInfo->SlotCount; i++) {
            if (FirmwareInfo->Slot[i].ReadOnly == FALSE) {
                slotId = FirmwareInfo->Slot[i].SlotNumber;
                break;
            }
        }

        if (slotId == STORAGE_HW_FIRMWARE_INVALID_SLOT) {
            _tprintf(_T("ERROR: IOCTL_STORAGE_FIRMWARE_GET_INFO - no writable firmware slot reported.\n"));
            result = FALSE;
        }
    }

    return result;
}

VOID
DeviceMiniportFirmwareUpgrade(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions
    )
/*++

Routine Description:

    Send IOCTL_SCSI_MINIPORT_FIRMWARE to miniport driver upgrading device firmware.
    Since Win10, a more general IOCTL is defined and this one will be mainly used by port / miniport drivers.

Arguments:

    DeviceList - 
    Index - 
    CommandOptions - 

Return Value:

    None

--*/
{
    BOOL                    result = FALSE;

    PUCHAR                  infoBuffer = NULL;
    ULONG                   infoBufferLength = 0;

    PUCHAR                  buffer = NULL;
    ULONG                   bufferSize = 0;
    ULONG                   firmwareStructureOffset = 0;
    ULONG                   imageBufferLength = 0;

    PSRB_IO_CONTROL             srbControl = NULL;
    PFIRMWARE_REQUEST_BLOCK     firmwareRequest = NULL;

    PSTORAGE_FIRMWARE_INFO      firmwareInfo = NULL;
    PSTORAGE_FIRMWARE_DOWNLOAD  firmwareDownload = NULL;
    PSTORAGE_FIRMWARE_ACTIVATE  firmwareActivate = NULL;

    ULONG                   slotID = 0;
    ULONG                   returnedLength = 0;
    ULONG                   i = 0;

    HANDLE                  fileHandle = NULL;
    ULONG                   imageOffset = 0;
    ULONG                   readLength = 0;
    BOOLEAN                 moreToDownload = FALSE;

    //
    // Get firmware information.
    //
    result = DeviceGetMiniportFirmwareInfo(DeviceList, Index, &infoBuffer, &infoBufferLength, FALSE);

    if (result == FALSE) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade: Get Firmware Information Failed. Error code: %d\n"), GetLastError());
        goto Exit;
    }

    srbControl = (PSRB_IO_CONTROL)infoBuffer;
    firmwareRequest = (PFIRMWARE_REQUEST_BLOCK)(srbControl + 1);
    firmwareInfo = (PSTORAGE_FIRMWARE_INFO)((PUCHAR)srbControl + firmwareRequest->DataBufferOffset);

    if (srbControl->ReturnCode != FIRMWARE_STATUS_SUCCESS) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - get firmware info failed. srbControl->ReturnCode %d.\n"), srbControl->ReturnCode);
        goto Exit;
    }

    if (!firmwareInfo->UpgradeSupport) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - The device does not support firmware upgrade.\n"));
        goto Exit;
    }

    //
    // Find the corresponding slot and validate input parameter - FirmwareSlot.
    //
    slotID = (ULONG)-1;

    for (i = 0; i < firmwareInfo->SlotCount; i++) {
        if (CommandOptions->Parameters.u.Firmware.SlotId == firmwareInfo->Slot[i].SlotNumber) {
            slotID = CommandOptions->Parameters.u.Firmware.SlotId;
            break;
        }
    }

    if (slotID == (ULONG)-1) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - The specified firmware slot ID is not valid.\n"));
        goto Exit;
    }

    if ((firmwareInfo->Slot[i].ReadOnly == TRUE) && (CommandOptions->Parameters.FileName != NULL)) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - The specified firmware slot is a read-only slot.\n"));
        goto Exit;
    }

    if ((CommandOptions->Parameters.FileName == NULL) && (firmwareInfo->Slot[i].Revision.AsUlonglong == 0)) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - The specified firmware slot doesn't contain firmware.\n"));
        goto Exit;
    }

    //
    // The Max Transfer Length limits the part of buffer that may need to transfer to controller, not the whole buffer.
    //
    firmwareStructureOffset = ((sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK) - 1) / sizeof(PVOID) + 1) * sizeof(PVOID);

    bufferSize = firmwareStructureOffset;
    bufferSize += FIELD_OFFSET(STORAGE_FIRMWARE_DOWNLOAD, ImageBuffer);
    bufferSize += min(DeviceList[Index].AdapterDescriptor.MaximumTransferLength, 2 * 1024 * 1024);

    //
    // Allocate buffer to transfer firmware image.
    //
    buffer = (PUCHAR)malloc(bufferSize);

    if (buffer == NULL) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - Allocate buffer failed. Error code: %d\n"), GetLastError());
        goto Exit;
    }

    srbControl = (PSRB_IO_CONTROL)buffer;
    firmwareRequest = (PFIRMWARE_REQUEST_BLOCK)(srbControl + 1);

    imageBufferLength = bufferSize - firmwareStructureOffset - sizeof(STORAGE_FIRMWARE_DOWNLOAD);

    RtlZeroMemory(buffer, bufferSize);

    //
    // Open image file and download it to controller.
    //
    if (CommandOptions->Parameters.FileName != NULL) {

        imageBufferLength = (imageBufferLength / sizeof(PVOID)) * sizeof(PVOID);
        imageOffset = 0;
        readLength = 0;
        moreToDownload = TRUE;

        fileHandle = CreateFile(CommandOptions->Parameters.FileName,              // file to open
                                GENERIC_READ,          // open for reading
                                FILE_SHARE_READ,       // share for reading
                                NULL,                  // default security
                                OPEN_EXISTING,         // existing file only
                                FILE_ATTRIBUTE_NORMAL, // normal file
                                NULL);                 // no attr. template

        if (fileHandle == INVALID_HANDLE_VALUE) {
            _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - unable to open firmware image file \"%s\" for read.\n"), CommandOptions->Parameters.FileName);
            goto Exit;
        }

        while (moreToDownload) {

            RtlZeroMemory(buffer, bufferSize);

            srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
            srbControl->ControlCode = IOCTL_SCSI_MINIPORT_FIRMWARE;
            RtlMoveMemory(srbControl->Signature, IOCTL_MINIPORT_SIGNATURE_FIRMWARE, 8);
            srbControl->Timeout = 30;
            srbControl->Length = bufferSize - sizeof(SRB_IO_CONTROL);

            firmwareRequest->Version = FIRMWARE_REQUEST_BLOCK_STRUCTURE_VERSION;
            firmwareRequest->Size = sizeof(FIRMWARE_REQUEST_BLOCK);
            firmwareRequest->Function = FIRMWARE_FUNCTION_DOWNLOAD;
            firmwareRequest->Flags = FIRMWARE_REQUEST_FLAG_CONTROLLER;
            firmwareRequest->DataBufferOffset = firmwareStructureOffset;
            firmwareRequest->DataBufferLength = bufferSize - firmwareStructureOffset;

            firmwareDownload = (PSTORAGE_FIRMWARE_DOWNLOAD)((PUCHAR)srbControl + firmwareRequest->DataBufferOffset);

            if (ReadFile(fileHandle, firmwareDownload->ImageBuffer, imageBufferLength, &readLength, NULL) == FALSE) {
                _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - Read firmware file failed.\n"));
                goto Exit;
            }

            if (readLength == 0) {
                moreToDownload = FALSE;
                break;
            }

            if ((readLength % sizeof(ULONG)) != 0) {
                _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - Read firmware file failed.\n"));
            }

            firmwareDownload->Version = STORAGE_FIRMWARE_DOWNLOAD_STRUCTURE_VERSION;
            firmwareDownload->Size = sizeof(STORAGE_FIRMWARE_DOWNLOAD);
            firmwareDownload->Offset = imageOffset;
            firmwareDownload->BufferSize = readLength;

            //
            // download this piece of firmware to device
            //
            result = DeviceIoControl(DeviceList[Index].Handle,
                                     IOCTL_SCSI_MINIPORT,
                                     buffer,
                                     bufferSize,
                                     buffer,
                                     bufferSize,
                                     &returnedLength,
                                     NULL
                                     );

            if (result == FALSE) {
                _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware download IOCTL failed. Error code: %d\n"), GetLastError());
                goto Exit;
            }

            if (srbControl->ReturnCode != FIRMWARE_STATUS_SUCCESS) {
                _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware download failed. Driver error - srbControl->ReturnCode %d.\n"), srbControl->ReturnCode);
                goto Exit;
            }

            //
            // Update Image Offset for next loop.
            //
            imageOffset += readLength;
        }
    }

    //
    // Activate the newly downloaded image.
    //
    RtlZeroMemory(buffer, bufferSize);

    srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
    srbControl->ControlCode = IOCTL_SCSI_MINIPORT_FIRMWARE;
    RtlMoveMemory(srbControl->Signature, IOCTL_MINIPORT_SIGNATURE_FIRMWARE, 8);
    srbControl->Timeout = 30;
    srbControl->Length = bufferSize - sizeof(SRB_IO_CONTROL);

    firmwareRequest->Version = FIRMWARE_REQUEST_BLOCK_STRUCTURE_VERSION;
    firmwareRequest->Size = sizeof(FIRMWARE_REQUEST_BLOCK);
    firmwareRequest->Function = FIRMWARE_FUNCTION_ACTIVATE;
    firmwareRequest->Flags = FIRMWARE_REQUEST_FLAG_CONTROLLER;
    firmwareRequest->DataBufferOffset = firmwareStructureOffset;
    firmwareRequest->DataBufferLength = bufferSize - firmwareStructureOffset;

    firmwareActivate = (PSTORAGE_FIRMWARE_ACTIVATE)((PUCHAR)srbControl + firmwareRequest->DataBufferOffset);
    firmwareActivate->Version = 1;
    firmwareActivate->Size = sizeof(STORAGE_FIRMWARE_ACTIVATE);
    firmwareActivate->SlotToActivate = (UCHAR)slotID;

    //
    // If firmware image is not specified, the intension is to activate the existing firmware in slot.
    //
    if (CommandOptions->Parameters.FileName == NULL) {
        firmwareRequest->Flags = FIRMWARE_REQUEST_FLAG_SWITCH_TO_EXISTING_FIRMWARE;
    }

    //
    // activate firmware
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                            IOCTL_SCSI_MINIPORT,
                            buffer,
                            bufferSize,
                            buffer,
                            bufferSize,
                            &returnedLength,
                            NULL
                            );


    if (result == FALSE) {
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate IOCTL failed. Error code: %d\n"), GetLastError());
        goto Exit;
    }

    switch (srbControl->ReturnCode) {
    case FIRMWARE_STATUS_SUCCESS:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate succeeded.\n"));
        break;

    case FIRMWARE_STATUS_POWER_CYCLE_REQUIRED:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate succeeded. PLEASE REBOOT COMPUTER.\n"));
        break;

    case FIRMWARE_STATUS_ILLEGAL_REQUEST:
    case FIRMWARE_STATUS_INVALID_PARAMETER:
    case FIRMWARE_STATUS_INPUT_BUFFER_TOO_BIG:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate parameter error. srbControl->ReturnCode %d.\n"), srbControl->ReturnCode);
        break;

    case FIRMWARE_STATUS_INVALID_SLOT:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate, slot ID invalid.\n"));
        break;

    case FIRMWARE_STATUS_INVALID_IMAGE:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate, invalid firmware image.\n"));
        break;

    case FIRMWARE_STATUS_ERROR:
    case FIRMWARE_STATUS_CONTROLLER_ERROR:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate, error returned.\n"));
        break;

    default:
        _tprintf(_T("\t DeviceMiniportFirmwareUpgrade - firmware activate, unexpected error. srbControl->ReturnCode %d.\n"), srbControl->ReturnCode);
        break;
   }

Exit:

    if (fileHandle != NULL) {
        CloseHandle(fileHandle);
    }

    if (infoBuffer != NULL) {
        free(infoBuffer);
    }

    if (buffer != NULL) {
        free(buffer);
    }

    return;
}

VOID
DeviceStorageFirmwareUpgrade(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions,
    _In_ BOOLEAN            VerboseDisplay
    )
/*++

Routine Description:

    Send IOCTL_STORAGE_FIRMWARE_DOWNLOAD/ACTIVATE to upgrade device firmware.
    These IOCTLs are defined in Win10.

Arguments:

    DeviceList - 
    Index - 
    CommandOptions - 
    VerboseDisplay - 

Return Value:

    None

--*/
{
    BOOL                    result = TRUE;

    PUCHAR                  infoBuffer = NULL;
    ULONG                   infoBufferLength = 0;
    PUCHAR                  buffer = NULL;
    ULONG                   bufferSize;
    ULONG                   imageBufferLength;

    PSTORAGE_HW_FIRMWARE_INFO       firmwareInfo = NULL;
    PSTORAGE_HW_FIRMWARE_DOWNLOAD   firmwareDownload = NULL;
    PSTORAGE_HW_FIRMWARE_ACTIVATE   firmwareActivate = NULL;

    UCHAR                   slotID;
    ULONG                   returnedLength;
    ULONG                   i;

    HANDLE                  fileHandle = NULL;
    ULONG                   imageOffset;
    ULONG                   readLength;
    BOOLEAN                 moreToDownload;

    ULONGLONG               tickCount1 = 0;
    ULONGLONG               tickCount2 = 0;

    //
    // Get firmware information.
    //
    result = DeviceGetStorageFirmwareInfo(DeviceList, Index, CommandOptions, &infoBuffer, &infoBufferLength, VerboseDisplay);

    if (result == FALSE) {
        goto Exit;
    }

    firmwareInfo = (PSTORAGE_HW_FIRMWARE_INFO)infoBuffer;

    result = DeviceValidateFirmwareUpgradeSupport(firmwareInfo, CommandOptions);

    if (result == FALSE) {
        goto Exit;
    }

    _tprintf(_T("Download firmware to device using IOCTL_STORAGE_FIRMWARE_DOWNLOAD.\n"));

    //
    // Find the first writable slot.
    //
    slotID = STORAGE_HW_FIRMWARE_INVALID_SLOT;

    for (i = 0; i < firmwareInfo->SlotCount; i++) {
        if (CommandOptions->Parameters.u.Firmware.SlotId == STORAGE_HW_FIRMWARE_INVALID_SLOT) {
            if (firmwareInfo->Slot[i].ReadOnly == FALSE) {
                slotID = firmwareInfo->Slot[i].SlotNumber;
                break;
            }
        } else {
            if (CommandOptions->Parameters.u.Firmware.SlotId == firmwareInfo->Slot[i].SlotNumber) {
                slotID = CommandOptions->Parameters.u.Firmware.SlotId;
                break;
            }
        }
    }

    if ((slotID == STORAGE_HW_FIRMWARE_INVALID_SLOT) ||
        ((firmwareInfo->Slot[i].ReadOnly == TRUE) && (CommandOptions->Parameters.FileName != NULL))) {

        _tprintf(_T("Not able to find a writable firmware slot.\n"));
        goto Exit;
    }

    //
    // The Max Transfer Length limits the part of buffer that may need to transfer to controller, not the whole buffer.
    //
    bufferSize = FIELD_OFFSET(STORAGE_HW_FIRMWARE_DOWNLOAD, ImageBuffer);
    bufferSize += min(DeviceList[Index].AdapterDescriptor.MaximumTransferLength, firmwareInfo->ImagePayloadMaxSize);

    buffer = (PUCHAR)malloc(bufferSize);

    if (buffer == NULL) {
        _tprintf(_T("Allocate buffer for sending firmware image file failed. Error code: %d\n"), GetLastError());
        goto Exit;
    }

    //
    // Setup header of firmware download data structure.
    //
    RtlZeroMemory(buffer, bufferSize);

    firmwareDownload = (PSTORAGE_HW_FIRMWARE_DOWNLOAD)buffer;

    firmwareDownload->Version = sizeof(STORAGE_HW_FIRMWARE_DOWNLOAD);
    firmwareDownload->Size = bufferSize;

    if (firmwareInfo->FirmwareShared) {
        firmwareDownload->Flags = STORAGE_HW_FIRMWARE_REQUEST_FLAG_CONTROLLER;
    }
    else {
        firmwareDownload->Flags = 0;
    }

    firmwareDownload->Slot = slotID;

    //
    // Open image file and download it to controller.
    //
    imageBufferLength = bufferSize - FIELD_OFFSET(STORAGE_HW_FIRMWARE_DOWNLOAD, ImageBuffer);

    if (CommandOptions->Parameters.FileName != NULL) {

        imageOffset = 0;
        readLength = 0;
        moreToDownload = TRUE;

        fileHandle = CreateFile(CommandOptions->Parameters.FileName,              // file to open
                                GENERIC_READ,          // open for reading
                                FILE_SHARE_READ,       // share for reading
                                NULL,                  // default security
                                OPEN_EXISTING,         // existing file only
                                FILE_ATTRIBUTE_NORMAL, // normal file
                                NULL);                 // no attr. template

        if (fileHandle == INVALID_HANDLE_VALUE) {
            _tprintf(_T("Unable to open handle for firmware image file %s.\n"), CommandOptions->Parameters.FileName);
            goto Exit;
        }

        while (moreToDownload) {

            RtlZeroMemory(firmwareDownload->ImageBuffer, imageBufferLength);

            if (ReadFile(fileHandle, firmwareDownload->ImageBuffer, imageBufferLength, &readLength, NULL) == FALSE) {
                _tprintf(_T("Read firmware file failed. Error code: %d\n"), GetLastError());
                goto Exit;
            }

            if (readLength == 0) {
                if (imageOffset == 0) {
                    _tprintf(_T("Firmware image file read return length value 0. Error code: %d\n"), GetLastError());
                    goto Exit;
                }

                moreToDownload = FALSE;
                break;
            }

            firmwareDownload->Offset = imageOffset;

            if (readLength > 0) {
                firmwareDownload->BufferSize = ((readLength - 1) / firmwareInfo->ImagePayloadAlignment + 1) * firmwareInfo->ImagePayloadAlignment;
            } else {
                firmwareDownload->BufferSize = 0;
            }

            //
            // download this piece of firmware to device
            //
            result = DeviceIoControl(DeviceList[Index].Handle,
                                     IOCTL_STORAGE_FIRMWARE_DOWNLOAD,
                                     buffer,
                                     bufferSize,
                                     buffer,
                                     bufferSize,
                                     &returnedLength,
                                     NULL
                                     );

            if (result == FALSE) {
                _tprintf(_T("\t DeviceStorageFirmwareUpgrade - firmware download IOCTL failed. Error code: %d\n"), GetLastError());
                goto Exit;
            }

            //
            // Update Image Offset for next loop.
            //
            imageOffset += (ULONG)firmwareDownload->BufferSize;
        }
    }

    //
    // Activate the newly downloaded image.
    //
    _tprintf(_T("Activate the new firmware image using IOCTL_STORAGE_FIRMWARE_ACTIVATE.\n"));

    RtlZeroMemory(buffer, bufferSize);

    firmwareActivate = (PSTORAGE_HW_FIRMWARE_ACTIVATE)buffer;
    firmwareActivate->Version = sizeof(STORAGE_HW_FIRMWARE_ACTIVATE);
    firmwareActivate->Size = sizeof(STORAGE_HW_FIRMWARE_ACTIVATE);
    firmwareActivate->Slot = slotID;

    if (firmwareInfo->FirmwareShared) {
        firmwareActivate->Flags = STORAGE_HW_FIRMWARE_REQUEST_FLAG_CONTROLLER;
    }
    else {
        firmwareActivate->Flags = 0;
    }

    //
    // If firmware image is not specified, the intension is to activate the existing firmware in slot.
    //
    if (CommandOptions->Parameters.FileName == NULL) {
        firmwareActivate->Flags |= STORAGE_HW_FIRMWARE_REQUEST_FLAG_SWITCH_TO_EXISTING_FIRMWARE;
    }

    tickCount1 = GetTickCount64();

    //
    // activate firmware
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_FIRMWARE_ACTIVATE,
                             buffer,
                             bufferSize,
                             buffer,
                             bufferSize,
                             &returnedLength,
                             NULL
                             );

    tickCount2 = GetTickCount64();

    {
        ULONG   seconds = (ULONG)((tickCount2 - tickCount1) / 1000);
        ULONG   milliseconds = (ULONG)((tickCount2 - tickCount1) % 1000);

        if (seconds < 5) {
            _tprintf(_T("\n\tFirmware activation process took %d.%d seconds.\n"), seconds, milliseconds);
        } else {
            _tprintf(_T("\n\tFirmware activation process took %d.%d seconds.\n"), seconds, milliseconds);
        }

        if (result) {
            _tprintf(_T("\tNew firmware has been successfully applied to device.\n"));

        } else if (GetLastError() == STG_S_POWER_CYCLE_REQUIRED) {
            _tprintf(_T("\tWarning: Upgrade completed. Power cycle is required to activate the new firmware.\n"));

            goto Exit;
        } else {
            _tprintf(_T("\tFirmware activate IOCTL failed. Error code: %d\n"), GetLastError());

            goto Exit;
        }
    }

    //
    // Validate the new firmware revision being different from old one.
    //
    if (result) {
        UCHAR   firmwareRevision[17] = {0};

        RtlCopyMemory(firmwareRevision, DeviceList[Index].FirmwareRevision, 17);

        //
        // Get new firmware revision.
        //
        if (infoBuffer != NULL) {
            free(infoBuffer);
            infoBuffer = NULL;
        }

        //
        // Wait 2 seconds before retrieving disk information so that system has time to get it ready.
        //
        Sleep(2000);
        
        result = DeviceGetStorageFirmwareInfo(DeviceList, Index, CommandOptions, &infoBuffer, &infoBufferLength, VerboseDisplay);

        if (result) {
            TCHAR revision[32] = {0};

            MultiByteToWideChar(CP_ACP,
                                0,
                                (LPCCH)firmwareInfo->Slot[i].Revision,
                                -1,
                                revision,
                                sizeof(revision) / sizeof(revision[0]) - 1
                                );

            _tprintf(_T("\n\tNew Firmware Revision - %s\n"), revision);

            if (RtlCompareMemory(firmwareRevision, DeviceList[Index].FirmwareRevision, 17) == 17) {
                _tprintf(_T("\tWarning: New Firmware Revision is same as old one - %s\n"), revision);
                goto Exit;
            }
        } else {
            _tprintf(_T("\n\tFailed to retrieve new Firmware Revision. Error code: %d\n"), GetLastError());
            goto Exit;
        }
    }

Exit:

    if (fileHandle != NULL) {
        CloseHandle(fileHandle);
    }

    if (infoBuffer != NULL) {
        free(infoBuffer);
    }

    if (buffer != NULL) {
        free(buffer);
    }

    return;
}

VOID
DeviceFirmwareTest(
    _In_ PDEVICE_LIST       DeviceList,
    _In_ DWORD              Index,
    _In_ PCOMMAND_OPTIONS   CommandOptions
    )
/*++

Routine Description:

    NOTE: This test applies to win10 and up.

    The test includes following portions:
    1. Validate device support through protocol command channel.
        a. Issue ATA pass-through IOCTL to ATA devices.
        b. Issue protocol info query IOCTL to NVMe devices.
        c. (if needed) Issue SCSI pass-through IOCTL to SCSI/SAS devices.
    2. Test Get Firmware Information using IOCTL_STORAGE_FIRMWARE_GET_INFO.
    3. Test Firmware Upgrade using IOCTL_STORAGE_FIRMWARE_DOWNLOAD and IOCTL_STORAGE_FIRMWARE_ACTIVATE.
    4. If firmware upgrade completed. Verify:
        a. Firmware revision has been changed.
        b. Device identification information not changed.

Arguments:

    DeviceList - 
    Index - 
    CommandOptions - 

Return Value:

    None

--*/
{
    BOOL                        result = FALSE;
    PSTORAGE_DEVICE_DESCRIPTOR  deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;

    //
    // Get protocol specific information to validate the device support status.
    //
    if (IsAtaDevice(deviceDesc->BusType)) {
        result = DeviceFirmwareTestAta(DeviceList, Index);
    } else if (IsNVMeDevice(deviceDesc->BusType)) {
        result = DeviceFirmwareTestNvme(DeviceList, Index);
    } else {
        //
        // Assume device supports SCSI protocol.
        //
        result = DeviceFirmwareTestScsi(DeviceList, Index);
    }

    //
    // Validate the support of firmware upgrade process.
    //
    if (result) {
        DeviceStorageFirmwareUpgrade(DeviceList, Index, CommandOptions, TRUE);
    }

    //
    // The device must retain the following data through download and activate of a firmware image: Vendor ID, Model ID, Serial Number.
    //

    return;
}


VOID
DeviceQueryProtocolDataTest(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
)
{
    PSTORAGE_DEVICE_DESCRIPTOR deviceDesc = (PSTORAGE_DEVICE_DESCRIPTOR)DeviceList[Index].DeviceDescriptor;

    if (IsAtaDevice(deviceDesc->BusType)) {
        DeviceAtaQueryProtocolDataTest(DeviceList, Index);
    } else if (IsNVMeDevice(deviceDesc->BusType)) {
        DeviceNVMeQueryProtocolDataTest(DeviceList, Index);
    } else {
        _tprintf(_T(" NOT Supported. \n"));
    }

    return;
}

VOID
DeviceQueryTemperatureTest (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    )
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;

    STORAGE_PROPERTY_QUERY query;
    PSTORAGE_TEMPERATURE_DATA_DESCRIPTOR temperatureDataDescr = NULL;

    //
    // Allocate buffer for use.
    //
    bufferLength = sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR) + 8 * sizeof(STORAGE_TEMPERATURE_INFO);
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DeviceQueryTemperatureTest: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(&query, sizeof(STORAGE_PROPERTY_QUERY));
    ZeroMemory(buffer, bufferLength);
    temperatureDataDescr = (PSTORAGE_TEMPERATURE_DATA_DESCRIPTOR)buffer;

    query.PropertyId = StorageDeviceTemperatureProperty;
    query.QueryType = PropertyStandardQuery;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             &query,
                             sizeof(STORAGE_PROPERTY_QUERY),
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceQueryTemperatureTest: failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((temperatureDataDescr->Version != sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR)) ||
        (temperatureDataDescr->Size != sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceQueryTemperatureTest: data descriptor header not valid.\n"));
        return;
    }

    _tprintf(_T("\t\tTemperature Info Count: %d.\n"), temperatureDataDescr->InfoCount);
    _tprintf(_T("\t\tCritical Temperature: %d C.\n"), temperatureDataDescr->CriticalTemperature);
    _tprintf(_T("\t\tWarning Temperature: %d C.\n"), temperatureDataDescr->WarningTemperature);

    if (temperatureDataDescr->InfoCount > 0) {
        USHORT i = 0;

        for (i = 0; i < temperatureDataDescr->InfoCount; i++) {
            _tprintf(_T("\t\t\tSensor Index: %d.\n"), temperatureDataDescr->TemperatureInfo[i].Index);
            _tprintf(_T("\t\t\tCurrent Temperature: %d C.\n"), temperatureDataDescr->TemperatureInfo[i].Temperature);
            _tprintf(_T("\t\t\tOver Threshold: %d C.\n"), temperatureDataDescr->TemperatureInfo[i].OverThreshold);
            _tprintf(_T("\t\t\tUnder Threshold: %d C.\n"), temperatureDataDescr->TemperatureInfo[i].UnderThreshold);
            _tprintf(_T("\t\tOver Threshold Can be Changed: %s.\n"), temperatureDataDescr->TemperatureInfo[i].OverThresholdChangable ? _T("TRUE") : _T("FALSE"));
            _tprintf(_T("\t\tUnder Threshold Can be Changed: %s.\n"), temperatureDataDescr->TemperatureInfo[i].UnderThresholdChangable ?  _T("TRUE") :  _T("FALSE"));
            _tprintf(_T("\t\tEvent generated when temperature crosses Threshold: %s.\n"), temperatureDataDescr->TemperatureInfo[i].EventGenerated ?  _T("TRUE") :  _T("FALSE"));
            _tprintf(_T("\n"));
        }

    }

    _tprintf(_T("DeviceQueryTemperatureTest: ***succeeded***.\n"));


exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}

VOID
DeviceChangeTemperatureThresholdTest (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex,
    _In_ USHORT       SensorIndex,
    _In_ BOOLEAN      UpdateOverThreshold,
    _In_ SHORT        Threshold
    )
{
    BOOL    result;
    ULONG   returnedLength = 0;

    STORAGE_TEMPERATURE_THRESHOLD setThreshold = {0};

    setThreshold.Version = sizeof(STORAGE_TEMPERATURE_THRESHOLD);
    setThreshold.Size = sizeof(STORAGE_TEMPERATURE_THRESHOLD);
    setThreshold.Flags = STORAGE_TEMPERATURE_THRESHOLD_FLAG_ADAPTER_REQUEST;
    setThreshold.Index = SensorIndex;
    setThreshold.Threshold = Threshold;
    setThreshold.OverThreshold = UpdateOverThreshold;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_SET_TEMPERATURE_THRESHOLD,
                             &setThreshold,
                             sizeof(STORAGE_TEMPERATURE_THRESHOLD),
                             NULL,
                             0,
                             &returnedLength,
                             NULL
                             );

    if (!result) {
        _tprintf(_T("DeviceChangeTemperatureThresholdTest: failed. Error Code %d.\n"), GetLastError());
    } else {
        _tprintf(_T("DeviceChangeTemperatureThresholdTest: ***succeeded***.\n"));
    }

    return;
}

VOID
DevicePassThroughTest (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    )
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;

    PSTORAGE_PROTOCOL_COMMAND protocolCommand = NULL;
    PNVME_COMMAND command = NULL;

    //
    // Allocate buffer for getting identify controller.
    //
    bufferLength = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) +
                   STORAGE_PROTOCOL_COMMAND_LENGTH_NVME +
                   4096 +
                   sizeof(NVME_ERROR_INFO_LOG);

    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DevicePassThroughTest: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);
    protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

    protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
    protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
    protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    command->u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_CONTROLLER;
    command->NSID = 0;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_PROTOCOL_COMMAND,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DevicePassThroughTest: Identify Controller test succeeded as it's blocked. Error Code %d.\n"), GetLastError());
    }

    //
    // Initialize query data structure to SMART Log.
    //
    ZeroMemory(buffer, bufferLength);
    protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

    protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
    protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
    protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = sizeof(NVME_HEALTH_INFO_LOG);
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = NVME_ADMIN_COMMAND_GET_LOG_PAGE;
    command->NSID = ULONG_MAX;
    command->u.GETLOGPAGE.CDW10.LID = NVME_LOG_PAGE_HEALTH_INFO;
    command->u.GETLOGPAGE.CDW10.NUMD = sizeof(NVME_HEALTH_INFO_LOG) / sizeof(ULONG) - 1;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_PROTOCOL_COMMAND,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DevicePassThroughTest: Get Log Page test succeeded as it's blocked. Error Code %d.\n"), GetLastError());
    }


    //
    // The command data transfer flag should match the buffers.
    // for command code, bit 00:01 indicates the data transfer direction:
    //      00b - no data transfer
    //      01h - data to device
    //      10h - data from device
    //

    //
    // Send Intel Vendor Specific command 0xE2.
    //
    ZeroMemory(buffer, bufferLength);
    protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

    protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
    protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
    protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = 0xE2;
    command->u.GENERAL.CDW10 = 0x00000100;
    command->u.GENERAL.CDW12 = 0xFFFFFFF0;
    command->u.GENERAL.CDW13 = 0x00000001;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_PROTOCOL_COMMAND,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result) {
        _tprintf(_T("DevicePassThroughTest: Command 0xE2 IOCTL failed. Error Code %d.\n"), GetLastError());
    } else {
        if (protocolCommand->ReturnStatus == STORAGE_PROTOCOL_STATUS_SUCCESS) {
            _tprintf(_T("DevicePassThroughTest: Command 0xE2 IOCTL succeeded.\n"));
        } else {
            _tprintf(_T("DevicePassThroughTest: Command 0xE2 failed. Returned status: 0x%x \n"), protocolCommand->ReturnStatus);
            _tprintf(_T("DevicePassThroughTest: Command 0xE2 failed. Error Code: 0x%x \n"), protocolCommand->ErrorCode);
        }
    }


    //
    // Send Intel Vendor Specific command 0xD2.
    //
    ZeroMemory(buffer, bufferLength);
    protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

    protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
    protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
    protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = 0xD2;
    command->NSID = 0x00000001;
    command->u.GENERAL.CDW10 = 0x00000400;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_PROTOCOL_COMMAND,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result) {
        _tprintf(_T("DevicePassThroughTest: Command 0xD2 IOCTL failed. Error Code %d.\n"), GetLastError());
    } else {
        if (protocolCommand->ReturnStatus == STORAGE_PROTOCOL_STATUS_SUCCESS) {
            _tprintf(_T("DevicePassThroughTest: Command 0xD2 IOCTL succeeded.\n"));
        } else {
            _tprintf(_T("DevicePassThroughTest: Command 0xD2 failed. Returned status: 0x%x \n"), protocolCommand->ReturnStatus);
            _tprintf(_T("DevicePassThroughTest: Command 0xD2 failed. Error Code: 0x%x \n"), protocolCommand->ErrorCode);
        }
    }

    //
    // Send 2nd Intel Vendor Specific command 0xD2 and expect a sequence error.
    //
    ZeroMemory(buffer, bufferLength);
    protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

    protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
    protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
    protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = 0xD2;
    command->NSID = 0x00000001;
    command->u.GENERAL.CDW10 = 0x00000400;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                             IOCTL_STORAGE_PROTOCOL_COMMAND,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result) {
        _tprintf(_T("DevicePassThroughTest: 2nd Command 0xD2 IOCTL failed. Error Code %d.\n"), GetLastError());
    } else {
        if (protocolCommand->ReturnStatus == STORAGE_PROTOCOL_STATUS_SUCCESS) {
            _tprintf(_T("DevicePassThroughTest: 2nd Command 0xD2 IOCTL succeeded. This is an ERROR as we expect this command fails.\n"));
        } else {
            NVME_COMMAND_STATUS commandStatus = {0};

            commandStatus.AsUshort = (USHORT)protocolCommand->ErrorCode;

            if ((commandStatus.SCT == NVME_STATUS_TYPE_GENERIC_COMMAND) && (commandStatus.SC == NVME_STATUS_COMMAND_SEQUENCE_ERROR)) {
                _tprintf(_T("DevicePassThroughTest: 2nd Command 0xD2 test succeeded. Device returned command sequence error.\n"));
            } else {
                _tprintf(_T("DevicePassThroughTest: 2nd Command 0xD2 test failed. Returned status: 0x%x \n"), protocolCommand->ReturnStatus);
                _tprintf(_T("DevicePassThroughTest: 2nd Command 0xD2 test failed. Error Code: 0x%x \n"), protocolCommand->ErrorCode);
            }
        }
    }

exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}

VOID
DeviceCacheStatus(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex
    )
{
    DeviceList;
    DeviceIndex;
/*
    BOOL    success = TRUE;
    ULONG   error = ERROR_SUCCESS;
    ULONG   bytesReturned = 0;
    DISK_CACHE_INFORMATION  cacheInfo = {0};
    DISK_CACHE_SETTING      cacheSetting = {0};

    //
    // Retrieve data for DiskWriteCacheEnabled
    //
    success = DeviceIoControl(DeviceList[DeviceIndex].Handle,
                              IOCTL_DISK_GET_CACHE_INFORMATION,
                              NULL,
                              0,
                              (LPVOID)&cacheInfo,
                              (DWORD)sizeof(cacheInfo),
                              &bytesReturned,
                              NULL
                              );

    if (!success) {
        error = GetLastError();
        DALTRACE_WARNING(
            L"DeviceIoControl for IOCTL_DISK_GET_CACHE_INFORMATION on DiskHandle: %p{HANDLE} " \
            L"failed with error code 0x%x",
            DiskHandle,
            error);
    } else {
        // Only set the value upon success
        Row->DiskWriteCacheEnabled = cacheInfo.WriteCacheEnabled;
    }


    if (!success) {
        _tprintf(_T("Get Cache Information - IOCTL_DISK_GET_CACHE_INFORMATION failed, error: %d\n\n"), GetLastError());
    } else {
        cacheInfo.WriteCacheEnabled;

        _tprintf(_T("  Device %s TRIM attribute.\n"), supportsTrim ? _T("supports") : _T("does not support"));
    }

    _tprintf(_T("\n"));
*/
    return;
}

VOID
DeviceIdentify(
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
)
/*++

Routine Description:

    Issue ATA Identify data (0xec) to the device

Arguments:

    Device - Handle to the device

Return Value:

    TRUE if the command could be sent.
    FALSE otherwise

--*/
{
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned;

    BuildIdentifyDeviceCommand(passThru);

    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
    );

    AtaDisplayDataBuffer(&ataPt, bytesReturned, TRUE);

    return;
}

VOID
DeviceAtaCommandPassThrough(
    _In_ PDEVICE_LIST     DeviceList,
    _In_ ULONG            DeviceIndex,
    _In_ PCOMMAND_OPTIONS CommandOptions
)
/*++

Routine Description:

    pass ATA command to the device

Arguments:

    DeviceList
    DeviceIndex
    CommandOptions

Return Value:

    TRUE if the command could be sent.
    FALSE otherwise

--*/
{
    ATA_PT               ataPt;

    PATA_PASS_THROUGH_EX passThru;
    ULONG                bytesReturned;
    PATA_COMMAND         ideReg;

    ZeroMemory(&ataPt, sizeof(ATA_PT));

    passThru = &ataPt.AtaPassThrough;

    // This is more like a version number. If the definition of the structure changes we can detect a mismatch
    passThru->Length = sizeof(ATA_PASS_THROUGH_EX);

    // This is required for ATA devices
    passThru->AtaFlags = ATA_FLAGS_DRDY_REQUIRED;

    // indicate the direction of data transfer
    passThru->AtaFlags |= ATA_FLAGS_DATA_IN;

    // Timeout of 10s. A request shouldn't take more than 10s to complete.
    // Setting a large timeout value is bad. It could hang the system.
    passThru->TimeOutValue = 30;

    // set the transfer size
    passThru->DataTransferLength = ATA_BLOCK_SIZE;

    // Data buffer starts right after the header
    // The filler aligns the buffer
    passThru->DataBufferOffset = FIELD_OFFSET(ATA_PT, Buffer);

    ideReg = (PATA_COMMAND)passThru->CurrentTaskFile;

    ideReg->Features = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Feature.Bits7_0;
    ideReg->SectorCount = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Count.Bits7_0;
    ideReg->LbaLow = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits7_0;
    ideReg->LbaMid = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits15_8;
    ideReg->LbaHigh = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits23_16;
    ideReg->bDriveHeadReg = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Device.AsByte;
    ideReg->Command = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Command;

    ideReg = (PATA_COMMAND)passThru->PreviousTaskFile;

    ideReg->Features = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Feature.Bits15_8;
    ideReg->SectorCount = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.Count.Bits15_8;
    ideReg->LbaLow = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits31_24;
    ideReg->LbaMid = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits39_32;
    ideReg->LbaHigh = CommandOptions->Parameters.u.AtaCmd.AtaAcsCommand.LBA.Bits47_40;


    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
    );
    if (bytesReturned > 0) {
        AtaDisplayDataBuffer(&ataPt, bytesReturned, FALSE);
    }

    return;
}



BOOLEAN
DeviceReadLogDirectory (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex,
    _In_ BOOLEAN      DisplayResult
)
{
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned = 0;

    BuildReadLogExCommand(passThru, IDE_GP_LOG_DIRECTORY_ADDRESS, 0, 1, 0);

    bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );

    if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
        PUSHORT logpages = (PUSHORT)ataPt.Buffer;

        DeviceList[DeviceIndex].LogPage.Version = logpages[0];
        DeviceList[DeviceIndex].LogPage.DeviceStatisticsPageCount = logpages[IDE_GP_LOG_DEVICE_STATISTICS_ADDRESS];
        DeviceList[DeviceIndex].LogPage.NcqCommandErrorPageCount = logpages[IDE_GP_LOG_NCQ_COMMAND_ERROR_ADDRESS];
        DeviceList[DeviceIndex].LogPage.PhyEventCounterPageCount = logpages[IDE_GP_LOG_PHY_EVENT_COUNTER_ADDRESS];
        DeviceList[DeviceIndex].LogPage.NcqNonDataPageCount = logpages[IDE_GP_LOG_NCQ_NON_DATA_ADDRESS];
        DeviceList[DeviceIndex].LogPage.NcqSendReceivePageCount = logpages[IDE_GP_LOG_NCQ_SEND_RECEIVE_ADDRESS];
        DeviceList[DeviceIndex].LogPage.HybridInfoPageCount = logpages[IDE_GP_LOG_HYBRID_INFO_ADDRESS];
        DeviceList[DeviceIndex].LogPage.CurrentDeviceInteralPageCount = logpages[IDE_GP_LOG_CURRENT_DEVICE_INTERNAL_STATUS];
        DeviceList[DeviceIndex].LogPage.SavedDeviceInternalPageCount = logpages[IDE_GP_LOG_SAVED_DEVICE_INTERNAL_STATUS];
        DeviceList[DeviceIndex].LogPage.IdentifyDeviceDataPageCount = logpages[IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS];

        if (DisplayResult) {
            _tprintf(_T("\t Log Page: Version: %d\n"), DeviceList[DeviceIndex].LogPage.Version);
            _tprintf(_T("\t Log Page: DeviceStatisticsPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.DeviceStatisticsPageCount);
            _tprintf(_T("\t Log Page: NcqCommandErrorPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.NcqCommandErrorPageCount);
            _tprintf(_T("\t Log Page: PhyEventCounterPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.PhyEventCounterPageCount);
            _tprintf(_T("\t Log Page: NcqNonDataPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.NcqNonDataPageCount);
            _tprintf(_T("\t Log Page: NcqSendReceivePageCount: %d\n"), DeviceList[DeviceIndex].LogPage.NcqSendReceivePageCount);
            _tprintf(_T("\t Log Page: HybridInfoPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.HybridInfoPageCount);
            _tprintf(_T("\t Log Page: CurrentDeviceInteralPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.CurrentDeviceInteralPageCount);
            _tprintf(_T("\t Log Page: SavedDeviceInternalPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.SavedDeviceInternalPageCount);
            _tprintf(_T("\t Log Page: IdentifyDeviceDataPageCount: %d\n"), DeviceList[DeviceIndex].LogPage.IdentifyDeviceDataPageCount);

            _tprintf(_T("\n"));
        }
    }

    return (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer));
}

BOOLEAN
DeviceReadLogNcqCommandError (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
)
{
    if (DeviceList[DeviceIndex].LogPage.NcqCommandErrorPageCount == 0) {
        _tprintf(_T("\t NCQ Command Error Log is NOT supported.\n"));
    } else {
        ATA_PT               ataPt = {0};
        PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
        ULONG                bytesReturned = 0;

        BuildReadLogExCommand(passThru, IDE_GP_LOG_NCQ_COMMAND_ERROR_ADDRESS, 0, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE
                                           );

        if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
            PGP_LOG_NCQ_COMMAND_ERROR logpage = (PGP_LOG_NCQ_COMMAND_ERROR)ataPt.Buffer;

            _tprintf(_T("\t NCQ Command Tag: %X\n"), logpage->NcqTag);
            _tprintf(_T("\t IDLE IMMEDIATE with UNLOAD error: %s\n"), (logpage->UNL == 1) ? _T("Yes") : _T("No"));
            _tprintf(_T("\t Non-NCQ Command: %s\n"), (logpage->NonQueuedCmd == 1) ? _T("Yes") : _T("No"));
            _tprintf(_T("\t Status: %X\n"), logpage->Status);
            _tprintf(_T("\t Error: %X\n"), logpage->Error);
            _tprintf(_T("\t LBA7_0: %X\n"), logpage->LBA7_0);
            _tprintf(_T("\t LBA15_8: %X\n"), logpage->LBA15_8);
            _tprintf(_T("\t LBA23_16: %X\n"), logpage->LBA23_16);
            _tprintf(_T("\t LBA31_24: %X\n"), logpage->LBA31_24);
            _tprintf(_T("\t LBA39_32: %X\n"), logpage->LBA39_32);
            _tprintf(_T("\t LBA47_40: %X\n"), logpage->LBA47_40);
            _tprintf(_T("\t Device: %X\n"), logpage->Device);
            _tprintf(_T("\t Count7_0: %X\n"), logpage->Count7_0);
            _tprintf(_T("\t Count15_8: %X\n"), logpage->Count15_8);
            _tprintf(_T("\t SenseKey: %X\n"), logpage->SenseKey);
            _tprintf(_T("\t ASC: %X\n"), logpage->ASC);
            _tprintf(_T("\t ASCQ: %X\n"), logpage->ASCQ);
            _tprintf(_T("\t Checksum: %X\n"), logpage->Checksum);
            _tprintf(_T("\n"));
        }
    }

    return TRUE;
}

BOOLEAN
DeviceReadLogNcqNonData (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
)
{
    BOOLEAN demoteBySize = FALSE;
    BOOLEAN changeByLbaRange = FALSE;
    BOOLEAN control = FALSE;

    if (DeviceList[DeviceIndex].LogPage.NcqNonDataPageCount > 0) {
        ATA_PT               ataPt = {0};
        PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
        ULONG                bytesReturned = 0;

        BuildReadLogExCommand(passThru, IDE_GP_LOG_NCQ_NON_DATA_ADDRESS, 0, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE
                                           );

        if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
            PGP_LOG_NCQ_NON_DATA logpage = (PGP_LOG_NCQ_NON_DATA)ataPt.Buffer;

            demoteBySize = (logpage->SubCmd2.HybridDemoteBySize == 1);
            changeByLbaRange = (logpage->SubCmd3.HybridChangeByLbaRange == 1);
            control = (logpage->SubCmd4.HybridControl == 1);
        }
    }

    _tprintf(_T("\t HybridDemoteBySize:     %s\n"), demoteBySize ? _T("Supported") : _T("NOT Supported"));
    _tprintf(_T("\t HybridChangeByLbaRange: %s\n"), changeByLbaRange ? _T("Supported") : _T("NOT Supported"));
    _tprintf(_T("\t HybridControl:          %s\n"), control ? _T("Supported") : _T("NOT Supported"));
    _tprintf(_T("\n"));

    return TRUE;
}

BOOLEAN
DeviceReadLogNcqSendReceive (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
)
{
    BOOLEAN evict = FALSE;
    BOOLEAN dataSetManagement = FALSE;
    BOOLEAN ncqTrim = FALSE;

    if (DeviceList[DeviceIndex].LogPage.NcqSendReceivePageCount > 0) {
        ATA_PT               ataPt = {0};
        PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
        ULONG                bytesReturned = 0;

        BuildReadLogExCommand(passThru, IDE_GP_LOG_NCQ_SEND_RECEIVE_ADDRESS, 0, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE
                                           );

        if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
            PGP_LOG_NCQ_SEND_RECEIVE logpage = (PGP_LOG_NCQ_SEND_RECEIVE)ataPt.Buffer;

            evict = (logpage->SubCmd.HybridEvict == 1);
            dataSetManagement = (logpage->SubCmd.DataSetManagement == 1);
            if (dataSetManagement) {
                ncqTrim = (logpage->DataSetManagement.Trim == 1);
            }
        }
    }

    _tprintf(_T("\t HybridEvict:     %s\n"), evict ? _T("Supported") : _T("NOT Supported"));
    _tprintf(_T("\t DataSetManagement: %s\n"), dataSetManagement ? _T("Supported") : _T("NOT Supported"));
    if (dataSetManagement) {
        _tprintf(_T("\t     NcqTrim:          %s\n"), ncqTrim ? _T("Supported") : _T("NOT Supported"));
    }
    _tprintf(_T("\n"));

    return TRUE;
}

BOOLEAN
DeviceReadLogHybridInfo (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
)
{
    if (DeviceList[DeviceIndex].LogPage.HybridInfoPageCount > 0) {
        ATA_PT               ataPt = {0};
        PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
        ULONG                bytesReturned = 0;

        BuildReadLogExCommand(passThru, IDE_GP_LOG_HYBRID_INFO_ADDRESS, 0, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE
                                           );

        if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
            PGP_LOG_HYBRID_INFORMATION_HEADER header = (PGP_LOG_HYBRID_INFORMATION_HEADER)ataPt.Buffer;

            int i;

            _tprintf(_T("\t Hybrid Info Descr Count: %u\n"), header->HybridInfoDescrCount);
            _tprintf(_T("\t Hybrid Info Enabled: %u\n"), header->Enabled);
            _tprintf(_T("\t Hybrid Health: %u\n"), header->HybridHealth);
            _tprintf(_T("\t Dirty Low Threshold: %u\n"), header->DirtyLowThreshold);
            _tprintf(_T("\t Dirty High Threshold: %u\n"), header->DirtyHighThreshold);
            _tprintf(_T("\t Optimal Write Granularity: %u\n"), header->OptimalWriteGranularity);
            _tprintf(_T("\t Maximum Hybrid PriorityLevel: %u\n"), header->MaximumHybridPriorityLevel);
            _tprintf(_T("\t Power Condition: %u\n"), header->PowerCondidtion);
            _tprintf(_T("\t Caching Medium Enabled: %u\n"), header->CachingMediumEnabled);
            _tprintf(_T("\t Maximum Priority Behavior: %u\n"), header->SupportedOptions.MaximumPriorityBehavior);
            _tprintf(_T("\t Time Since Enabled: %u\n"), header->TimeSinceEnabled);
            _tprintf(_T("\t NVM Size: %I64u\n"), header->NVMSize);
            _tprintf(_T("\t Enable Count: %I64u\n"), header->EnableCount);
            _tprintf(_T("\t Maximum Eviction Commands: %u\n"), header->MaximumEvictionCommands);
            _tprintf(_T("\t Maximum Eviction Data Blocks: %u\n"), header->MaximumEvictionDataBlocks);
            _tprintf(_T("\n"));

            for (i = 0; i < header->HybridInfoDescrCount; i++) {
                PGP_LOG_HYBRID_INFORMATION_DESCRIPTOR descr = (PGP_LOG_HYBRID_INFORMATION_DESCRIPTOR)((PUCHAR)header + sizeof(GP_LOG_HYBRID_INFORMATION_HEADER) * (i + 1));

                _tprintf(_T("\t\t Hybrid Priority: %u\n"), descr->HybridPriority);
                _tprintf(_T("\t\t Consumed NVM Size Fraction: %u\n"), descr->ConsumedNVMSizeFraction);
                _tprintf(_T("\t\t Consumed Mapping Resources Fraction: %u\n"), descr->ConsumedMappingResourcesFraction);
                _tprintf(_T("\t\t Consumed NVM Size For Dirty Data Fraction: %u\n"), descr->ConsumedNVMSizeForDirtyDataFraction);
                _tprintf(_T("\t\t Consumed Mapping Resources For Dirty Data Fraction: %u\n"), descr->ConsumedMappingResourcesForDirtyDataFraction);
                _tprintf(_T("\n"));
            }

        }
    }

    return TRUE;
}

VOID
DeviceHybridIdentify (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    )
/*++

Routine Description:

    Display Hybrid Information feature of a device

Arguments:

    Device - Handle to the device

Return Value:

    TRUE if the command could be sent.
    FALSE otherwise

--*/
{
    BOOLEAN              succ = FALSE;

    BOOLEAN              hybridSupported = FALSE;
    BOOLEAN              hybridEnabled = FALSE;

    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned;

    //ZeroMemory(&ataPt, sizeof(ATA_PT));

    BuildIdentifyDeviceCommand(passThru);

    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );

    if (bytesReturned > 0) {
        PIDENTIFY_DEVICE_DATA identifyData = (PIDENTIFY_DEVICE_DATA)ataPt.Buffer;
        ULONG len = min(bytesReturned - FIELD_OFFSET(ATA_PT, Buffer), 512);

        if (len >= 512) {
            if (identifyData->SerialAtaFeaturesSupported.HybridInformation == 0) {
                _tprintf(_T("\t HybridInfo: NOT Supported.\n"));
            } else {
                hybridSupported = TRUE;

                if (identifyData->SerialAtaFeaturesEnabled.HybridInformation == 0) {
                    _tprintf(_T("\t HybridInfo: Supported, Disabled.\n"));
                } else {
                    hybridEnabled = TRUE;
                    _tprintf(_T("\t HybridInfo: Supported, Enabled.\n"));
                }
            }
        }
    }

    succ = DeviceReadLogDirectory(DeviceList, DeviceIndex, TRUE);

    if (succ && hybridSupported) {
        DeviceReadLogNcqNonData(DeviceList, DeviceIndex);
        DeviceReadLogNcqSendReceive(DeviceList, DeviceIndex);
        DeviceReadLogHybridInfo(DeviceList, DeviceIndex);
    }

    return;
}

VOID
DeviceNcqErrorLog (
    _In_ PDEVICE_LIST DeviceList,
    _In_ ULONG        DeviceIndex
    )
/*++

Routine Description:

    Display the NCQ command error log information

Arguments:

    Device - Handle to the device

Return Value:

    None
--*/
{
    BOOLEAN              succ = FALSE;

    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned;

    //ZeroMemory(&ataPt, sizeof(ATA_PT));

    BuildIdentifyDeviceCommand(passThru);

    //
    // send it down
    //
    bytesReturned = AtaSendPassThrough(DeviceList[DeviceIndex].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE
                                       );

    if (bytesReturned > 0) {
        PIDENTIFY_DEVICE_DATA identifyData = (PIDENTIFY_DEVICE_DATA)ataPt.Buffer;
        ULONG len = min(bytesReturned - FIELD_OFFSET(ATA_PT, Buffer), 512);

        if (len >= 512) {
            if (identifyData->SerialAtaFeaturesSupported.NCQAutosense == 0) {
                _tprintf(_T("\t NCQ AutoSense: NOT Supported.\n"));
            } else {
                _tprintf(_T("\t NCQ AutoSense: Supported.\n"));
            }
        }
    }

    succ = DeviceReadLogDirectory(DeviceList, DeviceIndex, TRUE);

    if (succ) {
        succ = DeviceReadLogNcqCommandError(DeviceList, DeviceIndex);
    }

    return;
}

BOOL
DeviceFirmwareTestAta(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
/*++

Routine Description:

    Send ATA Pass-Through IOCTL to retrieve information.
    And determine if device supports firmware upgrade.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    BOOL                 result = FALSE;
    ATA_PT               ataPt = {0};
    PATA_PASS_THROUGH_EX passThru = &ataPt.AtaPassThrough;
    ULONG                bytesReturned;

    //
    // 1. Validate the command support by getting IDENTIFY DEVICE data.
    //
    _tprintf(_T("ATA - Check DOWNLOAD MICROCODE (DMA) Command support.\n"));

    BuildIdentifyDeviceCommand(passThru);

    bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                       passThru,
                                       sizeof(ATA_PT),
                                       sizeof(ATA_PT),
                                       FALSE);

    if (bytesReturned > 0) {
        PIDENTIFY_DEVICE_DATA identifyData = (PIDENTIFY_DEVICE_DATA)ataPt.Buffer;
        ULONG len = min(bytesReturned - FIELD_OFFSET(ATA_PT, Buffer), sizeof(IDENTIFY_DEVICE_DATA));

        if (len >= sizeof(IDENTIFY_DEVICE_DATA)) {
            BOOLEAN supportDownloadMicrocode = (identifyData->CommandSetSupport.DownloadMicrocode == 1);
            BOOLEAN supportDownloadMicrocodeDma = (identifyData->AdditionalSupported.DownloadMicrocodeDmaSupported == 1);

            if (supportDownloadMicrocode) {
                _tprintf(_T("DOWNLOAD MICROCODE Command is supported.\n"));
            }

            if (supportDownloadMicrocodeDma) {
                _tprintf(_T("DOWNLOAD MICROCODE DMA Command is supported.\n"));
            }

            if (!supportDownloadMicrocode && !supportDownloadMicrocodeDma) {
                _tprintf(_T("ERROR: neither DOWNLOAD MICROCODE Command nor DOWNLOAD MICROCODE DMA Command is supported.\n"));

                result = FALSE;
                goto exit;
            }

            result = TRUE;
        }
    }

    if (!result) {
        _tprintf(_T("ERROR: IDENTIFY DEVICE Command failed to get firmware command support information.\n"));
        goto exit;
    }

    //
    // Get Identify Device Log - Supported Capability page to determine if device supports 0Eh and 0Fh subCommand.
    //
    _tprintf(_T("ATA - Check SubCommand 0x0E, 0x0F support.\n"));

    result = DeviceReadLogDirectory(DeviceList, Index, TRUE);

    if (!result) {
        _tprintf(_T("ERROR: READ LOG EXT command failed to retrieve Log Directory or Log Directory doesn't have correct version.\n"));
        goto exit;
    } else {
        //
        // Read Log - IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS
        // This is to see if the Capabilities page is supported or not.
        //
        RtlZeroMemory(passThru, sizeof(ATA_PT));

        BuildReadLogExCommand(passThru, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS, 0, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE);

        if (bytesReturned <= FIELD_OFFSET(ATA_PT, Buffer)) {
            _tprintf(_T("ERROR: READ LOG EXT command failed to retrieve IDENTIFY DEVICE DATA log.\n"));

            result = FALSE;
            goto exit;
        } else {
            _tprintf(_T("READ LOG EXT command to retrieve IDENTIFY DEVICE DATA log succeeded.\n"));
        }

        //
        // Read Log - IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS & IDE_GP_LOG_IDENTIFY_DEVICE_DATA_SUPPORTED_CAPABILITIES_PAGE
        //
        RtlZeroMemory(passThru, sizeof(ATA_PT));

        BuildReadLogExCommand(passThru, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_ADDRESS, IDE_GP_LOG_IDENTIFY_DEVICE_DATA_SUPPORTED_CAPABILITIES_PAGE, 1, 0);

        bytesReturned = AtaSendPassThrough(DeviceList[Index].Handle,
                                           passThru,
                                           sizeof(ATA_PT),
                                           sizeof(ATA_PT),
                                           FALSE);

        if (bytesReturned > FIELD_OFFSET(ATA_PT, Buffer)) {
            PIDENTIFY_DEVICE_DATA_LOG_PAGE_SUPPORTED_CAPABILITIES page = (PIDENTIFY_DEVICE_DATA_LOG_PAGE_SUPPORTED_CAPABILITIES)ataPt.Buffer;

            if (page->Header.RevisionNumber != IDE_GP_LOG_VERSION) {
                _tprintf(_T("ERROR: READ LOG - IDENTIFY DEVICE DATA log - Capabilities page: Revision (should be 1): %I64d\n"), page->Header.RevisionNumber);
                result = FALSE;
                goto exit;
            } else if (page->Header.PageNumber != IDE_GP_LOG_IDENTIFY_DEVICE_DATA_SUPPORTED_CAPABILITIES_PAGE) {
                _tprintf(_T("ERROR: READ LOG - IDENTIFY DEVICE DATA log - Capabilities page: Page Number (should be 3): %I64d\n"), page->Header.PageNumber);
                result = FALSE;
                goto exit;
            } else if (page->DownloadMicrocodeCapabilities.Valid != 1) {
                _tprintf(_T("ERROR: READ LOG - IDENTIFY DEVICE DATA log - Capabilities page: Data Valid (should be 1): %I64d\n"), page->DownloadMicrocodeCapabilities.Valid);
                result = FALSE;
                goto exit;
            } else if (page->DownloadMicrocodeCapabilities.DmOffsetsDeferredSupported == 0) {
                _tprintf(_T("ERROR: READ LOG - IDENTIFY DEVICE DATA log - Capabilities page: SubCommand 0Eh and 0Fh are not supported.\n"));
                result = FALSE;
                goto exit;
            }
        } else {
            _tprintf(_T("ERROR: READ LOG - IDENTIFY DEVICE DATA log - Capabilities page failed.\n"));
            result = FALSE;
            goto exit;
        }
    }

    result = TRUE;

    //
    // Issue IOCTL to get firmware upgrade support information.
    //

exit:

    return result;
}

VOID
DeviceAtaQueryProtocolDataTest (
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
/*
    Issue IOCTL_STORAGE_QUERY_PROPERTY with StorageDeviceProtocolSpecificProperty.

    STORAGE_PROTOCOL_TYPE - ProtocolTypeAta

    AtaDataTypeIdentify,        // Retrieved by command - IDENTIFY DEVICE
    AtaDataTypeLogPage,         // Retrieved by command - READ LOG EXT

*/
{
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
    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + IDE_GP_LOG_SECTOR_SIZE;
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: allocate buffer failed, exit.\n"));
        goto exit;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeAta;
    protocolData->DataType = AtaDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = 0;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = IDE_GP_LOG_SECTOR_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Identify Device Data failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Identify Device Data - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < IDE_GP_LOG_SECTOR_SIZE)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Identify Device Data - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Identify Device Data 
    //
    {
        PIDENTIFY_DEVICE_DATA identifyDeviceData = (PIDENTIFY_DEVICE_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if ((identifyDeviceData->CheckSum == 0) ||
            (identifyDeviceData->Signature == 0)) {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: Identify Device Data not valid.\n"));
            goto exit;
        } else {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: ***Identify Device Data succeeded***.\n"));
        }
    }

    //
    // Initialize query data structure to READ LOG EXT.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeAta;
    protocolData->DataType = AtaDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = IDE_GP_LOG_DIRECTORY_ADDRESS;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = IDE_GP_LOG_SECTOR_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Log Directory failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Log Directory - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < IDE_GP_LOG_SECTOR_SIZE)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Log Directory - ProtocolData Offset/Length not valid.\n"));
    } else {

        //
        // Log Directory Data 
        //
        PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER logHeader = (PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if (logHeader->RevisionNumber != 0x0001) {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Log Directory not valid. Revision value: 0x%I64x\n"), logHeader->RevisionNumber);
            goto exit;
        } else {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: ***Get Log Directory succeeded***.\n"));
        }
    }

    //
    // Initialize query data structure to READ LOG EXT.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeAta;
    protocolData->DataType = AtaDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = IDE_GP_LOG_DEVICE_STATISTICS_ADDRESS;
    protocolData->ProtocolDataRequestSubValue = IDE_GP_LOG_DEVICE_STATISTICS_GENERAL_ERROR_PAGE;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = IDE_GP_LOG_SECTOR_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Statistics General Error failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Statistics General Error - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < IDE_GP_LOG_SECTOR_SIZE)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Statistics General Error - ProtocolData Offset/Length not valid.\n"));
    } else {

        //
        // Statistics General Error Data 
        //
        PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER logHeader = (PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if (logHeader->RevisionNumber != 0x0001) {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Statistics General Error not valid. Revision value: 0x%I64x\n"), logHeader->RevisionNumber);
            goto exit;
        } else {
            _tprintf(_T("DeviceAtaQueryProtocolDataTest: ***Get Statistics General Error succeeded***.\n"));
        }
    }

    //
    // Initialize query data structure to get SMART data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;

    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeAta;
    protocolData->DataType = AtaDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = IDE_GP_SUMMARY_SMART_ERROR;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = IDE_GP_LOG_SECTOR_SIZE;

    //
    // Send request down.
    //
    result = DeviceIoControl(DeviceList[Index].Handle,
                             IOCTL_STORAGE_QUERY_PROPERTY,
                             buffer,
                             bufferLength,
                             buffer,
                             bufferLength,
                             &returnedLength,
                             NULL
                             );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Smart Error failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Smart Error Error - data descriptor header not valid.\n"));
        return;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < IDE_GP_LOG_SECTOR_SIZE)) {
        _tprintf(_T("DeviceAtaQueryProtocolDataTest: Get Smart Error - ProtocolData Offset/Length not valid.\n"));
    } else {

        //
        // Statistics General Error Data 
        //
        PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER logHeader = (PIDENTIFY_DEVICE_DATA_LOG_PAGE_HEADER)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        //_tprintf(_T("DeviceAtaQueryProtocolDataTest: Smart Error 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X .\n"), 
        //         logHeader[10], logHeader[11], logHeader[12], logHeader[13], logHeader[14], logHeader[15], logHeader[16], logHeader[17]);

        _tprintf(_T("DeviceAtaQueryProtocolDataTest: ***Get Smart Error succeeded***.\n"));
    }


exit:

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    return;
}

BOOL
DeviceFirmwareTestScsi(
    _In_ PDEVICE_LIST DeviceList,
    _In_ DWORD        Index
    )
/*++

Routine Description:

   Send SCSI Pass-Through request to determine if device supports firmware upgrade.
   Note: this function is left empty currently.

Arguments:

    DeviceList - 
    Index - 

Return Value:

    None

--*/
{
    BOOL    result = TRUE;
    UNREFERENCED_PARAMETER(DeviceList);
    UNREFERENCED_PARAMETER(Index);

    //
    // The device must retain VPD page 0x83 through download and activate of a firmware image.
    // The download operation must be possible using SCSI Buffer ID 0 and a transfer size of 64KB.
    //

    return result;
}

