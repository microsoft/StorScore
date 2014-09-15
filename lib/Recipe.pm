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
use Cwd;
use Util;

no if $PERL_VERSION >= 5.017011, 
    warnings => 'experimental::smartmatch';

has 'file_name' => (
    is       => 'ro',
    isa      => 'Str',
    required => 1
);

has 'io_generator_type' => (
    is       => 'ro',
    isa      => 'Str',
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

has 'do_initialize' => (
    is      => 'ro',
    isa     => 'Bool',
    default => 1
);

has 'do_precondition' => (
    is      => 'ro',
    isa     => 'Bool',
    default => 1
);

has 'quick_test' => (
    is      => 'ro',
    isa     => 'Maybe[Bool]',
    default => 0
);

has 'start_on_step' => (
    is      => 'ro',
    isa     => 'Int',
    default => 1
);

has 'test_time_override' => (
    is      => 'ro',
    isa     => 'Maybe[Int]',
    default => undef
);

has 'warmup_time_override' => (
    is      => 'ro',
    isa     => 'Maybe[Int]',
    default => undef
);

has 'is_target_ssd' => (
    is      => 'ro',
    isa     => 'Bool',
    default => undef
);

has 'preconditioner' => (
    is     => 'ro',
    isa    => 'Precondition',
    writer => '_preconditioner'
);

has 'io_generator' => (
    is     => 'ro',
    does   => 'IOGenerator',
    writer => '_io_generator'
);

has 'smartctl_runner' => (
    is     => 'ro',
    isa    => 'Maybe[SmartCtlRunner]',
    default => undef,
    writer => '_smartctl_runner'
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

has 'bg_pids' => (
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

    if( $self->io_generator_type eq 'sqlio' )
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
    else
    {
        die unless $self->io_generator_type eq 'diskspd';

        return 0 if defined $step_ref->{'compressibility'};
    }

    return 1;
}

sub warn_illegal_args
{    
    my $self = shift;
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    return unless $kind eq 'test';

    if( $self->io_generator_type eq 'sqlio' )
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
    else
    {
        die unless $self->io_generator_type eq 'diskspd';

        if( defined $step_ref->{'compressibility'} )
        {
            my $msg = "\tWarning!\n";
            $msg .= "\tDiskSpd doesn't support variable entropy. ";
            $msg .= "Will skip step ". $self->current_step . ".";

            warn "$msg\n\n";
        }
    }
}

sub canonicalize_step
{
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};
    
    return unless $kind ~~ [qw( test precondition )];

    # create read_percentage from write_percentage
    $step_ref->{'read_percentage'} =
        100 - $step_ref->{'write_percentage'};

    # ensure name_string can be used as a filename
    $step_ref->{'name_string'} =
        make_legal_filename( $step_ref->{'name_string'} );
}

sub apply_overrides
{
    my $self = shift;

    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    return unless $kind eq 'test';

    if( defined $self->test_time_override )
    {
        $step_ref->{'run_time'} = $self->test_time_override;
    }

    if( defined $self->warmup_time_override )
    {
        $step_ref->{'warmup_time'} = $self->warmup_time_override;
    }
}

sub make_step
{
    my $kind = shift;
    my @args = @_;
      
    # test and precondition use named-parameter idiom
    return ( kind => $kind, @args ) 
        if $kind ~~ [qw( test precondition )];
   
    # bg_exec has a single unnamed argument, the command
    return ( kind => $kind, command => shift @args )
        if $kind eq 'bg_exec';
        
    # idle has a single unnamed argument, the time
    return ( kind => $kind, run_time => shift @args )
        if $kind eq 'idle';

    # default: no arguments, just store the kind
    return ( kind => $kind );
}

sub handle_step
{
    my $self = shift;

    my $callback = shift;
    my $do_warnings = shift;
    my $kind = shift;
    my @step_args = @_;
    
    # All done if the user wanted to skip preconditions
    return if $kind eq 'precondition' and not $self->do_precondition;
    
    my %step = make_step( $kind, @step_args );

    canonicalize_step( \%step );
    $self->apply_overrides( \%step );
    $self->warn_illegal_args( \%step ) if $do_warnings;
   
    my $is_legal = $self->try_legalize_arguments( \%step );

    if( $is_legal )
    {
        $callback->( %step );
        $self->advance_current_step();
    }
}

sub generate_header($$$)
{
    my $file_name = shift;
    my $package = shift;
    my $be_permissive = shift;

    my $header = <<"HEADER";
        package $package;
        use GlobalConfig; # Allows access to %gc
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

    # Improve the quality of diagnostic messages
    $header .= qq(\n# line 1 "$file_name"\n);
    
    return $header;
}

sub execute_recipe
{
    my $self = shift;
    my %args = @_;

    my $callback = $args{'callback'}
        // die "Must provide a callback";

    my $recipe_warnings = $args{'recipe_warnings'} // 0;
    my $permissive_perl = $args{'permissive_perl'} // 0;

    state $eval_count = 0;

    # Throw-away package to "insulate" us from the eval code
    my $package = "RecipeEval" . $eval_count++;

    my @step_kinds = qw(test precondition bg_exec bg_killall idle);

    {
        no strict 'refs';

        # Install our handler for each kind of step 
        foreach my $kind ( @step_kinds )
        {
            my $sym = "${package}::$kind";

            *$sym = sub
            {
                $self->handle_step(
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
       
            my $eval_string =
                generate_header( $file_name, $package, $permissive_perl );

            $eval_string .= slurp_file( $file_name );

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

    # Prefix our header to recipe string
    my $eval_string =
        generate_header( $self->file_name, $package, $permissive_perl );

    $eval_string .= $self->recipe_string;
   
    # Make %gc read-only for the recipes
    GlobalConfig::lock();

    # Eval recipe code with our hooks installed
    my $retval = eval( $eval_string );
    die $EVAL_ERROR unless defined $retval;
    
    # Restore mutability to %gc
    GlobalConfig::unlock();

    # Restore cwd 
    chdir( $previous_cwd );

    # Delete the throw-away package
    delete_package( $package );

    $self->reset_current_step();
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

sub get_num_precondition_steps
{
    my $self = shift;
    
    return scalar grep { $_->{'kind'} eq 'precondition' } @{$self->steps};
}

sub estimate_step_run_time($;$)
{
    my $self = shift;

    my $step_ref = shift;
    my $est_pc_time = shift // Precondition::NORMAL_MIN_RUN_SECONDS;

    my $time = 0;

    $time += $step_ref->{'run_time'} // 0;
    $time += $step_ref->{'warmup_time'} // 0;

    if( $step_ref->{'kind'} eq 'precondition' )
    {
        $time += $est_pc_time;
    }

    return $time;
}

sub estimate_run_time(;$)
{
    my $self = shift;

    my $est_pc_time = shift // Precondition::NORMAL_MIN_RUN_SECONDS;

    my $total = 0;

    foreach my $step_ref ( @{$self->steps} )
    {
        $total += $self->estimate_step_run_time( $step_ref, $est_pc_time );
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

sub BUILD
{
    my $self = shift;
    
    $self->_recipe_string( slurp_file( $self->file_name ) );

    # Phase 1: Parse the recipe to estimate runtime, etc.
    #
    # In this case, our callback merely records the step's details,
    # so we aren't really "executing" much of anything.  Here we
    # generate recipe warnings, and we also hide all Perl warnings,
    # such as undefined variables.
    $self->execute_recipe(
        recipe_warnings => 1,
        permissive_perl => 1,
        callback        => sub { push @{$self->steps}, {@_}; }
    );
}

sub get_announcement_message
{
    my $self = shift;
    my $step_ref = shift;
    
    my $msg;

    if( $step_ref->{'kind'} eq 'test' )
    {
        my $short_pattern =
            $step_ref->{'access_pattern'} eq "random" ?  "rand" : "seq";

        $msg = sprintf( "%4s,  %4s,  %3d%% read,  %3d%% write,  QD=%2d",
            $step_ref->{'block_size'},
            $short_pattern,
            $step_ref->{'read_percentage'},
            $step_ref->{'write_percentage'},
            $step_ref->{'queue_depth'}
        );
    } 
    elsif( $step_ref->{'kind'} eq 'idle' )
    {
        $msg = "Idling...";
    }
    elsif( $step_ref->{'kind'} eq 'bg_exec' )
    {
        my $command = $step_ref->{'command'};
        $msg = qq{Executing "$command" in the background...};
    }
    elsif( $step_ref->{'kind'} eq 'bg_killall' )
    {
        $msg = "Killing all background processes...";
    }

    return $msg;
}

sub run_step
{
    my $self = shift;
    my $step_ref = shift;

    my $kind = $step_ref->{'kind'};

    my $progress = sprintf( 
        "[%3d/%3d] ", $self->current_step, $self->get_num_steps );

    if( $kind ne 'precondition' )
    {
        my $announce = $self->get_announcement_message( $step_ref );

        my $time = $self->estimate_step_run_time( $step_ref );

        my $eta = 
            "ETA: " . strftime "%I:%M:%S %p", localtime( time() + $time );

        my $cols_remaining =
            80 - length( $progress )
            - length( $announce )
            - length( $eta ) - 1;

        my $pad = ' ' x $cols_remaining;

        print $progress . $announce . $pad . $eta;
    }

    if( $kind eq 'test' )
    {
        if( $step_ref->{'warmup_time'} > 0 )
        {
            $self->io_generator->run( $step_ref, 'warmup' );
        }

        my $ns = $step_ref->{'name_string'};

        $self->smartctl_runner->collect( "smart-before-$ns.txt" )
            if defined $self->smartctl_runner;

        if( defined $self->logman_runner )
        {
            $self->logman_runner->name_string( $ns );
            $self->logman_runner->start();
        }

        if( defined $self->power )
        {
            $self->power->name_string( $ns );
            $self->power->start();
        }

        $self->io_generator->run( $step_ref, 'test' );

        $self->smartctl_runner->collect( "smart-after-$ns.txt" )
            if defined $self->smartctl_runner;

        $self->logman_runner->stop() if defined $self->logman_runner;
        $self->power->stop() if defined $self->power;
    }
    elsif( $kind eq 'precondition' )
    {
        my $msg_prefix = $progress . "Preconditioning, ";

        $self->preconditioner->run_to_steady_state( $msg_prefix, $step_ref );
    }
    elsif( $kind eq 'idle' )
    {
        my $run_time = $step_ref->{'run_time'};

        sleep( $run_time ) unless $pretend; 
    }
    elsif( $kind eq 'bg_exec' )
    {
        my $command = $step_ref->{'command'};
        
        my $pid = execute_task(
            $command,
            background => 1,
            new_window => 1
        ); 

        push @{$self->bg_pids}, $pid;
    }
    elsif( $kind eq "bg_killall" )
    {
        kill_task( $_ ) foreach @{$self->bg_pids};
        @{$self->bg_pids} = ();
    }

    print "\n" if $kind ne 'precondition';
}

sub run
{
    my $self = shift;

    my %args = @_;

    $self->_preconditioner( $args{'preconditioner'} );
    $self->_io_generator( $args{'io_generator'} );
    $self->_smartctl_runner( $args{'smartctl_runner'} );
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
            $self->run_step( {@_} ) 
                unless $self->current_step < $self->start_on_step; 
        }
    );

    # kill any leftover background processes        
    kill_task( $_ ) foreach @{$self->bg_pids};
}

sub warn_expected_run_time
{
    my $self = shift;

    my $num_pc = $self->get_num_precondition_steps();
    my $est_pc_time = 0;
   
    if( $self->do_precondition and $num_pc > 0 )
    {
        if( $self->quick_test )
        {
            $est_pc_time = Precondition::QUICK_TEST_MIN_RUN_SECONDS;
        }
        else
        {
            $est_pc_time = Precondition::NORMAL_MIN_RUN_SECONDS;
        }
    }

    my $time_string = 
        seconds_to_human(
            $self->estimate_run_time( $est_pc_time )
        );

    print "\tRun time will be >= $time_string";
    
    if( $self->do_initialize )
    {
        if( $self->is_target_ssd )
        {
            print" after 2 passes of target init.\n";
        }
        else
        {
            print" after 1 pass of target init.\n";
        }
    }
    else
    {
        print ".\n";
    }
    
    if( $self->do_precondition and $num_pc > 0 )
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
