//========================================================================
//
// FoFiTrueType.cc
//
// Copyright 1999-2004 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdlib.h>
#include "gtypes.h"
#include "gmem.h"
#include "GString.h"
#include "GHash.h"
#include "FoFiTrueType.h"

//
// Terminology
// -----------
//
// character code = number used as an element of a text string
//
// character name = glyph name = name for a particular glyph within a
//                  font
//
// glyph index = GID = position (within some internal table in the font)
//               where the instructions to draw a particular glyph are
//               stored
//
// Type 1 fonts
// ------------
//
// Type 1 fonts contain:
//
// Encoding: array of glyph names, maps char codes to glyph names
//
//           Encoding[charCode] = charName
//
// CharStrings: dictionary of instructions, keyed by character names,
//              maps character name to glyph data
//
//              CharStrings[charName] = glyphData
//
// TrueType fonts
// --------------
//
// TrueType fonts contain:
//
// 'cmap' table: mapping from character code to glyph index; there may
//               be multiple cmaps in a TrueType font
//
//               cmap[charCode] = gid
//
// 'post' table: mapping from glyph index to glyph name
//
//               post[gid] = glyphName
//
// Type 42 fonts
// -------------
//
// Type 42 fonts contain:
//
// Encoding: array of glyph names, maps char codes to glyph names
//
//           Encoding[charCode] = charName
//
// CharStrings: dictionary of glyph indexes, keyed by character names,
//              maps character name to glyph index
//
//              CharStrings[charName] = gid
//

//------------------------------------------------------------------------

#define ttcfTag 0x74746366

//------------------------------------------------------------------------

struct TrueTypeTable {
  Guint tag;
  Guint checksum;
  int offset;
  int origOffset;
  int len;
};

struct TrueTypeCmap {
  int platform;
  int encoding;
  int offset;
  int len;
  int fmt;
};

struct TrueTypeLoca {
  int idx;
  int origOffset;
  int newOffset;
  int len;
};

#define cmapTag 0x636d6170
#define glyfTag 0x676c7966
#define locaTag 0x6c6f6361
#define nameTag 0x6e616d65
#define postTag 0x706f7374

static int cmpTrueTypeLocaOffset(const void *p1, const void *p2) {
  TrueTypeLoca *loca1 = (TrueTypeLoca *)p1;
  TrueTypeLoca *loca2 = (TrueTypeLoca *)p2;

  if (loca1->origOffset == loca2->origOffset) {
    return loca1->idx - loca2->idx;
  }
  return loca1->origOffset - loca2->origOffset;
}

static int cmpTrueTypeLocaIdx(const void *p1, const void *p2) {
  TrueTypeLoca *loca1 = (TrueTypeLoca *)p1;
  TrueTypeLoca *loca2 = (TrueTypeLoca *)p2;

  return loca1->idx - loca2->idx;
}

static int cmpTrueTypeTableTag(const void *p1, const void *p2) {
  TrueTypeTable *tab1 = (TrueTypeTable *)p1;
  TrueTypeTable *tab2 = (TrueTypeTable *)p2;

  return (int)tab1->tag - (int)tab2->tag;
}

//------------------------------------------------------------------------

struct T42Table {
  char *tag;			// 4-byte tag
  GBool required;		// required by the TrueType spec?
};

// TrueType tables to be embedded in Type 42 fonts.
// NB: the table names must be in alphabetical order here.
#define nT42Tables 11
static T42Table t42Tables[nT42Tables] = {
  { "cvt ", gTrue  },
  { "fpgm", gTrue  },
  { "glyf", gTrue  },
  { "head", gTrue  },
  { "hhea", gTrue  },
  { "hmtx", gTrue  },
  { "loca", gTrue  },
  { "maxp", gTrue  },
  { "prep", gTrue  },
  { "vhea", gFalse },
  { "vmtx", gFalse }
};
#define t42HeadTable 3
#define t42LocaTable 6
#define t42GlyfTable 2

//------------------------------------------------------------------------

// Glyph names in some arbitrary standard order that Apple uses for
// their TrueType fonts.
static char *macGlyphNames[258] = {
  ".notdef",        "null",           "CR",             "space",
  "exclam",         "quotedbl",       "numbersign",     "dollar",
  "percent",        "ampersand",      "quotesingle",    "parenleft",
  "parenright",     "asterisk",       "plus",           "comma",
  "hyphen",         "period",         "slash",          "zero",
  "one",            "two",            "three",          "four",
  "five",           "six",            "seven",          "eight",
  "nine",           "colon",          "semicolon",      "less",
  "equal",          "greater",        "question",       "at",
  "A",              "B",              "C",              "D",
  "E",              "F",              "G",              "H",
  "I",              "J",              "K",              "L",
  "M",              "N",              "O",              "P",
  "Q",              "R",              "S",              "T",
  "U",              "V",              "W",              "X",
  "Y",              "Z",              "bracketleft",    "backslash",
  "bracketright",   "asciicircum",    "underscore",     "grave",
  "a",              "b",              "c",              "d",
  "e",              "f",              "g",              "h",
  "i",              "j",              "k",              "l",
  "m",              "n",              "o",              "p",
  "q",              "r",              "s",              "t",
  "u",              "v",              "w",              "x",
  "y",              "z",              "braceleft",      "bar",
  "braceright",     "asciitilde",     "Adieresis",      "Aring",
  "Ccedilla",       "Eacute",         "Ntilde",         "Odieresis",
  "Udieresis",      "aacute",         "agrave",         "acircumflex",
  "adieresis",      "atilde",         "aring",          "ccedilla",
  "eacute",         "egrave",         "ecircumflex",    "edieresis",
  "iacute",         "igrave",         "icircumflex",    "idieresis",
  "ntilde",         "oacute",         "ograve",         "ocircumflex",
  "odieresis",      "otilde",         "uacute",         "ugrave",
  "ucircumflex",    "udieresis",      "dagger",         "degree",
  "cent",           "sterling",       "section",        "bullet",
  "paragraph",      "germandbls",     "registered",     "copyright",
  "trademark",      "acute",          "dieresis",       "notequal",
  "AE",             "Oslash",         "infinity",       "plusminus",
  "lessequal",      "greaterequal",   "yen",            "mu1",
  "partialdiff",    "summation",      "product",        "pi",
  "integral",       "ordfeminine",    "ordmasculine",   "Ohm",
  "ae",             "oslash",         "questiondown",   "exclamdown",
  "logicalnot",     "radical",        "florin",         "approxequal",
  "increment",      "guillemotleft",  "guillemotright", "ellipsis",
  "nbspace",        "Agrave",         "Atilde",         "Otilde",
  "OE",             "oe",             "endash",         "emdash",
  "quotedblleft",   "quotedblright",  "quoteleft",      "quoteright",
  "divide",         "lozenge",        "ydieresis",      "Ydieresis",
  "fraction",       "currency",       "guilsinglleft",  "guilsinglright",
  "fi",             "fl",             "daggerdbl",      "periodcentered",
  "quotesinglbase", "quotedblbase",   "perthousand",    "Acircumflex",
  "Ecircumflex",    "Aacute",         "Edieresis",      "Egrave",
  "Iacute",         "Icircumflex",    "Idieresis",      "Igrave",
  "Oacute",         "Ocircumflex",    "applelogo",      "Ograve",
  "Uacute",         "Ucircumflex",    "Ugrave",         "dotlessi",
  "circumflex",     "tilde",          "overscore",      "breve",
  "dotaccent",      "ring",           "cedilla",        "hungarumlaut",
  "ogonek",         "caron",          "Lslash",         "lslash",
  "Scaron",         "scaron",         "Zcaron",         "zcaron",
  "brokenbar",      "Eth",            "eth",            "Yacute",
  "yacute",         "Thorn",          "thorn",          "minus",
  "multiply",       "onesuperior",    "twosuperior",    "threesuperior",
  "onehalf",        "onequarter",     "threequarters",  "franc",
  "Gbreve",         "gbreve",         "Idot",           "Scedilla",
  "scedilla",       "Cacute",         "cacute",         "Ccaron",
  "ccaron",         "dmacron"
};

