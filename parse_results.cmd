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
use feature qw( state switch );

use English;
use Cwd qw( cwd abs_path );
use Getopt::Long;
use File::Basename;
use Carp;
#$SIG{ __DIE__ } = sub { Carp::confess( @_ ) };
use List::Util qw( max min );
use List::MoreUtils qw( uniq zip );
use Time::Piece;

# ISSUE-REVIEW: this is already done by Util.pm?
my $script_name = basename( $PROGRAM_NAME );
my $script_dir = dirname( $PROGRAM_NAME );

use FindBin;
use lib "$FindBin::Bin";
use lib "$FindBin::Bin\\lib";

use Module::Load::Conditional 'can_load';

if( can_load( modules => { 'Excel::Writer::XLSX' => undef } ) )
{
    use Excel::Writer::XLSX;
}
else
{
    warn "$script_name requires the Excel::Writer::XLSX package from CPAN.\n";
    warn "Please install it to $FindBin::Bin\\lib\n\n";
    exit( -1 );
}

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

use DeviceDB;
use Util;
use SqlioParser;
use DiskSpdParser;
use PreconditionParser;
use LogmanParser;
use Power;
use SmartCtlParser;
use StorageToolParser;
use WmicParser;
use Endurance;

$OUTPUT_AUTOFLUSH = 1;

use constant KB_PER_GB => 1000 * 1000;

my $sanitize = 0;

my $outlier_method = 2;  # 1 = mean/stddev, 2 = median abs deviation

if( scalar @ARGV < 2 )
{
    print_usage();
    exit(-1);
}

GetOptions(
    "sanitize!"        => \$sanitize,
    "outlier_method=i" => \$outlier_method,
    "prompt!"          => \$prompt,
);

my $data_file_glob = "test*.txt";

sub print_usage
{
    print STDERR <<"END_USAGE";

USAGE 
  $script_name [options] {input dirs} {output file}

OPTIONS
  --sanitize    Omit vendor-specific details from report

OPERANDS
  input dirs    Space separated list of directories containing
                diskspd, sqlio or both output text files

  output file   File name for XLSX-format report (suffix optional)
   
EXAMPLES
  $script_name results\\input_dir report.xlsx
  $script_name results\\* report
  $script_name input_dir1 input_dir2 report

END_USAGE
}

sub parse_warmup_file($$)
{
    my $file_name = shift;
    my $stats_ref = shift;
        
    return 0 unless -e $file_name;

    my %warmup_stats;

    parse_test_file( $file_name, \%warmup_stats );

    # Inject into the main stats hash with "Warmup" prefix
    foreach my $key ( keys %warmup_stats )
    {
        $stats_ref->{"Warmup $key"} = $warmup_stats{$key};
    }
}

sub parse_background_file($$)
{
    my $file_name = shift;
    my $stats_ref = shift;
       
    unless( -e $file_name )
    {
        $stats_ref->{'Background Processes'} = "None";
        return;
    }

    open my $LOG, "<$file_name" 
        or die "Error opening $file_name";

    while( my $line = <$LOG> ) 
    {
        my $description = $line;
        <$LOG>; # Command line is currently ignored
        <$LOG>; # PID is currently ignored
        <$LOG>; # Blank line expected here

        if( defined $stats_ref->{'Background Processes'} )
        {
            $stats_ref->{'Background Processes'} .= ", ";
        }

        $stats_ref->{'Background Processes'} .= $description;
    }

    close $LOG;
}

sub parse_test_file($$)
{
    my $file_name = shift;
    my $stats_ref = shift;
            
    open my $LOG, "<$file_name" 
        or die "Error opening $file_name";

    # first line is always the command line
    my $cmd_line = <$LOG>;
        
    my $iogen;

    if( $cmd_line =~ /diskspd/i )
    {
        $stats_ref->{'IO Generator'} = "diskspd";
        $iogen = DiskSpdParser->new();

    }
    elsif( $cmd_line =~ /sqlio/i )
    {
        $stats_ref->{'IO Generator'} = "sqlio";
        $iogen = SqlioParser->new();
    }
    else
    {
        die "Unknown IO generator\n";
    }
        
    $iogen->parse_cmd_line( $cmd_line, $stats_ref );
    $iogen->parse( $LOG, $stats_ref );
        
    close $LOG;
}

sub compute_rw_amounts($)
{
    my $stats_ref = shift;

    my $total_IOs = $stats_ref->{'IOs Total'} // 0;
    my $total_GB = $total_IOs * $stats_ref->{'Access Size'} / KB_PER_GB;
    $stats_ref->{'GB Total'} = $total_GB;
    
    my $read_frac = $stats_ref->{'R Mix'} / 100;
    my $write_frac = $stats_ref->{'W Mix'} / 100;

    # The following are *estimates* (not measured)
    # If the workload generator measures this directly, do not overwrite

    unless( exists $stats_ref->{'IOs Read'} )
    {
        $stats_ref->{'IOs Read'}  = $total_IOs * $read_frac; 
        $stats_ref->{'Notes'} .= "IOs Read is an estimate; ";
    }

    unless( exists $stats_ref->{'IOs Write'} )
    {
        $stats_ref->{'IOs Write'} = $total_IOs * $write_frac; 
        $stats_ref->{'Notes'} .= "IOs Write is an estimate; ";
    }

    unless( exists $stats_ref->{'GB Read'} )
    {
        $stats_ref->{'GB Read'}  = $total_GB * $read_frac; 
        $stats_ref->{'Notes'} .= "GB Read is an estimate; ";
    }

    unless( exists $stats_ref->{'GB Write'} )
    {
        $stats_ref->{'GB Write'} = $total_GB * $write_frac; 
        $stats_ref->{'Notes'} .= "GB Write is an estimate; ";
    }
}

