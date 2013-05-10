//========================================================================
//
// GfxState.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stddef.h>
#include <math.h>
#include <string.h> // for memcpy()
#include "gmem.h"
#include "Error.h"
#include "Object.h"
#include "GfxState.h"

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

void GfxColor::setGray(double gray) {
  if (gray < 0) {
    r = g = b = 0;
  } else if (gray > 1) {
    r = g = b = 1;
  } else {
    r = g = b = gray;
  }
}

void GfxColor::setCMYK(double c, double m, double y, double k) {
  if ((r = 1 - (c + k)) < 0) {
    r = 0;
  } else if (r > 1) {
    r = 1;
  }
  if ((g = 1 - (m + k)) < 0) {
    g = 0;
  } else if (g > 1) {
    g = 1;
  }
  if ((b = 1 - (y + k)) < 0) {
    b = 0;
  } else if (b > 1) {
    b = 1;
  }
}

void GfxColor::setRGB(double r1, double g1, double b1) {
  if (r1 < 0) {
    r = 0;
  } else if (r1 > 1) {
    r = 1;
  } else {
    r = r1;
  }
  if (g1 < 0) {
    g = 0;
  } else if (g1 > 1) {
    g = 1;
  } else {
    g = g1;
  }
  if (b1 < 0) {
    b = 0;
  } else if (b1 > 1) {
    b = 1;
  } else {
    b = b1;
  }
}

// Handle colors in the L*a*b* color space.
void GfxColor::setLab(double L, double a, double bb, LabParams *params) {
  double X, Y, Z;
  double t1, t2;

  // convert L*a*b* to CIE 1931 XYZ color space
  // (This ignores the white point parameter, because I don't
  // understand exactly how it should work.)
  t1 = (L + 16) / 116;
  t2 = t1 + a / 500;
  if (t2 >= (6.0 / 29.0)) {
    X = t2 * t2 * t2;
  } else {
    X = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
  }
#if 0 //~
  X *= params->whiteX;
#endif
  if (t1 >= (6.0 / 29.0)) {
    Y = t1 * t1 * t1;
  } else {
    Y = (108.0 / 841.0) * (t1 - (4.0 / 29.0));
  }
#if 0 //~
  Y *= params->whiteY;
#endif
  t2 = t1 - bb / 200;
  if (t2 >= (6.0 / 29.0)) {
    Z = t2 * t2 * t2;
  } else {
    Z = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
  }
#if 0 //~
  Z *= params->whiteZ;
#endif

  // convert XYZ to RGB
#if 0 //~
  X *= 0.9505;
  Z *= 1.0890;
#endif
  r =  3.240479 * X - 1.537150 * Y - 0.498535 * Z;
  g = -0.969256 * X + 1.875992 * Y + 0.041556 * Z;
  b =  0.055648 * X - 0.204043 * Y + 1.057311 * Z;

  // clip RGB
  if (r < 0) {
    r = 0;
  } else if (r > 1) {
    r = 1;
  }
  if (g < 0) {
    g = 0;
  } else if (g > 1) {
    g = 1;
  }
  if (b < 0) {
    b = 0;
  } else if (b > 1) {
    b = 1;
  }
}

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

