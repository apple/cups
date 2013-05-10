//========================================================================
//
// GfxState.cc
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stddef.h>
#include <math.h>
#include <string.h> // for memcpy()
#include "gmem.h"
#include "Error.h"
#include "Object.h"
#include "Array.h"
#include "Page.h"
#include "GfxState.h"

//------------------------------------------------------------------------

static inline double clip01(double x) {
  return (x < 0) ? 0 : ((x > 1) ? 1 : x);
}

//------------------------------------------------------------------------

static char *gfxColorSpaceModeNames[] = {
  "DeviceGray",
  "CalGray",
  "DeviceRGB",
  "CalRGB",
  "DeviceCMYK",
  "Lab",
  "ICCBased",
  "Indexed",
  "Separation",
  "DeviceN",
  "Pattern"
};

#define nGfxColorSpaceModes ((sizeof(gfxColorSpaceModeNames) / sizeof(char *)))

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

GfxColorSpace::GfxColorSpace() {
}

GfxColorSpace::~GfxColorSpace() {
}

GfxColorSpace *GfxColorSpace::parse(Object *csObj) {
  GfxColorSpace *cs;
  Object obj1;

  cs = NULL;
  if (csObj->isName()) {
    if (csObj->isName("DeviceGray") || csObj->isName("G")) {
      cs = new GfxDeviceGrayColorSpace();
    } else if (csObj->isName("DeviceRGB") || csObj->isName("RGB")) {
      cs = new GfxDeviceRGBColorSpace();
    } else if (csObj->isName("DeviceCMYK") || csObj->isName("CMYK")) {
      cs = new GfxDeviceCMYKColorSpace();
    } else if (csObj->isName("Pattern")) {
      cs = new GfxPatternColorSpace(NULL);
    } else {
      error(-1, "Bad color space '%s'", csObj->getName());
    }
  } else if (csObj->isArray()) {
    csObj->arrayGet(0, &obj1);
    if (obj1.isName("DeviceGray") || obj1.isName("G")) {
      cs = new GfxDeviceGrayColorSpace();
    } else if (obj1.isName("DeviceRGB") || obj1.isName("RGB")) {
      cs = new GfxDeviceRGBColorSpace();
    } else if (obj1.isName("DeviceCMYK") || obj1.isName("CMYK")) {
      cs = new GfxDeviceCMYKColorSpace();
    } else if (obj1.isName("CalGray")) {
      cs = GfxCalGrayColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("CalRGB")) {
      cs = GfxCalRGBColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("Lab")) {
      cs = GfxLabColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("ICCBased")) {
      cs = GfxICCBasedColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("Indexed") || obj1.isName("I")) {
      cs = GfxIndexedColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("Separation")) {
      cs = GfxSeparationColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("DeviceN")) {
      cs = GfxDeviceNColorSpace::parse(csObj->getArray());
    } else if (obj1.isName("Pattern")) {
      cs = GfxPatternColorSpace::parse(csObj->getArray());
    } else {
      error(-1, "Bad color space '%s'", csObj->getName());
    }
    obj1.free();
  } else {
    error(-1, "Bad color space - expected name or array");
  }
  return cs;
}

void GfxColorSpace::getDefaultRanges(double *decodeLow, double *decodeRange,
				     int maxImgPixel) {
  int i;

  for (i = 0; i < getNComps(); ++i) {
    decodeLow[i] = 0;
    decodeRange[i] = 1;
  }
}

int GfxColorSpace::getNumColorSpaceModes() {
  return nGfxColorSpaceModes;
}

char *GfxColorSpace::getColorSpaceModeName(int idx) {
  return gfxColorSpaceModeNames[idx];
}

//------------------------------------------------------------------------
// GfxDeviceGrayColorSpace
//------------------------------------------------------------------------

GfxDeviceGrayColorSpace::GfxDeviceGrayColorSpace() {
}

GfxDeviceGrayColorSpace::~GfxDeviceGrayColorSpace() {
}

GfxColorSpace *GfxDeviceGrayColorSpace::copy() {
  return new GfxDeviceGrayColorSpace();
}

void GfxDeviceGrayColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = clip01(color->c[0]);
}

void GfxDeviceGrayColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  rgb->r = rgb->g = rgb->b = clip01(color->c[0]);
}

void GfxDeviceGrayColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  cmyk->c = cmyk->m = cmyk->y = 0;
  cmyk->k = clip01(1 - color->c[0]);
}

//------------------------------------------------------------------------
// GfxCalGrayColorSpace
//------------------------------------------------------------------------

GfxCalGrayColorSpace::GfxCalGrayColorSpace() {
  whiteX = whiteY = whiteZ = 1;
  blackX = blackY = blackZ = 0;
  gamma = 1;
}

GfxCalGrayColorSpace::~GfxCalGrayColorSpace() {
}

GfxColorSpace *GfxCalGrayColorSpace::copy() {
  GfxCalGrayColorSpace *cs;

  cs = new GfxCalGrayColorSpace();
  cs->whiteX = whiteX;
  cs->whiteY = whiteY;
  cs->whiteZ = whiteZ;
  cs->blackX = blackX;
  cs->blackY = blackY;
  cs->blackZ = blackZ;
  cs->gamma = gamma;
  return cs;
}

GfxColorSpace *GfxCalGrayColorSpace::parse(Array *arr) {
  GfxCalGrayColorSpace *cs;
  Object obj1, obj2, obj3;

  arr->get(1, &obj1);
  if (!obj1.isDict()) {
    error(-1, "Bad CalGray color space");
    obj1.free();
    return NULL;
  }
  cs = new GfxCalGrayColorSpace();
  if (obj1.dictLookup("WhitePoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->whiteX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->whiteY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->whiteZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("BlackPoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->blackX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->blackY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->blackZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("Gamma", &obj2)->isNum()) {
    cs->gamma = obj2.getNum();
  }
  obj2.free();
  obj1.free();
  return cs;
}

void GfxCalGrayColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = clip01(color->c[0]);
}

void GfxCalGrayColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  rgb->r = rgb->g = rgb->b = clip01(color->c[0]);
}

void GfxCalGrayColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  cmyk->c = cmyk->m = cmyk->y = 0;
  cmyk->k = clip01(1 - color->c[0]);
}

//------------------------------------------------------------------------
// GfxDeviceRGBColorSpace
//------------------------------------------------------------------------

GfxDeviceRGBColorSpace::GfxDeviceRGBColorSpace() {
}

GfxDeviceRGBColorSpace::~GfxDeviceRGBColorSpace() {
}

GfxColorSpace *GfxDeviceRGBColorSpace::copy() {
  return new GfxDeviceRGBColorSpace();
}

void GfxDeviceRGBColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = clip01(0.299 * color->c[0] +
		 0.587 * color->c[1] +
		 0.114 * color->c[2]);
}

void GfxDeviceRGBColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  rgb->r = clip01(color->c[0]);
  rgb->g = clip01(color->c[1]);
  rgb->b = clip01(color->c[2]);
}

void GfxDeviceRGBColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  double c, m, y, k;

  c = clip01(1 - color->c[0]);
  m = clip01(1 - color->c[1]);
  y = clip01(1 - color->c[2]);
  k = c;
  if (m < k) {
    k = m;
  }
  if (y < k) {
    k = y;
  }
  cmyk->c = c - k;
  cmyk->m = m - k;
  cmyk->y = y - k;
  cmyk->k = k;
}

//------------------------------------------------------------------------
// GfxCalRGBColorSpace
//------------------------------------------------------------------------

