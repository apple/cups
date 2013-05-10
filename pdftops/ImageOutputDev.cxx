//========================================================================
//
// ImageOutputDev.cc
//
// Copyright 1998 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include "gmem.h"
#include "config.h"
#include "Error.h"
#include "GfxState.h"
#include "Object.h"
#include "Stream.h"
#include "ImageOutputDev.h"

ImageOutputDev::ImageOutputDev(char *fileRoot1, GBool dumpJPEG1) {
  fileRoot = copyString(fileRoot1);
  fileName = (char *)gmalloc(strlen(fileRoot) + 20);
  dumpJPEG = dumpJPEG1;
  imgNum = 0;
  ok = gTrue;
}

ImageOutputDev::~ImageOutputDev() {
  gfree(fileName);
  gfree(fileRoot);
}

void ImageOutputDev::drawImageMask(GfxState *state, Stream *str,
				   int width, int height, GBool invert,
				   GBool inlineImg) {
  FILE *f;
  int c;

  // dump JPEG file
  if (dumpJPEG && str->getKind() == strDCT) {

    // open the image file
    sprintf(fileName, "%s-%03d.jpg", fileRoot, imgNum);
    ++imgNum;
    if (!(f = fopen(fileName, "wb"))) {
      error(-1, "Couldn't open image file '%s'", fileName);
      return;
    }

    // initialize stream
    str = ((DCTStream *)str)->getRawStream();
    str->reset();

    // copy the stream
    while ((c = str->getChar()) != EOF)
      fputc(c, f);

    fclose(f);

  // dump PBM file
  } else {

    // open the image file and write the PBM header
    sprintf(fileName, "%s-%03d.pbm", fileRoot, imgNum);
    ++imgNum;
    if (!(f = fopen(fileName, "wb"))) {
      error(-1, "Couldn't open image file '%s'", fileName);
      return;
    }
    fprintf(f, "P4\n");
    fprintf(f, "%d %d\n", width, height);

    // initialize stream
    str->reset();

    // copy the stream
    while ((c = str->getChar()) != EOF)
      fputc(c, f);

    fclose(f);
  }
}

void ImageOutputDev::drawImage(GfxState *state, Stream *str, int width,
			       int height, GfxImageColorMap *colorMap,
			       GBool inlineImg) {
  FILE *f;
  ImageStream *imgStr;
  Guchar pixBuf[4];
  GfxColor color;
  int x, y;
  int c;

  // dump JPEG file
  if (dumpJPEG && str->getKind() == strDCT) {

    // open the image file
    sprintf(fileName, "%s-%03d.jpg", fileRoot, imgNum);
    ++imgNum;
    if (!(f = fopen(fileName, "wb"))) {
      error(-1, "Couldn't open image file '%s'", fileName);
      return;
    }

    // initialize stream
    str = ((DCTStream *)str)->getRawStream();
    str->reset();

    // copy the stream
    while ((c = str->getChar()) != EOF)
      fputc(c, f);

    fclose(f);

  // dump PPM file
  } else {

    // open the image file and write the PPM header
    sprintf(fileName, "%s-%03d.ppm", fileRoot, imgNum);
    ++imgNum;
    if (!(f = fopen(fileName, "wb"))) {
      error(-1, "Couldn't open image file '%s'", fileName);
      return;
    }
    fprintf(f, "P6\n");
    fprintf(f, "%d %d\n", width, height);
    fprintf(f, "255\n");

    // initialize stream
    imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
			     colorMap->getBits());
    imgStr->reset();

    // for each line...
    for (y = 0; y < height; ++y) {

      // write the line
      for (x = 0; x < width; ++x) {
	imgStr->getPixel(pixBuf);
	colorMap->getColor(pixBuf, &color);
	fputc((int)(color.getR() * 255 + 0.5), f);
	fputc((int)(color.getG() * 255 + 0.5), f);
	fputc((int)(color.getB() * 255 + 0.5), f);
      }
    }
    delete imgStr;

    fclose(f);
  }
}