GfxColorSpace::GfxColorSpace(Object *colorSpace) {
  Object csObj;
  Object obj, obj2, obj3;
  Dict *dict;
  char *s;
  int x;
  int i, j;

  ok = gTrue;
  lookup = NULL;

  // check for Separation, DeviceN, ICCBased, and Pattern colorspaces
  colorSpace->copy(&csObj);
  sepFunc = NULL;
  if (colorSpace->isArray()) {
    colorSpace->arrayGet(0, &obj);
    if (obj.isName("Separation") || obj.isName("DeviceN")) {
      csObj.free();
      colorSpace->arrayGet(2, &csObj);
      sepFunc = new Function(colorSpace->arrayGet(3, &obj2));
      obj2.free();
      if (!sepFunc->isOk()) {
	delete sepFunc;
	sepFunc = NULL;
      }
    } else if (obj.isName("ICCBased")) {
      colorSpace->arrayGet(1, &obj2);
      if (obj2.isStream()) {
	if ((dict = obj2.streamGetDict())) {
	  dict->lookup("Alternate", &obj3);
	  if (!obj3.isNull()) {
	    csObj.free();
	    csObj = obj3;
	  } else {
	    obj3.free();
	    dict->lookup("N", &obj3);
	    if (!obj3.isNull()) {
	      csObj.free();
	      if (obj3.getInt() == 4) {
		csObj.initName("DeviceCMYK");
	      } else if (obj3.getInt() == 3) {
		csObj.initName("DeviceRGB");
	      } else {
		csObj.initName("DeviceGray");
	      }
	    }
	    obj3.free();
	  }
	}
      }
      obj2.free();
    } else if (obj.isName("Pattern")) {
      csObj.free();
      colorSpace->arrayGet(1, &csObj);
    }
    obj.free();
  }

  // get mode
  indexed = gFalse;
  if (csObj.isName()) {
    setMode(&csObj);
  } else if (csObj.isArray()) {
    csObj.arrayGet(0, &obj);
    if (obj.isName("Indexed") || obj.isName("I")) {
      indexed = gTrue;
      setMode(csObj.arrayGet(1, &obj2));
      obj2.free();
    } else {
      setMode(&csObj);
    }
    obj.free();
  } else {
    goto err1;
  }
  if (!ok) {
    goto err1;
  }

  // get lookup table for indexed colorspace
  if (indexed) {
    csObj.arrayGet(2, &obj);
    if (!obj.isInt())
      goto err2;
    indexHigh = obj.getInt();
    obj.free();
    lookup = (Guchar (*)[4])gmalloc((indexHigh + 1) * 4 * sizeof(Guchar));
    csObj.arrayGet(3, &obj);
    if (obj.isStream()) {
      obj.streamReset();
      for (i = 0; i <= indexHigh; ++i) {
	for (j = 0; j < numComps; ++j) {
	  if ((x = obj.streamGetChar()) == EOF)
	    goto err2;
	  lookup[i][j] = (Guchar)x;
	}
      }
    } else if (obj.isString()) {
      s = obj.getString()->getCString();
      for (i = 0; i <= indexHigh; ++i)
	for (j = 0; j < numComps; ++j)
	  lookup[i][j] = (Guchar)*s++;
    } else {
      goto err2;
    }
    obj.free();
  }

  csObj.free();
  return;

 err2:
  obj.free();
 err1:
  csObj.free();
  ok = gFalse;
}

GfxColorSpace::GfxColorSpace(GfxColorMode mode1) {
  sepFunc = NULL;
  mode = mode1;
  indexed = gFalse;
  switch (mode) {
  case colorGray: numComps = 1; break;
  case colorCMYK: numComps = 4; break;
  case colorRGB:  numComps = 3; break;
  case colorLab:  numComps = 3; break;
  }
  lookup = NULL;
  ok = gTrue;
}

GfxColorSpace::~GfxColorSpace() {
  if (sepFunc)
    delete sepFunc;
  gfree(lookup);
}

GfxColorSpace::GfxColorSpace(GfxColorSpace *colorSpace) {
  int size;

  if (colorSpace->sepFunc)
    sepFunc = colorSpace->sepFunc->copy();
  else
    sepFunc = NULL;
  mode = colorSpace->mode;
  indexed = colorSpace->indexed;
  numComps = colorSpace->numComps;
  indexHigh = colorSpace->indexHigh;
  if (indexed) {
    size = (indexHigh + 1) * 4 * sizeof(Guchar);
    lookup = (Guchar (*)[4])gmalloc(size);
    memcpy(lookup, colorSpace->lookup, size);
  } else {
    lookup = NULL;
  }
  ok = gTrue;
}