//------------------------------------------------------------------------
// FoFiTrueType
//------------------------------------------------------------------------

FoFiTrueType *FoFiTrueType::make(char *fileA, int lenA) {
  FoFiTrueType *ff;

  ff = new FoFiTrueType(fileA, lenA, gFalse);
  if (!ff->parsedOk) {
    delete ff;
    return NULL;
  }
  return ff;
}

FoFiTrueType *FoFiTrueType::load(char *fileName) {
  FoFiTrueType *ff;
  char *fileA;
  int lenA;

  if (!(fileA = FoFiBase::readFile(fileName, &lenA))) {
    return NULL;
  }
  ff = new FoFiTrueType(fileA, lenA, gTrue);
  if (!ff->parsedOk) {
    delete ff;
    return NULL;
  }
  return ff;
}

FoFiTrueType::FoFiTrueType(char *fileA, int lenA, GBool freeFileDataA):
  FoFiBase(fileA, lenA, freeFileDataA)
{
  tables = NULL;
  nTables = 0;
  cmaps = NULL;
  nCmaps = 0;
  nameToGID = NULL;
  parsedOk = gFalse;

  parse();
}

FoFiTrueType::~FoFiTrueType() {
  gfree(tables);
  gfree(cmaps);
  if (nameToGID) {
    delete nameToGID;
  }
}

int FoFiTrueType::getNumCmaps() {
  return nCmaps;
}

int FoFiTrueType::getCmapPlatform(int i) {
  return cmaps[i].platform;
}

int FoFiTrueType::getCmapEncoding(int i) {
  return cmaps[i].encoding;
}

int FoFiTrueType::findCmap(int platform, int encoding) {
  int i;

  for (i = 0; i < nCmaps; ++i) {
    if (cmaps[i].platform == platform && cmaps[i].encoding == encoding) {
      return i;
    }
  }
  return -1;
}

Gushort FoFiTrueType::mapCodeToGID(int i, int c) {
  Gushort gid;
  int segCnt, segEnd, segStart, segDelta, segOffset;
  int cmapFirst, cmapLen;
  int pos, a, b, m;
  GBool ok;

  if (i < 0 || i >= nCmaps) {
    return 0;
  }
  ok = gTrue;
  pos = cmaps[i].offset;
  switch (cmaps[i].fmt) {
  case 0:
    if (c < 0 || c >= cmaps[i].len - 6) {
      return 0;
    }
    gid = getU8(cmaps[i].offset + 6 + c, &ok);
    break;
  case 4:
    segCnt = getU16BE(pos + 6, &ok) / 2;
    a = -1;
    b = segCnt - 1;
    segEnd = getU16BE(pos + 14 + 2*b, &ok);
    if (c > segEnd) {
      // malformed font -- the TrueType spec requires the last segEnd
      // to be 0xffff
      return 0;
    }
    // invariant: seg[a].end < code <= seg[b].end
    while (b - a > 1 && ok) {
      m = (a + b) / 2;
      segEnd = getU16BE(pos + 14 + 2*m, &ok);
      if (segEnd < c) {
	a = m;
      } else {
	b = m;
      }
    }
    segStart = getU16BE(pos + 16 + 2*segCnt + 2*b, &ok);
    segDelta = getU16BE(pos + 16 + 4*segCnt + 2*b, &ok);
    segOffset = getU16BE(pos + 16 + 6*segCnt + 2*b, &ok);
    if (c < segStart) {
      return 0;
    }
    if (segOffset == 0) {
      gid = (c + segDelta) & 0xffff;
    } else {
      gid = getU16BE(pos + 16 + 6*segCnt + 2*b +
		       segOffset + 2 * (c - segStart), &ok);
      if (gid != 0) {
	gid = (gid + segDelta) & 0xffff;
      }
    }
    break;
  case 6:
    cmapFirst = getU16BE(pos + 6, &ok);
    cmapLen = getU16BE(pos + 8, &ok);
    if (c < cmapFirst || c >= cmapFirst + cmapLen) {
      return 0;
    }
    gid = getU16BE(pos + 10 + 2 * (c - cmapFirst), &ok);
    break;
  default:
    return 0;
  }
  if (!ok) {
    return 0;
  }
  return gid;
}

int FoFiTrueType::mapNameToGID(char *name) {
  if (!nameToGID) {
    return 0;
  }
  return nameToGID->lookupInt(name);
}

int FoFiTrueType::getEmbeddingRights() {
  int i, fsType;
  GBool ok;

  if ((i = seekTable("OS/2")) < 0) {
    return 4;
  }
  ok = gTrue;
  fsType = getU16BE(tables[i].offset + 8, &ok);
  if (!ok) {
    return 4;
  }
  if (fsType & 0x0008) {
    return 2;
  }
  if (fsType & 0x0004) {
    return 1;
  }
  if (fsType & 0x0002) {
    return 0;
  }
  return 3;
}

void FoFiTrueType::convertToType42(char *psName, char **encoding,
				   Gushort *codeToGID,
				   FoFiOutputFunc outputFunc,
				   void *outputStream) {
  char buf[512];
  GBool ok;

  // write the header
  ok = gTrue;
  sprintf(buf, "%%!PS-TrueTypeFont-%g\n", (double)getS32BE(0, &ok) / 65536.0);
  (*outputFunc)(outputStream, buf, strlen(buf));

  // begin the font dictionary
  (*outputFunc)(outputStream, "10 dict begin\n", 14);
  (*outputFunc)(outputStream, "/FontName /", 11);
  (*outputFunc)(outputStream, psName, strlen(psName));
  (*outputFunc)(outputStream, " def\n", 5);
  (*outputFunc)(outputStream, "/FontType 42 def\n", 17);
  (*outputFunc)(outputStream, "/FontMatrix [1 0 0 1 0 0] def\n", 30);
  sprintf(buf, "/FontBBox [%d %d %d %d] def\n",
	  bbox[0], bbox[1], bbox[2], bbox[3]);
  (*outputFunc)(outputStream, buf, strlen(buf));
  (*outputFunc)(outputStream, "/PaintType 0 def\n", 17);

  // write the guts of the dictionary
  cvtEncoding(encoding, outputFunc, outputStream);
  cvtCharStrings(encoding, codeToGID, outputFunc, outputStream);
  cvtSfnts(outputFunc, outputStream, NULL);

  // end the dictionary and define the font
  (*outputFunc)(outputStream, "FontName currentdict end definefont pop\n", 40);
}

