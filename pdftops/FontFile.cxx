//========================================================================
//
// FontFile.cc
//
// Copyright 1999 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "gmem.h"
#include "Error.h"
#include "FontFile.h"

#include "StdFontInfo.h"
#include "CompactFontInfo.h"

//------------------------------------------------------------------------

static Guint getWord(Guchar *ptr, int size);
static double getNum(Guchar **ptr, GBool *fp);
static char *getString(int sid, Guchar *stringIdxPtr,
		       Guchar *stringStartPtr, int stringOffSize,
		       char *buf);

//------------------------------------------------------------------------

static inline char *nextLine(char *line, char *end) {
  while (line < end && *line != '\n' && *line != '\r')
    ++line;
  while (line < end && *line == '\n' || *line == '\r')
    ++line;
  return line;
}

static char hexChars[17] = "0123456789ABCDEF";

//------------------------------------------------------------------------
// FontFile
//------------------------------------------------------------------------

FontFile::FontFile() {
}

FontFile::~FontFile() {
}

//------------------------------------------------------------------------
// Type1FontFile
//------------------------------------------------------------------------

Type1FontFile::Type1FontFile(char *file, int len) {
  char *line, *line1, *p;
  char buf[256];
  int n, code, i;

  name = NULL;
  encoding = NULL;
  freeEnc = gTrue;

  for (i = 1, line = file; i <= 100 && line < file + len && !encoding; ++i) {

    // get font name
    if (!strncmp(line, "/FontName", 9)) {
      strncpy(buf, line, 255);
      buf[255] = '\0';
      if ((p = strchr(buf+9, '/')) &&
	  (p = strtok(p+1, " \t\n\r")))
	name = copyString(p);
      line = nextLine(line, file + len);

    // get encoding
    } else if (!strncmp(line, "/Encoding StandardEncoding def", 30)) {
      encoding = type1StdEncoding.copy();
    } else if (!strncmp(line, "/Encoding 256 array", 19)) {
      encoding = new FontEncoding();
      for (i = 0; i < 300; ++i) {
	line1 = nextLine(line, file + len);
	if ((n = line1 - line) > 255)
	  n = 255;
	strncpy(buf, line, n);
	buf[n] = '\0';
	p = strtok(buf, " \t");
	if (p && !strcmp(p, "dup")) {
	  if ((p = strtok(NULL, " \t"))) {
	    if ((code = atoi(p)) < 256) {
	      if ((p = strtok(NULL, " \t"))) {
		if (p[0] == '/') {
		  encoding->addChar(code, copyString(p+1));
		}
	      }
	    }
	  }
	} else {
	  if ((p = strtok(NULL, " \t\n\r")) && !strcmp(p, "def")) {
	    break;
	  }
	}
	line = line1;
      }
      //~ check for getinterval/putinterval junk

    } else {
      line = nextLine(line, file + len);
    }
  }
}

Type1FontFile::~Type1FontFile() {
  if (name)
    gfree(name);
  if (encoding && freeEnc)
    delete encoding;
}

FontEncoding *Type1FontFile::getEncoding(GBool taken) {
  if (taken)
    freeEnc = gFalse;
  return encoding;
}

//------------------------------------------------------------------------
// Type1CFontFile
//------------------------------------------------------------------------

Type1CFontFile::Type1CFontFile(char *file, int len) {
  char buf[256];
  Guchar *topPtr, *idxStartPtr, *idxPtr0, *idxPtr1;
  Guchar *stringIdxPtr, *stringStartPtr;
  int topOffSize, idxOffSize, stringOffSize;
  int nFonts, nStrings, nGlyphs;
  int nCodes, nRanges, nLeft, nSups;
  Gushort *glyphNames;
  int charset, enc, charstrings;
  int charsetFormat, encFormat;
  int c, sid;
  double op[48];
  double x;
  GBool isFP;
  int key;
  int i, j, n;

  name = NULL;
  encoding = NULL;
  freeEnc = gTrue;

  // read header
  topPtr = (Guchar *)file + (file[2] & 0xff);
  topOffSize = file[3] & 0xff;

  // read name index (first font only)
  nFonts = getWord(topPtr, 2);
  idxOffSize = topPtr[2];
  topPtr += 3;
  idxStartPtr = topPtr + (nFonts + 1) * idxOffSize - 1;
  idxPtr0 = idxStartPtr + getWord(topPtr, idxOffSize);
  idxPtr1 = idxStartPtr + getWord(topPtr + idxOffSize, idxOffSize);
  if ((n = idxPtr1 - idxPtr0) > 255)
    n = 255;
  strncpy(buf, (char *)idxPtr0, n);
  buf[n] = '\0';
  name = copyString(buf);
  topPtr = idxStartPtr + getWord(topPtr + nFonts * idxOffSize, idxOffSize);

  // read top dict index (first font only)
  nFonts = getWord(topPtr, 2);
  idxOffSize = topPtr[2];
  topPtr += 3;
  idxStartPtr = topPtr + (nFonts + 1) * idxOffSize - 1;
  idxPtr0 = idxStartPtr + getWord(topPtr, idxOffSize);
  idxPtr1 = idxStartPtr + getWord(topPtr + idxOffSize, idxOffSize);
  charset = 0;
  enc = 0;
  charstrings = 0;
  i = 0;
  while (idxPtr0 < idxPtr1) {
    if (*idxPtr0 <= 27 || *idxPtr0 == 31) {
      key = *idxPtr0++;
      if (key == 0x0c)
	key = (key << 8) | *idxPtr0++;
      if (key == 0x0f) { // charset
	charset = (int)op[0];
      } else if (key == 0x10) { // encoding
	enc = (int)op[0];
      } else if (key == 0x11) { // charstrings
	charstrings = (int)op[0];
      }
      i = 0;
    } else {
      x = getNum(&idxPtr0, &isFP);
      if (i < 48)
	op[i++] = x;
    }
  }
  topPtr = idxStartPtr + getWord(topPtr + nFonts * idxOffSize, idxOffSize);

  // read string index
  nStrings = getWord(topPtr, 2);
  stringOffSize = topPtr[2];
  topPtr += 3;
  stringIdxPtr = topPtr;
  stringStartPtr = topPtr + (nStrings + 1) * stringOffSize - 1;
  topPtr = stringStartPtr + getWord(topPtr + nStrings * stringOffSize,
				    stringOffSize);

  // get number of glyphs from charstrings index
  topPtr = (Guchar *)file + charstrings;
  nGlyphs = getWord(topPtr, 2);

  // read charset
  if (charset == 0) {
    glyphNames = type1CISOAdobeCharset;
  } else if (charset == 1) {
    glyphNames = type1CExpertCharset;
  } else if (charset == 2) {
    glyphNames = type1CExpertSubsetCharset;
  } else {
    glyphNames = (Gushort *)gmalloc(nGlyphs * sizeof(Gushort));
    glyphNames[0] = 0;
    topPtr = (Guchar *)file + charset;
    charsetFormat = *topPtr++;
    if (charsetFormat == 0) {
      for (i = 1; i < nGlyphs; ++i) {
	glyphNames[i] = getWord(topPtr, 2);
	topPtr += 2;
      }
    } else if (charsetFormat == 1) {
      i = 1;
      while (i < nGlyphs) {
	c = getWord(topPtr, 2);
	topPtr += 2;
	nLeft = *topPtr++;
	for (j = 0; j <= nLeft; ++j)
	  glyphNames[i++] = c++;
      }
    } else if (charsetFormat == 2) {
      i = 1;
      while (i < nGlyphs) {
	c = getWord(topPtr, 2);
	topPtr += 2;
	nLeft = getWord(topPtr, 2);
	topPtr += 2;
	for (j = 0; j <= nLeft; ++j)
	  glyphNames[i++] = c++;
      }
    }
  }

  // read encoding (glyph -> code mapping)
  if (enc == 0) {
    encoding = type1StdEncoding.copy();
  } else if (enc == 1) {
    encoding = type1ExpertEncoding.copy();
  } else {
    encoding = new FontEncoding();
    topPtr = (Guchar *)file + enc;
    encFormat = *topPtr++;
    if ((encFormat & 0x7f) == 0) {
      nCodes = 1 + *topPtr++;
      if (nCodes > nGlyphs) {
	nCodes = nGlyphs;
      }
      for (i = 1; i < nCodes; ++i) {
	c = *topPtr++;
	getString(glyphNames[i], stringIdxPtr, stringStartPtr,
		  stringOffSize, buf);
	encoding->addChar(c, copyString(buf));
      }
    } else if ((encFormat & 0x7f) == 1) {
      nRanges = *topPtr++;
      nCodes = 1;
      for (i = 0; i < nRanges; ++i) {
	c = *topPtr++;
	nLeft = *topPtr++;
	for (j = 0; j <= nLeft && nCodes < nGlyphs; ++j) {
	  getString(glyphNames[nCodes], stringIdxPtr, stringStartPtr,
		    stringOffSize, buf);
	  encoding->addChar(c, copyString(buf));
	  ++nCodes;
	  ++c;
	}
      }
    }
    if (encFormat & 0x80) {
      nSups = *topPtr++;
      for (i = 0; i < nSups; ++i) {
	c = *topPtr++;
	sid = getWord(topPtr, 2);
	topPtr += 2;
	getString(sid, stringIdxPtr, stringStartPtr,
		  stringOffSize, buf);
	encoding->addChar(c, copyString(buf));
      }
    }
  }

  if (charset > 2)
    gfree(glyphNames);
}

