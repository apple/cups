//========================================================================
//
// GlobalParams.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>
#include <string.h>
#include <ctype.h>
#if HAVE_PAPER_H
#include <paper.h>
#endif
#include "gmem.h"
#include "GString.h"
#include "GList.h"
#include "GHash.h"
#include "gfile.h"
#include "Error.h"
#include "NameToCharCode.h"
#include "CharCodeToUnicode.h"
#include "UnicodeMap.h"
#include "CMap.h"
#include "BuiltinFontTables.h"
#include "FontEncodingTables.h"
#include "GlobalParams.h"

#include "NameToUnicodeTable.h"
#include "UnicodeMapTables.h"
#include "DisplayFontTable.h"
#include "UTF8.h"

//------------------------------------------------------------------------

GlobalParams *globalParams = NULL;

//------------------------------------------------------------------------
// DisplayFontParam
//------------------------------------------------------------------------

DisplayFontParam::DisplayFontParam(GString *nameA,
				   DisplayFontParamKind kindA) {
  name = nameA;
  kind = kindA;
  switch (kind) {
  case displayFontX:
    x.xlfd = NULL;
    x.encoding = NULL;
    break;
  case displayFontT1:
    t1.fileName = NULL;
    break;
  case displayFontTT:
    tt.fileName = NULL;
    break;
  }
}

DisplayFontParam::DisplayFontParam(const char *nameA, const char *xlfdA, const char *encodingA) {
  name = new GString(nameA);
  kind = displayFontX;
  x.xlfd = new GString(xlfdA);
  x.encoding = new GString(encodingA);
}

DisplayFontParam::~DisplayFontParam() {
  delete name;
  switch (kind) {
  case displayFontX:
    if (x.xlfd) {
      delete x.xlfd;
    }
    if (x.encoding) {
      delete x.encoding;
    }
    break;
  case displayFontT1:
    if (t1.fileName) {
      delete t1.fileName;
    }
    break;
  case displayFontTT:
    if (tt.fileName) {
      delete tt.fileName;
    }
    break;
  }
}

//------------------------------------------------------------------------
// PSFontParam
//------------------------------------------------------------------------

PSFontParam::PSFontParam(GString *pdfFontNameA, int wModeA,
			 GString *psFontNameA, GString *encodingA) {
  pdfFontName = pdfFontNameA;
  wMode = wModeA;
  psFontName = psFontNameA;
  encoding = encodingA;
}

PSFontParam::~PSFontParam() {
  delete pdfFontName;
  delete psFontName;
  if (encoding) {
    delete encoding;
  }
}

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

