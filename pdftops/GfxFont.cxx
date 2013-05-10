//========================================================================
//
// GfxFont.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "GString.h"
#include "gmem.h"
#include "gfile.h"
#include "config.h"
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Error.h"
#include "Params.h"
#include "FontFile.h"
#include "GfxFont.h"

#include "FontInfo.h"
#if JAPANESE_SUPPORT
#include "Japan12CMapInfo.h"
#endif
#if CHINESE_SUPPORT
#include "GB12CMapInfo.h"
#endif

//------------------------------------------------------------------------

static int CDECL cmpWidthExcep(const void *w1, const void *w2);
static int CDECL cmpWidthExcepV(const void *w1, const void *w2);

//------------------------------------------------------------------------

static Gushort *defCharWidths[12] = {
  courierWidths,
  courierObliqueWidths,
  courierBoldWidths,
  courierBoldObliqueWidths,
  helveticaWidths,
  helveticaObliqueWidths,
  helveticaBoldWidths,
  helveticaBoldObliqueWidths,
  timesRomanWidths,
  timesItalicWidths,
  timesBoldWidths,
  timesBoldItalicWidths
};

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

GfxFont::GfxFont(char *tag1, Ref id1, Dict *fontDict) {
  BuiltinFont *builtinFont;
  Object obj1, obj2, obj3, obj4;
  int missingWidth;
  char *name2, *p;
  int i;

  // get font tag and ID
  tag = new GString(tag1);
  id = id1;

  // get base font name
  name = NULL;
  fontDict->lookup("BaseFont", &obj1);
  if (obj1.isName())
    name = new GString(obj1.getName());
  obj1.free();

  // is it a built-in font?
  builtinFont = NULL;
  if (name) {
    for (i = 0; i < numBuiltinFonts; ++i) {
      if (!strcmp(builtinFonts[i].name, name->getCString())) {
	builtinFont = &builtinFonts[i];
	break;
      }
    }
  }

  // get font type
  type = fontUnknownType;
  fontDict->lookup("Subtype", &obj1);
  if (obj1.isName("Type1"))
    type = fontType1;
  else if (obj1.isName("Type1C"))
    type = fontType1C;
  else if (obj1.isName("Type3"))
    type = fontType3;
  else if (obj1.isName("TrueType"))
    type = fontTrueType;
  else if (obj1.isName("Type0"))
    type = fontType0;
  obj1.free();
  is16 = gFalse;

  // assume Times-Roman by default (for substitution purposes)
  flags = fontSerif;

  // Newer Adobe tools are using Base14-compatible TrueType fonts
  // without embedding them, so munge the names into the equivalent
  // PostScript names.  This is a kludge -- it would be nice if Adobe
  // followed their own spec.
  if (type == fontTrueType) {
    p = name->getCString();
    name2 = NULL;
    if (!strncmp(p, "Arial", 5)) {
      if (!strcmp(p+5, ",Bold")) {
	name2 = "Helvetica-Bold";
      } else if (!strcmp(p+5, "Italic")) {
	name2 = "Helvetica-Oblique";
      } else if (!strcmp(p+5, "BoldItalic")) {
	name2 = "Helvetica-BoldOblique";
      } else {
	name2 = "Helvetica";
      }
    } else if (!strncmp(p, "TimesNewRoman", 13)) {
      if (!strcmp(p+5, ",Bold")) {
	name2 = "Times-Bold";
      } else if (!strcmp(p+5, "Italic")) {
	name2 = "Times-Italic";
      } else if (!strcmp(p+5, "BoldItalic")) {
	name2 = "Times-BoldItalic";
      } else {
	name2 = "Times-Roman";
      }
    } else if (!strncmp(p, "CourierNew", 10)) {
      if (!strcmp(p+5, ",Bold")) {
	name2 = "Courier-Bold";
      } else if (!strcmp(p+5, "Italic")) {
	name2 = "Courier-Oblique";
      } else if (!strcmp(p+5, "BoldItalic")) {
	name2 = "Courier-BoldOblique";
      } else {
	name2 = "Courier";
      }
    }
    if (name2) {
      delete name;
      name = new GString(name2);
    }
  }

  // get info from font descriptor
  embFontName = NULL;
  embFontID.num = -1;
  embFontID.gen = -1;
  missingWidth = 0;
  fontDict->lookup("FontDescriptor", &obj1);
  if (obj1.isDict()) {

    // get flags
    obj1.dictLookup("Flags", &obj2);
    if (obj2.isInt())
      flags = obj2.getInt();
    obj2.free();

    // get name
    obj1.dictLookup("FontName", &obj2);
    if (obj2.isName())
      embFontName = new GString(obj2.getName());
    obj2.free();

    // look for embedded font file
    if (type == fontType1) {
      obj1.dictLookupNF("FontFile", &obj2);
      if (obj2.isRef())
	embFontID = obj2.getRef();
      obj2.free();
    }
    if (embFontID.num == -1 && type == fontTrueType) {
      obj1.dictLookupNF("FontFile2", &obj2);
      if (obj2.isRef())
	embFontID = obj2.getRef();
      obj2.free();
    }
    if (embFontID.num == -1) {
      obj1.dictLookupNF("FontFile3", &obj2);
      if (obj2.isRef()) {
	embFontID = obj2.getRef();
	obj2.fetch(&obj3);
	if (obj3.isStream()) {
	  obj3.streamGetDict()->lookup("Subtype", &obj4);
	  if (obj4.isName("Type1"))
	    type = fontType1;
	  else if (obj4.isName("Type1C"))
	    type = fontType1C;
	  else if (obj4.isName("Type3"))
	    type = fontType3;
	  else if (obj4.isName("TrueType"))
	    type = fontTrueType;
	  else if (obj4.isName("Type0"))
	    type = fontType0;
	  obj4.free();
	}
	obj3.free();
      }
      obj2.free();
    }

    // look for MissingWidth
    obj1.dictLookup("MissingWidth", &obj2);
    if (obj2.isInt()) {
      missingWidth = obj2.getInt();
    }
    obj2.free();
  }
  obj1.free();

  // get Type3 font definition
  if (type == fontType3) {
    fontDict->lookup("CharProcs", &charProcs);
    if (!charProcs.isDict()) {
      error(-1, "Missing or invalid CharProcs dictionary in Type 3 font");
      charProcs.free();
    }
  }

  // look for an external font file
  extFontFile = NULL;
  if (type == fontType1 && name)
    findExtFontFile();

  // get font matrix
  fontMat[0] = fontMat[3] = 1;
  fontMat[1] = fontMat[2] = fontMat[4] = fontMat[5] = 0;
  if (fontDict->lookup("FontMatrix", &obj1)->isArray()) {
    for (i = 0; i < 6 && i < obj1.arrayGetLength(); ++i) {
      if (obj1.arrayGet(i, &obj2)->isNum())
	fontMat[i] = obj2.getNum();
      obj2.free();
    }
  }
  obj1.free();

  // get encoding and character widths
  if (type == fontType0) {
    getType0EncAndWidths(fontDict);
  } else {
    getEncAndWidths(fontDict, builtinFont, missingWidth);
  }
}

