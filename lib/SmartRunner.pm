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

package SmartRunner;

use strict;
use warnings;
use Moose;
use File::Temp 'mktemp';
use Util;

has 'physical_drive' => (
    is => 'ro',
    isa => 'Int',
    required => 1
);

has 'device_type' => (
    is       => 'rw',
    isa      => 'Maybe[Str]',
    writer   => '_device_type',
    init_arg => undef,
    default  => undef 
);

has 'is_functional' => (
    is       => 'rw',
    isa      => 'Bool',
    writer   => '_is_functional',
    init_arg => undef,
    default  => 0
);

has 'rotation_rate' => (
    is       => 'rw',
    isa      => 'Maybe[Str]',
    writer   => '_rotation_rate',
    init_arg => undef,
    default  => undef 
);

has 'sata_version' => (
    is       => 'rw',
    isa      => 'Maybe[Str]',
    writer   => '_sata_version',
    init_arg => undef,
    default  => undef 
);

sub is_smartctl_present()
{
    my $missing = execute_task( 
        'where smartctl',
        quiet => 1
    );
    
    return 0 if $missing;
    return 1;
}

sub collect
{
    my $self = shift;
    my %args = @_;
    
    my $file_name = $args{'file_name'};
    my $output_dir = $args{'output_dir'};
    my $dev_type = $args{'dev_type'} // $self->device_type;
    my $do_identify = $args{'do_identify'};

    my $cmd = "";
    if( $dev_type =~ /nvme/i )
    {
        $cmd .= "StorageTool.exe ";
        $cmd .= "-HealthInfo ";
        $cmd .= "Disk " . $self->physical_drive . " ";
    }
    else
    {
        return unless( $self->is_smartctl_present() );
    
        $cmd .= "smartctl.exe ";
        $cmd .= "-d $dev_type " if defined $dev_type;
        $cmd .= "--identify " if defined $do_identify;
        $cmd .= "-a /dev/pd" . $self->physical_drive . " ";
        $cmd .= "-s on ";
    }

    if( defined $file_name )
    {
        $file_name = $output_dir . "\\$file_name"
            unless is_absolute_path( $file_name );

        execute_task( "echo $cmd > $file_name" );

        $cmd .= ">> $file_name";
    }
   
    return execute_task( $cmd );
}

sub detect_device_type()
{
    my $self = shift;

    my $file_name =
        mktemp( $ENV{'TEMP'} . "\\smart_devtypeXXXXXX" );
  
    my $detected_device_type;

    foreach my $try_type ( qw( nvme ata sat scsi ) )
    {
        $self->collect(
            file_name => $file_name,
            dev_type  => $try_type
        );

        open my $SMART, "<$file_name"
            or die "Couldn't open SMART output file: $file_name\n";

        while( my $line = <$SMART> )
        {
            # Previous code looked for "Vendor Specific SMART Attributes"
            # Sometimes this text appears but the counters are garbage.
            # Look for this ubiquitous attribute instead.
            if( $line =~ "9 Power_On_Hours" )
            {
                $detected_device_type = $try_type;
                last;
            }
            elsif( $line =~ /NVME/i )
            {
                $detected_device_type = $try_type;
                last;
            } 
        }

        close $SMART;
        last if( $detected_device_type );
    }

    unlink $file_name;

    return $detected_device_type;
}

my @extract_rules =
(
    {
        match => qr/Rotation Rate:\s+(.+)/,
        store => 'Rotation Rate'
    },
    {
        match => qr/SATA Version is:\s+(.+)/,
        store => 'SATA Version'
    },
    {#ToDo: Integrate this parameter with Sata Version & SmartCtl.exe
     #      (This format matches only the output from StorageTool.exe)
        match => qr/Disk #\d+\s+:\s+\[(\S+)\s+\]/,
        store => 'Protocol Version'
    },
);

sub BUILD
{
    my $self = shift;
    
    if( $pretend )
    {
        # Mock up a fake SATA III SSD for pretend mode
        $self->_is_functional( 1 );
        $self->_device_type( 'ata' );
        $self->_rotation_rate( 'Solid State Device' );
        $self->_sata_version( 'current: 6.0 Gb/s' );
        
        return;
    }

    $self->_device_type( $self->detect_device_type() );

    return unless defined $self->device_type;

    # If we get here, it looks like everything is working
    $self->_is_functional( 1 );

    my $file_name = mktemp( $ENV{'TEMP'} . "\\smart_initXXXXXX" );
   
    $self->collect( file_name => $file_name );
       
    my %stats;

    open my $FILE, '<', $file_name or die "Error opening $file_name";

    while( my $line = <$FILE> )
    {
        do_simple_extract( $line, \%stats, \@extract_rules );
    }
    close $FILE;
    
    unlink $file_name;

    $self->_rotation_rate( $stats{'Rotation Rate'} );
    $self->_sata_version( $stats{'SATA Version'} );
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
