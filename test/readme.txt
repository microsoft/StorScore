How to use the StorScore regression suite:

Regr.cmd is just a Perl script that runs StorScore.cmd with a vast array of
different command line options, in an attempt to exercise many code paths.

Two command line options are always used:
  1. --pretend, so we don't actually do much of anything
  2. --verbose, so STDOUT will log what we *would* have done

To use regr.cmd, you must establish a baseline of "known good" output, which
you can then compare against the "diff" output from a possibly change set.
The workflow looks vaguely like this:

1. Start with a known-good codebase
2. Generate a baseline dir with something like "regr base"
3. Make your changes, unpack a patch, etc.
4. Generate a diff dir with something like "regr diff"
5. Use a diff tool to compare the files in the base & diff directories
6. Understand/explain any differences before check-in

Note that, because we always use --pretend, regr.cmd doesn't ever really do
much of anything.  Thus, it's never enough to *only* run regr.cmd.  You
should always run a real test or two (perhaps using --demo_mode to speed
things up) against a real target device.

- MarkSan
