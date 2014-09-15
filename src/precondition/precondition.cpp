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

// SSD/HDD initialization and "preconditioning" utility.  MarkSan 9/2013

#include "precondition.h"
#include "steady_state_detector.h"

using namespace std;

struct Parameters
{
    string testFileName;
    int blockSize;
    AccessPattern accessPattern;
    int outstandingIOs;
    int writePercentage;
    bool runUntilSteadyState;
    int steadyStateGatherSec;
    int steadyStateDwellSec;
    double steadyStateTolerance;
    bool rawDisk;
    bool shouldPrompt;
    string progressPrefix;

    Parameters()
        : testFileName( "INVALID" )
        , blockSize( DEFAULT_IO_SIZE )
        , accessPattern( DEFAULT_ACCESS_PATTERN )
        , outstandingIOs( DEFAULT_OUTSTANDING_IOS )
        , writePercentage( DEFAULT_WRITE_PERCENTAGE )
        , runUntilSteadyState( false )
        , steadyStateGatherSec( SteadyStateDetector::DEFAULT_GATHER_SECONDS )
        , steadyStateDwellSec( SteadyStateDetector::DEFAULT_DWELL_SECONDS )
        , steadyStateTolerance( SteadyStateDetector::DEFAULT_SLOPE_TOLERANCE )
        , rawDisk( false )
        , shouldPrompt( true )
    {};
}
params;

void printUsage( int argc, char *argv[] )
{
    string exeName( argv[0] );

    cerr 
        << "Usage: " << exeName 
        << " <target> [options]" << endl << endl;

    cerr 
        << "For <target>, pass a filename or \\\\.\\PHYSICALDRIVE number"
        << endl << endl;
    cerr 
        << "Available options:\n"
        << "  -Y\tDon't prompt before writing target (use with caution)\n"
        << "  -bX\tUse X kilobyte sized blocks (default: " 
            << ( DEFAULT_IO_SIZE / 1024 / 1024 ) << "MB)\n"
        << "  -r\tUse a random pattern of IOs (default: " 
            << accessPatternToString( DEFAULT_ACCESS_PATTERN ) << ")\n"
        << "  -oX\tUse X outstanding IOs (default: "
            << DEFAULT_OUTSTANDING_IOS << ")\n"
        << "  -wX\tGenerate IOs such that X% are writes (default: "
            << DEFAULT_WRITE_PERCENTAGE << "%)\n"
        << "  -ss\tRun until steady-state is achieved\n"
        << "  -gX\tGather IOPS for X seconds for steady-state (default: "
            << SteadyStateDetector::DEFAULT_GATHER_SECONDS << ")\n"
        << "  -dX\tRequire X seconds of dwell in steady-state (default: "
            << SteadyStateDetector::DEFAULT_DWELL_SECONDS << ")\n"
        << "  -tX\tSlope tolerance for steady-state (default: "
            << SteadyStateDetector::DEFAULT_SLOPE_TOLERANCE << ")\n"
        << "  -pSTR\tPrefix progress message with STR (default: none)\n"
        << endl << endl;

    exit( EXIT_FAILURE );
}

