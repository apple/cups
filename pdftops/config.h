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
#define xpdfVersion "0.91"

// supported PDF version
#define supportedPDFVersionStr "1.3"
#define supportedPDFVersionNum 1.3
  
// copyright notice
#define xpdfCopyright "Copyright 1996-2001 Derek B. Noonburg"

//------------------------------------------------------------------------
// uncompress program
//------------------------------------------------------------------------

#define HAVE_POPEN
#define uncompressCmd "uncompress -c"

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
