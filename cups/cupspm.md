---
title: CUPS Programming Manual
author: Michael R Sweet
copyright: Copyright © 2007-2017 by Apple Inc. All Rights Reserved.
version: 2.2.5
...

> Please [file issues on Github](https://github.com/apple/cups/issues) to
> provide feedback on this document.


# Introduction

CUPS provides the "cups" library to talk to the different parts of CUPS and with
Internet Printing Protocol (IPP) printers. The "cups" library functions are
accessed by including the `<cups/cups.h>` header.

CUPS is based on the Internet Printing Protocol ("IPP"), which allows clients
(applications) to communicate with a server (the scheduler, printers, etc.) to
get a list of destinations, send print jobs, and so forth.  You identify which
server you want to communicate with using a pointer to the opaque structure
`http_t`.  The `CUPS_HTTP_DEFAULT` constant can be used when you want to talk to
the CUPS scheduler.


## Guidelines

When writing software that uses the "cups" library:

- Do not use undocumented or deprecated APIs,
- Do not rely on pre-configured printers,
- Do not assume that printers support specific features or formats, and
- Do not rely on implementation details (PPDs, etc.)

CUPS is designed to insulate users and developers from the implementation
details of printers and file formats.  The goal is to allow an application to
supply a print file in a standard format with the user intent ("print four
copies, two-sided on A4 media, and staple each copy") and have the printing
system manage the printer communication and format conversion needed.

Similarly, printer and job management applications can use standard query
operations to obtain the status information in a common, generic form and use
standard management operations to control the state of those printers and jobs.


## Terms Used in This Document

A *Destination* is a printer or print queue that accepts print jobs.  A
*Print Job* is one or more documents that are processed by a destination
using options supplied when creating the job.  A *Document* is a file (JPEG
image, PDF file, etc.) suitable for printing.  An *Option* controls some aspect
of printing, such as the media used. *Media* is the sheets or roll that is
printed on.  An *Attribute* is an option encoded for an Internet Printing
Protocol (IPP) request.


## Compiling Programs That Use the CUPS API

The CUPS libraries can be used from any C, C++, or Objective C program.
The method of compiling against the libraries varies depending on the
operating system and installation of CUPS. The following sections show how
to compile a simple program (shown below) in two common environments.

The following simple program lists the available destinations:

    #include <stdio.h>
    #include <cups/cups.h>

    int print_dest(void *user_data, unsigned flags, cups_dest_t *dest)
    {
      if (dest->instance)
        printf("%s/%s\n", dest->name, dest->instance);
      else
        puts(dest->name);

      return (1);
    }

    int main(void)
    {
      cupsEnumDests(CUPS_DEST_FLAGS_NONE, 1000, NULL, 0, 0, print_dest, NULL);

      return (0);
    }


### Compiling with Xcode

In Xcode, choose *New Project...* from the *File* menu (or press SHIFT+CMD+N),
then select the *Command Line Tool* under the macOS Application project type.
Click *Next* and enter a name for the project, for example "firstcups".  Click
*Next* and choose a project directory. The click *Next* to create the project.

In the project window, click on the *Build Phases* group and expand the
*Link Binary with Libraries* section. Click *+*, type "libcups" to show the
library, and then double-click on `libcups.tbd`.

Finally, click on the `main.c` file in the sidebar and copy the example program
to the file.  Build and run (CMD+R) to see the list of destinations.


### Compiling with GCC

From the command-line, create a file called `sample.c` using your favorite
editor, copy the example to this file, and save.  Then run the following command
to compile it with GCC and run it:

    gcc -o simple `cups-config --cflags` simple.c `cups-config --libs`
    ./simple

The `cups-config` command provides the compiler flags (`cups-config --cflags`)
and libraries (`cups-config --libs`) needed for the local system.


# Working with Destinations

Destinations, which in CUPS represent individual printers or classes
(collections or pools) of printers, are represented by the `cups_dest_t`
structure which includes the name \(`name`), instance \(`instance`, saved
options/settings), whether the destination is the default for the user
\(`is_default`), and the options and basic information associated with that
destination \(`num_options` and `options`).

Historically destinations have been manually maintained by the administrator of
a system or network, but CUPS also supports dynamic discovery of destinations on
the current network.


## Finding Available Destinations

The `cupsEnumDests` function finds all of the available destinations:

     int
     cupsEnumDests(unsigned flags, int msec, int *cancel,
                   cups_ptype_t type, cups_ptype_t mask,
                   cups_dest_cb_t cb, void *user_data)

The `flags` argument specifies enumeration options, which at present must be
`CUPS_DEST_FLAGS_NONE`.

The `msec` argument specifies the maximum amount of time that should be used for
enumeration in milliseconds - interactive applications should keep this value to
5000 or less when run on the main thread.

The `cancel` argument points to an integer variable that, when set to a non-zero
value, will cause enumeration to stop as soon as possible.  It can be `NULL` if
not needed.

The `type` and `mask` arguments are bitfields that allow the caller to filter
the destinations based on categories and/or capabilities.  The destination's
"printer-type" value is masked by the `mask` value and compared to the `type`
value when filtering.  For example, to only enumerate destinations that are
hosted on the local system, pass `CUPS_PRINTER_LOCAL` for the `type` argument
and `CUPS_PRINTER_DISCOVERED` for the `mask` argument.  The following constants
can be used for filtering:

- `CUPS_PRINTER_CLASS`: A collection of destinations.
- `CUPS_PRINTER_FAX`: A facsimile device.
- `CUPS_PRINTER_LOCAL`: A local printer or class.  This constant has the value 0
  (no bits set) and is only used for the `type` argument and is paired with the
  `CUPS_PRINTER_REMOTE` or `CUPS_PRINTER_DISCOVERED` constant passed in the
  `mask` argument.
- `CUPS_PRINTER_REMOTE`: A remote (shared) printer or class.
- `CUPS_PRINTER_DISCOVERED`: An available network printer or class.
- `CUPS_PRINTER_BW`: Can do B&W printing.
- `CUPS_PRINTER_COLOR`: Can do color printing.
- `CUPS_PRINTER_DUPLEX`: Can do two-sided printing.
- `CUPS_PRINTER_STAPLE`: Can staple output.
- `CUPS_PRINTER_COLLATE`: Can quickly collate copies.
- `CUPS_PRINTER_PUNCH`: Can punch output.
- `CUPS_PRINTER_COVER`: Can cover output.
- `CUPS_PRINTER_BIND`: Can bind output.
- `CUPS_PRINTER_SORT`: Can sort output (mailboxes, etc.)
- `CUPS_PRINTER_SMALL`: Can print on Letter/Legal/A4-size media.
- `CUPS_PRINTER_MEDIUM`: Can print on Tabloid/B/C/A3/A2-size media.
- `CUPS_PRINTER_LARGE`: Can print on D/E/A1/A0-size media.
- `CUPS_PRINTER_VARIABLE`: Can print on rolls and custom-size media.

The `cb` argument specifies a function to call for every destination that is
found:

    typedef int (*cups_dest_cb_t)(void *user_data,
                                  unsigned flags,
                                  cups_dest_t *dest);

The callback function receives a copy of the `user_data` argument along with a
bitfield \(`flags`) and the destination that was found.  The `flags` argument
can have any of the following constant (bit) values set:

- `CUPS_DEST_FLAGS_MORE`: There are more destinations coming.
- `CUPS_DEST_FLAGS_REMOVED`: The destination has gone away and should be removed
  from the list of destinations a user can select.
- `CUPS_DEST_FLAGS_ERROR`: An error occurred.  The reason for the error can be
  found by calling the `cupsLastError` and/or `cupsLastErrorString` functions.

The callback function returns 0 to stop enumeration or 1 to continue.

> Note that the callback function will likely be called multiple times for the
> same destination, so it is up to the caller to suppress any duplicate
> destinations.

The following example shows how to use `cupsEnumDests` to get a filtered array
of destinations:

    typedef struct
    {
      int num_dests;
      cups_dest_t *dests;
    } my_user_data_t;

    int
    my_dest_cb(my_user_data_t *user_data, unsigned flags,
               cups_dest_t *dest)
    {
      if (flags & CUPS_DEST_FLAGS_REMOVED)
      {
       /*
        * Remove destination from array...
        */

        user_data->num_dests =
            cupsRemoveDest(dest->name, dest->instance,
                           user_data->num_dests,
                           &(user_data->dests));
      }
      else
      {
       /*
        * Add destination to array...
        */

        user_data->num_dests =
            cupsCopyDest(dest, user_data->num_dests,
                         &(user_data->dests));
      }

      return (1);
    }

    int
    my_get_dests(cups_ptype_t type, cups_ptype_t mask,
                 cups_dest_t **dests)
    {
      my_user_data_t user_data = { 0, NULL };

      if (!cupsEnumDests(CUPS_DEST_FLAGS_NONE, 1000, NULL, type,
                         mask, (cups_dest_cb_t)my_dest_cb,
                         &user_data))
      {
       /*
        * An error occurred, free all of the destinations and
        * return...
        */

        cupsFreeDests(user_data.num_dests, user_dasta.dests);

        *dests = NULL;

        return (0);
      }

     /*
      * Return the destination array...
      */

      *dests = user_data.dests;

      return (user_data.num_dests);
    }


## Basic Destination Information

The `num_options` and `options` members of the `cups_dest_t` structure provide
basic attributes about the destination in addition to the user default options
and values for that destination.  The following names are predefined for various
destination attributes:

- "auth-info-required": The type of authentication required for printing to this
  destination: "none", "username,password", "domain,username,password", or
  "negotiate" (Kerberos).
- "printer-info": The human-readable description of the destination such as "My
  Laser Printer".
- "printer-is-accepting-jobs": "true" if the destination is accepting new jobs,
  "false" otherwise.
- "printer-is-shared": "true" if the destination is being shared with other
  computers, "false" otherwise.
- "printer-location": The human-readable location of the destination such as
  "Lab 4".
- "printer-make-and-model": The human-readable make and model of the destination
  such as "ExampleCorp LaserPrinter 4000 Series".
- "printer-state": "3" if the destination is idle, "4" if the destination is
  printing a job, and "5" if the destination is stopped.
- "printer-state-change-time": The UNIX time when the destination entered the
  current state.
- "printer-state-reasons": Additional comma-delimited state keywords for the
  destination such as "media-tray-empty-error" and "toner-low-warning".
- "printer-type": The `cups_ptype_t` value associated with the destination.
- "printer-uri-supported": The URI associated with the destination; if not set,
  this destination was discovered but is not yet setup as a local printer.

Use the `cupsGetOption` function to retrieve the value.  For example, the
following code gets the make and model of a destination:

    const char *model = cupsGetOption("printer-make-and-model",
                                      dest->num_options,
                                      dest->options);


## Detailed Destination Information

Once a destination has been chosen, the `cupsCopyDestInfo` function can be used
to gather detailed information about the destination:

    cups_dinfo_t *
    cupsCopyDestInfo(http_t *http, cups_dest_t *dest);

The `http` argument specifies a connection to the CUPS scheduler and is
typically the constant `CUPS_HTTP_DEFAULT`.  The `dest` argument specifies the
destination to query.

The `cups_dinfo_t` structure that is returned contains a snapshot of the
supported options and their supported, ready, and default values.  It also can
report constraints between different options and values, and recommend changes
to resolve those constraints.


### Getting Supported Options and Values

The `cupsCheckDestSupported` function can be used to test whether a particular
option or option and value is supported:

    int
    cupsCheckDestSupported(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *info,
                           const char *option,
                           const char *value);

The `option` argument specifies the name of the option to check.  The following
constants can be used to check the various standard options:

- `CUPS_COPIES`: Controls the number of copies that are produced.
- `CUPS_FINISHINGS`: A comma-delimited list of integer constants that control
  the finishing processes that are applied to the job, including stapling,
  punching, and folding.
- `CUPS_MEDIA`: Controls the media size that is used, typically one of the
  following: `CUPS_MEDIA_3X5`, `CUPS_MEDIA_4X6`, `CUPS_MEDIA_5X7`,
  `CUPS_MEDIA_8X10`, `CUPS_MEDIA_A3`, `CUPS_MEDIA_A4`, `CUPS_MEDIA_A5`,
  `CUPS_MEDIA_A6`, `CUPS_MEDIA_ENV10`, `CUPS_MEDIA_ENVDL`, `CUPS_MEDIA_LEGAL`,
  `CUPS_MEDIA_LETTER`, `CUPS_MEDIA_PHOTO_L`, `CUPS_MEDIA_SUPERBA3`, or
  `CUPS_MEDIA_TABLOID`.
- `CUPS_MEDIA_SOURCE`: Controls where the media is pulled from, typically either
  `CUPS_MEDIA_SOURCE_AUTO` or `CUPS_MEDIA_SOURCE_MANUAL`.
- `CUPS_MEDIA_TYPE`: Controls the type of media that is used, typically one of
  the following: `CUPS_MEDIA_TYPE_AUTO`, `CUPS_MEDIA_TYPE_ENVELOPE`,
  `CUPS_MEDIA_TYPE_LABELS`, `CUPS_MEDIA_TYPE_LETTERHEAD`,
  `CUPS_MEDIA_TYPE_PHOTO`, `CUPS_MEDIA_TYPE_PHOTO_GLOSSY`,
  `CUPS_MEDIA_TYPE_PHOTO_MATTE`, `CUPS_MEDIA_TYPE_PLAIN`, or
  `CUPS_MEDIA_TYPE_TRANSPARENCY`.
- `CUPS_NUMBER_UP`: Controls the number of document pages that are placed on
  each media side.
- `CUPS_ORIENTATION`: Controls the orientation of document pages placed on the
  media: `CUPS_ORIENTATION_PORTRAIT` or `CUPS_ORIENTATION_LANDSCAPE`.
- `CUPS_PRINT_COLOR_MODE`: Controls whether the output is in color
  \(`CUPS_PRINT_COLOR_MODE_COLOR`), grayscale
  \(`CUPS_PRINT_COLOR_MODE_MONOCHROME`), or either
  \(`CUPS_PRINT_COLOR_MODE_AUTO`).
- `CUPS_PRINT_QUALITY`: Controls the generate quality of the output:
  `CUPS_PRINT_QUALITY_DRAFT`, `CUPS_PRINT_QUALITY_NORMAL`, or
  `CUPS_PRINT_QUALITY_HIGH`.
- `CUPS_SIDES`: Controls whether prints are placed on one or both sides of the
  media: `CUPS_SIDES_ONE_SIDED`, `CUPS_SIDES_TWO_SIDED_PORTRAIT`, or
  `CUPS_SIDES_TWO_SIDED_LANDSCAPE`.

If the `value` argument is `NULL`, the `cupsCheckDestSupported` function returns
whether the option is supported by the destination.  Otherwise, the function
returns whether the specified value of the option is supported.

The `cupsFindDestSupported` function returns the IPP attribute containing the
supported values for a given option:

     ipp_attribute_t *
     cupsFindDestSupported(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *dinfo,
                           const char *option);

For example, the following code prints the supported finishing processes for a
destination, if any, to the standard output:

    cups_dinfo_t *info = cupsCopyDestInfo(CUPS_HTTP_DEFAULT,
                                          dest);

    if (cupsCheckDestSupported(CUPS_HTTP_DEFAULT, dest, info,
                               CUPS_FINISHINGS, NULL))
    {
      ipp_attribute_t *finishings =
          cupsFindDestSupported(CUPS_HTTP_DEFAULT, dest, info,
                                CUPS_FINISHINGS);
      int i, count = ippGetCount(finishings);

      puts("finishings supported:");
      for (i = 0; i < count; i ++)
        printf("  %d\n", ippGetInteger(finishings, i));
    }
    else
      puts("finishings not supported.");

The "job-creation-attributes" option can be queried to get a list of supported
options.  For example, the following code prints the list of supported options
to the standard output:

    ipp_attribute_t *attrs =
        cupsFindDestSupported(CUPS_HTTP_DEFAULT, dest, info,
                              "job-creation-attributes");
    int i, count = ippGetCount(attrs);

    for (i = 0; i < count; i ++)
      puts(ippGetString(attrs, i, NULL));


### Getting Default Values

There are two sets of default values - user defaults that are available via the
`num_options` and `options` members of the `cups_dest_t` structure, and
destination defaults that available via the `cups_dinfo_t` structure and the
`cupsFindDestDefault` function which returns the IPP attribute containing the
default value(s) for a given option:

    ipp_attribute_t *
    cupsFindDestDefault(http_t *http, cups_dest_t *dest,
                        cups_dinfo_t *dinfo,
                        const char *option);

The user defaults from `cupsGetOption` should always take preference over the
destination defaults.  For example, the following code prints the default
finishings value(s) to the standard output:

    const char *def_value =
        cupsGetOption(CUPS_FINISHINGS, dest->num_options,
                      dest->options);
    ipp_attribute_t *def_attr =
        cupsFindDestDefault(CUPS_HTTP_DEFAULT, dest, info,
                            CUPS_FINISHINGS);

    if (def_value != NULL)
    {
      printf("Default finishings: %s\n", def_value);
    }
    else
    {
      int i, count = ippGetCount(def_attr);

      printf("Default finishings: %d",
             ippGetInteger(def_attr, 0));
      for (i = 1; i < count; i ++)
        printf(",%d", ippGetInteger(def_attr, i));
      putchar('\n');
    }


### Getting Ready (Loaded) Values

The finishings and media options also support queries for the ready, or loaded,
values.  For example, a printer may have punch and staple finishers installed
but be out of staples - the supported values will list both punch and staple
finishing processes but the ready values will only list the punch processes.
Similarly, a printer may support hundreds of different sizes of media but only
have a single size loaded at any given time - the ready values are limited to
the media that is actually in the printer.

The `cupsFindDestReady` function finds the IPP attribute containing the ready
values for a given option:

    ipp_attribute_t *
    cupsFindDestReady(http_t *http, cups_dest_t *dest,
                      cups_dinfo_t *dinfo, const char *option);

For example, the following code lists the ready finishing processes:

    ipp_attribute_t *ready_finishings =
        cupsFindDestReady(CUPS_HTTP_DEFAULT, dest, info,
                          CUPS_FINISHINGS);

    if (ready_finishings != NULL)
    {
      int i, count = ippGetCount(ready_finishings);

      puts("finishings ready:");
      for (i = 0; i < count; i ++)
        printf("  %d\n", ippGetInteger(ready_finishings, i));
    }
    else
      puts("no finishings are ready.");


### Media Size Options

CUPS provides functions for querying the dimensions and margins for each of the
supported media size options.  The `cups_size_t` structure is used to describe a
media size:

    typedef struct cups_size_s
    {
      char media[128];
      int width, length;
      int bottom, left, right, top;
    } cups_size_t;

The `width` and `length` members specify the dimensions of the media in
hundredths of millimeters (1/2540th of an inch).  The `bottom`, `left`, `right`,
and `top` members specify the margins of the printable area, also in hundredths
of millimeters.

The `cupsGetDestMediaByName` and `cupsGetDestMediaBySize` functions lookup the
media size information using a standard media size name or dimensions in
hundredths of millimeters:

    int
    cupsGetDestMediaByName(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *dinfo,
                           const char *media,
                           unsigned flags, cups_size_t *size);

    int
    cupsGetDestMediaBySize(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *dinfo,
                           int width, int length,
                           unsigned flags, cups_size_t *size);

The `media`, `width`, and `length` arguments specify the size to lookup.  The
`flags` argument specifies a bitfield controlling various lookup options:

- `CUPS_MEDIA_FLAGS_DEFAULT`: Find the closest size supported by the printer.
- `CUPS_MEDIA_FLAGS_BORDERLESS`: Find a borderless size.
- `CUPS_MEDIA_FLAGS_DUPLEX`: Find a size compatible with two-sided printing.
- `CUPS_MEDIA_FLAGS_EXACT`: Find an exact match for the size.
- `CUPS_MEDIA_FLAGS_READY`: If the printer supports media sensing or
  configuration of the media in each tray/source, find the size amongst the
  "ready" media.

If a matching size is found for the destination, the size information is stored
in the structure pointed to by the `size` argument and 1 is returned.  Otherwise
0 is returned.

For example, the following code prints the margins for two-sided printing on US
Letter media:

    cups_size_t size;

    if (cupsGetDestMediaByName(CUPS_HTTP_DEFAULT, dest, info,
                               CUPS_MEDIA_LETTER,
                               CUPS_MEDIA_FLAGS_DUPLEX, &size))
    {
      puts("Margins for duplex US Letter:");
      printf("  Bottom: %.2fin\n", size.bottom / 2540.0);
      printf("    Left: %.2fin\n", size.left / 2540.0);
      printf("   Right: %.2fin\n", size.right / 2540.0);
      printf("     Top: %.2fin\n", size.top / 2540.0);
    }
    else
      puts("Margins for duplex US Letter are not available.");

You can also enumerate all of the sizes that match a given `flags` value using
the `cupsGetDestMediaByIndex` and `cupsGetDestMediaCount` functions:

    int
    cupsGetDestMediaByIndex(http_t *http, cups_dest_t *dest,
                            cups_dinfo_t *dinfo, int n,
                            unsigned flags, cups_size_t *size);

    int
    cupsGetDestMediaCount(http_t *http, cups_dest_t *dest,
                          cups_dinfo_t *dinfo, unsigned flags);

For example, the following code prints the list of ready media and corresponding
margins:

    cups_size_t size;
    int i;
    int count = cupsGetDestMediaCount(CUPS_HTTP_DEFAULT,
                                      dest, info,
                                      CUPS_MEDIA_FLAGS_READY);

    for (i = 0; i < count; i ++)
    {
      if (cupsGetDestMediaByIndex(CUPS_HTTP_DEFAULT, dest, info,
                                  i, CUPS_MEDIA_FLAGS_READY,
                                  &size))
      {
        printf("%s:\n", size.name);
        printf("   Width: %.2fin\n", size.width / 2540.0);
        printf("  Length: %.2fin\n", size.length / 2540.0);
        printf("  Bottom: %.2fin\n", size.bottom / 2540.0);
        printf("    Left: %.2fin\n", size.left / 2540.0);
        printf("   Right: %.2fin\n", size.right / 2540.0);
        printf("     Top: %.2fin\n", size.top / 2540.0);
      }
    }

Finally, the `cupsGetDestMediaDefault` function returns the default media size:

    int
    cupsGetDestMediaDefault(http_t *http, cups_dest_t *dest,
                            cups_dinfo_t *dinfo, unsigned flags,
                            cups_size_t *size);


### Localizing Options and Values

CUPS provides three functions to get localized, human-readable strings in the
user's current locale for options and values: `cupsLocalizeDestMedia`,
`cupsLocalizeDestOption`, and `cupsLocalizeDestValue`:

    const char *
    cupsLocalizeDestMedia(http_t *http, cups_dest_t *dest,
                          cups_dinfo_t *info, unsigned flags,
                          cups_size_t *size);

    const char *
    cupsLocalizeDestOption(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *info,
                           const char *option);

    const char *
    cupsLocalizeDestValue(http_t *http, cups_dest_t *dest,
                          cups_dinfo_t *info,
                          const char *option, const char *value);


## Submitting a Print Job

Once you are ready to submit a print job, you create a job using the
`cupsCreateDestJob` function:

    ipp_status_t
    cupsCreateDestJob(http_t *http, cups_dest_t *dest,
                      cups_dinfo_t *info, int *job_id,
                      const char *title, int num_options,
                      cups_option_t *options);

The `title` argument specifies a name for the print job such as "My Document".
The `num_options` and `options` arguments specify the options for the print
job which are allocated using the `cupsAddOption` function.

When successful, the job's numeric identifier is stored in the integer pointed
to by the `job_id` argument and `IPP_STATUS_OK` is returned.  Otherwise, an IPP
error status is returned.

For example, the following code creates a new job that will print 42 copies of a
two-sided US Letter document:

    int job_id = 0;
    int num_options = 0;
    cups_option_t *options = NULL;

    num_options = cupsAddOption(CUPS_COPIES, "42",
                                num_options, &options);
    num_options = cupsAddOption(CUPS_MEDIA, CUPS_MEDIA_LETTER,
                                num_options, &options);
    num_options = cupsAddOption(CUPS_SIDES,
                                CUPS_SIDES_TWO_SIDED_PORTRAIT,
                                num_options, &options);

    if (cupsCreateDestJob(CUPS_HTTP_DEFAULT, dest, info,
                          &job_id, "My Document", num_options,
                          options) == IPP_STATUS_OK)
      printf("Created job: %d\n", job_id);
    else
      printf("Unable to create job: %s\n",
             cupsLastErrorString());

Once the job is created, you submit documents for the job using the
`cupsStartDestDocument`, `cupsWriteRequestData`, and `cupsFinishDestDocument`
functions:

    http_status_t
    cupsStartDestDocument(http_t *http, cups_dest_t *dest,
                          cups_dinfo_t *info, int job_id,
                          const char *docname,
                          const char *format,
                          int num_options,
                          cups_option_t *options,
                          int last_document);

    http_status_t
    cupsWriteRequestData(http_t *http, const char *buffer,
                         size_t length);

    ipp_status_t
    cupsFinishDestDocument(http_t *http, cups_dest_t *dest,
                           cups_dinfo_t *info);

The `docname` argument specifies the name of the document, typically the
original filename.  The `format` argument specifies the MIME media type of the
document, including the following constants:

- `CUPS_FORMAT_JPEG`: "image/jpeg"
- `CUPS_FORMAT_PDF`: "application/pdf"
- `CUPS_FORMAT_POSTSCRIPT`: "application/postscript"
- `CUPS_FORMAT_TEXT`: "text/plain"

The `num_options` and `options` arguments specify per-document print options,
which at present must be 0 and `NULL`.  The `last_document` argument specifies
whether this is the last document in the job.

For example, the following code submits a PDF file to the job that was just
created:

    FILE *fp = fopen("filename.pdf", "rb");
    size_t bytes;
    char buffer[65536];

    if (cupsStartDestDocument(CUPS_HTTP_DEFAULT, dest, info,
                              job_id, "filename.pdf", 0, NULL,
                              1) == HTTP_STATUS_CONTINUE)
    {
      while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        if (cupsWriteRequestData(CUPS_HTTP_DEFAULT, buffer,
                                 bytes) != HTTP_STATUS_CONTINUE)
          break;

      if (cupsFinishDestDocument(CUPS_HTTP_DEFAULT, dest,
                                 info) == IPP_STATUS_OK)
        puts("Document send succeeded.");
      else
        printf("Document send failed: %s\n",
               cupsLastErrorString());
    }

    fclose(fp);


# Sending IPP Requests

CUPS provides a rich API for sending IPP requests to the scheduler or printers,
typically from management or utility applications whose primary purpose is not
to send print jobs.


## Connecting to the Scheduler or Printer

The connection to the scheduler or printer is represented by the HTTP connection
type `http_t`.  The `cupsConnectDest` function connects to the scheduler or
printer associated with the destination:

    http_t *
    cupsConnectDest(cups_dest_t *dest, unsigned flags, int msec,
                    int *cancel, char *resource,
                    size_t resourcesize, cups_dest_cb_t cb,
                    void *user_data);

The `dest` argument specifies the destination to connect to.

The `flags` argument specifies whether you want to connect to the scheduler
(`CUPS_DEST_FLAGS_NONE`) or device/printer (`CUPS_DEST_FLAGS_DEVICE`) associated
with the destination.

The `msec` argument specifies how long you are willing to wait for the
connection to be established in milliseconds.  Specify a value of `-1` to wait
indefinitely.

The `cancel` argument specifies the address of an integer variable that can be
set to a non-zero value to cancel the connection.  Specify a value of `NULL`
to not provide a cancel variable.

The `resource` and `resourcesize` arguments specify the address and size of a
character string array to hold the path to use when sending an IPP request.

The `cb` and `user_data` arguments specify a destination callback function that
returns 1 to continue connecting or 0 to stop.  The destination callback work
the same way as the one used for the `cupsEnumDests` function.

On success, a HTTP connection is returned that can be used to send IPP requests
and get IPP responses.

For example, the following code connects to the printer associated with a
destination with a 30 second timeout:

    char resource[256];
    http_t *http = cupsConnectDest(dest, CUPS_DEST_FLAGS_DEVICE,
                                   30000, NULL, resource,
                                   sizeof(resource), NULL, NULL);


## Creating an IPP Request

IPP requests are represented by the IPP message type `ipp_t` and each IPP
attribute in the request is representing using the type `ipp_attribute_t`.  Each
IPP request includes an operation code (`IPP_OP_CREATE_JOB`,
`IPP_OP_GET_PRINTER_ATTRIBUTES`, etc.) and a 32-bit integer identifier.

The `ippNewRequest` function creates a new IPP request:

    ipp_t *
    ippNewRequest(ipp_op_t op);

The `op` argument specifies the IPP operation code for the request.  For
example, the following code creates an IPP Get-Printer-Attributes request:

    ipp_t *request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

The request identifier is automatically set to a unique value for the current
process.

Each IPP request starts with two IPP attributes, "attributes-charset" and
"attributes-natural-language", followed by IPP attribute(s) that specify the
target of the operation.  The `ippNewRequest` automatically adds the correct
"attributes-charset" and "attributes-natural-language" attributes, but you must
add the target attribute(s).  For example, the following code adds the
"printer-uri" attribute to the IPP Get-Printer-Attributes request to specify
which printer is being queried:

    const char *printer_uri = cupsGetOption("device-uri",
                                            dest->num_options,
                                            dest->options);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, printer_uri);

> Note: If we wanted to query the scheduler instead of the device, we would look
> up the "printer-uri-supported" option instead of the "device-uri" value.

The `ippAddString` function adds the "printer-uri" attribute the the IPP
request.  The `IPP_TAG_OPERATION` argument specifies that the attribute is part
of the operation.  The `IPP_TAG_URI` argument specifies that the value is a
Universal Resource Identifier (URI) string.  The `NULL` argument specifies there
is no language (English, French, Japanese, etc.) associated with the string, and
the `printer_uri` argument specifies the string value.

The IPP Get-Printer-Attributes request also supports an IPP attribute called
"requested-attributes" that lists the attributes and values you are interested
in.  For example, the following code requests the printer state attributes:

    static const char * const requested_attributes[] =
    {
      "printer-state",
      "printer-state-message",
      "printer-state-reasons"
    };

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes", 3, NULL,
                  requested_attributes);