Type1CFontFile::~Type1CFontFile() {
  if (name)
    gfree(name);
  if (encoding && freeEnc)
    delete encoding;
}

FontEncoding *Type1CFontFile::getEncoding(GBool taken) {
  if (taken)
    freeEnc = gFalse;
  return encoding;
}

static Guint getWord(Guchar *ptr, int size) {
  Guint x;
  int i;

  x = 0;
  for (i = 0; i < size; ++i)
    x = (x << 8) + *ptr++;
  return x;
}

static double getNum(Guchar **ptr, GBool *fp) {
  static char nybChars[16] = "0123456789.ee -";
  int b0, b, nyb0, nyb1;
  double x;
  char buf[65];
  int i;

  x = 0;
  *fp = gFalse;
  b0 = (*ptr)[0];
  if (b0 < 28) {
    x = 0;
  } else if (b0 == 28) {
    x = ((*ptr)[1] << 8) + (*ptr)[2];
    *ptr += 3;
  } else if (b0 == 29) {
    x = ((*ptr)[1] << 24) + ((*ptr)[2] << 16) + ((*ptr)[3] << 8) + (*ptr)[4];
    *ptr += 5;
  } else if (b0 == 30) {
    *ptr += 1;
    i = 0;
    do {
      b = *(*ptr)++;
      nyb0 = b >> 4;
      nyb1 = b & 0x0f;
      if (nyb0 == 0xf)
	break;
      buf[i++] = nybChars[nyb0];
      if (i == 64)
	break;
      if (nyb0 == 0xc)
	buf[i++] = '-';
      if (i == 64)
	break;
      if (nyb1 == 0xf)
	break;
      buf[i++] = nybChars[nyb1];
      if (i == 64)
	break;
      if (nyb1 == 0xc)
	buf[i++] = '-';
    } while (i < 64);
    buf[i] = '\0';
    x = atof(buf);
    *fp = gTrue;
  } else if (b0 == 31) {
    x = 0;
  } else if (b0 < 247) {
    x = b0 - 139;
    *ptr += 1;
  } else if (b0 < 251) {
    x = ((b0 - 247) << 8) + (*ptr)[1] + 108;
    *ptr += 2;
  } else {
    x = -((b0 - 251) << 8) - (*ptr)[1] - 108;
    *ptr += 2;
  }
  return x;
}

static char *getString(int sid, Guchar *stringIdxPtr,
		       Guchar *stringStartPtr, int stringOffSize,
		       char *buf) {
  Guchar *idxPtr0, *idxPtr1;
  int len;

  if (sid < 391) {
    strcpy(buf, type1CStdStrings[sid]);
  } else {
    sid -= 391;
    idxPtr0 = stringStartPtr + getWord(stringIdxPtr + sid * stringOffSize,
				       stringOffSize);
    idxPtr1 = stringStartPtr + getWord(stringIdxPtr + (sid+1) * stringOffSize,
				       stringOffSize);
    if ((len = idxPtr1 - idxPtr0) > 255)
      len = 255;
    strncpy(buf, (char *)idxPtr0, len);
    buf[len] = '\0';
  }
  return buf;
}

//------------------------------------------------------------------------
// Type1CFontConverter
//------------------------------------------------------------------------

Type1CFontConverter::Type1CFontConverter(char *file, int len, FILE *out) {
  this->file = file;
  this->len = len;
  this->out = out;
  r1 = 55665;
  line = 0;
}

Type1CFontConverter::~Type1CFontConverter() {
}

