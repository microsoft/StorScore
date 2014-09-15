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

package WmicParser;

use strict;
use warnings;
use Moose;
use Util;

my @extract_rules_Win32_DiskDrive =
(
    {
        match => qr/^Model=(.+)/,
        store => 'Device Model'
    },
    {
        match => qr/^SerialNumber=(.+)/,
        store => 'Serial Number'
    },
    {
        match => qr/^FirmwareRevision=(.+)/,
        store => 'Firmware Version'
    },
    {
        match => qr/^Size=(.+)/,
        store => 'User Capacity (Bytes)'
    },
);

my @extract_rules_Win32_LogicalDisk =
(
    {
        match => qr/^Size=(.+)/,
        store => 'Partition Size (Bytes)'
    },
);

sub post_process($)
{
    my $stats_ref = shift;

    my $user_capacity_bytes = $stats_ref->{'User Capacity (Bytes)'};
    
    $stats_ref->{'User Capacity'} =
        bytes_to_human_base10( $user_capacity_bytes );

    my $part_size_bytes = $stats_ref->{'Partition Size (Bytes)'};

    if( $part_size_bytes )
    {
        $stats_ref->{'Partition Size'} =
            bytes_to_human_base10( $part_size_bytes );
    }
}

sub parse($$)
{
    my $self = shift;

    my $stats_ref = shift;
    my $file_name = shift;

    open my $FILE, '<', $file_name
        or die "Error opening $file_name";

    # Support the old format, which only had Win32_DiskDrive
    my $rules_ref = \@extract_rules_Win32_DiskDrive;

    while( my $line = <$FILE> )
    {
        if( $line =~ /^Win32_DiskDrive$/ )
        {
            $rules_ref = \@extract_rules_Win32_DiskDrive;
        }
        elsif( $line =~ /^Win32_LogicalDisk$/ )
        {
            $rules_ref = \@extract_rules_Win32_LogicalDisk;
        }

        do_simple_extract( $line, $stats_ref, $rules_ref );
    }

    post_process( $stats_ref );

    close $FILE;
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
