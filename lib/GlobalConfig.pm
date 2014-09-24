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

package GlobalConfig;

use strict;
use warnings;
use English;
use Getopt::Long 'GetOptionsFromArray';
use POSIX 'strftime';
use Hash::Util qw( lock_hash unlock_hash );

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

use Exporter;
use vars qw(@ISA @EXPORT);

use Util;
use Compressibility;

our %gc;

@ISA = qw(Exporter);
@EXPORT = qw( %gc );

sub init(@)
{
    my @args = @_;

    my $help;
    my $target;
    my $raw_disk = 0;
    my $recipe;
    my $initialize = 1;
    my $precondition;
    my $test_id;
    my $test_id_prefix;
    my $test_time_override;
    my $warmup_time_override;
    my $io_generator = 'sqlio';
    my $io_generator_args;
    my $active_range = 100;
    my $partition_bytes;
    my $quick_test = 0;
    my $results_share;
    my $results_share_user;
    my $results_share_pass;
    my $compressibility = -1;
    my $collect_smart = 1;
    my $collect_logman = 1;
    my $collect_power = 1;
    my $keep_logman_raw = 0;
    my $force_ssd = 0;
    my $start_on_step = 1;

    GetOptionsFromArray(
        \@args,
        "help|?"                 => \$help,
        "target=s"               => \$target,
        "raw_disk!"              => \$raw_disk, 
        "recipe=s"               => \$recipe,
        "verbose!"               => \$verbose,
        "pretend!"               => \$pretend,
        "initialize!"            => \$initialize,
        "precondition!"          => \$precondition,
        "prompt!"                => \$prompt,
        "test_id=s"              => \$test_id,
        "test_id_prefix=s"       => \$test_id_prefix,
        "test_time_override=i"   => \$test_time_override,
        "warmup_time_override=i" => \$warmup_time_override,
        "io_generator=s"         => \$io_generator,
        "io_generator_args=s"    => \$io_generator_args,
        "active_range=i"         => \$active_range,
        "partition_bytes=i"      => \$partition_bytes,
        "quick_test!"            => \$quick_test,
        "results_share=s"        => \$results_share,
        "results_share_user=s"   => \$results_share_user,
        "results_share_pass=s"   => \$results_share_pass,
        "compressibility=i"      => \$compressibility,
        "collect_smart!"         => \$collect_smart,
        "collect_logman!"        => \$collect_logman,
        "collect_power!"         => \$collect_power,
        "keep_logman_raw!"       => \$keep_logman_raw,
        "force_ssd!"             => \$force_ssd,
        "start_on_step=i"        => \$start_on_step,
    ) 
    or exit( -1 ); 

    if( $help )
    {
        warn get_usage_string();
        exit( -1 );
    }

    unless( defined $target )
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

    my $target_physicaldrive;
    my $target_volume;
    my $target_file;
    my $clean_disk;
    my $create_new_filesystem;
    my $create_new_file;

    if( $target =~ /(\\\\\.\\PHYSICALDRIVE)?(\d+)$/ )
    {
        $target_physicaldrive = $2;
        
        die qq(Error: target "$target" does not exist.\n)
            unless physical_drive_exists( $target_physicaldrive );

        # We *must* ensure the disk is free of any partitions.
        # Otherwise, writes can silently fail and appear extremely fast.
        $clean_disk = 1;

        if( $raw_disk )
        {
            $create_new_filesystem = 0;
            $create_new_file = 0;
        }
        else
        {
            $create_new_filesystem = 1;
            $create_new_file = 1;
        }
    }
    elsif( lc( $target ) =~ /^([a-z]{1}\:)$/ )
    {
        die "Error: --raw_disk unsupported with existing volumes.\n"
            if $raw_disk;

        $target_volume = $1;
       
        my $pdname = volume_to_physicaldrive( $target_volume );
        $target_physicaldrive = chop $pdname; 

        $clean_disk = 0;
        $create_new_filesystem = 0;
        $create_new_file = 1;
    }
    elsif( -r $target )
    {
        die "Error: --raw_disk unsupported with existing files.\n"
            if $raw_disk;
      
        $target_file = $target;

        lc( $target ) =~ /^([a-z]{1}\:)/;
        $target_volume = $1;
        
        my $pdname = volume_to_physicaldrive( $target_volume );
        $target_physicaldrive = chop $pdname; 
        
        $clean_disk = 0;
        $create_new_filesystem = 0;
        $create_new_file = 0;
    }
    else
    {
        die "Unexpected target: $target\n";
    }

    unless( $active_range > 0 and $active_range <= 100 )
    {
        die "Invalid active_range. Expected a percentage between 0 and 100." 
    }

    unless( $io_generator ~~ [ 'sqlio', 'diskspd' ] )
    {
        die "Unsupported io_generator. Use --io_generator=diskspd|sqlio\n";
    }

    if( $raw_disk )
    {
        if( $active_range != 100 )
        {
            my $msg;

            $msg .= "Ignoring --active_range. ";
            $msg .= "Not supported in raw disk mode.\n";

            warn $msg;
        }

        if( defined $partition_bytes )
        {
            my $msg;

            $msg .= "Ignoring --partition_bytes. ";
            $msg .= "Not supported in raw disk mode.\n";

            warn $msg;
        }
    }

    if( $quick_test )
    {
        $initialize = 0;
        $active_range = 1;
        $test_time_override = 5;
        $warmup_time_override = 0;
    }

    if( defined $test_time_override )
    {
        if( $io_generator eq 'sqlio' 
                and $test_time_override < 5 )
        {
            die "Minimum test time when using sqlio is 5 seconds\n";
        }
    }

    if( defined $warmup_time_override )
    {
        if( $io_generator eq 'sqlio'
                and $warmup_time_override > 0 
                and $warmup_time_override < 5 )
        {
            die "Warm up time must be 0 or > 5 seconds when using sqlio\n";
        }
    }

    # If no test_id was given, construct a sensible default
    if( not defined $test_id )
    {
        my $prefix = 
            make_legal_filename( 
                get_drive_model( $target_physicaldrive ) );

        $test_id = "$prefix-" . strftime( "%Y-%m-%d_%H-%M-%S", localtime );
    }

    if( defined $test_id_prefix )
    {
        $test_id = $test_id_prefix . $test_id;
    }

    my $output_dir = "$results_dir\\$test_id";

    unless( defined $results_share )
    {
        # Automatically enable upload if running in the lab
        if( Win32::DomainName() =~ /wcs/i
                or primary_dns_suffix() =~ /wcs/i )
        {
            $results_share = '\\\\wcsfs\\StorScore';
            $results_share_user = 'wcsfs\StorScore';
            $results_share_pass = 'nope';
        }
    }
   
    if( $compressibility == -1 )
    {
        $compressibility = 0; # default to 100% entropy
    }

    unless( Compressibility::is_valid_percentage( $compressibility ) )
    {
        my $msg;

        $msg .= "Unsupported compressibility percentage.\n";
        $msg .= "Use --compressibility=[";
        $msg .= join( '|', Compressibility::get_valid_percentages() );
        $msg .= "]\n";

        die $msg;
    }

    %gc = (
        target_physicaldrive    => $target_physicaldrive,
        target_volume           => $target_volume,
        target_file             => $target_file,
        raw_disk                => $raw_disk,
        clean_disk              => $clean_disk,
        create_new_filesystem   => $create_new_filesystem,
        create_new_file         => $create_new_file,
        initialize              => $initialize,
        precondition            => $precondition,
        recipe                  => $recipe,
        test_id                 => $test_id,
        test_time_override      => $test_time_override,
        warmup_time_override    => $warmup_time_override,
        io_generator            => $io_generator,
        io_generator_args       => $io_generator_args,
        active_range            => $active_range,
        partition_bytes         => $partition_bytes,
        quick_test              => $quick_test,
        output_dir              => $output_dir,
        results_share           => $results_share,
        results_share_user      => $results_share_user,
        results_share_pass      => $results_share_pass,
        compressibility         => $compressibility,
        collect_smart           => $collect_smart,
        collect_logman          => $collect_logman,
        collect_power           => $collect_power,
        keep_logman_raw         => $keep_logman_raw,
        force_ssd               => $force_ssd,
        start_on_step           => $start_on_step,
    );
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
  --force_ssd       Treat the target as an SSD, regardless of appearances.
  --start_on_step=n Begin testing on step n of the recipe.

EXAMPLES
  $script_name --target=1 
  $script_name --target=Z: --recipe=recipes\\my_new_recipe.rcp
  $script_name --target=Z:\\file.dat --nocollect_power

END_USAGE

    return $usage;
}

sub lock()
{
    lock_hash( %gc );
}

sub unlock()
{
    unlock_hash( %gc );
}

1;
