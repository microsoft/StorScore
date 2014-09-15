@rem = ' vim: set filetype=perl: ';
@rem = ' --*-Perl-*-- ';
@rem = '
@echo off
setlocal
set PATH=%~dp0\..\perl\bin;%~dp0\..\bin;%PATH%
perl -w %~f0 %*
exit /B %ERRORLEVEL%
';

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

use strict;
use warnings;
use English;
use Compress::Zlib;
use List::Util 'max';
    
use constant BLOCK_SIZE => 4096;
use constant DEFLATE_LEVEL => 3;
    
# N.B.
# I purposely avoid using $d->total_in() and $d->total_out() to
# compute % compressible because these appear to be signed 32-bit
# integers which overflow on large files leading to odd results.

my @prefixes = sort { $a <=> $b } map { /(\d+)/; $1 } glob "*.bin";

foreach my $prefix ( @prefixes )
{
    my $bin_file = $prefix . "_pct_comp.bin";

    my $d = deflateInit(
        -Bufsize => BLOCK_SIZE,
        -Level => DEFLATE_LEVEL,
    );

    open( my $FH, "<$bin_file" );
    binmode $FH;

    my $buffer;
    
    my $original_size = 0;
    my $compressed_size = 0;

    while( read( $FH, $buffer, BLOCK_SIZE ) )
    {
        $original_size += length( $buffer );

        $compressed_size += length( $d->deflate( $buffer ) );
        $compressed_size += length( $d->flush( Z_FULL_FLUSH ) );
    }

    close $FH;

    my $percent_compressible =
        ( $original_size - $compressed_size ) / $original_size * 100;

    $percent_compressible = max( 0, $percent_compressible );

    printf( "%17s: %8d --> %8d = %5.1f%% compressible\n",
        $bin_file,
        $original_size,
        $compressed_size,
        $percent_compressible );
}