void FoFiTrueType::convertToCIDType2(char *psName,
				     Gushort *cidMap, int nCIDs,
				     FoFiOutputFunc outputFunc,
				     void *outputStream) {
  char buf[512];
  Gushort cid;
  GBool ok;
  int i, j, k;

  // write the header
  ok = gTrue;
  sprintf(buf, "%%!PS-TrueTypeFont-%g\n", (double)getS32BE(0, &ok) / 65536.0);
  (*outputFunc)(outputStream, buf, strlen(buf));

  // begin the font dictionary
  (*outputFunc)(outputStream, "20 dict begin\n", 14);
  (*outputFunc)(outputStream, "/CIDFontName /", 14);
  (*outputFunc)(outputStream, psName, strlen(psName));
  (*outputFunc)(outputStream, " def\n", 5);
  (*outputFunc)(outputStream, "/CIDFontType 2 def\n", 19);
  (*outputFunc)(outputStream, "/FontType 42 def\n", 17);
  (*outputFunc)(outputStream, "/CIDSystemInfo 3 dict dup begin\n", 32);
  (*outputFunc)(outputStream, "  /Registry (Adobe) def\n", 24);
  (*outputFunc)(outputStream, "  /Ordering (Identity) def\n", 27);
  (*outputFunc)(outputStream, "  /Supplement 0 def\n", 20);
  (*outputFunc)(outputStream, "  end def\n", 10);
  (*outputFunc)(outputStream, "/GDBytes 2 def\n", 15);
  if (cidMap) {
    sprintf(buf, "/CIDCount %d def\n", nCIDs);
    (*outputFunc)(outputStream, buf, strlen(buf));
    if (nCIDs > 32767) {
      (*outputFunc)(outputStream, "/CIDMap [", 9);
      for (i = 0; i < nCIDs; i += 32768 - 16) {
	(*outputFunc)(outputStream, "<\n", 2);
	for (j = 0; j < 32768 - 16 && i+j < nCIDs; j += 16) {
	  (*outputFunc)(outputStream, "  ", 2);
	  for (k = 0; k < 16 && i+j+k < nCIDs; ++k) {
	    cid = cidMap[i+j+k];
	    sprintf(buf, "%02x%02x", (cid >> 8) & 0xff, cid & 0xff);
	    (*outputFunc)(outputStream, buf, strlen(buf));
	  }
	  (*outputFunc)(outputStream, "\n", 1);
	}
	(*outputFunc)(outputStream, "  >", 3);
      }
      (*outputFunc)(outputStream, "\n", 1);
      (*outputFunc)(outputStream, "] def\n", 6);
    } else {
      (*outputFunc)(outputStream, "/CIDMap <\n", 10);
      for (i = 0; i < nCIDs; i += 16) {
	(*outputFunc)(outputStream, "  ", 2);
	for (j = 0; j < 16 && i+j < nCIDs; ++j) {
	  cid = cidMap[i+j];
	  sprintf(buf, "%02x%02x", (cid >> 8) & 0xff, cid & 0xff);
	  (*outputFunc)(outputStream, buf, strlen(buf));
	}
	(*outputFunc)(outputStream, "\n", 1);
      }
      (*outputFunc)(outputStream, "> def\n", 6);
    }
  } else {
    // direct mapping - just fill the string(s) with s[i]=i
    sprintf(buf, "/CIDCount %d def\n", nGlyphs);
    (*outputFunc)(outputStream, buf, strlen(buf));
    if (nGlyphs > 32767) {
      (*outputFunc)(outputStream, "/CIDMap [\n", 10);
      for (i = 0; i < nGlyphs; i += 32767) {
	j = nGlyphs - i < 32767 ? nGlyphs - i : 32767;
	sprintf(buf, "  %d string 0 1 %d {\n", 2 * j, j - 1);
	(*outputFunc)(outputStream, buf, strlen(buf));
	sprintf(buf, "    2 copy dup 2 mul exch %d add -8 bitshift put\n", i);
	(*outputFunc)(outputStream, buf, strlen(buf));
	sprintf(buf, "    1 index exch dup 2 mul 1 add exch %d add"
		" 255 and put\n", i);
	(*outputFunc)(outputStream, buf, strlen(buf));
	(*outputFunc)(outputStream, "  } for\n", 8);
      }
      (*outputFunc)(outputStream, "] def\n", 6);
    } else {
      sprintf(buf, "/CIDMap %d string\n", 2 * nGlyphs);
      (*outputFunc)(outputStream, buf, strlen(buf));
      sprintf(buf, "  0 1 %d {\n", nGlyphs - 1);
      (*outputFunc)(outputStream, buf, strlen(buf));
      (*outputFunc)(outputStream,
		    "    2 copy dup 2 mul exch -8 bitshift put\n", 42);
      (*outputFunc)(outputStream,
		    "    1 index exch dup 2 mul 1 add exch 255 and put\n", 50);
      (*outputFunc)(outputStream, "  } for\n", 8);
      (*outputFunc)(outputStream, "def\n", 4);
    }
  }
  (*outputFunc)(outputStream, "/FontMatrix [1 0 0 1 0 0] def\n", 30);
  sprintf(buf, "/FontBBox [%d %d %d %d] def\n",
	  bbox[0], bbox[1], bbox[2], bbox[3]);
  (*outputFunc)(outputStream, buf, strlen(buf));
  (*outputFunc)(outputStream, "/PaintType 0 def\n", 17);
  (*outputFunc)(outputStream, "/Encoding [] readonly def\n", 26);
  (*outputFunc)(outputStream, "/CharStrings 1 dict dup begin\n", 30);
  (*outputFunc)(outputStream, "  /.notdef 0 def\n", 17);
  (*outputFunc)(outputStream, "  end readonly def\n", 19);

  // write the guts of the dictionary
  cvtSfnts(outputFunc, outputStream, NULL);

  // end the dictionary and define the font
  (*outputFunc)(outputStream,
		"CIDFontName currentdict end /CIDFont defineresource pop\n",
		56);
}