The `ippAddStrings` function adds an attribute with one or more strings, in this
case three.  The `IPP_TAG_KEYWORD` argument specifies that the strings are
keyword values, which are used for attribute names.  All strings use the same
language (`NULL`), and the attribute will contain the three strings in the
array `requested_attributes`.

CUPS provides many functions to adding attributes of different types:

- `ippAddBoolean` adds a boolean (`IPP_TAG_BOOLEAN`) attribute with one value.
- `ippAddInteger` adds an enum (`IPP_TAG_ENUM`) or integer (`IPP_TAG_INTEGER`)
  attribute with one value.
- `ippAddIntegers` adds an enum or integer attribute with one or more values.
- `ippAddOctetString` adds an octetString attribute with one value.
- `ippAddOutOfBand` adds a admin-defined (`IPP_TAG_ADMINDEFINE`), default
  (`IPP_TAG_DEFAULT`), delete-attribute (`IPP_TAG_DELETEATTR`), no-value
  (`IPP_TAG_NOVALUE`), not-settable (`IPP_TAG_NOTSETTABLE`), unknown
  (`IPP_TAG_UNKNOWN`), or unsupported (`IPP_TAG_UNSUPPORTED_VALUE`) out-of-band
  attribute.
- `ippAddRange` adds a rangeOfInteger attribute with one range.
- `ippAddRanges` adds a rangeOfInteger attribute with one or more ranges.
- `ippAddResolution` adds a resolution attribute with one resolution.
- `ippAddResolutions` adds a resolution attribute with one or more resolutions.
- `ippAddString` adds a charset (`IPP_TAG_CHARSET`), keyword (`IPP_TAG_KEYWORD`),
  mimeMediaType (`IPP_TAG_MIMETYPE`), name (`IPP_TAG_NAME` and
  `IPP_TAG_NAMELANG`), naturalLanguage (`IPP_TAG_NATURAL_LANGUAGE`), text
  (`IPP_TAG_TEXT` and `IPP_TAG_TEXTLANG`), uri (`IPP_TAG_URI`), or uriScheme
  (`IPP_TAG_URISCHEME`) attribute with one value.