void parseCmdline( int argc, char *argv[] )
{
    if( argc < 2 )
        printUsage( argc, argv );
   
    vector<string> args( argv + 1, argv + argc );

    bool gatherSeen = false;
    bool dwellSeen = false;
    bool tolerSeen = false;

    for( auto &arg : args )
    {
        if( arg[0] == '-' )
        {
            if( arg.substr( 1 ) == "ss" )
            {
                params.runUntilSteadyState = true;
            }
            else
            {
                switch( arg[1] )
                {
                    case 'Y':
                        params.shouldPrompt = false;
                        break;

                    case 'b':
                        // Block size in KB.  Convert to bytes.
                        params.blockSize = stoi( arg.substr( 2 ) ) * 1024;
                        break;

                    case 'r':
                        params.accessPattern = RANDOM;
                        break;

                    case 'o':
                        params.outstandingIOs = stoi( arg.substr( 2 ) );
                        break;
                    
                    case 'w':
                        params.writePercentage = stoi( arg.substr( 2 ) );
                        break;
                   
                    case 'g':
                        params.steadyStateGatherSec = 
                            stoi( arg.substr( 2 ) );

                        gatherSeen = true;
                        break;

                    case 'd':
                        params.steadyStateDwellSec
                            = stoi( arg.substr( 2 ) );

                        dwellSeen = true;
                        break;
                    
                    case 't':
                        params.steadyStateTolerance
                            = stod( arg.substr( 2 ) );

                        tolerSeen = true;
                        break;

                    case 'p':
                        params.progressPrefix = arg.substr( 2 );
                        break;

                    default:
                        cerr << "Unknown switch: " << arg << endl;
                        printUsage( argc, argv );
                }
            }
        }
        else
        {
            if( regex_match( arg, regex( "^\\d+$" ) ) )
            {
                params.testFileName = 
                    string( "\\\\.\\PHYSICALDRIVE" ) + arg;

                params.rawDisk = true;
            }
            else if( regex_match( arg, regex( "^[[:alpha:]]:.*" ) ) )
            {
                params.testFileName = arg;
            }
            else
            {
                cerr << "Unexpected target: " << arg << endl;
                printUsage( argc, argv );
            }
        }
    }

    if( ( params.accessPattern == RANDOM ) && 
            !params.runUntilSteadyState )
    {
        // TO DO: Properly support this case.
        //
        // This is tricker than it may seem.
        // 
        // We'd need to randomly write each block once and only once,
        // leaving no gaps and never writing the same block twice.  "Oh,
        // we'll just Fisher-Yates shuffle a vector of offsets.", you say.
        // Congratulations, you've just designed an algorithm that requires
        // ~4 GB of DRAM.
        //
        // LauraCa's solution is promising.  For a target with X blocks, we
        // randomly pick X of them, avoiding duplicates via a bitset.  Then
        // we walk the bitset pushing the offsets of the "holes" onto a
        // vector.  We then shuffle the vector and fill the holes.  This will
        // require less DRAM.  Seems like a good uniform RNG will leave us
        // with 36.8% "gaps" to fill. 
        //
        // --MarkSan
        cerr << "Warning: full target write not guaranteed with -r\n";
    }
    
    if( !(params.outstandingIOs >= 1) ) 
    {
        cerr << "Error: -oX must be >= 1\n";
        exit( EXIT_FAILURE ); 
    }
    else if( !(params.outstandingIOs <= MAX_OUTSTANDING_IOS) )
    {
        cerr << "Error: -oX must be <= " << MAX_OUTSTANDING_IOS << "\n";
        exit( EXIT_FAILURE ); 
    }
    
    if( params.writePercentage < 0 ) 
    {
        cerr << "Error: -wX must be >= 0\n";
        exit( EXIT_FAILURE ); 
    }
    else if( params.writePercentage > 100 )
    {
        cerr << "Error: -oX must be <= 100\n";
        exit( EXIT_FAILURE ); 
    }
    
    if( ( params.writePercentage < 100 ) && 
            !params.runUntilSteadyState )
    {
        cerr << "Warning: full target write not guaranteed with -wX < 100\n";
    }

    if( params.steadyStateGatherSec <= 0 ) 
    {
        cerr << "Error: -ssg must be > 0\n";
        exit( EXIT_FAILURE ); 
    }
    
    if( params.steadyStateDwellSec <= 0 ) 
    {
        cerr << "Error: -ssd must be > 0\n";
        exit( EXIT_FAILURE ); 
    }
    
    if( ( gatherSeen || dwellSeen || tolerSeen ) && 
            !params.runUntilSteadyState)
    {
        cerr << "Error: -g, -d, and -t require -ss\n";
        exit( EXIT_FAILURE ); 
    }
}

void continuePrompt()
{
    cerr << endl 
        << "\tWARNING! WARNING! WARNING!"
        << endl 
        << "\tThis will overwrite "
        << params.testFileName
        << endl << endl;

    cerr << "Are you sure you want to continue? [Y/N]" << endl;

    if( toupper( _getch() ) == 'Y' )
    {
        cerr << endl;
        return;
    }

    exit( EXIT_SUCCESS );
}

class StatusLine
{
    private:
    
    int64_t period_;
    int64_t lastUpdate_;
    int lastLen_;

    public:

    StatusLine( double periodSeconds = 1 )
        : period_( periodSeconds * QPC_TICKS_PER_SEC )
        , lastUpdate_( qpc() )
        , lastLen_( 0 )
    {}

    public:
    
    void forceWrite( std::string msg )
    {
        fprintf( stderr, "%-*s\r", lastLen_, msg.c_str() );

        lastLen_ = msg.length();
        lastUpdate_ = qpc();
    }