void FoFiTrueType::convertToType0(char *psName, Gushort *cidMap, int nCIDs,
				  FoFiOutputFunc outputFunc,
				  void *outputStream) {
  char buf[512];
  GString *sfntsName;
  int n, i, j;

  // write the Type 42 sfnts array
  sfntsName = (new GString(psName))->append("_sfnts");
  cvtSfnts(outputFunc, outputStream, sfntsName);
  delete sfntsName;

  // write the descendant Type 42 fonts
  n = cidMap ? nCIDs : nGlyphs;
  for (i = 0; i < n; i += 256) {
    (*outputFunc)(outputStream, "10 dict begin\n", 14);
    (*outputFunc)(outputStream, "/FontName /", 11);
    (*outputFunc)(outputStream, psName, strlen(psName));
    sprintf(buf, "_%02x def\n", i >> 8);
    (*outputFunc)(outputStream, buf, strlen(buf));
    (*outputFunc)(outputStream, "/FontType 42 def\n", 17);
    (*outputFunc)(outputStream, "/FontMatrix [1 0 0 1 0 0] def\n", 30);
    sprintf(buf, "/FontBBox [%d %d %d %d] def\n",
	    bbox[0], bbox[1], bbox[2], bbox[3]);
    (*outputFunc)(outputStream, buf, strlen(buf));
    (*outputFunc)(outputStream, "/PaintType 0 def\n", 17);
    (*outputFunc)(outputStream, "/sfnts ", 7);
    (*outputFunc)(outputStream, psName, strlen(psName));
    (*outputFunc)(outputStream, "_sfnts def\n", 11);
    (*outputFunc)(outputStream, "/Encoding 256 array\n", 20);
    for (j = 0; j < 256 && i+j < n; ++j) {
      sprintf(buf, "dup %d /c%02x put\n", j, j);
      (*outputFunc)(outputStream, buf, strlen(buf));
    }
    (*outputFunc)(outputStream, "readonly def\n", 13);
    (*outputFunc)(outputStream, "/CharStrings 257 dict dup begin\n", 32);
    (*outputFunc)(outputStream, "/.notdef 0 def\n", 15);
    for (j = 0; j < 256 && i+j < n; ++j) {
      sprintf(buf, "/c%02x %d def\n", j, cidMap ? cidMap[i+j] : i+j);
      (*outputFunc)(outputStream, buf, strlen(buf));
    }
    (*outputFunc)(outputStream, "end readonly def\n", 17);
    (*outputFunc)(outputStream,
		  "FontName currentdict end definefont pop\n", 40);
  }

  // write the Type 0 parent font
  (*outputFunc)(outputStream, "16 dict begin\n", 14);
  (*outputFunc)(outputStream, "/FontName /", 11);
  (*outputFunc)(outputStream, psName, strlen(psName));
  (*outputFunc)(outputStream, " def\n", 5);
  (*outputFunc)(outputStream, "/FontType 0 def\n", 16);
  (*outputFunc)(outputStream, "/FontMatrix [1 0 0 1 0 0] def\n", 30);
  (*outputFunc)(outputStream, "/FMapType 2 def\n", 16);
  (*outputFunc)(outputStream, "/Encoding [\n", 12);
  for (i = 0; i < n; i += 256) {
    sprintf(buf, "%d\n", i >> 8);
    (*outputFunc)(outputStream, buf, strlen(buf));
  }
  (*outputFunc)(outputStream, "] def\n", 6);
  (*outputFunc)(outputStream, "/FDepVector [\n", 14);
  for (i = 0; i < n; i += 256) {
    (*outputFunc)(outputStream, "/", 1);
    (*outputFunc)(outputStream, psName, strlen(psName));
    sprintf(buf, "_%02x findfont\n", i >> 8);
    (*outputFunc)(outputStream, buf, strlen(buf));
  }
  (*outputFunc)(outputStream, "] def\n", 6);
  (*outputFunc)(outputStream, "FontName currentdict end definefont pop\n", 40);
}