GfxFont::~GfxFont() {
  delete tag;
  if (name) {
    delete name;
  }
  if (!is16 && encoding) {
    delete encoding;
  }
  if (embFontName) {
    delete embFontName;
  }
  if (extFontFile) {
    delete extFontFile;
  }
  if (charProcs.isDict()) {
    charProcs.free();
  }
  if (is16) {
    gfree(widths16.exceps);
    gfree(widths16.excepsV);
  }
}

double GfxFont::getWidth(GString *s) {
  double w;
  int i;

  w = 0;
  for (i = 0; i < s->getLength(); ++i)
    w += widths[s->getChar(i) & 0xff];
  return w;
}

double GfxFont::getWidth16(int c) {
  double w;
  int a, b, m;

  w = widths16.defWidth;
  a = -1;
  b = widths16.numExceps;
  // invariant: widths16.exceps[a].last < c < widths16.exceps[b].first
  while (b - a > 1) {
    m = (a + b) / 2;
    if (widths16.exceps[m].last < c) {
      a = m;
    } else if (c < widths16.exceps[m].first) {
      b = m;
    } else {
      w = widths16.exceps[m].width;
      break;
    }
  }
  return w;
}

double GfxFont::getHeight16(int c) {
  double h;
  int a, b, m;

  h = widths16.defHeight;
  a = -1;
  b = widths16.numExcepsV;
  // invariant: widths16.excepsV[a].last < c < widths16.excepsV[b].first
  while (b - a > 1) {
    m = (a + b) / 2;
    if (widths16.excepsV[m].last < c) {
      a = m;
    } else if (c < widths16.excepsV[m].first) {
      b = m;
    } else {
      h = widths16.excepsV[m].height;
      break;
    }
  }
  return h;
}

