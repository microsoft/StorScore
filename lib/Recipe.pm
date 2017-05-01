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

package Recipe;

use strict;
use warnings;
use Moose;
use English;
use feature 'state';
use POSIX 'strftime';
use Symbol 'delete_package';
use Text::Balanced 'extract_bracketed';
use Cwd;
use Util;
use PreconditionRunner;
use DiskSpdParser;
use SqlioParser;

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

has 'file_name' => (
    is       => 'ro',
    isa      => 'Str',
    required => 1
);

has 'output_dir' => (
    is       => 'ro',
    isa      => 'Str',
    required => 1
);

has 'target' => (
    is      => 'ro',
    isa     => 'Target',
    required => 1
);

has 'cmd_line' => (
    is      => 'ro',
    isa     => 'CommandLine',
    required => 1
);

has 'recipe_string' => (
    is  => 'ro',
    isa => 'Str',
    writer => '_recipe_string'
);

has 'steps' => (
    is  => 'ro',
    isa => 'ArrayRef',
    default => sub { [] },
);

has 'io_generator' => (
    is     => 'ro',
    does   => 'IOGenerator',
    writer => '_io_generator'
);

has 'smart_runner' => (
    is     => 'ro',
    isa    => 'Maybe[SmartRunner]',
    default => undef,
    writer => '_smart_runner'
);

has 'logman_runner' => (
    is      => 'rw',
    isa     => 'Maybe[LogmanRunner]',
    default => undef,
    writer  => '_logman_runner'
);

has 'power' => (
    is      => 'rw',
    isa     => 'Maybe[Power]',
    default => undef,
    writer  => '_power'
);

has 'current_step' => (
    traits  => ['Counter'],
    is      => 'rw',
    isa     => 'Num',
    default => 1,
    handles => {
        advance_current_step => 'inc',
        reset_current_step   => 'reset'
    },
);

has 'bg_processes' => (
    is  => 'ro',
    isa => 'ArrayRef',
    default => sub { [] },
);

sub try_legalize_arguments
{
    my $self = shift;
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    return 1 unless $kind eq 'test';

    if( $self->cmd_line->io_generator eq 'sqlio' )
    {
        # sqlio requires time >= 5 sec 
        if( $step_ref->{'run_time'} < 5 )
        {
            $step_ref->{'run_time'} = 5;
        }

        if( $step_ref->{'warmup_time'} > 0 and 
            $step_ref->{'warmup_time'} < 5 )
        {
            $step_ref->{'warmup_time'} = 5;
        }
    }

    # N.B.: You can return zero here to skip this test
    
    return 1;
}

sub warn_illegal_args
{    
    my $self = shift;
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    return unless $kind eq 'test';

    if( $self->cmd_line->io_generator eq 'sqlio' )
    {
        # sqlio requires time >= 5 sec 
        if( $step_ref->{'run_time'} < 5 )
        {
            my $msg = "\tWarning!\n";
            $msg .= "\tTest run_time below minimum of 5 sec. ";
            $msg .= "Modifying step ". $self->current_step . ".";

            warn "$msg\n\n";
        }

        if( $step_ref->{'warmup_time'} > 0 and 
            $step_ref->{'warmup_time'} < 5 )
        {
            my $msg = "\tWarning!\n";
            $msg .= "\tStep warmup_time below minimum of 5 sec. ";
            $msg .= "Modifying step ". $self->current_step . ".";

            warn "$msg\n\n";
        }
    }
}

sub canonicalize_step
{
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};
    my $number = $step_ref->{'number'};

    return unless $kind ~~ [qw( test precondition )]; 

    # create read_percentage from write_percentage
    $step_ref->{'read_percentage'} =
        100 - $step_ref->{'write_percentage'};

    # ensure description can be used as a unique filename
    $step_ref->{'description'} = make_legal_filename( $step_ref->{'description'});
}

sub apply_overrides
{
    my $self = shift;

    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    return unless $kind eq 'test';

    if( defined $self->cmd_line->test_time_override )
    {
        $step_ref->{'run_time'} = $self->cmd_line->test_time_override;
    }

    if( defined $self->cmd_line->warmup_time_override )
    {
        $step_ref->{'warmup_time'} = $self->cmd_line->warmup_time_override;
    }
}

