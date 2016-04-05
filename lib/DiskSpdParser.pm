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

package DiskSpdParser;

use strict;
use warnings;
use Moose;
use Util;

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

    $cmd_line =~ /-b(\w+)/;
    $stats_ref->{'Access Size'} = human_to_kilobytes( $1 );
    
    my $num_threads = 1;

    if( $cmd_line =~ /-t(\d+)/ )
    {
        $num_threads = $1;
    }

    $cmd_line =~ /-o(\d+)/;
    my $ios_per_thread = $1;
    
    $stats_ref->{'QD'} = $ios_per_thread * $num_threads;
       
    if( $cmd_line =~ /-Z.*?([^\\]*)\.bin/ )
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

    if( $target =~ /^\#\d+$/ )
    {
        $stats_ref->{'Raw Disk'} = 1;
    }
    else
    {
        $stats_ref->{'Raw Disk'} = 0;
    }
}

my $pct_table_pat = qr/\|\s+(.+)\s+\|\s+(.+)\s+\|\s+(.+)\s+/;

my @extract_rules =
(
    {
        match => qr/.+alignment:\s+(\d+).+/,
        store =>
        [
            'Alignment'
        ]
    },
    {
        match => qr/.+stride:\s+(\d+).+/,
        store =>
        [
            'Alignment'
        ]
    },
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
        match => qr/95th $pct_table_pat/,
        store => 
        [
            '95th Percentile Read',
            '95th Percentile Write',
            '95th Percentile Total'
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
        match => qr/9-nines $pct_table_pat/,
        store => 
        [
            '9-nines Percentile Read',
            '9-nines Percentile Write',
            '9-nines Percentile Total'
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
