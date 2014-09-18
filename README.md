StorScore: A test framework to evaluate SSDs and HDDs
=========

StorScore is a component-level evaluation tool for testing storage devices.
When run with default settings it should give realistic metrics similar to
what can be expected by a Windows application developer.

Background
----------

We were motivated to write StorScore because most existing solutions had
some problems:

1.      Difficult to automate (GUI tools)
2.      Don't properly measure SSDs (history effect, endurance)
3.      Linux-centric

StorScore is driven by a "recipe" file, which, like all good things, is just
another Perl script.  The recipe is simply a series of steps to be followed.

By default, StorScore will run the "turkey test", which is the recipe used by
Microsoft to evaluate HDD and SSD for potential cloud deployments.  Take a
look in the recipes subdirectory to see other examples.

The only required command line option is --target.  This can specify an
existing file, volume, or a \\.\PHYSICALDRIVE number.  There are other
command line parameters that may be useful, but documentation has not yet
been written.  Take a look at lib\GlobalConfig.pm to see them all.

Be aware that StorScore can easily be used in a data-destructive manner.  Be
careful with the --target option.

When running, StorScore will create a bunch of files in the results directory.
We rarely look at these directly.  Instead, we typically gather many results
directories, from a cohort of comparable devices, and pass them to the
parse_results.cmd script, which generates a nice Excel XLSX file.  The Excel
file is structured to facilitate use of pivot charts.

The Excel file has the usual raw metrics (throughput, latency, etc.) but also
contains the result of our scoring system, which we designed to help summarize
what would otherwise be far too much data (hence the name: StorScore).

Dependencies
------------

StorScore depends on some "external" software components.

You must download and install the following or StorScore will not work:

    A Windows Perl interpreter:
        ActiveState: http://www.activestate.com/activeperl
        Strawberry: http://strawberryperl.com/
    
    The Visual Studio 2013 C++ runtime libraries for x86 & x64:
        http://www.microsoft.com/en-us/download/details.aspx?id=40784

StorScore will work without these components, but some features will be
disabled:

    SmartCtl.exe, from SmartMonTools:
        http://www.smartmontools.org/
    
    Ipmiutil.exe, from the IPMI Management Utilities:
        http://ipmiutil.sourceforge.net/


StorScore includes the following components "in the box."  We would like
to thank the authors and acknowledge their contribution:

    The excellent Perl library, Excel::Writer::XLSX, by John McNamara.
        http://search.cpan.org/~jmcnamara/Excel-Writer-XLSX/lib/Excel/Writer/XLSX.pm        

    DiskSpd.exe: an IO generator from the Microsoft Windows team.
        http://aka.ms/diskspd
        https://github.com/microsoft/diskspd

    SQLIO2.exe: an IO generator from the Microsoft SQL Server team.

Feedback?
---------

Questions, comments, bug reports, and especially accolades may be directed
to the developers:
-       Laura Caulfield <lauraca@microsoft.com>
-       Mark Santaniello <marksan@microsoft.com>
-       Bikash Sharma <bsharma@microsoft.com>
