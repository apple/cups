//========================================================================
//
// GlobalParams.h
//
// Copyright 2001-2004 Glyph & Cog, LLC
//
//========================================================================

#ifndef GLOBALPARAMS_H
#define GLOBALPARAMS_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "CharTypes.h"

#if MULTITHREADED
#include "GMutex.h"
#endif

class GString;
class GList;
class GHash;
class NameToCharCode;
class CharCodeToUnicode;
class CharCodeToUnicodeCache;
class UnicodeMap;
class UnicodeMapCache;
class CMap;
class CMapCache;
struct XpdfSecurityHandler;
class GlobalParams;

//------------------------------------------------------------------------

// The global parameters object.
extern GlobalParams *globalParams;

//------------------------------------------------------------------------

enum DisplayFontParamKind {
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
      GString *fileName;
    } t1;
    struct {
      GString *fileName;
    } tt;
  };

  DisplayFontParam(GString *nameA, DisplayFontParamKind kindA);
  ~DisplayFontParam();
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

  void setBaseDir(char *dir);
  void setupBaseFonts(char *dir);

  //----- accessors

  CharCode getMacRomanCharCode(char *charName);

  GString *getBaseDir();
  Unicode mapNameToUnicode(char *charName);
  UnicodeMap *getResidentUnicodeMap(GString *encodingName);
  FILE *getUnicodeMapFile(GString *encodingName);
  FILE *findCMapFile(GString *collection, GString *cMapName);
  FILE *findToUnicodeFile(GString *name);
  DisplayFontParam *getDisplayFont(GString *fontName);
  DisplayFontParam *getDisplayCIDFont(GString *fontName, GString *collection);
  GString *getPSFile();
  int getPSPaperWidth();
  int getPSPaperHeight();
  void getPSImageableArea(int *llx, int *lly, int *urx, int *ury);
  GBool getPSDuplex();
  GBool getPSCrop();
  GBool getPSExpandSmaller();
  GBool getPSShrinkLarger();
  GBool getPSCenter();
  PSLevel getPSLevel();
  PSFontParam *getPSFont(GString *fontName);
  PSFontParam *getPSFont16(GString *fontName, GString *collection, int wMode);
  GBool getPSEmbedType1();
  GBool getPSEmbedTrueType();
  GBool getPSEmbedCIDPostScript();
  GBool getPSEmbedCIDTrueType();
  GBool getPSOPI();
  GBool getPSASCIIHex();
  GString *getTextEncodingName();
  EndOfLineKind getTextEOL();
  GBool getTextPageBreaks();
  GBool getTextKeepTinyChars();
  GString *findFontFile(GString *fontName, char **exts);
  GString *getInitialZoom();
  GBool getEnableT1lib();
  GBool getEnableFreeType();
  GBool getAntialias();
  GString *getURLCommand() { return urlCommand; }
  GString *getMovieCommand() { return movieCommand; }
  GBool getMapNumericCharNames();
  GBool getPrintCommands();
  GBool getErrQuiet();

  CharCodeToUnicode *getCIDToUnicode(GString *collection);
  CharCodeToUnicode *getUnicodeToUnicode(GString *fontName);
  UnicodeMap *getUnicodeMap(GString *encodingName);
  CMap *getCMap(GString *collection, GString *cMapName);
  UnicodeMap *getTextEncoding();

  //----- functions to set parameters

  void addDisplayFont(DisplayFontParam *param);
  void setPSFile(char *file);
  GBool setPSPaperSize(char *size);
  void setPSPaperWidth(int width);
  void setPSPaperHeight(int height);
  void setPSImageableArea(int llx, int lly, int urx, int ury);
  void setPSDuplex(GBool duplex);
  void setPSCrop(GBool crop);
  void setPSExpandSmaller(GBool expand);
  void setPSShrinkLarger(GBool shrink);
  void setPSCenter(GBool center);
  void setPSLevel(PSLevel level);
  void setPSEmbedType1(GBool embed);
  void setPSEmbedTrueType(GBool embed);
  void setPSEmbedCIDPostScript(GBool embed);
  void setPSEmbedCIDTrueType(GBool embed);
  void setPSOPI(GBool opi);
  void setPSASCIIHex(GBool hex);
  void setTextEncoding(char *encodingName);
  GBool setTextEOL(char *s);
  void setTextPageBreaks(GBool pageBreaks);
  void setTextKeepTinyChars(GBool keep);
  void setInitialZoom(char *s);
  GBool setEnableT1lib(char *s);
  GBool setEnableFreeType(char *s);
  GBool setAntialias(char *s);
  void setMapNumericCharNames(GBool map);
  void setPrintCommands(GBool printCommandsA);
  void setErrQuiet(GBool errQuietA);

  //----- security handlers

  void addSecurityHandler(XpdfSecurityHandler *handler);
  XpdfSecurityHandler *getSecurityHandler(char *name);

