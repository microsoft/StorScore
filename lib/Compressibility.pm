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

package Compressibility;

use strict;
use warnings;
use English;

use Util;

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

# Our entropy files will be 20 MB each
use constant FILE_SIZE => 20 * 1024 * 1024;

my @valid_percentages = 
    qw( 0 1 2 3 4 5 6 7 8 9 10 20 30 40 50 60 70 80 90 100 );
   
sub get_valid_percentages()
{
    return @valid_percentages;
}

sub is_valid_percentage($)
{
    my $percentage = shift;

    return 1 if $percentage ~~ @valid_percentages;
    return 0;
}

sub get_filename($)
{
    my $percentage = shift;

    return "$entropy_dir\\$percentage" . "_pct_comp.bin";
}

sub create_entropy_files()
{
    use constant BLOCK_SIZE => 4096;

    my $num_blocks = FILE_SIZE / BLOCK_SIZE; 

    # paranoia: we want to make the same files every time
    srand( 42 );

    foreach my $percent ( @valid_percentages )
    {
        my $bin_file = $percent . "_pct_comp.bin";
        
        next if -e "$entropy_dir\\$bin_file";

        print "\tWriting $bin_file\n";

        open my $OUTFILE, ">$entropy_dir\\$bin_file";
        binmode $OUTFILE;

        my $num_zeros = 0;

        unless( $percent == 0 )
        {
            my $intercept = 1.5; # determined experimentally
            my $slope = ( 100 - $intercept ) / 100;
            my $percent_zeros = ( ( $percent * $slope ) + $intercept );

            $num_zeros = BLOCK_SIZE * ( $percent_zeros / 100 );
        }

        for( my $block = 0; $block < $num_blocks; $block++ )
        {
            for( my $i = 0; $i < BLOCK_SIZE; $i++ )
            {
                if( $i <= $num_zeros )
                {
                    print $OUTFILE pack( "C", 0 );
                }
                else
                {
                    print $OUTFILE pack( "C", int( rand( 256 ) ) );
                }
            }
        }

        close $OUTFILE;
    }
}

sub entropy_files_ok()
{
    foreach my $percent ( @valid_percentages )
    {
        my $bin_file = 
            "$entropy_dir\\$percent" . "_pct_comp.bin";
        
        return 0 unless -e $bin_file;
    }

    return 1;
}

unless( entropy_files_ok() )
{
    warn "One-time auto-generation of entropy files...\n";

    mkdir( $entropy_dir ) unless -d $entropy_dir;
    create_entropy_files();

    warn "Done!\n\n";
}

1;