- `ippAddStrings` adds a charset, keyword, mimeMediaType, name, naturalLanguage,
  text, uri, or uriScheme attribute with one or more values.


## Sending the IPP Request

Once you have created the IPP request, you can send it using the
`cupsDoRequest` function.  For example, the following code sends the IPP
Get-Printer-Attributes request to the destination and saves the response:

    ipp_t *response = cupsDoRequest(http, request, resource);

For requests like Send-Document that include a file, the `cupsDoFileRequest`
function should be used:

    ipp_t *response = cupsDoFileRequest(http, request, resource,
                                        filename);

Both `cupsDoRequest` and `cupsDoFileRequest` free the IPP request.  If a valid
IPP response is received, it is stored in a new IPP message (`ipp_t`) and
returned to the caller.  Otherwise `NULL` is returned.

The status from the most recent request can be queried using the `cupsLastError`
function, for example:

    if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      /* request failed */
    }

A human-readable error message is also available using the `cupsLastErrorString`
function:

    if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
    {
      /* request failed */
      printf("Request failed: %s\n", cupsLastErrorString());
    }


## Processing the IPP Response

Each response to an IPP request is also an IPP message (`ipp_t`) with its own
IPP attributes (`ipp_attribute_t`) that includes a status code (`IPP_STATUS_OK`,
`IPP_STATUS_ERROR_BAD_REQUEST`, etc.) and the corresponding 32-bit integer
identifier from the request.

