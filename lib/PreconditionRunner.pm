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

has 'raw_disk' => (
    is       => 'ro',
    isa      => 'Bool',
    required => 1
);

has 'pdnum' => (
    is  => 'ro',
    isa => 'Str'
);

has 'target_file' => (
    is      => 'ro',
    isa     => 'Maybe[Str]',
    default => undef
);

has 'output_dir' => (
    is       => 'ro',
    isa      => 'Str',
    required => 1
);

has 'quick_test' => (
    is       => 'ro',
    isa      => 'Maybe[Bool]',
    default  => 0
);

has 'is_target_ssd' => (
    is      => 'ro',
    isa     => 'Bool',
    default => undef
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

sub initialize()
{
    my $self = shift;
    
    my $target = $self->raw_disk ? $self->pdnum : $self->target_file;
      
    my $num_passes = 1;
    
    if( $self->is_target_ssd and not $self->quick_test )
    {
        my $file_size = -s $self->target_file;
        my $pd_size = get_drive_size( $self->pdnum );

        # We want to dirty all of the NAND, including the OP
        # to avoid measuring the fresh-out-of-the-box condition.
        # 
        # Writing the drive 2x is overkill, but we do it only once.
        #
        # Note that in cases where the file is much smaller than
        # the drive, we will need to write the file many times in
        # order to write the drive once.
        $num_passes = int( 2 * ( $pd_size / $file_size ) );
    }

    my $cmd = "precondition.exe ";

    $cmd .= "-n$num_passes ";
    $cmd .= q(-p"Initializing target: " );
    $cmd .= "-Y ";
    $cmd .= $target;

    my $failed = execute_task( $cmd );

    die "Precondition returned non-zero errorlevel" if $failed;
}

sub run_to_steady_state($)
{
    my $self = shift;

    my $msg_prefix = shift;
    my $test_ref = shift;

    my $write_percentage = $test_ref->{'write_percentage'};
    my $access_pattern   = $test_ref->{'access_pattern'};
    my $block_size       = $test_ref->{'block_size'};
    my $queue_depth      = $test_ref->{'queue_depth'};
    
    my $target = $self->raw_disk ? $self->pdnum : $self->target_file;

    my $block_size_kB = human_to_kilobytes( $block_size );

    my $cmd = "precondition.exe ";

    $cmd .= "-Y ";
    $cmd .= "-b$block_size_kB ";
    $cmd .= "-r " if $access_pattern eq 'random';
    $cmd .= "-o$queue_depth ";
    $cmd .= "-w$write_percentage ";
    $cmd .= "-ss ";
    
    if( $self->quick_test )
    {
        $cmd .= "-g" . QUICK_TEST_GATHER_SECONDS . " ";
        $cmd .= "-d" . QUICK_TEST_DWELL_SECONDS . " ";
        $cmd .= "-t" . QUICK_TEST_SLOPE_TOLERANCE . " ";
    }

    $cmd .= qq(-p"$msg_prefix" );

    $cmd .= $target;

    my $out_file = $self->output_dir . 
        "\\precondition-$test_ref->{'name_string'}.txt";
    
    open( my $OUT, ">$out_file" )
        or die "could not open $out_file: $!";
   
    # Save the command line as line 1 of the file
    print $OUT "$cmd\n";

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