void GfxColorSpace::setMode(Object *colorSpace) {
  Object obj1, obj2, obj3, obj4;

  if (colorSpace->isName("DeviceGray") || colorSpace->isName("G")) {
    mode = colorGray;
    numComps = 1;
  } else if (colorSpace->isName("DeviceRGB") || colorSpace->isName("RGB")) {
    mode = colorRGB;
    numComps = 3;
  } else if (colorSpace->isName("DeviceCMYK") || colorSpace->isName("CMYK")) {
    mode = colorCMYK;
    numComps = 4;
  } else if (colorSpace->isArray()) {
    colorSpace->arrayGet(0, &obj1);
    if (obj1.isName("CalGray")) {
      mode = colorGray;
      numComps = 1;
    } else if (obj1.isName("CalRGB")) {
      mode = colorRGB;
      numComps = 3;
    } else if (obj1.isName("CalCMYK")) {
      mode = colorCMYK;
      numComps = 4;
    } else if (obj1.isName("Lab")) {
      mode = colorLab;
      numComps = 3;
      labParams.whiteX = 0.9505;
      labParams.whiteY = 1;
      labParams.whiteZ = 1.0890;
      labParams.aMin = -100;
      labParams.aMax = 100;
      labParams.bMin = -100;
      labParams.bMax = 100;
      colorSpace->arrayGet(1, &obj2);
      if (obj2.isDict()) {
	obj2.dictLookup("WhitePoint", &obj3);
	if (obj3.isArray() && obj3.arrayGetLength() == 3) {
	  obj3.arrayGet(0, &obj4);
	  if (obj4.isNum()) {
	    labParams.whiteX = obj4.getNum();
	  }
	  obj4.free();
	  obj3.arrayGet(1, &obj4);
	  if (obj4.isNum()) {
	    labParams.whiteY = obj4.getNum();
	  }
	  obj4.free();
	  obj3.arrayGet(2, &obj4);
	  if (obj4.isNum()) {
	    labParams.whiteZ = obj4.getNum();
	  }
	  obj4.free();
	}
	obj3.free();
	obj2.dictLookup("Range", &obj3);
	if (obj3.isArray() && obj3.arrayGetLength() == 4) {
	  obj3.arrayGet(0, &obj4);
	  if (obj4.isNum()) {
	    labParams.aMin = obj4.getNum();
	  }
	  obj4.free();
	  obj3.arrayGet(1, &obj4);
	  if (obj4.isNum()) {
	    labParams.aMax = obj4.getNum();
	  }
	  obj4.free();
	  obj3.arrayGet(2, &obj4);
	  if (obj4.isNum()) {
	    labParams.bMin = obj4.getNum();
	  }
	  obj4.free();
	  obj3.arrayGet(3, &obj4);
	  if (obj4.isNum()) {
	    labParams.bMax = obj4.getNum();
	  }
	  obj4.free();
	}
	obj3.free();
      }
      obj2.free();
    } else {
      ok = gFalse;
    }
    obj1.free();
  } else {
    ok = gFalse;
  }
}

void GfxColorSpace::getDefaultRanges(double *decodeLow, double *decodeRange,
				     int maxPixel) {
  int i;

  if (indexed) {
    decodeLow[0] = 0;
    decodeRange[0] = maxPixel;
  } else if (mode == colorLab) {
    decodeLow[0] = 0;
    decodeRange[0] = 100;
    decodeLow[1] = labParams.aMin;
    decodeRange[1] = labParams.aMax - labParams.aMin;
    decodeLow[2] = labParams.bMin;
    decodeRange[2] = labParams.bMax - labParams.bMin;
  } else {
    for (i = 0; i < numComps; ++i) {
      decodeLow[i] = 0;
      decodeRange[i] = 1;
    }
  }
}

void GfxColorSpace::getColor(double x[4], GfxColor *color) {
  double y[4];
  Guchar *p;

  if (sepFunc) {
    sepFunc->transform(x, y);
  } else {
    y[0] = x[0];
    y[1] = x[1];
    y[2] = x[2];
    y[3] = x[3];
  }
  if (indexed) {
    p = lookup[(int)(y[0] + 0.5)];
    switch (mode) {
    case colorGray:
      color->setGray(p[0] / 255.0);
      break;
    case colorCMYK:
      color->setCMYK(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0, p[3] / 255.0);
      break;
    case colorRGB:
      color->setRGB(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0);
      break;
    case colorLab:
      color->setLab(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0, &labParams);
      break;
    }
  } else {
    switch (mode) {
    case colorGray:
      color->setGray(y[0]);
      break;
    case colorCMYK:
      color->setCMYK(y[0], y[1], y[2], y[3]);
      break;
    case colorRGB:
      color->setRGB(y[0], y[1], y[2]);
      break;
    case colorLab:
      color->setLab(y[0], y[1], y[2], &labParams);
      break;
    }
  }
}

//------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------

