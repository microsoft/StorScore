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

package CommandLine;

use strict;
use warnings;
use Moose;
use English;
use Getopt::Long 'GetOptionsFromArray';
use POSIX 'strftime';

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

use Util;
use Compressibility;

has 'argv' => (
    is  => 'ro',
    isa => 'ArrayRef[Str]',
    required => 1
);

has 'target' => (
    is  => 'ro',
    isa => 'Maybe[Target]',
    default => undef,
    writer  => '_target'
);

has 'raw_disk' => (
    is  => 'ro',
    isa => 'Bool',
    default => 0,
    writer  => '_raw_disk'
);

has 'recipe' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_recipe'
);

has 'initialize' => (
    is  => 'ro',
    isa => 'Bool',
    default => 1,
    writer  => '_initialize'
);

has 'precondition' => (
    is  => 'ro',
    isa => 'Maybe[Bool]',
    default => undef,
    writer  => '_precondition'
);

has 'test_id' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_test_id'
);

has 'test_time_override' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_test_time_override'
);

has 'warmup_time_override' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_warmup_time_override'
);

has 'io_generator' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => 'diskspd',
    writer  => '_io_generator'
);

has 'io_generator_args' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_io_generator_args'
);

has 'active_range' => (
    is  => 'ro',
    isa => 'Int',
    default => 100,
    writer  => '_active_range'
);

has 'partition_bytes' => (
    is  => 'ro',
    isa => 'Maybe[Int]',
    default => undef,
    writer  => '_partition_bytes'
);

has 'demo_mode' => (
    is  => 'ro',
    isa => 'Bool',
    default => 0,
    writer  => '_demo_mode'
);

has 'auto_upload' => (
    is  => 'ro',
    isa => 'Bool',
    default => 1,
    writer  => '_auto_upload'
);

has 'results_share' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_results_share'
);

has 'results_share_user' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_results_share_user'
);

has 'results_share_pass' => (
    is  => 'ro',
    isa => 'Maybe[Str]',
    default => undef,
    writer  => '_results_share_pass'
);

has 'compressibility' => (
    is  => 'ro',
    isa => 'Maybe[Int]',
    default => undef,
    writer  => '_compressibility'
);

has 'collect_smart' => (
    is  => 'ro',
    isa => 'Bool',
    default => 1,
    writer  => '_collect_smart'
);

has 'collect_logman' => (
    is  => 'ro',
    isa => 'Bool',
    default => 1,
    writer  => '_collect_logman'
);

has 'collect_power' => (
    is  => 'ro',
    isa => 'Bool',
    default => 1,
    writer  => '_collect_power'
);

has 'keep_logman_raw' => (
    is  => 'ro',
    isa => 'Bool',
    default => 0,
    writer  => '_keep_logman_raw'
);

has 'target_type' => (
    is  => 'ro',
    isa => 'Str',
    default => 'auto',
    writer  => '_target_type'
);

has 'start_on_step' => (
    is  => 'ro',
    isa => 'Int',
    default => 1,
    writer  => '_start_on_step'
);

has 'stop_on_step' => (
    is  => 'ro',
    isa => 'Maybe[Int]',
    default => undef,
    writer  => '_stop_on_step'
);

# Helper to set Moose attributes via GetOpt::Long
sub attr
{
    my $self = shift;
    
    my $name = shift;
    my $val = shift;

    my $retval = eval( qq(\$self->_$name( '$val' );) );

    die $EVAL_ERROR unless defined $retval;
}