sub make_step
{
    my $kind = shift;
    my $number = shift;
    my @args = @_;

    my %step_args;

    # Every step records its kind and number
    $step_args{'kind'} = $kind;
    $step_args{'number'} = $number;
    
    # These kinds use the named-parameter idiom
    %step_args = ( %step_args, @args )
        if $kind ~~ [qw( test initialize precondition bg_exec fg_exec )]; 
   
    # idle has a single unnamed argument, the time
    %step_args = ( %step_args, run_time => shift @args )
        if $kind eq 'idle';

    return %step_args; 
}

sub handle_step
{
    my $self = shift;

    my $callback = shift;
    my $do_warnings = shift;
    my $kind = shift;
    my @step_args = @_;
   
    my $step_number = $self->current_step;

    my %step = make_step( $kind, $step_number, @step_args );

    canonicalize_step( \%step );
    $self->apply_overrides( \%step );
    $self->warn_illegal_args( \%step ) if $do_warnings;
    
    # Keep track of the context in which we were called
    my $context;
    $context = 'list' if wantarray;
    $context = 'void' unless defined wantarray;
    $context = 'scalar' unless wantarray;

    my $scalar_retval;
    my @list_retval; 

    my $is_legal = $self->try_legalize_arguments( \%step );

    if( $is_legal )
    {
        # Carry forward our caller's context
        $callback->( %step ) if $context eq 'void';
        $scalar_retval = $callback->( %step ) if $context eq 'scalar';
        @list_retval = $callback->( %step ) if $context eq 'list';

        $self->advance_current_step();
    }
    
    return $scalar_retval if $context eq 'scalar';
    return @list_retval if $context eq 'list';
}

