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

package LogmanParser;

use strict;
use warnings;
use List::Util qw(sum);
use Moose;
use Util;

sub remove_blanks(@)
{
    return grep { $_ !~ m/^\s*$/ } @_;
}

sub compute_average(@)
{
    my @values = @_;
    
    my $num_values = scalar @values;

    my $average = 0;

    if( $num_values > 0 )
    {
        $average = sum( @values ) / $num_values;
    }

    return $average;
}

sub parse($$)
{
    my $self = shift;
    my $filename = shift;
    my $stats_ref = shift;
   
    my $cpu_seen = 0;
    my $qd_seen = 0;
    
    unless( -e $filename )
    {
        warn "\tCouldn't find logman file. Will not parse logman.\n";
        return 0;
    }

    # ISSUE-REVIEW: potential out-of-memory crash here on 32-bit Perl 
    foreach my $row_aref ( read_csv( $filename ) )
    {
        last if $cpu_seen and $qd_seen;

        my @values = @$row_aref;
        my $counter_name = shift @values;
        
        my $average = compute_average( remove_blanks( @values ) );
        
        if( $counter_name =~ /Processor Time/ )
        {
            $stats_ref->{'CPU Util'} = $average;
            $cpu_seen = 1;
        }
        elsif( $counter_name =~ /Disk Queue Length/ )
        {
            $stats_ref->{'Measured QD'} = $average;
            $qd_seen = 1;

            # Raise a warning if the percentage difference between
            # actual QD and measured QD exceeds 90%
            my $target_qd = $stats_ref->{'QD'};
            my $measured_qd = $stats_ref->{'Measured QD'};

            my $qd_threshold = 90;
            my $qd_diff = abs($target_qd - $measured_qd) / $target_qd * 100;

            if( $qd_diff > $qd_threshold )
            {
                my $test_desc = $stats_ref->{'Test Description'};

                warn "\tFailed to achieve target QD: $test_desc\n";
            }
        }
        elsif( $counter_name =~ /System Driver Total Bytes/ )
        {
            $stats_ref->{'Driver Memory (MB)'} = $average / ( 1024 * 1024 );
        }
        elsif( $counter_name =~ /System Driver Resident Bytes/ )
        {
            $stats_ref->{'Driver Resident Memory (MB)'} = $average / ( 1024 * 1024 );
        }
        else
        {
            warn "\tUnnammed Counter: $counter_name\n";
        }
    }

    return 1;
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
