//========================================================================
//
// GlobalParams.h
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef GLOBALPARAMS_H
#define GLOBALPARAMS_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "CharTypes.h"

class GString;
class GList;
class GHash;
class NameToCharCode;
class CharCodeToUnicode;
class CIDToUnicodeCache;
class UnicodeMap;
class UnicodeMapCache;
class CMap;
class CMapCache;
class GlobalParams;

//------------------------------------------------------------------------

// The global parameters object.
extern GlobalParams *globalParams;

//------------------------------------------------------------------------

enum DisplayFontParamKind {
  displayFontX,
  displayFontT1,
  displayFontTT
};

class DisplayFontParam {
public:

  GString *name;		// font name for 8-bit fonts and named
				//   CID fonts; collection name for
				//   generic CID fonts
  DisplayFontParamKind kind;
  union {
    struct {
      GString *xlfd;
      GString *encoding;
    } x;
    struct {
      GString *fileName;
    } t1;
    struct {
      GString *fileName;
    } tt;
  };

  DisplayFontParam(GString *nameA, DisplayFontParamKind kindA);
  DisplayFontParam(char *nameA, char *xlfdA, char *encodingA);
  ~DisplayFontParam();
};

// Font rasterizer control.
enum FontRastControl {
  fontRastNone,			// don't use this rasterizer
  fontRastPlain,		// use it, without anti-aliasing
  fontRastAALow,		// use it, with low-level anti-aliasing
  fontRastAAHigh		// use it, with high-level anti-aliasing
};

//------------------------------------------------------------------------

class PSFontParam {
public:

  GString *pdfFontName;		// PDF font name for 8-bit fonts and
				//   named 16-bit fonts; char collection
				//   name for generic 16-bit fonts
  int wMode;			// writing mode (0=horiz, 1=vert) for
				//   16-bit fonts
  GString *psFontName;		// PostScript font name
  GString *encoding;		// encoding, for 16-bit fonts only

  PSFontParam(GString *pdfFontNameA, int wModeA,
	      GString *psFontNameA, GString *encodingA);
  ~PSFontParam();
};

//------------------------------------------------------------------------

enum PSLevel {
  psLevel1,
  psLevel1Sep,
  psLevel2,
  psLevel2Sep,
  psLevel3,
  psLevel3Sep
};

//------------------------------------------------------------------------

enum EndOfLineKind {
  eolUnix,			// LF
  eolDOS,			// CR+LF
  eolMac			// CR
};

//------------------------------------------------------------------------

class GlobalParams {
public:

  // Initialize the global parameters by attempting to read a config
  // file.
  GlobalParams(char *cfgFileName);

  ~GlobalParams();

  //----- accessors

  CharCode getMacRomanCharCode(char *charName);

  Unicode mapNameToUnicode(char *charName);
  FILE *getCIDToUnicodeFile(GString *collection);
  UnicodeMap *getResidentUnicodeMap(GString *encodingName);
  FILE *getUnicodeMapFile(GString *encodingName);
  FILE *findCMapFile(GString *collection, GString *cMapName);
  FILE *findToUnicodeFile(GString *name);
  DisplayFontParam *getDisplayFont(GString *fontName);
  DisplayFontParam *getDisplayCIDFont(GString *fontName, GString *collection);
  GString *getPSFile() { return psFile; }
  int getPSPaperWidth() { return psPaperWidth; }
  int getPSPaperHeight() { return psPaperHeight; }
  GBool getPSDuplex() { return psDuplex; }
  PSLevel getPSLevel() { return psLevel; }
  PSFontParam *getPSFont(GString *fontName);
  PSFontParam *getPSFont16(GString *fontName, GString *collection, int wMode);
  GBool getPSEmbedType1() { return psEmbedType1; }
  GBool getPSEmbedTrueType() { return psEmbedTrueType; }
  GBool getPSEmbedCIDPostScript() { return psEmbedCIDPostScript; }
  GBool getPSEmbedCIDTrueType() { return psEmbedCIDTrueType; }
  GBool getPSOPI() { return psOPI; }
  GBool getPSASCIIHex() { return psASCIIHex; }
  GString *getTextEncodingName() { return textEncoding; }
  EndOfLineKind getTextEOL() { return textEOL; }
  GString *findFontFile(GString *fontName, char *ext1, char *ext2);
  GString *getInitialZoom() { return initialZoom; }
  FontRastControl getT1libControl() { return t1libControl; }
  FontRastControl getFreeTypeControl() { return freetypeControl; }
  GString *getURLCommand() { return urlCommand; }
  GBool getMapNumericCharNames() { return mapNumericCharNames; }
  GBool getErrQuiet() { return errQuiet; }

  CharCodeToUnicode *getCIDToUnicode(GString *collection);
  UnicodeMap *getUnicodeMap(GString *encodingName);
  CMap *getCMap(GString *collection, GString *cMapName);
  UnicodeMap *getTextEncoding();

  //----- functions to set parameters

