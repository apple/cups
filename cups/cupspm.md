---
title: CUPS Programming Manual
author: Michael R Sweet
copyright: Copyright (c) 2007-2017 by Apple Inc. All Rights Reserved.
version: 2.2.4
...

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


## Finding Available Destinations


## Getting Information About a Destination


### Getting Supported Options and Values

cupsCheckDestSupported and cups

### Getting Default Options and Values

### Getting Ready Options and Values

### Media Options

### Other Standard Options

### Localizing Options and Values

## Submitting a Print Job

## Canceling a Print Job


# IPP Requests and Responses

Why you'd want to do this, etc.


## Connecting to a Destination

## Sending an IPP Request

## Getting the IPP Response

## Handling Authentication

## Handling Certificate Validation



















<h3><a name='PRINTERS_AND_CLASSES'>Printers and Classes</a></h3>

<p>Printers and classes (collections of printers) are accessed through
the <a href="#cups_dest_t"><code>cups_dest_t</code></a> structure which
includes the name (<code>name</code>), instance (<code>instance</code> -
a way of selecting certain saved options/settings), and the options and
attributes associated with that destination (<code>num_options</code> and
<code>options</code>). Destinations are created using the
<a href="#cupsGetDests"><code>cupsGetDests</code></a> function and freed
using the <a href='#cupsFreeDests'><code>cupsFreeDests</code></a> function.
The <a href='#cupsGetDest'><code>cupsGetDest</code></a> function finds a
specific destination for printing:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dests;
int num_dests = <a href='#cupsGetDests'>cupsGetDests</a>(&amp;dests);
<a href='#cups_dest_t'>cups_dest_t</a> *dest = <a href='#cupsGetDest'>cupsGetDest</a>("name", NULL, num_dests, dests);

/* do something with dest */

<a href='#cupsFreeDests'>cupsFreeDests</a>(num_dests, dests);
</pre>

<p>Passing <code>NULL</code> to
<a href='#cupsGetDest'><code>cupsGetDest</code></a> for the destination name
will return the default destination. Similarly, passing a <code>NULL</code>
instance will return the default instance for that destination.</p>

<div class='table'><table summary='Table 1: Printer Attributes' width='80%'>
<caption>Table 1: <a name='TABLE1'>Printer Attributes</a></caption>
<thead>
<tr>
	<th>Attribute Name</th>
	<th>Description</th>
</tr>
</thead>
<tbody>
<tr>
	<td>"auth-info-required"</td>
	<td>The type of authentication required for printing to this
	destination: "none", "username,password", "domain,username,password",
	or "negotiate" (Kerberos)</td>
</tr>
<tr>
	<td>"printer-info"</td>
	<td>The human-readable description of the destination such as "My
	Laser Printer".</td>
</tr>
<tr>
	<td>"printer-is-accepting-jobs"</td>
	<td>"true" if the destination is accepting new jobs, "false" if
	not.</td>
</tr>
<tr>
	<td>"printer-is-shared"</td>
	<td>"true" if the destination is being shared with other computers,
	"false" if not.</td>
</tr>
<tr>
	<td>"printer-location"</td>
	<td>The human-readable location of the destination such as "Lab 4".</td>
</tr>
<tr>
	<td>"printer-make-and-model"</td>
	<td>The human-readable make and model of the destination such as "HP
	LaserJet 4000 Series".</td>
</tr>
<tr>
	<td>"printer-state"</td>
	<td>"3" if the destination is idle, "4" if the destination is printing
	a job, and "5" if the destination is stopped.</td>
</tr>
<tr>
	<td>"printer-state-change-time"</td>
	<td>The UNIX time when the destination entered the current state.</td>
</tr>
<tr>
	<td>"printer-state-reasons"</td>
	<td>Additional comma-delimited state keywords for the destination
	such as "media-tray-empty-error" and "toner-low-warning".</td>
</tr>
<tr>
	<td>"printer-type"</td>
	<td>The <a href='#cups_printer_t'><code>cups_printer_t</code></a>
	value associated with the destination.</td>
</tr>
</tbody>
</table></div>

<h3><a name='OPTIONS'>Options</a></h3>

<p>Options are stored in arrays of
<a href='#cups_option_t'><code>cups_option_t</code></a> structures. Each
option has a name (<code>name</code>) and value (<code>value</code>)
associated with it. The <a href='#cups_dest_t'><code>cups_dest_t</code></a>
<code>num_options</code> and <code>options</code> members contain the
default options for a particular destination, along with several informational
attributes about the destination as shown in <a href='#TABLE1'>Table 1</a>.
The <a href='#cupsGetOption'><code>cupsGetOption</code></a> function gets
the value for the named option. For example, the following code lists the
available destinations and their human-readable descriptions:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dests;
int num_dests = <a href='#cupsGetDests'>cupsGetDests</a>(&amp;dests);
<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int i;
const char *value;

