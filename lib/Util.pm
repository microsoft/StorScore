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

package Util;

use strict;
use warnings;

use English;
use File::Basename;
use File::Temp 'mktemp';
use File::stat;
use Time::localtime;
use List::Util 'sum';
use POSIX qw(strftime ceil);
use Term::ReadKey;
use Time::Seconds;
use Win32;
use Win32::API;
use Win32::Process;
use Encode;

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

use Exporter;
use vars qw(@ISA @EXPORT);

@ISA = qw(Exporter);
@EXPORT = qw(
    BYTES_PER_GB_BASE2
    BYTES_PER_MB_BASE2
    BYTES_PER_KB_BASE2
    BYTES_PER_GB_BASE10
    BYTES_PER_MB_BASE10
    BYTES_PER_KB_BASE10
    BYTES_PER_SECTOR
    BYTES_IN_2MB
    $TEST_FILE_NAME
    $script_name
    $script_dir
    $recipes_dir
    $results_dir
    $entropy_dir
    $devices_dir
    $verbose
    $pretend
    $prompt
    should_proceed
    list_physical_drives
    list_volumes
    wmic_helper
    physical_drive_exists
    get_drive_size
    get_volume_size
    get_volume_free_space
    get_drive_model
    get_drive_serialnumber
    bytes_to_human
    bytes_to_human_base2
    bytes_to_human_base10
    human_to_bytes
    human_to_kilobytes
    by_human
    fast_create_file
    secure_erase
    clean_disk
    create_filesystem
    is_power_of_two
    is_process_running
    detect_scep_and_warn
    volume_to_partition
    partition_to_physical_drive
    volume_to_physical_drive
    physical_drive_to_partition
    partition_to_volume
    physical_drive_to_volume
    seconds_to_human
    ltrim
    rtrim
    primary_dns_suffix
    make_legal_filename
    num_physical_cores
    num_logical_cores
    hyperthreading_enabled
    do_simple_extract
    slurp_file
    median
    mean
    stddev
    execute_task
    kill_task
    is_windows_x64
    is_registry_key_present
    is_vc_runtime_present
    read_csv
    write_csv
    is_absolute_path
    get_file_modified_time
    log_base2
    round_up_power2
    unix_date_to_excel_date
);

use constant BYTES_PER_GB_BASE2 => 1024 * 1024 * 1024;
use constant BYTES_PER_MB_BASE2 => 1024 * 1024;
use constant BYTES_PER_KB_BASE2 => 1024;

use constant BYTES_PER_GB_BASE10 => 1000 * 1000 * 1000;
use constant BYTES_PER_MB_BASE10 => 1000 * 1000;
use constant BYTES_PER_KB_BASE10 => 1000;

use constant BYTES_PER_SECTOR => 512;
use constant BYTES_IN_2MB => 1024 * 1024 * 2;

our $TEST_FILE_NAME = 'testfile.dat';

our $script_name = basename( $PROGRAM_NAME, ".cmd" );
our $script_dir = dirname( $PROGRAM_NAME );
our $recipes_dir = "$script_dir\\recipes";
our $results_dir = "$script_dir\\results";
our $entropy_dir = "$script_dir\\entropy";
our $devices_dir = "$script_dir\\lib\\DeviceDB";

our $verbose = 0;
our $pretend = 0;
our $prompt = 1;

sub should_proceed(;$)
{
    return 1 unless $prompt;

    my $msg = shift // "Do you wish to continue?";

    print "$msg [Y/N]";

    ReadMode( 'cbreak' );
    my $key = ReadKey(0); 
    ReadMode( 'restore' );

    print "\n\n";

    return 1 if( $key =~ /Y/i );
    return 0;
}

sub list_physical_drives()
{
    execute_task( 'wmic path Win32_DiskDrive get Model, Name' );
}

sub list_volumes()
{
    my $cmd = 'wmic path Win32_LogicalDisk';
    $cmd .= ' where Description="Local Fixed Disk"';
    $cmd .= ' get Name';

    execute_task( $cmd );
}

sub unicode_to_ascii($)
{
    my $str = shift;

    $str = decode( 'utf16', $str );
    $str =~ s/\r//gm;

    return $str;
}