For example, the following code finds the printer state attributes and prints
their values:

    ipp_attribute_t *attr;

    if ((attr = ippFindAttribute(response, "printer-state",
                                 IPP_TAG_ENUM)) != NULL)
    {
      printf("printer-state=%s\n",
             ippEnumString("printer-state", ippGetInteger(attr, 0)));
    }
    else
      puts("printer-state=unknown");

    if ((attr = ippFindAttribute(response, "printer-state-message",
                                 IPP_TAG_TEXT)) != NULL)
    {
      printf("printer-state-message=\"%s\"\n",
             ippGetString(attr, 0, NULL)));
    }

    if ((attr = ippFindAttribute(response, "printer-state-reasons",
                                 IPP_TAG_KEYWORD)) != NULL)
    {
      int i, count = ippGetCount(attr);

      puts("printer-state-reasons=");
      for (i = 0; i < count; i ++)
        printf("    %s\n", ippGetString(attr, i, NULL)));
    }

The `ippGetCount` function returns the number of values in an attribute.

The `ippGetInteger` and `ippGetString` functions return a single integer or
string value from an attribute.

The `ippEnumString` function converts a enum value to its keyword (string)
equivalent.

Once you are done using the IPP response message, free it using the `ippDelete`
function:

    ippDelete(response);


## Authentication

CUPS normally handles authentication through the console.  GUI applications
should set a password callback using the `cupsSetPasswordCB2` function:

    void
    cupsSetPasswordCB2(cups_password_cb2_t cb, void *user_data);

The password callback will be called when needed and is responsible for setting
the current user name using `cupsSetUser` and returning a string:

    const char *
    cups_password_cb2(const char *prompt, http_t *http,
                      const char *method, const char *resource,
                      void *user_data);

The `prompt` argument is a string from CUPS that should be displayed to the
user.

The `http` argument is the connection hosting the request that is being
authenticated.  The password callback can call the `httpGetField` and
`httpGetSubField` functions to look for additional details concerning the
authentication challenge.

The `method` argument specifies the HTTP method used for the request and is
typically "POST".

The `resource` argument specifies the path used for the request.

The `user_data` argument provides the user data pointer from the
`cupsSetPasswordCB2` call.
