//========================================================================
//
// SplashBitmap.cc
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include "gmem.h"
#include "SplashErrorCodes.h"
#include "SplashBitmap.h"

//------------------------------------------------------------------------
// SplashBitmap
//------------------------------------------------------------------------

SplashBitmap::SplashBitmap(int widthA, int heightA, int rowPad,
			   SplashColorMode modeA) {
  width = widthA;
  height = heightA;
  mode = modeA;
  switch (mode) {
  case splashModeMono1:
    rowSize = ((width + 7) >> 3) + rowPad - 1;
    rowSize -= rowSize % rowPad;
    data.mono1 = (SplashMono1P *)gmalloc(rowSize * height);
    break;
  case splashModeMono8:
    rowSize = width + rowPad - 1;
    rowSize -= rowSize % rowPad;
    data.mono8 = (SplashMono8 *)gmalloc(rowSize * height);
    break;
  case splashModeRGB8:
    if (rowPad < 4) {
      rowPad = 4;
    } else {
      rowPad &= ~3;
    }
    rowSize = (width << 2) + rowPad - 1;
    rowSize -= rowSize % rowPad;
    data.rgb8 = (SplashRGB8 *)gmalloc(rowSize * height);
    break;
  case splashModeBGR8Packed:
    rowSize = width * 3 + rowPad - 1;
    rowSize -= rowSize % rowPad;
    data.bgr8 = (SplashBGR8P *)gmalloc(rowSize * height);
  }
}


SplashBitmap::~SplashBitmap() {
  switch (mode) {
  case splashModeMono1:
    gfree(data.mono1);
    break;
  case splashModeMono8:
    gfree(data.mono8);
    break;
  case splashModeRGB8:
    gfree(data.rgb8);
    break;
  case splashModeBGR8Packed:
    gfree(data.bgr8);
    break;
  }
}

SplashError SplashBitmap::writePNMFile(char *fileName) {
  FILE *f;
  SplashMono1P *mono1Row, *mono1;
  SplashMono8 *mono8Row, *mono8;
  SplashRGB8 *rgb8Row, *rgb8;
  SplashBGR8P *bgr8Row, *bgr8;
  int x, y;

  if (!(f = fopen(fileName, "wb"))) {
    return splashErrOpenFile;
  }

  switch (mode) {

  case splashModeMono1:
    fprintf(f, "P4\n%d %d\n", width, height);
    mono1Row = data.mono1;
    for (y = 0; y < height; ++y) {
      mono1 = mono1Row;
      for (x = 0; x < width; x += 8) {
	fputc(*mono1 ^ 0xff, f);
	++mono1;
      }
      mono1Row += rowSize;
    }
    break;

  case splashModeMono8:
    fprintf(f, "P5\n%d %d\n255\n", width, height);
    mono8Row = data.mono8;
    for (y = 0; y < height; ++y) {
      mono8 = mono8Row;
      for (x = 0; x < width; ++x) {
	fputc(*mono8, f);
	++mono8;
      }
      mono8Row += rowSize;
    }
    break;

  case splashModeRGB8:
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    rgb8Row = data.rgb8;
    for (y = 0; y < height; ++y) {
      rgb8 = rgb8Row;
      for (x = 0; x < width; ++x) {
	fputc(splashRGB8R(*rgb8), f);
	fputc(splashRGB8G(*rgb8), f);
	fputc(splashRGB8B(*rgb8), f);
	++rgb8;
      }
      rgb8Row += rowSize >> 2;
    }
    break;

  case splashModeBGR8Packed:
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    bgr8Row = data.bgr8;
    for (y = 0; y < height; ++y) {
      bgr8 = bgr8Row;
      for (x = 0; x < width; ++x) {
	fputc(bgr8[2], f);
	fputc(bgr8[1], f);
	fputc(bgr8[0], f);
	bgr8 += 3;
      }
      bgr8Row += rowSize;
    }
    break;
  }

  fclose(f);
  return splashOk;
}
