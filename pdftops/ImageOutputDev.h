//========================================================================
//
// ImageOutputDev.h
//
// Copyright 1998 Derek B. Noonburg
//
//========================================================================

#ifndef IMAGEOUTPUTDEV_H
#define IMAGEOUTPUTDEV_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "OutputDev.h"

class GfxState;

//------------------------------------------------------------------------
// ImageOutputDev
//------------------------------------------------------------------------

class ImageOutputDev: public OutputDev {
public:

  // Create an OutputDev which will write images to files named
  // <fileRoot>-NNN.<type>.  Normally, all images are written as PBM
  // (.pbm) or PPM (.ppm) files.  If <dumpJPEG> is set, JPEG images are
  // written as JPEG (.jpg) files.
  ImageOutputDev(char *fileRoot1, GBool dumpJPEG1);

  // Destructor.
  virtual ~ImageOutputDev();

  // Check if file was successfully created.
  virtual GBool isOk() { return ok; }

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() { return gTrue; }

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() { return gFalse; }

  //----- image drawing
  virtual void drawImageMask(GfxState *state, Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Stream *str, int width,
			 int height, GfxImageColorMap *colorMap,
			 GBool inlineImg);

private:

  char *fileRoot;		// root of output file names
  char *fileName;		// buffer for output file names
  GBool dumpJPEG;		// set to dump native JPEG files
  int imgNum;			// current image number
  GBool ok;			// set up ok?
};

#endif
