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

package LogmanRunner;

use strict;
use warnings;
use Moose;
use File::Temp 'mktemp';
use Util;
use sigtrap qw( die normal-signals );

has 'target' => (
    is => 'ro',
    isa => 'Target',
    required => 1
);

has 'cmd_line' => (
    is      => 'ro',
    isa     => 'CommandLine',
    required => 1
);

has 'output_dir' => (
    is => 'ro',
    isa => 'Str',
    required => 1
);

has 'description' => (
    is      => 'rw',
    isa     => 'Maybe[Str]',
    default => undef
);

# Text file containing the list of Logman counters
my $counters_filename =
    mktemp( $ENV{'TEMP'} . "\\logman_countersXXXXXX" );

sub build_csv_filename
{
    my $self = shift;
    my $prefix = shift;

    my $description = $self->description;
    my $dir = $self->output_dir;

    return "$dir\\logman-$prefix-$description.csv";
}

sub raw_filename
{
    my $self = shift;
    return $self->build_csv_filename( 'raw' );
}

sub pruned_filename
{
    my $self = shift;
    return $self->build_csv_filename( 'pruned' );
}

sub status
{
    my ($status, $out) = execute_task( "logman query" );

    foreach my $line ( split '\n', $out )
    {
        if( $line =~ /storscore_counters/ )
        {
            if( $line =~ /Running/ )
            {
                return 'RUNNING';
            }
            elsif( $line =~ /Stopped/ )
            {
                return 'STOPPED';
            }
            else
            {
                warn "Unexpected: $line";
            
                return 'ERROR';
            }
        }
    }

    return 'NOT_FOUND';
}

sub try_logman_stop
{
    my $failed = 
        execute_task(
            "logman stop storscore_counters",
            quiet => 1
        );

    warn "Error trying to stop logman.\n" if $failed;
}

sub try_logman_delete
{
    my $failed = 
        execute_task(
            "logman delete storscore_counters",
            quiet => 1
        );
    
    warn "Error trying to delete logman counters.\n" if $failed;
}

sub cleanup
{
    my $status = status();

    return if $status eq 'NOT_FOUND';

    if( $status ne 'STOPPED' )
    {
        try_logman_stop();
    }
    
    try_logman_delete();

    if( -e $counters_filename )
    {
        unlink $counters_filename
            or warn "Could not unlink file $counters_filename: $!\n";
    }
}

sub start()
{
    my $self = shift;

    cleanup();
  
    create_counters_file();

    my $cmd = "logman create counter ";
    $cmd .= "storscore_counters "; # name of StorScore data collector
    $cmd .= "--v "; # be quiet
    $cmd .= "-si 1 "; # collection interval = 1 second
    $cmd .= "-cf $counters_filename ";
    $cmd .= "-f csv ";
    $cmd .= "-o " . $self->raw_filename;

    execute_task( $cmd, quiet => 1 );
    execute_task( "logman start storscore_counters", quiet => 1 );
}

# Get the transpose of the input array
sub transpose(@)
{
    my @input = @_;
    
    my @output;

    foreach my $row_ref ( @input )
    {
        my @row = @$row_ref;

        my $col_num = 0;

        foreach my $val ( @row )
        {
            push @{ $output[$col_num++] }, $val;
        }
    }

    return @output;
}