void FoFiTrueType::writeTTF(FoFiOutputFunc outputFunc,
			    void *outputStream) {
  // this substitute cmap table maps char codes 0000-ffff directly to
  // glyphs 0000-ffff
  static char cmapTab[36] = {
    0, 0,			// table version number
    0, 1,			// number of encoding tables
    0, 1,			// platform ID
    0, 0,			// encoding ID
    0, 0, 0, 12,		// offset of subtable
    0, 4,			// subtable format
    0, 24,			// subtable length
    0, 0,			// subtable version
    0, 2,			// segment count * 2
    0, 2,			// 2 * 2 ^ floor(log2(segCount))
    0, 0,			// floor(log2(segCount))
    0, 0,			// 2*segCount - 2*2^floor(log2(segCount))
    0xff, 0xff,			// endCount[0]
    0, 0,			// reserved
    0, 0,			// startCount[0]
    0, 0,			// idDelta[0]
    0, 0			// pad to a mulitple of four bytes
  };
  static char nameTab[8] = {
    0, 0,			// format
    0, 0,			// number of name records
    0, 6,			// offset to start of string storage
    0, 0			// pad to multiple of four bytes
  };
  static char postTab[32] = {
    0, 1, 0, 0,			// format
    0, 0, 0, 0,			// italic angle
    0, 0,			// underline position
    0, 0,			// underline thickness
    0, 0, 0, 0,			// fixed pitch
    0, 0, 0, 0,			// min Type 42 memory
    0, 0, 0, 0,			// max Type 42 memory
    0, 0, 0, 0,			// min Type 1 memory
    0, 0, 0, 0			// max Type 1 memory
  };
  GBool missingCmap, missingName, missingPost, unsortedLoca, badCmapLen;
  int nZeroLengthTables;
  TrueTypeLoca *locaTable;
  TrueTypeTable *newTables;
  int nNewTables, cmapIdx, cmapLen, glyfLen;
  char *tableDir;
  char locaBuf[4];
  GBool ok;
  Guint t;
  int pos, i, j, k, n;

  // check for missing tables
  missingCmap = (cmapIdx = seekTable("cmap")) < 0;
  missingName = seekTable("name") < 0;
  missingPost = seekTable("post") < 0;

  // read the loca table, check to see if it's sorted
  locaTable = (TrueTypeLoca *)gmalloc((nGlyphs + 1) * sizeof(TrueTypeLoca));
  unsortedLoca = gFalse;
  i = seekTable("loca");
  pos = tables[i].offset;
  ok = gTrue;
  for (i = 0; i <= nGlyphs; ++i) {
    if (locaFmt) {
      locaTable[i].origOffset = (int)getU32BE(pos + i*4, &ok);
    } else {
      locaTable[i].origOffset = 2 * getU16BE(pos + i*2, &ok);
    }
    if (i > 0 && locaTable[i].origOffset < locaTable[i-1].origOffset) {
      unsortedLoca = gTrue;
    }
    locaTable[i].idx = i;
  }

  // check for zero-length tables
  nZeroLengthTables = 0;
  for (i = 0; i < nTables; ++i) {
    if (tables[i].len == 0) {
      ++nZeroLengthTables;
    }
  }

  // check for an incorrect cmap table length
  badCmapLen = gFalse;
  cmapLen = 0; // make gcc happy
  if (!missingCmap) {
    cmapLen = cmaps[0].offset + cmaps[0].len;
    for (i = 1; i < nCmaps; ++i) {
      if (cmaps[i].offset + cmaps[i].len > cmapLen) {
	cmapLen = cmaps[i].offset + cmaps[i].len;
      }
    }
    cmapLen -= tables[cmapIdx].offset;
    if (cmapLen > tables[cmapIdx].len) {
      badCmapLen = gTrue;
    }
  }

  // if nothing is broken, just write the TTF file as is
  if (!missingCmap && !missingName && !missingPost && !unsortedLoca &&
      !badCmapLen && nZeroLengthTables == 0) {
    (*outputFunc)(outputStream, (char *)file, len);
    goto done1;
  }

  // sort the 'loca' table: some (non-compliant) fonts have
  // out-of-order loca tables; in order to correctly handle the case
  // where (compliant) fonts have empty entries in the middle of the
  // table, cmpTrueTypeLocaOffset uses offset as its primary sort key,
  // and idx as its secondary key (ensuring that adjacent entries with
  // the same pos value remain in the same order)
  glyfLen = 0; // make gcc happy
  if (unsortedLoca) {
    qsort(locaTable, nGlyphs + 1, sizeof(TrueTypeLoca),
	  &cmpTrueTypeLocaOffset);
    for (i = 0; i < nGlyphs; ++i) {
      locaTable[i].len = locaTable[i+1].origOffset - locaTable[i].origOffset;
    }
    locaTable[nGlyphs].len = 0;
    qsort(locaTable, nGlyphs + 1, sizeof(TrueTypeLoca),
	  &cmpTrueTypeLocaIdx);
    pos = 0;
    for (i = 0; i <= nGlyphs; ++i) {
      locaTable[i].newOffset = pos;
      pos += locaTable[i].len;
      if (pos & 3) {
	pos += 4 - (pos & 3);
      }
    }
    glyfLen = pos;
  }

  // construct the new table directory:
  // - keep all original tables with non-zero length
  // - fix the cmap table's length, if necessary
  // - add missing tables
  // - sort the table by tag
  // - compute new table positions, including 4-byte alignment
  nNewTables = nTables - nZeroLengthTables +
               (missingCmap ? 1 : 0) + (missingName ? 1 : 0) +
               (missingPost ? 1 : 0);
  newTables = (TrueTypeTable *)gmalloc(nNewTables * sizeof(TrueTypeTable));
  j = 0;
  for (i = 0; i < nTables; ++i) {
    if (tables[i].len > 0) {
      newTables[j] = tables[i];
      newTables[j].origOffset = tables[i].offset;
      if (newTables[j].tag == cmapTag && badCmapLen) {
	newTables[j].len = cmapLen;
      } else if (newTables[j].tag == locaTag && unsortedLoca) {
	newTables[j].len = (nGlyphs + 1) * (locaFmt ? 4 : 2);
      } else if (newTables[j].tag == glyfTag && unsortedLoca) {
	newTables[j].len = glyfLen;
      }
      ++j;
    }
  }
  if (missingCmap) {
    newTables[j].tag = cmapTag;
    newTables[j].checksum = 0; //~ should compute the checksum
    newTables[j].len = sizeof(cmapTab);
    ++j;
  }
  if (missingName) {
    newTables[j].tag = nameTag;
    newTables[j].checksum = 0; //~ should compute the checksum
    newTables[j].len = sizeof(nameTab);
    ++j;
  }
  if (missingPost) {
    newTables[j].tag = postTag;
    newTables[j].checksum = 0; //~ should compute the checksum
    newTables[j].len = sizeof(postTab);
    ++j;
  }
  qsort(newTables, nNewTables, sizeof(TrueTypeTable),
	&cmpTrueTypeTableTag);
  pos = 12 + nNewTables * 16;
  for (i = 0; i < nNewTables; ++i) {
    newTables[i].offset = pos;
    pos += newTables[i].len;
    if (pos & 3) {
      pos += 4 - (pos & 3);
    }
  }

  // write the table directory
  tableDir = (char *)gmalloc(12 + nNewTables * 16);
  tableDir[0] = 0x00;					// sfnt version
  tableDir[1] = 0x01;
  tableDir[2] = 0x00;
  tableDir[3] = 0x00;
  tableDir[4] = (char)((nNewTables >> 8) & 0xff);	// numTables
  tableDir[5] = (char)(nNewTables & 0xff);
  for (i = -1, t = (Guint)nNewTables; t; ++i, t >>= 1) ;
  t = 1 << (4 + i);
  tableDir[6] = (char)((t >> 8) & 0xff);		// searchRange
  tableDir[7] = (char)(t & 0xff);
  tableDir[8] = (char)((i >> 8) & 0xff);		// entrySelector
  tableDir[9] = (char)(i & 0xff);
  t = nNewTables * 16 - t;
  tableDir[10] = (char)((t >> 8) & 0xff);		// rangeShift
  tableDir[11] = (char)(t & 0xff);
  pos = 12;
  for (i = 0; i < nNewTables; ++i) {
    tableDir[pos   ] = (char)(newTables[i].tag >> 24);
    tableDir[pos+ 1] = (char)(newTables[i].tag >> 16);
    tableDir[pos+ 2] = (char)(newTables[i].tag >>  8);
    tableDir[pos+ 3] = (char) newTables[i].tag;
    tableDir[pos+ 4] = (char)(newTables[i].checksum >> 24);
    tableDir[pos+ 5] = (char)(newTables[i].checksum >> 16);
    tableDir[pos+ 6] = (char)(newTables[i].checksum >>  8);
    tableDir[pos+ 7] = (char) newTables[i].checksum;
    tableDir[pos+ 8] = (char)(newTables[i].offset >> 24);
    tableDir[pos+ 9] = (char)(newTables[i].offset >> 16);
    tableDir[pos+10] = (char)(newTables[i].offset >>  8);
    tableDir[pos+11] = (char) newTables[i].offset;
    tableDir[pos+12] = (char)(newTables[i].len >> 24);
    tableDir[pos+13] = (char)(newTables[i].len >> 16);
    tableDir[pos+14] = (char)(newTables[i].len >>  8);
    tableDir[pos+15] = (char) newTables[i].len;
    pos += 16;
  }
  (*outputFunc)(outputStream, tableDir, 12 + nNewTables * 16);

  // write the tables
  for (i = 0; i < nNewTables; ++i) {
    if (newTables[i].tag == cmapTag && missingCmap) {
      (*outputFunc)(outputStream, cmapTab, newTables[i].len);
    } else if (newTables[i].tag == nameTag && missingName) {
      (*outputFunc)(outputStream, nameTab, newTables[i].len);
    } else if (newTables[i].tag == postTag && missingPost) {
      (*outputFunc)(outputStream, postTab, newTables[i].len);
    } else if (newTables[i].tag == locaTag && unsortedLoca) {
      for (j = 0; j <= nGlyphs; ++j) {
	if (locaFmt) {
	  locaBuf[0] = (char)(locaTable[j].newOffset >> 24);
	  locaBuf[1] = (char)(locaTable[j].newOffset >> 16);
	  locaBuf[2] = (char)(locaTable[j].newOffset >>  8);
	  locaBuf[3] = (char) locaTable[j].newOffset;
	  (*outputFunc)(outputStream, locaBuf, 4);
	} else {
	  locaBuf[0] = (char)(locaTable[j].newOffset >> 9);
	  locaBuf[1] = (char)(locaTable[j].newOffset >> 1);
	  (*outputFunc)(outputStream, locaBuf, 2);
	}
      }
    } else if (newTables[i].tag == glyfTag && unsortedLoca) {
      pos = tables[seekTable("glyf")].offset;
      for (j = 0; j < nGlyphs; ++j) {
	n = locaTable[j].len;
	if (n > 0) {
	  k = locaTable[j].origOffset;
	  if (checkRegion(pos + k, n)) {
	    (*outputFunc)(outputStream, (char *)file + pos + k, n);
	  } else {
	    for (k = 0; k < n; ++k) {
	      (*outputFunc)(outputStream, "\0", 1);
	    }
	  }
	  if ((k = locaTable[j].len & 3)) {
	    (*outputFunc)(outputStream, "\0\0\0\0", 4 - k);
	  }
	}
      }
    } else {
      if (checkRegion(newTables[i].origOffset, newTables[i].len)) {
	(*outputFunc)(outputStream, (char *)file + newTables[i].origOffset,
		      newTables[i].len);
      } else {
	for (j = 0; j < newTables[i].len; ++j) {
	  (*outputFunc)(outputStream, "\0", 1);
	}
      }
    }
    if (newTables[i].len & 3) {
      (*outputFunc)(outputStream, "\0\0\0", 4 - (newTables[i].len & 3));
    }
  }

  gfree(tableDir);
  gfree(newTables);
 done1:
  gfree(locaTable);
}

