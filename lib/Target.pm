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
use SmartRunner;
use PreconditionRunner;

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

has 'protocol_type' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_protocol_type'
);

has 'model' => (
    is => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer => '_model'
);

has 'do_create_new_filesystem' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_do_create_new_filesystem'
);

has 'do_create_new_file' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_do_create_new_file'
);

has 'do_initialize' => (
    is => 'ro',
    isa => 'Maybe[Bool]',
    default => undef,
    writer => '_do_initialize'
);

has 'do_purge' => (
    is => 'ro',
    isa => 'Maybe[Bool]',
    default => undef,
    writer => '_do_purge'
);

has 'do_precondition' => (
    is => 'ro',
    isa => 'Maybe[Bool]',
    default => undef,
    writer => '_do_precondition'
);

has 'precondition_runner' => (
    is => 'ro',
    isa => 'Maybe[PreconditionRunner]',
    default => undef,
    writer => '_precondition_runner'
);

has 'is_purged' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_is_purged'
);

has 'is_prepared' => (
    is => 'ro',
    isa => 'Bool',
    default => 0,
    writer => '_is_prepared'
);

has 'is_existing_file_or_volume' => (
    is => 'ro',
    isa => 'Bool',
    default => 1,
    writer => '_is_existing_file_or_volume'
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
    
    if( $self->supports_smart )
    {
        # NB: the order of the following two checks matters
        # the NVMe smart log does not define the rotation rate
        if( $self->protocol_type =~ /nvme/ or 
    	    $self->rotation_rate =~ /Solid State Device/ )
        {
            return 'ssd';
        }
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

    my $cmd_line = $self->cmd_line;
    my $target_str = $cmd_line->target;
    my $raw_disk = $cmd_line->raw_disk;

    if( $target_str =~ /(\\\\\.\\PHYSICALDRIVE)?(\d+)$/ )
    {
        $self->_physical_drive( $2 );
        
        die qq(Error: target "$target_str" does not exist.\n)
            unless physical_drive_exists( $self->physical_drive );

        $self->_is_existing_file_or_volume( 0 );

        unless( $raw_disk )
        {
            $self->_do_create_new_filesystem( 1 );
            $self->_do_create_new_file( 1 );
        }
    }
    elsif( uc( $target_str ) =~ /^([A-Z]{1}\:)\\?$/ )
    {
        die "Error: --raw_disk unsupported with existing volumes.\n"
            if $raw_disk;

        $self->_volume( $1 );
    
        my $pdname = volume_to_physical_drive( $self->volume );
        $pdname =~ /(\d+$)/;
        $self->_physical_drive( $1 ); 

        $self->_do_create_new_file( 1 );
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

    my $smart = SmartRunner->new(
        physical_drive => $self->physical_drive
    );

    if( $smart->is_functional )
    {
        $self->_supports_smart( 1 );
        $self->_rotation_rate( $smart->rotation_rate );
        $self->_sata_version( $smart->sata_version );
        $self->_protocol_type( $smart->device_type );
    }
  
    if( $self->is_existing_file_or_volume )
    {
        # Targeting an existing file/volume, not a whole physical drive.
        # We cannot purge without destroying the existing file/volume.
        $self->_do_purge( 0 );
    }
    else
    {
        # Targeting a whole physical drive, not an existing volume.
        # Purge by default, unless the command line specified otherwise.
        $self->_do_purge( $cmd_line->purge // 1 );
    }

    # Default policy (can be overridden by command line):
    #   SSD: both precondition and initialize
    #   HDD: do not precondition or initialize
    $self->_do_initialize( $cmd_line->initialize // $self->is_ssd );
    $self->_do_precondition( $cmd_line->precondition // $self->is_ssd );
    
    $self->_precondition_runner(
        PreconditionRunner->new(
            cmd_line => $cmd_line,
            target   => $self
        ) 
    );
}   

sub purge
{
    my $self = shift;
    my %args = @_;

    my $msg_prefix = $args{'msg_prefix'} // die;
   
    print $msg_prefix;

    if( $self->is_purged )
    {
        print "Skipping purge of already-purged target\n";
        return;
    }
    
    if( $self->is_existing_file_or_volume )
    {
        print "Skipping purge of existing file/volume\n";
        return;
    }
    
    unless( $self->do_purge )
    {
        my $skip_requested =
            ( defined $self->cmd_line->purge and
                ( $self->cmd_line->purge == 0 ) );

        if( $skip_requested ) 
        {
            print "Skipping purge as requested\n";
        }
        else
        {
            print "Skipping purge\n";
        }

        return;
    }
        
    print "Purging..."; 

    my $failed = secure_erase( $self->physical_drive );
    if ($failed) { print "Secure Erase Failed [$failed]"; }

    print "\n";


    clean_disk( $self->physical_drive );

    $self->_is_purged( 1 );
    $self->_is_prepared( 0 );
}

sub prepare($)
{
    my $self = shift;
    my $test_ref = shift;

    return if $self->is_prepared;

    if( $self->do_create_new_filesystem )
    {
        my $partition_bytes;

        $partition_bytes = ( $test_ref->{'percent_used'} / 100.0 ) *
                              get_drive_size( $self->physical_drive )
            if defined $test_ref->{'percent_used'};

        $partition_bytes = $self->cmd_line->partition_bytes_override
            if defined $self->cmd_line->partition_bytes_override;

        create_filesystem(
            $self->physical_drive,
            $partition_bytes
        );
    
        $self->_volume( physical_drive_to_volume( $self->physical_drive ) );
    }

    if( $self->do_create_new_file )
    {
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
        execute_task( "sync.cmd " . $self->volume, quiet => 1 );
    }

    $self->_is_purged( 0 );
    $self->_is_prepared( 1 );
}

sub calculate_num_init_passes
{
    my $self = shift;
   
    # Go fast in demo mode
    return 1 if $self->cmd_line->demo_mode;

    # For HDD: if we choose to initialize, one pass is sufficient
    return 1 if $self->is_hdd;
   
    # For SSD: we want to dirty all of the NAND, including the OP
    # to avoid measuring the fresh-out-of-the-box condition.
    # 
    # Writing the drive 2x is overkill, but we do it only once.
    #
    # Note that in cases where the file is much smaller than
    # the drive, we will need to write the file many times in
    # order to write the drive once.

    return 2 if $self->cmd_line->raw_disk;

    my $file_size;

    if( $pretend )
    {
        # Ficticious 42GB test file for pretend mode
        $file_size = 42 * BYTES_PER_GB_BASE2;
    }
    else
    {
        $file_size = -s $self->file_name;
    }

    $file_size > 0 or die "Target file has zero size?";

    my $vol_size = get_volume_size( $self->volume );

    return( int( 2 * ( $vol_size / $file_size ) ) );
}

# Similar to SNIA "workload independent preconditioning"
sub initialize
{
    my $self = shift;
    
    my %args = @_;

    my $msg_prefix = $args{'msg_prefix'} // die;
    my $test_ref = $args{'test_ref'};
   
    # Future work: allow for custom init pattern
    
    unless( $self->do_initialize )
    {
        my $skip_requested =
            ( defined $self->cmd_line->initialize and
                ( $self->cmd_line->initialize == 0 ) );

        if( $skip_requested )
        {
            print $msg_prefix . "Skipping initialization as requested\n";
        }
        else
        {
            print $msg_prefix . "Skipping initialization\n";
        }
        
        return;
    }

    $self->prepare( $test_ref ) unless $self->is_prepared();
   
    $self->precondition_runner->write_num_passes(
        msg_prefix => $msg_prefix . "Initializing: ",
        num_passes => $self->calculate_num_init_passes(),
    );
}

# Similar to SNIA "workload dependent preconditioning"
sub precondition
{
    my $self = shift;

    my %args = @_;

    my $msg_prefix = $args{'msg_prefix'} // die;
    my $output_dir = $args{'output_dir'} // die;
    my $test_ref = $args{'test_ref'} // die;
    
    unless( $self->do_precondition )
    {
        my $skip_requested =
            ( defined $self->cmd_line->precondition and
                ( $self->cmd_line->precondition == 0 ) );

        if( $skip_requested )
        {
            print $msg_prefix . "Skipping preconditioning as requested\n";
        }
        else
        {
            print $msg_prefix . "Skipping preconditioning\n";
        }
        
        return;
    }
    
    $self->prepare( $test_ref ) unless $self->is_prepared();
   
    $self->precondition_runner->run_to_steady_state(
        msg_prefix => $msg_prefix . "Preconditioning: ",
        output_dir => $output_dir,
        test_ref => $test_ref
    );
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