for (i = num_dests, dest = dests; i > 0; i --, dest ++)
  if (dest->instance == NULL)
  {
    value = <a href='#cupsGetOption'>cupsGetOption</a>("printer-info", dest->num_options, dest->options);
    printf("%s (%s)\n", dest->name, value ? value : "no description");
  }

<a href='#cupsFreeDests'>cupsFreeDests</a>(num_dests, dests);
</pre>

<p>You can create your own option arrays using the
<a href='#cupsAddOption'><code>cupsAddOption</code></a> function, which
adds a single named option to an array:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

int num_options = 0;
<a href='#cups_option_t'>cups_option_t</a> *options = NULL;

/* The returned num_options value is updated as needed */
num_options = <a href='#cupsAddOption'>cupsAddOption</a>("first", "value", num_options, &amp;options);

/* This adds a second option value */
num_options = <a href='#cupsAddOption'>cupsAddOption</a>("second", "value", num_options, &amp;options);

/* This replaces the first option we added */
num_options = <a href='#cupsAddOption'>cupsAddOption</a>("first", "new value", num_options, &amp;options);
</pre>

<p>Use a <code>for</code> loop to copy the options from a destination:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

int i;
int num_options = 0;
<a href='#cups_option_t'>cups_option_t</a> *options = NULL;
<a href='#cups_dest_t'>cups_dest_t</a> *dest;

for (i = 0; i &lt; dest->num_options; i ++)
  num_options = <a href='#cupsAddOption'>cupsAddOption</a>(dest->options[i].name, dest->options[i].value,
                              num_options, &amp;options);
</pre>

<p>Use the <a href='#cupsFreeOptions'><code>cupsFreeOptions</code></a>
function to free the options array when you are done using it:</p>

<pre class='example'>
<a href='#cupsFreeOptions'>cupsFreeOptions</a>(num_options, options);
</pre>

<h3><a name='PRINT_JOBS'>Print Jobs</a></h3>

<p>Print jobs are identified by a locally-unique job ID number from 1 to
2<sup>31</sup>-1 and have options and one or more files for printing to a
single destination. The <a href='#cupsPrintFile'><code>cupsPrintFile</code></a>
function creates a new job with one file. The following code prints the CUPS
test page file:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int num_options;
<a href='#cups_option_t'>cups_option_t</a> *options;
int job_id;

/* Print a single file */
job_id = <a href='#cupsPrintFile'>cupsPrintFile</a>(dest->name, "/usr/share/cups/data/testprint.ps",
                        "Test Print", num_options, options);
</pre>

<p>The <a href='#cupsPrintFiles'><code>cupsPrintFiles</code></a> function
creates a job with multiple files. The files are provided in a
<code>char *</code> array:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int num_options;
<a href='#cups_option_t'>cups_option_t</a> *options;
int job_id;
char *files[3] = { "file1.pdf", "file2.pdf", "file3.pdf" };

/* Print three files */
job_id = <a href='#cupsPrintFiles'>cupsPrintFiles</a>(dest->name, 3, files, "Test Print", num_options, options);
</pre>

<p>Finally, the <a href='#cupsCreateJob'><code>cupsCreateJob</code></a>
function creates a new job with no files in it. Files are added using the
<a href='#cupsStartDocument'><code>cupsStartDocument</code></a>,
<a href='api-httpipp.html#cupsWriteRequestData'><code>cupsWriteRequestData</code></a>,
and <a href='#cupsFinishDocument'><code>cupsFinishDocument</code></a> functions.
The following example creates a job with 10 text files for printing:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int num_options;
<a href='#cups_option_t'>cups_option_t</a> *options;
int job_id;
int i;
char buffer[1024];

/* Create the job */
job_id = <a href='#cupsCreateJob'>cupsCreateJob</a>(CUPS_HTTP_DEFAULT, dest->name, "10 Text Files",
                       num_options, options);

/* If the job is created, add 10 files */
if (job_id > 0)
{
  for (i = 1; i &lt;= 10; i ++)
  {
    snprintf(buffer, sizeof(buffer), "file%d.txt", i);

    <a href='#cupsStartDocument'>cupsStartDocument</a>(CUPS_HTTP_DEFAULT, dest->name, job_id, buffer,
                      CUPS_FORMAT_TEXT, i == 10);

    snprintf(buffer, sizeof(buffer),
             "File %d\n"
             "\n"
             "One fish,\n"
             "Two fish,\n
             "Red fish,\n
             "Blue fish\n", i);

    /* cupsWriteRequestData can be called as many times as needed */
    <a href='#cupsWriteRequestData'>cupsWriteRequestData</a>(CUPS_HTTP_DEFAULT, buffer, strlen(buffer));

    <a href='#cupsFinishDocument'>cupsFinishDocument</a>(CUPS_HTTP_DEFAULT, dest->name);
  }
}
</pre>