sub compute_steady_state_error_and_warn($)
{
    my $stats_ref = shift;

    my $warmup_IOPS = $stats_ref->{'Warmup IOPS Total'};
    my $test_IOPS = $stats_ref->{'IOPS Total'};
 
    return unless defined $warmup_IOPS;

    my $abs_diff = abs( $warmup_IOPS - $test_IOPS );
    my $max_abs = max( abs( $warmup_IOPS ), abs( $test_IOPS ) );

    return unless $max_abs > 0;

    my $rel_diff = $abs_diff / $max_abs * 100; 

    # Complain if the relative difference is greater than 10%
    unless( $rel_diff <= 10 )
    {
        my $test_desc = $stats_ref->{'Test Description'};
        warn "\tPrecondition didn't achieve steady-state? $test_desc\n";

        $stats_ref->{'Notes'} .= "No steady-state?; ";
    }

    $stats_ref->{'Steady-State Error'} = $rel_diff;
}

sub get_timestamp_from_smart_file($)
{
    my $file_name = shift;
    
    open my $LOG, "<$file_name" or 
        die "Error opening $file_name\n";

    my $timestamp;

    while( my $line = <$LOG> )
    {
            chomp $line;

        if( $line =~ /Local Time is:\s+(.+)/ )
        {
            # truncate to remove timezone
            $timestamp = substr( $1, 0, -4 );

            last;
        }
    }

    close $LOG;

    return $timestamp;
}

sub generate_timestamp($$)
{
    my $base_name = shift;
    my $stats_ref = shift;

    my $smart_file = "smart-before-$base_name.txt";

    my $timestamp;

    # Prefer the timestamp in the smart file
    if( -e $smart_file )
    {
        $timestamp = get_timestamp_from_smart_file( $smart_file );
    }

    # Fall back to the test file's modified time 
    unless( defined $timestamp ) 
    {
        $timestamp = get_file_modified_time( "test-$base_name.txt" ); 
    }

    $stats_ref->{'Timestamp'} =
        Time::Piece->strptime( $timestamp, '%a %b %d %H:%M:%S %Y' );
}

my @cols = 
(
    { name => 'Display Name'},
    { name => 'User Capacity (GB)' },
    { name => 'Partition Size (GB)' },
    { name => 'Test Description' },
    {
        name   => 'Timestamp',
        format => 'mm/dd/yyyy HH:mm:ss',
    },
    { name => 'Test Ordinal' },
    { name => 'Steady-State Time' },
    {
        name   => 'Steady-State Error',
        format => '0%',
    },
    {
        name   => 'R Mix',
        format => '0%',
        type   => 'io_pattern'
    },
    {
        name   => 'W Mix',
        format => '0%',
        type   => 'io_pattern'
    },
    { 
        name   => 'Access Size',
        type   => 'io_pattern'
    },
    {
        name   => 'Alignment',
    },
    {
        name   => 'Access Type',
        type   => 'io_pattern'
    },
    {
        name   => 'QD',
        type   => 'io_pattern'
    },
    {   
        name   => 'Compressibility',
        format => '0%',
        type   => 'io_pattern'
    },
        
    {
        name   => "Warmup MB/sec Total",
        format => '#,##0.00',
    },
    { name => 'Background Processes' },
);

