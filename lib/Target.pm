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

package Target;

use strict;
use warnings;
use Moose;
use English;

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

use Util;
use SmartCtlRunner;

has 'cmd_line' => (
    is => 'ro',
    isa => 'CommandLine',
    required => 1
);

has 'physical_drive' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_physical_drive'
);

has 'volume' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_volume'
);

has 'file_name' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_file_name'
);

has 'supports_smart' => (
    is  => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_supports_smart'
);

has 'rotation_rate' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_rotation_rate'
);

has 'sata_version' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_sata_version'
);

has 'model' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_model'
);

has 'must_clean_disk' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_must_clean_disk'
);

has 'must_create_new_filesystem' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_must_create_new_filesystem'
);

has 'must_create_new_file' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_must_create_new_file'
);

sub is_ssd()
{
    my $self = shift;
    
    return $self->type eq 'ssd';
}

sub is_hdd
{
    my $self = shift;
    
    return $self->type eq 'hdd';
}

sub type
{
    my $self = shift;
   
    # Allow command line to override automatic target detection
    return $self->cmd_line->target_type
        unless $self->cmd_line->target_type eq 'auto';
    
    if( $self->supports_smart and 
        $self->rotation_rate =~ /Solid State Device/ )
    {
        return 'ssd';
    }
      
    return 'ssd' if $self->model =~ /NVMe|SSD/i;
    
    return 'hdd';
}

sub is_sata
{
    my $self = shift;

    return 1 if defined $self->sata_version;
    return 0;
}

sub is_6Gbps_sata
{
    my $self = shift;

    return 0 unless defined $self->sata_version;
    return 1 if $self->sata_version =~ /current: 6\.0 Gb\/s/;
    return 0;
}

sub BUILD
{
    my $self = shift;

    my $target_str = $self->cmd_line->target;
    my $raw_disk = $self->cmd_line->raw_disk;

    if( $target_str =~ /(\\\\\.\\PHYSICALDRIVE)?(\d+)$/ )
    {
        $self->_physical_drive( $2 );
        
        die qq(Error: target "$target_str" does not exist.\n)
            unless physical_drive_exists( $self->physical_drive );

        # We *must* ensure the disk is free of any partitions.
        # Otherwise, writes can silently fail and appear extremely fast.
        $self->_must_clean_disk( 1 );

        unless( $raw_disk )
        {
            $self->_must_create_new_filesystem( 1 );
            $self->_must_create_new_file( 1 );
        }
    }
    elsif( uc( $target_str ) =~ /^([A-Z]{1}\:)$/ )
    {
        die "Error: --raw_disk unsupported with existing volumes.\n"
            if $raw_disk;

        $self->_volume( $1 );
       
        my $pdname = volume_to_physical_drive( $self->volume );
        $pdname =~ /(\d+$)/;
        $self->_physical_drive( $1 ); 

        $self->_must_create_new_file( 1 );
    }
    elsif( -r $target_str or $pretend )
    {
        die "Error: --raw_disk unsupported with existing files.\n"
            if $raw_disk;
      
        $self->_file_name( $target_str );

        uc( $target_str ) =~ /^([A-Z]{1}\:)/;
        $self->_volume( $1 );
        
        my $pdname = volume_to_physical_drive( $self->volume );
        $pdname =~ /(\d+$)/;
        $self->_physical_drive( $1 ); 
    }
    else
    {
        die "Unexpected target: $target_str\n";
    }

    $self->_model( get_drive_model( $self->physical_drive ) );

    my $smartctl = SmartCtlRunner->new(
        physical_drive => $self->physical_drive
    );

    if( $smartctl->is_functional )
    {
        $self->_supports_smart( 1 );
        $self->_rotation_rate( $smartctl->rotation_rate );
        $self->_sata_version( $smartctl->sata_version );
    }
}   

sub prepare
{
    my $self = shift;

    if( $self->must_clean_disk )
    {
        # TODO: SECURE ERASE 
        #
        # When the target is an SSD, we should SECURE ERASE here instead of
        # the "diskpart clean":
        #
        #  if( $self->is_ssd )
        #  {
        #      print "Secure erasing disk...\n";
        #      secure_erase( $self->physical_drive );
        #  }
        #  else
        #  {
        #      print "Cleaning disk...\n";
        #      clean_disk( $self->physical_drive );
        #  }

        print "Cleaning disk...\n";

        clean_disk( $self->physical_drive );
    }

    if( $self->must_create_new_filesystem )
    {
        print "Creating new filesystem...\n";

        create_filesystem(
            $self->physical_drive,
            $self->cmd_line->partition_bytes
        );

        $self->_volume( physical_drive_to_volume( $self->physical_drive ) );
    }

    if( $self->must_create_new_file )
    {
        print "Creating test file...\n";

        my $free_bytes = get_volume_free_space( $self->volume );

        die "Couldn't determine free space"
        unless defined $free_bytes;

        # Reserve 1GB right off the top.
        # When we tried to use the whole drive, we saw odd errors.
        # Expectation is that test results should still be valid. 
        my $size = $free_bytes - BYTES_PER_GB_BASE2;

        # Support testing less then 100% of the disk
        $size = int( $size * $self->cmd_line->active_range / 100 );

        # Round to an even increment of 2MB.
        # Idea is to ensure the file size is an even multiple 
        # of pretty much any block size we might test. 
        $size = int( $size / BYTES_IN_2MB ) * BYTES_IN_2MB;

        $self->_file_name( $self->volume . "\\$TEST_FILE_NAME" );

        if( -e $self->file_name and not $pretend )
        {
            die "Error: target file " . $self->file_name . " exists!\n";
        }

        fast_create_file( $self->file_name, $size ) 
            or die "Couldn't create $size byte file: " .
                $self->file_name . "\n";
    }

    if( defined $self->volume )
    {
        print "Syncing target volume...\n";
        
        execute_task( "sync.cmd " . $self->volume, quiet => 1 );
    }

    if( $self->cmd_line->initialize )
    {
        my $pc = PreconditionRunner->new(
            raw_disk        => $self->cmd_line->raw_disk,
            pdnum           => $self->physical_drive,
            volume          => $self->volume,
            target_file     => $self->file_name,
            demo_mode       => $self->cmd_line->demo_mode,
            is_target_ssd   => $self->is_ssd
        );

        $pc->initialize();
    }
    else
    {
        print "Skipping initialization as requested.\n";
    }
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
