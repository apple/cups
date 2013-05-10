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
#define xpdfVersion "0.92"

// supported PDF version
#define supportedPDFVersionStr "1.3"
#define supportedPDFVersionNum 1.3

// copyright notice
#define xpdfCopyright "Copyright 1996-2000 Derek B. Noonburg"

// default paper size (in points) for PostScript output
#ifdef A4_PAPER
#define defPaperWidth  595    // ISO A4 (210x297 mm)
#define defPaperHeight 842
#else
#define defPaperWidth  612    // American letter (8.5x11")
#define defPaperHeight 792
#endif

// config file name
#if defined(VMS)
#define xpdfConfigFile "xpdfrc"
#else
#define xpdfConfigFile ".xpdfrc"
#endif

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

//------------------------------------------------------------------------
// popen
//------------------------------------------------------------------------

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

#if defined(VMS) || defined(VMCMS) || defined(DOS) || defined(OS2) || defined(WIN32) || defined(__DJGPP__) || defined(__CYGWIN32) || defined(MACOS)
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