  void setPSFile(char *file);
  GBool setPSPaperSize(char *size);
  void setPSPaperWidth(int width);
  void setPSPaperHeight(int height);
  void setPSDuplex(GBool duplex);
  void setPSLevel(PSLevel level);
  void setPSEmbedType1(GBool embed);
  void setPSEmbedTrueType(GBool embed);
  void setPSEmbedCIDPostScript(GBool embed);
  void setPSEmbedCIDTrueType(GBool embed);
  void setPSOPI(GBool opi);
  void setPSASCIIHex(GBool hex);
  void setTextEncoding(char *encodingName);
  GBool setTextEOL(char *s);
  void setInitialZoom(char *s);
  GBool setT1libControl(char *s);
  GBool setFreeTypeControl(char *s);
  void setErrQuiet(GBool errQuietA);

private:

  void parseFile(GString *fileName, FILE *f);
  void parseNameToUnicode(GList *tokens, GString *fileName, int line);
  void parseCIDToUnicode(GList *tokens, GString *fileName, int line);
  void parseUnicodeMap(GList *tokens, GString *fileName, int line);
  void parseCMapDir(GList *tokens, GString *fileName, int line);
  void parseToUnicodeDir(GList *tokens, GString *fileName, int line);
  void parseDisplayFont(GList *tokens, GHash *fontHash,
			DisplayFontParamKind kind,
			GString *fileName, int line);
  void parsePSFile(GList *tokens, GString *fileName, int line);
  void parsePSPaperSize(GList *tokens, GString *fileName, int line);
  void parsePSLevel(GList *tokens, GString *fileName, int line);
  void parsePSFont(GList *tokens, GString *fileName, int line);
  void parsePSFont16(char *cmdName, GList *fontList,
		     GList *tokens, GString *fileName, int line);
  void parseTextEncoding(GList *tokens, GString *fileName, int line);
  void parseTextEOL(GList *tokens, GString *fileName, int line);
  void parseFontDir(GList *tokens, GString *fileName, int line);
  void parseInitialZoom(GList *tokens, GString *fileName, int line);
  void parseFontRastControl(char *cmdName, FontRastControl *val,
			    GList *tokens, GString *fileName, int line);
  void parseURLCommand(GList *tokens, GString *fileName, int line);
  void parseYesNo(char *cmdName, GBool *flag,
		  GList *tokens, GString *fileName, int line);
  GBool setFontRastControl(FontRastControl *val, char *s);

  //----- static tables

  NameToCharCode *		// mapping from char name to
    macRomanReverseMap;		//   MacRomanEncoding index

  //----- user-modifiable settings

  NameToCharCode *		// mapping from char name to Unicode
    nameToUnicode;
  GHash *cidToUnicodes;		// files for mappings from char collections
				//   to Unicode, indexed by collection name
				//   [GString]
  GHash *residentUnicodeMaps;	// mappings from Unicode to char codes,
				//   indexed by encoding name [UnicodeMap]
  GHash *unicodeMaps;		// files for mappings from Unicode to char
				//   codes, indexed by encoding name [GString]
  GHash *cMapDirs;		// list of CMap dirs, indexed by collection
				//   name [GList[GString]]
  GList *toUnicodeDirs;		// list of ToUnicode CMap dirs [GString]
  GHash *displayFonts;		// display font info, indexed by font name
				//   [DisplayFontParam]
  GHash *displayCIDFonts;	// display CID font info, indexed by
				//   collection [DisplayFontParam]
  GHash *displayNamedCIDFonts;	// display CID font info, indexed by
				//   font name [DisplayFontParam]
  GString *psFile;		// PostScript file or command (for xpdf)
  int psPaperWidth;		// paper size, in PostScript points, for
  int psPaperHeight;		//   PostScript output
  GBool psDuplex;		// enable duplexing in PostScript?
  PSLevel psLevel;		// PostScript level to generate
  GHash *psFonts;		// PostScript font info, indexed by PDF
				//   font name [PSFontParam]
  GList *psNamedFonts16;	// named 16-bit fonts [PSFontParam]
  GList *psFonts16;		// generic 16-bit fonts [PSFontParam]
  GBool psEmbedType1;		// embed Type 1 fonts?
  GBool psEmbedTrueType;	// embed TrueType fonts?
  GBool psEmbedCIDPostScript;	// embed CID PostScript fonts?
  GBool psEmbedCIDTrueType;	// embed CID TrueType fonts?
  GBool psOPI;			// generate PostScript OPI comments?
  GBool psASCIIHex;		// use ASCIIHex instead of ASCII85?
  GString *textEncoding;	// encoding (unicodeMap) to use for text
				//   output
  EndOfLineKind textEOL;	// type of EOL marker to use for text
				//   output
  GList *fontDirs;		// list of font dirs [GString]
  GString *initialZoom;		// initial zoom level
  FontRastControl t1libControl;	// t1lib rasterization mode
  FontRastControl		// FreeType rasterization mode
    freetypeControl;
  GString *urlCommand;		// command executed for URL links
  GBool mapNumericCharNames;	// map numeric char names (from font subsets)?
  GBool errQuiet;		// suppress error messages?

  CIDToUnicodeCache *cidToUnicodeCache;
  UnicodeMapCache *unicodeMapCache;
  CMapCache *cMapCache;
};

#endif