    void writeMaybe( std::string msg )
    {
        // Rate limit to 1 update per period
        bool overdue = ( qpc() > lastUpdate_ + period_ );

        if( overdue ) forceWrite( msg );
    }
};

class ThroughputMeter
{
    private:

    double periodSeconds_;

    int64_t periodTicks_;
    int64_t lastResetTicks_;
    
    int64_t currentIOs_;
    int64_t currentBytes_;
    
    int64_t previousIOs_;
    int64_t previousBytes_;

    public:

    ThroughputMeter( double periodSeconds = 1 )
        : periodSeconds_( periodSeconds )
        , periodTicks_( periodSeconds * QPC_TICKS_PER_SEC )
        , lastResetTicks_( std::numeric_limits<int64_t>::min() )
        , currentIOs_( 0 )
        , currentBytes_( 0 )
    {}

    public:

    void trackCompletion( int bytes )
    {
        int64_t now = qpc();

        bool resetOverdue =
            now > ( lastResetTicks_ + periodTicks_ );

        if( resetOverdue )
        {
            previousIOs_ = currentIOs_;
            previousBytes_ = currentBytes_;

            currentIOs_ = 0;
            currentBytes_ = 0;

            lastResetTicks_ = now;
        }
            
        currentIOs_++;
        currentBytes_ += bytes;
    }
    
    double getIOPS() const
    {
        return static_cast<double>(previousIOs_) / periodSeconds_; 
    }
    
    double getMBPS() const
    {
        double megaBytes = 
            static_cast<double>(previousBytes_) / 1024 / 1024;

        return megaBytes / periodSeconds_; 
    }
};

class IOGenerator
{
    private:

    HANDLE targetHandle_;
    int64_t targetSize_;

    int64_t completedBytes_;
    int postedIOs_;
    int completedIOs_;
    int inFlight_;
    
    bool steadyStateAchieved_;
    bool steadyStateAbandonedTime_;
    
    array< OVERLAPPED, MAX_OUTSTANDING_IOS > overlapped_;

    const int TOTAL_BLOCKS;
    
    static const int MAX_STEADY_STATE_WAIT_HOURS = 6;
   
    int64_t qpcStart_;
    
    SteadyStateDetector steadyStateDetector_;
    ThroughputMeter throughputMeter_;
    StatusLine statusLine_;

    public:

    IOGenerator( 
            HANDLE targetHandle,
            int64_t targetSize )
        : targetHandle_( targetHandle )
        , targetSize_( targetSize )
        , completedBytes_( 0 )
        , postedIOs_( 0 )
        , completedIOs_( 0 )
        , inFlight_( 0 )
        , steadyStateAchieved_( false )
        , steadyStateAbandonedTime_( false )
        , TOTAL_BLOCKS( divRoundUp( targetSize, params.blockSize ) )
        , qpcStart_( qpc() )
        , steadyStateDetector_(
                params.steadyStateGatherSec,
                params.steadyStateDwellSec,
                params.steadyStateTolerance )
    {
        // We will reuse this write buffer over and over with a 
        // random offset. Should be enough entropy to defeat compression.
        //
        // N.B: Earlier attempts generated new random data 
        // for each IO, and ended up CPU-limited.
        randomFillBuffer( writeDataBuffer );

#ifndef NDEBUG
        for( auto &i: readDataBuffers )
        {
            i.fill( 0xFF );
        }
#endif
        for( auto &i: overlapped_ )
        {
            // Completion routines allow us to abuse the hEvent field
            // See ioCompletionRoutine for the matching cast back.
            i.hEvent = reinterpret_cast<HANDLE>( this );
        }
    }

    void run()
    {
        // Kick off initial IOs
        const int initialIOs = min( TOTAL_BLOCKS, params.outstandingIOs ); 

        for( int i = 0; i < initialIOs; ++i )
        {
            postNextIO( i );
        }

        do 
        {
            // Alertable wait allows async IOs to complete
            SleepEx( INFINITE, true );
        }
        while( !allIOsCompleted() );

        // We are now finshed writing
        
        checkedFlushFileBuffers( targetHandle_ );
       
        doFinalSanityChecks();

        cerr << endl;

        if( params.runUntilSteadyState )
        {
            cout << getSteadyStateReasonString() << endl;
        }     
    }

    private:
    
