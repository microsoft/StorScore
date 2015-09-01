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

package PreconditionParser;

use strict;
use warnings;
use Moose;

sub parse($$)
{
    my $self = shift;
    my $file_name = shift;
    my $stats_ref = shift;
            
    unless( -e $file_name )
    {
        warn "\tNo precondition file. Will not report steady-state time.\n";
        return 0;
    }

    open my $LOG, "<$file_name" 
        or die "Error opening $file_name";

    <$LOG>; # ignore command line
       
    my $ss_line = <$LOG>;

    close $LOG;
    
    if( $ss_line =~ /achieved/ )
    {
        my $time;

        if( $ss_line =~ /(\d+) minutes/ )
        {
            $time = $1 * 60;
        }
        elsif( $ss_line =~ /(\d+) seconds/ )
        {
            $time = $1;
        }

        $stats_ref->{'Steady-State Time'} = $time;
    }
    elsif( $ss_line =~ /(abandoned|assumed)/ )
    {
        my $msg = "Steady-state $1 after ";
        
        if( $ss_line =~ /(\d+) minutes/ )
        {
            $msg .= "$1 minutes";
        }
        elsif( $ss_line =~ /(\d+) seconds/ )
        {
            $msg .= "$1 seconds";
        }
        elsif( $ss_line =~ /(\d+) IOs/ )
        {
            $msg .= "$1 IOs";
        }
        
        my $test_desc = $stats_ref->{'Test Description'};

        warn "\t$msg: $test_desc\n";
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