void FoFiTrueType::cvtEncoding(char **encoding,
			       FoFiOutputFunc outputFunc,
			       void *outputStream) {
  char *name;
  char buf[64];
  int i;

  (*outputFunc)(outputStream, "/Encoding 256 array\n", 20);
  if (encoding) {
    for (i = 0; i < 256; ++i) {
      if (!(name = encoding[i])) {
	name = ".notdef";
      }
      sprintf(buf, "dup %d /", i);
      (*outputFunc)(outputStream, buf, strlen(buf));
      (*outputFunc)(outputStream, name, strlen(name));
      (*outputFunc)(outputStream, " put\n", 5);
    }
  } else {
    for (i = 0; i < 256; ++i) {
      sprintf(buf, "dup %d /c%02x put\n", i, i);
      (*outputFunc)(outputStream, buf, strlen(buf));
    }
  }
  (*outputFunc)(outputStream, "readonly def\n", 13);
}

void FoFiTrueType::cvtCharStrings(char **encoding,
				  Gushort *codeToGID,
				  FoFiOutputFunc outputFunc,
				  void *outputStream) {
  char *name;
  char buf[64], buf2[16];
  int i, k;

  // always define '.notdef'
  (*outputFunc)(outputStream, "/CharStrings 256 dict dup begin\n", 32);
  (*outputFunc)(outputStream, "/.notdef 0 def\n", 15);

  // if there's no 'cmap' table, punt
  if (nCmaps == 0) {
    goto err;
  }

  // map char name to glyph index:
  // 1. use encoding to map name to char code
  // 2. use codeToGID to map char code to glyph index
  // N.B. We do this in reverse order because font subsets can have
  //      weird encodings that use the same character name twice, and
  //      the first definition is probably the one we want.
  k = 0; // make gcc happy
  for (i = 255; i >= 0; --i) {
    if (encoding) {
      name = encoding[i];
    } else {
      sprintf(buf2, "c%02x", i);
      name = buf2;
    }
    if (name && strcmp(name, ".notdef")) {
      k = codeToGID[i];
      // note: Distiller (maybe Adobe's PS interpreter in general)
      // doesn't like TrueType fonts that have CharStrings entries
      // which point to nonexistent glyphs, hence the (k < nGlyphs)
      // test
      if (k > 0 && k < nGlyphs) {
	(*outputFunc)(outputStream, "/", 1);
	(*outputFunc)(outputStream, name, strlen(name));
	sprintf(buf, " %d def\n", k);
	(*outputFunc)(outputStream, buf, strlen(buf));
      }
    }
  }

 err:
  (*outputFunc)(outputStream, "end readonly def\n", 17);
}