GfxCalRGBColorSpace::GfxCalRGBColorSpace() {
  whiteX = whiteY = whiteZ = 1;
  blackX = blackY = blackZ = 0;
  gammaR = gammaG = gammaB = 1;
  mat[0] = 1; mat[1] = 0; mat[2] = 0;
  mat[3] = 0; mat[4] = 1; mat[5] = 0;
  mat[6] = 0; mat[7] = 0; mat[8] = 1;
}

GfxCalRGBColorSpace::~GfxCalRGBColorSpace() {
}

GfxColorSpace *GfxCalRGBColorSpace::copy() {
  GfxCalRGBColorSpace *cs;
  int i;

  cs = new GfxCalRGBColorSpace();
  cs->whiteX = whiteX;
  cs->whiteY = whiteY;
  cs->whiteZ = whiteZ;
  cs->blackX = blackX;
  cs->blackY = blackY;
  cs->blackZ = blackZ;
  cs->gammaR = gammaR;
  cs->gammaG = gammaG;
  cs->gammaB = gammaB;
  for (i = 0; i < 9; ++i) {
    cs->mat[i] = mat[i];
  }
  return cs;
}

GfxColorSpace *GfxCalRGBColorSpace::parse(Array *arr) {
  GfxCalRGBColorSpace *cs;
  Object obj1, obj2, obj3;
  int i;

  arr->get(1, &obj1);
  if (!obj1.isDict()) {
    error(-1, "Bad CalRGB color space");
    obj1.free();
    return NULL;
  }
  cs = new GfxCalRGBColorSpace();
  if (obj1.dictLookup("WhitePoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->whiteX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->whiteY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->whiteZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("BlackPoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->blackX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->blackY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->blackZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("Gamma", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->gammaR = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->gammaG = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->gammaB = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("Matrix", &obj2)->isArray() &&
      obj2.arrayGetLength() == 9) {
    for (i = 0; i < 9; ++i) {
      obj2.arrayGet(i, &obj3);
      cs->mat[i] = obj3.getNum();
      obj3.free();
    }
  }
  obj2.free();
  obj1.free();
  return cs;
}

void GfxCalRGBColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = clip01(0.299 * color->c[0] +
		 0.587 * color->c[1] +
		 0.114 * color->c[2]);
}

void GfxCalRGBColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  rgb->r = clip01(color->c[0]);
  rgb->g = clip01(color->c[1]);
  rgb->b = clip01(color->c[2]);
}

void GfxCalRGBColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  double c, m, y, k;

  c = clip01(1 - color->c[0]);
  m = clip01(1 - color->c[1]);
  y = clip01(1 - color->c[2]);
  k = c;
  if (m < k) {
    k = m;
  }
  if (y < k) {
    k = y;
  }
  cmyk->c = c - k;
  cmyk->m = m - k;
  cmyk->y = y - k;
  cmyk->k = k;
}

//------------------------------------------------------------------------
// GfxDeviceCMYKColorSpace
//------------------------------------------------------------------------

GfxDeviceCMYKColorSpace::GfxDeviceCMYKColorSpace() {
}

GfxDeviceCMYKColorSpace::~GfxDeviceCMYKColorSpace() {
}

GfxColorSpace *GfxDeviceCMYKColorSpace::copy() {
  return new GfxDeviceCMYKColorSpace();
}

void GfxDeviceCMYKColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = clip01(1 - color->c[3]
		 - 0.299 * color->c[0]
		 - 0.587 * color->c[1]
		 - 0.114 * color->c[2]);
}

void GfxDeviceCMYKColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  double c, m, y, aw, ac, am, ay, ar, ag, ab;

  c = clip01(color->c[0] + color->c[3]);
  m = clip01(color->c[1] + color->c[3]);
  y = clip01(color->c[2] + color->c[3]);
  aw = (1-c) * (1-m) * (1-y);
  ac = c * (1-m) * (1-y);
  am = (1-c) * m * (1-y);
  ay = (1-c) * (1-m) * y;
  ar = (1-c) * m * y;
  ag = c * (1-m) * y;
  ab = c * m * (1-y);
  rgb->r = clip01(aw + 0.9137*am + 0.9961*ay + 0.9882*ar);
  rgb->g = clip01(aw + 0.6196*ac + ay + 0.5176*ag);
  rgb->b = clip01(aw + 0.7804*ac + 0.5412*am + 0.0667*ar + 0.2118*ag +
		  0.4863*ab);
}

void GfxDeviceCMYKColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  cmyk->c = clip01(color->c[0]);
  cmyk->m = clip01(color->c[1]);
  cmyk->y = clip01(color->c[2]);
  cmyk->k = clip01(color->c[3]);
}

//------------------------------------------------------------------------
// GfxLabColorSpace
//------------------------------------------------------------------------

// This is the inverse of MatrixLMN in Example 4.10 from the PostScript
// Language Reference, Third Edition.
static double xyzrgb[3][3] = {
  {  3.240449, -1.537136, -0.498531 },
  { -0.969265,  1.876011,  0.041556 },
  {  0.055643, -0.204026,  1.057229 }
};

GfxLabColorSpace::GfxLabColorSpace() {
  whiteX = whiteY = whiteZ = 1;
  blackX = blackY = blackZ = 0;
  aMin = bMin = -100;
  aMax = bMax = 100;
}

GfxLabColorSpace::~GfxLabColorSpace() {
}

GfxColorSpace *GfxLabColorSpace::copy() {
  GfxLabColorSpace *cs;

  cs = new GfxLabColorSpace();
  cs->whiteX = whiteX;
  cs->whiteY = whiteY;
  cs->whiteZ = whiteZ;
  cs->blackX = blackX;
  cs->blackY = blackY;
  cs->blackZ = blackZ;
  cs->aMin = aMin;
  cs->aMax = aMax;
  cs->bMin = bMin;
  cs->bMax = bMax;
  cs->kr = kr;
  cs->kg = kg;
  cs->kb = kb;
  return cs;
}