    void doFinalSanityChecks() const
    {
        assert( inFlight_ == 0 );
        assert( shouldPostAnotherIO() == false );

        if( !params.runUntilSteadyState &&
                ( params.accessPattern == SEQUENTIAL ) )
        {
            assert( completedIOs_ == TOTAL_BLOCKS );
            assert( completedBytes_ == targetSize_ );
        }
    }

    bool shouldPostAnotherIO() const
    {
        if( params.runUntilSteadyState )
        {
            if( steadyStateAchieved_ || steadyStateAbandonedTime_ )
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        
        if( postedIOs_ < TOTAL_BLOCKS )
        {
            return true;
        }

        return false;
    }
    
    bool allIOsCompleted()
    {
        return ( shouldPostAnotherIO() == false ) && ( inFlight_ == 0 );
    }

    friend 
        void CALLBACK ioCompletionRoutine( DWORD, DWORD, LPOVERLAPPED );

    int getRandomLegalDataBufferOffset() const
    {
        const int MAX_LEGAL_SECTOR_OFFSET = MAX_IO_SIZE / SECTOR_SIZE;

        uniform_int_distribution<int> dist( 0, MAX_LEGAL_SECTOR_OFFSET );

        int randomSectorOffset = dist( rngEngine );

        // Convert sector offset to byte offset
        return randomSectorOffset * SECTOR_SIZE;
    }

    int64_t getNextFileOffset() const
    {
        int64_t nextBlockNum;
       
        if( params.accessPattern == SEQUENTIAL )
        {
            nextBlockNum = postedIOs_ % TOTAL_BLOCKS;
        }
        else
        {
            assert( params.accessPattern == RANDOM );
        
            const int MAX_LEGAL_BLOCK = TOTAL_BLOCKS - 1;

            uniform_int_distribution<int> dist( 0, MAX_LEGAL_BLOCK );

            nextBlockNum = dist( rngEngine );
        }
            
        return nextBlockNum * params.blockSize;
    }

    int getIOSizeForFileOffset( LARGE_INTEGER offset ) const
    {
        bool isLastBlock =
            ( targetSize_ - offset.QuadPart ) < params.blockSize;

        if( isLastBlock )
        {
            return targetSize_ % params.blockSize;
        }

        return params.blockSize;
    }

    bool shouldPostWrite()
    {
        uniform_int_distribution<int> dist( 1, 100 );

        if( dist( rngEngine ) <= params.writePercentage )
        {
            return true;
        }
        
        return false;
    }

    void postNextIO( int idx )
    {
        assert( idx < params.outstandingIOs );
        
        LARGE_INTEGER fileOffset;
        fileOffset.QuadPart = getNextFileOffset();
        
        int ioSize = getIOSizeForFileOffset( fileOffset );

        overlapped_[idx].Offset = fileOffset.LowPart;
        overlapped_[idx].OffsetHigh = fileOffset.HighPart;
  
        if( shouldPostWrite() )
        {
            // Defeat de-duplication.
            // This is safe because buffer is 2x MAX_IO_SIZE.
            //
            // ISSUE-REVIEW: we might pick the same offset twice.
            // Should we just round-robin instead?
            int dataBufferOffset = getRandomLegalDataBufferOffset();

            checkedWriteFileEx(
                    targetHandle_,
                    &writeDataBuffer[dataBufferOffset],
                    ioSize,
                    &overlapped_[idx],
                    &ioCompletionRoutine );
        }
        else
        {
            checkedReadFileEx(
                    targetHandle_,
                    &readDataBuffers[idx][0],
                    ioSize,
                    &overlapped_[idx],
                    &ioCompletionRoutine );
        }

        inFlight_++;
    
        assert( inFlight_ <= params.outstandingIOs );
        
        postedIOs_++;
    }

    void postNextIO( OVERLAPPED *op )
    {
        // Convert overlapped pointer to index
        int idx = op - &overlapped_[0];
        postNextIO( idx );
    }

    bool timeToUpdateProgressMessage() const
    {
        int64_t now = qpc();
        static int64_t lastUpdate = 0;

        // Limit console updates to once a second
        bool timeForAnotherUpdate = 
            now > ( lastUpdate + QPC_TICKS_PER_SEC ); 

        if( timeForAnotherUpdate )
        {
            lastUpdate = now;
            return true;
        }

        return false;
    }

    void handleCompletionTotalIOs()
    {
        double percentCompleted = 
            static_cast<double>( completedBytes_ ) / targetSize_ * 100;

        ostringstream msg;
        
        msg.setf( std::ios::fixed );
        msg.precision( 1 );

        msg << params.progressPrefix.c_str()
            << percentCompleted << "%";

        msg << " [" << throughputMeter_.getMBPS() << " MB/s]";

        // Ensure we print a message for 100% to avoid
        // the appearance of having stopped prematurely
        if( percentCompleted == 100 )
        {
            statusLine_.forceWrite( msg.str() );
        }
        else
        {
            statusLine_.writeMaybe( msg.str() );
        }
    }

    string getSteadyStateReasonString() const
    {
        assert( steadyStateAchieved_ || steadyStateAbandonedTime_ );
        
        ostringstream msg;
        
        int minutesElapsed = secondsSince( qpcStart_ ) / 60;
        
        if( steadyStateAchieved_ )
        {
            msg << "achieved steady-state after "
                << minutesElapsed << " minutes";
        }
        else if( steadyStateAbandonedTime_ )
        {
            msg << "abandoned steady-state after "
                << minutesElapsed << " minutes";
        }

        return msg.str();
    }

    void handleCompletionSteadyState()
    {
        steadyStateDetector_.trackCompletion();
        
        double hoursElapsed = secondsSince( qpcStart_ ) / 3600;
        
        bool done = false;
        
        ostringstream msg;
            
        msg.setf( std::ios::fixed );
        msg.precision( 1 );
        
        msg << params.progressPrefix.c_str();

        // Some drives have such erratic performance that
        // they may never meet our definiton of steady-state.
        if( hoursElapsed >= MAX_STEADY_STATE_WAIT_HOURS )
        {
            steadyStateAbandonedTime_ = true;
            done = true;
        }
            
        if( steadyStateDetector_.done() )
        {
            steadyStateAchieved_ = true;
            done = true;
        }

        if( done )
        {
            msg << getSteadyStateReasonString();

            statusLine_.forceWrite( msg.str() );
        }
        else
        {
            msg << steadyStateDetector_.getProgressMessage()
                << " [" << throughputMeter_.getMBPS() << " MB/s]";

            statusLine_.writeMaybe( msg.str() );
        }
    }

    void handleCompletion( int bytes )
    {
        inFlight_--;

        completedIOs_++;
        completedBytes_ += bytes;
 
        throughputMeter_.trackCompletion( bytes );

        if( params.runUntilSteadyState )
        {
            handleCompletionSteadyState();
        }
        else
        {
            handleCompletionTotalIOs();
        }
    }
};
    
void CALLBACK ioCompletionRoutine(
        DWORD error,
        DWORD bytes,
        LPOVERLAPPED overlapped )
{
    assert( overlapped != NULL );

    if( error )
    {
        cerr
            << endl << "IO failed to complete. Error: " 
            << error << endl;

        exit( EXIT_FAILURE );
    }
   
    // Coerce our secret pointer back to its proper type.
    IOGenerator *ioGen = 
        reinterpret_cast<IOGenerator*>( overlapped->hEvent );

    assert( ioGen != NULL );
                
    ioGen->handleCompletion( bytes );

    if( ioGen->shouldPostAnotherIO() )
    {
        ioGen->postNextIO( overlapped );
    }
}

int main( int argc, char *argv[] )
{
    parseCmdline( argc, argv );

    if( params.writePercentage > 0 && params.shouldPrompt )
    {
        continuePrompt();
    }

    HANDLE targetHandle = checkedOpenTarget( params.testFileName );
    
    const int64_t originalTargetSize = params.rawDisk ?
        checkedGetDiskLength( targetHandle ) :
        checkedGetFileSizeEx( targetHandle );
  
    int64_t targetSize = originalTargetSize;

    if( targetSize % SECTOR_SIZE != 0 )
    {
        cerr << "Warning: target is not an even multiple of "
            << SECTOR_SIZE << " B" << endl;
        
        cerr << "Target will not be completely overwritten" << endl;

        targetSize = 
            (targetSize / SECTOR_SIZE ) * SECTOR_SIZE ;
    }

    // Do all the IOs
    IOGenerator( targetHandle, targetSize ).run();

    // We should never extend the target size
    const int64_t finalTargetSize = params.rawDisk ?
        checkedGetDiskLength( targetHandle ) :
        checkedGetFileSizeEx( targetHandle );

    assert( finalTargetSize == originalTargetSize );

    CloseHandle( targetHandle );

    exit( EXIT_SUCCESS );
}