<p>Once you have created a job, you can monitor its status using the
<a href='#cupsGetJobs'><code>cupsGetJobs</code></a> function, which returns
an array of <a href='#cups_job_t'><code>cups_job_t</code></a> structures.
Each contains the job ID (<code>id</code>), destination name
(<code>dest</code>), title (<code>title</code>), and other information
associated with the job. The job array is freed using the
<a href='#cupsFreeJobs'><code>cupsFreeJobs</code></a> function. The following
example monitors a specific job ID, showing the current job state once every
5 seconds until the job is completed:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int job_id;
int num_jobs;
<a href='#cups_job_t'>cups_job_t</a> *jobs;
int i;
ipp_jstate_t job_state = IPP_JOB_PENDING;

while (job_state &lt; IPP_JOB_STOPPED)
{
  /* Get my jobs (1) with any state (-1) */
  num_jobs = <a href='#cupsGetJobs'>cupsGetJobs</a>(&amp;jobs, dest->name, 1, -1);

  /* Loop to find my job */
  job_state = IPP_JOB_COMPLETED;

  for (i = 0; i &lt; num_jobs; i ++)
    if (jobs[i].id == job_id)
    {
      job_state = jobs[i].state;
      break;
    }

  /* Free the job array */
  <a href='#cupsFreeJobs'>cupsFreeJobs</a>(num_jobs, jobs);

  /* Show the current state */
  switch (job_state)
  {
    case IPP_JOB_PENDING :
        printf("Job %d is pending.\n", job_id);
        break;
    case IPP_JOB_HELD :
        printf("Job %d is held.\n", job_id);
        break;
    case IPP_JOB_PROCESSING :
        printf("Job %d is processing.\n", job_id);
        break;
    case IPP_JOB_STOPPED :
        printf("Job %d is stopped.\n", job_id);
        break;
    case IPP_JOB_CANCELED :
        printf("Job %d is canceled.\n", job_id);
        break;
    case IPP_JOB_ABORTED :
        printf("Job %d is aborted.\n", job_id);
        break;
    case IPP_JOB_COMPLETED :
        printf("Job %d is completed.\n", job_id);
        break;
  }

  /* Sleep if the job is not finished */
  if (job_state &lt; IPP_JOB_STOPPED)
    sleep(5);
}
</pre>

<p>To cancel a job, use the
<a href='#cupsCancelJob'><code>cupsCancelJob</code></a> function with the
job ID:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

<a href='#cups_dest_t'>cups_dest_t</a> *dest;
int job_id;

<a href='#cupsCancelJob'>cupsCancelJob</a>(dest->name, job_id);
</pre>

<h3><a name='ERROR_HANDLING'>Error Handling</a></h3>

<p>If any of the CUPS API printing functions returns an error, the reason for
that error can be found by calling the
<a href='#cupsLastError'><code>cupsLastError</code></a> and
<a href='#cupsLastErrorString'><code>cupsLastErrorString</code></a> functions.
<a href='#cupsLastError'><code>cupsLastError</code></a> returns the last IPP
error code
(<a href='api-httpipp.html#ipp_status_t'><code>ipp_status_t</code></a>)
that was encountered, while
<a href='#cupsLastErrorString'><code>cupsLastErrorString</code></a> returns
a (localized) human-readable string that can be shown to the user. For example,
if any of the job creation functions returns a job ID of 0, you can use
<a href='#cupsLastErrorString'><code>cupsLastErrorString</code></a> to show
the reason why the job could not be created:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

int job_id;

if (job_id == 0)
  puts(cupsLastErrorString());
</pre>

<h3><a name='PASSWORDS_AND_AUTHENTICATION'>Passwords and Authentication</a></h3>

<p>CUPS supports authentication of any request, including submission of print
jobs. The default mechanism for getting the username and password is to use the
login user and a password from the console.</p>

<p>To support other types of applications, in particular Graphical User
Interfaces ("GUIs"), the CUPS API provides functions to set the default
username and to register a callback function that returns a password string.</p>

<p>The <a href="#cupsSetPasswordCB"><code>cupsSetPasswordCB</code></a>
function is used to set a password callback in your program. Only one
function can be used at any time.</p>

<p>The <a href="#cupsSetUser"><code>cupsSetUser</code></a> function sets the
current username for authentication. This function can be called by your
password callback function to change the current username as needed.</p>

<p>The following example shows a simple password callback that gets a
username and password from the user:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;

const char *
my_password_cb(const char *prompt)
{
  char	user[65];


  puts(prompt);

  /* Get a username from the user */
  printf("Username: ");
  if (fgets(user, sizeof(user), stdin) == NULL)
    return (NULL);

  /* Strip the newline from the string and set the user */
  user[strlen(user) - 1] = '\0';

  <a href='#cupsSetUser'>cupsSetUser</a>(user);

  /* Use getpass() to ask for the password... */
  return (getpass("Password: "));
}

<a href='#cupsSetPasswordCB'>cupsSetPasswordCB</a>(my_password_cb);
</pre>