sub generate_cols_section($)
{
    my $suffix = shift;

    return (
        {
            name   => "MB/sec $suffix",
            format => '#,##0.00',
            sense  => 'bigger is better',
        },
        {
            name   => "IOPS $suffix",
            format => '#,##0.00',
            sense  => 'bigger is better',
        },
        { 
            name   => "IOs $suffix",
            format => '#,##0',
            sense  => 'bigger is better',
        },
        { 
            name   => "GB $suffix",
            format => '#,###.###',
            sense  => 'bigger is better',
        },
        {
            name   => "Min Latency $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "Avg Latency $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "50th Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "90th Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "95th Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "2-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "3-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "4-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "5-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "6-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "7-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "8-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "9-nines Percentile $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
        {
            name   => "Max Latency $suffix",
            format => '#,##0.00',
            sense  => 'smaller is better',
        },
    );
}

push @cols, generate_cols_section( 'Total' );
push @cols, generate_cols_section( 'Read' );
push @cols, generate_cols_section( 'Write' );

push @cols,
(
    {
        name    => 'DWPD 3yr',
        format  => '#.#',
    },
    {
        name    => 'DWPD 5yr',
        format  => '#.#',
    },
    {
        name   => 'CPU Util',
        format => '0%',
    },
    {
        name   => 'Measured QD',
        format => '#,##0.00',
    },
    {
        name   => 'System Power (W)',
        format => '#,##0.00',
    },
    { 
        name   => 'Driver Memory (MB)',
        format => '#,##0.00',
    },
    {   name   => 'Driver Resident Memory (MB)',
        format => '#,##0.00',
    },
    { name => 'IO Generator' },
    { name => 'Smart Collector' },
);
   
# don't want 3rd parties to see these
push @cols,
(
    { name => 'Raw Capacity (GiB)' },
    { 
        name => 'Overprovisioning',
        format => '#.#%',
    },
    {
        name   => 'Wear Range Before',
        format => '#.#',
    },
    {
        name   => 'Wear Range After',
        format => '#.#',
    },
    {
        name   => 'Filesystem Write Amplification',
        format => '#.##',
    },
    {
        name   => 'Drive Write Amplification',
        format => '#.##',
    },
    {
        name   => 'Total Write Amplification',
        format => '#.##',
    },
    {
        name    => 'Rated PE Cycles',
        format  => '#,###',
    },
    {
        name    => 'NAND Writes (GB)',
        format  => '#,###.00',
    },
    {
        name    => 'NAND Write BW (MB/sec)',
        format  => '#,##0.00',
    },
    { name => 'Log File' },
    { name => 'Directory' },
    { name => 'Unique Device ID' },
    { name => 'Friendly Name'},
    { name => 'Device Model' },
    { name => 'Model Family' },
    { name => 'Firmware Version' },
    {
        name    => 'Serial Number',
        protect => 1,
    },
    { name => 'Notes' },
) 
unless $sanitize;

sub generate_probable_partition_size($)
{
    my $stats_ref = shift;

    # In the old code we never captured the partition size, but
    # it was usually equal to the user capacity.  We make that
    # assumption here.
    $stats_ref->{'Partition Size (GB)'} =
        $stats_ref->{'User Capacity (GB)'}
            unless defined $stats_ref->{'Partition Size (GB)'};

    $stats_ref->{'Partition Size (B)'} =
        $stats_ref->{'User Capacity (B)'}
            unless defined $stats_ref->{'Partition Size (B)'};
}

sub by_timestamp($$)
{
    my $a = shift;
    my $b = shift;

    return $a->{'Timestamp'} <=> $b->{'Timestamp'};
}

sub parse_directories(@)
{
    my @input_dirs = @_;

    my @all_devices;

    my $directory_number = 0;

    foreach my $dir ( @input_dirs )
    {
        chdir( $dir );

        my $subdir = basename( $dir );
        
        print "Processing $subdir\n";

        my @files = glob( $data_file_glob );

        next unless scalar @files > 0;

        my %dir_stats;

        my $try_precondition = 1;
        my $try_smart_attr = 1;
        my $try_logman = 1;
        my $try_power = 1;

        my $smart;
        my $smart_file = 'smart.txt';
        $dir_stats{'Smart Collector'} = "unavailable";
        if( -e $smart_file )
        {
            open my $SMART, "<$smart_file"
                or die "Error opening $smart_file";

            my $cmd_line = <$SMART>;
            if( $cmd_line =~ /smartctl/i )
            {
                $dir_stats{'Smart Collector'} = "smartctl";
                $smart = SmartCtlParser->new();
            }
            elsif( $cmd_line =~ /StorageTool/i )
            {
                $dir_stats{'Smart Collector'} = "StorageTool";
                $smart = StorageToolParser->new();
            }
            else
            {
                die "Unknown SMART collector\n";
            }

            close $SMART;

            $smart->parse_info( \%dir_stats, $smart_file );
        }
        else
        {
            $try_smart_attr = 0;
        }
        
        if( -e 'wmic.txt' )
        {
            my $wmic = WmicParser->new();
            $wmic->parse( \%dir_stats, 'wmic.txt' );
        }

        generate_probable_partition_size( \%dir_stats );

        $dir_stats{'Directory'} = "external:$dir";
        $dir_stats{'Unique Device ID'} = $directory_number;
         
        my @current_device;

        foreach my $test_file_name ( glob( $data_file_glob ) )
        {
            my $base_name =
                $test_file_name =~ s/(test-|\.txt)//rg;

            my %file_stats = %dir_stats;
            
            $file_stats{'Log File'} =
                "external:$dir\\$test_file_name";
            
            $file_stats{'Test Description'} = $base_name;

            generate_timestamp( $base_name, \%file_stats );

            if( -e "wmic-$base_name.txt" )
            {
                my $wmic = WmicParser->new();
                $wmic->parse( \%file_stats, "wmic-$base_name.txt" );
            }
    
            parse_warmup_file(
                "warmup-$base_name.txt",
                \%file_stats
            );

            parse_background_file(
                "background-$base_name.txt",
                \%file_stats
            );

            parse_test_file(
                $test_file_name,
                \%file_stats
            );

            compute_rw_amounts( \%file_stats );
  
            compute_steady_state_error_and_warn( \%file_stats );

            if( $try_precondition )
            {
                my $pc = PreconditionParser->new();

                $pc->parse(
                    "precondition-$base_name.txt",
                    \%file_stats
                )
                or $try_precondition = 0;
            }

            if( $try_smart_attr )
            {
                my $success = 1;

                $success &= 
                    $smart->parse_attributes( 
                        "smart-before-$base_name.txt",
                        ' Before',
                        \%file_stats
                    );

                $success &= 
                    $smart->parse_attributes( 
                        "smart-after-$base_name.txt",
                        ' After',
                        \%file_stats
                    ) if $success;

                if( $success )
                {
                    compute_endurance( \%file_stats );
                }
                else
                {
                    $try_smart_attr = 0;
                }
            }

            if( $try_logman )
            {
                my $logman = LogmanParser->new();
               
                $logman->parse(
                    "logman-pruned-$base_name.csv",
                    \%file_stats
                )
                or $try_logman = 0;
            }

            if( $try_power )
            {
                my $power = Power->new(
                    output_dir  => $dir,
                    description => $file_stats{'Test Description'},
                );

                $power->parse( \%file_stats ) or $try_power = 0;
            }

            push @current_device, \%file_stats;
        }

        my $ordinal = 0;

        foreach my $stats_ref ( sort by_timestamp @current_device )
        {
            $stats_ref->{'Test Ordinal'} = $ordinal++;
            push @all_devices, $stats_ref;
        }

        $directory_number++;
    }
        
    return @all_devices;
}

# When extracting, we store a statistic's value directly in a hash.
# Later, we might want to track other information related to that 
# statistic.  For example, outlier status, or formatting rules. To
# make this possible, but still enable straight-forward extraction,
# we run this helper function post-extraction to create a 'value'
# namespace, making room for other things.
sub inject_value_namespace($)
{
    # BEFORE:
    #   {
    #     'stat name 1' => 'value 1',
    #     'stat name 2' => 'value 2'
    #   };
    #
    # AFTER:
    #   {
    #     'stat name 1' => { 'value' => 'value 1' },
    #     'stat name 2' => { 'value' => 'value 2' }
    #   };

    my $stats_ref = shift;
    
    foreach my $key ( keys %$stats_ref )
    {
        my $value = $stats_ref->{$key};
        delete $stats_ref->{$key};

        $stats_ref->{$key}{'value'} = $value;
    }
}

sub get_io_pattern_cols()
{
    return 
        map { $_->{'name'} }
        grep { defined $_->{'type'} and $_->{'type'} eq 'io_pattern' } 
        @cols;
}

my %test_description_to_io_pattern;

sub build_test_description_to_io_pattern_hash(@)
{
    my @all_stats = @_;
   
    foreach my $stats_ref ( @all_stats )
    {
        my $test_desc = $stats_ref->{'Test Description'};

        next if defined $test_description_to_io_pattern{$test_desc};

        my %fields =
            map { $_ => $stats_ref->{$_} } get_io_pattern_cols(); 

        $test_description_to_io_pattern{$test_desc} = \%fields
    }
}

sub get_friendly_names(@)
{
    my @all_stats = @_;

    foreach my $stats_ref ( @all_stats )
    {
        my $device_model = $stats_ref->{'Device Model'};

        $stats_ref->{'Friendly Name'} =
            $device_db{$device_model}{'Friendly Name'} // $device_model;
    }
}

sub inject_default_compressibility(@)
{
    my @all_stats = @_;

    my %all_compress;

    foreach my $stats_ref ( @all_stats )
    {
        my $directory = $stats_ref->{'Directory'};
        my $compress = $stats_ref->{'Compressibility'};

        $all_compress{$directory}{$compress}++;
    }
  
    my %default_compress;

    foreach my $directory ( keys %all_compress )
    {
        my %compress = %{ $all_compress{$directory} };

        my @descending_by_frequency =
            sort { $compress{$b} <=> $compress{$a} } keys %compress;

        my $most_common = $descending_by_frequency[0];

        $default_compress{$directory} = $most_common;
    }

    foreach my $stats_ref ( @all_stats )
    {
        my $directory = $stats_ref->{'Directory'};
        
        $stats_ref->{'Default Compressibility'} =
            $default_compress{$directory};
    }
}

sub display_names_are_unique(@)
{
    my @all_stats = @_;
   
    my %dn_hash;

    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};
        my $directory = $stats_ref->{'Directory'};
        
        $dn_hash{$display_name}{$directory} = 1;
    }
    
    foreach my $display_name ( sort keys %dn_hash )
    {
        my %directories = %{ $dn_hash{$display_name} };

        return 0 if keys %directories > 1;
    }

    return 1;
}