sub wmic_helper($)
{
    my $wmic_cmd = shift;

    my ( $errorlevel, $stdout ) = execute_task( "wmic $wmic_cmd" );

    return "" if $pretend;

    die "wmic failed" if $errorlevel != 0;

    return unicode_to_ascii( $stdout );
}

sub physical_drive_exists
{
    my $pdnum = shift;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";

    my $wmic_cmd =
        qq(path Win32_DiskDrive where Name="$pdname");

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
    
    return 1 if $pretend; # Pretend drives always exist :)

    return 0 if $wmic_lines[0] =~ /No Instance/;

    return 1;
}

sub get_volume_size($)
{
    my $vol = shift;
    
    my $wmic_cmd = 
        qq(path Win32_LogicalDisk where Name="$vol" get Size);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
    
    # Report "pretend volumes" as having 512 GB size
    return 512 * BYTES_PER_GB_BASE10 if $pretend;
 
    return $wmic_lines[1];
}

sub get_volume_free_space($)
{
    my $vol = shift;
    
    my $wmic_cmd = 
        qq(path Win32_LogicalDisk where Name="$vol" get FreeSpace);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
    
    # Report "pretend volumes" as having 512 GB free
    return 512 * BYTES_PER_GB_BASE10 if $pretend;
    
    return $wmic_lines[1];
}

sub get_drive_size($)
{
    my $pdnum = shift;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";
    
    my $wmic_cmd = 
        qq(path Win32_DiskDrive where Name="$pdname" get Size);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
 
    return $wmic_lines[1];
}

sub get_drive_model($)
{
    my $pdnum = shift;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";
    
    my $wmic_cmd = 
        qq(path Win32_DiskDrive where Name="$pdname" get Model);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
 
    return 'StorScore Pretend Drive' if $pretend;

    return $wmic_lines[1];
}

sub get_drive_serialnumber($)
{
    my $pdnum = shift;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";
    
    my $wmic_cmd = 
        qq(path Win32_DiskDrive where Name="$pdname" get SerialNumber);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );
 
    return $wmic_lines[1];
}

sub bytes_to_human_internal($$)
{
    my $bytes = shift;
    my $divisor = shift;

    return "${bytes} B" unless $bytes >= $divisor;

    $bytes = int( $bytes / $divisor );
    return "${bytes} KB" unless $bytes >= $divisor;

    $bytes = int( $bytes / $divisor );
    return "${bytes} MB" unless $bytes >= $divisor;
    
    $bytes = int( $bytes / $divisor );
    return "${bytes} GB";
}

sub bytes_to_human_base2($)
{
    return bytes_to_human_internal( shift, 1024 );
}

sub bytes_to_human_base10($)
{
    return bytes_to_human_internal( shift, 1000 );
}

sub bytes_to_human($) # base 2 by default
{
    return bytes_to_human_base2( shift );
}

# TO DO: add a base 10 version similar to above
sub human_to_bytes($)
{
    my $human = shift;

    return ( $human << 30 ) if $human =~ s/(\d+)\s*GB?$/$1/i;
    return ( $human << 20 ) if $human =~ s/(\d+)\s*MB?$/$1/i;
    return ( $human << 10 ) if $human =~ s/(\d+)\s*KB?$/$1/i;
    return $human if $human =~ s/(\d+)\s*B?$/$1/i;
    
    die "Cannot parse the human-readable value";
}

sub human_to_kilobytes($)
{
    return human_to_bytes( shift ) >> 10;
}

# for use with sort
sub by_human($$)
{
    my $a = shift;
    my $b = shift;

    return human_to_bytes($a) <=> human_to_bytes($b);
}

