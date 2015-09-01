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

package PreconditionRunner;

use strict;
use warnings;
use Moose;
use Util;

has 'target' => (
    is      => 'ro',
    isa     => 'Target',
    required => 1
);

has 'cmd_line' => (
    is      => 'ro',
    isa     => 'CommandLine',
    required => 1
);

use constant NORMAL_GATHER_SECONDS => 540;
use constant NORMAL_DWELL_SECONDS => 60;
use constant NORMAL_MIN_RUN_SECONDS => 
    NORMAL_GATHER_SECONDS + NORMAL_DWELL_SECONDS;

use constant QUICK_TEST_GATHER_SECONDS => 10;
use constant QUICK_TEST_DWELL_SECONDS => 5;
use constant QUICK_TEST_MIN_RUN_SECONDS => 
    QUICK_TEST_GATHER_SECONDS + QUICK_TEST_DWELL_SECONDS;

use constant QUICK_TEST_SLOPE_TOLERANCE => 0.5;

# Future work: take an arbitrary workload here
# but override the write-percentage to 100%
sub write_num_passes
{
    my $self = shift;
    
    my %args = @_;

    my $msg_prefix = $args{'msg_prefix'};
    my $num_passes = $args{'num_passes'};
    
    # Fake progress message for pretend mode
    if( $pretend )
    {
        print $msg_prefix . "100% [xx.x MB/s]\n";
    }

    my $cmd = "precondition.exe ";

    $cmd .= "-n$num_passes ";
    $cmd .= qq(-p"$msg_prefix" );
    $cmd .= "-Y ";
    
    if( $self->cmd_line->raw_disk )
    {
        $cmd .= $self->target->physical_drive;
    }
    else
    {
        $cmd .= $self->target->file_name;
    }

    my $failed = execute_task( $cmd );

    die "Precondition returned non-zero errorlevel" if $failed;
}

sub run_to_steady_state($)
{
    my $self = shift;
    
    my %args = @_;

    my $msg_prefix = $args{'msg_prefix'};
    my $output_dir = $args{'output_dir'};
    my $test_ref = $args{'test_ref'};

    my $write_percentage = $test_ref->{'write_percentage'} // die;
    my $access_pattern   = $test_ref->{'access_pattern'} // die;
    my $block_size       = $test_ref->{'block_size'} // die;
    my $queue_depth      = $test_ref->{'queue_depth'} // die;
    my $description      = $test_ref->{'description'} // die;

    my $block_size_kB = human_to_kilobytes( $block_size );

    my $cmd = "precondition.exe ";

    $cmd .= "-Y ";
    $cmd .= "-b$block_size_kB ";
    $cmd .= "-r " if $access_pattern eq 'random';
    $cmd .= "-o$queue_depth ";
    $cmd .= "-w$write_percentage ";
    $cmd .= "-ss ";
    
    if( $self->cmd_line->demo_mode )
    {
        $cmd .= "-g" . QUICK_TEST_GATHER_SECONDS . " ";
        $cmd .= "-d" . QUICK_TEST_DWELL_SECONDS . " ";
        $cmd .= "-t" . QUICK_TEST_SLOPE_TOLERANCE . " ";
    }

    $cmd .= qq(-p"$msg_prefix" );

    if( $self->cmd_line->raw_disk )
    {
        $cmd .= $self->target->physical_drive;
    }
    else
    {
        $cmd .= $self->target->file_name;
    }

    my $out_file =
        "$output_dir\\precondition-$description.txt";
    
    open( my $OUT, ">$out_file" )
        or die "could not open $out_file: $!";
   
    # Save the command line as line 1 of the file
    print $OUT "$cmd\n";
    
    # Fake progress message for pretend mode
    if( $pretend )
    {
        print $msg_prefix . "achieved steady-state after 0 sec\n";
    }

    # Don't capture stderr so progress message goes to console
    my ( $errorlevel, $stdout ) = 
        execute_task( $cmd, capture_stderr => 0 );
   
    # Save steady-state achieved / abandoned message
    print $OUT "$stdout\n";

    close( $OUT );

    die "Precondition returned non-zero errorlevel" if $errorlevel;
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