GfxColorSpace *GfxLabColorSpace::parse(Array *arr) {
  GfxLabColorSpace *cs;
  Object obj1, obj2, obj3;

  arr->get(1, &obj1);
  if (!obj1.isDict()) {
    error(-1, "Bad Lab color space");
    obj1.free();
    return NULL;
  }
  cs = new GfxLabColorSpace();
  if (obj1.dictLookup("WhitePoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->whiteX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->whiteY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->whiteZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("BlackPoint", &obj2)->isArray() &&
      obj2.arrayGetLength() == 3) {
    obj2.arrayGet(0, &obj3);
    cs->blackX = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->blackY = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->blackZ = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  if (obj1.dictLookup("Range", &obj2)->isArray() &&
      obj2.arrayGetLength() == 4) {
    obj2.arrayGet(0, &obj3);
    cs->aMin = obj3.getNum();
    obj3.free();
    obj2.arrayGet(1, &obj3);
    cs->aMax = obj3.getNum();
    obj3.free();
    obj2.arrayGet(2, &obj3);
    cs->bMin = obj3.getNum();
    obj3.free();
    obj2.arrayGet(3, &obj3);
    cs->bMax = obj3.getNum();
    obj3.free();
  }
  obj2.free();
  obj1.free();

  cs->kr = 1 / (xyzrgb[0][0] * cs->whiteX +
		xyzrgb[0][1] * cs->whiteY +
		xyzrgb[0][2] * cs->whiteZ);
  cs->kg = 1 / (xyzrgb[1][0] * cs->whiteX +
		xyzrgb[1][1] * cs->whiteY +
		xyzrgb[1][2] * cs->whiteZ);
  cs->kb = 1 / (xyzrgb[2][0] * cs->whiteX +
		xyzrgb[2][1] * cs->whiteY +
		xyzrgb[2][2] * cs->whiteZ);

  return cs;
}

void GfxLabColorSpace::getGray(GfxColor *color, double *gray) {
  GfxRGB rgb;

  getRGB(color, &rgb);
  *gray = clip01(0.299 * rgb.r +
		 0.587 * rgb.g +
		 0.114 * rgb.b);
}

void GfxLabColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  double X, Y, Z;
  double t1, t2;
  double r, g, b;

  // convert L*a*b* to CIE 1931 XYZ color space
  t1 = (color->c[0] + 16) / 116;
  t2 = t1 + color->c[1] / 500;
  if (t2 >= (6.0 / 29.0)) {
    X = t2 * t2 * t2;
  } else {
    X = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
  }
  X *= whiteX;
  if (t1 >= (6.0 / 29.0)) {
    Y = t1 * t1 * t1;
  } else {
    Y = (108.0 / 841.0) * (t1 - (4.0 / 29.0));
  }
  Y *= whiteY;
  t2 = t1 - color->c[2] / 200;
  if (t2 >= (6.0 / 29.0)) {
    Z = t2 * t2 * t2;
  } else {
    Z = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
  }
  Z *= whiteZ;

  // convert XYZ to RGB, including gamut mapping and gamma correction
  r = xyzrgb[0][0] * X + xyzrgb[0][1] * Y + xyzrgb[0][2] * Z;
  g = xyzrgb[1][0] * X + xyzrgb[1][1] * Y + xyzrgb[1][2] * Z;
  b = xyzrgb[2][0] * X + xyzrgb[2][1] * Y + xyzrgb[2][2] * Z;
  rgb->r = pow(clip01(r * kr), 0.5);
  rgb->g = pow(clip01(g * kg), 0.5);
  rgb->b = pow(clip01(b * kb), 0.5);
}

void GfxLabColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  GfxRGB rgb;
  double c, m, y, k;

  getRGB(color, &rgb);
  c = clip01(1 - rgb.r);
  m = clip01(1 - rgb.g);
  y = clip01(1 - rgb.b);
  k = c;
  if (m < k) {
    k = m;
  }
  if (y < k) {
    k = y;
  }
  cmyk->c = c - k;
  cmyk->m = m - k;
  cmyk->y = y - k;
  cmyk->k = k;
}

void GfxLabColorSpace::getDefaultRanges(double *decodeLow, double *decodeRange,
					int maxImgPixel) {
  decodeLow[0] = 0;
  decodeRange[0] = 100;
  decodeLow[1] = aMin;
  decodeRange[1] = aMax - aMin;
  decodeLow[2] = bMin;
  decodeRange[2] = bMax - bMin;
}

//------------------------------------------------------------------------
// GfxICCBasedColorSpace
//------------------------------------------------------------------------

GfxICCBasedColorSpace::GfxICCBasedColorSpace(int nCompsA, GfxColorSpace *altA,
					     Ref *iccProfileStreamA) {
  nComps = nCompsA;
  alt = altA;
  iccProfileStream = *iccProfileStreamA;
  rangeMin[0] = rangeMin[1] = rangeMin[2] = rangeMin[3] = 0;
  rangeMax[0] = rangeMax[1] = rangeMax[2] = rangeMax[3] = 1;
}

GfxICCBasedColorSpace::~GfxICCBasedColorSpace() {
  delete alt;
}

GfxColorSpace *GfxICCBasedColorSpace::copy() {
  GfxICCBasedColorSpace *cs;
  int i;

  cs = new GfxICCBasedColorSpace(nComps, alt->copy(), &iccProfileStream);
  for (i = 0; i < 4; ++i) {
    cs->rangeMin[i] = rangeMin[i];
    cs->rangeMax[i] = rangeMax[i];
  }
  return cs;
}

GfxColorSpace *GfxICCBasedColorSpace::parse(Array *arr) {
  GfxICCBasedColorSpace *cs;
  Ref iccProfileStreamA;
  int nCompsA;
  GfxColorSpace *altA;
  Dict *dict;
  Object obj1, obj2, obj3;
  int i;

  arr->getNF(1, &obj1);
  if (obj1.isRef()) {
    iccProfileStreamA = obj1.getRef();
  } else {
    iccProfileStreamA.num = 0;
    iccProfileStreamA.gen = 0;
  }
  obj1.free();
  arr->get(1, &obj1);
  if (!obj1.isStream()) {
    error(-1, "Bad ICCBased color space (stream)");
    obj1.free();
    return NULL;
  }
  dict = obj1.streamGetDict();
  if (!dict->lookup("N", &obj2)->isInt()) {
    error(-1, "Bad ICCBased color space (N)");
    obj2.free();
    obj1.free();
    return NULL;
  }
  nCompsA = obj2.getInt();
  obj2.free();
  if (dict->lookup("Alternate", &obj2)->isNull() ||
      !(altA = GfxColorSpace::parse(&obj2))) {
    switch (nCompsA) {
    case 1:
      altA = new GfxDeviceGrayColorSpace();
      break;
    case 3:
      altA = new GfxDeviceRGBColorSpace();
      break;
    case 4:
      altA = new GfxDeviceCMYKColorSpace();
      break;
    default:
      error(-1, "Bad ICCBased color space - invalid N");
      obj2.free();
      obj1.free();
      return NULL;
    }
  }
  obj2.free();
  cs = new GfxICCBasedColorSpace(nCompsA, altA, &iccProfileStreamA);
  if (dict->lookup("Range", &obj2)->isArray() &&
      obj2.arrayGetLength() == 2 * nCompsA) {
    for (i = 0; i < nCompsA; ++i) {
      obj2.arrayGet(2*i, &obj3);
      cs->rangeMin[i] = obj3.getNum();
      obj3.free();
      obj2.arrayGet(2*i+1, &obj3);
      cs->rangeMax[i] = obj3.getNum();
      obj3.free();
    }
  }
  obj2.free();
  obj1.free();
  return cs;
}

void GfxICCBasedColorSpace::getGray(GfxColor *color, double *gray) {
  alt->getGray(color, gray);
}

void GfxICCBasedColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  alt->getRGB(color, rgb);
}

void GfxICCBasedColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  alt->getCMYK(color, cmyk);
}

void GfxICCBasedColorSpace::getDefaultRanges(double *decodeLow,
					     double *decodeRange,
					     int maxImgPixel) {
  int i;

  for (i = 0; i < nComps; ++i) {
    decodeLow[i] = rangeMin[i];
    decodeRange[i] = rangeMax[i] - rangeMin[i];
  }
}

//------------------------------------------------------------------------
// GfxIndexedColorSpace
//------------------------------------------------------------------------

GfxIndexedColorSpace::GfxIndexedColorSpace(GfxColorSpace *baseA,
					   int indexHighA) {
  base = baseA;
  indexHigh = indexHighA;
  lookup = (Guchar *)gmalloc((indexHigh + 1) * base->getNComps() *
			     sizeof(Guchar));
}

GfxIndexedColorSpace::~GfxIndexedColorSpace() {
  delete base;
  gfree(lookup);
}

GfxColorSpace *GfxIndexedColorSpace::copy() {
  GfxIndexedColorSpace *cs;

  cs = new GfxIndexedColorSpace(base->copy(), indexHigh);
  memcpy(cs->lookup, lookup,
	 (indexHigh + 1) * base->getNComps() * sizeof(Guchar));
  return cs;
}