sub fast_create_file($$)
{
    my $file = shift;
    my $size = shift;

    # NTFS has two distinct concepts: length, and validdatalength (VDL).
    # 
    # The "createnew" sets length, but VDL will still be 0.  At this point, 
    # a write to the end of the file would take an extremely long time as 
    # it would trigger NTFS to zero fill the entire file.
    # 
    # Calling "setvaliddata" overrides the VDL, effectively disabling the
    # zero fill behavior.  Note that because "setvaliddata" also makes it
    # possible to read stale deallocated data, this requires admin rights. 

    my $create_failed = 
        execute_task
        (
            "fsutil file createnew $file $size",
            quiet => 1
        );

    return 0 if $create_failed;

    my $set_vdl_failed = 
        execute_task
        (
            "fsutil file setvaliddata $file $size",
            quiet => 1
        );

    if( $set_vdl_failed )
    {
        my $msg;

        $msg .= "\n\tWarning!\n";
        $msg .= "\tSet valid data length failed. Not an NTFS volume?\n";

        warn $msg;
    }

    return 1;
}

sub do_diskpart($)
{
    my $script = shift;
    
    my $script_file = mktemp( $ENV{'TEMP'} . "\\diskpartXXXXXX" );
    
    open( my $FILE, ">$script_file" )
        or die "Couldn't open $script_file for output: $!";
    
    print $FILE $script;
    
    close $FILE;

    my $failed = 
        execute_task( "diskpart /s $script_file", quiet => 1 );
   
    die "Diskpart failed!\n" if $failed;

    unlink $script_file or warn "Could not unlink $script_file: $!";
}

sub secure_erase($)
{
    my $pdnum = shift;

    my $cmd = "";
    $cmd .= "StorageTool.exe ";
    $cmd .= "-SecureErase ";
    $cmd .= "Disk " . $pdnum . " ";
    
    return execute_task( $cmd, quiet => 1 );
}

sub clean_disk($)
{
    my $pdnum = shift;
    
    my $dp_script = <<"END";
SELECT DISK $pdnum
CLEAN
END
    
    do_diskpart( $dp_script );
}

sub create_filesystem($;$)
{
    my $pdnum = shift;
    my $size_bytes = shift;
    
    my $dp_script = "SELECT DISK $pdnum\n";

    # Partition is aligned to 1MB.  Overkill?
    if( defined $size_bytes )
    {
        my $size_mb = int( $size_bytes / 1024 / 1024 );

        $dp_script .= 
            "CREATE PARTITION PRIMARY SIZE=$size_mb ALIGN=1024\n";
    }
    else
    {
        # Take the default size (largest possible)
        $dp_script .= 
            "CREATE PARTITION PRIMARY ALIGN=1024\n";
    }

    $dp_script .= <<"END";
FORMAT FS=NTFS LABEL="StorScore Test Drive" QUICK
ASSIGN
END

    do_diskpart( $dp_script );
}

sub is_power_of_two($)
{
    my $x = shift;
    
    return 0 if $x == 0;

    return ( $x & ( $x - 1 ) ) == 0;
}

sub is_process_running($)
{
    my $exe_name = shift;
    
    my $cmd = qq{tasklist /FI "IMAGENAME eq $exe_name"};

    my ($status, $out) = execute_task( $cmd, force => 1 );

    foreach my $line ( split '\n', $out )
    {
        if( $line =~ /$exe_name/ )
        {
            return 1;
        }
    }

    return 0;
}

sub detect_scep_and_warn()
{
    # ISSUE-REVIEW: this is only one example of something
    # that could get in the storage stack via a filter driver.
    # Is there a more generic way to check for this?

    if( is_process_running( 'MsMpEng.exe' ) )
    {   
        warn <<"WARNING";
        Warning!
        Detected System Center Endpoint Protection (MsMpEng.exe) 
        This can delay IOs and cause bogus latency results.
        You have the following options:
            - Run on a machine without SCEP
            - Disable SCEP real-time protection
            - Exclude $TEST_FILE_NAME from SCEP scan.

WARNING
    }
}

sub volume_to_partition($)
{
    my $vol = shift;

    my $wmic_cmd;
    
    $wmic_cmd .= qq(path Win32_LogicalDisk );
    $wmic_cmd .= qq(where DeviceID="$vol" );
    $wmic_cmd .= qq(assoc /assocclass:Win32_LogicalDiskToPartition);
    
    my $wmic_out = ( split /\n/, wmic_helper( $wmic_cmd ) )[2];
    
    return 'Disk #42, Partition #42' if $pretend;

    $wmic_out =~ /Win32_DiskPartition\.DeviceID=\"([^\"]*)\"/;

    return $1;
}