GlobalParams::GlobalParams(const char *cfgFileName) {
  UnicodeMap *map;
  DisplayFontParam *dfp;
  GString *fileName;
  FILE *f;
  int i;

  initBuiltinFontTables();

  // scan the encoding in reverse because we want the lowest-numbered
  // index for each char name ('space' is encoded twice)
  macRomanReverseMap = new NameToCharCode();
  for (i = 255; i >= 0; --i) {
    if (macRomanEncoding[i]) {
      macRomanReverseMap->add(macRomanEncoding[i], (CharCode)i);
    }
  }

  nameToUnicode = new NameToCharCode();
  cidToUnicodes = new GHash(gTrue);
  residentUnicodeMaps = new GHash();
  unicodeMaps = new GHash(gTrue);
  cMapDirs = new GHash(gTrue);
  toUnicodeDirs = new GList();
  displayFonts = new GHash();
  displayCIDFonts = new GHash();
  displayNamedCIDFonts = new GHash();
#if HAVE_PAPER_H
  const struct paper *paperType;
  paperinit();
  paperType = paperinfo(systempapername());
  psPaperWidth = (int)paperpswidth(paperType);
  psPaperHeight = (int)paperpsheight(paperType);
  paperdone();
#else
  psPaperWidth = defPaperWidth;
  psPaperHeight = defPaperHeight;
#endif
  psDuplex = gFalse;
  psLevel = psLevel2;
  psFile = NULL;
  psFonts = new GHash();
  psNamedFonts16 = new GList();
  psFonts16 = new GList();
  psEmbedType1 = gTrue;
  psEmbedTrueType = gTrue;
  psEmbedCIDPostScript = gTrue;
  psEmbedCIDTrueType = gTrue;
  psOPI = gFalse;
  psASCIIHex = gFalse;
  textEncoding = new GString("Latin1");
#if defined(WIN32)
  textEOL = eolDOS;
#elif defined(MACOS)
  textEOL = eolMac;
#else
  textEOL = eolUnix;
#endif
  fontDirs = new GList();
  initialZoom = new GString("1");
  t1libControl = fontRastAALow;
  freetypeControl = fontRastAALow;
  urlCommand = NULL;
  mapNumericCharNames = gTrue;
  errQuiet = gFalse;

  cidToUnicodeCache = new CIDToUnicodeCache();
  unicodeMapCache = new UnicodeMapCache();
  cMapCache = new CMapCache();

  // set up the initial nameToUnicode table
  for (i = 0; nameToUnicodeTab[i].name; ++i) {
    nameToUnicode->add(nameToUnicodeTab[i].name, nameToUnicodeTab[i].u);
  }

  // set up the residentUnicodeMaps table
  map = new UnicodeMap("Latin1", latin1UnicodeMapRanges, latin1UnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("ASCII7", ascii7UnicodeMapRanges, ascii7UnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("Symbol", symbolUnicodeMapRanges, symbolUnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("ZapfDingbats", zapfDingbatsUnicodeMapRanges,
		       zapfDingbatsUnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("UTF-8", &mapUTF8);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("UCS-2", &mapUCS2);
  residentUnicodeMaps->add(map->getEncodingName(), map);

  // default displayFonts table
  for (i = 0; displayFontTab[i].name; ++i) {
    dfp = new DisplayFontParam(displayFontTab[i].name,
			       displayFontTab[i].xlfd,
			       displayFontTab[i].encoding);
    displayFonts->add(dfp->name, dfp);
  }

  // look for a user config file, then a system-wide config file
  f = NULL;
  fileName = NULL;
  if (cfgFileName && cfgFileName[0]) {
    fileName = new GString(cfgFileName);
    if (!(f = fopen(fileName->getCString(), "r"))) {
      delete fileName;
    }
  }
  if (!f) {
    fileName = appendToPath(getHomeDir(), xpdfUserConfigFile);
    if (!(f = fopen(fileName->getCString(), "r"))) {
      delete fileName;
    }
  }
  if (!f) {
#if defined(WIN32) && !defined(__CYGWIN32__)
    char buf[512];
    i = GetModuleFileName(NULL, buf, sizeof(buf));
    if (i <= 0 || i >= sizeof(buf)) {
      // error or path too long for buffer - just use the current dir
      buf[i] = '\0';
    }
    fileName = grabPath(buf);
    appendToPath(fileName, xpdfSysConfigFile);
#else
    fileName = new GString(xpdfSysConfigFile);
#endif
    if (!(f = fopen(fileName->getCString(), "r"))) {
      delete fileName;
    }
  }
  if (f) {
    parseFile(fileName, f);
    delete fileName;
  }
}

void GlobalParams::parseFile(GString *fileName, FILE *f) {
  int line;
  GList *tokens;
  GString *cmd, *incFile;
  char *p1, *p2;
  char buf[512];
  FILE *f2;

  line = 1;
  while (fgets(buf, sizeof(buf) - 1, f)) {

    // break the line into tokens
    tokens = new GList();
    p1 = buf;
    while (*p1) {
      for (; *p1 && isspace(*p1); ++p1) ;
      if (!*p1) {
	break;
      }
      if (*p1 == '"' || *p1 == '\'') {
	for (p2 = p1 + 1; *p2 && *p2 != *p1; ++p2) ;
	++p1;
      } else {
	for (p2 = p1 + 1; *p2 && !isspace(*p2); ++p2) ;
      }
      tokens->append(new GString(p1, p2 - p1));
      p1 = p2 + 1;
    }

    if (tokens->getLength() > 0 &&
	((GString *)tokens->get(0))->getChar(0) != '#') {
      cmd = (GString *)tokens->get(0);
      if (!cmd->cmp("include")) {
	if (tokens->getLength() == 2) {
	  incFile = (GString *)tokens->get(1);
	  if ((f2 = fopen(incFile->getCString(), "r"))) {
	    parseFile(incFile, f2);
	    fclose(f2);
	  } else {
	    error(-1, "Couldn't find included config file: '%s' (%s:%d)",
		  incFile->getCString(), fileName->getCString(), line);
	  }
	} else {
	  error(-1, "Bad 'include' config file command (%s:%d)",
		fileName->getCString(), line);
	}
      } else if (!cmd->cmp("nameToUnicode")) {
	parseNameToUnicode(tokens, fileName, line);
      } else if (!cmd->cmp("cidToUnicode")) {
	parseCIDToUnicode(tokens, fileName, line);
      } else if (!cmd->cmp("unicodeMap")) {
	parseUnicodeMap(tokens, fileName, line);
      } else if (!cmd->cmp("cMapDir")) {
	parseCMapDir(tokens, fileName, line);
      } else if (!cmd->cmp("toUnicodeDir")) {
	parseToUnicodeDir(tokens, fileName, line);
      } else if (!cmd->cmp("displayFontX")) {
	parseDisplayFont(tokens, displayFonts, displayFontX, fileName, line);
      } else if (!cmd->cmp("displayFontT1")) {
	parseDisplayFont(tokens, displayFonts, displayFontT1, fileName, line);
      } else if (!cmd->cmp("displayFontTT")) {
	parseDisplayFont(tokens, displayFonts, displayFontTT, fileName, line);
      } else if (!cmd->cmp("displayCIDFontX")) {
	parseDisplayFont(tokens, displayCIDFonts,
			 displayFontX, fileName, line);
      } else if (!cmd->cmp("displayNamedCIDFontX")) {
	parseDisplayFont(tokens, displayNamedCIDFonts,
			 displayFontX, fileName, line);
      } else if (!cmd->cmp("psFile")) {
	parsePSFile(tokens, fileName, line);
      } else if (!cmd->cmp("psFont")) {
	parsePSFont(tokens, fileName, line);
      } else if (!cmd->cmp("psNamedFont16")) {
	parsePSFont16("psNamedFont16", psNamedFonts16,
		      tokens, fileName, line);
      } else if (!cmd->cmp("psFont16")) {
	parsePSFont16("psFont16", psFonts16, tokens, fileName, line);
      } else if (!cmd->cmp("psPaperSize")) {
	parsePSPaperSize(tokens, fileName, line);
      } else if (!cmd->cmp("psDuplex")) {
	parseYesNo("psDuplex", &psDuplex, tokens, fileName, line);
      } else if (!cmd->cmp("psLevel")) {
	parsePSLevel(tokens, fileName, line);
      } else if (!cmd->cmp("psEmbedType1Fonts")) {
	parseYesNo("psEmbedType1", &psEmbedType1, tokens, fileName, line);
      } else if (!cmd->cmp("psEmbedTrueTypeFonts")) {
	parseYesNo("psEmbedTrueType", &psEmbedTrueType,
		   tokens, fileName, line);
      } else if (!cmd->cmp("psEmbedCIDPostScriptFonts")) {
	parseYesNo("psEmbedCIDPostScript", &psEmbedCIDPostScript,
		   tokens, fileName, line);
      } else if (!cmd->cmp("psEmbedCIDTrueTypeFonts")) {
	parseYesNo("psEmbedCIDTrueType", &psEmbedCIDTrueType,
		   tokens, fileName, line);
      } else if (!cmd->cmp("psOPI")) {
	parseYesNo("psOPI", &psOPI, tokens, fileName, line);
      } else if (!cmd->cmp("psASCIIHex")) {
	parseYesNo("psASCIIHex", &psASCIIHex, tokens, fileName, line);
      } else if (!cmd->cmp("textEncoding")) {
	parseTextEncoding(tokens, fileName, line);
      } else if (!cmd->cmp("textEOL")) {
	parseTextEOL(tokens, fileName, line);
      } else if (!cmd->cmp("fontDir")) {
	parseFontDir(tokens, fileName, line);
      } else if (!cmd->cmp("initialZoom")) {
	parseInitialZoom(tokens, fileName, line);
      } else if (!cmd->cmp("t1libControl")) {
	parseFontRastControl("t1libControl", &t1libControl,
			     tokens, fileName, line);
      } else if (!cmd->cmp("freetypeControl")) {
	parseFontRastControl("freetypeControl", &freetypeControl,
			     tokens, fileName, line);
      } else if (!cmd->cmp("urlCommand")) {
	parseURLCommand(tokens, fileName, line);
      } else if (!cmd->cmp("mapNumericCharNames")) {
	parseYesNo("mapNumericCharNames", &mapNumericCharNames,
		   tokens, fileName, line);
      } else if (!cmd->cmp("errQuiet")) {
	parseYesNo("errQuiet", &errQuiet, tokens, fileName, line);
      } else if (!cmd->cmp("fontpath") || !cmd->cmp("fontmap")) {
	error(-1, "Unknown config file command");
	error(-1, "-- the config file format has changed since Xpdf 0.9x");
      } else {
	error(-1, "Unknown config file command '%s' (%s:%d)",
	      cmd->getCString(), fileName->getCString(), line);
      }
    }

    deleteGList(tokens, GString);
    ++line;
  }
}

void GlobalParams::parseNameToUnicode(GList *tokens, GString *fileName,
					 int line) {
  GString *name;
  char *tok1, *tok2;
  FILE *f;
  char buf[256];
  int line2;
  Unicode u;

  if (tokens->getLength() != 2) {
    error(-1, "Bad 'nameToUnicode' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  name = (GString *)tokens->get(1);
  if (!(f = fopen(name->getCString(), "r"))) {
    error(-1, "Couldn't open 'nameToUnicode' file '%s'",
	  name->getCString());
    return;
  }
  line2 = 1;
  while (fgets(buf, sizeof(buf), f)) {
    tok1 = strtok(buf, " \t\r\n");
    tok2 = strtok(NULL, " \t\r\n");
    if (tok1 && tok2) {
      sscanf(tok1, "%x", &u);
      nameToUnicode->add(tok2, u);
    } else {
      error(-1, "Bad line in 'nameToUnicode' file (%s:%d)", name, line2);
    }
    ++line2;
  }
  fclose(f);
}

void GlobalParams::parseCIDToUnicode(GList *tokens, GString *fileName,
				     int line) {
  GString *collection, *name, *old;

  if (tokens->getLength() != 3) {
    error(-1, "Bad 'cidToUnicode' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  collection = (GString *)tokens->get(1);
  name = (GString *)tokens->get(2);
  if ((old = (GString *)cidToUnicodes->remove(collection))) {
    delete old;
  }
  cidToUnicodes->add(collection->copy(), name->copy());
}

void GlobalParams::parseUnicodeMap(GList *tokens, GString *fileName,
				   int line) {
  GString *encodingName, *name, *old;

  if (tokens->getLength() != 3) {
    error(-1, "Bad 'unicodeMap' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  encodingName = (GString *)tokens->get(1);
  name = (GString *)tokens->get(2);
  if ((old = (GString *)unicodeMaps->remove(encodingName))) {
    delete old;
  }
  unicodeMaps->add(encodingName->copy(), name->copy());
}

void GlobalParams::parseCMapDir(GList *tokens, GString *fileName, int line) {
  GString *collection, *dir;
  GList *list;

  if (tokens->getLength() != 3) {
    error(-1, "Bad 'cMapDir' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  collection = (GString *)tokens->get(1);
  dir = (GString *)tokens->get(2);
  if (!(list = (GList *)cMapDirs->lookup(collection))) {
    list = new GList();
    cMapDirs->add(collection->copy(), list);
  }
  list->append(dir->copy());
}

void GlobalParams::parseToUnicodeDir(GList *tokens, GString *fileName,
				     int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'toUnicodeDir' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  toUnicodeDirs->append(((GString *)tokens->get(1))->copy());
}

void GlobalParams::parseDisplayFont(GList *tokens, GHash *fontHash,
				    DisplayFontParamKind kind,
				    GString *fileName, int line) {
  DisplayFontParam *param, *old;

  if (tokens->getLength() < 2) {
    goto err1;
  }
  param = new DisplayFontParam(((GString *)tokens->get(1))->copy(), kind);
  
  switch (kind) {
  case displayFontX:
    if (tokens->getLength() != 4) {
      goto err2;
    }
    param->x.xlfd = ((GString *)tokens->get(2))->copy();
    param->x.encoding = ((GString *)tokens->get(3))->copy();
    break;
  case displayFontT1:
    if (tokens->getLength() != 3) {
      goto err2;
    }
    param->t1.fileName = ((GString *)tokens->get(2))->copy();
    break;
  case displayFontTT:
    if (tokens->getLength() != 3) {
      goto err2;
    }
    param->tt.fileName = ((GString *)tokens->get(2))->copy();
    break;
  }

  if ((old = (DisplayFontParam *)fontHash->remove(param->name))) {
    delete old;
  }
  fontHash->add(param->name, param);
  return;

 err2:
  delete param;
 err1:
  error(-1, "Bad 'display*Font*' config file command (%s:%d)",
	fileName->getCString(), line);
}

void GlobalParams::parsePSPaperSize(GList *tokens, GString *fileName,
				    int line) {
  GString *tok;

  if (tokens->getLength() == 2) {
    tok = (GString *)tokens->get(1);
    if (!setPSPaperSize(tok->getCString())) {
      error(-1, "Bad 'psPaperSize' config file command (%s:%d)",
	    fileName->getCString(), line);
    }
  } else if (tokens->getLength() == 3) {
    tok = (GString *)tokens->get(1);
    psPaperWidth = atoi(tok->getCString());
    tok = (GString *)tokens->get(2);
    psPaperHeight = atoi(tok->getCString());
  } else {
    error(-1, "Bad 'psPaperSize' config file command (%s:%d)",
	  fileName->getCString(), line);
  }
}

void GlobalParams::parsePSLevel(GList *tokens, GString *fileName, int line) {
  GString *tok;

  if (tokens->getLength() != 2) {
    error(-1, "Bad 'psLevel' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  tok = (GString *)tokens->get(1);
  if (!tok->cmp("level1")) {
    psLevel = psLevel1;
  } else if (!tok->cmp("level1sep")) {
    psLevel = psLevel1Sep;
  } else if (!tok->cmp("level2")) {
    psLevel = psLevel2;
  } else if (!tok->cmp("level2sep")) {
    psLevel = psLevel2Sep;
  } else if (!tok->cmp("level3")) {
    psLevel = psLevel3;
  } else if (!tok->cmp("level3Sep")) {
    psLevel = psLevel3Sep;
  } else {
    error(-1, "Bad 'psLevel' config file command (%s:%d)",
	  fileName->getCString(), line);
  }
}

void GlobalParams::parsePSFile(GList *tokens, GString *fileName, int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'psFile' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  if (psFile) {
    delete psFile;
  }
  psFile = ((GString *)tokens->get(1))->copy();
}

void GlobalParams::parsePSFont(GList *tokens, GString *fileName, int line) {
  PSFontParam *param;

  if (tokens->getLength() != 3) {
    error(-1, "Bad 'psFont' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  param = new PSFontParam(((GString *)tokens->get(1))->copy(), 0,
			  ((GString *)tokens->get(2))->copy(), NULL);
  psFonts->add(param->pdfFontName, param);
}

void GlobalParams::parsePSFont16(const char *cmdName, GList *fontList,
				 GList *tokens, GString *fileName, int line) {
  PSFontParam *param;
  int wMode;
  GString *tok;

  if (tokens->getLength() != 5) {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
    return;
  }
  tok = (GString *)tokens->get(2);
  if (!tok->cmp("H")) {
    wMode = 0;
  } else if (!tok->cmp("V")) {
    wMode = 1;
  } else {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
    return;
  }
  param = new PSFontParam(((GString *)tokens->get(1))->copy(),
			  wMode,
			  ((GString *)tokens->get(3))->copy(),
			  ((GString *)tokens->get(4))->copy());
  fontList->append(param);
}

void GlobalParams::parseTextEncoding(GList *tokens, GString *fileName,
				     int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'textEncoding' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  delete textEncoding;
  textEncoding = ((GString *)tokens->get(1))->copy();
}

void GlobalParams::parseTextEOL(GList *tokens, GString *fileName, int line) {
  GString *tok;

  if (tokens->getLength() != 2) {
    error(-1, "Bad 'textEOL' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  tok = (GString *)tokens->get(1);
  if (!tok->cmp("unix")) {
    textEOL = eolUnix;
  } else if (!tok->cmp("dos")) {
    textEOL = eolDOS;
  } else if (!tok->cmp("mac")) {
    textEOL = eolMac;
  } else {
    error(-1, "Bad 'textEOL' config file command (%s:%d)",
	  fileName->getCString(), line);
  }
}

void GlobalParams::parseFontDir(GList *tokens, GString *fileName, int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'fontDir' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  fontDirs->append(((GString *)tokens->get(1))->copy());
}

void GlobalParams::parseInitialZoom(GList *tokens,
				    GString *fileName, int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'initialZoom' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  delete initialZoom;
  initialZoom = ((GString *)tokens->get(1))->copy();
}

void GlobalParams::parseFontRastControl(const char *cmdName, FontRastControl *val,
					GList *tokens, GString *fileName,
					int line) {
  GString *tok;

  if (tokens->getLength() != 2) {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
    return;
  }
  tok = (GString *)tokens->get(1);
  if (!setFontRastControl(val, tok->getCString())) {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
  }
}

void GlobalParams::parseURLCommand(GList *tokens, GString *fileName,
				   int line) {
  if (tokens->getLength() != 2) {
    error(-1, "Bad 'urlCommand' config file command (%s:%d)",
	  fileName->getCString(), line);
    return;
  }
  if (urlCommand) {
    delete urlCommand;
  }
  urlCommand = ((GString *)tokens->get(1))->copy();
}

void GlobalParams::parseYesNo(const char *cmdName, GBool *flag,
			      GList *tokens, GString *fileName, int line) {
  GString *tok;

  if (tokens->getLength() != 2) {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
    return;
  }
  tok = (GString *)tokens->get(1);
  if (!tok->cmp("yes")) {
    *flag = gTrue;
  } else if (!tok->cmp("no")) {
    *flag = gFalse;
  } else {
    error(-1, "Bad '%s' config file command (%s:%d)",
	  cmdName, fileName->getCString(), line);
  }
}

GlobalParams::~GlobalParams() {
  GHashIter *iter;
  GString *key;
  GList *list;

  freeBuiltinFontTables();

  delete macRomanReverseMap;

  delete nameToUnicode;
  deleteGHash(cidToUnicodes, GString);
  deleteGHash(residentUnicodeMaps, UnicodeMap);
  deleteGHash(unicodeMaps, GString);
  deleteGList(toUnicodeDirs, GString);
  deleteGHash(displayFonts, DisplayFontParam);
  deleteGHash(displayCIDFonts, DisplayFontParam);
  deleteGHash(displayNamedCIDFonts, DisplayFontParam);
  if (psFile) {
    delete psFile;
  }
  deleteGHash(psFonts, PSFontParam);
  deleteGList(psNamedFonts16, PSFontParam);
  deleteGList(psFonts16, PSFontParam);
  delete textEncoding;
  deleteGList(fontDirs, GString);
  delete initialZoom;
  if (urlCommand) {
    delete urlCommand;
  }

  cMapDirs->startIter(&iter);
  while (cMapDirs->getNext(&iter, &key, (void **)&list)) {
    deleteGList(list, GString);
  }
  delete cMapDirs;

  delete cidToUnicodeCache;
  delete unicodeMapCache;
  delete cMapCache;
}

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

CharCode GlobalParams::getMacRomanCharCode(const char *charName) {
  return macRomanReverseMap->lookup(charName);
}

Unicode GlobalParams::mapNameToUnicode(const char *charName) {
  return nameToUnicode->lookup(charName);
}

FILE *GlobalParams::getCIDToUnicodeFile(GString *collection) {
  GString *fileName;

  if (!(fileName = (GString *)cidToUnicodes->lookup(collection))) {
    return NULL;
  }
  return fopen(fileName->getCString(), "r");
}

UnicodeMap *GlobalParams::getResidentUnicodeMap(GString *encodingName) {
  return (UnicodeMap *)residentUnicodeMaps->lookup(encodingName);
}

FILE *GlobalParams::getUnicodeMapFile(GString *encodingName) {
  GString *fileName;

  if (!(fileName = (GString *)unicodeMaps->lookup(encodingName))) {
    return NULL;
  }
  return fopen(fileName->getCString(), "r");
}

FILE *GlobalParams::findCMapFile(GString *collection, GString *cMapName) {
  GList *list;
  GString *dir;
  GString *fileName;
  FILE *f;
  int i;

  if (!(list = (GList *)cMapDirs->lookup(collection))) {
    return NULL;
  }
  for (i = 0; i < list->getLength(); ++i) {
    dir = (GString *)list->get(i);
    fileName = appendToPath(dir->copy(), cMapName->getCString());
    f = fopen(fileName->getCString(), "r");
    delete fileName;
    if (f) {
      return f;
    }
  }
  return NULL;
}

FILE *GlobalParams::findToUnicodeFile(GString *name) {
  GString *dir, *fileName;
  FILE *f;
  int i;

  for (i = 0; i < toUnicodeDirs->getLength(); ++i) {
    dir = (GString *)toUnicodeDirs->get(i);
    fileName = appendToPath(dir->copy(), name->getCString());
    f = fopen(fileName->getCString(), "r");
    delete fileName;
    if (f) {
      return f;
    }
  }
  return NULL;
}

DisplayFontParam *GlobalParams::getDisplayFont(GString *fontName) {
  return (DisplayFontParam *)displayFonts->lookup(fontName);
}

DisplayFontParam *GlobalParams::getDisplayCIDFont(GString *fontName,
						  GString *collection) {
  DisplayFontParam *dfp;

  if (!fontName ||
      !(dfp = (DisplayFontParam *)displayNamedCIDFonts->lookup(fontName))) {
    dfp = (DisplayFontParam *)displayCIDFonts->lookup(collection);
  }
  return dfp;
}

PSFontParam *GlobalParams::getPSFont(GString *fontName) {
  return (PSFontParam *)psFonts->lookup(fontName);
}

PSFontParam *GlobalParams::getPSFont16(GString *fontName,
				       GString *collection, int wMode) {
  PSFontParam *p;
  int i;

  p = NULL;
  if (fontName) {
    for (i = 0; i < psNamedFonts16->getLength(); ++i) {
      p = (PSFontParam *)psNamedFonts16->get(i);
      if (!p->pdfFontName->cmp(fontName) &&
	  p->wMode == wMode) {
	break;
      }
      p = NULL;
    }
  }
  if (!p && collection) {
    for (i = 0; i < psFonts16->getLength(); ++i) {
      p = (PSFontParam *)psFonts16->get(i);
      if (!p->pdfFontName->cmp(collection) &&
	  p->wMode == wMode) {
	break;
      }
      p = NULL;
    }
  }
  return p;
}

GString *GlobalParams::findFontFile(GString *fontName,
				    const char *ext1, const char *ext2) {
  GString *dir, *fileName;
  FILE *f;
  int i;

  for (i = 0; i < fontDirs->getLength(); ++i) {
    dir = (GString *)fontDirs->get(i);
    if (ext1) {
      fileName = appendToPath(dir->copy(), fontName->getCString());
      fileName->append(ext1);
      if ((f = fopen(fileName->getCString(), "r"))) {
	fclose(f);
	return fileName;
      }
      delete fileName;
    }
    if (ext2) {
      fileName = appendToPath(dir->copy(), fontName->getCString());
      fileName->append(ext2);
      if ((f = fopen(fileName->getCString(), "r"))) {
	fclose(f);
	return fileName;
      }
      delete fileName;
    }
  }
  return NULL;
}

CharCodeToUnicode *GlobalParams::getCIDToUnicode(GString *collection) {
  return cidToUnicodeCache->getCIDToUnicode(collection);
}

UnicodeMap *GlobalParams::getUnicodeMap(GString *encodingName) {
  UnicodeMap *map;

  if ((map = getResidentUnicodeMap(encodingName))) {
    map->incRefCnt();
    return map;
  }
  return unicodeMapCache->getUnicodeMap(encodingName);
}

CMap *GlobalParams::getCMap(GString *collection, GString *cMapName) {
  return cMapCache->getCMap(collection, cMapName);
}

UnicodeMap *GlobalParams::getTextEncoding() {
  return getUnicodeMap(textEncoding);
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void GlobalParams::setPSFile(const char *file) {
  if (psFile) {
    delete psFile;
  }
  psFile = new GString(file);
}

GBool GlobalParams::setPSPaperSize(const char *size) {
  if (!strcmp(size, "letter")) {
    psPaperWidth = 612;
    psPaperHeight = 792;
  } else if (!strcmp(size, "legal")) {
    psPaperWidth = 612;
    psPaperHeight = 1008;
  } else if (!strcmp(size, "A4")) {
    psPaperWidth = 595;
    psPaperHeight = 842;
  } else if (!strcmp(size, "A3")) {
    psPaperWidth = 842;
    psPaperHeight = 1190;
  } else {
    return gFalse;
  }
  return gTrue;
}

void GlobalParams::setPSPaperWidth(int width) {
  psPaperWidth = width;
}

void GlobalParams::setPSPaperHeight(int height) {
  psPaperHeight = height;
}

void GlobalParams::setPSDuplex(GBool duplex) {
  psDuplex = duplex;
}

void GlobalParams::setPSLevel(PSLevel level) {
  psLevel = level;
}

void GlobalParams::setPSEmbedType1(GBool embed) {
  psEmbedType1 = embed;
}

void GlobalParams::setPSEmbedTrueType(GBool embed) {
  psEmbedTrueType = embed;
}

void GlobalParams::setPSEmbedCIDPostScript(GBool embed) {
  psEmbedCIDPostScript = embed;
}

void GlobalParams::setPSEmbedCIDTrueType(GBool embed) {
  psEmbedCIDTrueType = embed;
}

void GlobalParams::setPSOPI(GBool opi) {
  psOPI = opi;
}

void GlobalParams::setPSASCIIHex(GBool hex) {
  psASCIIHex = hex;
}

void GlobalParams::setTextEncoding(const char *encodingName) {
  delete textEncoding;
  textEncoding = new GString(encodingName);
}

GBool GlobalParams::setTextEOL(const char *s) {
  if (!strcmp(s, "unix")) {
    textEOL = eolUnix;
  } else if (!strcmp(s, "dos")) {
    textEOL = eolDOS;
  } else if (!strcmp(s, "mac")) {
    textEOL = eolMac;
  } else {
    return gFalse;
  }
  return gTrue;
}

void GlobalParams::setInitialZoom(const char *s) {
  delete initialZoom;
  initialZoom = new GString(s);
}

GBool GlobalParams::setT1libControl(const char *s) {
  return setFontRastControl(&t1libControl, s);
}

GBool GlobalParams::setFreeTypeControl(const char *s) {
  return setFontRastControl(&freetypeControl, s);
}

GBool GlobalParams::setFontRastControl(FontRastControl *val, const char *s) {
  if (!strcmp(s, "none")) {
    *val = fontRastNone;
  } else if (!strcmp(s, "plain")) {
    *val = fontRastPlain;
  } else if (!strcmp(s, "low")) {
    *val = fontRastAALow;
  } else if (!strcmp(s, "high")) {
    *val = fontRastAAHigh;
  } else {
    return gFalse;
  }
  return gTrue;
}

void GlobalParams::setErrQuiet(GBool errQuietA) {
  errQuiet = errQuietA;
}