GfxColorSpace *GfxIndexedColorSpace::parse(Array *arr) {
  GfxIndexedColorSpace *cs;
  GfxColorSpace *baseA;
  int indexHighA;
  Object obj1;
  int x;
  char *s;
  int n, i, j;

  if (arr->getLength() != 4) {
    error(-1, "Bad Indexed color space");
    goto err1;
  }
  arr->get(1, &obj1);
  if (!(baseA = GfxColorSpace::parse(&obj1))) {
    error(-1, "Bad Indexed color space (base color space)");
    goto err2;
  }
  obj1.free();
  if (!arr->get(2, &obj1)->isInt()) {
    error(-1, "Bad Indexed color space (hival)");
    delete baseA;
    goto err2;
  }
  indexHighA = obj1.getInt();
  if (indexHighA < 0 || indexHighA > 255) {
    // the PDF spec requires indexHigh to be in [0,255] -- allowing
    // values larger than 255 creates a security hole: if nComps *
    // indexHigh is greater than 2^31, the loop below may overwrite
    // past the end of the array
    error(-1, "Bad Indexed color space (invalid indexHigh value)");
    delete baseA;
    goto err2;
  }
  obj1.free();
  cs = new GfxIndexedColorSpace(baseA, indexHighA);
  arr->get(3, &obj1);
  n = baseA->getNComps();
  if (obj1.isStream()) {
    obj1.streamReset();
    for (i = 0; i <= indexHighA; ++i) {
      for (j = 0; j < n; ++j) {
	if ((x = obj1.streamGetChar()) == EOF) {
	  error(-1, "Bad Indexed color space (lookup table stream too short)");
	  goto err3;
	}
	cs->lookup[i*n + j] = (Guchar)x;
      }
    }
    obj1.streamClose();
  } else if (obj1.isString()) {
    if (obj1.getString()->getLength() < (indexHighA + 1) * n) {
      error(-1, "Bad Indexed color space (lookup table string too short)");
      goto err3;
    }
    s = obj1.getString()->getCString();
    for (i = 0; i <= indexHighA; ++i) {
      for (j = 0; j < n; ++j) {
	cs->lookup[i*n + j] = (Guchar)*s++;
      }
    }
  } else {
    error(-1, "Bad Indexed color space (lookup table)");
    goto err3;
  }
  obj1.free();
  return cs;

 err3:
  delete cs;
 err2:
  obj1.free();
 err1:
  return NULL;
}

GfxColor *GfxIndexedColorSpace::mapColorToBase(GfxColor *color,
					       GfxColor *baseColor) {
  Guchar *p;
  double low[gfxColorMaxComps], range[gfxColorMaxComps];
  int n, i;

  n = base->getNComps();
  base->getDefaultRanges(low, range, indexHigh);
  p = &lookup[(int)(color->c[0] + 0.5) * n];
  for (i = 0; i < n; ++i) {
    baseColor->c[i] = low[i] + (p[i] / 255.0) * range[i];
  }
  return baseColor;
}

void GfxIndexedColorSpace::getGray(GfxColor *color, double *gray) {
  GfxColor color2;

  base->getGray(mapColorToBase(color, &color2), gray);
}

void GfxIndexedColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  GfxColor color2;

  base->getRGB(mapColorToBase(color, &color2), rgb);
}

void GfxIndexedColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  GfxColor color2;

  base->getCMYK(mapColorToBase(color, &color2), cmyk);
}

void GfxIndexedColorSpace::getDefaultRanges(double *decodeLow,
					    double *decodeRange,
					    int maxImgPixel) {
  decodeLow[0] = 0;
  decodeRange[0] = maxImgPixel;
}

//------------------------------------------------------------------------
// GfxSeparationColorSpace
//------------------------------------------------------------------------

GfxSeparationColorSpace::GfxSeparationColorSpace(GString *nameA,
						 GfxColorSpace *altA,
						 Function *funcA) {
  name = nameA;
  alt = altA;
  func = funcA;
}

GfxSeparationColorSpace::~GfxSeparationColorSpace() {
  delete name;
  delete alt;
  delete func;
}

GfxColorSpace *GfxSeparationColorSpace::copy() {
  return new GfxSeparationColorSpace(name->copy(), alt->copy(), func->copy());
}

//~ handle the 'All' and 'None' colorants
GfxColorSpace *GfxSeparationColorSpace::parse(Array *arr) {
  GfxSeparationColorSpace *cs;
  GString *nameA;
  GfxColorSpace *altA;
  Function *funcA;
  Object obj1;

  if (arr->getLength() != 4) {
    error(-1, "Bad Separation color space");
    goto err1;
  }
  if (!arr->get(1, &obj1)->isName()) {
    error(-1, "Bad Separation color space (name)");
    goto err2;
  }
  nameA = new GString(obj1.getName());
  obj1.free();
  arr->get(2, &obj1);
  if (!(altA = GfxColorSpace::parse(&obj1))) {
    error(-1, "Bad Separation color space (alternate color space)");
    goto err3;
  }
  obj1.free();
  arr->get(3, &obj1);
  if (!(funcA = Function::parse(&obj1))) {
    goto err4;
  }
  obj1.free();
  cs = new GfxSeparationColorSpace(nameA, altA, funcA);
  return cs;

 err4:
  delete altA;
 err3:
  delete nameA;
 err2:
  obj1.free();
 err1:
  return NULL;
}

void GfxSeparationColorSpace::getGray(GfxColor *color, double *gray) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getGray(&color2, gray);
}

void GfxSeparationColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getRGB(&color2, rgb);
}

void GfxSeparationColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getCMYK(&color2, cmyk);
}

//------------------------------------------------------------------------
// GfxDeviceNColorSpace
//------------------------------------------------------------------------

GfxDeviceNColorSpace::GfxDeviceNColorSpace(int nCompsA,
					   GfxColorSpace *altA,
					   Function *funcA) {
  nComps = nCompsA;
  alt = altA;
  func = funcA;
}

GfxDeviceNColorSpace::~GfxDeviceNColorSpace() {
  int i;

  for (i = 0; i < nComps; ++i) {
    delete names[i];
  }
  delete alt;
  delete func;
}

GfxColorSpace *GfxDeviceNColorSpace::copy() {
  GfxDeviceNColorSpace *cs;
  int i;

  cs = new GfxDeviceNColorSpace(nComps, alt->copy(), func->copy());
  for (i = 0; i < nComps; ++i) {
    cs->names[i] = names[i]->copy();
  }
  return cs;
}

//~ handle the 'None' colorant
GfxColorSpace *GfxDeviceNColorSpace::parse(Array *arr) {
  GfxDeviceNColorSpace *cs;
  int nCompsA;
  GString *namesA[gfxColorMaxComps];
  GfxColorSpace *altA;
  Function *funcA;
  Object obj1, obj2;
  int i;

  if (arr->getLength() != 4 && arr->getLength() != 5) {
    error(-1, "Bad DeviceN color space");
    goto err1;
  }
  if (!arr->get(1, &obj1)->isArray()) {
    error(-1, "Bad DeviceN color space (names)");
    goto err2;
  }
  nCompsA = obj1.arrayGetLength();
  for (i = 0; i < nCompsA; ++i) {
    if (!obj1.arrayGet(i, &obj2)->isName()) {
      error(-1, "Bad DeviceN color space (names)");
      obj2.free();
      goto err2;
    }
    namesA[i] = new GString(obj2.getName());
    obj2.free();
  }
  obj1.free();
  arr->get(2, &obj1);
  if (!(altA = GfxColorSpace::parse(&obj1))) {
    error(-1, "Bad DeviceN color space (alternate color space)");
    goto err3;
  }
  obj1.free();
  arr->get(3, &obj1);
  if (!(funcA = Function::parse(&obj1))) {
    goto err4;
  }
  obj1.free();
  cs = new GfxDeviceNColorSpace(nCompsA, altA, funcA);
  for (i = 0; i < nCompsA; ++i) {
    cs->names[i] = namesA[i];
  }
  return cs;

 err4:
  delete altA;
 err3:
  for (i = 0; i < nCompsA; ++i) {
    delete namesA[i];
  }
 err2:
  obj1.free();
 err1:
  return NULL;
}