<p>Similarly, a GUI could display the prompt string in a window with input
fields for the username and password. The username should default to the
string returned by the <a href="#cupsUser"><code>cupsUser</code></a>
function.</p>
<!--
  HTTP and IPP API introduction for CUPS.

  Copyright 2007-2012 by Apple Inc.
  Copyright 1997-2006 by Easy Software Products, all rights reserved.

  These coded instructions, statements, and computer programs are the
  property of Apple Inc. and are protected by Federal copyright
  law.  Distribution and use rights are outlined in the file "LICENSE.txt"
  which should have been included with this file.  If this file is
  file is missing or damaged, see the license at "http://www.cups.org/".
-->

<h2 class='title'><a name='OVERVIEW'>Overview</a></h2>

<p>The CUPS HTTP and IPP APIs provide low-level access to the HTTP and IPP
protocols and CUPS scheduler. They are typically used by monitoring and
administration programs to perform specific functions not supported by the
high-level CUPS API functions.</p>

<p>The HTTP APIs use an opaque structure called
<a href='#http_t'><code>http_t</code></a> to manage connections to
a particular HTTP or IPP server. The
<a href='#httpConnectEncrypt'><code>httpConnectEncrypt</code></a> function is
used to create an instance of this structure for a particular server.
The constant <code>CUPS_HTTP_DEFAULT</code> can be used with all of the
<code>cups</code> functions to refer to the default CUPS server - the functions
create a per-thread <a href='#http_t'><code>http_t</code></a> as needed.</p>

<p>The IPP APIs use two opaque structures for requests (messages sent to the CUPS scheduler) and responses (messages sent back to your application from the scheduler). The <a href='#ipp_t'><code>ipp_t</code></a> type holds a complete request or response and is allocated using the <a href='#ippNew'><code>ippNew</code></a> or <a href='#ippNewRequest'><code>ippNewRequest</code></a> functions and freed using the <a href='#ippDelete'><code>ippDelete</code></a> function.</p>

<p>The second opaque structure is called <a href='#ipp_attribute_t'><code>ipp_attribute_t</code></a> and holds a single IPP attribute which consists of a group tag (<a href='#ippGetGroupTag'><code>ippGetGroupTag</code></a>), a value type tag (<a href='#ippGetValueTag'><code>ippGetValueTag</code></a>), the attribute name (<a href='#ippGetName'><code>ippGetName</code></a>), and 1 or more values (<a href='#ippGetCount'><code>ippGetCount</code></a>, <a href='#ippGetBoolean'><code>ippGetBoolean</code></a>, <a href='#ippGetCollection'><code>ippGetCollection</code></a>, <a href='#ippGetDate'><code>ippGetDate</code></a>, <a href='#ippGetInteger'><code>ippGetInteger</code></a>, <a href='#ippGetRange'><code>ippGetRange</code></a>, <a href='#ippGetResolution'><code>ippGetResolution</code></a>, and <a href='#ippGetString'><code>ippGetString</code></a>). Attributes are added to an <a href='#ipp_t'><code>ipp_t</code></a> pointer using one of the <code>ippAdd</code> functions. For example, use <a href='#ippAddString'><code>ippAddString</code></a> to add the "printer-uri" and "requesting-user-name" string attributes to a request:</p>

<pre class='example'>
<a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(IPP_GET_JOBS);

<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
             NULL, "ipp://localhost/printers/");
<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
             NULL, cupsUser());
</pre>

<p>Once you have created an IPP request, use the <code>cups</code> functions to send the request to and read the response from the server. For example, the <a href='#cupsDoRequest'><code>cupsDoRequest</code></a> function can be used for simple query operations that do not involve files:</p>

<pre class='example'>
#include &lt;cups/cups.h&gt;


<a href='#ipp_t'>ipp_t</a> *<a name='get_jobs'>get_jobs</a>(void)
{
  <a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(IPP_GET_JOBS);

  <a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/printers/");
  <a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  return (<a href='#cupsDoRequest'>cupsDoRequest</a>(CUPS_HTTP_DEFAULT, request, "/"));
}
</pre>

<p>The <a href='#cupsDoRequest'><code>cupsDoRequest</code></a> function frees the request and returns an IPP response or <code>NULL</code> pointer if the request could not be sent to the server. Once you have a response from the server, you can either use the <a href='#ippFindAttribute'><code>ippFindAttribute</code></a> and <a href='#ippFindNextAttribute'><code>ippFindNextAttribute</code></a> functions to find specific attributes, for example:</p>

<pre class='example'>
<a href='#ipp_t'>ipp_t</a> *response;
<a href='#ipp_attribute_t'>ipp_attribute_t</a> *attr;

attr = <a href='#ippFindAttribute'>ippFindAttribute</a>(response, "printer-state", IPP_TAG_ENUM);
</pre>

<p>You can also walk the list of attributes with a simple <code>for</code> loop like this:</p>