# ISSUE-REVIEW: what about partitions which span multiple physical_drives?
sub partition_to_physical_drive($)
{
    my $partition = shift;

    my $wmic_cmd;
    
    $wmic_cmd .= qq(path Win32_DiskPartition );
    $wmic_cmd .= qq/where (DeviceID="$partition") /;
    $wmic_cmd .= qq(assoc /assocclass:Win32_DiskDriveToDiskPartition);

    my $wmic_out = ( split /\n/, wmic_helper( $wmic_cmd ) )[2];
    
    return '\\\\\\\\.\\\\PHYSICALDRIVE42' if $pretend;

    $wmic_out =~ /Win32_DiskDrive\.DeviceID=\"([^\"]*)\"/;

    return $1;
}

sub volume_to_physical_drive($)
{
    my $vol = shift;
    
    my $partition = volume_to_partition( $vol );

    return partition_to_physical_drive( $partition );
}

# ISSUE-REVIEW: what about physical_drives that have multiple partitions?
sub physical_drive_to_partition($)
{
    my $pdnum = shift;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";

    my $wmic_cmd;
    
    $wmic_cmd .= qq(path Win32_DiskDrive );
    $wmic_cmd .= qq(where DeviceID="$pdname" );
    $wmic_cmd .= qq(assoc /assocclass:Win32_DiskDriveToDiskPartition);

    my $wmic_out = ( split /\n/, wmic_helper( $wmic_cmd ) )[2];
    
    return 'Disk #42, Partition #42' if $pretend;
    
    $wmic_out =~ /Win32_DiskPartition\.DeviceID=\"([^\"]*)\"/;

    return $1;
}

sub partition_to_volume($)
{
    my $partition = shift;

    my $wmic_cmd;
    
    $wmic_cmd .= qq(path Win32_DiskPartition );
    $wmic_cmd .= qq/where (DeviceID="$partition") /;
    $wmic_cmd .= qq(assoc /assocclass:Win32_LogicalDiskToPartition);
    
    my $wmic_out = ( split /\n/, wmic_helper( $wmic_cmd ) )[2];

    return 'P:' if $pretend;

    $wmic_out =~ /Win32_LogicalDisk\.DeviceID=\"([^\"]*)\"/;

    return $1;
}

sub physical_drive_to_volume($)
{
    my $pdnum = shift;
    
    my $partition = physical_drive_to_partition( $pdnum );

    return partition_to_volume( $partition );
}

sub seconds_to_human($)
{
    my $ts = Time::Seconds->new( shift );
    my $human;

    if( $ts->days > 1 )
    {
        $human = sprintf( "%.2f days", $ts->days );
    }
    elsif( $ts->hours > 1 )
    {
        $human = sprintf( "%.2f hours", $ts->hours );
    }
    elsif( $ts->minutes > 1 )
    {
        $human = sprintf( "%.2f minutes", $ts->minutes );
    }
    else
    {
        $human = sprintf( "%d seconds", $ts->seconds );
    }

    return $human;
}

sub ltrim(\$)
{
    my $x = shift;
    $$x =~ s/^\s+//;
}

sub rtrim(\$)
{
    my $x = shift;
    $$x =~ s/\s+$//;
}

sub primary_dns_suffix()
{
    my ( $errorlevel, $stdout ) = execute_task( 'ipconfig /all' );
    
    die "ipconfig failed" if $errorlevel != 0;
   
    my @ipconfig_lines = split /\n/, $stdout;

    my $primary_dns_suffix = "Unknown";

    foreach my $line ( @ipconfig_lines )
    {
        if( $line =~ /Primary Dns Suffix/ )
        {
            $primary_dns_suffix = ( split /:/, $line )[1];
        }
    }

    return $primary_dns_suffix;
}

sub make_legal_filename($)
{
    my $str = shift;

    # trim whitespace
    $str =~ s/\s+$//;
    $str =~ s/\s+/_/g;

    # remove illegal chars
    $str =~ s([<>:"/\\|?*"])()g;

    return $str;
}