void GfxDeviceNColorSpace::getGray(GfxColor *color, double *gray) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getGray(&color2, gray);
}

void GfxDeviceNColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getRGB(&color2, rgb);
}

void GfxDeviceNColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  GfxColor color2;

  func->transform(color->c, color2.c);
  alt->getCMYK(&color2, cmyk);
}

//------------------------------------------------------------------------
// GfxPatternColorSpace
//------------------------------------------------------------------------

GfxPatternColorSpace::GfxPatternColorSpace(GfxColorSpace *underA) {
  under = underA;
}

GfxPatternColorSpace::~GfxPatternColorSpace() {
  if (under) {
    delete under;
  }
}

GfxColorSpace *GfxPatternColorSpace::copy() {
  return new GfxPatternColorSpace(under ? under->copy() :
				          (GfxColorSpace *)NULL);
}

GfxColorSpace *GfxPatternColorSpace::parse(Array *arr) {
  GfxPatternColorSpace *cs;
  GfxColorSpace *underA;
  Object obj1;

  if (arr->getLength() != 1 && arr->getLength() != 2) {
    error(-1, "Bad Pattern color space");
    return NULL;
  }
  underA = NULL;
  if (arr->getLength() == 2) {
    arr->get(1, &obj1);
    if (!(underA = GfxColorSpace::parse(&obj1))) {
      error(-1, "Bad Pattern color space (underlying color space)");
      obj1.free();
      return NULL;
    }
    obj1.free();
  }
  cs = new GfxPatternColorSpace(underA);
  return cs;
}

void GfxPatternColorSpace::getGray(GfxColor *color, double *gray) {
  *gray = 0;
}

void GfxPatternColorSpace::getRGB(GfxColor *color, GfxRGB *rgb) {
  rgb->r = rgb->g = rgb->b = 0;
}

void GfxPatternColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk) {
  cmyk->c = cmyk->m = cmyk->y = 0;
  cmyk->k = 1;
}

//------------------------------------------------------------------------
// Pattern
//------------------------------------------------------------------------

GfxPattern::GfxPattern(int typeA) {
  type = typeA;
}

GfxPattern::~GfxPattern() {
}

GfxPattern *GfxPattern::parse(Object *obj) {
  GfxPattern *pattern;
  Dict *dict;
  Object obj1;

  pattern = NULL;
  if (obj->isStream()) {
    dict = obj->streamGetDict();
    dict->lookup("PatternType", &obj1);
    if (obj1.isInt() && obj1.getInt() == 1) {
      pattern = new GfxTilingPattern(dict, obj);
    }
    obj1.free();
  }
  return pattern;
}

//------------------------------------------------------------------------
// GfxTilingPattern
//------------------------------------------------------------------------

GfxTilingPattern::GfxTilingPattern(Dict *streamDict, Object *stream):
  GfxPattern(1)
{
  Object obj1, obj2;
  int i;

  if (streamDict->lookup("PaintType", &obj1)->isInt()) {
    paintType = obj1.getInt();
  } else {
    paintType = 1;
    error(-1, "Invalid or missing PaintType in pattern");
  }
  obj1.free();
  if (streamDict->lookup("TilingType", &obj1)->isInt()) {
    tilingType = obj1.getInt();
  } else {
    tilingType = 1;
    error(-1, "Invalid or missing TilingType in pattern");
  }
  obj1.free();
  bbox[0] = bbox[1] = 0;
  bbox[2] = bbox[3] = 1;
  if (streamDict->lookup("BBox", &obj1)->isArray() &&
      obj1.arrayGetLength() == 4) {
    for (i = 0; i < 4; ++i) {
      if (obj1.arrayGet(i, &obj2)->isNum()) {
	bbox[i] = obj2.getNum();
      }
      obj2.free();
    }
  } else {
    error(-1, "Invalid or missing BBox in pattern");
  }
  obj1.free();
  if (streamDict->lookup("XStep", &obj1)->isNum()) {
    xStep = obj1.getNum();
  } else {
    xStep = 1;
    error(-1, "Invalid or missing XStep in pattern");
  }
  obj1.free();
  if (streamDict->lookup("YStep", &obj1)->isNum()) {
    yStep = obj1.getNum();
  } else {
    yStep = 1;
    error(-1, "Invalid or missing YStep in pattern");
  }
  obj1.free();
  if (!streamDict->lookup("Resources", &resDict)->isDict()) {
    resDict.free();
    resDict.initNull();
    error(-1, "Invalid or missing Resources in pattern");
  }
  matrix[0] = 1; matrix[1] = 0;
  matrix[2] = 0; matrix[3] = 1;
  matrix[4] = 0; matrix[5] = 0;
  if (streamDict->lookup("Matrix", &obj1)->isArray() &&
      obj1.arrayGetLength() == 6) {
    for (i = 0; i < 6; ++i) {
      if (obj1.arrayGet(i, &obj2)->isNum()) {
	matrix[i] = obj2.getNum();
      }
      obj2.free();
    }
  }
  obj1.free();
  stream->copy(&contentStream);
}

GfxTilingPattern::~GfxTilingPattern() {
  resDict.free();
  contentStream.free();
}

GfxPattern *GfxTilingPattern::copy() {
  return new GfxTilingPattern(this);
}

GfxTilingPattern::GfxTilingPattern(GfxTilingPattern *pat):
  GfxPattern(1)
{
  memcpy(this, pat, sizeof(GfxTilingPattern));
  pat->resDict.copy(&resDict);
  pat->contentStream.copy(&contentStream);
}

//------------------------------------------------------------------------
// GfxShading
//------------------------------------------------------------------------

GfxShading::GfxShading() {
}

GfxShading::~GfxShading() {
  delete colorSpace;
}

