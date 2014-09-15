@rem = ' vim: set filetype=perl: ';
@rem = ' --*-Perl-*-- ';
@rem = q( 
@echo off
setlocal
set PATH=%~dp0\perl\bin;%~dp0\bin;%PATH%
where perl.exe >NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo %~n0 requires Perl. Please install one of these:
    echo    ActiveState Perl: http://www.activestate.com/activeperl
    echo    Strawberry Perl: http://strawberryperl.com/
    exit /B 1
)
for /F "tokens=4" %%I IN ('powercfg -getactivescheme') DO set ORIG_SCHEME=%%I
powercfg -setactive SCHEME_MIN
start /B /WAIT /HIGH perl "%~f0" %*
powercfg -setactive %ORIG_SCHEME%
exit /B %ERRORLEVEL%
);

# StorScore
#
# Copyright (c) Microsoft Corporation
#
# All rights reserved. 
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

use strict;
use warnings;

use English;
use Win32;
use Time::Seconds;
use Carp;
#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };

use FindBin;
use lib "$FindBin::Bin";
use lib "$FindBin::Bin\\lib";

use Util;
use GlobalConfig;
use Precondition;
use SmartCtlRunner;
use Recipe;
use DiskSpd;
use Sqlio;
use LogmanRunner;
use Power;
use WmicRunner;

$OUTPUT_AUTOFLUSH = 1;

check_system_compatibility();

mkdir( $results_dir ) unless( -d $results_dir );

GlobalConfig::init( @ARGV ); # create %gc hash

my $smartctl_runner = SmartCtlRunner->new(
    pdnum      => $gc{'target_physicaldrive'},
    output_dir => $gc{'output_dir'}
);

$gc{'smart_supported'} = $smartctl_runner->is_functional();

my $target_model = get_drive_model( $gc{'target_physicaldrive'} );

if( ( $gc{'smart_supported'} and $smartctl_runner->is_target_ssd() ) or
    ( $target_model =~ /NVMe|SSD/ ) or
    ( $gc{'force_ssd'} ) )
{
    $gc{'is_target_ssd'} = 1;
}
else
{
    $gc{'is_target_ssd'} = 0;
}

# Precondition automatically when targeting ssd, unless
# the user specified --precondition or --noprecondition
$gc{'precondition'} //= $gc{'is_target_ssd'};

if( $gc{'is_target_ssd'} )
{
    print "Targeting SSD: $target_model\n";

    if( $gc{'smart_supported'} and 
        $smartctl_runner->is_target_sata() and
        !$smartctl_runner->is_target_6Gbps_sata() )
    {
        my $msg;

        $msg .= "\n\tWarning!\n";
        $msg .= "\tSSD target is not 6Gb/s SATA III.\n";
        $msg .= "\tThroughput will be limited.\n";

        warn $msg;
    }
   
    $gc{'recipe'} = 'recipes\\turkey_test.rcp'
        unless defined $gc{'recipe'};
}
else
{
    print "Targeting HDD: $target_model\n";
    
    $gc{'recipe'} = 'recipes\\corners.rcp'
        unless defined $gc{'recipe'};
}

undef $smartctl_runner
    unless $gc{'collect_smart'} and $gc{'smart_supported'};

print "\n";

my $recipe = Recipe->new(
    file_name            => $gc{'recipe'},
    do_initialize        => $gc{'initialize'},
    do_precondition      => $gc{'precondition'},
    io_generator_type    => $gc{'io_generator'},
    quick_test           => $gc{'quick_test'},
    test_time_override   => $gc{'test_time_override'},
    warmup_time_override => $gc{'warmup_time_override'},
    is_target_ssd        => $gc{'is_target_ssd'},
    start_on_step        => $gc{'start_on_step'},
);

my $num_steps = $recipe->get_num_steps();
my $num_tests = $recipe->get_num_test_steps();

die "Empty recipe. Nothing to do.\n" unless $num_steps > 0;

print "Loaded $gc{'recipe'} ($num_tests tests, $num_steps steps)\n\n";

