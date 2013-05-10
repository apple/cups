//========================================================================
//
// config.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <config.h>
#define HAVE_LIBCUPS

//------------------------------------------------------------------------
// general constants
//------------------------------------------------------------------------

// xpdf version
#define xpdfVersion "0.93a"

// supported PDF version
#define supportedPDFVersionStr "1.4"
#define supportedPDFVersionNum 1.4

// copyright notice
#define xpdfCopyright "Copyright 1996-2001 Derek B. Noonburg"

// default paper size (in points) for PostScript output
#ifdef A4_PAPER
#define defPaperWidth  595    // ISO A4 (210x297 mm)
#define defPaperHeight 842
#else
#define defPaperWidth  612    // American letter (8.5x11")
#define defPaperHeight 792
#endif

// user config file name, relative to the user's home directory
#if defined(VMS)
#define xpdfUserConfigFile "xpdfrc"
#else
#define xpdfUserConfigFile ".xpdfrc"
#endif

#ifndef SYSTEM_XPDFRC
#  define SYSTEM_XPDFRC	CUPS_SERVERROOT "/pdftops.conf"
#endif // SYSTEM_XPDFRC

// system config file name (set via the configure script)
#define xpdfSysConfigFile SYSTEM_XPDFRC

// Support Unicode/etc.
#define JAPANESE_SUPPORT 1
#define CHINESE_GB_SUPPORT 1
#define CHINESE_CNS_SUPPORT 1

//------------------------------------------------------------------------
// X-related constants
//------------------------------------------------------------------------

// default maximum size of color cube to allocate
#define defaultRGBCube 5

// number of X server fonts to cache
#define serverFontCacheSize 16

// number of Type 1 (t1lib) fonts to cache
#define t1FontCacheSize 32

// number of TrueType (FreeType) fonts to cache
#define ttFontCacheSize 32

// number of FreeType (TrueType and Type 1) fonts to cache
#define ftFontCacheSize 32

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