<pre class='example'>
<a href='#ipp_t'>ipp_t</a> *response;
<a href='#ipp_attribute_t'>ipp_attribute_t</a> *attr;

for (attr = <a href='#ippFirstAttribute'>ippFirstAttribute</a>(response); attr != NULL; attr = <a href='#ippNextAttribute'>ippNextAttribute</a>(response))
  if (ippGetName(attr) == NULL)
    puts("--SEPARATOR--");
  else
    puts(ippGetName(attr));
</pre>

<p>The <code>for</code> loop approach is normally used when collecting attributes for multiple objects (jobs, printers, etc.) in a response. Attributes with <code>NULL</code> names indicate a separator between the attributes of each object. For example, the following code will list the jobs returned from our previous <a href='#get_jobs'><code>get_jobs</code></a> example code:</p>

<pre class='example'>
<a href='#ipp_t'>ipp_t</a> *response = <a href='#get_jobs'>get_jobs</a>();

if (response != NULL)
{
  <a href='#ipp_attribute_t'>ipp_attribute_t</a> *attr;
  const char *attrname;
  int job_id = 0;
  const char *job_name = NULL;
  const char *job_originating_user_name = NULL;

  puts("Job ID  Owner             Title");
  puts("------  ----------------  ---------------------------------");

  for (attr = <a href='#ippFirstAttribute'>ippFirstAttribute</a>(response); attr != NULL; attr = <a href='#ippNextAttribute'>ippNextAttribute</a>(response))
  {
   /* Attributes without names are separators between jobs */
    attrname = ippGetName(attr);
    if (attrname == NULL)
    {
      if (job_id > 0)
      {
        if (job_name == NULL)
          job_name = "(withheld)";

        if (job_originating_user_name == NULL)
          job_originating_user_name = "(withheld)";

        printf("%5d  %-16s  %s\n", job_id, job_originating_user_name, job_name);
      }

      job_id = 0;
      job_name = NULL;
      job_originating_user_name = NULL;
      continue;
    }
    else if (!strcmp(attrname, "job-id") &amp;&amp; ippGetValueTag(attr) == IPP_TAG_INTEGER)
      job_id = ippGetInteger(attr, 0);
    else if (!strcmp(attrname, "job-name") &amp;&amp; ippGetValueTag(attr) == IPP_TAG_NAME)
      job_name = ippGetString(attr, 0, NULL);
    else if (!strcmp(attrname, "job-originating-user-name") &amp;&amp;
             ippGetValueTag(attr) == IPP_TAG_NAME)
      job_originating_user_name = ippGetString(attr, 0, NULL);
  }

  if (job_id > 0)
  {
    if (job_name == NULL)
      job_name = "(withheld)";

    if (job_originating_user_name == NULL)
      job_originating_user_name = "(withheld)";

    printf("%5d  %-16s  %s\n", job_id, job_originating_user_name, job_name);
  }
}
</pre>

<h3><a name='CREATING_URI_STRINGS'>Creating URI Strings</a></h3>

<p>To ensure proper encoding, the
<a href='#httpAssembleURIf'><code>httpAssembleURIf</code></a> function must be
used to format a "printer-uri" string for all printer-based requests:</p>

<pre class='example'>
const char *name = "Foo";
char uri[1024];
<a href='#ipp_t'>ipp_t</a> *request;

<a href='#httpAssembleURIf'>httpAssembleURIf</a>(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, cupsServer(),
                 ippPort(), "/printers/%s", name);
<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
</pre>

<h3><a name='SENDING_REQUESTS_WITH_FILES'>Sending Requests with Files</a></h3>

<p>The <a href='#cupsDoFileRequest'><code>cupsDoFileRequest</code></a> and
<a href='#cupsDoIORequest'><code>cupsDoIORequest</code></a> functions are
used for requests involving files. The
<a href='#cupsDoFileRequest'><code>cupsDoFileRequest</code></a> function
attaches the named file to a request and is typically used when sending a print
file or changing a printer's PPD file:</p>

<pre class='example'>
const char *filename = "/usr/share/cups/data/testprint.ps";
const char *name = "Foo";
char uri[1024];
char resource[1024];
<a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(IPP_PRINT_JOB);
<a href='#ipp_t'>ipp_t</a> *response;

/* Use httpAssembleURIf for the printer-uri string */
<a href='#httpAssembleURIf'>httpAssembleURIf</a>(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, cupsServer(),
                 ippPort(), "/printers/%s", name);
<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
             NULL, cupsUser());
<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
             NULL, "testprint.ps");

/* Use snprintf for the resource path */
snprintf(resource, sizeof(resource), "/printers/%s", name);

response = <a href='#cupsDoFileRequest'>cupsDoFileRequest</a>(CUPS_HTTP_DEFAULT, request, resource, filename);
</pre>