GfxShading *GfxShading::parse(Object *obj) {
  GfxShading *shading;
  int typeA;
  GfxColorSpace *colorSpaceA;
  GfxColor backgroundA;
  GBool hasBackgroundA;
  double xMinA, yMinA, xMaxA, yMaxA;
  GBool hasBBoxA;
  Object obj1, obj2;
  int i;

  shading = NULL;
  if (obj->isDict()) {

    if (!obj->dictLookup("ShadingType", &obj1)->isInt()) {
      error(-1, "Invalid ShadingType in shading dictionary");
      obj1.free();
      goto err1;
    }
    typeA = obj1.getInt();
    obj1.free();

    obj->dictLookup("ColorSpace", &obj1);
    if (!(colorSpaceA = GfxColorSpace::parse(&obj1))) {
      error(-1, "Bad color space in shading dictionary");
      obj1.free();
      goto err1;
    }
    obj1.free();

    for (i = 0; i < gfxColorMaxComps; ++i) {
      backgroundA.c[i] = 0;
    }
    hasBackgroundA = gFalse;
    if (obj->dictLookup("Background", &obj1)->isArray()) {
      if (obj1.arrayGetLength() == colorSpaceA->getNComps()) {
	hasBackgroundA = gTrue;
	for (i = 0; i < colorSpaceA->getNComps(); ++i) {
	  backgroundA.c[i] = obj1.arrayGet(i, &obj2)->getNum();
	  obj2.free();
	}
      } else {
	error(-1, "Bad Background in shading dictionary");
      }
    }
    obj1.free();

    xMinA = yMinA = xMaxA = yMaxA = 0;
    hasBBoxA = gFalse;
    if (obj->dictLookup("BBox", &obj1)->isArray()) {
      if (obj1.arrayGetLength() == 4) {
	hasBBoxA = gTrue;
	xMinA = obj1.arrayGet(0, &obj2)->getNum();
	obj2.free();
	yMinA = obj1.arrayGet(1, &obj2)->getNum();
	obj2.free();
	xMaxA = obj1.arrayGet(2, &obj2)->getNum();
	obj2.free();
	yMaxA = obj1.arrayGet(3, &obj2)->getNum();
	obj2.free();
      } else {
	error(-1, "Bad BBox in shading dictionary");
      }
    }
    obj1.free();

    switch (typeA) {
    case 2:
      shading = GfxAxialShading::parse(obj->getDict());
      break;
    case 3:
      shading = GfxRadialShading::parse(obj->getDict());
      break;
    default:
      error(-1, "Unimplemented shading type %d", typeA);
      goto err1;
    }

    if (shading) {
      shading->type = typeA;
      shading->colorSpace = colorSpaceA;
      shading->background = backgroundA;
      shading->hasBackground = hasBackgroundA;
      shading->xMin = xMinA;
      shading->yMin = yMinA;
      shading->xMax = xMaxA;
      shading->yMax = yMaxA;
      shading->hasBBox = hasBBoxA;
    } else {
      delete colorSpaceA;
    }
  }

  return shading;

 err1:
  return NULL;
}

//------------------------------------------------------------------------
// GfxAxialShading
//------------------------------------------------------------------------

GfxAxialShading::GfxAxialShading(double x0A, double y0A,
				 double x1A, double y1A,
				 double t0A, double t1A,
				 Function **funcsA, int nFuncsA,
				 GBool extend0A, GBool extend1A) {
  int i;

  x0 = x0A;
  y0 = y0A;
  x1 = x1A;
  y1 = y1A;
  t0 = t0A;
  t1 = t1A;
  nFuncs = nFuncsA;
  for (i = 0; i < nFuncs; ++i) {
    funcs[i] = funcsA[i];
  }
  extend0 = extend0A;
  extend1 = extend1A;
}

GfxAxialShading::~GfxAxialShading() {
  int i;

  for (i = 0; i < nFuncs; ++i) {
    delete funcs[i];
  }
}