void FoFiTrueType::cvtSfnts(FoFiOutputFunc outputFunc,
			    void *outputStream, GString *name) {
  Guchar headData[54];
  TrueTypeLoca *locaTable;
  Guchar *locaData;
  TrueTypeTable newTables[nT42Tables];
  Guchar tableDir[12 + nT42Tables*16];
  GBool ok;
  Guint checksum;
  int nNewTables;
  int length, pos, glyfPos, i, j, k;

  // construct the 'head' table, zero out the font checksum
  i = seekTable("head");
  pos = tables[i].offset;
  if (!checkRegion(pos, 54)) {
    return;
  }
  memcpy(headData, file + pos, 54);
  headData[8] = headData[9] = headData[10] = headData[11] = (Guchar)0;

  // read the original 'loca' table, pad entries out to 4 bytes, and
  // sort it into proper order -- some (non-compliant) fonts have
  // out-of-order loca tables; in order to correctly handle the case
  // where (compliant) fonts have empty entries in the middle of the
  // table, cmpTrueTypeLocaPos uses offset as its primary sort key,
  // and idx as its secondary key (ensuring that adjacent entries with
  // the same pos value remain in the same order)
  locaTable = (TrueTypeLoca *)gmalloc((nGlyphs + 1) * sizeof(TrueTypeLoca));
  i = seekTable("loca");
  pos = tables[i].offset;
  ok = gTrue;
  for (i = 0; i <= nGlyphs; ++i) {
    locaTable[i].idx = i;
    if (locaFmt) {
      locaTable[i].origOffset = (int)getU32BE(pos + i*4, &ok);
    } else {
      locaTable[i].origOffset = 2 * getU16BE(pos + i*2, &ok);
    }
  }
  qsort(locaTable, nGlyphs + 1, sizeof(TrueTypeLoca),
	&cmpTrueTypeLocaOffset);
  for (i = 0; i < nGlyphs; ++i) {
    locaTable[i].len = locaTable[i+1].origOffset - locaTable[i].origOffset;
  }
  locaTable[nGlyphs].len = 0;
  qsort(locaTable, nGlyphs + 1, sizeof(TrueTypeLoca),
	&cmpTrueTypeLocaIdx);
  pos = 0;
  for (i = 0; i <= nGlyphs; ++i) {
    locaTable[i].newOffset = pos;
    pos += locaTable[i].len;
    if (pos & 3) {
      pos += 4 - (pos & 3);
    }
  }

  // construct the new 'loca' table
  locaData = (Guchar *)gmalloc((nGlyphs + 1) * (locaFmt ? 4 : 2));
  for (i = 0; i <= nGlyphs; ++i) {
    pos = locaTable[i].newOffset;
    if (locaFmt) {
      locaData[4*i  ] = (Guchar)(pos >> 24);
      locaData[4*i+1] = (Guchar)(pos >> 16);
      locaData[4*i+2] = (Guchar)(pos >>  8);
      locaData[4*i+3] = (Guchar) pos;
    } else {
      locaData[2*i  ] = (Guchar)(pos >> 9);
      locaData[2*i+1] = (Guchar)(pos >> 1);
    }
  }

  // count the number of tables
  nNewTables = 0;
  for (i = 0; i < nT42Tables; ++i) {
    if (t42Tables[i].required ||
	seekTable(t42Tables[i].tag) >= 0) {
      ++nNewTables;
    }
  }

  // construct the new table headers, including table checksums
  // (pad each table out to a multiple of 4 bytes)
  pos = 12 + nNewTables*16;
  k = 0;
  for (i = 0; i < nT42Tables; ++i) {
    length = -1;
    checksum = 0; // make gcc happy
    if (i == t42HeadTable) {
      length = 54;
      checksum = computeTableChecksum(headData, 54);
    } else if (i == t42LocaTable) {
      length = (nGlyphs + 1) * (locaFmt ? 4 : 2);
      checksum = computeTableChecksum(locaData, length);
    } else if (i == t42GlyfTable) {
      length = 0;
      checksum = 0;
      glyfPos = tables[seekTable("glyf")].offset;
      for (j = 0; j < nGlyphs; ++j) {
	length += locaTable[j].len;
	if (length & 3) {
	  length += 4 - (length & 3);
	}
	if (checkRegion(glyfPos + locaTable[j].origOffset, locaTable[j].len)) {
	  checksum +=
	      computeTableChecksum(file + glyfPos + locaTable[j].origOffset,
				   locaTable[j].len);
	}
      }
    } else {
      if ((j = seekTable(t42Tables[i].tag)) >= 0) {
	length = tables[j].len;
	if (checkRegion(tables[j].offset, length)) {
	  checksum = computeTableChecksum(file + tables[j].offset, length);
	}
      } else if (t42Tables[i].required) {
	//~ error(-1, "Embedded TrueType font is missing a required table ('%s')",
	//~       t42Tables[i].tag);
	length = 0;
	checksum = 0;
      }
    }
    if (length >= 0) {
      newTables[k].tag = ((t42Tables[i].tag[0] & 0xff) << 24) |
	                 ((t42Tables[i].tag[1] & 0xff) << 16) |
	                 ((t42Tables[i].tag[2] & 0xff) <<  8) |
	                  (t42Tables[i].tag[3] & 0xff);
      newTables[k].checksum = checksum;
      newTables[k].offset = pos;
      newTables[k].len = length;
      pos += length;
      if (pos & 3) {
	pos += 4 - (length & 3);
      }
      ++k;
    }
  }

  // construct the table directory
  tableDir[0] = 0x00;		// sfnt version
  tableDir[1] = 0x01;
  tableDir[2] = 0x00;
  tableDir[3] = 0x00;
  tableDir[4] = 0;		// numTables
  tableDir[5] = nNewTables;
  tableDir[6] = 0;		// searchRange
  tableDir[7] = (Guchar)128;
  tableDir[8] = 0;		// entrySelector
  tableDir[9] = 3;
  tableDir[10] = 0;		// rangeShift
  tableDir[11] = (Guchar)(16 * nNewTables - 128);
  pos = 12;
  for (i = 0; i < nNewTables; ++i) {
    tableDir[pos   ] = (Guchar)(newTables[i].tag >> 24);
    tableDir[pos+ 1] = (Guchar)(newTables[i].tag >> 16);
    tableDir[pos+ 2] = (Guchar)(newTables[i].tag >>  8);
    tableDir[pos+ 3] = (Guchar) newTables[i].tag;
    tableDir[pos+ 4] = (Guchar)(newTables[i].checksum >> 24);
    tableDir[pos+ 5] = (Guchar)(newTables[i].checksum >> 16);
    tableDir[pos+ 6] = (Guchar)(newTables[i].checksum >>  8);
    tableDir[pos+ 7] = (Guchar) newTables[i].checksum;
    tableDir[pos+ 8] = (Guchar)(newTables[i].offset >> 24);
    tableDir[pos+ 9] = (Guchar)(newTables[i].offset >> 16);
    tableDir[pos+10] = (Guchar)(newTables[i].offset >>  8);
    tableDir[pos+11] = (Guchar) newTables[i].offset;
    tableDir[pos+12] = (Guchar)(newTables[i].len >> 24);
    tableDir[pos+13] = (Guchar)(newTables[i].len >> 16);
    tableDir[pos+14] = (Guchar)(newTables[i].len >>  8);
    tableDir[pos+15] = (Guchar) newTables[i].len;
    pos += 16;
  }

  // compute the font checksum and store it in the head table
  checksum = computeTableChecksum(tableDir, 12 + nNewTables*16);
  for (i = 0; i < nNewTables; ++i) {
    checksum += newTables[i].checksum;
  }
  checksum = 0xb1b0afba - checksum; // because the TrueType spec says so
  headData[ 8] = (Guchar)(checksum >> 24);
  headData[ 9] = (Guchar)(checksum >> 16);
  headData[10] = (Guchar)(checksum >>  8);
  headData[11] = (Guchar) checksum;

  // start the sfnts array
  if (name) {
    (*outputFunc)(outputStream, "/", 1);
    (*outputFunc)(outputStream, name->getCString(), name->getLength());
    (*outputFunc)(outputStream, " [\n", 3);
  } else {
    (*outputFunc)(outputStream, "/sfnts [\n", 9);
  }

  // write the table directory
  dumpString(tableDir, 12 + nNewTables*16, outputFunc, outputStream);

  // write the tables
  for (i = 0; i < nNewTables; ++i) {
    if (i == t42HeadTable) {
      dumpString(headData, 54, outputFunc, outputStream);
    } else if (i == t42LocaTable) {
      length = (nGlyphs + 1) * (locaFmt ? 4 : 2);
      dumpString(locaData, length, outputFunc, outputStream);
    } else if (i == t42GlyfTable) {
      glyfPos = tables[seekTable("glyf")].offset;
      for (j = 0; j < nGlyphs; ++j) {
	if (locaTable[j].len > 0 &&
	    checkRegion(glyfPos + locaTable[j].origOffset, locaTable[j].len)) {
	  dumpString(file + glyfPos + locaTable[j].origOffset,
		     locaTable[j].len, outputFunc, outputStream);
	}
      }
    } else {
      // length == 0 means the table is missing and the error was
      // already reported during the construction of the table
      // headers
      if ((length = newTables[i].len) > 0) {
	if ((j = seekTable(t42Tables[i].tag)) >= 0 &&
	    checkRegion(tables[j].offset, tables[j].len)) {
	  dumpString(file + tables[j].offset, tables[j].len,
		     outputFunc, outputStream);
	}
      }
    }
  }

  // end the sfnts array
  (*outputFunc)(outputStream, "] def\n", 6);

  gfree(locaData);
  gfree(locaTable);
}

void FoFiTrueType::dumpString(Guchar *s, int length,
			      FoFiOutputFunc outputFunc,
			      void *outputStream) {
  char buf[64];
  int pad, i, j;

  (*outputFunc)(outputStream, "<", 1);
  for (i = 0; i < length; i += 32) {
    for (j = 0; j < 32 && i+j < length; ++j) {
      sprintf(buf, "%02X", s[i+j] & 0xff);
      (*outputFunc)(outputStream, buf, strlen(buf));
    }
    if (i % (65536 - 32) == 65536 - 64) {
      (*outputFunc)(outputStream, ">\n<", 3);
    } else if (i+32 < length) {
      (*outputFunc)(outputStream, "\n", 1);
    }
  }
  if (length & 3) {
    pad = 4 - (length & 3);
    for (i = 0; i < pad; ++i) {
      (*outputFunc)(outputStream, "00", 2);
    }
  }
  // add an extra zero byte because the Adobe Type 42 spec says so
  (*outputFunc)(outputStream, "00>\n", 4);
}

