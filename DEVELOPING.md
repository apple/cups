Developing for CUPS
===================

Please see the [Contributing to CUPS](CONTRIBUTING.md) file for information on
contributing to the CUPS project.


How To Contact The Developers
-----------------------------

The CUPS mailing lists are the primary means of asking questions and informally
discussing issues and feature requests with the CUPS developers and other
experienced CUPS users and developers.  The "cups" mailing list is intended for
CUPS usage questions and new software announcements while the "cups-devel"
mailing list provides a forum for CUPS developers and monitoring new bugs.


Interfaces
----------

CUPS interfaces, including the C APIs and command-line arguments, environment
variables, configuration files, and output format, are stable across patch
versions and are generally backwards-compatible with interfaces used in prior
major and minor versions.  However, program interfaces such as those used by
the scheduler to run filter, port monitor, and backend processes for job
processing should only be considered stable from the point of view of a
filter, port monitor, or backend.  Software that simulates the scheduler in
order to run those programs outside of CUPS must necessarily be updated when
the corresponding interface is changed in a subsequent CUPS release, otherwise
undefined behavior can occur.

CUPS C APIs starting with an underscore (`_`) are considered to be private to
CUPS and are not subject to the normal guarantees of stability between CUPS
releases and must never be used in non-CUPS source code.  Similarly,
configuration and state files written by CUPS are considered private if a
corresponding man page is not provided with the CUPS release.  Never rely on
undocumented files or formats when developing software for CUPS.  Always use a
published C API to access data stored in a file to avoid compatibility problems
in the future.


Build System
------------

The CUPS build system uses GNU autoconf to tailor the library to the local
operating system.  Project files for the current release of Microsoft Visual
Studio are also provided for Microsoft Windows®.  To improve portability,
makefiles must not make use of features unique to GNU make.  See the MAKEFILE
GUIDELINES section for a description of the allowed make features and makefile
guidelines.

Additional GNU build programs such as GNU automake and GNU libtool must not be
used.  GNU automake produces non-portable makefiles which depend on GNU-
specific extensions, and GNU libtool is not portable or reliable enough for
CUPS.


Version Numbering
-----------------

CUPS uses a three-part version number separated by periods to represent the
major, minor, and patch release numbers.  Major release numbers indicate large
design changes or backwards-incompatible changes to the CUPS API or CUPS
Imaging API.  Minor release numbers indicate new features and other smaller
changes which are backwards-compatible with previous CUPS releases.  Patch
numbers indicate bug fixes to the previous feature or patch release.

> Note:
>
> When we talk about compatibility, we are talking about binary compatibility
> for public APIs and output format compatibility for program interfaces.
> Changes to configuration file formats or the default behavior of programs
> are not generally considered incompatible as the upgrade process can
> normally address such changes gracefully.

Production releases use the plain version numbers:

    MAJOR.MINOR.PATCH
    1.0.0
    ...
    1.1.0
    ...
    1.1.23
    ...
    2.0.0
    ...
    2.1.0
    2.1.1
    2.1.2
    2.1.3

The first production release in a MAJOR.MINOR series (MAJOR.MINOR.0) is called
a feature release.  Feature releases are the only releases that may contain new
features.  Subsequent production releases in a MAJOR.MINOR series may only
contain bug fixes.

Beta-test releases are identified by appending the letter B to the major and
minor version numbers followed by the beta release number:

    MAJOR.MINORbNUMBER
    2.2b1

Release candidates are identified by appending the letters RC to the major and
minor version numbers followed by the release candidate number:

    MAJOR.MINORrcNUMBER
    2.2rc1


Coding Guidelines
-----------------

Contributed source code must follow the guidelines below.  While the examples
are for C and C++ source files, source code for other languages should conform
to the same guidelines as allowed by the language.


### Source Files

All source files names must be 16 characters or less in length to ensure
compatibility with older UNIX filesystems.  Source files containing functions
have an extension of ".c" for C and ".cxx" for C++ source files.  All other
"include" files have an extension of ".h".  Tabs are set to 8 characters or
columns.