sub try_suffix_column
{
    my %args = @_;
    
    my $name = $args{'name'} // die;
    my $all_stats_ref = $args{'all_stats_ref'} // die;
    my $is_percentage = $args{'is_percentage'} // 0;
    
    my @all_stats = @{ $all_stats_ref };

    my %all_vals;

    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};
        my $val = $stats_ref->{$name};
        
        $all_vals{$display_name}{$val} = 1;
    }

    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};

        my $multiple_distinct_vals_present =
            keys %{ $all_vals{$display_name} } > 1;
            
        if( $multiple_distinct_vals_present )
        {
            my $val = $stats_ref->{$name};
       
            $val .= '%' if $is_percentage;
            
            $stats_ref->{'Display Name'} .= " - $val"
        }
    }
}

sub suffix_ordinal(@)
{
    my @all_stats = @_;
    
    my %ordinals;
    
    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};
        my $directory = $stats_ref->{'Directory'};
        
        $ordinals{$display_name}{$directory} = undef;
    }
    
    foreach my $display_name ( sort keys %ordinals )
    {
        my %directories = %{ $ordinals{$display_name} };

        next unless keys %directories > 1;

        my $ordinal = 1;

        foreach my $directory ( sort keys %directories )
        {
            $ordinals{$display_name}{$directory} = $ordinal++;
        }
    }
        
    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};
        my $directory = $stats_ref->{'Directory'};

        my $ordinal = $ordinals{$display_name}{$directory};

        next unless defined $ordinal;
        
        $stats_ref->{'Display Name'} .= " - $ordinal";
    }
}

sub build_display_names(@)
{
    my @all_stats = @_;
    
    foreach my $stats_ref ( @all_stats )
    {
        $stats_ref->{'Display Name'} = $stats_ref->{'Friendly Name'};
    }

    return if display_names_are_unique( @all_stats );

    # First try to disambiguate by suffixing a FW version
    try_suffix_column(
        name => 'Firmware Version',
        all_stats_ref => \@all_stats
    );

    return if display_names_are_unique( @all_stats );
   
    # If that wasn't enough, try suffixing the test's default compressibility
    try_suffix_column(
        name => 'Default Compressibility',
        is_percentage => 1,
        all_stats_ref => \@all_stats
    );

    return if display_names_are_unique( @all_stats );
   
    # As a last resort, suffix a simple ordinal.
    # This should *always* produce unique display names.
    suffix_ordinal( @all_stats );
    
    display_names_are_unique( @all_stats ) or die;
}

my %display_name_to_sanitized_name;

sub sanitize_display_names(@)
{
    my @all_stats = @_;
    
    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};

        my $sanitized_name =
            $display_name_to_sanitized_name{$display_name};

        unless( defined $sanitized_name )
        {
            state $letter = 'A';
            $sanitized_name = "Device $letter";

            $display_name_to_sanitized_name{$display_name}
                = $sanitized_name;

            $letter++;
        }

        $stats_ref->{'Display Name'} = $sanitized_name;
    }
}

sub print_sanitization_decoder_ring()
{
    print "Sanitization Decoder Ring:\n";

    foreach my $display_name ( sort keys %display_name_to_sanitized_name )
    {
        my $sanitized_name =
            $display_name_to_sanitized_name{$display_name};

        print "\t$display_name --> $sanitized_name\n";
    }
}

my %device_id_to_display_name;

sub build_device_id_to_display_name(@)
{
    my @all_stats = @_;
    
    foreach my $stats_ref ( @all_stats )
    {
        my $display_name = $stats_ref->{'Display Name'};
        my $device_id = $stats_ref->{'Unique Device ID'};

        $device_id_to_display_name{$device_id} = $display_name;
    }
}

my @unique_device_ids;
my @unique_test_descriptions;
my @common_test_descriptions;

sub post_process_stats(\@)
{
    my @all_stats = @{+shift};
   
    build_test_description_to_io_pattern_hash( @all_stats );
    get_friendly_names( @all_stats );
    inject_default_compressibility( @all_stats );
    build_display_names( @all_stats );
    sanitize_display_names( @all_stats ) if $sanitize;
    build_device_id_to_display_name( @all_stats );

    @unique_device_ids = 
        uniq map { $_->{'Unique Device ID'} } @all_stats;

    my %test_description_occurrences;
   
    foreach my $stats_ref ( @all_stats )
    {
        my $test_desc = $stats_ref->{'Test Description'};
        $test_description_occurrences{$test_desc}++;
    }
    
    @unique_test_descriptions = keys %test_description_occurrences;

    foreach my $test_desc ( @unique_test_descriptions )
    {
        if( $test_description_occurrences{$test_desc} ==
            scalar @unique_device_ids )
        {
            push @common_test_descriptions, $test_desc;
        }
    }

    my $expected_io_gen = $all_stats[0]->{'IO Generator'};

    foreach my $stats_ref ( @all_stats )
    {
        unless( $stats_ref->{'IO Generator'} eq $expected_io_gen )
        {
            warn "Inconsistent IO generator detected\n";
        }
    }

    foreach my $stats_ref ( @all_stats )
    {
        inject_value_namespace( $stats_ref );
    }
}