# Check if the given counter matches the collection of desired counters
sub is_counter_relevant($)
{
    my $self = shift;
    my $name = shift;

    if( ( $name =~ /Processor Time/ ) and
        ( $name =~ /_Total/ ) )
    {
        return 1;
    }

    if( $name =~ /Avg\. Disk Queue Length/ )
    {
        if( $self->cmd_line->raw_disk )
        {
            if( $name =~ /PhysicalDisk\((\d+)/ ) 
            {      
                return 1 if $1 == $self->target->physical_drive;
            }
        }
        else
        {
            if( $name =~ /LogicalDisk\((.+)\)/ ) 
            {
                my $counter_vol = $1;
                my $target_vol = $self->target->volume;
                return 1 if $counter_vol =~ m/$target_vol/i;
            }
        }
    }

    if( $name =~ /Memory/ )
    {
        if ( ( $name =~ /System Driver Total Bytes/ ) or
             ( $name =~ /System Driver Resident Bytes/ ) )
         {
             return 1;
         }
    }

    return 0;
}

sub stop()
{
    my $self = shift;

    cleanup();

    return if $pretend; 

    my @raw_csv = transpose( read_csv( $self->raw_filename ) );

    my @output_rows;

    foreach my $row_aref ( @raw_csv )
    {
        my @row = @$row_aref;

        my $counter_name = $row[0];
        
        if( $self->is_counter_relevant( $counter_name ) )
        {
            push @output_rows, $row_aref;
        }
    }
    
    write_csv( $self->pruned_filename, @output_rows );

    unlink $self->raw_filename unless $self->cmd_line->keep_logman_raw;
}

sub create_counters_file()
{
    return if $pretend;

    open(my $fh, '>', $counters_filename)
        or die "Could not open file '$counters_filename' $!";

    print $fh <<'COUNTERS';
\Memory\Page Faults/sec
\Memory\Available Bytes
\Memory\Committed Bytes
\Memory\Commit Limit
\Memory\Write Copies/sec
\Memory\Transition Faults/sec
\Memory\Cache Faults/sec
\Memory\Demand Zero Faults/sec
\Memory\Pages/sec
\Memory\Pages Input/sec
\Memory\Page Reads/sec
\Memory\Pages Output/sec
\Memory\Pool Paged Bytes
\Memory\Pool Nonpaged Bytes
\Memory\Page Writes/sec
\Memory\Pool Paged Allocs
\Memory\Pool Nonpaged Allocs
\Memory\Free System Page Table Entries
\Memory\Cache Bytes
\Memory\Cache Bytes Peak
\Memory\Pool Paged Resident Bytes
\Memory\System Code Total Bytes
\Memory\System Code Resident Bytes
\Memory\System Driver Total Bytes
\Memory\System Driver Resident Bytes
\Memory\System Cache Resident Bytes
\Memory\% Committed Bytes In Use
\Memory\Available KBytes
\Memory\Available MBytes
\Memory\Transition Pages RePurposed/sec
\Memory\Free & Zero Page List Bytes
\Memory\Modified Page List Bytes
\Memory\Standby Cache Reserve Bytes
\Memory\Standby Cache Normal Priority Bytes
\Memory\Standby Cache Core Bytes
\Processor(*)\% Processor Time
\Processor(*)\% User Time
\Processor(*)\% Privileged Time
\Processor(*)\Interrupts/sec
\Processor(*)\% DPC Time
\Processor(*)\% Interrupt Time
\Processor(*)\DPCs Queued/sec
\Processor(*)\DPC Rate
\Processor(*)\% Idle Time
\Processor(*)\% C1 Time
\Processor(*)\% C2 Time
\Processor(*)\% C3 Time
\Processor(*)\C1 Transitions/sec
\Processor(*)\C2 Transitions/sec
\Processor(*)\C3 Transitions/sec
\Physicaldisk(*)\Current Disk Queue Length
\Physicaldisk(*)\% Disk Time
\Physicaldisk(*)\Avg. Disk Queue Length
\Physicaldisk(*)\% Disk Write Time
\Physicaldisk(*)\Avg. Disk sec/Read
\Physicaldisk(*)\Avg. Disk sec/Write
\Physicaldisk(*)\Disk Reads/sec
\Physicaldisk(*)\Disk Writes/sec
\Physicaldisk(*)\Disk Read Bytes/sec
\Physicaldisk(*)\Disk Write Bytes/sec
\Physicaldisk(*)\% Idle Time
\LogicalDisk(*)\% Disk Read Time
\LogicalDisk(*)\% Disk Time
\LogicalDisk(*)\% Disk Write Time
\LogicalDisk(*)\% Free Space
\LogicalDisk(*)\% Idle Time
\LogicalDisk(*)\Avg. Disk Bytes/Read
\LogicalDisk(*)\Avg. Disk Bytes/Transfer
\LogicalDisk(*)\Avg. Disk Bytes/Write
\LogicalDisk(*)\Avg. Disk Queue Length
\LogicalDisk(*)\Avg. Disk Read Queue Length
\LogicalDisk(*)\Avg. Disk sec/Read
\LogicalDisk(*)\Avg. Disk sec/Transfer
\LogicalDisk(*)\Avg. Disk sec/Write
\LogicalDisk(*)\Avg. Disk Write Queue Length
\LogicalDisk(*)\Current Disk Queue Length
\LogicalDisk(*)\Disk Read Bytes/sec
\LogicalDisk(*)\Disk Reads/sec
\LogicalDisk(*)\Disk Transfers/sec
\LogicalDisk(*)\Disk Write Bytes/sec
\LogicalDisk(*)\Disk Bytes/sec
\LogicalDisk(*)\Free Megabytes
\LogicalDisk(*)\Split IO/Sec
\System\File Read Operations/sec
\System\File Write Operations/sec
\System\File Control Operations/sec
\System\File Read Bytes/sec
\System\File Write Bytes/sec
\System\File Control Bytes/sec
\System\Context Switches/sec
\System\System Calls/sec
\System\File Data Operations/sec
\System\System Up Time
\System\Processor Queue Length
\System\Processes
\System\Threads
\System\Alignment Fixups/sec
\System\Exception Dispatches/sec
\System\Floating Emulations/sec
\System\% Registry Quota In Use
COUNTERS

    close $fh;
}

END
{
    cleanup();
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