void Type1CFontConverter::convert() {
  char *fontName;
  struct {
    int version;
    int notice;
    int copyright;
    int fullName;
    int familyName;
    int weight;
    int isFixedPitch;
    double italicAngle;
    double underlinePosition;
    double underlineThickness;
    int paintType;
    int charstringType;		//~ ???
    double fontMatrix[6];
    int uniqueID;
    double fontBBox[4];
    double strokeWidth;		//~ ???
    int charset;
    int encoding;
    int charStrings;
    int privateSize;
    int privateOffset;
  } dict;
  char buf[256], eBuf[256];
  Guchar *topPtr, *idxStartPtr, *idxPtr0, *idxPtr1;
  Guchar *stringIdxPtr, *stringStartPtr;
  int topOffSize, idxOffSize, stringOffSize;
  int nFonts, nStrings, nGlyphs;
  int nCodes, nRanges, nLeft, nSups;
  Gushort *glyphNames;
  int charsetFormat, encFormat;
  int subrsOffset, nSubrs;
  int nCharStrings;
  int c, sid;
  double x;
  GBool isFP;
  int key;
  int i, j, n;

  // read header
  topPtr = (Guchar *)file + (file[2] & 0xff);
  topOffSize = file[3] & 0xff;

  // read name (first font only)
  nFonts = getWord(topPtr, 2);
  idxOffSize = topPtr[2];
  topPtr += 3;
  idxStartPtr = topPtr + (nFonts + 1) * idxOffSize - 1;
  idxPtr0 = idxStartPtr + getWord(topPtr, idxOffSize);
  idxPtr1 = idxStartPtr + getWord(topPtr + idxOffSize, idxOffSize);
  if ((n = idxPtr1 - idxPtr0) > 255)
    n = 255;
  strncpy(buf, (char *)idxPtr0, n);
  buf[n] = '\0';
  fontName = copyString(buf);
  topPtr = idxStartPtr + getWord(topPtr + nFonts * idxOffSize, idxOffSize);

  // read top dict (first font only)
  nFonts = getWord(topPtr, 2);
  idxOffSize = topPtr[2];
  topPtr += 3;
  idxStartPtr = topPtr + (nFonts + 1) * idxOffSize - 1;
  idxPtr0 = idxStartPtr + getWord(topPtr, idxOffSize);
  idxPtr1 = idxStartPtr + getWord(topPtr + idxOffSize, idxOffSize);
  dict.version = 0;
  dict.notice = 0;
  dict.copyright = 0;
  dict.fullName = 0;
  dict.familyName = 0;
  dict.weight = 0;
  dict.isFixedPitch = 0;
  dict.italicAngle = 0;
  dict.underlinePosition = -100;
  dict.underlineThickness = 50;
  dict.paintType = 0;
  dict.charstringType = 2;
  dict.fontMatrix[0] = 0.001;
  dict.fontMatrix[1] = 0;
  dict.fontMatrix[2] = 0;
  dict.fontMatrix[3] = 0.001;
  dict.fontMatrix[4] = 0;
  dict.fontMatrix[5] = 0;
  dict.uniqueID = 0;
  dict.fontBBox[0] = 0;
  dict.fontBBox[1] = 0;
  dict.fontBBox[2] = 0;
  dict.fontBBox[3] = 0;
  dict.strokeWidth = 0;
  dict.charset = 0;
  dict.encoding = 0;
  dict.charStrings = 0;
  dict.privateSize = 0;
  dict.privateOffset = 0;
  i = 0;
  while (idxPtr0 < idxPtr1) {
    if (*idxPtr0 <= 27 || *idxPtr0 == 31) {
      key = *idxPtr0++;
      if (key == 0x0c)
	key = (key << 8) | *idxPtr0++;
      switch (key) {
      case 0x0000: dict.version = (int)op[0]; break;
      case 0x0001: dict.notice = (int)op[0]; break;
      case 0x0c00: dict.copyright = (int)op[0]; break;
      case 0x0002: dict.fullName = (int)op[0]; break;
      case 0x0003: dict.familyName = (int)op[0]; break;
      case 0x0004: dict.weight = (int)op[0]; break;
      case 0x0c01: dict.isFixedPitch = (int)op[0]; break;
      case 0x0c02: dict.italicAngle = op[0]; break;
      case 0x0c03: dict.underlinePosition = op[0]; break;
      case 0x0c04: dict.underlineThickness = op[0]; break;
      case 0x0c05: dict.paintType = (int)op[0]; break;
      case 0x0c06: dict.charstringType = (int)op[0]; break;
      case 0x0c07: dict.fontMatrix[0] = op[0];
	           dict.fontMatrix[1] = op[1];
	           dict.fontMatrix[2] = op[2];
	           dict.fontMatrix[3] = op[3];
	           dict.fontMatrix[4] = op[4];
	           dict.fontMatrix[5] = op[5]; break;
      case 0x000d: dict.uniqueID = (int)op[0]; break;
      case 0x0005: dict.fontBBox[0] = op[0];
	           dict.fontBBox[1] = op[1];
	           dict.fontBBox[2] = op[2];
	           dict.fontBBox[3] = op[3]; break;
      case 0x0c08: dict.strokeWidth = op[0]; break;
      case 0x000f: dict.charset = (int)op[0]; break;
      case 0x0010: dict.encoding = (int)op[0]; break;
      case 0x0011: dict.charStrings = (int)op[0]; break;
      case 0x0012: dict.privateSize = (int)op[0];
	           dict.privateOffset = (int)op[1]; break;
      }
      i = 0;
    } else {
      x = getNum(&idxPtr0, &isFP);
      if (i < 48) {
	op[i] = x;
	fp[i++] = isFP;
      }
    }
  }
  topPtr = idxStartPtr + getWord(topPtr + nFonts * idxOffSize, idxOffSize);

  // read string index
  nStrings = getWord(topPtr, 2);
  stringOffSize = topPtr[2];
  topPtr += 3;
  stringIdxPtr = topPtr;
  stringStartPtr = topPtr + (nStrings + 1) * stringOffSize - 1;
  topPtr = stringStartPtr + getWord(topPtr + nStrings * stringOffSize,
				    stringOffSize);

#if 1 //~
  // get global subrs
  int nGSubrs;
  int gSubrOffSize;

  nGSubrs = getWord(topPtr, 2);
  gSubrOffSize = topPtr[2];
  topPtr += 3;
#endif

  // write header and font dictionary, up to encoding
  fprintf(out, "%%!FontType1-1.0: %s", fontName);
  if (dict.version != 0) {
    fprintf(out, "%s",
	    getString(dict.version, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  fprintf(out, "\n");
  fprintf(out, "11 dict begin\n");
  fprintf(out, "/FontInfo 10 dict dup begin\n");
  if (dict.version != 0) {
    fprintf(out, "/version (%s) readonly def\n",
	    getString(dict.version, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  if (dict.notice != 0) {
    fprintf(out, "/Notice (%s) readonly def\n",
	    getString(dict.notice, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  if (dict.copyright != 0) {
    fprintf(out, "/Copyright (%s) readonly def\n",
	    getString(dict.copyright, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  if (dict.fullName != 0) {
    fprintf(out, "/FullName (%s) readonly def\n",
	    getString(dict.fullName, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  if (dict.familyName != 0) {
    fprintf(out, "/FamilyName (%s) readonly def\n",
	    getString(dict.familyName, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  if (dict.weight != 0) {
    fprintf(out, "/Weight (%s) readonly def\n",
	    getString(dict.weight, stringIdxPtr, stringStartPtr,
		      stringOffSize, buf));
  }
  fprintf(out, "/isFixedPitch %s def\n", dict.isFixedPitch ? "true" : "false");
  fprintf(out, "/ItalicAngle %g def\n", dict.italicAngle);
  fprintf(out, "/UnderlinePosition %g def\n", dict.underlinePosition);
  fprintf(out, "/UnderlineThickness %g def\n", dict.underlineThickness);
  fprintf(out, "end readonly def\n");
  fprintf(out, "/FontName /%s def\n", fontName);
  fprintf(out, "/PaintType %d def\n", dict.paintType);
  fprintf(out, "/FontType 1 def\n");
  fprintf(out, "/FontMatrix [%g %g %g %g %g %g] readonly def\n",
	  dict.fontMatrix[0], dict.fontMatrix[1], dict.fontMatrix[2],
	  dict.fontMatrix[3], dict.fontMatrix[4], dict.fontMatrix[5]);
  fprintf(out, "/FontBBox [%g %g %g %g] readonly def\n",
	  dict.fontBBox[0], dict.fontBBox[1],
	  dict.fontBBox[2], dict.fontBBox[3]);
  if (dict.uniqueID != 0) {
    fprintf(out, "/UniqueID %d def\n", dict.uniqueID);
  }

  // get number of glyphs from charstrings index
  topPtr = (Guchar *)file + dict.charStrings;
  nGlyphs = getWord(topPtr, 2);

  // read charset
  if (dict.charset == 0) {
    glyphNames = type1CISOAdobeCharset;
  } else if (dict.charset == 1) {
    glyphNames = type1CExpertCharset;
  } else if (dict.charset == 2) {
    glyphNames = type1CExpertSubsetCharset;
  } else {
    glyphNames = (Gushort *)gmalloc(nGlyphs * sizeof(Gushort));
    glyphNames[0] = 0;
    topPtr = (Guchar *)file + dict.charset;
    charsetFormat = *topPtr++;
    if (charsetFormat == 0) {
      for (i = 1; i < nGlyphs; ++i) {
	glyphNames[i] = getWord(topPtr, 2);
	topPtr += 2;
      }
    } else if (charsetFormat == 1) {
      i = 1;
      while (i < nGlyphs) {
	c = getWord(topPtr, 2);
	topPtr += 2;
	nLeft = *topPtr++;
	for (j = 0; j <= nLeft; ++j)
	  glyphNames[i++] = c++;
      }
    } else if (charsetFormat == 2) {
      i = 1;
      while (i < nGlyphs) {
	c = getWord(topPtr, 2);
	topPtr += 2;
	nLeft = getWord(topPtr, 2);
	topPtr += 2;
	for (j = 0; j <= nLeft; ++j)
	  glyphNames[i++] = c++;
      }
    }
  }

  // read encoding (glyph -> code mapping), write Type 1 encoding
  fprintf(out, "/Encoding ");
  if (dict.encoding == 0) {
    fprintf(out, "StandardEncoding def\n");
  } else {
    fprintf(out, "256 array\n");
    fprintf(out, "0 1 255 {1 index exch /.notdef put} for\n");
    if (dict.encoding == 1) {
      for (i = 0; i < 256; ++i) {
	if (type1ExpertEncodingNames[i])
	  fprintf(out, "dup %d /%s put\n", i, type1ExpertEncodingNames[i]);
      }
    } else {
      topPtr = (Guchar *)file + dict.encoding;
      encFormat = *topPtr++;
      if ((encFormat & 0x7f) == 0) {
	nCodes = 1 + *topPtr++;
	if (nCodes > nGlyphs) {
	  nCodes = nGlyphs;
	}
	for (i = 1; i < nCodes; ++i) {
	  c = *topPtr++;
	  fprintf(out, "dup %d /%s put\n", c,
		  getString(glyphNames[i], stringIdxPtr, stringStartPtr,
			    stringOffSize, buf));
	}
      } else if ((encFormat & 0x7f) == 1) {
	nRanges = *topPtr++;
	nCodes = 1;
	for (i = 0; i < nRanges; ++i) {
	  c = *topPtr++;
	  nLeft = *topPtr++;
	  for (j = 0; j <= nLeft && nCodes < nGlyphs; ++j) {
	    fprintf(out, "dup %d /%s put\n", c,
		    getString(glyphNames[nCodes], stringIdxPtr, stringStartPtr,
			      stringOffSize, buf));
	    ++nCodes;
	    ++c;
	  }
	}
      }
      if (encFormat & 0x80) {
	nSups = *topPtr++;
	for (i = 0; i < nSups; ++i) {
	  c = *topPtr++;
	  sid = getWord(topPtr, 2);
	  topPtr += 2;
	  fprintf(out, "dup %d /%s put\n", c,
		  getString(sid, stringIdxPtr, stringStartPtr,
			    stringOffSize, buf));
	}
      }
    }
    fprintf(out, "readonly def\n");
  }
  fprintf(out, "currentdict end\n");
  fprintf(out, "currentfile eexec\n");

  // get private dictionary
  eexecWrite("\x83\xca\x73\xd5");
  eexecWrite("dup /Private 32 dict dup begin\n");
  eexecWrite("/RD {string currentfile exch readstring pop} executeonly def\n");
  eexecWrite("/ND {noaccess def} executeonly def\n");
  eexecWrite("/NP {noaccess put} executeonly def\n");
  eexecWrite("/MinFeature {16 16} ND\n");
  eexecWrite("/password 5839 def\n");
  subrsOffset = 0;
  defaultWidthX = 0;
  nominalWidthX = 0;
  topPtr = (Guchar *)file + dict.privateOffset;
  idxPtr0 = topPtr;
  idxPtr1 = idxPtr0 + dict.privateSize;
  i = 0;
  while (idxPtr0 < idxPtr1) {
    if (*idxPtr0 <= 27 || *idxPtr0 == 31) {
      key = *idxPtr0++;
      if (key == 0x0c)
	key = (key << 8) | *idxPtr0++;
      switch (key) {
      case 0x0006:
	getDeltaInt(eBuf, "BlueValues", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0007:
	getDeltaInt(eBuf, "OtherBlues", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0008:
	getDeltaInt(eBuf, "FamilyBlues", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0009:
	getDeltaInt(eBuf, "FamilyOtherBlues", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0c09:
	sprintf(eBuf, "/BlueScale %g def\n", op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c0a:
	sprintf(eBuf, "/BlueShift %d def\n", (int)op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c0b:
	sprintf(eBuf, "/BlueFuzz %d def\n", (int)op[0]);
	eexecWrite(eBuf);
	break;
      case 0x000a:
	sprintf(eBuf, "/StdHW [%g] def\n", op[0]);
	eexecWrite(eBuf);
	break;
      case 0x000b:
	sprintf(eBuf, "/StdVW [%g] def\n", op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c0c:
	getDeltaReal(eBuf, "StemSnapH", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0c0d:
	getDeltaReal(eBuf, "StemSnapV", op, i);
	eexecWrite(eBuf);
	break;
      case 0x0c0e:
	sprintf(eBuf, "/ForceBold %s def\n", op[0] ? "true" : "false");
	eexecWrite(eBuf);
	break;
      case 0x0c0f:
	sprintf(eBuf, "/ForceBoldThreshold %g def\n", op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c11:
	sprintf(eBuf, "/LanguageGroup %d def\n", (int)op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c12:
	sprintf(eBuf, "/ExpansionFactor %g def\n", op[0]);
	eexecWrite(eBuf);
	break;
      case 0x0c13:
	error(-1, "Got Type 1C InitialRandomSeed");
	break;
      case 0x0013:
	subrsOffset = (int)op[0];
	break;
      case 0x0014:
	defaultWidthX = op[0];
	defaultWidthXFP = fp[0];
	break;
      case 0x0015:
	nominalWidthX = op[0];
	nominalWidthXFP = fp[0];
	break;
      default:
	error(-1, "Uknown Type 1C private dict entry %04x", key);
	break;
      }
      i = 0;
    } else {
      x = getNum(&idxPtr0, &isFP);
      if (i < 48) {
	op[i] = x;
	fp[i++] = isFP;
      }
    }
  }

  // get subrs
  if (subrsOffset != 0) {
    topPtr += subrsOffset;
    nSubrs = getWord(topPtr, 2);
    idxOffSize = topPtr[2];
    topPtr += 3;
    sprintf(eBuf, "/Subrs %d array\n", nSubrs);
    eexecWrite(eBuf);
    idxStartPtr = topPtr + (nSubrs + 1) * idxOffSize - 1;
    idxPtr1 = idxStartPtr + getWord(topPtr, idxOffSize);
    for (i = 0; i < nSubrs; ++i) {
      idxPtr0 = idxPtr1;
      idxPtr1 = idxStartPtr + getWord(topPtr + (i+1)*idxOffSize, idxOffSize);
      n = idxPtr1 - idxPtr0;
#if 1 //~
      error(-1, "Unimplemented Type 2 subrs");
#else
      sprintf(eBuf, "dup %d %d RD ", i, n);
      eexecWrite(eBuf);
      cvtGlyph(idxPtr0, n);
      eexecWrite(" NP\n");
#endif
    }
    eexecWrite("ND\n");
  }

  // get CharStrings
  topPtr = (Guchar *)file + dict.charStrings;
  nCharStrings = getWord(topPtr, 2);
  idxOffSize = topPtr[2];
  topPtr += 3;
  sprintf(eBuf, "2 index /CharStrings %d dict dup begin\n", nCharStrings);
  eexecWrite(eBuf);
  idxStartPtr = topPtr + (nCharStrings + 1) * idxOffSize - 1;
  idxPtr1 = idxStartPtr + getWord(topPtr, idxOffSize);
  for (i = 0; i < nCharStrings; ++i) {
    idxPtr0 = idxPtr1;
    idxPtr1 = idxStartPtr + getWord(topPtr + (i+1)*idxOffSize, idxOffSize);
    n = idxPtr1 - idxPtr0;
    cvtGlyph(getString(glyphNames[i], stringIdxPtr, stringStartPtr,
		       stringOffSize, buf),
	     idxPtr0, n);
  }
  eexecWrite("end\n");
  eexecWrite("end\n");
  eexecWrite("readonly put\n");
  eexecWrite("noaccess put\n");
  eexecWrite("dup /FontName get exch definefont pop\n");
  eexecWrite("mark currentfile closefile\n");

  // trailer
  if (line > 0)
    fputc('\n', out);
  for (i = 0; i < 8; ++i) {
    fprintf(out, "0000000000000000000000000000000000000000000000000000000000000000\n");
  }
  fprintf(out, "cleartomark\n");

  // clean up
  if (dict.charset > 2)
    gfree(glyphNames);
  gfree(fontName);
}

void Type1CFontConverter::eexecWrite(char *s) {
  Guchar *p;
  Guchar x;

  for (p = (Guchar *)s; *p; ++p) {
    x = *p ^ (r1 >> 8);
    r1 = (x + r1) * 52845 + 22719;
    fputc(hexChars[x >> 4], out);
    fputc(hexChars[x & 0x0f], out);
    line += 2;
    if (line == 64) {
      fputc('\n', out);
      line = 0;
    }
  }
}

void Type1CFontConverter::cvtGlyph(char *name, Guchar *s, int n) {
  int nHints;
  int x;
  GBool first = gTrue;
  char eBuf[256];
  double d, dx, dy;
  GBool dFP;
  int i, k;

  charBuf = new GString();
  charBuf->append((char)73);
  charBuf->append((char)58);
  charBuf->append((char)147);
  charBuf->append((char)134);

  i = 0;
  nOps = 0;
  nHints = 0;
  while (i < n) {
    if (s[i] == 12) {
      switch (s[i+1]) {
      case 0:			// dotsection (should be Type 1 only?)
	//~ ignored
	break;
      case 34:			// hflex
	if (nOps != 7) {
	  error(-1, "Wrong number of args (%d) to Type 2 hflex", nOps);
	}
	eexecDumpNum(op[0], fp[0]);
	eexecDumpNum(0, gFalse);
	eexecDumpNum(op[1], fp[1]);
	eexecDumpNum(op[2], fp[2]);
	eexecDumpNum(op[3], fp[3]);
	eexecDumpNum(0, gFalse);
	eexecDumpOp1(8);
	eexecDumpNum(op[4], fp[4]);
	eexecDumpNum(0, gFalse);
	eexecDumpNum(op[5], fp[5]);
	eexecDumpNum(-op[2], fp[2]);
	eexecDumpNum(op[6], fp[6]);
	eexecDumpNum(0, gFalse);
	eexecDumpOp1(8);
	break;
      case 35:			// flex
	if (nOps != 13) {
	  error(-1, "Wrong number of args (%d) to Type 2 flex", nOps);
	}
	eexecDumpNum(op[0], fp[0]);
	eexecDumpNum(op[1], fp[1]);
	eexecDumpNum(op[2], fp[2]);
	eexecDumpNum(op[3], fp[3]);
	eexecDumpNum(op[4], fp[4]);
	eexecDumpNum(op[5], fp[5]);
	eexecDumpOp1(8);
	eexecDumpNum(op[6], fp[6]);
	eexecDumpNum(op[7], fp[7]);
	eexecDumpNum(op[8], fp[8]);
	eexecDumpNum(op[9], fp[9]);
	eexecDumpNum(op[10], fp[10]);
	eexecDumpNum(op[11], fp[11]);
	eexecDumpOp1(8);
	break;
      case 36:			// hflex1
	if (nOps != 9) {
	  error(-1, "Wrong number of args (%d) to Type 2 hflex1", nOps);
	}
	eexecDumpNum(op[0], fp[0]);
	eexecDumpNum(op[1], fp[1]);
	eexecDumpNum(op[2], fp[2]);
	eexecDumpNum(op[3], fp[3]);
	eexecDumpNum(op[4], fp[4]);
	eexecDumpNum(0, gFalse);
	eexecDumpOp1(8);
	eexecDumpNum(op[5], fp[5]);
	eexecDumpNum(0, gFalse);
	eexecDumpNum(op[6], fp[6]);
	eexecDumpNum(op[7], fp[7]);
	eexecDumpNum(op[8], fp[8]);
	eexecDumpNum(-(op[1] + op[3] + op[7]), fp[1] | fp[3] | fp[7]);
	eexecDumpOp1(8);
	break;
      case 37:			// flex1
	if (nOps != 11) {
	  error(-1, "Wrong number of args (%d) to Type 2 flex1", nOps);
	}
	eexecDumpNum(op[0], fp[0]);
	eexecDumpNum(op[1], fp[1]);
	eexecDumpNum(op[2], fp[2]);
	eexecDumpNum(op[3], fp[3]);
	eexecDumpNum(op[4], fp[4]);
	eexecDumpNum(op[5], fp[5]);
	eexecDumpOp1(8);
	eexecDumpNum(op[6], fp[6]);
	eexecDumpNum(op[7], fp[7]);
	eexecDumpNum(op[8], fp[8]);
	eexecDumpNum(op[9], fp[9]);
	dx = op[0] + op[2] + op[4] + op[6] + op[8];
	dy = op[1] + op[3] + op[5] + op[7] + op[9];
	if (fabs(dx) > fabs(dy)) {
	  eexecDumpNum(op[10], fp[10]);
	  eexecDumpNum(-dy, fp[1] | fp[3] | fp[5] | fp[7] | fp[9]);
	} else {
	  eexecDumpNum(-dx, fp[0] | fp[2] | fp[4] | fp[6] | fp[8]);
	  eexecDumpNum(op[10], fp[10]);
	}
	eexecDumpOp1(8);
	break;
      case 3:			// and
      case 4:			// or
      case 5:			// not
      case 8:			// store
      case 9:			// abs
      case 10:			// add
      case 11:			// sub
      case 12:			// div
      case 13:			// load
      case 14:			// neg
      case 15:			// eq
      case 18:			// drop
      case 20:			// put
      case 21:			// get
      case 22:			// ifelse
      case 23:			// random
      case 24:			// mul
      case 26:			// sqrt
      case 27:			// dup
      case 28:			// exch
      case 29:			// index
      case 30:			// roll
	error(-1, "Unimplemented Type 2 charstring op: 12.%d", s[i+1]);
	break;
      default:
	error(-1, "Illegal Type 2 charstring op: 12.%d", s[i+1]);
	break;
      }
      i += 2;
      nOps = 0;
    } else if (s[i] == 19) {	// hintmask
      //~ ignored
      if (first) {
	cvtGlyphWidth(nOps == 1);
	first = gFalse;
      }
      if (nOps > 0) {
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 hintmask/vstemhm",
		nOps);
	}
	nHints += nOps / 2;
      }
      i += 1 + ((nHints + 7) >> 3);
      nOps = 0;
    } else if (s[i] == 20) {	// cntrmask
      //~ ignored
      if (first) {
	cvtGlyphWidth(nOps == 1);
	first = gFalse;
      }
      if (nOps > 0) {
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 cntrmask/vstemhm",
		nOps);
	}
	nHints += nOps / 2;
      }
      i += 1 + ((nHints + 7) >> 3);
      nOps = 0;
    } else if (s[i] == 28) {
      x = (s[i+1] << 8) + s[i+2];
      if (x & 0x8000)
	x |= -1 << 15;
      if (nOps < 48) {
	fp[nOps] = gFalse;
	op[nOps++] = x;
      }
      i += 3;
    } else if (s[i] <= 31) {
      switch (s[i]) {
      case 4:			// vmoveto
	if (first) {
	  cvtGlyphWidth(nOps == 2);
	  first = gFalse;
	}
	if (nOps != 1)
	  error(-1, "Wrong number of args (%d) to Type 2 vmoveto", nOps);
	eexecDumpNum(op[0], fp[0]);
	eexecDumpOp1(4);
	break;
      case 5:			// rlineto
	if (nOps < 2 || nOps % 2 != 0)
	  error(-1, "Wrong number of args (%d) to Type 2 rlineto", nOps);
	for (k = 0; k < nOps; k += 2) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(op[k+1], fp[k+1]);
	  eexecDumpOp1(5);
	}
	break;
      case 6:			// hlineto
	if (nOps < 1)
	  error(-1, "Wrong number of args (%d) to Type 2 hlineto", nOps);
	for (k = 0; k < nOps; ++k) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpOp1((k & 1) ? 7 : 6);
	}
	break;
      case 7:			// vlineto
	if (nOps < 1)
	  error(-1, "Wrong number of args (%d) to Type 2 vlineto", nOps);
	for (k = 0; k < nOps; ++k) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpOp1((k & 1) ? 6 : 7);
	}
	break;
      case 8:			// rrcurveto
	if (nOps < 6 || nOps % 6 != 0)
	  error(-1, "Wrong number of args (%d) to Type 2 rrcurveto", nOps);
	for (k = 0; k < nOps; k += 6) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(op[k+1], fp[k+1]);
	  eexecDumpNum(op[k+2], fp[k+2]);
	  eexecDumpNum(op[k+3], fp[k+3]);
	  eexecDumpNum(op[k+4], fp[k+4]);
	  eexecDumpNum(op[k+5], fp[k+5]);
	  eexecDumpOp1(8);
	}
	break;
      case 14:			// endchar / seac
	if (first) {
	  cvtGlyphWidth(nOps == 1 || nOps == 5);
	  first = gFalse;
	}
	if (nOps == 4) {
	  eexecDumpNum(0, 0);
	  eexecDumpNum(op[0], fp[0]);
	  eexecDumpNum(op[1], fp[1]);
	  eexecDumpNum(op[2], fp[2]);
	  eexecDumpNum(op[3], fp[3]);
	  eexecDumpOp2(6);
	} else if (nOps == 0) {
	  eexecDumpOp1(14);
	} else {
	  error(-1, "Wrong number of args (%d) to Type 2 endchar", nOps);
	}
	break;
      case 21:			// rmoveto
	if (first) {
	  cvtGlyphWidth(nOps == 3);
	  first = gFalse;
	}
	if (nOps != 2)
	  error(-1, "Wrong number of args (%d) to Type 2 rmoveto", nOps);
	eexecDumpNum(op[0], fp[0]);
	eexecDumpNum(op[1], fp[1]);
	eexecDumpOp1(21);
	break;
      case 22:			// hmoveto
	if (first) {
	  cvtGlyphWidth(nOps == 2);
	  first = gFalse;
	}
	if (nOps != 1)
	  error(-1, "Wrong number of args (%d) to Type 2 hmoveto", nOps);
	eexecDumpNum(op[0], fp[0]);
	eexecDumpOp1(22);
	break;
      case 24:			// rcurveline
	if (nOps < 8 || (nOps - 2) % 6 != 0)
	  error(-1, "Wrong number of args (%d) to Type 2 rcurveline", nOps);
	for (k = 0; k < nOps - 2; k += 6) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(op[k+1], fp[k+1]);
	  eexecDumpNum(op[k+2], fp[k+2]);
	  eexecDumpNum(op[k+3], fp[k+3]);
	  eexecDumpNum(op[k+4], fp[k+4]);
	  eexecDumpNum(op[k+5], fp[k+5]);
	  eexecDumpOp1(8);
	}
	eexecDumpNum(op[k], fp[k]);
	eexecDumpNum(op[k+1], fp[k]);
	eexecDumpOp1(5);
	break;
      case 25:			// rlinecurve
	if (nOps < 8 || (nOps - 6) % 2 != 0)
	  error(-1, "Wrong number of args (%d) to Type 2 rlinecurve", nOps);
	for (k = 0; k < nOps - 6; k += 2) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(op[k+1], fp[k]);
	  eexecDumpOp1(5);
	}
	eexecDumpNum(op[k], fp[k]);
	eexecDumpNum(op[k+1], fp[k+1]);
	eexecDumpNum(op[k+2], fp[k+2]);
	eexecDumpNum(op[k+3], fp[k+3]);
	eexecDumpNum(op[k+4], fp[k+4]);
	eexecDumpNum(op[k+5], fp[k+5]);
	eexecDumpOp1(8);
	break;
      case 26:			// vvcurveto
	if (nOps < 4 || !(nOps % 4 == 0 || (nOps-1) % 4 == 0))
	  error(-1, "Wrong number of args (%d) to Type 2 vvcurveto", nOps);
	if (nOps % 2 == 1) {
	  eexecDumpNum(op[0], fp[0]);
	  eexecDumpNum(op[1], fp[1]);
	  eexecDumpNum(op[2], fp[2]);
	  eexecDumpNum(op[3], fp[3]);
	  eexecDumpNum(0, gFalse);
	  eexecDumpNum(op[4], fp[4]);
	  eexecDumpOp1(8);
	  k = 5;
	} else {
	  k = 0;
	}
	for (; k < nOps; k += 4) {
	  eexecDumpNum(0, gFalse);
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(op[k+1], fp[k+1]);
	  eexecDumpNum(op[k+2], fp[k+2]);
	  eexecDumpNum(0, gFalse);
	  eexecDumpNum(op[k+3], fp[k+3]);
	  eexecDumpOp1(8);
	}
	break;
      case 27:			// hhcurveto
	if (nOps < 4 || !(nOps % 4 == 0 || (nOps-1) % 4 == 0))
	  error(-1, "Wrong number of args (%d) to Type 2 hhcurveto", nOps);
	if (nOps % 2 == 1) {
	  eexecDumpNum(op[1], fp[1]);
	  eexecDumpNum(op[0], fp[0]);
	  eexecDumpNum(op[2], fp[2]);
	  eexecDumpNum(op[3], fp[3]);
	  eexecDumpNum(op[4], fp[4]);
	  eexecDumpNum(0, gFalse);
	  eexecDumpOp1(8);
	  k = 5;
	} else {
	  k = 0;
	}
	for (; k < nOps; k += 4) {
	  eexecDumpNum(op[k], fp[k]);
	  eexecDumpNum(0, gFalse);
	  eexecDumpNum(op[k+1], fp[k+1]);
	  eexecDumpNum(op[k+2], fp[k+2]);
	  eexecDumpNum(op[k+3], fp[k+3]);
	  eexecDumpNum(0, gFalse);
	  eexecDumpOp1(8);
	}
	break;
      case 30:			// vhcurveto
	if (nOps < 4 || !(nOps % 4 == 0 || (nOps-1) % 4 == 0))
	  error(-1, "Wrong number of args (%d) to Type 2 vhcurveto", nOps);
	for (k = 0; k < nOps && k != nOps-5; k += 4) {
	  if (k % 8 == 0) {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpOp1(30);
	  } else {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpOp1(31);
	  }
	}
	if (k == nOps-5) {
	  if (k % 8 == 0) {
	    eexecDumpNum(0, gFalse);
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpNum(op[k+4], fp[k+4]);
	  } else {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(0, gFalse);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+4], fp[k+4]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	  }
	  eexecDumpOp1(8);
	}
	break;
      case 31:			// hvcurveto
	if (nOps < 4 || !(nOps % 4 == 0 || (nOps-1) % 4 == 0))
	  error(-1, "Wrong number of args (%d) to Type 2 hvcurveto", nOps);
	for (k = 0; k < nOps && k != nOps-5; k += 4) {
	  if (k % 8 == 0) {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpOp1(31);
	  } else {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpOp1(30);
	  }
	}
	if (k == nOps-5) {
	  if (k % 8 == 0) {
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(0, gFalse);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+4], fp[k+4]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	  } else {
	    eexecDumpNum(0, gFalse);
	    eexecDumpNum(op[k], fp[k]);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    eexecDumpNum(op[k+2], fp[k+2]);
	    eexecDumpNum(op[k+3], fp[k+3]);
	    eexecDumpNum(op[k+4], fp[k+4]);
	  }
	  eexecDumpOp1(8);
	}
	break;
      case 1:			// hstem
	if (first) {
	  cvtGlyphWidth(nOps & 1);
	  first = gFalse;
	}
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 hstem", nOps);
	}
	d = 0;
	dFP = gFalse;
	for (k = 0; k < nOps; k += 2) {
	  if (op[k+1] < 0) {
	    d += op[k] + op[k+1];
	    dFP |= fp[k] | fp[k+1];
	    eexecDumpNum(d, dFP);
	    eexecDumpNum(-op[k+1], fp[k+1]);
	  } else {
	    d += op[k];
	    dFP |= fp[k];
	    eexecDumpNum(d, dFP);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    d += op[k+1];
	    dFP |= fp[k+1];
	  }
	  eexecDumpOp1(1);
	}
	nHints += nOps / 2;
	break;
      case 3:			// vstem
	if (first) {
	  cvtGlyphWidth(nOps & 1);
	  first = gFalse;
	}
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 vstem", nOps);
	}
	d = 0;
	dFP = gFalse;
	for (k = 0; k < nOps; k += 2) {
	  if (op[k+1] < 0) {
	    d += op[k] + op[k+1];
	    dFP |= fp[k] | fp[k+1];
	    eexecDumpNum(d, dFP);
	    eexecDumpNum(-op[k+1], fp[k+1]);
	  } else {
	    d += op[k];
	    dFP |= fp[k];
	    eexecDumpNum(d, dFP);
	    eexecDumpNum(op[k+1], fp[k+1]);
	    d += op[k+1];
	    dFP |= fp[k+1];
	  }
	  eexecDumpOp1(3);
	}
	nHints += nOps / 2;
	break;
      case 18:			// hstemhm
	//~ ignored
	if (first) {
	  cvtGlyphWidth(nOps & 1);
	  first = gFalse;
	}
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 hstemhm", nOps);
	}
	nHints += nOps / 2;
	break;
      case 23:			// vstemhm
	//~ ignored
	if (first) {
	  cvtGlyphWidth(nOps & 1);
	  first = gFalse;
	}
	if (nOps & 1) {
	  error(-1, "Wrong number of args (%d) to Type 2 vstemhm", nOps);
	}
	nHints += nOps / 2;
	break;
      case 10:			// callsubr
      case 11:			// return
      case 16:			// blend
      case 29:			// callgsubr
	error(-1, "Unimplemented Type 2 charstring op: %d", s[i]);
	break;
      default:
	error(-1, "Illegal Type 2 charstring op: %d", s[i]);
	break;
      }
      ++i;
      nOps = 0;
    } else if (s[i] <= 246) {
      if (nOps < 48) {
	fp[nOps] = gFalse;
	op[nOps++] = (int)s[i] - 139;
      }
      ++i;
    } else if (s[i] <= 250) {
      if (nOps < 48) {
	fp[nOps] = gFalse;
	op[nOps++] = (((int)s[i] - 247) << 8) + (int)s[i+1] + 108;
      }
      i += 2;
    } else if (s[i] <= 254) {
      if (nOps < 48) {
	fp[nOps] = gFalse;
	op[nOps++] = -(((int)s[i] - 251) << 8) - (int)s[i+1] - 108;
      }
      i += 2;
    } else {
      x = (s[i+1] << 24) | (s[i+2] << 16) | (s[i+3] << 8) | s[i+4];
      if (x & 0x80000000)
	x |= -1 << 31;
      if (nOps < 48) {
	fp[nOps] = gTrue;
	op[nOps++] = (double)x / 65536.0;
      }
      i += 5;
    }
  }

  sprintf(eBuf, "/%s %d RD ", name, charBuf->getLength());
  eexecWrite(eBuf);
  eexecWriteCharstring((Guchar *)charBuf->getCString(), charBuf->getLength());
  eexecWrite(" ND\n");
  delete charBuf;
}

void Type1CFontConverter::cvtGlyphWidth(GBool useOp) {
  double w;
  GBool wFP;
  int i;

  if (useOp) {
    w = nominalWidthX + op[0];
    wFP = nominalWidthXFP | fp[0];
    for (i = 1; i < nOps; ++i) {
      op[i-1] = op[i];
      fp[i-1] = fp[i];
    }
    --nOps;
  } else {
    w = defaultWidthX;
    wFP = defaultWidthXFP;
  }
  eexecDumpNum(0, gFalse);
  eexecDumpNum(w, wFP);
  eexecDumpOp1(13);
}

void Type1CFontConverter::eexecDumpNum(double x, GBool fp) {
  Guchar buf[12];
  int y, n;

  n = 0;
  if (fp) {
    if (x >= -32768 && x < 32768) {
      y = (int)(x * 256.0);
      buf[0] = 255;
      buf[1] = (Guchar)(y >> 24);
      buf[2] = (Guchar)(y >> 16);
      buf[3] = (Guchar)(y >> 8);
      buf[4] = (Guchar)y;
      buf[5] = 255;
      buf[6] = 0;
      buf[7] = 0;
      buf[8] = 1;
      buf[9] = 0;
      buf[10] = 12;
      buf[11] = 12;
      n = 12;
    } else {
      error(-1, "Type 2 fixed point constant out of range");
    }
  } else {
    y = (int)x;
    if (y >= -107 && y <= 107) {
      buf[0] = (Guchar)(y + 139);
      n = 1;
    } else if (y > 107 && y <= 1131) {
      y -= 108;
      buf[0] = (Guchar)((y >> 8) + 247);
      buf[1] = (Guchar)(y & 0xff);
      n = 2;
    } else if (y < -107 && y >= -1131) {
      y = -y - 108;
      buf[0] = (Guchar)((y >> 8) + 251);
      buf[1] = (Guchar)(y & 0xff);
      n = 2;
    } else {
      buf[0] = 255;
      buf[1] = (Guchar)(y >> 24);
      buf[2] = (Guchar)(y >> 16);
      buf[3] = (Guchar)(y >> 8);
      buf[4] = (Guchar)y;
      n = 5;
    }
  }
  charBuf->append((char *)buf, n);
}

void Type1CFontConverter::eexecDumpOp1(int op) {
  charBuf->append((char)op);
}

void Type1CFontConverter::eexecDumpOp2(int op) {
  charBuf->append((char)12);
  charBuf->append((char)op);
}

void Type1CFontConverter::eexecWriteCharstring(Guchar *s, int n) {
  Gushort r2;
  Guchar x;
  int i;

  r2 = 4330;

  for (i = 0; i < n; ++i) {
    // charstring encryption
    x = s[i];
    x ^= (r2 >> 8);
    r2 = (x + r2) * 52845 + 22719;

    // eexec encryption
    x ^= (r1 >> 8);
    r1 = (x + r1) * 52845 + 22719;
    fputc(hexChars[x >> 4], out);
    fputc(hexChars[x & 0x0f], out);
    line += 2;
    if (line == 64) {
      fputc('\n', out);
      line = 0;
    }
  }
}

void Type1CFontConverter::getDeltaInt(char *buf, char *name, double *op,
				      int n) {
  int x, i;

  sprintf(buf, "/%s [", name);
  buf += strlen(buf);
  x = 0;
  for (i = 0; i < n; ++i) {
    x += (int)op[i];
    sprintf(buf, "%s%d", i > 0 ? " " : "", x);
    buf += strlen(buf);
  }
  sprintf(buf, "] def\n");
}

void Type1CFontConverter::getDeltaReal(char *buf, char *name, double *op,
				       int n) {
  double x;
  int i;

  sprintf(buf, "/%s [", name);
  buf += strlen(buf);
  x = 0;
  for (i = 0; i < n; ++i) {
    x += op[i];
    sprintf(buf, "%s%g", i > 0 ? " " : "", x);
    buf += strlen(buf);
  }
  sprintf(buf, "] def\n");
}
