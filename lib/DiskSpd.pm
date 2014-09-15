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

package DiskSpd;

use strict;
use warnings;
use Moose;
use Util;

with 'IOGenerator';

has 'raw_disk' => (
    is       => 'ro',
    isa      => 'Bool',
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

has 'extra_args' => (
    is      => 'ro',
    isa     => 'Maybe[Str]',
    default => undef
);

has 'output_dir' => (
    is       => 'ro',
    isa      => 'Str',
);

my $affinity_string;

if( hyperthreading_enabled() )
{
    # Use only the physical processors (every other logical processor)
    $affinity_string = '0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30';
}
else
{
    # Do nothing, DiskSpd uses simple round-robin affinity by default
}

sub run($$)
{
    my $self = shift;

    my $test_ref = shift;
    my $run_type = shift;

    my $write_percentage = $test_ref->{'write_percentage'};
    my $access_pattern   = $test_ref->{'access_pattern'};
    my $block_size       = $test_ref->{'block_size'};
    my $queue_depth      = $test_ref->{'queue_depth'};
    my $run_time         = $test_ref->{'run_time'};
  
    die "DiskSpd doesn't support variable entropy.\n"
        if defined $test_ref->{'compressibility'};

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

    my $cmd = "DiskSpd.exe ";

    $cmd .= "-w$write_percentage " if $write_percentage != 0;
    $cmd .= "-r " if $access_pattern eq 'random';
    $cmd .= "-b$block_size ";
    $cmd .= "-t$num_threads ";  
    $cmd .= "-o$ios_per_thread ";
    $cmd .= "-a$affinity_string " if defined $affinity_string;
    $cmd .= "-L ";
    $cmd .= "-d$run_time ";
   
    if( $self->raw_disk )
    {
        $cmd .= "#" . $self->pdnum;
    }
    else
    {
        $cmd .= $self->target_file;
    }
    
    # All-purpose escape hatch.  Support arbitrary args.
    $cmd .= " " . $self->extra_args if defined $self->extra_args;

    my $out_file = $self->output_dir .
        "\\$run_type-$test_ref->{'name_string'}.txt";
    
    open( my $OUT, ">$out_file" )
        or die "could not open $out_file: $!";
   
    # Save the command line as line 1 of the file
    print $OUT "$cmd\n";

    my ( $errorlevel, $stdout, $stderr ) = execute_task( $cmd );
   
    print $OUT "$stdout\n";
    print $OUT "$stderr\n";

    close( $OUT );

    die "DiskSpd returned non-zero errorlevel" if $errorlevel;
}

sub parse_cmd_line($$)
{
    my $self = shift;

    my $cmd_line = shift;
    my $stats_ref = shift;
   
    if( $cmd_line =~ /-w(\d+)/ )
    {
        $stats_ref->{'W Mix'} = $1;
        $stats_ref->{'R Mix'} = 100 - $stats_ref->{'W Mix'};
    }
    else
    {
        $stats_ref->{'W Mix'} = 0;
        $stats_ref->{'R Mix'} = 100;
    }
    
    if( $cmd_line =~ /-r/ )
    {
        $stats_ref->{'Access Type'} = 'random';
    }
    else
    {
        $stats_ref->{'Access Type'} = 'sequential';
    }

    $cmd_line =~ /-b(\d+)/;
    $stats_ref->{'Access Size'} = $1;
    
    my $num_threads = 1;

    if( $cmd_line =~ /-t(\d+)/ )
    {
        $num_threads = $1;
    }

    $cmd_line =~ /-o(\d+)/;
    my $ios_per_thread = $1;
    
    $stats_ref->{'QD'} = $ios_per_thread * $num_threads;
       
    # DiskSpd doesn't currently support variable entropy
    $stats_ref->{'Compressibility'} = 0;
}

my $pct_table_pat = qr/\|\s+(.+)\s+\|\s+(.+)\s+\|\s+(.+)\s+/;

my @extract_rules =
(
    {
        match => qr/min $pct_table_pat/,
        store =>
        [ 
            'Min Latency Read',
            'Min Latency Write',
            'Min Latency Total'
        ]
    },
    {
        match => qr/50th $pct_table_pat/,
        store => 
        [
            '50th Percentile Read',
            '50th Percentile Write',
            '50th Percentile Total'
        ]
    },
    {
        match => qr/90th $pct_table_pat/,
        store => 
        [
            '90th Percentile Read',
            '90th Percentile Write',
            '90th Percentile Total'
        ]
    },
    {
        match => qr/99th $pct_table_pat/,
        store => 
        [
            '2-nines Percentile Read',
            '2-nines Percentile Write',
            '2-nines Percentile Total'
        ]
    },
    {
        match => qr/3-nines $pct_table_pat/,
        store => 
        [
            '3-nines Percentile Read',
            '3-nines Percentile Write',
            '3-nines Percentile Total'
        ]
    },
    {
        match => qr/4-nines $pct_table_pat/,
        store => 
        [
            '4-nines Percentile Read',
            '4-nines Percentile Write',
            '4-nines Percentile Total'
        ]
    },
    {
        match => qr/5-nines $pct_table_pat/,
        store => 
        [
            '5-nines Percentile Read',
            '5-nines Percentile Write',
            '5-nines Percentile Total'
        ]
    },
    {
        match => qr/6-nines $pct_table_pat/,
        store => 
        [
            '6-nines Percentile Read',
            '6-nines Percentile Write',
            '6-nines Percentile Total'
        ]
    },
    {
        match => qr/7-nines $pct_table_pat/,
        store => 
        [
            '7-nines Percentile Read',
            '7-nines Percentile Write',
            '7-nines Percentile Total'
        ]
    },
    {
        match => qr/8-nines $pct_table_pat/,
        store => 
        [
            '8-nines Percentile Read',
            '8-nines Percentile Write',
            '8-nines Percentile Total'
        ]
    },
    {
        match => qr/max $pct_table_pat/,
        store =>
        [ 
            'Max Latency Read',
            'Max Latency Write',
            'Max Latency Total'
        ]
    },
);

sub remove_na($@)
{
    my $stats_ref = shift;
    
    foreach my $key ( keys %$stats_ref )
    {
        if( $stats_ref->{$key} =~ m|N/A| )
        {
            delete $stats_ref->{$key};
        }
    }
}

sub parse($$)
{
    my $self = shift;

    my $LOG = shift;
    my $stats_ref = shift;
   
    while( my $line = <$LOG> )
    {
        if( $line =~ /^(\w+) IO/ )
        {
            my $io_type = $1;

            while( $line = <$LOG> )
            {
                if( $line =~ /total:/ )
                {
                    $line =~ s/\s+//g;
                    my @values = split /\|/, $line;
                        
                    $stats_ref->{"IOs $io_type"} = $values[1];
                    $stats_ref->{"MB/sec $io_type"} = $values[2];
                    $stats_ref->{"IOPS $io_type"} = $values[3];
                    $stats_ref->{"Avg Latency $io_type"} = $values[4]; 

                    last;
                }
            }
        }
        
        do_simple_extract( $line, $stats_ref, \@extract_rules );
    }

    remove_na( $stats_ref, \@extract_rules );
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
