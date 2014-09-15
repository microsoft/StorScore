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

#pragma once
#ifndef __WRITE_TARGET_H_
#define __WRITE_TARGET_H_

#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <functional>
#include <algorithm>
#include <regex>
#include <array>

#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cassert>

#define NOMINMAX
#include <windows.h>
#include <conio.h>

std::mt19937 rngEngine;

// ISSUE-REVIEW: can we actually sustain QD this high without multithreading?
const int MAX_OUTSTANDING_IOS = 256; // queue depth

const int MAX_IO_SIZE = 2 * 1024 * 1024; // 2MB
#define SECTOR_SIZE 512 // FIXME: This should be dynamic

enum AccessPattern { SEQUENTIAL, RANDOM };

std::string accessPatternToString( const AccessPattern& ap )
{
    return ( ap == SEQUENTIAL ) ? "sequential" : "random";
}

const int DEFAULT_IO_SIZE = 1024 * 1024; // 1MB
const AccessPattern DEFAULT_ACCESS_PATTERN = SEQUENTIAL;
const int DEFAULT_OUTSTANDING_IOS = MAX_OUTSTANDING_IOS;
const int DEFAULT_WRITE_PERCENTAGE = 100;

// 2x the MAX_IO_SIZE, to enable random offsets later on
__declspec( align( SECTOR_SIZE ) )
    std::array< uint8_t, MAX_IO_SIZE * 2 >
        writeDataBuffer;

typedef std::array< uint8_t, MAX_IO_SIZE > ReadDataBuffer;

__declspec( align( SECTOR_SIZE ) )
    std::array< ReadDataBuffer, MAX_OUTSTANDING_IOS >
        readDataBuffers;

template<typename T>
void randomFillBuffer( T& buffer )
{
    using namespace std;

    uniform_int_distribution<int> uniformByteDistribution( 0, 0xFF );

    auto rng = bind( uniformByteDistribution, rngEngine );

    generate_n( buffer.begin(), buffer.size(), rng );
}

int64_t qpf() 
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
}

const int64_t QPC_TICKS_PER_SEC = qpf();

int64_t qpc()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double secondsSince( int64_t start )
{
    return static_cast<double>( ( qpc() - start ) / QPC_TICKS_PER_SEC );
}

int64_t divRoundUp( int64_t dividend, int64_t divisor )
{
    return (dividend + (divisor - 1)) / divisor;
}

HANDLE checkedCreateEvent( bool state )
{
    using namespace std;

    HANDLE event 
        = CreateEvent( NULL, true, state, NULL );

    if( event == NULL )
    {
        cerr << "CreateEvent failed. GetLastError = "
            << GetLastError() << endl;

        exit( EXIT_FAILURE );
    }

    return event;
}

HANDLE checkedOpenTarget( std::string targetName )
{
    using namespace std;

    HANDLE h = CreateFile( 
        targetName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
        0 );

    if( h == INVALID_HANDLE_VALUE )
    {
        int error = GetLastError();

        cerr
            << "CreateFile failed. GetLastError = "
            << error << endl;

        if( error == ERROR_ACCESS_DENIED )
        {
            cerr 
                << "Are you running as administrator?" 
                << endl;
        }

        exit( EXIT_FAILURE );
    }

    return h;
}

int64_t checkedGetFileSizeEx( HANDLE targetHandle )
{
    using namespace std;

    LARGE_INTEGER fileSize = {0};

    bool retVal = GetFileSizeEx( targetHandle, &fileSize );

    if( retVal == 0 )
    {
        cerr << "GetFileSizeEx failed. GetLastError = " 
            << GetLastError() << endl;

        exit( EXIT_FAILURE );
    }

    return fileSize.QuadPart;
}

int64_t checkedGetDiskLength( HANDLE targetHandle )
{
    using namespace std;

    GET_LENGTH_INFORMATION diskLengthInfo = {0};
    OVERLAPPED overlapped = {0};
    DWORD bytesReturned = 0;

    HANDLE event = checkedCreateEvent( false );

    overlapped.hEvent = event;

    bool retVal = DeviceIoControl(
            targetHandle,
            IOCTL_DISK_GET_LENGTH_INFO,
            NULL,
            0,
            &diskLengthInfo,
            sizeof(diskLengthInfo),
            &bytesReturned,
            &overlapped );

    if( retVal == 0 )
    {
        int error = GetLastError();

        if( error == ERROR_IO_PENDING )
        {
            GetOverlappedResult(
                    targetHandle,
                    &overlapped,
                    &bytesReturned,
                    true );
        }
        else
        {
            cerr << "DeviceIOControl failed. GetLastError = " 
                << error << endl;

            exit( EXIT_FAILURE );
        }
    }

    CloseHandle( event );

    return diskLengthInfo.Length.QuadPart;
}

void checkedFlushFileBuffers( HANDLE targetHandle )
{
    using namespace std;

    bool success = FlushFileBuffers( targetHandle );

    if( !success )
    {
        cerr << "FlushFileBuffers failed. GetLastError = " 
            << GetLastError() << endl;

        exit( EXIT_FAILURE );
    }
}

void checkedWriteFileEx(
        HANDLE handle,
        LPCVOID buffer,
        DWORD bytes,
        LPOVERLAPPED overlapped,
        LPOVERLAPPED_COMPLETION_ROUTINE func )
{
    using namespace std;

    bool retVal = WriteFileEx(
            handle,
            buffer,
            bytes,
            overlapped,
            func );

    int error = GetLastError();

    if( ( retVal == 0 ) || ( error != ERROR_SUCCESS ) )
    {
        cerr << "WriteFile failed. GetLastError = "
            << error << endl;

        exit( EXIT_FAILURE );
    }
}

void checkedReadFileEx(
        HANDLE handle,
        LPVOID buffer,
        DWORD bytes,
        LPOVERLAPPED overlapped,
        LPOVERLAPPED_COMPLETION_ROUTINE func )
{
    using namespace std;

    bool retVal = ReadFileEx(
            handle,
            buffer,
            bytes,
            overlapped,
            func );

    int error = GetLastError();

    if( ( retVal == 0 ) || ( error != ERROR_SUCCESS ) )
    {
        cerr << "ReadFile failed. GetLastError = "
            << error << endl;

        exit( EXIT_FAILURE );
    }
}
#endif // __WRITE_TARGET_H_