sub median_abs_deviation($\@)
{
    my $median = shift;
    my @values = @{+shift};

    # absolute deviations from the median
    my @abs_dev = map { abs( $_ - $median ) } @values;

    return median( @abs_dev );
}

sub compute_outlier_bounds($$\@)
{
    my $test_desc = shift;
    my $col_name = shift;
    my @values = @{+shift};

    my $lower;
    my $upper;

    if( $outlier_method == 1 )
    {
        # use mean and standard deviation
        
        my $mean = mean( @values );
        my $std_dev = stddev( @values );

        # values more than 2 std dev from mean are outliers
        $upper = $mean + ( 2 * $std_dev );
        $lower = $mean - ( 2 * $std_dev );
    }
    elsif( $outlier_method == 2 )
    {
        # use median and median-absolute-deviation
        
        my $median = median( @values );
        my $mad = median_abs_deviation( $median, @values );
        
        # values more than 3 MAD from median are outliers
        $upper = $median + ( 3 * $mad );
        $lower = $median - ( 3 * $mad );
    }
    else
    {
        die "Unknown outlier method: $outlier_method\n";
    }

    return ( $lower, $upper );
}

sub filter_by_test_description($\@)
{
    my $test_desc = shift;
    my @all_stats = @{+shift};

    return
        grep { $_->{'Test Description'}{'value'} eq $test_desc } @all_stats;
}

sub get_matching_values($$\@)
{
    my $test_desc = shift;
    my $col_name = shift;
    my @all_stats = @{+shift};

    my @matching = filter_by_test_description( $test_desc, @all_stats );

    return 
        grep { defined $_ } # prune undefined vals
        map { $_->{ $col_name }{'value'} } @matching;
}

sub is_good_outlier($$$$)
{
    my $val = shift;
    my $lower = shift;
    my $upper = shift;
    my $sense = shift;

    return (
        ( ( $val > $upper ) and ( $sense eq 'bigger is better' ) ) 
        or
        ( ( $val < $lower ) and ( $sense eq 'smaller is better' ) )
    );
}

sub is_bad_outlier($$$$)
{
    my $val = shift;
    my $lower = shift;
    my $upper = shift;
    my $sense = shift;

    return (
        ( ( $val > $upper ) and ( $sense eq 'smaller is better' ) ) 
        or
        ( ( $val < $lower ) and ( $sense eq 'bigger is better' ) )
    );
}

sub find_outliers($\@)
{
    my $workbook = shift;
    my @all_stats = @{+shift};
   
    my %outliers;

    foreach my $test_desc ( @unique_test_descriptions )
    {
        foreach my $col ( @cols )
        {
            my $col_name  = $col->{'name'};
            my $col_sense = $col->{'sense'};

            next unless defined $col_sense; 

            my @values = 
                get_matching_values( $test_desc, $col_name, @all_stats );

            next unless scalar @values > 0;

            my ( $lower, $upper ) = 
                compute_outlier_bounds( $test_desc, $col_name, @values );
           
            foreach my $test_stats ( 
                filter_by_test_description( $test_desc, @all_stats ) )
            {
                my $val = $test_stats->{$col_name}{'value'};
                
                next unless defined $val;

                my $device_id = $test_stats->{'Unique Device ID'}{'value'};
 
                $outliers{$device_id}{$test_desc}{$col_name} = 'good'
                    if is_good_outlier( $val, $lower, $upper, $col_sense );

                $outliers{$device_id}{$test_desc}{$col_name} = 'bad'
                    if is_bad_outlier( $val, $lower, $upper, $col_sense );
            }
        }
    }

    return %outliers;
}

# helper to lazily create Excel::Writer::XLSX format objects
sub get_format_obj($$$)
{
    my $workbook = shift;
    my $test_stats = shift;
    my $col_name = shift;

    my $format_obj = $test_stats->{$col_name}{'format'};

    unless( defined $format_obj )
    {
        $format_obj = $workbook->add_format();
        $test_stats->{$col_name}{'format'} = $format_obj;
    }
    
    return $format_obj;
}

sub hilight_outliers($\%\@)
{
    my $workbook = shift;
    my %outliers = %{+shift};
    my @all_stats = @{+shift};
        
    my $good_text_color = $workbook->set_custom_color( 40, 0, 97, 0 );
    my $good_bg_color   = $workbook->set_custom_color( 41, 198, 239, 206 );

    my $bad_text_color = $workbook->set_custom_color( 42, 156, 0, 6 );
    my $bad_bg_color   = $workbook->set_custom_color( 43, 255, 199, 206 );

    foreach my $test_stats ( @all_stats )
    {
        my $test_desc = $test_stats->{'Test Description'}{'value'};
        my $device_id = $test_stats->{'Unique Device ID'}{'value'};
        foreach my $col ( @cols )
        {
            my $col_name  = $col->{'name'};
            
            my $format_obj =
                get_format_obj( $workbook, $test_stats, $col_name );

            given( $outliers{$device_id}{$test_desc}{$col_name} )
            {
                $format_obj->set_format_properties(
                    color    => $good_text_color,
                    bg_color => $good_bg_color,
                    pattern  => 1
                ) when 'good';

                $format_obj->set_format_properties(
                    color    => $bad_text_color,
                    bg_color => $bad_bg_color,
                    pattern  => 1
                ) when 'bad';
            }
        }
    }
}

# When Excel sees a string with numbers and the letter 'E', it will
# be interpreted as a number in exponential notation.  This cannot
# be fixed with cell formatting.  This function prevents the issue.
sub protect_excel_string($)
{
    my $str = shift;
    return qq(="$str");
}

