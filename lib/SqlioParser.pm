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

package SqlioParser;

use strict;
use warnings;
use Moose;
use Util;

my @extract_rules_sqlio =
(
    {
        match => qr/IOs\/sec:\s+([0-9]*\.\d+)/,
        store => 'IOPS Total'
    },
    {
        match => qr/MBs\/sec:\s+([0-9]*\.\d+)/,
        store => 'MB/sec Total'
    },
    {
        match => qr/Total\_IOs:\s+(\d+)/,
        store => 'IOs Total'
    },
    {
        match => qr/Min_Latency Overall\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Min Latency Total'
    },
    {
        match => qr/Avg_Latency Overall\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Avg Latency Total'
    },
    {
        match => qr/Max_Latency Overall\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Max Latency Total'
    },
    {
        match => qr/Min_Latency of Reads\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Min Latency Read'
    },
    {
        match => qr/Avg_Latency of Reads\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Avg Latency Read'
    },
    {
        match => qr/Max_Latency of Reads\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Max Latency Read'
    },
    {
        match => qr/Min_Latency of Writes\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Min Latency Write'
    },
    {
        match => qr/Avg_Latency of Writes\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Avg Latency Write'
    },
    {
        match => qr/Max_Latency of Writes\(ms\):\s+([0-9]*\.\d+)/,
        store => 'Max Latency Write'
    },
    {
        match => qr/50th percentile\(ms\):\s+(.+)/,
        store => '50th Percentile Raw'
    },
    {
        match => qr/90th percentile\(ms\):\s+(.+)/,
        store => '90th Percentile Raw'
    },
    {
        match => qr/95th percentile\(ms\):\s+(.+)/,
        store => '95th Percentile Raw'
    },
    {
        match => qr/99th percentile\(ms\):\s+(.+)/,
        store => '2-nines Percentile Raw'
    },
    {
        match => qr/99.9th percentile\(ms\):\s+(.+)/,
        store => '3-nines Percentile Raw'
    },
    {
        match => qr/4-nines percentile\(ms\):\s+(.+)/,
        store => '4-nines Percentile Raw'
    },
    {
        match => qr/5-nines percentile\(ms\):\s+(.+)/,
        store => '5-nines Percentile Raw'
    },
    {
        match => qr/6-nines percentile\(ms\):\s+(.+)/,
        store => '6-nines Percentile Raw'
    },
    {
        match => qr/7-nines percentile\(ms\):\s+(.+)/,
        store => '7-nines Percentile Raw'
    },
    {
        match => qr/8-nines percentile\(ms\):\s+(.+)/,
        store => '8-nines Percentile Raw'
    },
    {
        match => qr/IOs stall\(Latency > 15s\):\s+([0-9]*\.\d+)/,
        store => 'IO stall'
    },
);

sub process_percentiles($)
{
    my $stats_ref = shift;

    foreach my $stat ( keys %$stats_ref )
    {
        my $old_val = $stats_ref->{$stat};

        if ( $stat =~ /Percentile Raw/ )
        {
            my $new_val;

            # %ile expressed as range 'lb - ub'   
            if( $old_val =~ /\d+ - (\d+)/ )
            {
                # conservatively take the upper bound
                $new_val = $1;
            }
            # %ile expressed as floating point number
            elsif( $old_val =~ /(\d+\.\d+)/ )
            {
                $new_val = $1;
            }
            # %ile expressed as a minimum 'lb+'
            elsif( $old_val =~ /(\d+)\+/ )
            {
                $new_val = $1;
            }
            else
            {
                die "couldn't parse $stats_ref->{$stat}\n";
            }
           
            my $new_stat = $stat =~ s/Raw/Total/r;

            $stats_ref->{$new_stat} = $new_val;
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
        do_simple_extract( $line, $stats_ref, \@extract_rules_sqlio );
    }
    
    process_percentiles( $stats_ref );
}

sub parse_cmd_line($$)
{
    my $self = shift;

    my $cmd_line = shift;
    my $stats_ref = shift;
   
    $cmd_line =~ /-T(\d+)/;
    $stats_ref->{'R Mix'} = $1;
    $stats_ref->{'W Mix'} = 100 - $stats_ref->{'R Mix'};
    
    $cmd_line =~ /-f(\w+)/;
    $stats_ref->{'Access Type'} = $1;

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

    if( $cmd_line =~ /-q.*?([^\\]*)\.bin/ )
    {
        my $file_name = $1;
    
        $file_name =~ /(\d+)/;

        $stats_ref->{'Compressibility'} = $1;
    }
    else
    {
        $stats_ref->{'Compressibility'} = 0;
    }
    
    $cmd_line =~ /\s+(".*"|\S+)$/ or die;
    my $target = $1;

    if( $target =~ /^-R\d+$/ )
    {
        $stats_ref->{'Raw Disk'} = 1;
    }
    else
    {
        $stats_ref->{'Raw Disk'} = 0;
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
