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
use POSIX 'strftime';
use Carp;
#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };

use FindBin;
use lib "$FindBin::Bin";
use lib "$FindBin::Bin\\lib";

use Util;
use CommandLine;
use Target;
use PreconditionRunner;
use SmartCtlRunner;
use Recipe;
use DiskSpdRunner;
use SqlioRunner;
use LogmanRunner;
use Power;
use WmicRunner;
use SharedVariables;

check_system_compatibility();

mkdir( $results_dir ) unless( -d $results_dir );

# See SharedVariables.pm
our $cmd_line = CommandLine->new( argv => \@ARGV );
our $target = Target->new( cmd_line => $cmd_line );

my $recipe_file = $cmd_line->recipe;
    
unless( defined $recipe_file )
{
    if( $target->is_ssd )
    {
        $recipe_file = 'recipes\\turkey_test.rcp';
    }
    else
    {
        $recipe_file = 'recipes\\corners.rcp';
    }
}

my $output_dir = "$results_dir\\";
    
$output_dir .= $cmd_line->test_id_prefix
    if defined $cmd_line->test_id_prefix;

if( defined $cmd_line->test_id )
{
    $output_dir .= $cmd_line->test_id;
}
else
{
    # If no test_id was given, construct a sensible default
    my $prefix = 
        make_legal_filename( 
            get_drive_model( $target->physical_drive ) );

    $output_dir .= 
        "$prefix-" . strftime( "%Y-%m-%d_%H-%M-%S", localtime );
}
    
print "Targeting " . uc( $target->type ) . ": " . $target->model . "\n";

if( $target->is_ssd )
{
    if( $target->supports_smart and 
        $target->is_sata and
        !$target->is_6Gbps_sata )
    {
        my $msg;

        $msg .= "\n\tWarning!\n";
        $msg .= "\tSSD target is not 6Gb/s SATA III.\n";
        $msg .= "\tThroughput will be limited.\n";

        warn $msg;
    }
}

print "\n";

my $recipe = Recipe->new(
    file_name            => $recipe_file,
    output_dir           => $output_dir,
    target               => $target,
    cmd_line             => $cmd_line
);

my $num_steps = $recipe->get_num_steps();
my $num_tests = $recipe->get_num_test_steps();

die "Empty recipe. Nothing to do.\n" unless $num_steps > 0;

print "Loaded " . $recipe_file;
print " ($num_tests tests, $num_steps steps)\n\n";

$recipe->warn_expected_run_time();

detect_scep_and_warn();

if( $target->must_clean_disk ) 
{
    my $msg;

    $msg .= "\tWarning!\n";
    $msg .= "\tThis will destroy \\\\.\\PHYSICALDRIVE";
    $msg .= $target->physical_drive . "\n\n";

    warn $msg;
}
elsif( defined $target->file_name and
    ( $cmd_line->initialize or $recipe->contains_writes ) )
{
    unless( -w $target->file_name or $pretend )
    {
        die "Target is not writable\n"
    }

    my $msg;

    $msg .= "\tWarning!\n";
    $msg .= "\tThis will destroy ";
    $msg .= $target->file_name . "\n\n";

    warn $msg;
}

exit 0 unless should_proceed();

die "Results subdirectory $output_dir already exists!\n"
    if -e $output_dir;

my $overall_start = time();

mkdir( $output_dir );

if( $cmd_line->collect_smart )
{
    if( $target->supports_smart )
    {
        print "Collecting SMART counters via SmartCtl.\n";
    }
    else
    {
        warn "SmartCtl missing or broken. SMART capture disabled.\n";
    }
}

my $power;

if( $cmd_line->collect_power )
{
    $power = Power->new( output_dir => $output_dir );

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

# TODO: SECURE ERASE
#
# In the future when we support SECURE ERASE, the line below 
# should change to be conditional, like this:
#    $target->prepare() if $target->is_hdd();

$target->prepare();

my $wmic_runner = WmicRunner->new(
    target => $target,
    output_dir => $output_dir
);

$wmic_runner->collect( 'wmic.txt' );

my $smartctl_runner = undef;

if( $cmd_line->collect_smart and $target->supports_smart )
{
    $smartctl_runner = SmartCtlRunner->new(
        physical_drive => $target->physical_drive,
    );

    $smartctl_runner->collect(
        file_name => "smart.txt",
        output_dir => $output_dir,
        do_identify => 1
    ) 
}

my $logman_runner = LogmanRunner->new(
    target => $target,
    cmd_line => $cmd_line,
    output_dir => $output_dir
) 
if $cmd_line->collect_logman;
        
my %iogen_args = (
    target => $target,
    cmd_line => $cmd_line,
    output_dir => $output_dir
);

my $iogen;

$iogen = SqlioRunner->new( %iogen_args )
    if $cmd_line->io_generator =~ 'sqlio';

$iogen = DiskSpdRunner->new( %iogen_args )
    if $cmd_line->io_generator =~ 'diskspd';

print "Testing...\n";

$recipe->run(
    io_generator    => $iogen,
    smartctl_runner => $smartctl_runner,
    logman_runner   => $logman_runner,
    power           => $power
);
    
if( $cmd_line->auto_upload and defined $cmd_line->results_share )
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
    my $src = $output_dir;
    my $share = $cmd_line->results_share;
    my $dst = $output_dir =~ s/.*results/$share\\results/r;
    my $user = $cmd_line->results_share_user;
    my $pass = $cmd_line->results_share_pass;

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

    # False positive on Server 2012 R2?
    unless( is_vc_runtime_present( '12.0', 'x86' ) )
    {
        warn <<"MSG";

$script_name requires the x86 Visual C++ Redistributable Packages
for Visual Studio 2013.  Please install vcredist_x86.exe from:
    http://www.microsoft.com/en-us/download/details.aspx?id=40784

MSG
        exit( -1 );
    }

    # False positive on Server 2012 R2?
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

BEGIN
{
    # unbuffer STDERR and STDOUT
    select STDERR;
    $OUTPUT_AUTOFLUSH = 1;

    select STDOUT;
    $OUTPUT_AUTOFLUSH = 1;
}