$recipe->warn_expected_run_time();

detect_scep_and_warn();

if( $gc{'clean_disk'} ) 
{
    my $msg;

    $msg .= "\tWarning!\n";
    $msg .= "\tThis will destroy \\\\.\\PHYSICALDRIVE";
    $msg .= "$gc{'target_physicaldrive'}\n\n";

    warn $msg;
}
elsif( defined $gc{'target_file'} and
    ( $gc{'initialize'} or $recipe->contains_writes() ) )
{
    die "Target is not writable\n" unless -w $gc{'target_file'};

    my $msg;

    $msg .= "\tWarning!\n";
    $msg .= "\tThis will destroy $gc{'target_file'}\n\n";

    warn $msg;
}

exit 0 unless should_proceed();

die "Results subdirectory $gc{'output_dir'} already exists!\n"
    if -e $gc{'output_dir'};

my $overall_start = time();

mkdir( $gc{'output_dir'} );

if( $pretend )
{
    $gc{'target_volume'} = 'P:';
    $gc{'target_file'} = "P:\\pretend.dat";
}
else
{
    if( $gc{'clean_disk'} )
    {
        print "Cleaning disk...\n";

        clean_disk( $gc{'target_physicaldrive'} );
    }

    if( $gc{'create_new_filesystem'} )
    {
        print "Creating new filesystem...\n";
        
        create_filesystem(
            $gc{'target_physicaldrive'},
            $gc{'partition_bytes'}
        );
       
        $gc{'target_volume'} =
            physicaldrive_to_volume( $gc{'target_physicaldrive'} );
    }

    if( $gc{'create_new_file'} )
    {
        print "Creating test file...\n";
        
        my $free_bytes = get_disk_free( $gc{'target_volume'} );

        die "Couldn't determine free space"
            unless defined $free_bytes;

        # Reserve 1GB right off the top.
        # When we tried to use the whole drive, we saw odd errors.
        # Expectation is that test results should still be valid. 
        my $size = $free_bytes - BYTES_PER_GB;

        # Support testing less then 100% of the disk
        $size = int( $size * $gc{'active_range'} / 100 );
      
        # Round to an even increment of 2MB.
        # Idea is to ensure the file size is an even multiple 
        # of pretty much any block size we might test. 
        $size = int( $size / BYTES_IN_2MB ) * BYTES_IN_2MB;

        $gc{'target_file'} = "$gc{'target_volume'}\\$TEST_FILE_NAME";

        die "Error: target file $gc{'target_file'} exists!\n"
            if -e $gc{'target_file'};

        fast_create_file( $gc{'target_file'}, $size ) 
            or die "Couldn't create $size byte file: $gc{'target_file'}\n";
    }

    if( $gc{'target_volume'} )
    {
        print "Syncing target volume...\n";
        execute_task( "sync.cmd $gc{'target_volume'}", quiet => 1 );
    }
}

write_version_file( $gc{'output_dir'} );

my $wmic_runner = WmicRunner->new(
    pdnum      => $gc{'target_physicaldrive'},
    volume     => $gc{'target_volume'},
    output_dir => $gc{'output_dir'}
);

$wmic_runner->collect( 'wmic.txt' );

$smartctl_runner->collect( "smart.txt" ) 
    if defined $smartctl_runner; 

my $logman_runner = LogmanRunner->new(
    raw_disk      => $gc{'raw_disk'},
    pdnum         => $gc{'target_physicaldrive'},
    volume        => $gc{'target_volume'},
    output_dir    => $gc{'output_dir'},
    keep_raw_file => $gc{'keep_logman_raw'}
) 
if $gc{'collect_logman'};
        
if( $gc{'collect_smart'} )
{
    if( $gc{'smart_supported'} )
    {
        print "Collecting SMART counters via SmartCtl.\n";
    }
    else
    {
        warn "SmartCtl missing or broken. SMART capture disabled.\n";
    }
}

my $power;