Guint FoFiTrueType::computeTableChecksum(Guchar *data, int length) {
  Guint checksum, word;
  int i;

  checksum = 0;
  for (i = 0; i+3 < length; i += 4) {
    word = ((data[i  ] & 0xff) << 24) +
           ((data[i+1] & 0xff) << 16) +
           ((data[i+2] & 0xff) <<  8) +
            (data[i+3] & 0xff);
    checksum += word;
  }
  if (length & 3) {
    word = 0;
    i = length & ~3;
    switch (length & 3) {
    case 3:
      word |= (data[i+2] & 0xff) <<  8;
    case 2:
      word |= (data[i+1] & 0xff) << 16;
    case 1:
      word |= (data[i  ] & 0xff) << 24;
      break;
    }
    checksum += word;
  }
  return checksum;
}

void FoFiTrueType::parse() {
  Guint topTag;
  int pos, i, j;

  parsedOk = gTrue;

  // look for a collection (TTC)
  topTag = getU32BE(0, &parsedOk);
  if (!parsedOk) {
    return;
  }
  if (topTag == ttcfTag) {
    pos = getU32BE(12, &parsedOk);
    if (!parsedOk) {
      return;
    }
  } else {
    pos = 0;
  }

  // read the table directory
  nTables = getU16BE(pos + 4, &parsedOk);
  if (!parsedOk) {
    return;
  }
  tables = (TrueTypeTable *)gmalloc(nTables * sizeof(TrueTypeTable));
  pos += 12;
  for (i = 0; i < nTables; ++i) {
    tables[i].tag = getU32BE(pos, &parsedOk);
    tables[i].checksum = getU32BE(pos + 4, &parsedOk);
    tables[i].offset = (int)getU32BE(pos + 8, &parsedOk);
    tables[i].len = (int)getU32BE(pos + 12, &parsedOk);
    if (tables[i].offset + tables[i].len < tables[i].offset ||
	tables[i].offset + tables[i].len > len) {
      parsedOk = gFalse;
    }
    pos += 16;
  }
  if (!parsedOk) {
    return;
  }

  // check for tables that are required by both the TrueType spec and
  // the Type 42 spec
  if (seekTable("head") < 0 ||
      seekTable("hhea") < 0 ||
      seekTable("loca") < 0 ||
      seekTable("maxp") < 0 ||
      seekTable("glyf") < 0 ||
      seekTable("hmtx") < 0) {
    parsedOk = gFalse;
    return;
  }

  // read the cmaps
  if ((i = seekTable("cmap")) >= 0) {
    pos = tables[i].offset + 2;
    nCmaps = getU16BE(pos, &parsedOk);
    pos += 2;
    if (!parsedOk) {
      return;
    }
    cmaps = (TrueTypeCmap *)gmalloc(nCmaps * sizeof(TrueTypeCmap));
    for (j = 0; j < nCmaps; ++j) {
      cmaps[j].platform = getU16BE(pos, &parsedOk);
      cmaps[j].encoding = getU16BE(pos + 2, &parsedOk);
      cmaps[j].offset = tables[i].offset + getU32BE(pos + 4, &parsedOk);
      pos += 8;
      cmaps[j].fmt = getU16BE(cmaps[j].offset, &parsedOk);
      cmaps[j].len = getU16BE(cmaps[j].offset + 2, &parsedOk);
    }
    if (!parsedOk) {
      return;
    }
  } else {
    nCmaps = 0;
  }

  // get the number of glyphs from the maxp table
  i = seekTable("maxp");
  nGlyphs = getU16BE(tables[i].offset + 4, &parsedOk);
  if (!parsedOk) {
    return;
  }

  // get the bbox and loca table format from the head table
  i = seekTable("head");
  bbox[0] = getS16BE(tables[i].offset + 36, &parsedOk);
  bbox[1] = getS16BE(tables[i].offset + 38, &parsedOk);
  bbox[2] = getS16BE(tables[i].offset + 40, &parsedOk);
  bbox[3] = getS16BE(tables[i].offset + 42, &parsedOk);
  locaFmt = getS16BE(tables[i].offset + 50, &parsedOk);
  if (!parsedOk) {
    return;
  }

  // read the post table
  readPostTable();
}

void FoFiTrueType::readPostTable() {
  GString *name;
  int tablePos, postFmt, stringIdx, stringPos;
  GBool ok;
  int i, j, n, m;

  ok = gTrue;
  if ((i = seekTable("post")) < 0) {
    return;
  }
  tablePos = tables[i].offset;
  postFmt = getU32BE(tablePos, &ok);
  if (!ok) {
    goto err;
  }
  if (postFmt == 0x00010000) {
    nameToGID = new GHash(gTrue);
    for (i = 0; i < 258; ++i) {
      nameToGID->add(new GString(macGlyphNames[i]), i);
    }
  } else if (postFmt == 0x00020000) {
    nameToGID = new GHash(gTrue);
    n = getU16BE(tablePos + 32, &ok);
    if (!ok) {
      goto err;
    }
    if (n > nGlyphs) {
      n = nGlyphs;
    }
    stringIdx = 0;
    stringPos = tablePos + 34 + 2*n;
    for (i = 0; i < n; ++i) {
      j = getU16BE(tablePos + 34 + 2*i, &ok);
      if (j < 258) {
	nameToGID->removeInt(macGlyphNames[j]);
	nameToGID->add(new GString(macGlyphNames[j]), i);
      } else {
	j -= 258;
	if (j != stringIdx) {
	  for (stringIdx = 0, stringPos = tablePos + 34 + 2*n;
	       stringIdx < j;
	       ++stringIdx, stringPos += 1 + getU8(stringPos, &ok)) ;
	  if (!ok) {
	    goto err;
	  }
	}
	m = getU8(stringPos, &ok);
	if (!ok || !checkRegion(stringPos + 1, m)) {
	  goto err;
	}
	name = new GString((char *)&file[stringPos + 1], m);
	nameToGID->removeInt(name);
	nameToGID->add(name, i);
	++stringIdx;
	stringPos += 1 + m;
      }
    }
  } else if (postFmt == 0x00028000) {
    nameToGID = new GHash(gTrue);
    for (i = 0; i < nGlyphs; ++i) {
      j = getU8(tablePos + 32 + i, &ok);
      if (!ok) {
	goto err;
      }
      if (j < 258) {
	nameToGID->removeInt(macGlyphNames[j]);
	nameToGID->add(new GString(macGlyphNames[j]), i);
      }
    }
  }

  return;

 err:
  if (nameToGID) {
    delete nameToGID;
    nameToGID = NULL;
  }
}

int FoFiTrueType::seekTable(char *tag) {
  Guint tagI;
  int i;

  tagI = ((tag[0] & 0xff) << 24) |
         ((tag[1] & 0xff) << 16) |
         ((tag[2] & 0xff) << 8) |
          (tag[3] & 0xff);
  for (i = 0; i < nTables; ++i) {
    if (tables[i].tag == tagI) {
      return i;
    }
  }
  return -1;
}
