//========================================================================
//
// config.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef CONFIG_H
#define CONFIG_H

//------------------------------------------------------------------------
// general constants
//------------------------------------------------------------------------

// xpdf version
#define xpdfVersion "0.90"

// supported PDF version
#define pdfVersion "1.3"
#define pdfVersionNum 1.3

// copyright notice
#define xpdfCopyright "Copyright \251 1996-1999 Derek B. Noonburg"

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

#ifdef WIN32
#ifdef CDECL
#undef CDECL
#endif
#define CDECL __cdecl
#else
#define CDECL
#endif

#endif
