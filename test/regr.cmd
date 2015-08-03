@rem = ' vim: set filetype=perl: ';
@rem = ' --*-Perl-*-- ';
@rem = '
@echo off
setlocal
set PATH=%~dp0\perl\bin;%~dp0\bin;%PATH%
perl -w "%~f0" %*
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

use Getopt::Long;
use File::Basename;
use English;

my $script_name = basename( $PROGRAM_NAME );
my $script_dir = dirname( $PROGRAM_NAME );

my $target;
my $outfile;

GetOptions(
    "target=s"  => \$target,
    "outfile=s" => \$outfile,
);

unless( defined $target and defined $outfile )
{
    warn "usage: $script_name --target=TGT --outfile=FILE\n";
    exit(-1);
}

$outfile = "$script_dir\\$outfile";

sub my_exec
{
    my $cmd = shift;
    system( "echo $cmd >> $outfile" );
    system( "$cmd >> $outfile 2>&1" );
}

sub run_one
{
    my $args = shift;

    my $cargs;
    $cargs .= "--target=$target ";
    $cargs .= "--pretend ";
    $cargs .= "--verbose ";
    $cargs .= "--noprompt ";
    $cargs .= "--test_id=regr ";

    # ISSUE-REVIEW: add --force_ssd and --force_hdd here?

    # run corners
    system( "rmdir /S /Q results\\regr >NUL 2>&1" );
    
    my_exec(
        "storscore.cmd $cargs $args --recipe=recipes\\corners.rcp"
    );

    # run targeted tests
    system( "rmdir /S /Q results\\regr >NUL 2>&1" );
    
    my_exec(
        "storscore.cmd $cargs $args --recipe=recipes\\targeted_tests.rcp"
    );
}

unlink( $outfile );
chdir( ".." );

run_one( "" );
run_one( "--noinitialize" );
run_one( "--noprecondition" );
run_one( "--force_ssd" );
# ISSUE-REVIEW: uncomment once --force_hdd is supported?
#run_one( "--force_hdd" );
run_one( "--raw_disk" );
run_one( "--active_range=50" );
run_one( "--partition_bytes=1000000000" );

# post process output file to remove noise
rename( $outfile, "$outfile.orig" );

open( my $in, "<$outfile.orig" );
open( my $out, ">$outfile" );

while( my $line = <$in> )
{
    # Remove random temp file names
    $line =~ s/AppData\\\S*//;

    # Remove times
    $line =~ s/\d{2}:\d{2}:\d{2} (AM|PM)//g;

    # Remove "Done" line with overall runtime
    $line =~ s/^Done.*//;

    print $out $line;
}

close $in;
close $out;

unlink( "$outfile.orig" );

print "Done! Diff $outfile against another run.\n";
