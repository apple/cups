//========================================================================
//
// SplashTypes.h
//
//========================================================================

#ifndef SPLASHTYPES_H
#define SPLASHTYPES_H

#include <config.h>
#include "gtypes.h"

//------------------------------------------------------------------------
// coordinates
//------------------------------------------------------------------------

typedef double SplashCoord;

//------------------------------------------------------------------------
// colors
//------------------------------------------------------------------------

enum SplashColorMode {
  splashModeMono1,
  splashModeMono8,
  splashModeRGB8,
  splashModeBGR8Packed
};

// max number of components in any SplashColor
#define splashMaxColorComps 3

// 1-bit gray or alpha
typedef Guchar SplashMono1;
typedef Guchar SplashMono1P; // packed

// 8-bit gray or alpha
typedef Guchar SplashMono8;

// 3x8-bit RGB: (MSB) 00RRGGBB (LSB)
typedef Guint SplashRGB8;
#define splashRGB8R(rgb8) (((rgb8) >> 16) & 0xff)
#define splashRGB8G(rgb8) (((rgb8) >> 8) & 0xff)
#define splashRGB8B(rgb8) ((rgb8) & 0xff)
#define splashMakeRGB8(r, g, b) \
  ((((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))

// 3x8-bit RGB: (MSB) 00BBGGRR (LSB)
typedef Guint SplashBGR8;
typedef Guchar SplashBGR8P; // packed
#define splashBGR8R(bgr8) ((bgr8) & 0xff)
#define splashBGR8G(bgr8) (((bgr8) >> 8) & 0xff)
#define splashBGR8B(bgr8) (((bgr8) >> 16) & 0xff)
#define splashMakeBGR8(r, g, b) \
  ((((b) & 0xff) << 16) | (((g) & 0xff) << 8) | ((r) & 0xff))

union SplashColor {
  SplashMono1 mono1;
  SplashMono8 mono8;
  SplashRGB8 rgb8;
  SplashBGR8 bgr8;
};

union SplashColorPtr {
  SplashMono1P *mono1;
  SplashMono8 *mono8;
  SplashRGB8 *rgb8;
  SplashBGR8P *bgr8;
};

//------------------------------------------------------------------------
// error results
//------------------------------------------------------------------------

typedef int SplashError;

#endif