> Note:
>
> The ".cxx" extension is used because it is the only common C++ extension
> between Linux, macOS, UNIX, and Windows.

The top of each source file contains a header giving the purpose or nature of
the source file and the copyright and licensing notice:

    /*
     * Description of file contents.
     *
     * Copyright 2017 by Apple Inc.
     *
     * Licensed under Apache License v2.0.  See the file "LICENSE" for more
     * information.
     */


### Header Files

All public header files must include the "versioning.h" header file, or a header
that does so.  Function declarations are then "decorated" with the correct
`_CUPS_API_major_minor` macro to define its availability based on the build
environment, for example:

    extern int cupsDoThis(int foo, int bar) _CUPS_API_2_2;

Private API header files must be named with the suffix "-private", for example
the "cups.h" header file defines all of the public CUPS APIs while the
"cups-private.h" header file defines all of the private CUPS APIs as well.
Typically a private API header file will include the corresponding public API
header file.


### Comments

All source code utilizes block comments within functions to describe the
operations being performed by a group of statements; avoid putting a comment
per line unless absolutely necessary, and then consider refactoring the code
so that it is not necessary.  C source files use the block comment format
("/* comment */") since many vendor C compilers still do not support C99/C++
comments ("// comment"):

    /*
     * Clear the state array before we begin...
     */

     for (i = 0; i < (sizeof(array) / sizeof(sizeof(array[0])); i ++)
       array[i] = CUPS_STATE_IDLE;

    /*
     * Wait for state changes on another thread...
     */

     do
     {
       for (i = 0; i < (sizeof(array) / sizeof(sizeof(array[0])); i ++)
         if (array[i] != CUPS_STATE_IDLE)
           break;

       if (i == (sizeof(array) / sizeof(array[0])))
         sleep(1);
     } while (i == (sizeof(array) / sizeof(array[0])));


### Indentation

All code blocks enclosed by brackets begin with the opening brace on a new
line.  The code then follows starting on a new line after the brace and is
indented 2 spaces.  The closing brace is then placed on a new line following
the code at the original indentation:

    {
      int i; /* Looping var */

     /*
      * Process foobar values from 0 to 999...
      */

      for (i = 0; i < 1000; i ++)
      {
        do_this(i);
        do_that(i);
      }
    }

Single-line statements following "do", "else", "for", "if", and "while" are
indented 2 spaces as well.  Blocks of code in a "switch" block are indented 4
spaces after each "case" and "default" case:

    switch (array[i])
    {
      case CUPS_STATE_IDLE :
          do_this(i);
          do_that(i);
          break;

      default :
          do_nothing(i);
          break;
    }


### Spacing

A space follows each reserved word such as `if`, `while`, etc.  Spaces are not
inserted between a function name and the arguments in parenthesis.


### Return Values

Parenthesis surround values returned from a function:

    return (CUPS_STATE_IDLE);


### Functions

Functions with a global scope have a lowercase prefix followed by capitalized
words, e.g., `cupsDoThis`, `cupsDoThat`, `cupsDoSomethingElse`, etc.  Private
global functions begin with a leading underscore, e.g., `_cupsDoThis`,
`_cupsDoThat`, etc.

Functions with a local scope are declared static with lowercase names and
underscores between words, e.g., `do_this`, `do_that`, `do_something_else`, etc.

Each function begins with a comment header describing what the function does,
the possible input limits (if any), the possible output values (if any), and
any special information needed:

    /*
     * 'do_this()' - Compute y = this(x).
     *
     * Notes: none.
     */

    static float       /* O - Inverse power value, 0.0 <= y <= 1.1 */
    do_this(float x)   /* I - Power value (0.0 <= x <= 1.1) */
    {
      ...
      return (y);
    }

Return/output values are indicated using an "O" prefix, input values are
indicated using the "I" prefix, and values that are both input and output use
the "IO" prefix for the corresponding in-line comment.

The Mini-XML documentation generator also understands the following special
text in the function description comment:

    @deprecated@         - Marks the function as deprecated (not recommended
                           for new development and scheduled for removal)
    @since CUPS version@ - Marks the function as new in the specified version
                           of CUPS.
    @private@            - Marks the function as private (same as starting the
                           function name with an underscore)


### Variables

Variables with a global scope are capitalized, e.g., `ThisVariable`,
`ThatVariable`, `ThisStateVariable`, etc.  Globals in CUPS libraries are either
part of the per-thread global values managed by the `_cupsGlobals` function
or are suitably protected for concurrent access.  Global variables should be
replaced by function arguments whenever possible.

Variables with a local scope are lowercase with underscores between words,
e.g., `this_variable`, `that_variable`, etc.  Any "local global" variables
shared by functions within a source file are declared static.  As for global
variables, local static variables are suitably protected for concurrent access.

Each variable is declared on a separate line and is immediately followed by a
comment block describing the variable:

    int         ThisVariable;    /* The current state of this */
    static int  that_variable;   /* The current state of that */


### Types

All type names are lowercase with underscores between words and `_t` appended
to the end of the name, e.g., `cups_this_type_t`, `cups_that_type_t`, etc.
Type names start with a prefix, typically `cups` or the name of the program,
to avoid conflicts with system types.  Private type names start with an
underscore, e.g., `_cups_this_t`, `_cups_that_t`, etc.

Each type has a comment block immediately after the typedef:

    typedef int cups_this_type_t;  /* This type is for CUPS foobar options. */


### Structures

All structure names are lowercase with underscores between words and `_s`
appended to the end of the name, e.g., `cups_this_s`, `cups_that_s`, etc.
Structure names start with a prefix, typically `cups` or the name of the
program, to avoid conflicts with system types.  Private structure names start
with an underscore, e.g., `_cups_this_s`, `_cups_that_s`, etc.

Each structure has a comment block immediately after the struct and each member
is documented similar to the variable naming policy above:

    struct cups_this_struct_s  /* This structure is for CUPS foobar options. */
    {
      int this_member;         /* Current state for this */
      int that_member;         /* Current state for that */
    };


### Constants

All constant names are uppercase with underscores between words, e.g.,
`CUPS_THIS_CONSTANT`, `CUPS_THAT_CONSTANT`, etc.  Constants begin with an
uppercase prefix, typically `CUPS_` or the program or type name.  Private
constants start with an underscore, e.g., `_CUPS_THIS_CONSTANT`,
`_CUPS_THAT_CONSTANT`, etc.

Typed enumerations should be used whenever possible to allow for type checking
by the compiler.

Comment blocks immediately follow each constant:

    typedef enum cups_tray_e  /* Tray enumerations */
    {
      CUPS_TRAY_THIS,         /* This tray */
      CUPS_TRAY_THAT          /* That tray */
    } cups_tray_t;


## Makefile Guidelines

The following is a guide to the makefile-based build system used by CUPS.
These standards have been developed over the years to allow CUPS to be built on
as many systems and environments as possible.


### General Organization

The CUPS source code is organized functionally into a top-level makefile,
include file, and subdirectories each with their own makefile and dependencies
files.  The ".in" files are template files for the autoconf software and are
used to generate a static version of the corresponding file.


### Makefile Documentation

Each makefile starts with the standard CUPS header containing the description
of the file, and CUPS copyright and license notice:

    #
    # Makefile for ...
    #
    # Copyright 2017 by Apple Inc.
    #
    # Licensed under Apache License v2.0.  See the file "LICENSE" for more
    # information.
    #


### Portable Makefile Construction

CUPS uses a common subset of make program syntax to ensure that the software
can be compiled "out of the box" on as many systems as possible.  The following
is a list of assumptions we follow when constructing makefiles:

- Targets; we assume that the make program supports the notion of simple
  targets of the form "name:" that perform tab-indented commands that follow
  the target, e.g.:

      target:
      TAB target commands

- Dependencies; we assume that the make program supports recursive dependencies
  on targets, e.g.:

      target: foo bar
      TAB target commands

      foo: bla
      TAB foo commands

      bar:
      TAB bar commands

      bla:
      TAB bla commands

- Variable Definition; we assume that the make program supports variable
  definition on the command-line or in the makefile using the following form:

      name=value

- Variable Substitution; we assume that the make program supports variable
  substitution using the following forms:

      - `$(name)`; substitutes the value of "name",
      - `$(name:.old=.new)`; substitutes the value of "name" with the filename
        extension ".old" changed to ".new",
      - `$(MAKEFLAGS)`; substitutes the command-line options passed to the
        program without the leading hyphen (-),
      - `$$`; substitutes a single $ character,
      - `$<`; substitutes the current source file or dependency, and
      - `$@`; substitutes the current target name.

- Suffixes; we assume that the make program supports filename suffixes with
  assumed dependencies, e.g.:

      .SUFFIXES: .c .o
      .c.o:
      TAB $(CC) $(CFLAGS) -o $@ -c $<

- Include Files; we assume that the make program supports the include
  directive, e.g.:

      include ../Makedefs
      include Dependencies

- Comments; we assume that comments begin with a # character and proceed to the
  end of the current line.

- Line Length; we assume that there is no practical limit to the length of
  lines.

- Continuation of long lines; we assume that the `\` character may be placed at
  the end of a line to concatenate two or more lines in a makefile to form a
  single long line.

- Shell; we assume a POSIX-compatible shell is present on the build system.


### Standard Variables

The following variables are defined in the "Makedefs" file generated by the
autoconf software:

- `ALL_CFLAGS`; the combined C compiler options,
- `ALL_CXXFLAGS`; the combined C++ compiler options,
- `AMANDIR`; the administrative man page installation directory (section 8/1m
  depending on the platform),
- `AR`; the library archiver command,
- `ARFLAGS`; options for the library archiver command,
- `AWK`; the local awk command,
- `BINDIR`; the binary installation directory,
- `BUILDROOT`; optional installation prefix (defaults to DSTROOT),
- `CC`; the C compiler command,
- `CFLAGS`; options for the C compiler command,
- `CHMOD`; the chmod command,
- `CXX`; the C++ compiler command,
- `CXXFLAGS`; options for the C++ compiler command,
- `DATADIR`; the data file installation directory,
- `DSO`; the C shared library building command,
- `DSOXX`; the C++ shared library building command,
- `DSOFLAGS`; options for the shared library building command,
- `INCLUDEDIR`; the public header file installation directory,
- `INSTALL`; the install command,
- `INSTALL_BIN`; the program installation command,
- `INSTALL_COMPDATA`; the compressed data file installation command,
- `INSTALL_CONFIG`; the configuration file installation command,
- `INSTALL_DATA`; the data file installation command,
- `INSTALL_DIR`; the directory installation command,
- `INSTALL_LIB`; the library installation command,
- `INSTALL_MAN`; the documentation installation command,
- `INSTALL_SCRIPT`; the shell script installation command,
- `LD`; the linker command,
- `LDFLAGS`; options for the linker,
- `LIBDIR`; the library installation directory,
- `LIBS`; libraries for all programs,
- `LN`; the ln command,
- `MAN1EXT`; extension for man pages in section 1,
- `MAN3EXT`; extension for man pages in section 3,
- `MAN5EXT`; extension for man pages in section 5,
- `MAN7EXT`; extension for man pages in section 7,
- `MAN8DIR`; subdirectory for man pages in section 8,
- `MAN8EXT`; extension for man pages in section 8,
- `MANDIR`; the man page installation directory,
- `OPTIM`; common compiler optimization options,
- `PRIVATEINCLUDE`; the private header file installation directory,
- `RM`; the rm command,
- `SHELL`; the sh (POSIX shell) command,
- `STRIP`; the strip command,
- `srcdir`; the source directory.


### Standard Targets

The following standard targets are defined in each makefile:

- `all`; creates all target programs, libraries, and documentation files,
- `clean`; removes all target programs libraries, documentation files, and object
  files,
- `depend`; generates automatic dependencies for any C or C++ source files (also
  see "DEPENDENCIES"),
- `distclean`; removes autoconf-generated files in addition to those removed by
  the "clean" target,
- `install`; installs all distribution files in their corresponding locations
  (also see "INSTALL/UNINSTALL SUPPORT"),
- `install-data`; installs all data files in their corresponding locations (also
  see "INSTALL/UNINSTALL SUPPORT"),
- `install-exec`; installs all executable files in their corresponding locations
  (also see "INSTALL/UNINSTALL SUPPORT"),
- `install-headers`; installs all include files in their corresponding locations
  (also see "INSTALL/UNINSTALL SUPPORT"),
- `install-libs`; installs all library files in their corresponding locations
  (also see "INSTALL/UNINSTALL SUPPORT"), and
- `uninstall`; removes all distribution files from their corresponding locations
  (also see "INSTALL/UNINSTALL SUPPORT").


### Object Files

Object files (the result of compiling a C or C++ source file) have the
extension ".o".


### Programs

Program files are the result of linking object files and libraries together to
form an executable file.  A typical program target looks like:

    program: $(OBJS)
    TAB echo Linking $@...
    TAB $(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

### Static Libraries

Static libraries have a prefix of "lib" and the extension ".a". A typical
static library target looks like:

    libname.a: $(OBJECTS)
    TAB echo Creating $@...
    TAB $(RM) $@
    TAB $(AR) $(ARFLAGS) $@ $(OBJECTS)
    TAB $(RANLIB) $@

### Shared Libraries

Shared libraries have a prefix of "lib" and the extension ".dylib" or ".so"
depending on the operating system.  A typical shared library is composed of
several targets that look like:

    libname.so: $(OBJECTS)
    TAB echo $(DSOCOMMAND) libname.so.$(DSOVERSION) ...
    TAB $(DSOCOMMAND) libname.so.$(DSOVERSION) $(OBJECTS)
    TAB $(RM) libname.so libname.so.$(DSOMAJOR)
    TAB $(LN) libname.so.$(DSOVERSION) libname.so.$(DSOMAJOR)
    TAB $(LN) libname.so.$(DSOVERSION) libname.so

    libname.dylib: $(OBJECTS)
    TAB echo $(DSOCOMMAND) libname.$(DSOVERSION).dylib ...
    TAB $(DSOCOMMAND) libname.$(DSOVERSION).dylib \
    TAB TAB -install_name $(libdir)/libname.$(DSOMAJOR).dylib \
    TAB TAB -current_version libname.$(DSOVERSION).dylib \
    TAB TAB -compatibility_version $(DSOMAJOR).0 \
    TAB TAB $(OBJECTS) $(LIBS)
    TAB $(RM) libname.dylib
    TAB $(RM) libname.$(DSOMAJOR).dylib
    TAB $(LN) libname.$(DSOVERSION).dylib libname.$(DSOMAJOR).dylib
    TAB $(LN) libname.$(DSOVERSION).dylib libname.dylib

### Dependencies

Static dependencies are expressed in each makefile following the target, for
example:

    foo: bar

Static dependencies are only used when it is not possible to automatically
generate them.  Automatic dependencies are stored in a file named
"Dependencies" and included at the end of the makefile.  The following "depend"
target rule is used to create the automatic dependencies:

    depend:
    TAB $(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies

We regenerate the automatic dependencies on an macOS system and express any
non-macOS dependencies statically in the makefile.


### Install/Uninstall Support

All makefiles contains install and uninstall rules which install or remove the
corresponding software.  These rules must use the $(BUILDROOT) variable as a
prefix to any installation directory so that CUPS can be installed in a
temporary location for packaging by programs like rpmbuild.

The `INSTALL_BIN`, `INSTALL_COMPDATA`, `INSTALL_CONFIG`, `INSTALL_DATA`,
`INSTALL_DIR`, `INSTALL_LIB`, `INSTALL_MAN`, and `INSTALL_SCRIPT` variables
must be used when installing files so that the proper ownership and permissions
are set on the installed files.

The `$(RANLIB)` command must be run on any static libraries after installation
since the symbol table is invalidated when the library is copied on some
platforms.
