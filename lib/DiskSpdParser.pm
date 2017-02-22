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

sub parse_workload($$$)
{
    my $self = shift;

    my $dir = shift;
    my $cmd_line = shift;
    my $stats_ref = shift;

    $cmd_line =~ /.+-X(.+).xml/;
    my $xml_profile_path = $1;

    if( $xml_profile_path )
    {
        # NB: we can't use the filename as-is from the result file because 
        # the results directory probably moved from its original location
        
        $xml_profile_path =~ /.*(profile-.*)/;
        my $xml_profile = $1;

        parse_cmd_xml($dir, "$xml_profile.xml", $stats_ref);
    }
    else
    {
        parse_cmd_flags($cmd_line, $stats_ref);
    }
}

sub parse_cmd_xml($$$)
{
    my $dir = shift;
    my $profile_filename = shift;
    my $stats_ref = shift;

    my %target_stats = ();
    my $target_count = 0;
    my $sum_of_write_mix = 0;
    my $sum_of_QD = 0;
    my $sum_of_entropy = 0;

    my $xml_parser = XML::LibXML->new();
    my $doc = $xml_parser->parse_file( "$dir\\$profile_filename" );

    foreach my $target ( $doc->findnodes( '//Target' ) )
    {
        $target_count++;

        my $path = $target->findvalue( './Path' ); 
        #
        #cannot target multiple files when there is no filesystem
        $target_stats{$path}{'Raw Disk'} = 0; 

        my $write_ratio = $target->findvalue( './WriteRatio' );
        $target_stats{$path}{'W Mix'} = $write_ratio; 
        $target_stats{$path}{'R Mix'} = 100 - $write_ratio;
        $sum_of_write_mix += $write_ratio;

        my $io_size_B = $target->findvalue( './BlockSize' );
        $target_stats{$path}{'Access Size'} = $io_size_B / BYTES_PER_KB_BASE2;

        my $alignment = $io_size_B;
        if( $target->findvalue( './Random' ) )
        {
            $target_stats{$path}{'Access Type'} = 'random';
            $alignment = $target->findvalue( './Random' );
        }
        else
        {
            $target_stats{$path}{'Access Type'} = 'sequential';
            $alignment = $target->findvalue( './StrideSize' );
        }
        $target_stats{$path}{'Alignment'} = $alignment / BYTES_PER_KB_BASE2;

        my $ios_per_thread = $target->findvalue( './RequestCount' );
        my $num_threads = $target->findvalue( './ThreadsPerFile' );
        $target_stats{$path}{'QD'} = $ios_per_thread * $num_threads;
        $sum_of_QD += $target_stats{$path}{'QD'};

        my $entropy_filename = $target->findvalue( './/FilePath' );

        #search for the format of Storscore's pre-generated entropy files
        if( $entropy_filename =~ /.?([^\\]*)_pct_comp\.bin/ )
        {
            my $file_name = $1;
        
            $file_name =~ /(\d+)/;

            $target_stats{$path}{'Compressibility'} = $1;
            $sum_of_entropy += $1;
        }
        else
        {
            #file name format is not recognized
            $target_stats{$path}{'Compressibility'} = "unknown";
            $sum_of_entropy = "unknown";
        }
    }

    # Issue review: the aggregate workload of multiple targets is random
    # Is there anything more intersting we can call it?  For example, if all the
    # individual workloads are sequential, should we call it something like
    # "streaming"?
    
    # This entry representing the aggregate workload
    $target_stats{'Total'}{'Raw Disk'} = 0;
    $target_stats{'Total'}{'Access Type'} = 'random';
    $target_stats{'Total'}{'W Mix'} = $sum_of_write_mix / $target_count;
    $target_stats{'Total'}{'R Mix'} = 100 - $target_stats{'Total'}{'W Mix'};
    $target_stats{'Total'}{'QD'} = $sum_of_QD;
    $target_stats{'Total'}{'Compressibility'} = $sum_of_entropy / $target_count;

    $stats_ref->{'Workloads'} = \%target_stats;
}
sub parse_cmd_flags($$)
{
    my $cmd_line = shift;
    my $stats_ref = shift;

    my %target_stats = ();

    $cmd_line =~ /\s+(".*"|\S+)$/ or die;
    my $target = $1;

    if( $target =~ /^\#\d+$/ )
    {
        $target_stats{'Total'}{'Raw Disk'} = 1;
    }
    else
    {
        $target_stats{'Total'}{'Raw Disk'} = 0;
    }
   
    if( $cmd_line =~ /-w(\d+)/ )
    {
        $target_stats{'Total'}{'W Mix'} = $1;
        $target_stats{'Total'}{'R Mix'} = 100 - $1;
    }
    else
    {
        $target_stats{'Total'}{'W Mix'} = 0;
        $target_stats{'Total'}{'R Mix'} = 100;
    }
    
    $cmd_line =~ /-b(\w+)/;
    $target_stats{'Total'}{'Access Size'} = human_to_kilobytes( $1 );

    $target_stats{'Total'}{'Alignment'} = human_to_kilobytes( $1 );
    if( $cmd_line =~ /-r(\d+.*)\s+/ or $cmd_line =~ /-si(\d+.*)\s+/ )
    {
        $target_stats{'Total'}{'Alignment'} = human_to_kilobytes( $1 );
    } 

    if( $cmd_line =~ /-r\s+/ )
    {
        $target_stats{'Total'}{'Access Type'} = 'random';
    }
    else
    {
        $target_stats{'Total'}{'Access Type'} = 'sequential';
    }

    my $num_threads = 1;

    if( $cmd_line =~ /-t(\d+)/ )
    {
        $num_threads = $1;
    }

    $cmd_line =~ /-o(\d+)/;
    my $ios_per_thread = $1;
    
    $target_stats{'Total'}{'QD'} = $ios_per_thread * $num_threads;
       
    #search for the format of Storscore's pre-generated entropy files
    if( $cmd_line =~ /-Z.*?([^\\]*)_pct_comp\.bin/ )
    {
        my $file_name = $1;
    
        $file_name =~ /(\d+)/;

        $target_stats{'Total'}{'Compressibility'} = $1;
    }
    else
    {
        #file name format is not recognized
        $target_stats{'Total'}{'Compressibility'} = "unknown";
    }

    $stats_ref->{'Workloads'} = \%target_stats;

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

sub remove_na($)
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

    my %target_stats = ();
    my $previous_line = "";
    my $target_count = 0;

    while( my $line = <$LOG> )
    {

        if( $line =~ /path:/ )
        {
            $target_count++;
        }

        if( $line =~ /^(\w+) IO/ )
        {
            my $io_type = $1;

            while( $line = <$LOG> )
            {
                # This line contains per-thread throughput information
                if( $line =~ /\s(\d+.*B)/ and $target_count > 1 )
                {
                    $line =~ s/\s+//g;
                    my @values = split /\|/, $line;

                    my $mb = $values[1] / BYTES_PER_MB_BASE2;
                    my $ios = $values[2];
                    my $mb_s = $values[3];
                    my $iops = $values[4];
                    my $lat = $values[5];

                    my $target_with_size = $values[7];
                    $target_with_size =~ /(.*)\(.*\)/;
                    my $target = $1;

                    $target_stats{$target}{"IOs $io_type"} += $ios;
                    $target_stats{$target}{"MB/sec $io_type"} += $mb_s;
                    $target_stats{$target}{"IOPS $io_type"} += $iops;
                    $target_stats{$target}{"Avg Latency $io_type"} += $ios*$lat;
                }
                
                # This line contains the throughput totals for this IO type
                if( $line =~ /total:/ )
                {
                    $line =~ s/\s+//g;
                    my @values = split /\|/, $line;

                    $target_stats{"Total"}{"IOs $io_type"}    = $values[1];
                    $target_stats{"Total"}{"MB/sec $io_type"} = $values[2];
                    $target_stats{"Total"}{"IOPS $io_type"}   = $values[3];
                    $target_stats{"Total"}{"Avg Latency $io_type"} = $values[4];

                    last;
                }
            }

            foreach my $path ( keys %target_stats )
            {
                unless ( $path eq "Total" or 
                         $target_stats{$path}{"IOs $io_type"} == 0 )
                {
                    $target_stats{$path}{"Avg Latency $io_type"} = 
                        $target_stats{$path}{"Avg Latency $io_type"} / 
                        $target_stats{$path}{"IOs $io_type"};
                }
            }
            
        }
        

        if( $line =~ /-ile/ )
        {

            my $target = "Total";
            $previous_line =~ s/\s+//g;
            $target = $previous_line 
                unless ( $previous_line eq "total:" ) or
                       ( $previous_line eq "" );

            while( $line = <$LOG> )
            {
                do_simple_extract( $line, 
                                   $target_stats{$target}, 
                                   \@extract_rules );
                last if( $line =~ /max/ );
            }
        }

        $previous_line = $line;
    }

    foreach my $path ( keys %target_stats ) 
    {
        remove_na( $target_stats{$path} );
    }

    $stats_ref->{"Measurements"} = \%target_stats;
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