sub generate_raw_data_sheet($\@)
{
    my $workbook = shift;
    my @all_stats = @{+shift};

    my $raw_sheet = $workbook->add_worksheet( 'Raw Data' );

    my $header_format = 
        $workbook->add_format( 
            bold     => 1,
            rotation => 45,
        );

    my @col_names = map { $_->{'name'} } @cols;

    $raw_sheet->write_row( 'A1', \@col_names, $header_format );

    my $row_num = 1; # offset for header

    foreach my $test_stats ( @all_stats )
    {
        my $col_num = 0;

        foreach my $col ( @cols )
        {
            my $col_name = $col->{'name'};
            my $format_str = $col->{'format'};
            
            my $value = $test_stats->{$col_name}{'value'};
                
            if( defined $col->{'protect'} )
            {
                $value = protect_excel_string( $value );
            }

            # Convert Time::Piece object to Excel date format
            if( ref $value eq 'Time::Piece' )
            {
                $value = unix_date_to_excel_date( $value->epoch )
            }

            my $format_obj;

            if( defined $format_str )
            {
                $format_obj =
                    get_format_obj( $workbook, $test_stats, $col_name );

                $format_obj->set_num_format( $format_str );
           
                # Our percentages are 0 to 100. Excel prefers 0.0 to 1.0.
                $value /= 100 if defined $value and $format_str =~ /%/;

            }

            $raw_sheet->write( $row_num, $col_num, $value, $format_obj );

            $col_num++;
        }

        $row_num++;
    }

    $raw_sheet->autofilter( 0, 0, 0, $#cols );
    $raw_sheet->freeze_panes( 1, 0 );
}

sub scores_hash_is_valid(\%)
{
    my %scores = %{+shift};

    # We should see one value per device
    my $expected_num_values = scalar @unique_device_ids;

    my $failed = 0;
    foreach my $test_desc ( keys %scores )
    {
        foreach my $metric ( keys %{ $scores{$test_desc} } )
        {
            my $num_values = values %{ $scores{$test_desc}{$metric} };

            if( $num_values != $expected_num_values )
            {
                my $ratio = "$num_values/$expected_num_values";
                warn "Unexpected number of values ($ratio): $test_desc, $metric\n";

                $failed = 1;
            }
        }
    }

    return $failed;
}

sub extract_raw_scores(\@)
{
    my @all_stats = @{+shift};

    my %raw_scores;

    my @skipped;

    foreach my $test_stats ( @all_stats )
    {
        my $test_desc = $test_stats->{'Test Description'}{'value'};

        unless( $test_desc ~~ @common_test_descriptions )
        {
            push @skipped, $test_desc
                unless $test_desc ~~ @skipped;

            next;
        }

        my $device_id = $test_stats->{'Unique Device ID'}{'value'};
        
        foreach my $col ( @cols )
        {
            next unless defined $col->{'sense'};
            
            my $metric = $col->{'name'};

            my $value = $test_stats->{$metric}{'value'};

            # Every metric doesn't make sense for every test
            $raw_scores{$test_desc}{$metric}{$device_id} = $value
                if defined $value;
        }
    }

    my $num_skipped = scalar @skipped;
    my $num_total = $num_skipped + scalar @common_test_descriptions;

    unless( $num_skipped == 0 )
    {
        my $msg;

        $msg .= "Warning: not all devices ran all tests. ";
        $msg .= "Skipped scoring $num_skipped/$num_total tests.\n";

        warn $msg;
    }

    return %raw_scores;
}

# Compute standard scores, or "z-scores".
# This is simply the number of standard deviations from the mean.
sub compute_std_scores(\%)
{
    my %raw_scores = %{+shift};

    my %std_scores;

    foreach my $test_desc ( keys %raw_scores )
    {
        foreach my $metric ( keys %{ $raw_scores{$test_desc} })
        {
            my @values = values %{ $raw_scores{$test_desc}{$metric} };

            my $mean = mean( @values );
            my $std_dev = stddev( @values );

            foreach my $device_id ( keys %{ $raw_scores{$test_desc}{$metric} })
            {
                my $value = $raw_scores{$test_desc}{$metric}{$device_id}; 
           
                my $std_score;

                if( $std_dev == 0 )
                {
                    $std_score = 0;
                }
                else
                {
                    $std_score = ( $value - $mean ) / $std_dev;
                }

                $std_scores{$test_desc}{$metric}{$device_id} = $std_score;
            }
        }
    }

    return %std_scores;
}

sub get_column_sense($)
{
    my $col_name = shift;
    
    my ($col) = grep { $_->{'name'} eq $col_name } @cols;
    
    return $col->{'sense'};
}

sub negate(\$)
{
    my $val_ref = shift;
    
    $$val_ref = -$$val_ref;
}

sub make_bigger_better(\%)
{
    my $href = shift;

    foreach my $test_desc ( keys %$href )
    {
        foreach my $metric ( keys %{ $href->{$test_desc} })
        {
            my $sense = get_column_sense( $metric );

            if( $sense =~ /smaller is better/ )
            {
                foreach my $device_id ( keys % { $href->{$test_desc}{$metric} })
                {
                    negate( $href->{$test_desc}{$metric}{$device_id} );
                }
            }
        }
    }
}

sub scale($$$$$)
{
    my $val = shift;

    my $old_min = shift;
    my $old_max = shift;
    my $new_min = shift;
    my $new_max = shift;
    
    my $old_range = $old_max - $old_min;
    my $new_range = $new_max - $new_min;

    if( $old_range == 0 )
    {
        return ( $new_range / 2 );
    }

    return ( ( ( $val - $old_min ) * $new_range ) / $old_range ) + $new_min;
}

# Normalize scores to the range 0 to 100.
#   Worst score always receives a 0.
#   Best score always receives a 100.
sub normalize_scores(\%)
{
    my %std_scores = %{+shift};

    my %normalized_scores;

    foreach my $test_desc ( keys %std_scores )
    {
        foreach my $metric ( keys %{ $std_scores{$test_desc} })
        {
            my @values = values %{ $std_scores{$test_desc}{$metric} };
     
            my $min = min( @values );
            my $max = max( @values );
            
            foreach my $device_id ( keys %{ $std_scores{$test_desc}{$metric} })
            {
                my $value = $std_scores{$test_desc}{$metric}{$device_id}; 

                $normalized_scores{$test_desc}{$metric}{$device_id} = 
                    scale( $value, $min, $max, 0, 100 );
            }
        }
    }

    return %normalized_scores;
}

# "metrics" are just the columns with the "sense" property defined
sub get_metrics()
{
    return map { $_->{'name'} } grep { defined $_->{'sense'} } @cols;
}

sub compute_weights_as_fractions(\%)
{
    my %policies = %{+shift};

    my %weights_as_fractions;

    my @metrics = get_metrics();

    foreach my $policy ( keys %policies )
    {
        my $total_weight = 0;
        
        my $weight_func = $policies{$policy};
        
        foreach my $test_desc ( @unique_test_descriptions )
        {
            my %io_pattern = %{$test_description_to_io_pattern{$test_desc}};

            foreach my $metric ( @metrics )
            {
                $total_weight += $weight_func->( $metric, %io_pattern );
            }
        }
       
        foreach my $test_desc ( @unique_test_descriptions )
        {
            my %io_pattern = %{$test_description_to_io_pattern{$test_desc}};

            foreach my $metric ( @metrics )
            {
                my $weight = $weight_func->( $metric, %io_pattern );
                
                my $fraction = 0;

                $fraction = $weight / $total_weight 
                    unless $total_weight == 0;

                $weights_as_fractions
                    {$policy}
                    {$test_desc}
                    {$metric} = $fraction;
            }
        }
    }
    
    return %weights_as_fractions;
}

sub apply_weights_as_fractions(\%\%)
{
    my %scores = %{+shift};
    my %weights_as_fractions = %{+shift};
   
    my %weighted_score_components;

    foreach my $policy ( keys %weights_as_fractions )
    {
        foreach my $test_desc ( keys %scores )
        {
            foreach my $metric ( keys %{ $scores{$test_desc} } )
            {
                foreach my $device_id ( 
                    keys %{ $scores{$test_desc}{$metric} } )
                {
                    my $weight = 
                        $weights_as_fractions{$policy}{$test_desc}{$metric};

                    my $nrml_score = 
                        $scores{$test_desc}{$metric}{$device_id}; 

                    $weighted_score_components
                        {$policy}
                        {$test_desc}
                        {$metric}
                        {$device_id} = $weight * $nrml_score;
                }
            }
        }
    }
    
    return %weighted_score_components;
}

sub combine_scores(\%)
{
    my %weighted_score_components = %{+shift};

    my %final_scores;

    foreach my $policy ( keys %weighted_score_components )
    {
        foreach my $test_desc ( keys %{ $weighted_score_components{$policy} } )
        {
            foreach my $metric (
                keys %{ $weighted_score_components{$policy}{$test_desc} } )
            {
                foreach my $device_id (
                    keys
                        %{ $weighted_score_components
                        {$policy}
                        {$test_desc}
                        {$metric} })
                {
                    my $weighted_score = 
                        $weighted_score_components
                            {$policy}
                            {$test_desc}
                            {$metric}
                            {$device_id}; 
                    
                    $final_scores{$device_id}{$policy} += $weighted_score;
                }
            }
        }
    }

    return %final_scores;
}

sub generate_scores_sheets($\%\%\%\%\%\%)
{
    my $workbook = shift;
    my %raw_scores = %{+shift};
    my %std_scores = %{+shift};
    my %normalized_scores = %{+shift};
    my %weighted_score_components = %{+shift};
    my %final_scores = %{+shift};
    my %weights_as_fractions = %{+shift};

    my $final_sheet =
        $workbook->add_worksheet( 'Final Scores' );

    my $header_format = 
        $workbook->add_format( 
            bold     => 1,
            rotation => 45,
        );
    
    my @header = (
        'Display Name',
        map { "$_ Score" } reverse sort keys %weights_as_fractions
    );
    
    $final_sheet->write_row( 'A1', \@header, $header_format );

    my $row = 1; # offset for header

    foreach my $device_id ( keys %final_scores )
    {
        my $display_name = $device_id_to_display_name{$device_id};

        my @row_data = ( $display_name );

        foreach my $policy ( reverse sort keys %weights_as_fractions ) 
        {
            my $score = $final_scores{$device_id}{$policy};
            push @row_data, $score;
        }

        $final_sheet->write_row( $row, 0, \@row_data );

        $row++;
    }
    
    $final_sheet->autofilter( 0, 0, 0, $#header );
    $final_sheet->freeze_panes( 1, 0 );

    my $detail_sheet = 
        $workbook->add_worksheet( 'Score Details' );

    my @weight_header = 
        map { "$_ Weight" } reverse sort keys %weights_as_fractions;
    
    my @weighted_score_header = 
        map { "$_ Score" } reverse sort keys %weights_as_fractions;

    @header = (
        'Display Name',
        'Test Description',
        get_io_pattern_cols(),
        'Metric',
        'Raw',
        'Standard',
        'Normalized',
        zip( @weight_header, @weighted_score_header )
    );  
    
    $detail_sheet->write_row( 'A1', \@header, $header_format );

    $row = 1; # offset for header

    foreach my $test_desc ( keys %normalized_scores )
    {
        foreach my $metric ( keys %{ $normalized_scores{$test_desc} })
        {
            foreach my $device_id (
                keys %{ $normalized_scores{$test_desc}{$metric} })
            {
                my @row_data = (
                    $device_id_to_display_name{$device_id},
                    $test_desc
                );

                my %io_pattern =
                    %{$test_description_to_io_pattern{$test_desc}};

                push @row_data, @io_pattern{ get_io_pattern_cols() }; 

                push @row_data, (
                    $metric,
                    $raw_scores{$test_desc}{$metric}{$device_id},
                    $std_scores{$test_desc}{$metric}{$device_id},
                    $normalized_scores{$test_desc}{$metric}{$device_id},
                );
        
                foreach my $policy ( reverse sort keys %weights_as_fractions ) 
                {
                    my $fraction = 
                        $weights_as_fractions{$policy}{$test_desc}{$metric};
                    
                    my $weighted_score_component = 
                        $weighted_score_components
                            {$policy}
                            {$test_desc}
                            {$metric}
                            {$device_id};

                    push @row_data, ( $fraction, $weighted_score_component );
                }
        
                $detail_sheet->write_row( $row, 0, \@row_data );

                $row++;
            }
        }
    }
    
    $detail_sheet->autofilter( 0, 0, 0, $#header );
    $detail_sheet->freeze_panes( 1, 0 );
}

sub get_outfile_name($)
{
    my $arg = shift;

    $arg .= ".xlsx" unless  $arg =~ /\.xlsx$/;

    return $arg;
}

sub get_input_dirs(@)
{
    my @dirs;

    # Build list of directories to process.
    # Expand wildcards and canonicalize to absolute paths.
    foreach my $arg ( map { glob } @_ )
    {
        next unless( -d $arg );
        push @dirs, abs_path( $arg );
    }

    return @dirs;
}

sub get_general_weight
{
    my $metric = shift;
    my %io_pattern = @_;

    # ISSUE-REVIEW:
    # Should we filter out the smart tests?
    # return 0 if $test_desc =~ /Background/;

    # We extract 23 raw metrics, but only 3 of them measure throughput.
    # Ignore most of them to undo this latency bias.

    return 2 if $metric eq "MB/sec Total";
    return 1 if $metric eq "50th Percentile Total";
    return 1 if $metric eq "5-nines Percentile Total";

    return 0; # Ignore by default.
}

sub get_azure_weight
{
    my $metric = shift;
    my %io_pattern = @_;

    my $weight = get_general_weight( $metric, %io_pattern );

    # Azure needs to perform well on arbitrary customer workloads.
    # Overweight the common 70/30 mixture by 5x.
    if( $io_pattern{'W Mix'} == 30 )
    {
        $weight *= 5;
    }

    return $weight;
}

sub get_bing_weight
{
    my $metric = shift;
    my %io_pattern = @_;

    my $metric_weight = 0; # Ignore by default.
    
    # Index serving depends on consistently-low latency.
    # We consider 6 latency metrics and only 1 throughput.
    # Latency is 3x as important as throughput in this scheme.
    if( ( $metric eq "50th Percentile Total" ) or
        ( $metric eq "2-nines Percentile Total" ) or
        ( $metric eq "3-nines Percentile Total" ) or
        ( $metric eq "4-nines Percentile Total" ) or
        ( $metric eq "5-nines Percentile Total" ) or 
        ( $metric eq "6-nines Percentile Total" ) )
    {
        $metric_weight = 1;
    }
    elsif( $metric eq "MB/sec Total" )
    {
        $metric_weight = 2;
    }

    my $test_weight = 1;
    
    # ISSUE-REVIEW:
    # Should we filter out the smart tests?
    # $test_weight = 0 if $test_desc =~ /Background/;

    # Index serving is mostly reads
    if( $io_pattern{'R Mix'} > 0 )
    {
        $test_weight++;

        # Access pattern is mostly random
        $test_weight++ if $io_pattern{'Access Type'} =~ /random/i;
        
        # IFM is mostly 8K, L2Ranker is mostly 64K
        $test_weight++ if $io_pattern{'Access Size'} ~~ [ 8, 64 ];

        # Queue depth is typically very low
        $test_weight++ if $io_pattern{'QD'} ~~ [ 1, 2 ];
    }

    return $test_weight * $metric_weight;
}

my %scoring_policies = (
    'General' => \&get_general_weight,
    'Azure'   => \&get_azure_weight,
    'Bing'    => \&get_bing_weight,
);
    

#
# MAIN
#

# last argument is expected to be the output file name
my $outfile = get_outfile_name( pop @ARGV );
    
# remaining arguments are expected to be input dirs / globs
my @input_dirs = get_input_dirs( @ARGV );

my $num_input_dirs = scalar @input_dirs;

die "Nothing to do\n" unless $num_input_dirs > 0;

if ( -e $outfile )
{
    print "File $outfile already exists. ";
    if( $prompt )
    {
        exit 0 unless should_proceed("Overwrite?");
    }
    unlink $outfile;
}

my $workbook = Excel::Writer::XLSX->new( $outfile )
    or die "Couldn't create new workbook $outfile: $!\n";

print "Parsing $num_input_dirs dirs and generating $outfile\n";

my @all_stats = parse_directories( @input_dirs );

die "Nothing to do\n" unless scalar @all_stats > 0;

post_process_stats( @all_stats );

print "Detecting outliers...\n";
my %outliers = find_outliers( $workbook, @all_stats );

print "Computing scores...\n";

my $scores_invalid = 0;

my %raw_scores = extract_raw_scores( @all_stats );

unless( scores_hash_is_valid( %raw_scores ) )
{
    warn "Error while generating raw scores\n";
    $scores_invalid = 1;
}

my %std_scores = compute_std_scores( %raw_scores );

unless( scores_hash_is_valid( %std_scores ) )
{
    warn "Error while generating standard scores\n";
    $scores_invalid = 1;
}
    
make_bigger_better( %std_scores );

my %normalized_scores = normalize_scores( %std_scores );
    
unless( scores_hash_is_valid( %normalized_scores ) )
{
    warn "Error while generating normalized scores\n";
    $scores_invalid = 1;
}
    
warn "Scores sheets will not be generated\n" if $scores_invalid;

# Scoring policies compute a weight with arbitrary range.
# Here we convert to a fractional weight between 0 and 1. 
my %weights_as_fractions =
    compute_weights_as_fractions( %scoring_policies );

my %weighted_score_components =
    apply_weights_as_fractions( %normalized_scores, %weights_as_fractions );

my %final_scores = combine_scores( %weighted_score_components );
    
keys %final_scores == $num_input_dirs
    or warn "Warning: could not generate score for 1+ directories.\n";

print "Writing XLSX file...\n";
hilight_outliers( $workbook, %outliers, @all_stats );
generate_raw_data_sheet( $workbook, @all_stats );

generate_scores_sheets( 
    $workbook, 
    %raw_scores,
    %std_scores,
    %normalized_scores, 
    %weighted_score_components,
    %final_scores,
    %weights_as_fractions
)
unless $scores_invalid;

$workbook->close() or die "Error closing workbook $outfile: $!\n";

print_sanitization_decoder_ring() if $sanitize;

print "Done\n";
