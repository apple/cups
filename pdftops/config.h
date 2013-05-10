//========================================================================
//
// config.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include "../config.h"
#define HAVE_LIBCUPS

//------------------------------------------------------------------------
// version
//------------------------------------------------------------------------

// xpdf version

#define xpdfVersion "1.01"

// supported PDF version
#define supportedPDFVersionStr "1.4"
#define supportedPDFVersionNum 1.4

// copyright notice
#define xpdfCopyright "Copyright 1996-2002 Glyph & Cog, LLC"

//------------------------------------------------------------------------
// paper size
//------------------------------------------------------------------------

// default paper size (in points) for PostScript output
#ifdef A4_PAPER
#define defPaperWidth  595    // ISO A4 (210x297 mm)
#define defPaperHeight 842
#else
#define defPaperWidth  612    // American letter (8.5x11")
#define defPaperHeight 792
#endif

//------------------------------------------------------------------------
// config file (xpdfrc) path
//------------------------------------------------------------------------

// user config file name, relative to the user's home directory
#if defined(VMS) || (defined(WIN32) && !defined(__CYGWIN32__))
#define xpdfUserConfigFile "xpdfrc"
#else
#define xpdfUserConfigFile ".xpdfrc"
#endif

// system config file name (set via the configure script)
#ifdef SYSTEM_XPDFRC
#define xpdfSysConfigFile CUPS_SERVERROOT "/pdftops.conf"
#else
// under Windows, we get the directory with the executable and then
// append this file name
#define xpdfSysConfigFile "xpdfrc"
#endif

// Support Unicode/etc.
//
// The IBM AIX GNUPro compilers seem not to like the Asian font
// code, causing a "virtual memory exhausted" error.  Only support
// Asian fonts on platforms that will compile it...

#if !defined(_AIX) || __GNUC__ != 2 || __GNUC_MINOR__ != 9
#  define JAPANESE_SUPPORT 1
#  define CHINESE_GB_SUPPORT 1
#  define CHINESE_CNS_SUPPORT 1
#endif // !_AIX || !GCC 2.9

//------------------------------------------------------------------------
// X-related constants
//------------------------------------------------------------------------

// default maximum size of color cube to allocate
#define defaultRGBCube 5

// number of fonts (combined t1lib, FreeType, X server) to cache
#define xOutFontCacheSize 64

// number of Type 3 fonts to cache
#define xOutT3FontCacheSize 8

//------------------------------------------------------------------------
// popen
//------------------------------------------------------------------------

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

#if defined(VMS) || defined(VMCMS) || defined(DOS) || defined(OS2) || defined(__EMX__) || defined(WIN32) || defined(__DJGPP__) || defined(__CYGWIN32__) || defined(MACOS)
#define POPEN_READ_MODE "rb"
#else
#define POPEN_READ_MODE "r"
#endif

//------------------------------------------------------------------------
// uncompress program
//------------------------------------------------------------------------

// Many Linux distributions no longer ship uncompress, but all ship
// gzip...

#if defined(__linux) && !defined(USE_GZIP)
#  define USE_GZIP
#endif // __linux && USE_GZIP

#ifdef HAVE_POPEN

// command to uncompress to stdout
#  ifdef USE_GZIP
#    define uncompressCmd "gzip -d -c -q"
#  else
#    ifdef __EMX__
#      define uncompressCmd "compress -d -c"
#    else
#      define uncompressCmd "uncompress -c"
#    endif // __EMX__
#  endif // USE_GZIP

#else // HAVE_POPEN

// command to uncompress a file
#  ifdef USE_GZIP
#    define uncompressCmd "gzip -d -q"
#  else
#    define uncompressCmd "uncompress"
#  endif // USE_GZIP

#endif // HAVE_POPEN

//------------------------------------------------------------------------
// Win32 stuff
//------------------------------------------------------------------------

#ifdef CDECL
#undef CDECL
#endif

#ifdef _MSC_VER
#define CDECL __cdecl
#else
#define CDECL
#endif

#endif