<p>The <a href='#cupsDoIORequest'><code>cupsDoIORequest</code></a> function
optionally attaches a file to the request and optionally saves a file in the
response from the server. It is used when using a pipe for the request
attachment or when using a request that returns a file, currently only
<code>CUPS_GET_DOCUMENT</code> and <code>CUPS_GET_PPD</code>. For example,
the following code will download the PPD file for the sample HP LaserJet
printer driver:</p>

<pre class='example'>
char tempfile[1024];
int tempfd;
<a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(CUPS_GET_PPD);
<a href='#ipp_t'>ipp_t</a> *response;

<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "ppd-name",
             NULL, "laserjet.ppd");

tempfd = cupsTempFd(tempfile, sizeof(tempfile));

response = <a href='#cupsDoIORequest'>cupsDoIORequest</a>(CUPS_HTTP_DEFAULT, request, "/", -1, tempfd);
</pre>

<p>The example passes <code>-1</code> for the input file descriptor to specify
that no file is to be attached to the request. The PPD file attached to the
response is written to the temporary file descriptor we created using the
<code>cupsTempFd</code> function.</p>

<h3><a name='ASYNCHRONOUS_REQUEST_PROCESSING'>Asynchronous Request Processing</a></h3>

<p>The <a href='#cupsSendRequest'><code>cupsSendRequest</code></a> and
<a href='#cupsGetResponse'><code>cupsGetResponse</code></a> support
asynchronous communications with the server. Unlike the other request
functions, the IPP request is not automatically freed, so remember to
free your request with the <a href='#ippDelete'><code>ippDelete</code></a>
function.</p>

<p>File data is attached to the request using the
<a href='#cupsWriteRequestData'><code>cupsWriteRequestData</code></a>
function, while file data returned from the server is read using the
<a href='#cupsReadResponseData'><code>cupsReadResponseData</code></a>
function. We can rewrite the previous <code>CUPS_GET_PPD</code> example
to use the asynchronous functions quite easily:</p>

<pre class='example'>
char tempfile[1024];
int tempfd;
<a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(CUPS_GET_PPD);
<a href='#ipp_t'>ipp_t</a> *response;

<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "ppd-name",
             NULL, "laserjet.ppd");

tempfd = cupsTempFd(tempfile, sizeof(tempfile));

if (<a href='#cupsSendRequest'>cupsSendRequest</a>(CUPS_HTTP_DEFAULT, request, "/") == HTTP_CONTINUE)
{
  response = <a href='#cupsGetResponse'>cupsGetResponse</a>(CUPS_HTTP_DEFAULT, "/");

  if (response != NULL)
  {
    ssize_t bytes;
    char buffer[8192];

    while ((bytes = <a href='#cupsReadResponseData'>cupsReadResponseData</a>(CUPS_HTTP_DEFAULT, buffer, sizeof(buffer))) > 0)
      write(tempfd, buffer, bytes);
  }
}

/* Free the request! */
<a href='#ippDelete'>ippDelete</a>(request);
</pre>

<p>The <a href='#cupsSendRequest'><code>cupsSendRequest</code></a> function
returns the initial HTTP request status, typically either
<code>HTTP_CONTINUE</code> or <code>HTTP_UNAUTHORIZED</code>. The latter status
is returned when the request requires authentication of some sort. The
<a href='#cupsDoAuthentication'><code>cupsDoAuthentication</code></a> function
must be called when your see <code>HTTP_UNAUTHORIZED</code> and the request
re-sent. We can add authentication support to our example code by using a
<code>do ... while</code> loop:</p>

<pre class='example'>
char tempfile[1024];
int tempfd;
<a href='#ipp_t'>ipp_t</a> *request = <a href='#ippNewRequest'>ippNewRequest</a>(CUPS_GET_PPD);
<a href='#ipp_t'>ipp_t</a> *response;
http_status_t status;

<a href='#ippAddString'>ippAddString</a>(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "ppd-name",
             NULL, "laserjet.ppd");

tempfd = cupsTempFd(tempfile, sizeof(tempfile));

/* Loop for authentication */
do
{
  status = <a href='#cupsSendRequest'>cupsSendRequest</a>(CUPS_HTTP_DEFAULT, request, "/");

  if (status == HTTP_UNAUTHORIZED)
  {
    /* Try to authenticate, break out of the loop if that fails */
    if (<a href='#cupsDoAuthentication'>cupsDoAuthentication</a>(CUPS_HTTP_DEFAULT, "POST", "/"))
      break;
  }
}
while (status != HTTP_CONTINUE &amp;&amp; status != HTTP_UNAUTHORIZED);

if (status == HTTP_CONTINUE)
{
  response = <a href='#cupsGetResponse'>cupsGetResponse</a>(CUPS_HTTP_DEFAULT, "/");

  if (response != NULL)
  {
    ssize_t bytes;
    char buffer[8192];

    while ((bytes = <a href='#cupsReadResponseData'>cupsReadResponseData</a>(CUPS_HTTP_DEFAULT, buffer, sizeof(buffer))) > 0)
      write(tempfd, buffer, bytes);
  }
}

