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

package WmicRunner;

use strict;
use warnings;
use Moose;
use Util;

has 'target' => (
    is => 'ro',
    isa => 'Target',
    required => 1
);

has 'output_dir' => (
    is => 'ro',
    isa => 'Str',
    required => 1
);

sub collect
{
    my $self = shift;
    my $file_name = shift;
    
    my $out_file = $self->output_dir . "\\$file_name";

    open( my $FILE, ">$out_file" )
        or die "could not open $out_file: $!";
    
    # Collect device info
    my $pdnum = $self->target->physical_drive;
    my $pdname = "\\\\\\\\.\\\\PHYSICALDRIVE$pdnum";

    my $wmic_cmd;

    $wmic_cmd .= qq(path Win32_DiskDrive );
    $wmic_cmd .= qq(where Name="$pdname" );
    $wmic_cmd .= qq(get /format:list );

    print $FILE "Win32_DiskDrive\n";
    print $FILE wmic_helper( $wmic_cmd );

    # Collect volume info
    my $vol = $self->target->volume;

    if( $vol )
    {
        my $wmic_cmd;

        $wmic_cmd .= qq(path Win32_LogicalDisk );
        $wmic_cmd .= qq(where Name="$vol" );
        $wmic_cmd .= qq(get /format:list );

        print $FILE "Win32_LogicalDisk\n";
        print $FILE wmic_helper( $wmic_cmd );
    }

    close( $FILE );
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