double GfxFont::getOriginX16(int c) {
  double vx;
  int a, b, m;

  vx = widths16.defWidth / 2;
  a = -1;
  b = widths16.numExcepsV;
  // invariant: widths16.excepsV[a].last < c < widths16.excepsV[b].first
  while (b - a > 1) {
    m = (a + b) / 2;
    if (widths16.excepsV[m].last < c) {
      a = m;
    } else if (c < widths16.excepsV[m].first) {
      b = m;
    } else {
      vx = widths16.excepsV[m].vx;
      break;
    }
  }
  return vx;
}

double GfxFont::getOriginY16(int c) {
  double vy;
  int a, b, m;

  vy = widths16.defVY;
  a = -1;
  b = widths16.numExcepsV;
  // invariant: widths16.excepsV[a].last < c < widths16.excepsV[b].first
  while (b - a > 1) {
    m = (a + b) / 2;
    if (widths16.excepsV[m].last < c) {
      a = m;
    } else if (c < widths16.excepsV[m].first) {
      b = m;
    } else {
      vy = widths16.excepsV[m].vy;
      break;
    }
  }
  return vy;
}

Object *GfxFont::getCharProc(int code, Object *proc) {
  if (charProcs.isDict()) {
    charProcs.dictLookup(encoding->getCharName(code), proc);
  } else {
    proc->initNull();
  }
  return proc;
}

void GfxFont::getEncAndWidths(Dict *fontDict, BuiltinFont *builtinFont,
			      int missingWidth) {
  Object obj1, obj2, obj3;
  char *buf;
  int len;
  FontFile *fontFile;
  int code, i;

  // Encodings start with a base encoding, which can come from
  // (in order of priority):
  //   1. FontDict.Encoding or FontDict.Encoding.BaseEncoding
  //        - MacRoman / WinAnsi / Standard
  //   2. embedded font file
  //   3. default:
  //        - builtin --> builtin encoding
  //        - TrueType --> MacRomanEncoding
  //        - others --> StandardEncoding
  // and then add a list of differences from
  // FontDict.Encoding.Differences.

  // check FontDict for base encoding
  encoding = NULL;
  fontDict->lookup("Encoding", &obj1);
  if (obj1.isDict()) {
    obj1.dictLookup("BaseEncoding", &obj2);
    if (obj2.isName("MacRomanEncoding")) {
      encoding = macRomanEncoding.copy();
    } else if (obj2.isName("WinAnsiEncoding")) {
      encoding = winAnsiEncoding.copy();
    } else if (obj2.isName("StandardEncoding")) {
      encoding = standardEncoding.copy();
    }
    obj2.free();
  } else if (obj1.isName("MacRomanEncoding")) {
    encoding = macRomanEncoding.copy();
  } else if (obj1.isName("WinAnsiEncoding")) {
    encoding = winAnsiEncoding.copy();
  } else if (obj1.isName("StandardEncoding")) {
    encoding = standardEncoding.copy();
  }
  obj1.free();

  // check embedded or external font file for base encoding
  if ((type == fontType1 || type == fontType1C) &&
      (extFontFile || embFontID.num >= 0)) {
    if (extFontFile)
      buf = readExtFontFile(&len);
    else
      buf = readEmbFontFile(&len);
    if (buf) {
      if (type == fontType1)
	fontFile = new Type1FontFile(buf, len);
      else
	fontFile = new Type1CFontFile(buf, len);
      if (fontFile->getName()) {
	if (embFontName)
	  delete embFontName;
	embFontName = new GString(fontFile->getName());
      }
      if (!encoding)
	encoding = fontFile->getEncoding(gTrue);
      delete fontFile;
      gfree(buf);
    }
  }

  // get default base encoding
  if (!encoding) {
    if (builtinFont)
      encoding = builtinFont->encoding->copy();
    else if (type == fontTrueType)
      encoding = macRomanEncoding.copy();
    else
      encoding = standardEncoding.copy();
  }

  // merge differences into encoding
  fontDict->lookup("Encoding", &obj1);
  if (obj1.isDict()) {
    obj1.dictLookup("Differences", &obj2);
    if (obj2.isArray()) {
      code = 0;
      for (i = 0; i < obj2.arrayGetLength(); ++i) {
	obj2.arrayGet(i, &obj3);
	if (obj3.isInt()) {
	  code = obj3.getInt();
	} else if (obj3.isName()) {
	  if (code < 256)
	    encoding->addChar(code, copyString(obj3.getName()));
	  ++code;
	} else {
	  error(-1, "Wrong type in font encoding resource differences (%s)",
		obj3.getTypeName());
	}
	obj3.free();
      }
    }
    obj2.free();
  }
  obj1.free();

  // get character widths
  if (builtinFont)
    makeWidths(fontDict, builtinFont->encoding, builtinFont->widths,
	       missingWidth);
  else
    makeWidths(fontDict, NULL, NULL, missingWidth);
}