sub BUILD
{
    my $self = shift;
   
    my $help = 0;
    my $target_str;
    my $test_id_prefix;

    GetOptionsFromArray(
        $self->argv,
        "help|?"                 => \$help,
        "target=s"               => \$target_str, 
        "raw_disk!"              => sub { $self->attr(@_) }, 
        "recipe=s"               => sub { $self->attr(@_) },  
        "verbose!"               => \$verbose,
        "pretend!"               => \$pretend,
        "initialize!"            => sub { $self->attr(@_) },  
        "precondition!"          => sub { $self->attr(@_) },  
        "prompt!"                => \$prompt,
        "test_id=s"              => sub { $self->attr(@_) },  
        "test_id_prefix=s"       => \$test_id_prefix,
        "test_time_override=i"   => sub { $self->attr(@_) },
        "warmup_time_override=i" => sub { $self->attr(@_) },
        "io_generator=s"         => sub { $self->attr(@_) },
        "io_generator_args=s"    => sub { $self->attr(@_) },
        "active_range=i"         => sub { $self->attr(@_) },
        "partition_bytes=i"      => sub { $self->attr(@_) },
        "demo_mode!"             => sub { $self->attr(@_) },
        "auto_upload!"           => sub { $self->attr(@_) },
        "results_share=s"        => sub { $self->attr(@_) },
        "results_share_user=s"   => sub { $self->attr(@_) },
        "results_share_pass=s"   => sub { $self->attr(@_) },
        "compressibility=i"      => sub { $self->attr(@_) },
        "collect_smart!"         => sub { $self->attr(@_) },
        "collect_logman!"        => sub { $self->attr(@_) },
        "collect_power!"         => sub { $self->attr(@_) },
        "keep_logman_raw!"       => sub { $self->attr(@_) },
        "target_type=s"          => sub { $self->attr(@_) },
        "start_on_step=i"        => sub { $self->attr(@_) },
        "stop_on_step=i"         => sub { $self->attr(@_) },
    ) 
    or exit( -1 ); 

    if( $help )
    {
        warn get_usage_string();
        exit( -1 );
    }
    
    unless( defined $target_str )
    {
        my $msg =<<'END';
Provide a target with --target=X
  --target=\\\\.\\PHYSICALDRIVE3   Creates new volume and test file
  --target=3                       Shorthand for the above
  --target=Z:                      Create a test file on existing volume Z:
  --target=Z:\file.dat             Use existing volume and test file
END
        warn "$msg\n";

        list_physical_drives();
        list_volumes();

        exit( -1 );
    }
 
    unless( $self->active_range > 0 and $self->active_range <= 100 )
    {
        die "Invalid active_range. Expected a percentage between 0 and 100." 
    }

    unless( $self->io_generator ~~ [ 'sqlio', 'diskspd' ] )
    {
        die "Unsupported io_generator. Use --io_generator=diskspd|sqlio\n";
    }

    # Canonicalize target type to lower case
    $self->_target_type( lc( $self->target_type ) );

    unless( $self->target_type ~~ [qw( auto ssd hdd )] )
    {
        die "Unsupported target_type. Use --target_type=auto|ssd|hdd\n";
    }
    
    if( $self->raw_disk )
    {
        if( $self->active_range != 100 )
        {
            my $msg;

            $msg .= "Ignoring --active_range. ";
            $msg .= "Not supported in raw disk mode.\n";

            warn $msg;
        }

        if( defined $self->partition_bytes )
        {
            my $msg;

            $msg .= "Ignoring --partition_bytes. ";
            $msg .= "Not supported in raw disk mode.\n";

            warn $msg;
        }
    }

    if( $self->demo_mode )
    {
        $self->_active_range( 1 );
        $self->_test_time_override( 5 );
        $self->_warmup_time_override( 0 );
    }
   
    my %target_args = (
        target_str      => $target_str,
        raw_disk        => $self->raw_disk,
        active_range    => $self->active_range,
        partition_bytes => $self->partition_bytes,
    );
        
    $target_args{'override_type'} = $self->target_type
        unless $self->target_type eq 'auto';

    my $target = Target->new( %target_args );
    
    $self->_target( $target );
  
    unless( defined $self->recipe )
    {
        if( $target->is_ssd )
        {
            $self->_recipe( 'recipes\\turkey_test.rcp' );
        }
        else
        {
            $self->_recipe( 'recipes\\corners.rcp' );
        }
    }

    if( defined $self->test_time_override )
    {
        if( $self->io_generator eq 'sqlio' 
                and $self->test_time_override < 5 )
        {
            die "Minimum test time when using sqlio is 5 seconds\n";
        }
    }

    if( defined $self->warmup_time_override )
    {
        if( $self->io_generator eq 'sqlio'
                and $self->warmup_time_override > 0 
                and $self->warmup_time_override < 5 )
        {
            die "Warm up time must be 0 or > 5 seconds when using sqlio\n";
        }
    }
   
    # If no test_id was given, construct a sensible default
    if( not defined $self->test_id )
    {
        my $prefix = 
            make_legal_filename( 
                get_drive_model( $target->physical_drive ) );

        $self->_test_id(
            "$prefix-" . strftime( "%Y-%m-%d_%H-%M-%S", localtime ) );
    }

    if( defined $test_id_prefix )
    {
        $self->_test_id( $test_id_prefix . $self->test_id );
    }

    # Precondition automatically when targeting ssd, unless
    # the user specified --precondition or --noprecondition
    unless( defined $self->precondition )
    {
        $self->_precondition( $target->is_ssd );
    }
    
    unless( defined $self->results_share )
    {
        # Automatically enable upload if running in the lab
        if( Win32::DomainName() =~ /wcs/i
                or primary_dns_suffix() =~ /wcs/i )
        {
            $self->_results_share( '\\\\wcsfs\\StorScore' );
            $self->_results_share_user( 'wcsfs\StorScore' );
            $self->_results_share_pass( 'nope' );
        }
    }
   
    unless( defined $self->compressibility )
    {
        $self->_compressibility( 0 ); # default to 100% entropy
    }

    unless( Compressibility::is_valid_percentage( $self->compressibility ) )
    {
        my $msg;

        $msg .= "Unsupported compressibility percentage.\n";
        $msg .= "Use --compressibility=[";
        $msg .= join( '|', Compressibility::get_valid_percentages() );
        $msg .= "]\n";

        die $msg;
    }
}

sub get_usage_string
{
    my $usage = <<"END_USAGE";

USAGE 
  $script_name [options] 

OPTIONS
  --target          Indicates the drive or file to test (required)
  --initialize      Write the whole drive before testing.  Defaults to true.
  --precondition    Performs workload-dependent preconditioning. Defaults to true.
  --recipe=A.rcp    Use the test list defined in "A.rcp"
  --collect_smart   Collect drive's SMART metadata. Defaults to true.
  --collect_logman  Collect performance counters from logman. Defaults to true.
  --collect_power   Collect system power usage statistics. Defaults to true.
  --keep_logman_raw Prevent StorScore from deleting the logman data files.
  --target_type     Force target type to "ssd" or "hdd". Defaults to "auto".
  --start_on_step=n Begin testing on step n of the recipe.

EXAMPLES
  $script_name --target=1 
  $script_name --target=Z: --recipe=recipes\\my_new_recipe.rcp
  $script_name --target=Z:\\file.dat --nocollect_power

END_USAGE

    return $usage;
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;