private:

  void parseFile(GString *fileName, FILE *f);
  void parseNameToUnicode(GList *tokens, GString *fileName, int line);
  void parseCIDToUnicode(GList *tokens, GString *fileName, int line);
  void parseUnicodeToUnicode(GList *tokens, GString *fileName, int line);
  void parseUnicodeMap(GList *tokens, GString *fileName, int line);
  void parseCMapDir(GList *tokens, GString *fileName, int line);
  void parseToUnicodeDir(GList *tokens, GString *fileName, int line);
  void parseDisplayFont(GList *tokens, GHash *fontHash,
			DisplayFontParamKind kind,
			GString *fileName, int line);
  void parsePSFile(GList *tokens, GString *fileName, int line);
  void parsePSPaperSize(GList *tokens, GString *fileName, int line);
  void parsePSImageableArea(GList *tokens, GString *fileName, int line);
  void parsePSLevel(GList *tokens, GString *fileName, int line);
  void parsePSFont(GList *tokens, GString *fileName, int line);
  void parsePSFont16(char *cmdName, GList *fontList,
		     GList *tokens, GString *fileName, int line);
  void parseTextEncoding(GList *tokens, GString *fileName, int line);
  void parseTextEOL(GList *tokens, GString *fileName, int line);
  void parseFontDir(GList *tokens, GString *fileName, int line);
  void parseInitialZoom(GList *tokens, GString *fileName, int line);
  void parseCommand(char *cmdName, GString **val,
		    GList *tokens, GString *fileName, int line);
  void parseYesNo(char *cmdName, GBool *flag,
		  GList *tokens, GString *fileName, int line);
  GBool parseYesNo2(char *token, GBool *flag);
  UnicodeMap *getUnicodeMap2(GString *encodingName);
#ifdef ENABLE_PLUGINS
  GBool loadPlugin(char *type, char *name);
#endif

  //----- static tables

  NameToCharCode *		// mapping from char name to
    macRomanReverseMap;		//   MacRomanEncoding index

  //----- user-modifiable settings

  GString *baseDir;		// base directory - for plugins, etc.
  NameToCharCode *		// mapping from char name to Unicode
    nameToUnicode;
  GHash *cidToUnicodes;		// files for mappings from char collections
				//   to Unicode, indexed by collection name
				//   [GString]
  GHash *unicodeToUnicodes;	// files for Unicode-to-Unicode mappings,
				//   indexed by font name pattern [GString]
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
  int psImageableLLX,		// imageable area, in PostScript points,
      psImageableLLY,		//   for PostScript output
      psImageableURX,
      psImageableURY;
  GBool psCrop;			// crop PS output to CropBox
  GBool psExpandSmaller;	// expand smaller pages to fill paper
  GBool psShrinkLarger;		// shrink larger pages to fit paper
  GBool psCenter;		// center pages on the paper
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
  GBool textPageBreaks;		// insert end-of-page markers?
  GBool textKeepTinyChars;	// keep all characters in text output
  GList *fontDirs;		// list of font dirs [GString]
  GString *initialZoom;		// initial zoom level
  GBool enableT1lib;		// t1lib enable flag
  GBool enableFreeType;		// FreeType enable flag
  GBool antialias;		// anti-aliasing enable flag
  GString *urlCommand;		// command executed for URL links
  GString *movieCommand;	// command executed for movie annotations
  GBool mapNumericCharNames;	// map numeric char names (from font subsets)?
  GBool printCommands;		// print the drawing commands
  GBool errQuiet;		// suppress error messages?

  CharCodeToUnicodeCache *cidToUnicodeCache;
  CharCodeToUnicodeCache *unicodeToUnicodeCache;
  UnicodeMapCache *unicodeMapCache;
  CMapCache *cMapCache;

#ifdef ENABLE_PLUGINS
  GList *plugins;		// list of plugins [Plugin]
  GList *securityHandlers;	// list of loaded security handlers
				//   [XpdfSecurityHandler]
#endif

#if MULTITHREADED
  GMutex mutex;
  GMutex unicodeMapCacheMutex;
  GMutex cMapCacheMutex;
#endif
};

#endif
