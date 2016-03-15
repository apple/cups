README.txt - 2016-03-15
-----------------------

This directory contains an automated build and test environment for CUPS.


ENVIRONMENT

If present, the buildtest.env file sets additional shell variables for the
build and test scripts described below.  The following variables are
currently supported:

    BUILDOPTIONS      - Configure options that are passed to each target
    BUILDNOTIFY       - An email address or URL that specifies where the
                        build log should be sent.
    BUILDSUBJECT      - A string to include in the subject (default is empty
                        string)


SCRIPTS

The build.sh script builds the current stable branch of CUPS along with
master ("latest"), running "make check" in each.  Normally you will run the
script via a cron job:

    0   0   *   *   0   /path/to/build.sh --update --quiet clean all
    0   0   *   *   1-6 /path/to/build.sh --update --quiet

The "--update" option tells build.sh to do an "git pull" prior to building.

The "--quiet" option tells build.sh to work quietly and optionally send the
build log via email or HTTP POST for recording, automated processing, etc.

The "clean" and "all" options specify build targets; the default target is
"all".  In the example above, the first line tells cron to run the script at
midnight each day; Sunday (day 0) also does a clean build, while every other
day only builds files that have changed.