void GfxFont::findExtFontFile() {
  char **path;
  FILE *f;

  for (path = fontPath; *path; ++path) {
    extFontFile = appendToPath(new GString(*path), name->getCString());
    f = fopen(extFontFile->getCString(), "rb");
    if (!f) {
      extFontFile->append(".pfb");
      f = fopen(extFontFile->getCString(), "rb");
    }
    if (!f) {
      extFontFile->del(extFontFile->getLength() - 4, 4);
      extFontFile->append(".pfa");
      f = fopen(extFontFile->getCString(), "rb");
    }
    if (f) {
      fclose(f);
      break;
    }
    delete extFontFile;
    extFontFile = NULL;
  }
}

char *GfxFont::readExtFontFile(int *len) {
  FILE *f;
  char *buf;

  if (!(f = fopen(extFontFile->getCString(), "rb"))) {
    error(-1, "Internal: external font file '%s' vanished", extFontFile);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  *len = (int)ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = (char *)gmalloc(*len);
  if ((int)fread(buf, 1, *len, f) != *len)
    error(-1, "Error reading external font file '%s'", extFontFile);
  fclose(f);
  return buf;
}

char *GfxFont::readEmbFontFile(int *len) {
  char *buf;
  Object obj1, obj2;
  Stream *str;
  int c;
  int size, i;

  obj1.initRef(embFontID.num, embFontID.gen);
  obj1.fetch(&obj2);
  if (!obj2.isStream()) {
    error(-1, "Embedded font file is not a stream");
    obj2.free();
    obj1.free();
    embFontID.num = -1;
    return NULL;
  }
  str = obj2.getStream();

  buf = NULL;
  i = size = 0;
  str->reset();
  while ((c = str->getChar()) != EOF) {
    if (i == size) {
      size += 4096;
      buf = (char *)grealloc(buf, size);
    }
    buf[i++] = c;
  }
  *len = i;

  obj2.free();
  obj1.free();

  return buf;
}

void GfxFont::makeWidths(Dict *fontDict, FontEncoding *builtinEncoding,
			 Gushort *builtinWidths, int missingWidth) {
  Object obj1, obj2;
  int firstChar, lastChar;
  int code, code2;
  char *charName;
  Gushort *defWidths;
  int index;
  double mult;

  // initialize all widths
  for (code = 0; code < 256; ++code) {
    widths[code] = missingWidth * 0.001;
  }

  // use widths from built-in font
  if (builtinEncoding) {
    code2 = 0; // to make gcc happy
    for (code = 0; code < 256; ++code) {
      if ((charName = encoding->getCharName(code)) &&
	  (code2 = builtinEncoding->getCharCode(charName)) >= 0)
	widths[code] = builtinWidths[code2] * 0.001;
    }

  // get widths from font dict
  } else {
    fontDict->lookup("FirstChar", &obj1);
    firstChar = obj1.isInt() ? obj1.getInt() : 0;
    obj1.free();
    fontDict->lookup("LastChar", &obj1);
    lastChar = obj1.isInt() ? obj1.getInt() : 255;
    obj1.free();
    if (type == fontType3)
      mult = fontMat[0];
    else
      mult = 0.001;
    fontDict->lookup("Widths", &obj1);
    if (obj1.isArray()) {
      for (code = firstChar; code <= lastChar; ++code) {
	obj1.arrayGet(code - firstChar, &obj2);
	if (obj2.isNum())
	  widths[code] = obj2.getNum() * mult;
	obj2.free();
      }
    } else {

      // couldn't find widths -- use defaults 
#if 0 //~
      //~ certain PDF generators apparently don't include widths
      //~ for Arial and TimesNewRoman -- and this error message
      //~ is a nuisance
      error(-1, "No character widths resource for non-builtin font");
#endif
      if (isFixedWidth())
	index = 0;
      else if (isSerif())
	index = 8;
      else
	index = 4;
      if (isBold())
	index += 2;
      if (isItalic())
	index += 1;
      defWidths = defCharWidths[index];
      code2 = 0; // to make gcc happy
      for (code = 0; code < 256; ++code) {
	if ((charName = encoding->getCharName(code)) &&
	    (code2 = standardEncoding.getCharCode(charName)) >= 0)
	  widths[code] = defWidths[code2] * 0.001;
      }
    }
    obj1.free();
  }
}

void GfxFont::getType0EncAndWidths(Dict *fontDict) {
  Object obj1, obj2, obj3, obj4, obj5, obj6, obj7, obj8;
  int excepsSize;
  int i, j, k, n;

  widths16.exceps = NULL;
  widths16.excepsV = NULL;

  // get the CIDFont
  fontDict->lookup("DescendantFonts", &obj1);
  if (!obj1.isArray() || obj1.arrayGetLength() != 1) {
    error(-1, "Bad DescendantFonts entry for Type 0 font");
    goto err1;
  }
  obj1.arrayGet(0, &obj2);
  if (!obj2.isDict()) {
    error(-1, "Bad descendant font of Type 0 font");
    goto err2;
  }

  // get font info
  obj2.dictLookup("CIDSystemInfo", &obj3);
  if (!obj3.isDict()) {
    error(-1, "Bad CIDSystemInfo in Type 0 font descendant");
    goto err3;
  }
  obj3.dictLookup("Registry", &obj4);
  obj3.dictLookup("Ordering", &obj5);
  if (obj4.isString() && obj5.isString()) {
    if (obj4.getString()->cmp("Adobe") == 0 &&
	obj5.getString()->cmp("Japan1") == 0) {
#if JAPANESE_SUPPORT
      is16 = gTrue;
      enc16.charSet = font16AdobeJapan12;
#else
      error(-1, "Xpdf was compiled without Japanese font support");
      goto err4;
#endif
    } else if (obj4.getString()->cmp("Adobe") == 0 &&
	       obj5.getString()->cmp("GB1") == 0) {
#if CHINESE_SUPPORT
      is16 = gTrue;
      enc16.charSet = font16AdobeGB12;
#else
      error(-1, "Xpdf was compiled without Chinese font support");
      goto err4;
#endif
    } else {
      error(-1, "Uknown Type 0 character set: %s-%s",
	    obj4.getString()->getCString(), obj5.getString()->getCString());
      goto err4;
    }
  } else {
    error(-1, "Unknown Type 0 character set");
    goto err4;
  }
  obj5.free();
  obj4.free();
  obj3.free();

  // get default char width
  obj2.dictLookup("DW", &obj3);
  if (obj3.isInt())
    widths16.defWidth = obj3.getInt() * 0.001;
  else
    widths16.defWidth = 1.0;
  obj3.free();

  // get default char metrics for vertical font
  obj2.dictLookup("DW2", &obj3);
  widths16.defVY = 0.880;
  widths16.defHeight = -1;
  if (obj3.isArray() && obj3.arrayGetLength() == 2) {
    obj3.arrayGet(0, &obj4);
    if (obj4.isInt()) {
      widths16.defVY = obj4.getInt() * 0.001;
    }
    obj4.free();
    obj3.arrayGet(1, &obj4);
    if (obj4.isInt()) {
      widths16.defHeight = obj4.getInt() * 0.001;
    }
    obj4.free();
  }
  obj3.free();

  // get char width exceptions
  widths16.exceps = NULL;
  widths16.numExceps = 0;
  obj2.dictLookup("W", &obj3);
  if (obj3.isArray()) {
    excepsSize = 0;
    k = 0;
    i = 0;
    while (i+1 < obj3.arrayGetLength()) {
      obj3.arrayGet(i, &obj4);
      obj3.arrayGet(i+1, &obj5);
      if (obj4.isInt() && obj5.isInt()) {
	obj3.arrayGet(i+2, &obj6);
	if (!obj6.isNum()) {
	  error(-1, "Bad widths array in Type 0 font");
	  obj6.free();
	  obj5.free();
	  obj4.free();
	  break;
	}
	if (k == excepsSize) {
	  excepsSize += 16;
	  widths16.exceps = (GfxFontWidthExcep *)
	                grealloc(widths16.exceps,
				 excepsSize * sizeof(GfxFontWidthExcep));
	}
	widths16.exceps[k].first = obj4.getInt();
	widths16.exceps[k].last = obj5.getInt();
	widths16.exceps[k].width = obj6.getNum() * 0.001;
	obj6.free();
	++k;
	i += 3;
      } else if (obj4.isInt() && obj5.isArray()) {
	if (k + obj5.arrayGetLength() >= excepsSize) {
	  excepsSize = (k + obj5.arrayGetLength() + 15) & ~15;
	  widths16.exceps = (GfxFontWidthExcep *)
	                grealloc(widths16.exceps,
				 excepsSize * sizeof(GfxFontWidthExcep));
	}
	n = obj4.getInt();
	for (j = 0; j < obj5.arrayGetLength(); ++j) {
	  obj5.arrayGet(j, &obj6);
	  if (!obj6.isNum()) {
	    error(-1, "Bad widths array in Type 0 font");
	    obj6.free();
	    break;
	  }
	  widths16.exceps[k].first = widths16.exceps[k].last = n++;
	  widths16.exceps[k].width = obj6.getNum() * 0.001;
	  obj6.free();
	  ++k;
	}
	i += 2;
      } else {
	error(-1, "Bad widths array in Type 0 font");
	obj6.free();
	obj5.free();
	obj4.free();
	break;
      }
      obj5.free();
      obj4.free();
    }
    widths16.numExceps = k;
    if (k > 0)
      qsort(widths16.exceps, k, sizeof(GfxFontWidthExcep), &cmpWidthExcep);
  }
  obj3.free();

  // get char metric exceptions for vertical font
  widths16.excepsV = NULL;
  widths16.numExcepsV = 0;
  obj2.dictLookup("W2", &obj3);
  if (obj3.isArray()) {
    excepsSize = 0;
    k = 0;
    i = 0;
    while (i+1 < obj3.arrayGetLength()) {
      obj3.arrayGet(i, &obj4);
      obj3.arrayGet(i+1, &obj5);
      if (obj4.isInt() && obj5.isInt()) {
	obj3.arrayGet(i+2, &obj6);
	obj3.arrayGet(i+3, &obj7);
	obj3.arrayGet(i+4, &obj8);
	if (!obj6.isNum() || !obj7.isNum() || !obj8.isNum()) {
	  error(-1, "Bad widths (W2) array in Type 0 font");
	  obj8.free();
	  obj7.free();
	  obj6.free();
	  obj5.free();
	  obj4.free();
	  break;
	}
	if (k == excepsSize) {
	  excepsSize += 16;
	  widths16.excepsV = (GfxFontWidthExcepV *)
	                grealloc(widths16.excepsV,
				 excepsSize * sizeof(GfxFontWidthExcepV));
	}
	widths16.excepsV[k].first = obj4.getInt();
	widths16.excepsV[k].last = obj5.getInt();
	widths16.excepsV[k].height = obj6.getNum() * 0.001;
	widths16.excepsV[k].vx = obj7.getNum() * 0.001;
	widths16.excepsV[k].vy = obj8.getNum() * 0.001;
	obj8.free();
	obj7.free();
	obj6.free();
	++k;
	i += 5;
      } else if (obj4.isInt() && obj5.isArray()) {
	if (k + obj5.arrayGetLength() / 3 >= excepsSize) {
	  excepsSize = (k + obj5.arrayGetLength() / 3 + 15) & ~15;
	  widths16.excepsV = (GfxFontWidthExcepV *)
	                grealloc(widths16.excepsV,
				 excepsSize * sizeof(GfxFontWidthExcepV));
	}
	n = obj4.getInt();
	for (j = 0; j < obj5.arrayGetLength(); j += 3) {
	  obj5.arrayGet(j, &obj6);
	  obj5.arrayGet(j+1, &obj7);
	  obj5.arrayGet(j+1, &obj8);
	  if (!obj6.isNum() || !obj7.isNum() || !obj8.isNum()) {
	    error(-1, "Bad widths (W2) array in Type 0 font");
	    obj6.free();
	    break;
	  }
	  widths16.excepsV[k].first = widths16.exceps[k].last = n++;
	  widths16.excepsV[k].height = obj6.getNum() * 0.001;
	  widths16.excepsV[k].vx = obj7.getNum() * 0.001;
	  widths16.excepsV[k].vy = obj8.getNum() * 0.001;
	  obj8.free();
	  obj7.free();
	  obj6.free();
	  ++k;
	}
	i += 2;
      } else {
	error(-1, "Bad widths array in Type 0 font");
	obj5.free();
	obj4.free();
	break;
      }
      obj5.free();
      obj4.free();
    }
    widths16.numExcepsV = k;
    if (k > 0) {
      qsort(widths16.excepsV, k, sizeof(GfxFontWidthExcepV), &cmpWidthExcepV);
    }
  }
  obj3.free();

  obj2.free();
  obj1.free();

  // get encoding (CMap)
  fontDict->lookup("Encoding", &obj1);
  if (!obj1.isName()) {
    error(-1, "Bad encoding for Type 0 font");
    goto err1;
  }
#if JAPANESE_SUPPORT
  if (enc16.charSet == font16AdobeJapan12) {
    for (i = 0; gfxJapan12Tab[i].name; ++i) {
      if (!strcmp(obj1.getName(), gfxJapan12Tab[i].name))
	break;
    }
    if (!gfxJapan12Tab[i].name) {
      error(-1, "Unknown encoding '%s' for Adobe-Japan1-2 font",
	    obj1.getName());
      goto err1;
    }
    enc16.enc = gfxJapan12Tab[i].enc;
  }
#endif
#if CHINESE_SUPPORT
  if (enc16.charSet == font16AdobeGB12) {
    for (i = 0; gfxGB12Tab[i].name; ++i) {
      if (!strcmp(obj1.getName(), gfxGB12Tab[i].name))
	break;
    }
    if (!gfxGB12Tab[i].name) {
      error(-1, "Unknown encoding '%s' for Adobe-GB1-2 font",
	    obj1.getName());
      goto err1;
    }
    enc16.enc = gfxGB12Tab[i].enc;
  }
#endif
  obj1.free();

  return;

 err4:
  obj5.free();
  obj4.free();
 err3:
  obj3.free();
 err2:
  obj2.free();
 err1:
  obj1.free();
  //~ fix this --> add 16-bit font support to FontFile
  encoding = new FontEncoding();
  makeWidths(fontDict, NULL, NULL, 0);
}

static int CDECL cmpWidthExcep(const void *w1, const void *w2) {
  return ((GfxFontWidthExcep *)w1)->first - ((GfxFontWidthExcep *)w2)->first;
}

static int CDECL cmpWidthExcepV(const void *w1, const void *w2) {
  return ((GfxFontWidthExcepV *)w1)->first - ((GfxFontWidthExcepV *)w2)->first;
}

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

GfxFontDict::GfxFontDict(Dict *fontDict) {
  int i;
  Object obj1, obj2;

  numFonts = fontDict->getLength();
  fonts = (GfxFont **)gmalloc(numFonts * sizeof(GfxFont *));
  for (i = 0; i < numFonts; ++i) {
    fontDict->getValNF(i, &obj1);
    obj1.fetch(&obj2);
    if (obj1.isRef() && obj2.isDict()) {
      fonts[i] = new GfxFont(fontDict->getKey(i), obj1.getRef(),
			     obj2.getDict());
    } else {
      error(-1, "font resource is not a dictionary");
      fonts[i] = NULL;
    }
    obj1.free();
    obj2.free();
  }
}

GfxFontDict::~GfxFontDict() {
  int i;

  for (i = 0; i < numFonts; ++i)
    delete fonts[i];
  gfree(fonts);
}

GfxFont *GfxFontDict::lookup(char *tag) {
  int i;

  for (i = 0; i < numFonts; ++i) {
    if (fonts[i]->matches(tag))
      return fonts[i];
  }
  return NULL;
}