sub num_physical_cores()
{
    my $wmic_cmd =
        qq(path Win32_Processor get NumberOfCores);
    
    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );

    shift @wmic_lines; # ditch header

    return sum( @wmic_lines );
}

sub num_logical_cores()
{
    my $wmic_cmd =
    qq(path Win32_Processor get NumberOfLogicalProcessors);

    my @wmic_lines = split /\n/, wmic_helper( $wmic_cmd );

    shift @wmic_lines; # ditch header

    return sum( @wmic_lines );
}

sub hyperthreading_enabled()
{
    if( num_physical_cores() == num_logical_cores() )
    {
        return 0;
    }

    return 1;
}

sub do_simple_extract($$@)
{
    my $line = shift;
    my $href = shift;
    my @rules = @{shift @_};
    my %args = @_;

    my $suffix = $args{'suffix'} // "";

    foreach my $rule ( @rules )
    {
        my $match = $rule->{'match'};
        my $store = $rule->{'store'};

        if( ref $store eq 'ARRAY' )
        {
            my @matches = $line =~ /$match/;
            my $num_matches = scalar @matches;

            if( $num_matches > 0 )
            {
                my @vars = @$store;
                my $num_vars = scalar @vars;

                $num_vars == $num_matches
                    or die "Matches and stores mismatch!"; 

                for( my $i = 0; $i < $num_vars; $i++ )
                {
                    $href->{"$vars[$i]$suffix"} = $matches[$i];
                }
            }
        }
        else
        {
            if( $line =~ /$match/ )
            {
                $href->{"$store$suffix"} = $1;
            }
        }
    }
}

# reads a whole file and returns the content as a string
sub slurp_file($)
{
    my $file_name = shift;
    
    local $INPUT_RECORD_SEPARATOR = undef;
    
    open my $FH, '<', $file_name
        or die "error opening $file_name: $!\n";

    my $str = <$FH>;

    close $FH;

    return $str;
}

sub median(@)
{
    my @sorted_values = sort @_;
    
    my $num_vals = scalar @sorted_values;
    
    return $sorted_values[ int( $num_vals/2 ) ];
}

sub mean(@)
{
    my @values = @_;

    my $num_vals = scalar @values;

    $num_vals > 0 
        or die "Can't compute the mean of an empty list\n";

    my $sum = sum( @values );

    my $mean = $sum / $num_vals;

    return $mean;
}

sub stddev(@)
{
    my @values = @_;

    my $num_vals = scalar @values;

    my $mean = mean( @values );

    # array of squared differences from the mean
    my @sq_diff_mean = map { ( $_ - $mean ) ** 2 } @values;

    # average of the squared differences from the mean
    my $variance = sum( @sq_diff_mean ) / $num_vals;

    my $std_dev = sqrt( $variance );

    return $std_dev; 
}


sub execute_task($;@)
{
    my $cmd = shift;
    my %args = @_;

    my $quiet      = $args{'quiet'};
    my $force      = $args{'force'};
    my $cwd        = $args{'cwd'} // '.';
    my $background = $args{'background'};
    my $new_window = $args{'new_window'};

    # In scalar context: default to returning just errorlevel
    # In list context: default to additionally returning stdout & stderr
    my $capture_stdout = $args{'capture_stdout'} // wantarray;
    my $capture_stderr = $args{'capture_stderr'} // wantarray;

    die "Unsupported"
        if( $capture_stdout or $capture_stderr ) and $background;

    warn "\n\t>>> $cmd\n" if $verbose;
    
    # force runs the cmd even if pretend flag is set
    if( $pretend and not $force )
    {
        return wantarray ? (0, "", "") : 0;
    }
   
    my $appname = $ENV{'COMSPEC'};
    my $cmdline = "/c $cmd";
    my $errorlevel = 0;

    my $stdout_file;
    my $stderr_file;

    if( $quiet )
    {
        $cmdline .= " 1>NUL 2>&1";
    }
    else
    {
        if( $capture_stdout )
        {
            $stdout_file = mktemp( $ENV{'TEMP'} . "\\stdoutXXXXXX" );
            $cmdline .= " 1>$stdout_file";
        }
        
        if( $capture_stderr )
        {
            $stderr_file = mktemp( $ENV{'TEMP'} . "\\stderrXXXXXX" );
            $cmdline .= " 2>$stderr_file";
        }
    }

    my $flags = NORMAL_PRIORITY_CLASS;
    $flags |= CREATE_NEW_CONSOLE if $new_window;

    Win32::Process::Create(
        my $proc,
        $appname,
        $cmdline,
        0,
        $flags,
        $cwd
    ) or die Win32::FormatMessage( Win32::GetLastError() );

    unless( $background )
    {
        $proc->Wait( INFINITE );
        $proc->GetExitCode( $errorlevel );
    }

    if( $capture_stdout or $capture_stderr )
    {
        my @return_array = ( $errorlevel );
    
        if( $capture_stdout )
        {
            push @return_array, slurp_file( $stdout_file );
            unlink( $stdout_file );
        }

        if( $capture_stderr )
        {
            push @return_array, slurp_file( $stderr_file );
            unlink( $stderr_file );
        }

        return @return_array;
    }

    return $background ? $proc->GetProcessID() : $errorlevel;
}