GfxAxialShading *GfxAxialShading::parse(Dict *dict) {
  double x0A, y0A, x1A, y1A;
  double t0A, t1A;
  Function *funcsA[gfxColorMaxComps];
  int nFuncsA;
  GBool extend0A, extend1A;
  Object obj1, obj2;
  int i;

  x0A = y0A = x1A = y1A = 0;
  if (dict->lookup("Coords", &obj1)->isArray() &&
      obj1.arrayGetLength() == 4) {
    x0A = obj1.arrayGet(0, &obj2)->getNum();
    obj2.free();
    y0A = obj1.arrayGet(1, &obj2)->getNum();
    obj2.free();
    x1A = obj1.arrayGet(2, &obj2)->getNum();
    obj2.free();
    y1A = obj1.arrayGet(3, &obj2)->getNum();
    obj2.free();
  } else {
    error(-1, "Missing or invalid Coords in shading dictionary");
    goto err1;
  }
  obj1.free();

  t0A = 0;
  t1A = 1;
  if (dict->lookup("Domain", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2) {
    t0A = obj1.arrayGet(0, &obj2)->getNum();
    obj2.free();
    t1A = obj1.arrayGet(1, &obj2)->getNum();
    obj2.free();
  }
  obj1.free();

  dict->lookup("Function", &obj1);
  if (obj1.isArray()) {
    nFuncsA = obj1.arrayGetLength();
    for (i = 0; i < nFuncsA; ++i) {
      obj1.arrayGet(i, &obj2);
      if (!(funcsA[i] = Function::parse(&obj2))) {
	obj1.free();
	obj2.free();
	goto err1;
      }
      obj2.free();
    }
  } else {
    nFuncsA = 1;
    if (!(funcsA[0] = Function::parse(&obj1))) {
      obj1.free();
      goto err1;
    }
  }
  obj1.free();

  extend0A = extend1A = gFalse;
  if (dict->lookup("Extend", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2) {
    extend0A = obj1.arrayGet(0, &obj2)->getBool();
    obj2.free();
    extend1A = obj1.arrayGet(1, &obj2)->getBool();
    obj2.free();
  }
  obj1.free();

  return new GfxAxialShading(x0A, y0A, x1A, y1A, t0A, t1A,
			     funcsA, nFuncsA, extend0A, extend1A);

 err1:
  return NULL;
}

void GfxAxialShading::getColor(double t, GfxColor *color) {
  int i;

  for (i = 0; i < nFuncs; ++i) {
    funcs[i]->transform(&t, &color->c[i]);
  }
}

//------------------------------------------------------------------------
// GfxRadialShading
//------------------------------------------------------------------------

GfxRadialShading::GfxRadialShading(double x0A, double y0A, double r0A,
				   double x1A, double y1A, double r1A,
				   double t0A, double t1A,
				   Function **funcsA, int nFuncsA,
				   GBool extend0A, GBool extend1A) {
  int i;

  x0 = x0A;
  y0 = y0A;
  r0 = r0A;
  x1 = x1A;
  y1 = y1A;
  r1 = r1A;
  t0 = t0A;
  t1 = t1A;
  nFuncs = nFuncsA;
  for (i = 0; i < nFuncs; ++i) {
    funcs[i] = funcsA[i];
  }
  extend0 = extend0A;
  extend1 = extend1A;
}

GfxRadialShading::~GfxRadialShading() {
  int i;

  for (i = 0; i < nFuncs; ++i) {
    delete funcs[i];
  }
}

GfxRadialShading *GfxRadialShading::parse(Dict *dict) {
  double x0A, y0A, r0A, x1A, y1A, r1A;
  double t0A, t1A;
  Function *funcsA[gfxColorMaxComps];
  int nFuncsA;
  GBool extend0A, extend1A;
  Object obj1, obj2;
  int i;

  x0A = y0A = r0A = x1A = y1A = r1A = 0;
  if (dict->lookup("Coords", &obj1)->isArray() &&
      obj1.arrayGetLength() == 6) {
    x0A = obj1.arrayGet(0, &obj2)->getNum();
    obj2.free();
    y0A = obj1.arrayGet(1, &obj2)->getNum();
    obj2.free();
    r0A = obj1.arrayGet(2, &obj2)->getNum();
    obj2.free();
    x1A = obj1.arrayGet(3, &obj2)->getNum();
    obj2.free();
    y1A = obj1.arrayGet(4, &obj2)->getNum();
    obj2.free();
    r1A = obj1.arrayGet(5, &obj2)->getNum();
    obj2.free();
  } else {
    error(-1, "Missing or invalid Coords in shading dictionary");
    goto err1;
  }
  obj1.free();

  t0A = 0;
  t1A = 1;
  if (dict->lookup("Domain", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2) {
    t0A = obj1.arrayGet(0, &obj2)->getNum();
    obj2.free();
    t1A = obj1.arrayGet(1, &obj2)->getNum();
    obj2.free();
  }
  obj1.free();

  dict->lookup("Function", &obj1);
  if (obj1.isArray()) {
    nFuncsA = obj1.arrayGetLength();
    for (i = 0; i < nFuncsA; ++i) {
      obj1.arrayGet(i, &obj2);
      if (!(funcsA[i] = Function::parse(&obj2))) {
	obj1.free();
	obj2.free();
	goto err1;
      }
      obj2.free();
    }
  } else {
    nFuncsA = 1;
    if (!(funcsA[0] = Function::parse(&obj1))) {
      obj1.free();
      goto err1;
    }
  }
  obj1.free();

  extend0A = extend1A = gFalse;
  if (dict->lookup("Extend", &obj1)->isArray() &&
      obj1.arrayGetLength() == 2) {
    extend0A = obj1.arrayGet(0, &obj2)->getBool();
    obj2.free();
    extend1A = obj1.arrayGet(1, &obj2)->getBool();
    obj2.free();
  }
  obj1.free();

  return new GfxRadialShading(x0A, y0A, r0A, x1A, y1A, r1A, t0A, t1A,
			      funcsA, nFuncsA, extend0A, extend1A);

 err1:
  return NULL;
}

void GfxRadialShading::getColor(double t, GfxColor *color) {
  int i;

  for (i = 0; i < nFuncs; ++i) {
    funcs[i]->transform(&t, &color->c[i]);
  }
}

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

GfxImageColorMap::GfxImageColorMap(int bitsA, Object *decode,
				   GfxColorSpace *colorSpaceA) {
  GfxIndexedColorSpace *indexedCS;
  GfxSeparationColorSpace *sepCS;
  int maxPixel, indexHigh;
  Guchar *lookup2;
  Function *sepFunc;
  Object obj;
  double x[gfxColorMaxComps];
  double y[gfxColorMaxComps];
  int i, j, k;

  ok = gTrue;

  // bits per component and color space
  bits = bitsA;
  maxPixel = (1 << bits) - 1;
  colorSpace = colorSpaceA;

  // get decode map
  if (decode->isNull()) {
    nComps = colorSpace->getNComps();
    colorSpace->getDefaultRanges(decodeLow, decodeRange, maxPixel);
  } else if (decode->isArray()) {
    nComps = decode->arrayGetLength() / 2;
    if (nComps != colorSpace->getNComps()) {
      goto err1;
    }
    for (i = 0; i < nComps; ++i) {
      decode->arrayGet(2*i, &obj);
      if (!obj.isNum()) {
	goto err2;
      }
      decodeLow[i] = obj.getNum();
      obj.free();
      decode->arrayGet(2*i+1, &obj);
      if (!obj.isNum()) {
	goto err2;
      }
      decodeRange[i] = obj.getNum() - decodeLow[i];
      obj.free();
    }
  } else {
    goto err1;
  }

  // Construct a lookup table -- this stores pre-computed decoded
  // values for each component, i.e., the result of applying the
  // decode mapping to each possible image pixel component value.
  //
  // Optimization: for Indexed and Separation color spaces (which have
  // only one component), we store color values in the lookup table
  // rather than component values.
  colorSpace2 = NULL;
  nComps2 = 0;
  if (colorSpace->getMode() == csIndexed) {
    // Note that indexHigh may not be the same as maxPixel --
    // Distiller will remove unused palette entries, resulting in
    // indexHigh < maxPixel.
    indexedCS = (GfxIndexedColorSpace *)colorSpace;
    colorSpace2 = indexedCS->getBase();
    indexHigh = indexedCS->getIndexHigh();
    nComps2 = colorSpace2->getNComps();
    lookup = (double *)gmalloc((indexHigh + 1) * nComps2 * sizeof(double));
    lookup2 = indexedCS->getLookup();
    colorSpace2->getDefaultRanges(x, y, indexHigh);
    for (i = 0; i <= indexHigh; ++i) {
      j = (int)(decodeLow[0] + (i * decodeRange[0]) / maxPixel + 0.5);
      for (k = 0; k < nComps2; ++k) {
	lookup[j*nComps2 + k] = x[k] + (lookup2[i*nComps2 + k] / 255.0) * y[k];
      }
    }
  } else if (colorSpace->getMode() == csSeparation) {
    sepCS = (GfxSeparationColorSpace *)colorSpace;
    colorSpace2 = sepCS->getAlt();
    nComps2 = colorSpace2->getNComps();
    lookup = (double *)gmalloc((maxPixel + 1) * nComps2 * sizeof(double));
    sepFunc = sepCS->getFunc();
    for (i = 0; i <= maxPixel; ++i) {
      x[0] = decodeLow[0] + (i * decodeRange[0]) / maxPixel;
      sepFunc->transform(x, y);
      for (k = 0; k < nComps2; ++k) {
	lookup[i*nComps2 + k] = y[k];
      }
    }
  } else {
    lookup = (double *)gmalloc((maxPixel + 1) * nComps * sizeof(double));
    for (i = 0; i <= maxPixel; ++i) {
      for (k = 0; k < nComps; ++k) {
	lookup[i*nComps + k] = decodeLow[k] +
	                         (i * decodeRange[k]) / maxPixel;
      }
    }
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

void GfxImageColorMap::getGray(Guchar *x, double *gray) {
  GfxColor color;
  double *p;
  int i;

  if (colorSpace2) {
    p = &lookup[x[0] * nComps2];
    for (i = 0; i < nComps2; ++i) {
      color.c[i] = *p++;
    }
    colorSpace2->getGray(&color, gray);
  } else {
    for (i = 0; i < nComps; ++i) {
      color.c[i] = lookup[x[i] * nComps + i];
    }
    colorSpace->getGray(&color, gray);
  }
}

void GfxImageColorMap::getRGB(Guchar *x, GfxRGB *rgb) {
  GfxColor color;
  double *p;
  int i;

  if (colorSpace2) {
    p = &lookup[x[0] * nComps2];
    for (i = 0; i < nComps2; ++i) {
      color.c[i] = *p++;
    }
    colorSpace2->getRGB(&color, rgb);
  } else {
    for (i = 0; i < nComps; ++i) {
      color.c[i] = lookup[x[i] * nComps + i];
    }
    colorSpace->getRGB(&color, rgb);
  }
}

void GfxImageColorMap::getCMYK(Guchar *x, GfxCMYK *cmyk) {
  GfxColor color;
  double *p;
  int i;

  if (colorSpace2) {
    p = &lookup[x[0] * nComps2];
    for (i = 0; i < nComps2; ++i) {
      color.c[i] = *p++;
    }
    colorSpace2->getCMYK(&color, cmyk);
  } else {
    for (i = 0; i < nComps; ++i) {
      color.c[i] = lookup[x[i] * nComps + i];
    }
    colorSpace->getCMYK(&color, cmyk);
  }
}

void GfxImageColorMap::getColor(Guchar *x, GfxColor *color) {
  int maxPixel, i;

  maxPixel = (1 << bits) - 1;
  for (i = 0; i < nComps; ++i) {
    color->c[i] = decodeLow[i] + (x[i] * decodeRange[i]) / maxPixel;
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

void GfxPath::close() {
  // this is necessary to handle the pathological case of
  // moveto/closepath/clip, which defines an empty clipping region
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
  subpaths[n-1]->close();
}

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

GfxState::GfxState(double dpi, PDFRectangle *pageBox, int rotate,
		   GBool upsideDown) {
  double k;

  px1 = pageBox->x1;
  py1 = pageBox->y1;
  px2 = pageBox->x2;
  py2 = pageBox->y2;
  k = dpi / 72.0;
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

  fillColorSpace = new GfxDeviceGrayColorSpace();
  strokeColorSpace = new GfxDeviceGrayColorSpace();
  fillColor.c[0] = 0;
  strokeColor.c[0] = 0;
  fillPattern = NULL;
  strokePattern = NULL;
  fillOpacity = 1;
  strokeOpacity = 1;

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

  clipXMin = 0;
  clipYMin = 0;
  clipXMax = pageWidth;
  clipYMax = pageHeight;

  saved = NULL;
}

GfxState::~GfxState() {
  if (fillColorSpace) {
    delete fillColorSpace;
  }
  if (strokeColorSpace) {
    delete strokeColorSpace;
  }
  if (fillPattern) {
    delete fillPattern;
  }
  if (strokePattern) {
    delete strokePattern;
  }
  gfree(lineDash);
  if (path) {
    // this gets set to NULL by restore()
    delete path;
  }
  if (saved) {
    delete saved;
  }
}

// Used for copy();
GfxState::GfxState(GfxState *state) {
  memcpy(this, state, sizeof(GfxState));
  if (fillColorSpace) {
    fillColorSpace = state->fillColorSpace->copy();
  }
  if (strokeColorSpace) {
    strokeColorSpace = state->strokeColorSpace->copy();
  }
  if (fillPattern) {
    fillPattern = state->fillPattern->copy();
  }
  if (strokePattern) {
    strokePattern = state->strokePattern->copy();
  }
  if (lineDashLength > 0) {
    lineDash = (double *)gmalloc(lineDashLength * sizeof(double));
    memcpy(lineDash, state->lineDash, lineDashLength * sizeof(double));
  }
  saved = NULL;
}

void GfxState::getUserClipBBox(double *xMin, double *yMin,
			       double *xMax, double *yMax) {
  double ictm[6];
  double xMin1, yMin1, xMax1, yMax1, det, tx, ty;

  // invert the CTM
  det = 1 / (ctm[0] * ctm[3] - ctm[1] * ctm[2]);
  ictm[0] = ctm[3] * det;
  ictm[1] = -ctm[1] * det;
  ictm[2] = -ctm[2] * det;
  ictm[3] = ctm[0] * det;
  ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
  ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;

  // transform all four corners of the clip bbox; find the min and max
  // x and y values
  xMin1 = xMax1 = clipXMin * ictm[0] + clipYMin * ictm[2] + ictm[4];
  yMin1 = yMax1 = clipXMin * ictm[1] + clipYMin * ictm[3] + ictm[5];
  tx = clipXMin * ictm[0] + clipYMax * ictm[2] + ictm[4];
  ty = clipXMin * ictm[1] + clipYMax * ictm[3] + ictm[5];
  if (tx < xMin1) {
    xMin1 = tx;
  } else if (tx > xMax1) {
    xMax1 = tx;
  }
  if (ty < yMin1) {
    yMin1 = ty;
  } else if (ty > yMax1) {
    yMax1 = ty;
  }
  tx = clipXMax * ictm[0] + clipYMin * ictm[2] + ictm[4];
  ty = clipXMax * ictm[1] + clipYMin * ictm[3] + ictm[5];
  if (tx < xMin1) {
    xMin1 = tx;
  } else if (tx > xMax1) {
    xMax1 = tx;
  }
  if (ty < yMin1) {
    yMin1 = ty;
  } else if (ty > yMax1) {
    yMax1 = ty;
  }
  tx = clipXMax * ictm[0] + clipYMax * ictm[2] + ictm[4];
  ty = clipXMax * ictm[1] + clipYMax * ictm[3] + ictm[5];
  if (tx < xMin1) {
    xMin1 = tx;
  } else if (tx > xMax1) {
    xMax1 = tx;
  }
  if (ty < yMin1) {
    yMin1 = ty;
  } else if (ty > yMax1) {
    yMax1 = ty;
  }

  *xMin = xMin1;
  *yMin = yMin1;
  *xMax = xMax1;
  *yMax = yMax1;
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
  int i;

  ctm[0] = a;
  ctm[1] = b;
  ctm[2] = c;
  ctm[3] = d;
  ctm[4] = e;
  ctm[5] = f;

  // avoid FP exceptions on badly messed up PDF files
  for (i = 0; i < 6; ++i) {
    if (ctm[i] > 1e10) {
      ctm[i] = 1e10;
    } else if (ctm[i] < -1e10) {
      ctm[i] = -1e10;
    }
  }
}

void GfxState::concatCTM(double a, double b, double c,
			 double d, double e, double f) {
  double a1 = ctm[0];
  double b1 = ctm[1];
  double c1 = ctm[2];
  double d1 = ctm[3];
  int i;

  ctm[0] = a * a1 + b * c1;
  ctm[1] = a * b1 + b * d1;
  ctm[2] = c * a1 + d * c1;
  ctm[3] = c * b1 + d * d1;
  ctm[4] = e * a1 + f * c1 + ctm[4];
  ctm[5] = e * b1 + f * d1 + ctm[5];

  // avoid FP exceptions on badly messed up PDF files
  for (i = 0; i < 6; ++i) {
    if (ctm[i] > 1e10) {
      ctm[i] = 1e10;
    } else if (ctm[i] < -1e10) {
      ctm[i] = -1e10;
    }
  }
}

void GfxState::setFillColorSpace(GfxColorSpace *colorSpace) {
  if (fillColorSpace) {
    delete fillColorSpace;
  }
  fillColorSpace = colorSpace;
}

void GfxState::setStrokeColorSpace(GfxColorSpace *colorSpace) {
  if (strokeColorSpace) {
    delete strokeColorSpace;
  }
  strokeColorSpace = colorSpace;
}

void GfxState::setFillPattern(GfxPattern *pattern) {
  if (fillPattern) {
    delete fillPattern;
  }
  fillPattern = pattern;
}

void GfxState::setStrokePattern(GfxPattern *pattern) {
  if (strokePattern) {
    delete strokePattern;
  }
  strokePattern = pattern;
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

void GfxState::clip() {
  double xMin, yMin, xMax, yMax, x, y;
  GfxSubpath *subpath;
  int i, j;

  xMin = xMax = yMin = yMax = 0; // make gcc happy
  for (i = 0; i < path->getNumSubpaths(); ++i) {
    subpath = path->getSubpath(i);
    for (j = 0; j < subpath->getNumPoints(); ++j) {
      transform(subpath->getX(j), subpath->getY(j), &x, &y);
      if (i == 0 && j == 0) {
	xMin = xMax = x;
	yMin = yMax = y;
      } else {
	if (x < xMin) {
	  xMin = x;
	} else if (x > xMax) {
	  xMax = x;
	}
	if (y < yMin) {
	  yMin = y;
	} else if (y > yMax) {
	  yMax = y;
	}
      }
    }
  }
  if (xMin > clipXMin) {
    clipXMin = xMin;
  }
  if (yMin > clipYMin) {
    clipYMin = yMin;
  }
  if (xMax < clipXMax) {
    clipXMax = xMax;
  }
  if (yMax < clipYMax) {
    clipYMax = yMax;
  }
}

void GfxState::textShift(double tx, double ty) {
  double dx, dy;

  textTransformDelta(tx, ty, &dx, &dy);
  curX += dx;
  curY += dy;
}

void GfxState::shift(double dx, double dy) {
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

    // these attributes aren't saved/restored by the q/Q operators
    oldState->path = path;
    oldState->curX = curX;
    oldState->curY = curY;
    oldState->lineX = lineX;
    oldState->lineY = lineY;

    path = NULL;
    saved = NULL;
    delete this;

  } else {
    oldState = this;
  }

  return oldState;
}
