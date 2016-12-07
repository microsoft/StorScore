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

package SqlioRunner;

use strict;
use warnings;
use Moose;
use Util;
use Compressibility;

with 'IOGenerator';

has 'output_dir' => (
    is => 'ro',
    isa => 'Str',
    required => 1
);

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

sub get_affinity_mask()
{
    # Under HT, use only physical processors (every other logical processor)
    my $mask = hyperthreading_enabled() ? 0x55555555 : 0xFFFFFFFF;

    # Mask away processors that don't exist
    $mask &= ( 1 << num_logical_cores() ) - 1;

    return sprintf( '0x%X', $mask );
}

my $affinity_mask = get_affinity_mask();

sub run($$)
{
    my $self = shift;

    my $test_ref = shift;
    my $run_type = shift;
    
    my $write_percentage = $test_ref->{'write_percentage'};
    my $read_percentage  = $test_ref->{'read_percentage'};
    my $access_pattern   = $test_ref->{'access_pattern'};
    my $block_size       = $test_ref->{'block_size'};
    my $queue_depth      = $test_ref->{'queue_depth'};
    my $run_time         = $test_ref->{'run_time'};
    my $compressibility  = $test_ref->{'compressibility'};

    if( $run_type eq 'warmup' )
    {
        $run_time = $test_ref->{'warmup_time'};
    }

    # Gradually increase the number of threads as queue_depth
    # increases in order to prevent CPU limitation.
    my $num_threads = 1;

    if( $queue_depth >= 16 )
    {
        $num_threads = 4;
    }
    elsif( $queue_depth >= 4 )
    {
        $num_threads = 2;
    }
   
    my $ios_per_thread = $queue_depth / $num_threads; 

    my $block_size_kB = human_to_kilobytes( $block_size );

    my $cmd = "sqlio2.exe ";

    $cmd .= "-T$read_percentage "; 
    $cmd .= "-f$access_pattern ";
    $cmd .= "-b$block_size_kB ";
    $cmd .= "-t$num_threads ";  
    $cmd .= "-o$ios_per_thread ";  

    # ISSUE-REVIEW: consider using -aRI here instead of -aR
    $cmd .= "-aR$affinity_mask ";

    $cmd .= "-LS ";
    $cmd .= "-s$run_time ";

    # Use default unless compressibility is specified by the test
    my $entropy_file = 
        Compressibility::get_filename( 
            $compressibility // $self->cmd_line->compressibility
        );

    $cmd .= qq(-q"$entropy_file" );
    
    # All-purpose escape hatch.  Support arbitrary args.
    $cmd .= " " . $self->cmd_line->io_generator_args . " "
        if defined $self->cmd_line->io_generator_args;

    if( $self->cmd_line->raw_disk )
    {
        $cmd .= "-R" . $self->target->physical_drive;
    }
    else
    {
        $cmd .= $self->target->file_name; 
    }

    my $out_file = $self->output_dir . 
        "\\$run_type-$test_ref->{'description'}.txt";

    open( my $OUT, ">$out_file" )
        or die "could not open $out_file: $!";
   
    # Save the command line as line 1 of the file
    print $OUT "$cmd\n";

    my ( $errorlevel, $stdout, $stderr ) = execute_task( $cmd );
   
    print $OUT "$stdout\n";
    print $OUT "$stderr\n";

    close( $OUT );

    print STDERR "\n\tSqlio2 returned non-zero errorlevel" if $errorlevel;
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
