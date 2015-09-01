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

package Power;

use strict;
use warnings;
use Moose;
use Util;

has 'output_dir' => (
    is      => 'rw',
    isa     => 'Maybe[Str]',
    default => undef
);

has 'description' => (
    is      => 'rw',
    isa     => 'Maybe[Str]',
    default => undef
);

my $power_csv;
my $power_pid;

sub is_binary_present()
{
    my $missing = execute_task( 'where ipmiutil', quiet => 1 );

    return 0 if $missing;
    return 1;
}

sub is_functional()
{
    return 0 unless is_binary_present();

    my $failed =
        execute_task(
            "ipmiutil.exe dcmi power",
            quiet => 1
        );
    
    return 0 if $failed;
    return 1;
}

# Start in-blade power measurement collection through IPMI
sub start()
{
    my $self = shift;
  
    my $dir = $self->output_dir;
    my $description = $self->description;

    $power_csv = "$dir\\power-$description.csv";

    $power_pid = 
        execute_task( "get-power.cmd > $power_csv", background => 1 );
}

sub parse()
{
    my $self = shift;
    my $stats_ref = shift;
    my $dir = $self->output_dir;
    my $description = $self->description;

    $power_csv = "$dir\\power-$description.csv";
   
    unless( -e $power_csv )
    {
        warn "\tCouldn't find power file. Will not parse power.\n";
        return 0;
    }

    open(my $data, '<', $power_csv)
        or die "Could not open '$power_csv' $!\n";
    
    my $sum = 0;
    my $count = 0;
    my @power = ();
    
    while (my $line = <$data>) 
    {
        chomp $line;
        $line =~ s/"//g;
        my @fields = split /,/, $line;
        my $val = $fields[0];
        $sum += $val;
        $count++;
        push (@power, $val);
    }
    close ( $data ) or die "Couldn't close file properly";

    my $average = 0;
    if ( $count > 0 ) 
    {
        $average = $sum / $count;
    }
   
    $stats_ref->{'System Power (W)'} = $average;

    return 1;
}

sub stop($)
{
    kill_task( $power_pid );
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