if( $gc{'collect_power'} )
{
    $power = Power->new( output_dir => $gc{'output_dir'} );

    if( $power->is_functional() )
    {
        print "Collecting system power via IPMI.\n";
    }
    else
    {
        warn "Ipmiutil missing or broken. Power measurement disabled.\n";
        undef $power;
    }
}

my $pc = Precondition->new(
    raw_disk        => $gc{'raw_disk'},
    pdnum           => $gc{'target_physicaldrive'},
    target_file     => $gc{'target_file'},
    output_dir      => $gc{'output_dir'},
    quick_test      => $gc{'quick_test'}
);

if( $gc{'initialize'} )
{
    # Write SSDs 2x, touch all the OP
    $pc->run_sequential_passes( 
        "Initializing target",
        $gc{'is_target_ssd'} ? 2 : 1
    );
}
else
{
    print "Skipping initialization as requested.\n";
}

my %iogen_args = (
    raw_disk     => $gc{'raw_disk'},
    pdnum        => $gc{'target_physicaldrive'},
    target_file  => $gc{'target_file'},
    extra_args   => $gc{'io_generator_args'},
    output_dir   => $gc{'output_dir'},
    default_comp => $gc{'compressibility'},
);

my $iogen;
$iogen = Sqlio->new( %iogen_args ) if $gc{'io_generator'} =~ 'sqlio';
$iogen = DiskSpd->new( %iogen_args ) if $gc{'io_generator'} =~ 'diskspd';

print "Testing...\n";
$recipe->run(
    preconditioner  => $pc,
    io_generator    => $iogen,
    smartctl_runner => $smartctl_runner,
    logman_runner   => $logman_runner,
    power           => $power
);
    
if( defined $gc{'results_share'} )
{
    print "Attempting upload to results share...";

    my $upload_start = time();
    my $success = upload_results();
    my $dstr = seconds_to_human( time() - $upload_start );

    print "success!" if $success;
    print "failed!" if not $success;

    print " (took $dstr)\n";
}

my $dstr = seconds_to_human( time() - $overall_start );
print( "Done (took $dstr)\n" );

exit( 0 );

sub upload_results
{
    my $src = $gc{'output_dir'};
    my $share = $gc{'results_share'};
    my $dst = "$share\\results\\$gc{'test_id'}";
    my $user = $gc{'results_share_user'};
    my $pass = $gc{'results_share_pass'}; 

    my $nu_cmd = "net use $share";
    $nu_cmd .= " $pass" if defined $pass;
    $nu_cmd .= " /USER:$user" if defined $user;

    my $failed = execute_task( $nu_cmd, quiet => 1 );
    return 0 if $failed;

    my $success = 
        execute_task(
            "ROBOCOPY /E /R:10 /W:5 $src $dst",
            quiet => 1
        );
    
    return 0 unless $success;

    $failed = execute_task( "net use $share /DELETE /Y", quiet => 1 );
    return 0 if $failed;
       
    return 1;
}

sub check_system_compatibility
{
    unless( is_windows_x64() )
    {
        warn "\n$script_name requires a 64-bit version of Windows\n\n";
        exit( -1 );
    }

    unless( is_vc_runtime_present( '12.0', 'x86' ) )
    {
        warn <<"MSG";

$script_name requires the x86 Visual C++ Redistributable Packages
for Visual Studio 2013.  Please install vcredist_x86.exe from:
    http://www.microsoft.com/en-us/download/details.aspx?id=40784

MSG
        exit( -1 );
    }

    unless( is_vc_runtime_present( '12.0', 'x64' ) )
    {
        warn <<"MSG";

$script_name requires the x64 Visual C++ Redistributable Packages
for Visual Studio 2013.  Please install vcredist_x64.exe from:
    http://www.microsoft.com/en-us/download/details.aspx?id=40784
   
MSG
        exit( -1 );
    }

    unless( Win32::IsAdminUser() )
    {
        warn "\n$script_name: must run as Administrator\n\n";
        exit( -1 );
    }
}
