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
#define xpdfCopyright "Copyright 1996-1999 Derek B. Noonburg"

//------------------------------------------------------------------------
// uncompress program
//------------------------------------------------------------------------

#define HAVE_POPEN
#define uncompressCmd "uncompress -c"

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