sub generate_header($$$$)
{
    my $file_name = shift;
    my $package = shift;
    my $be_permissive = shift;
    my $be_quiet = shift;

    my $header = <<"HEADER";
        package $package;
        use SharedVariables;
        use Util;         # Allows access to by_human, etc
HEADER

    if( $be_permissive )
    {
        $header .= "no warnings;\n";
    }
    else
    {
        $header .= "use warnings;\n";

        # Hide warning "Subroutine do_workload redefined"
        $header .= "no warnings 'redefine';\n";
       
        # Hide warning "Prototype mismatch: sub do_workload"
        $header .= "no warnings 'prototype';\n";
    }

    if( $pretend )
    {
        # Hide warning "Use of unitialized value"
        $header .= "no warnings 'uninitialized';\n";
    }

    if( $be_quiet )
    {
        # Replace STDOUT and STDERR with NUL
        $header .= "local *STDOUT; open STDOUT, '>NUL';\n";
        $header .= "local *STDERR; open STDERR, '>NUL';\n";
    }

    # Improve the quality of diagnostic messages
    $header .= qq(\n# line 1 "$file_name"\n);
    
    return $header;
}

sub generate_footer
{
    my $footer = <<"FOOTER";
        1; # Return true if everything goes well
FOOTER

    return $footer;
}

sub execute_recipe
{
    my $self = shift;
    my %args = @_;

    my $callback = $args{'callback'}
        // die "Must provide a callback";

    my $recipe_warnings = $args{'recipe_warnings'} // 0;
    my $permissive_perl = $args{'permissive_perl'} // 0;
    my $quiet_stdio = $args{'quiet_stdio'} // 0;

    state $eval_count = 0;

    # Throw-away package to "insulate" us from the eval code
    my $package = "RecipeEval" . $eval_count++;

    my @step_kinds = 
        qw(test purge initialize precondition bg_exec fg_exec bg_killall idle);

    {
        no strict 'refs';

        # Install our handler for each kind of step 
        foreach my $kind ( @step_kinds )
        {
            my $sym = "${package}::$kind";

            *$sym = sub
            {
                return $self->handle_step(
                    $callback,
                    $recipe_warnings,
                    $kind,
                    @_
                )
            };
        }

        # Generate and inject an include function that adds our header
        my $sym = "${package}::include";

        *$sym = sub
        { 
            my $file_name = shift;
       
            # Emulate the behavior of Perl's require statement
            foreach my $prefix (@INC)
            {
                if( -e "$prefix\\$file_name" )
                {
                    $file_name = "$prefix\\$file_name";
                    last;
                }
            }

            my $recipe_str = slurp_file( $file_name );

            # For SSD: expand test macros to follow proper methodology
            # We must do this every time we include a new file.
            $recipe_str = $self->expand_test_macro( $recipe_str )
                if $self->target->is_ssd();
       
            my $eval_string;

            $eval_string .= 
                generate_header(
                    $file_name,
                    $package,
                    $permissive_perl,
                    $quiet_stdio
                );
            
            $eval_string .= $recipe_str;
            
            $eval_string .= generate_footer();

            my $retval = eval( $eval_string );

            die $EVAL_ERROR unless defined $retval;
        };
    }

    # Allow recipes to require ".rpm" recipe modules from lib subdir
    local @INC = ( @INC, "$recipes_dir\\lib" );

    # Ensure that requires happen every time we eval
    local %INC = %INC;

    # recipes are executed with the recipes subdir as cwd
    my $previous_cwd = getcwd();
    chdir( $recipes_dir );

    my $eval_string;

    # Prefix our header to recipe string
    $eval_string =
        generate_header(
            $self->file_name,
            $package,
            $permissive_perl,
            $quiet_stdio
        );

    $eval_string .= $self->recipe_string;
    $eval_string .= generate_footer();

    # Eval recipe code with our hooks installed
    my $retval = eval( $eval_string );
    die $EVAL_ERROR unless defined $retval;
    
    # Restore cwd 
    chdir( $previous_cwd );

    # Delete the throw-away package
    delete_package( $package );

    $self->reset_current_step();
}

sub check_unique_test_descriptions
{
	my $self = shift;

	my %desc_hash = ();

	foreach my $step (@{$self->steps})
	{
		
		my $desc = $step->{'description'};
		if ( $desc && $step->{'kind'} eq "test" )
		{
			die "Test description '$desc' is not unique" 
				if ( $desc_hash{ $desc } );
			$desc_hash{ $desc } = 1;
		}
	}

}

sub get_num_steps
{
    my $self = shift;
    
    return scalar @{$self->steps};
}

sub get_num_test_steps
{
    my $self = shift;
    
    return scalar grep { $_->{'kind'} eq 'test' } @{$self->steps};
}

sub calculate_step_run_time($)
{
    my $self = shift;
    my $step_ref = shift;

    my $time = 0;

    $time += $step_ref->{'run_time'} // 0;
    $time += $step_ref->{'warmup_time'} // 0;

    return $time;
}

sub estimate_run_time(;$)
{
    my $self = shift;

    my $est_pc_time =
        shift // PreconditionRunner::NORMAL_MIN_RUN_SECONDS;

    my $total = 0;

    foreach my $step_ref ( @{$self->steps} )
    {
        $total += $self->calculate_step_run_time( $step_ref );
    }
   
    if( $self->target->do_precondition )
    {
        foreach my $step_ref ( @{$self->steps} )
        {
            if( $step_ref->{'kind'} eq 'test' )
            {
                $total += $est_pc_time;
            }
        }
    }

    return $total;
}

sub contains_writes()
{
    my $self = shift;

    foreach my $step_ref ( @{$self->steps} )
    {
        no warnings 'uninitialized';

        return 1 if $step_ref->{'write_percentage'} > 0;
    }

    return 0;
}

sub prefix_target_init
{
    my $self = shift;
    my $input = shift;
    
    my $str = "";

    # It's important that we don't add extra newlines otherwise
    # error messages may reference the wrong recipe line number.
    $str .= "purge();" if $self->target->do_purge;
    $str .= "initialize();" if $self->target->do_initialize;

    return $str . $input;
}

sub get_test_macro_string
{
    my $self = shift;
    my $test_args = shift;
   
    my $str;
   
    # It's important that we don't add extra newlines otherwise
    # error messages may reference the wrong recipe line number.
    $str .= q( do { );
    $str .= q( my %args = ) . $test_args . ';';

    $str .= q( purge() if $args{'purge'} // 1; )
        if $self->target->do_purge;

    $str .= q( initialize( %args ) if $args{'initialize'} // 1; )
        if $self->target->do_initialize;

    $str .= q( precondition( %args ) if $args{'precondition'} // 1; )
        if $self->target->do_precondition;

    $str .= q( test( %args ); );
    $str .= q( } );

    return $str;
}

sub expand_test_macro
{
    my $self = shift;
    my $input = shift;
    my $output;
    
    while( $input =~ s/(.*?)\btest(\(.*)/$2/s )
    {
        $output .= $1;

        my $test_args = extract_bracketed( $input, '()' );
        die $EVAL_ERROR unless defined $test_args;
    
        $output .= $self->get_test_macro_string( $test_args );
    }

    $output .= $input; # Handle remainder
    
    return $output;
}

sub BUILD
{
    my $self = shift;
    
    my $recipe_str = slurp_file( $self->file_name );

    # For HDD: do this exactly once before running the recipe
    $recipe_str = $self->prefix_target_init( $recipe_str )
        if $self->target->is_hdd();

    # For SSD: expand test macros to follow proper methodology
    $recipe_str = $self->expand_test_macro( $recipe_str )
        if $self->target->is_ssd();

    $self->_recipe_string( $recipe_str );

    # Phase 1: Parse the recipe to estimate runtime, etc.
    #
    # In this case, our callback merely records the step's details,
    # so we aren't really "executing" much of anything.  Here we
    # generate recipe warnings, and we also hide all Perl warnings,
    # such as undefined variables.
    $self->execute_recipe(
        recipe_warnings => 1,
        permissive_perl => 1,
        quiet_stdio => 1,
        callback => sub { push @{$self->steps}, {@_}; }
    );
}

sub get_time_message
{
    my $self = shift;
    my $step_ref = shift;
   
    my $msg = "";

    # Only do this for "kinds" that take a while 
    if( $step_ref->{'kind'} ~~ [qw( test idle )] )
    {
        my $start_time = strftime "%I:%M:%S %p", localtime( time() );

        my $run_time = $self->calculate_step_run_time( $step_ref );
        
        my $end_time = 
            strftime "%I:%M:%S %p", localtime( time() + $run_time );

        $msg = "$start_time -> $end_time";
    }

    return $msg;
}

sub get_announcement_style
{
    my $step_ref = shift;
    
    my $kind = $step_ref->{'kind'};

    # These step "kinds" announce their own progress
    return 'internal'
        if $kind ~~ [qw( purge precondition initialize )];

    return 'external';
}

sub get_announcement_message
{
    my $step_ref = shift;
    
    my $msg;
	my $command = $step_ref->{'command'};

    if( $step_ref->{'kind'} eq 'test' )
    {
        my $short_pattern =
            $step_ref->{'access_pattern'} eq "random" ?  "rnd" : "seq";

        $msg = sprintf( " %4s, %3s, %3d%% wri, QD=%3d, Used=%3d%%",
            $step_ref->{'block_size'},
            $short_pattern,
            $step_ref->{'write_percentage'},
            $step_ref->{'queue_depth'},
            $step_ref->{'percent_used'} // 100
        );
    } 
    elsif( $step_ref->{'kind'} eq 'idle' )
    {
        $msg = "Idling";
    }
    elsif( $step_ref->{'kind'} eq 'bg_exec' )
    {
        $msg = qq{Executing "$command" in the background};
    }
	elsif( $step_ref->{'kind'} eq 'fg_exec' )
	{
		$msg = qq{Executing "$command" in the foreground};
	}
    elsif( $step_ref->{'kind'} eq 'bg_killall' )
    {
        $msg = "Killing all background processes";
    }

    return $msg;
}

sub bg_killall
{
    my $self = shift;

    foreach my $proc ( @{$self->bg_processes} )
    {
        my $pid = $proc->{'pid'};
        kill_task( $pid );
    }
    
    @{$self->bg_processes} = ();
}

sub run_step
{
    my $self = shift;
    my $step_ref = shift;
        
    my $kind = $step_ref->{'kind'};

    # Keep track of the context in which we were called
    my $context;
    $context = 'list' if wantarray;
    $context = 'void' unless defined wantarray;
    $context = 'scalar' unless wantarray;

    my $scalar_retval;
    my @list_retval;

    my $num_step_digits = length( $self->get_num_steps );
    
    my $progress = sprintf( 
        "%${num_step_digits}d/%${num_step_digits}d: ",
        $self->current_step, $self->get_num_steps
    );

    my $announcement_style = get_announcement_style( $step_ref );

    if( $announcement_style eq 'external' )
    {
        my $announce = get_announcement_message( $step_ref );

        my $time = $self->get_time_message( $step_ref );

        my $cols_remaining = 80;

        $cols_remaining -=
        length( $progress ) +
        length( $announce ) +
        length( $time ) + 1;

        my $pad = ' ' x $cols_remaining;

        print $progress . $announce . $pad . $time;
    }

    if( $kind eq 'purge' )
    {
        $self->target->purge(
            msg_prefix => $progress,
        );
    }
    elsif( $kind eq 'initialize' )
    {
        $self->target->initialize(
            msg_prefix => $progress,
            test_ref => $step_ref
        );
    }
    elsif( $kind eq 'precondition' )
    {
        $self->target->precondition( 
            msg_prefix => $progress,
            output_dir => $self->output_dir,
            test_ref => $step_ref
        );
    }
    elsif( $kind eq 'test' )
    {
        $self->target->prepare( $step_ref )
            unless $self->target->is_prepared();
        
        unless( $pretend or $self->cmd_line->raw_disk )
        {
            die "Attempt to test non-existent target file?\n"
                unless -e $self->target->file_name;
        }

        my $desc = $step_ref->{'description'};

        # Record statistics about this volume
        my $wmic_runner = WmicRunner->new(
            target => $self->target,
            output_dir => $self->output_dir
        );

        $wmic_runner->collect( "wmic-$desc.txt" );
  
        # Record background activity, if any, during this test
        if( scalar @{$self->bg_processes} > 0 )
        {
            open my $FH, '>', $self->output_dir . "\\background-$desc.txt";

            foreach my $proc ( @{$self->bg_processes} )
            {
                print $FH $proc->{'description'} . "\n";
                print $FH $proc->{'command'} . "\n";
                print $FH $proc->{'pid'} . "\n\n";
            }

            close $FH;
        }

        if( $step_ref->{'warmup_time'} > 0 )
        {
            $self->io_generator->run( $step_ref, 'warmup' );
        }

        $self->smart_runner->collect(
            file_name => "smart-before-$desc.txt",
            output_dir => $self->output_dir,
        )
        if defined $self->smart_runner;

        if( defined $self->logman_runner )
        {
            $self->logman_runner->description( $desc );
            $self->logman_runner->start();
        }

        if( defined $self->power )
        {
            $self->power->description( $desc );
            $self->power->start();
        }

        $self->io_generator->run( $step_ref, 'test' );

        $self->smart_runner->collect(
            file_name => "smart-after-$desc.txt",
            output_dir => $self->output_dir,
        )
        if defined $self->smart_runner;

        $self->logman_runner->stop() if defined $self->logman_runner;
        $self->power->stop() if defined $self->power;
            
        # If test() called in list context, parse and return results
        if( $context eq 'list' )
        {
            my $iogen_parser;

            $iogen_parser = DiskSpdParser->new()
                if $self->cmd_line->io_generator eq 'diskspd';
            
            $iogen_parser = SqlioParser->new()
                if $self->cmd_line->io_generator eq 'sqlio';
  
            # ISSUE-REVIEW: is this guaranteed to be correct?
            my $iogen_outfile = $self->output_dir . "\\test-$desc.txt";

            open my $IOGEN_OUT, "<$iogen_outfile" 
                or die "Error opening $iogen_outfile";

            my %stats;

            $iogen_parser->parse( $IOGEN_OUT, \%stats );

            @list_retval = %stats;

            close $IOGEN_OUT;
        }

        # Allow test to specify that output files be discarded
        my $discard_results = $step_ref->{'discard_results'} // 0;

        unlink( glob( $self->output_dir . "\\*$desc*" ) )
            if $discard_results;
    }
    elsif( $kind eq 'idle' )
    {
        my $run_time = $step_ref->{'run_time'};

        sleep( $run_time ) unless $pretend; 
    }
    elsif( $kind eq 'bg_exec' || $kind eq 'fg_exec' )
    {
        # ISSUE-REVIEW: 
        # 
        # Ensure that the target file exists to support stuff like
        #    purge();
        #    bg_exec( do something to target file here );
        #
        # Does this make sense in a general purpose exec (fg & bg)?
        
        $self->target->prepare( $step_ref )
            unless $self->target->is_prepared();
        
        unless( $pretend )
        {
            die "Attempt to test non-existent target file?\n"
                unless -e $self->target->file_name;
        }

        my $command = $step_ref->{'command'};

		my $pid = execute_task(
			$command,
			background => ($kind eq 'bg_exec'),
			new_window => 1
		); 

		if( $kind eq 'bg_exec')
		{
			my $description = $step_ref->{'description'};
			push @{$self->bg_processes}, {
				pid => $pid,
				command => $command,
				description => $description
			};
		}

    }
    elsif( $kind eq "bg_killall" )
    {
        $self->bg_killall();
    }

    print "\n" if $announcement_style eq 'external';
    
    return $scalar_retval if $context eq 'scalar';
    return @list_retval if $context eq 'list';
}

sub run
{
    my $self = shift;

    my %args = @_;

    $self->_io_generator( $args{'io_generator'} );
    $self->_smart_runner( $args{'smart_runner'} );
    $self->_logman_runner( $args{'logman_runner'} );
    $self->_power( $args{'power'} );

    # Phase 2: Execute the recipe
    #
    # Our callback calls run_step(), so we will truly /execute/ the recipe.
    # This time we do not emit recipe warnings, but we do enable most Perl
    # warnings.
    $self->execute_recipe(
        recipe_warnings => 0,
        permissive_perl => 0,
        callback => sub
        { 
            unless( ($self->current_step < $self->cmd_line->start_on_step) or
                    ($self->current_step > $self->cmd_line->stop_on_step) )
            {
                return $self->run_step( {@_} );
            }
    
            # Maintain proper context on skipped steps.
            # Avoids "Odd number of elements in hash assignment" warning.
            return () if wantarray;
        }
    );

    # kill any leftover background processes        
    $self->bg_killall();
}

sub warn_expected_run_time
{
    my $self = shift;

    my $num_pc =
        $self->target->do_precondition ? $self->get_num_test_steps() : 0;

    my $est_pc_time = 0;
   
    if( $num_pc > 0 )
    {
        if( $self->cmd_line->demo_mode )
        {
            $est_pc_time =
                PreconditionRunner::QUICK_TEST_MIN_RUN_SECONDS;
        }
        else
        {
            $est_pc_time =
                PreconditionRunner::NORMAL_MIN_RUN_SECONDS;
        }
    }

    my $time_string = 
        seconds_to_human(
            $self->estimate_run_time( $est_pc_time )
        );

    print "\tRun time will be >= $time_string.\n";
    
    if( $self->target->do_initialize )
    {
        print "\tThis does not include target init ";

        if( $self->target->is_ssd )
        {
            print "(2 overwrites per-test).\n";
        }
        else
        {
            print "(1 overwrite).\n";
        }
    }
    
    if( $num_pc > 0 )
    {
        my $human_pc_time = seconds_to_human( $est_pc_time ); 
        
        print "\tRecipe includes $num_pc " .
            "preconditions, each >= $human_pc_time.\n"
    }

    print "\n";
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