/* Free the request! */
<a href='#ippDelete'>ippDelete</a>(request);
</pre>
<!--
  File and directory API introduction for CUPS.

  Copyright 2007-2011 by Apple Inc.
  Copyright 1997-2005 by Easy Software Products, all rights reserved.

  These coded instructions, statements, and computer programs are the
  property of Apple Inc. and are protected by Federal copyright
  law.  Distribution and use rights are outlined in the file "LICENSE.txt"
  which should have been included with this file.  If this file is
  file is missing or damaged, see the license at "http://www.cups.org/".
-->

<h2 class='title'><a name="OVERVIEW">Overview</a></h2>

<p>The CUPS file and directory APIs provide portable interfaces
for manipulating files and listing files and directories. Unlike
stdio <code>FILE</code> streams, the <code>cupsFile</code> functions
allow you to open more than 256 files at any given time. They
also manage the platform-specific details of locking, large file
support, line endings (CR, LF, or CR LF), and reading and writing
files using Flate ("gzip") compression. Finally, you can also
connect, read from, and write to network connections using the
<code>cupsFile</code> functions.</p>

<p>The <code>cupsDir</code> functions manage the platform-specific
details of directory access/listing and provide a convenient way
to get both a list of files and the information (permissions,
size, timestamp, etc.) for each of those files.</p>
<!--
  Array API introduction for CUPS.

  Copyright 2007-2011 by Apple Inc.
  Copyright 1997-2006 by Easy Software Products, all rights reserved.

  These coded instructions, statements, and computer programs are the
  property of Apple Inc. and are protected by Federal copyright
  law.  Distribution and use rights are outlined in the file "LICENSE.txt"
  which should have been included with this file.  If this file is
  file is missing or damaged, see the license at "http://www.cups.org/".
-->

<h2 class='title'><a name='OVERVIEW'>Overview</a></h2>

<p>The CUPS array API provides a high-performance generic array container.
The contents of the array container can be sorted and the container itself is
designed for optimal speed and memory usage under a wide variety of conditions.
Sorted arrays use a binary search algorithm from the last found or inserted
element to quickly find matching elements in the array. Arrays created with the
optional hash function can often find elements with a single lookup. The
<a href='#cups_array_t'><code>cups_array_t</code></a> type is used when
referring to a CUPS array.</p>

<p>The CUPS scheduler (<tt>cupsd</tt>) and many of the CUPS API
functions use the array API to efficiently manage large lists of
data.</p>

<h3><a name='MANAGING_ARRAYS'>Managing Arrays</a></h3>

<p>Arrays are created using either the
<a href='#cupsArrayNew'><code>cupsArrayNew</code></a>,
<a href='#cupsArrayNew2'><code>cupsArrayNew2</code></a>, or
<a href='#cupsArrayNew2'><code>cupsArrayNew3</code></a> functions. The
first function creates a new array with the specified callback function
and user data pointer:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

static int compare_func(void *first, void *second, void *user_data);

void *user_data;
<a href='#cups_array_t'>cups_array_t</a> *array = <a href='#cupsArrayNew'>cupsArrayNew</a>(compare_func, user_data);
</pre>

<p>The comparison function (type
<a href="#cups_arrayfunc_t"><code>cups_arrayfunc_t</code></a>) is called
whenever an element is added to the array and can be <code>NULL</code> to
create an unsorted array. The function returns -1 if the first element should
come before the second, 0 if the first and second elements should have the same
ordering, and 1 if the first element should come after the second.</p>

<p>The "user_data" pointer is passed to your comparison function. Pass
<code>NULL</code> if you do not need to associate the elements in your array
with additional information.</p>

<p>The <a href='#cupsArrayNew2'><code>cupsArrayNew2</code></a> function adds
two more arguments to support hashed lookups, which can potentially provide
instantaneous ("O(1)") lookups in your array:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

#define HASH_SIZE 512 /* Size of hash table */

static int compare_func(void *first, void *second, void *user_data);
static int hash_func(void *element, void *user_data);

void *user_data;
<a href='#cups_array_t'>cups_array_t</a> *hash_array = <a href='#cupsArrayNew2'>cupsArrayNew2</a>(compare_func, user_data, hash_func, HASH_SIZE);
</pre>

<p>The hash function (type
<a href="#cups_ahash_func_t"><code>cups_ahash_func_t</code></a>) should return a
number from 0 to (hash_size-1) that (hopefully) uniquely identifies the
element and is called whenever you look up an element in the array with
<a href='#cupsArrayFind'><code>cupsArrayFind</code></a>. The hash size is
only limited by available memory, but generally should not be larger than
16384 to realize any performance improvement.</p>

<p>The <a href='#cupsArrayNew3'><code>cupsArrayNew3</code></a> function adds
copy and free callbacks to support basic memory management of elements:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

#define HASH_SIZE 512 /* Size of hash table */