Function::Function(Object *funcObj) {
  Stream *str;
  Dict *dict;
  int nSamples, sampleBits;
  double sampleMul;
  Object obj1, obj2;
  Guint buf, bitMask;
  int bits;
  int s;
  int i;

  ok = gFalse;
  samples = NULL;

  if (!funcObj->isStream()) {
    error(-1, "Expected function dictionary");
    goto err3;
  }
  str = funcObj->getStream();
  dict = str->getDict();

  //----- FunctionType
  if (!dict->lookup("FunctionType", &obj1)->isInt() ||
      obj1.getInt() != 0) {
    error(-1, "Unknown function type");
    goto err2;
  }
  obj1.free();

  //----- Domain
  if (!dict->lookup("Domain", &obj1)->isArray()) {
    error(-1, "Function is missing domain");
    goto err2;
  }
  m = obj1.arrayGetLength() / 2;
  if (m > 1) {
    error(-1, "Functions with more than 1 input are unsupported");
    goto err2;
  }
  for (i = 0; i < m; ++i) {
    obj1.arrayGet(2*i, &obj2);
    if (!obj2.isNum()) {
      error(-1, "Illegal value in function domain array");
      goto err1;
    }
    domain[i][0] = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2*i+1, &obj2);
    if (!obj2.isNum()) {
      error(-1, "Illegal value in function domain array");
      goto err1;
    }
    domain[i][1] = obj2.getNum();
    obj2.free();
  }
  obj1.free();

  //----- Range
  if (!dict->lookup("Range", &obj1)->isArray()) {
    error(-1, "Function is missing range");
    goto err2;
  }
  n = obj1.arrayGetLength() / 2;
  if (n > 4) {
    error(-1, "Functions with more than 4 outputs are unsupported");
    goto err2;
  }
  for (i = 0; i < n; ++i) {
    obj1.arrayGet(2*i, &obj2);
    if (!obj2.isNum()) {
      error(-1, "Illegal value in function range array");
      goto err1;
    }
    range[i][0] = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2*i+1, &obj2);
    if (!obj2.isNum()) {
      error(-1, "Illegal value in function range array");
      goto err1;
    }
    range[i][1] = obj2.getNum();
    obj2.free();
  }
  obj1.free();

  //----- Size
  if (!dict->lookup("Size", &obj1)->isArray() ||
      obj1.arrayGetLength() != m) {
    error(-1, "Function has missing or invalid size array");
    goto err2;
  }
  for (i = 0; i < m; ++i) {
    obj1.arrayGet(i, &obj2);
    if (!obj2.isInt()) {
      error(-1, "Illegal value in function size array");
      goto err1;
    }
    sampleSize[i] = obj2.getInt();
    obj2.free();
  }
  obj1.free();

  //----- BitsPerSample
  if (!dict->lookup("BitsPerSample", &obj1)->isInt()) {
    error(-1, "Function has missing or invalid BitsPerSample");
    goto err2;
  }
  sampleBits = obj1.getInt();
  sampleMul = 1.0 / (double)((1 << sampleBits) - 1);
  obj1.free();

  //----- Encode
  if (dict->lookup("Encode", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2*m) {
    for (i = 0; i < m; ++i) {
      obj1.arrayGet(2*i, &obj2);
      if (!obj2.isNum()) {
	error(-1, "Illegal value in function encode array");
	goto err1;
      }
      encode[i][0] = obj2.getNum();
      obj2.free();
      obj1.arrayGet(2*i+1, &obj2);
      if (!obj2.isNum()) {
	error(-1, "Illegal value in function encode array");
	goto err1;
      }
      encode[i][1] = obj2.getNum();
      obj2.free();
    }
  } else {
    for (i = 0; i < m; ++i) {
      encode[i][0] = 0;
      encode[i][1] = sampleSize[i] - 1;
    }
  }
  obj1.free();

  //----- Decode
  if (dict->lookup("Decode", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2*n) {
    for (i = 0; i < n; ++i) {
      obj1.arrayGet(2*i, &obj2);
      if (!obj2.isNum()) {
	error(-1, "Illegal value in function decode array");
	goto err1;
      }
      decode[i][0] = obj2.getNum();
      obj2.free();
      obj1.arrayGet(2*i+1, &obj2);
      if (!obj2.isNum()) {
	error(-1, "Illegal value in function decode array");
	goto err1;
      }
      decode[i][1] = obj2.getNum();
      obj2.free();
    }
  } else {
    for (i = 0; i < n; ++i) {
      decode[i][0] = range[i][0];
      decode[i][1] = range[i][1];
    }
  }
  obj1.free();

  //----- samples
  nSamples = n;
  for (i = 0; i < m; ++i)
    nSamples *= sampleSize[i];
  samples = (double *)gmalloc(nSamples * sizeof(double));
  buf = 0;
  bits = 0;
  bitMask = (1 << sampleBits) - 1;
  str->reset();
  for (i = 0; i < nSamples; ++i) {
    if (sampleBits == 8) {
      s = str->getChar();
    } else if (sampleBits == 16) {
      s = str->getChar();
      s = (s << 8) + str->getChar();
    } else if (sampleBits == 32) {
      s = str->getChar();
      s = (s << 8) + str->getChar();
      s = (s << 8) + str->getChar();
      s = (s << 8) + str->getChar();
    } else {
      while (bits < sampleBits) {
	buf = (buf << 8) | (str->getChar() & 0xff);
	bits += 8;
      }
      s = (buf >> (bits - sampleBits)) & bitMask;
      bits -= sampleBits;
    }
    samples[i] = (double)s * sampleMul;
  }

  ok = gTrue;
  return;

 err1:
  obj2.free();
 err2:
  obj1.free();
 err3:
  return;
}

Function::Function(Function *func) {
  int nSamples, i;

  m = func->m;
  n = func->n;
  memcpy(domain, func->domain, sizeof(domain));
  memcpy(range, func->range, sizeof(range));
  memcpy(sampleSize, func->sampleSize, sizeof(sampleSize));
  memcpy(encode, func->encode, sizeof(encode));
  memcpy(decode, func->decode, sizeof(decode));

  nSamples = n;
  for (i = 0; i < m; ++i)
    nSamples *= sampleSize[i];
  samples = (double *)gmalloc(nSamples * sizeof(double));
  memcpy(samples, func->samples, nSamples * sizeof(double));

  ok = gTrue;
}

Function::~Function() {
  if (samples)
    gfree(samples);
}

void Function::transform(double *in, double *out) {
  double e[4];
  double s;
  double x0, x1;
  int e0, e1;
  double efrac;
  int i;

  // map input values into sample array
  for (i = 0; i < m; ++i) {
    e[i] = ((in[i] - domain[i][0]) / (domain[i][1] - domain[i][0])) *
           (encode[i][1] - encode[i][0]) + encode[i][0];
    if (e[i] < 0)
      e[i] = 0;
    else if (e[i] > sampleSize[i] - 1)
      e[i] = sampleSize[i] - 1;
  }

  for (i = 0; i < n; ++i) {

    // m-linear interpolation
    // (only m=1 is currently supported)
    e0 = (int)floor(e[0]);
    e1 = (int)ceil(e[0]);
    efrac = e[0] - e0;
    x0 = samples[e0 * n + i];
    x1 = samples[e1 * n + i];
    s = (1 - efrac) * x0 + efrac * x1;

    // map output values to range
    out[i] = s * (decode[i][1] - decode[i][0]) + decode[i][0];
    if (out[i] < range[i][0])
      out[i] = range[i][0];
    else if (out[i] > range[i][1])
      out[i] = range[i][1];
  }
}

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

GfxImageColorMap::GfxImageColorMap(int bits1, Object *decode,
				   GfxColorSpace *colorSpace1) {
  GfxColor color;
  double x[4];
  int maxPixel;
  Object obj;
  int i, j;

  ok = gTrue;

  // bits per component and colorspace
  bits = bits1;
  maxPixel = (1 << bits) - 1;
  colorSpace = colorSpace1;
  mode = colorSpace->getMode();

  // get decode map
  indexed = colorSpace->isIndexed();
  sep = colorSpace->isSeparation();
  if (decode->isNull()) {
    numComps = colorSpace->getNumPixelComps();
    colorSpace->getDefaultRanges(decodeLow, decodeRange, maxPixel);
  } else if (decode->isArray()) {
    numComps = decode->arrayGetLength() / 2;
    if (numComps != colorSpace->getNumPixelComps())
      goto err1;
    indexed = colorSpace->isIndexed();
    for (i = 0; i < numComps; ++i) {
      decode->arrayGet(2*i, &obj);
      if (!obj.isNum())
	goto err2;
      decodeLow[i] = obj.getNum();
      obj.free();
      decode->arrayGet(2*i+1, &obj);
      if (!obj.isNum())
	goto err2;
      decodeRange[i] = obj.getNum() - decodeLow[i];
      obj.free();
    }
  } else {
    goto err1;
  }

  // handle the case where fewer than 2^n palette entries of an n-bit
  // indexed color space are populated (this happens, e.g., in files
  // optimized by Distiller)
  if (colorSpace->isIndexed() && maxPixel > colorSpace->getIndexHigh()) {
    maxPixel = colorSpace->getIndexHigh();
  }

  // handle the case where fewer than 2^n palette entries of an n-bit
  // indexed color space are populated (this happens, e.g., in files
  // optimized by Distiller)
  if (indexed && maxPixel > colorSpace->getIndexHigh()) {
    maxPixel = colorSpace->getIndexHigh();
  }

  // construct lookup table
  lookup = (double (*)[4])gmalloc((maxPixel + 1) * 4 * sizeof(double));
  if (sep) {
    for (i = 0; i <= maxPixel; ++i) {
      x[0] = (double)i / (double)maxPixel;
      colorSpace->getColor(x, &color);
      lookup[i][0] = color.getR();
      lookup[i][1] = color.getG();
      lookup[i][2] = color.getB();
    }
  } else if (indexed) {
    for (i = 0; i <= maxPixel; ++i) {
      x[0] = (double)i;
      colorSpace->getColor(x, &color);
      lookup[i][0] = color.getR();
      lookup[i][1] = color.getG();
      lookup[i][2] = color.getB();
    }
  } else {
    for (i = 0; i <= maxPixel; ++i)
      for (j = 0; j < numComps; ++j)
	lookup[i][j] = decodeLow[j] + (i * decodeRange[j]) / maxPixel;
  }

  return;

 err2:
  obj.free();
 err1:
  ok = gFalse;
}

GfxImageColorMap::~GfxImageColorMap() {
  delete colorSpace;
  gfree(lookup);
}

void GfxImageColorMap::getColor(Guchar x[4], GfxColor *color) {
  double *p;

  if (sep || indexed) {
    p = lookup[x[0]];
    color->setRGB(p[0], p[1], p[2]);
  } else {
    switch (mode) {
    case colorGray:
      color->setGray(lookup[x[0]][0]);
      break;
    case colorCMYK:
      color->setCMYK(lookup[x[0]][0], lookup[x[1]][1],
		     lookup[x[2]][2], lookup[x[3]][3]);
      break;
    case colorRGB:
      color->setRGB(lookup[x[0]][0], lookup[x[1]][1], lookup[x[2]][2]);
      break;
    case colorLab:
      color->setLab(lookup[x[0]][0], lookup[x[1]][1], lookup[x[2]][2],
		    colorSpace->getLabParams());
      break;
    }
  }
}

//------------------------------------------------------------------------
// GfxSubpath and GfxPath
//------------------------------------------------------------------------

GfxSubpath::GfxSubpath(double x1, double y1) {
  size = 16;
  x = (double *)gmalloc(size * sizeof(double));
  y = (double *)gmalloc(size * sizeof(double));
  curve = (GBool *)gmalloc(size * sizeof(GBool));
  n = 1;
  x[0] = x1;
  y[0] = y1;
  curve[0] = gFalse;
  closed = gFalse;
}

GfxSubpath::~GfxSubpath() {
  gfree(x);
  gfree(y);
  gfree(curve);
}

// Used for copy().
GfxSubpath::GfxSubpath(GfxSubpath *subpath) {
  size = subpath->size;
  n = subpath->n;
  x = (double *)gmalloc(size * sizeof(double));
  y = (double *)gmalloc(size * sizeof(double));
  curve = (GBool *)gmalloc(size * sizeof(GBool));
  memcpy(x, subpath->x, n * sizeof(double));
  memcpy(y, subpath->y, n * sizeof(double));
  memcpy(curve, subpath->curve, n * sizeof(GBool));
  closed = subpath->closed;
}

void GfxSubpath::lineTo(double x1, double y1) {
  if (n >= size) {
    size += 16;
    x = (double *)grealloc(x, size * sizeof(double));
    y = (double *)grealloc(y, size * sizeof(double));
    curve = (GBool *)grealloc(curve, size * sizeof(GBool));
  }
  x[n] = x1;
  y[n] = y1;
  curve[n] = gFalse;
  ++n;
}

void GfxSubpath::curveTo(double x1, double y1, double x2, double y2,
			 double x3, double y3) {
  if (n+3 > size) {
    size += 16;
    x = (double *)grealloc(x, size * sizeof(double));
    y = (double *)grealloc(y, size * sizeof(double));
    curve = (GBool *)grealloc(curve, size * sizeof(GBool));
  }
  x[n] = x1;
  y[n] = y1;
  x[n+1] = x2;
  y[n+1] = y2;
  x[n+2] = x3;
  y[n+2] = y3;
  curve[n] = curve[n+1] = gTrue;
  curve[n+2] = gFalse;
  n += 3;
}

void GfxSubpath::close() {
  if (x[n-1] != x[0] || y[n-1] != y[0]) {
    lineTo(x[0], y[0]);
  }
  closed = gTrue;
}

GfxPath::GfxPath() {
  justMoved = gFalse;
  size = 16;
  n = 0;
  firstX = firstY = 0;
  subpaths = (GfxSubpath **)gmalloc(size * sizeof(GfxSubpath *));
}

GfxPath::~GfxPath() {
  int i;

  for (i = 0; i < n; ++i)
    delete subpaths[i];
  gfree(subpaths);
}

// Used for copy().
GfxPath::GfxPath(GBool justMoved1, double firstX1, double firstY1,
		 GfxSubpath **subpaths1, int n1, int size1) {
  int i;

  justMoved = justMoved1;
  firstX = firstX1;
  firstY = firstY1;
  size = size1;
  n = n1;
  subpaths = (GfxSubpath **)gmalloc(size * sizeof(GfxSubpath *));
  for (i = 0; i < n; ++i)
    subpaths[i] = subpaths1[i]->copy();
}

void GfxPath::moveTo(double x, double y) {
  justMoved = gTrue;
  firstX = x;
  firstY = y;
}

void GfxPath::lineTo(double x, double y) {
  if (justMoved) {
    if (n >= size) {
      size += 16;
      subpaths = (GfxSubpath **)
	           grealloc(subpaths, size * sizeof(GfxSubpath *));
    }
    subpaths[n] = new GfxSubpath(firstX, firstY);
    ++n;
    justMoved = gFalse;
  }
  subpaths[n-1]->lineTo(x, y);
}

void GfxPath::curveTo(double x1, double y1, double x2, double y2,
	     double x3, double y3) {
  if (justMoved) {
    if (n >= size) {
      size += 16;
      subpaths = (GfxSubpath **)
	           grealloc(subpaths, size * sizeof(GfxSubpath *));
    }
    subpaths[n] = new GfxSubpath(firstX, firstY);
    ++n;
    justMoved = gFalse;
  }
  subpaths[n-1]->curveTo(x1, y1, x2, y2, x3, y3);
}


//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

GfxState::GfxState(int dpi, double px1a, double py1a, double px2a, double py2a,
		   int rotate, GBool upsideDown) {
  double k;

  px1 = px1a;
  py1 = py1a;
  px2 = px2a;
  py2 = py2a;
  k = (double)dpi / 72.0;
  if (rotate == 90) {
    ctm[0] = 0;
    ctm[1] = upsideDown ? k : -k;
    ctm[2] = k;
    ctm[3] = 0;
    ctm[4] = -k * py1;
    ctm[5] = k * (upsideDown ? -px1 : px2);
    pageWidth = k * (py2 - py1);
    pageHeight = k * (px2 - px1);
  } else if (rotate == 180) {
    ctm[0] = -k;
    ctm[1] = 0;
    ctm[2] = 0;
    ctm[3] = upsideDown ? k : -k;
    ctm[4] = k * px2;
    ctm[5] = k * (upsideDown ? -py1 : py2);
    pageWidth = k * (px2 - px1);
    pageHeight = k * (py2 - py1);
  } else if (rotate == 270) {
    ctm[0] = 0;
    ctm[1] = upsideDown ? -k : k;
    ctm[2] = -k;
    ctm[3] = 0;
    ctm[4] = k * py2;
    ctm[5] = k * (upsideDown ? px2 : -px1);
    pageWidth = k * (py2 - py1);
    pageHeight = k * (px2 - px1);
  } else {
    ctm[0] = k;
    ctm[1] = 0;
    ctm[2] = 0;
    ctm[3] = upsideDown ? -k : k;
    ctm[4] = -k * px1;
    ctm[5] = k * (upsideDown ? py2 : -py1);
    pageWidth = k * (px2 - px1);
    pageHeight = k * (py2 - py1);
  }

  fillColorSpace = new GfxColorSpace(colorGray);
  strokeColorSpace = new GfxColorSpace(colorGray);
  fillColor.setGray(0);
  strokeColor.setGray(0);

  lineWidth = 1;
  lineDash = NULL;
  lineDashLength = 0;
  lineDashStart = 0;
  flatness = 0;
  lineJoin = 0;
  lineCap = 0;
  miterLimit = 10;

  font = NULL;
  fontSize = 0;
  textMat[0] = 1; textMat[1] = 0;
  textMat[2] = 0; textMat[3] = 1;
  textMat[4] = 0; textMat[5] = 0;
  charSpace = 0;
  wordSpace = 0;
  horizScaling = 1;
  leading = 0;
  rise = 0;
  render = 0;

  path = new GfxPath();
  curX = curY = 0;
  lineX = lineY = 0;

  saved = NULL;
}

GfxState::~GfxState() {
  if (fillColorSpace)
    delete fillColorSpace;
  if (strokeColorSpace)
    delete strokeColorSpace;
  gfree(lineDash);
  delete path;
  if (saved)
    delete saved;
}

// Used for copy();
GfxState::GfxState(GfxState *state) {
  memcpy(this, state, sizeof(GfxState));
  if (fillColorSpace)
    fillColorSpace = state->fillColorSpace->copy();
  if (strokeColorSpace)
    strokeColorSpace = state->strokeColorSpace->copy();
  if (lineDashLength > 0) {
    lineDash = (double *)gmalloc(lineDashLength * sizeof(double));
    memcpy(lineDash, state->lineDash, lineDashLength * sizeof(double));
  }
  path = state->path->copy();
  saved = NULL;
}

double GfxState::transformWidth(double w) {
  double x, y;

  x = ctm[0] + ctm[2];
  y = ctm[1] + ctm[3];
  return w * sqrt(0.5 * (x * x + y * y));
}

double GfxState::getTransformedFontSize() {
  double x1, y1, x2, y2;

  x1 = textMat[2] * fontSize;
  y1 = textMat[3] * fontSize;
  x2 = ctm[0] * x1 + ctm[2] * y1;
  y2 = ctm[1] * x1 + ctm[3] * y1;
  return sqrt(x2 * x2 + y2 * y2);
}

void GfxState::getFontTransMat(double *m11, double *m12,
			       double *m21, double *m22) {
  *m11 = (textMat[0] * ctm[0] + textMat[1] * ctm[2]) * fontSize;
  *m12 = (textMat[0] * ctm[1] + textMat[1] * ctm[3]) * fontSize;
  *m21 = (textMat[2] * ctm[0] + textMat[3] * ctm[2]) * fontSize;
  *m22 = (textMat[2] * ctm[1] + textMat[3] * ctm[3]) * fontSize;
}

void GfxState::setCTM(double a, double b, double c,
		      double d, double e, double f) {
  ctm[0] = a;
  ctm[1] = b;
  ctm[2] = c;
  ctm[3] = d;
  ctm[4] = e;
  ctm[5] = f;
}

void GfxState::concatCTM(double a, double b, double c,
			 double d, double e, double f) {
  double a1 = ctm[0];
  double b1 = ctm[1];
  double c1 = ctm[2];
  double d1 = ctm[3];

  ctm[0] = a * a1 + b * c1;
  ctm[1] = a * b1 + b * d1;
  ctm[2] = c * a1 + d * c1;
  ctm[3] = c * b1 + d * d1;
  ctm[4] = e * a1 + f * c1 + ctm[4];
  ctm[5] = e * b1 + f * d1 + ctm[5];
}

void GfxState::setFillColorSpace(GfxColorSpace *colorSpace) {
  if (fillColorSpace)
    delete fillColorSpace;
  fillColorSpace = colorSpace;
}

void GfxState::setStrokeColorSpace(GfxColorSpace *colorSpace) {
  if (strokeColorSpace)
    delete strokeColorSpace;
  strokeColorSpace = colorSpace;
}

void GfxState::setLineDash(double *dash, int length, double start) {
  if (lineDash)
    gfree(lineDash);
  lineDash = dash;
  lineDashLength = length;
  lineDashStart = start;
}

void GfxState::clearPath() {
  delete path;
  path = new GfxPath();
}

void GfxState::textShift(double tx) {
  double dx, dy;

  textTransformDelta(tx, 0, &dx, &dy);
  curX += dx;
  curY += dy;
}

void GfxState::textShift(double tx, double ty) {
  double dx, dy;

  textTransformDelta(tx, ty, &dx, &dy);
  curX += dx;
  curY += dy;
}

GfxState *GfxState::save() {
  GfxState *newState;

  newState = copy();
  newState->saved = this;
  return newState;
}

GfxState *GfxState::restore() {
  GfxState *oldState;

  if (saved) {
    oldState = saved;
    saved = NULL;
    delete this;
  } else {
    oldState = this;
  }
  return oldState;
}