sub kill_task($)
{
    my $arg = shift;
   
    # Argument can be either a PID or an image name
    my $pid = $arg if $arg =~ /^\d+$/;
    
    my $cmd = "TASKKILL /F /T ";
    $cmd .= $pid ? "/PID $arg" : "/IM $arg";

    execute_task( $cmd, quiet => 1 );
}

sub is_windows_x64()
{
    return 1 if Win32::GetOSDisplayName() =~ /64-bit/;
    return 0;
}

sub is_registry_key_present($)
{
    my $key_name = shift;

    my $missing = 
        execute_task( "reg query $key_name ", quiet => 1 );

    return not $missing; 
}

sub is_vc_runtime_present($$)
{
    my $version = shift;
    my $machine = shift;

    $version ~~ [qw( 12.0 )] or die "Unimplemented";
    $machine ~~ [qw( x86 x64 )] or die "Unimplemented";

    my $reg_key = "HKLM\\Software";

    $reg_key .= "\\Wow6432Node" if is_windows_x64();

    $reg_key .=
        "\\Microsoft\\VisualStudio\\$version\\VC\\Runtimes\\$machine";

    return is_registry_key_present( $reg_key );
}

sub read_csv($)
{
    my $filename = shift;

    open( my $fh, '<', $filename )
        or die "Could not open file '$filename' $!\n";
   
    my @csv;

    while( my $line = <$fh> )
    {
        $line =~ s/"//g; # remove quotes
        chomp $line;
     
        push @csv, [ split /,/, $line ];
    }
    
    close $fh or die "Could not close file '$filename' $!";

    return @csv;
}

sub write_csv($@)
{
    my $filename = shift;
    my @csv = @_;

    open( my $fh, ">$filename" )
        or die "Could not open file '$filename' $!\n";
    
    foreach my $aref ( @csv )
    {
        print $fh join( ',', @$aref ) . "\n";
    }

    close $fh or die "Could not close file '$filename' $!";
}

sub is_absolute_path($)
{
    my $path = shift;

    return 1 if $path =~ /^[a-zA-z]:/;
    return 0;
}

sub get_file_modified_time($)
{
    my $filename = shift;
    
    open( my $fh, "<$filename" )
        or die "Could not open file '$filename' $!\n";

    my $mtime = ctime( stat($fh)->mtime );

    close $fh or die "Could not close file '$filename' $!";

    return $mtime;
}

sub log_base2($)
{
    my $val = shift;

    return log( $val ) / log( 2 );
}

sub round_up_power2($)
{
    my $val = shift;

    return 2 ** ceil( log_base2( $val ) );
}

sub unix_date_to_excel_date($)
{
    my $unix_date = shift;
    
    # Input: integer seconds since the Unix epoch, 1/1/1970
    # Output: floating-point days since the Excel epoch, 1/1/1900
    
    # Number of days between 1/1/1970 and 1/1/1900
    use constant EPOCH_SHIFT_DAYS => 25569;

    use constant SECONDS_PER_DAY => 86400;

    return ( $unix_date / SECONDS_PER_DAY ) + EPOCH_SHIFT_DAYS;
}

1;