static int compare_func(void *first, void *second, void *user_data);
static void *copy_func(void *element, void *user_data);
static void free_func(void *element, void *user_data);
static int hash_func(void *element, void *user_data);

void *user_data;
<a href='#cups_array_t'>cups_array_t</a> *array = <a href='#cupsArrayNew3'>cupsArrayNew3</a>(compare_func, user_data, NULL, 0, copy_func, free_func);

<a href='#cups_array_t'>cups_array_t</a> *hash_array = <a href='#cupsArrayNew3'>cupsArrayNew3</a>(compare_func, user_data, hash_func, HASH_SIZE, copy_func, free_func);
</pre>

<p>Once you have created the array, you add elements using the
<a href='#cupsArrayAdd'><code>cupsArrayAdd</code></a>
<a href='#cupsArrayInsert'><code>cupsArrayInsert</code></a> functions.
The first function adds an element to the array, adding the new element
after any elements that have the same order, while the second inserts the
element before others with the same order. For unsorted arrays,
<a href='#cupsArrayAdd'><code>cupsArrayAdd</code></a> appends the element to
the end of the array while
<a href='#cupsArrayInsert'><code>cupsArrayInsert</code></a> inserts the
element at the beginning of the array. For example, the following code
creates a sorted array of character strings:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

/* Use strcmp() to compare strings - it will ignore the user_data pointer */
<a href='#cups_array_t'>cups_array_t</a> *array = <a href='#cupsArrayNew'>cupsArrayNew</a>((<a href='#cups_array_func_t'>cups_array_func_t</a>)strcmp, NULL);

/* Add four strings to the array */
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "One Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Two Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Red Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Blue Fish");
</pre>

<p>Elements are removed using the
<a href='#cupsArrayRemove'><code>cupsArrayRemove</code></a> function, for
example:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

/* Use strcmp() to compare strings - it will ignore the user_data pointer */
<a href='#cups_array_t'>cups_array_t</a> *array = <a href='#cupsArrayNew'>cupsArrayNew</a>((<a href='#cups_array_func_t'>cups_array_func_t</a>)strcmp, NULL);

/* Add four strings to the array */
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "One Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Two Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Red Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Blue Fish");

/* Remove "Red Fish" */
<a href='#cupsArrayRemove'>cupsArrayRemove</a>(array, "Red Fish");
</pre>

<p>Finally, you free the memory used by the array using the
<a href='#cupsArrayDelete'><code>cupsArrayDelete</code></a> function. All
of the memory for the array and hash table (if any) is freed, however <em>CUPS
does not free the elements unless you provide copy and free functions</em>.</p>

<h3><a name='FINDING_AND_ENUMERATING'>Finding and Enumerating Elements</a></h3>

<p>CUPS provides several functions to find and enumerate elements in an
array. Each one sets or updates a "current index" into the array, such that
future lookups will start where the last one left off:</p>

<dl>
	<dt><a href='#cupsArrayFind'><code>cupsArrayFind</code></a></dt>
	<dd>Returns the first matching element.</dd>
	<dt><a href='#cupsArrayFirst'><code>cupsArrayFirst</code></a></dt>
	<dd>Returns the first element in the array.</dd>
	<dt><a href='#cupsArrayIndex'><code>cupsArrayIndex</code></a></dt>
	<dd>Returns the Nth element in the array, starting at 0.</dd>
	<dt><a href='#cupsArrayLast'><code>cupsArrayLast</code></a></dt>
	<dd>Returns the last element in the array.</dd>
	<dt><a href='#cupsArrayNext'><code>cupsArrayNext</code></a></dt>
	<dd>Returns the next element in the array.</dd>
	<dt><a href='#cupsArrayPrev'><code>cupsArrayPrev</code></a></dt>
	<dd>Returns the previous element in the array.</dd>
</dl>

<p>Each of these functions returns <code>NULL</code> when there is no
corresponding element.  For example, a simple <code>for</code> loop using the
<a href='#cupsArrayFirst'><code>cupsArrayFirst</code></a> and
<a href='#cupsArrayNext'><code>cupsArrayNext</code></a> functions will
enumerate all of the strings in our previous example:</p>

<pre class='example'>
#include &lt;cups/array.h&gt;

/* Use strcmp() to compare strings - it will ignore the user_data pointer */
<a href='#cups_array_t'>cups_array_t</a> *array = <a href='#cupsArrayNew'>cupsArrayNew</a>((<a href='#cups_array_func_t'>cups_array_func_t</a>)strcmp, NULL);

/* Add four strings to the array */
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "One Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Two Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Red Fish");
<a href='#cupsArrayAdd'>cupsArrayAdd</a>(array, "Blue Fish");

/* Show all of the strings in the array */
char *s;
for (s = (char *)<a href='#cupsArrayFirst'>cupsArrayFirst</a>(array); s != NULL; s = (char *)<a href='#cupsArrayNext'>cupsArrayNext</a>(array))
  puts(s);
</pre>
