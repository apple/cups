//========================================================================
//
// PSOutputDev.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <signal.h>
#include <math.h>
#include "GString.h"
#include "config.h"
#include "Object.h"
#include "Error.h"
#include "Function.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "FontFile.h"
#include "Catalog.h"
#include "Page.h"
#include "Stream.h"
#include "FormWidget.h"
#include "PSOutputDev.h"

#if JAPANESE_SUPPORT
#include "Japan12ToRKSJ.h"
#endif

#ifdef MACOS
// needed for setting type/creator of MacOS files
#include "ICSupport.h"
#endif

//------------------------------------------------------------------------
// PostScript prolog and setup
//------------------------------------------------------------------------

static char *prolog[] = {
  "/xpdf 75 dict def xpdf begin",
  "% PDF special state",
  "/pdfDictSize 14 def",
  "/pdfSetup {",
  "  2 array astore",
  "  /setpagedevice where {",
  "    pop 3 dict dup begin",
  "      exch /PageSize exch def",
  "      /ImagingBBox null def",
  "      /Policies 1 dict dup begin /PageSize 3 def end def",
  "    end setpagedevice",
  "  } {",
  "    pop",
  "  } ifelse",
  "} def",
  "/pdfStartPage {",
  "  pdfDictSize dict begin",
  "  /pdfFill [0] def",
  "  /pdfStroke [0] def",
  "  /pdfLastFill false def",
  "  /pdfLastStroke false def",
  "  /pdfTextMat [1 0 0 1 0 0] def",
  "  /pdfFontSize 0 def",
  "  /pdfCharSpacing 0 def",
  "  /pdfTextRender 0 def",
  "  /pdfTextRise 0 def",
  "  /pdfWordSpacing 0 def",
  "  /pdfHorizScaling 1 def",
  "} def",
  "/pdfEndPage { end } def",
  "% separation convention operators",
  "/findcmykcustomcolor where {",
  "  pop",
  "}{",
  "  /findcmykcustomcolor { 5 array astore } def",
  "} ifelse",
  "/setcustomcolor where {",
  "  pop",
  "}{",
  "  /setcustomcolor {",
  "    exch",
  "    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
  "      0 4 getinterval cvx",
  "      [ exch /dup load exch { mul exch dup } /forall load",
  "        /pop load dup ] cvx",
  "    ] setcolorspace setcolor",
  "  } def",
  "} ifelse",
  "/customcolorimage where {",
  "  pop",
  "}{",
  "  /customcolorimage {",
  "    gsave",
  "    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
  "      0 4 getinterval cvx",
  "      [ exch /dup load exch { mul exch dup } /forall load",
  "        /pop load dup ] cvx",
  "    ] setcolorspace",
  "    10 dict begin",
  "      /ImageType 1 def",
  "      /DataSource exch def",
  "      /ImageMatrix exch def",
  "      /BitsPerComponent exch def",
  "      /Height exch def",
  "      /Width exch def",
  "      /Decode [1 0] def",
  "    currentdict end",
  "    image",
  "    grestore",
  "  } def",
  "} ifelse",
  "% PDF color state",
  "/sCol {",
  "  pdfLastStroke not {",
  "    pdfStroke aload length",
  "    dup 1 eq {",
  "      pop setgray",
  "    }{",
  "      dup 3 eq {",
  "        pop setrgbcolor",
  "      }{",
  "        4 eq {",
  "          setcmykcolor",
  "        }{",
  "          findcmykcustomcolor exch setcustomcolor",
  "        } ifelse",
  "      } ifelse",
  "    } ifelse",
  "    /pdfLastStroke true def /pdfLastFill false def",
  "  } if",
  "} def",
  "/fCol {",
  "  pdfLastFill not {",
  "    pdfFill aload length",
  "    dup 1 eq {",
  "      pop setgray",
  "    }{",
  "      dup 3 eq {",
  "        pop setrgbcolor",
  "      }{",
  "        4 eq {",
  "          setcmykcolor",
  "        }{",
  "          findcmykcustomcolor exch setcustomcolor",
  "        } ifelse",
  "      } ifelse",
  "    } ifelse",
  "    /pdfLastFill true def /pdfLastStroke false def",
  "  } if",
  "} def",
  "% build a font",
  "/pdfMakeFont {",
  "  4 3 roll findfont",
  "  4 2 roll matrix scale makefont",
  "  dup length dict begin",
  "    { 1 index /FID ne { def } { pop pop } ifelse } forall",
  "    /Encoding exch def",
  "    currentdict",
  "  end",
  "  definefont pop",
  "} def",
  "/pdfMakeFont16 { findfont definefont pop } def",
  "% graphics state operators",
  "/q { gsave pdfDictSize dict begin } def",
  "/Q { end grestore } def",
  "/cm { concat } def",
  "/d { setdash } def",
  "/i { setflat } def",
  "/j { setlinejoin } def",
  "/J { setlinecap } def",
  "/M { setmiterlimit } def",
  "/w { setlinewidth } def",
  "% color operators",
  "/g { dup 1 array astore /pdfFill exch def setgray",
  "     /pdfLastFill true def /pdfLastStroke false def } def",
  "/G { dup 1 array astore /pdfStroke exch def setgray",
  "     /pdfLastStroke true def /pdfLastFill false def } def",
  "/rg { 3 copy 3 array astore /pdfFill exch def setrgbcolor",
  "      /pdfLastFill true def /pdfLastStroke false def } def",
  "/RG { 3 copy 3 array astore /pdfStroke exch def setrgbcolor",
  "      /pdfLastStroke true def /pdfLastFill false def } def",
  "/k { 4 copy 4 array astore /pdfFill exch def setcmykcolor",
  "     /pdfLastFill true def /pdfLastStroke false def } def",
  "/K { 4 copy 4 array astore /pdfStroke exch def setcmykcolor",
  "     /pdfLastStroke true def /pdfLastFill false def } def",
  "/ck { 6 copy 6 array astore /pdfFill exch def",
  "      findcmykcustomcolor exch setcustomcolor",
  "      /pdfLastFill true def /pdfLastStroke false def } def",
  "/CK { 6 copy 6 array astore /pdfStroke exch def",
  "      findcmykcustomcolor exch setcustomcolor",
  "      /pdfLastStroke true def /pdfLastFill false def } def",
  "% path segment operators",
  "/m { moveto } def",
  "/l { lineto } def",
  "/c { curveto } def",
  "/re { 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto",
  "      neg 0 rlineto closepath } def",
  "/h { closepath } def",
  "% path painting operators",
  "/S { sCol stroke } def",
  "/f { fCol fill } def",
  "/f* { fCol eofill } def",
  "% clipping operators",
  "/W { clip newpath } def",
  "/W* { eoclip newpath } def",
  "% text state operators",
  "/Tc { /pdfCharSpacing exch def } def",
  "/Tf { dup /pdfFontSize exch def",
  "      dup pdfHorizScaling mul exch matrix scale",
  "      pdfTextMat matrix concatmatrix dup 4 0 put dup 5 0 put",
  "      exch findfont exch makefont setfont } def",
  "/Tr { /pdfTextRender exch def } def",
  "/Ts { /pdfTextRise exch def } def",
  "/Tw { /pdfWordSpacing exch def } def",
  "/Tz { /pdfHorizScaling exch def } def",
  "% text positioning operators",
  "/Td { pdfTextMat transform moveto } def",
  "/Tm { /pdfTextMat exch def } def",
  "% text string operators",
  "/Tj { pdfTextRender 1 and 0 eq { fCol } { sCol } ifelse",
  "      0 pdfTextRise pdfTextMat dtransform rmoveto",
  "      pdfFontSize mul pdfHorizScaling mul",
  "      1 index stringwidth pdfTextMat idtransform pop",
  "      sub 1 index length dup 0 ne { div } { pop pop 0 } ifelse",
  "      pdfWordSpacing pdfHorizScaling mul 0 pdfTextMat dtransform 32",
  "      4 3 roll pdfCharSpacing pdfHorizScaling mul add 0",
  "      pdfTextMat dtransform",
  "      6 5 roll awidthshow",
  "      0 pdfTextRise neg pdfTextMat dtransform rmoveto } def",
  "/TJm { pdfFontSize 0.001 mul mul neg 0",
  "       pdfTextMat dtransform rmoveto } def",
  "% Level 1 image operators",
  "/pdfIm1 {",
  "  /pdfImBuf1 4 index string def",
  "  { currentfile pdfImBuf1 readhexstring pop } image",
  "} def",
  "/pdfIm1Sep {",
  "  /pdfImBuf1 4 index string def",
  "  /pdfImBuf2 4 index string def",
  "  /pdfImBuf3 4 index string def",
  "  /pdfImBuf4 4 index string def",
  "  { currentfile pdfImBuf1 readhexstring pop }",
  "  { currentfile pdfImBuf2 readhexstring pop }",
  "  { currentfile pdfImBuf3 readhexstring pop }",
  "  { currentfile pdfImBuf4 readhexstring pop }",
  "  true 4 colorimage",
  "} def",
  "/pdfImM1 {",
  "  /pdfImBuf1 4 index 7 add 8 idiv string def",
  "  { currentfile pdfImBuf1 readhexstring pop } imagemask",
  "} def",
  "% Level 2 image operators",
  "/pdfImBuf 100 string def",
  "/pdfIm {",
  "  image",
  "  { currentfile pdfImBuf readline",
  "    not { pop exit } if",
  "    (%-EOD-) eq { exit } if } loop",
  "} def",
  "/pdfImSep {",
  "  findcmykcustomcolor exch",
  "  dup /Width get /pdfImBuf1 exch string def",
  "  begin Width Height BitsPerComponent ImageMatrix DataSource end",
  "  /pdfImData exch def",
  "  { pdfImData pdfImBuf1 readstring pop",
  "    0 1 2 index length 1 sub {",
  "      1 index exch 2 copy get 255 exch sub put",
  "    } for }",
  "  6 5 roll customcolorimage",
  "  { currentfile pdfImBuf readline",
  "    not { pop exit } if",
  "    (%-EOD-) eq { exit } if } loop",
  "} def",
  "/pdfImM {",
  "  fCol imagemask",
  "  { currentfile pdfImBuf readline",
  "    not { pop exit } if",
  "    (%-EOD-) eq { exit } if } loop",
  "} def",
  "end",
  NULL
};

//------------------------------------------------------------------------
// Fonts
//------------------------------------------------------------------------

struct PSFont {
  char *name;			// PDF name
  char *psName;			// PostScript name
};

struct PSSubstFont {
  char *psName;			// PostScript name
  double mWidth;		// width of 'm' character
};

static PSFont psFonts[] = {
  {"Courier",               "Courier"},
  {"Courier-Bold",          "Courier-Bold"},
  {"Courier-Oblique",       "Courier-Oblique"},
  {"Courier-BoldOblique",   "Courier-BoldOblique"},
  {"Helvetica",             "Helvetica"},
  {"Helvetica-Bold",        "Helvetica-Bold"},
  {"Helvetica-Oblique",     "Helvetica-Oblique"},
  {"Helvetica-BoldOblique", "Helvetica-BoldOblique"},
  {"Symbol",                "Symbol"},
  {"Times-Roman",           "Times-Roman"},
  {"Times-Bold",            "Times-Bold"},
  {"Times-Italic",          "Times-Italic"},
  {"Times-BoldItalic",      "Times-BoldItalic"},
  {"ZapfDingbats",          "ZapfDingbats"},
  {NULL}
};

static PSSubstFont psSubstFonts[] = {
  {"Helvetica",             0.833},
  {"Helvetica-Oblique",     0.833},
  {"Helvetica-Bold",        0.889},
  {"Helvetica-BoldOblique", 0.889},
  {"Times-Roman",           0.788},
  {"Times-Italic",          0.722},
  {"Times-Bold",            0.833},
  {"Times-BoldItalic",      0.778},
  {"Courier",               0.600},
  {"Courier-Oblique",       0.600},
  {"Courier-Bold",          0.600},
  {"Courier-BoldOblique",   0.600}
};

//------------------------------------------------------------------------
// process colors
//------------------------------------------------------------------------

#define psProcessCyan     1
#define psProcessMagenta  2
#define psProcessYellow   4
#define psProcessBlack    8
#define psProcessCMYK    15

//------------------------------------------------------------------------
// PSOutCustomColor
//------------------------------------------------------------------------

class PSOutCustomColor {
public:

  PSOutCustomColor(double cA, double mA,
		   double yA, double kA, GString *nameA);
  ~PSOutCustomColor();

  double c, m, y, k;
  GString *name;
  PSOutCustomColor *next;
};

PSOutCustomColor::PSOutCustomColor(double cA, double mA,
				   double yA, double kA, GString *nameA) {
  c = cA;
  m = mA;
  y = yA;
  k = kA;
  name = nameA;
  next = NULL;
}

PSOutCustomColor::~PSOutCustomColor() {
  delete name;
}

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

extern "C" {
typedef void (*SignalFunc)(int);
}

PSOutputDev::PSOutputDev(char *fileName, XRef *xrefA, Catalog *catalog,
			 int firstPage, int lastPage,
			 PSOutLevel levelA, PSOutMode modeA, GBool doOPIA,
			 GBool embedType1A, GBool embedTrueTypeA,
			 int paperWidthA, int paperHeightA) {
  Page *page;
  PDFRectangle *box;
  Dict *resDict;
  FormWidgets *formWidgets;
  char **p;
  int pg;
  Object obj1, obj2;
  int i;

  // initialize
  xref = xrefA;
  level = levelA;
  mode = modeA;
  doOPI = doOPIA;
  embedType1 = embedType1A;
  embedTrueType = embedTrueTypeA;
  paperWidth = paperWidthA;
  paperHeight = paperHeightA;
  fontIDs = NULL;
  fontFileIDs = NULL;
  fontFileNames = NULL;
  embFontList = NULL;
  f = NULL;
  if (mode == psModeForm) {
    lastPage = firstPage;
  }
  processColors = 0;
  customColors = NULL;

  // open file or pipe
  ok = gTrue;
  if (!strcmp(fileName, "-")) {
    fileType = psStdout;
    f = stdout;
  } else if (fileName[0] == '|') {
    fileType = psPipe;
#ifdef HAVE_POPEN
#ifndef WIN32
    signal(SIGPIPE, (SignalFunc)SIG_IGN);
#endif
    if (!(f = popen(fileName + 1, "w"))) {
      error(-1, "Couldn't run print command '%s'", fileName);
      ok = gFalse;
      return;
    }
#else
    error(-1, "Print commands are not supported ('%s')", fileName);
    ok = gFalse;
    return;
#endif
  } else {
    fileType = psFile;
    if (!(f = fopen(fileName, "w"))) {
      error(-1, "Couldn't open PostScript file '%s'", fileName);
      ok = gFalse;
      return;
    }
  }

  // initialize fontIDs, fontFileIDs, and fontFileNames lists
  fontIDSize = 64;
  fontIDLen = 0;
  fontIDs = (Ref *)gmalloc(fontIDSize * sizeof(Ref));
  fontFileIDSize = 64;
  fontFileIDLen = 0;
  fontFileIDs = (Ref *)gmalloc(fontFileIDSize * sizeof(Ref));
  fontFileNameSize = 64;
  fontFileNameLen = 0;
  fontFileNames = (GString **)gmalloc(fontFileNameSize * sizeof(GString *));

  // initialize embedded font resource comment list
  embFontList = new GString();

  // write header
  switch (mode) {
  case psModePS:
    writePS("%%!PS-Adobe-3.0\n");
    writePS("%%%%Creator: xpdf/pdftops %s\n", xpdfVersion);
    writePS("%%%%LanguageLevel: %d\n",
	    (level == psLevel1 || level == psLevel1Sep) ? 1 : 2);
    if (level == psLevel1Sep || level == psLevel2Sep) {
      writePS("%%%%DocumentProcessColors: (atend)\n");
      writePS("%%%%DocumentCustomColors: (atend)\n");
    }
    writePS("%%%%DocumentMedia: plain %d %d 0 () ()\n",
	    paperWidth, paperHeight);
    writePS("%%%%Pages: %d\n", lastPage - firstPage + 1);
    writePS("%%%%EndComments\n");
    writePS("%%%%BeginDefaults\n");
    writePS("%%%%PageMedia: plain\n");
    writePS("%%%%EndDefaults\n");
    break;
  case psModeEPS:
    writePS("%%!PS-Adobe-3.0 EPSF-3.0\n");
    writePS("%%%%Creator: xpdf/pdftops %s\n", xpdfVersion);
    writePS("%%%%LanguageLevel: %d\n",
	    (level == psLevel1 || level == psLevel1Sep) ? 1 : 2);
    if (level == psLevel1Sep || level == psLevel2Sep) {
      writePS("%%%%DocumentProcessColors: (atend)\n");
      writePS("%%%%DocumentCustomColors: (atend)\n");
    }
    page = catalog->getPage(firstPage);
    box = page->getBox();
    writePS("%%%%BoundingBox: %d %d %d %d\n",
	    (int)floor(box->x1), (int)floor(box->y1),
	    (int)ceil(box->x2), (int)ceil(box->y2));
    if (floor(box->x1) != ceil(box->x1) ||
	floor(box->y1) != ceil(box->y1) ||
	floor(box->x2) != ceil(box->x2) ||
	floor(box->y2) != ceil(box->y2)) {
      writePS("%%%%HiResBoundingBox: %g %g %g %g\n",
	      box->x1, box->y1, box->x2, box->y2);
    }
    writePS("%%%%DocumentSuppliedResources: (atend)\n");
    writePS("%%%%EndComments\n");
    break;
  case psModeForm:
    writePS("%%!PS-Adobe-3.0 Resource-Form\n");
    writePS("%%%%Creator: xpdf/pdftops %s\n", xpdfVersion);
    writePS("%%%%LanguageLevel: %d\n",
	    (level == psLevel1 || level == psLevel1Sep) ? 1 : 2);
    if (level == psLevel1Sep || level == psLevel2Sep) {
      writePS("%%%%DocumentProcessColors: (atend)\n");
      writePS("%%%%DocumentCustomColors: (atend)\n");
    }
    writePS("%%%%EndComments\n");
    page = catalog->getPage(firstPage);
    box = page->getBox();
    writePS("32 dict dup begin\n");
    writePS("/BBox [%d %d %d %d] def\n",
	    (int)box->x1, (int)box->y1, (int)box->x2, (int)box->y2);
    writePS("/FormType 1 def\n");
    writePS("/Matrix [1 0 0 1 0 0] def\n");
    break;
  }

  // write prolog
  if (mode != psModeForm) {
    writePS("%%%%BeginProlog\n");
  }
  writePS("%%%%BeginResource: procset xpdf %s 0\n", xpdfVersion);
  for (p = prolog; *p; ++p) {
    writePS("%s\n", *p);
  }
  writePS("%%%%EndResource\n");
  if (mode != psModeForm) {
    writePS("%%%%EndProlog\n");
  }

  // set up fonts and images
  type3Warning = gFalse;
  if (mode == psModeForm) {
    // swap the form and xpdf dicts
    writePS("xpdf end begin dup begin\n");
  } else {
    writePS("%%%%BeginSetup\n");
    writePS("xpdf begin\n");
  }
  for (pg = firstPage; pg <= lastPage; ++pg) {
    page = catalog->getPage(pg);
    if ((resDict = page->getResourceDict())) {
      setupResources(resDict);
    }
    formWidgets = new FormWidgets(xref, page->getAnnots(&obj1));
    obj1.free();
    for (i = 0; i < formWidgets->getNumWidgets(); ++i) {
      if (formWidgets->getWidget(i)->getAppearance(&obj1)->isStream()) {
	obj1.streamGetDict()->lookup("Resources", &obj2);
	if (obj2.isDict()) {
	  setupResources(obj2.getDict());
	}
	obj2.free();
      }
      obj1.free();
    }
    delete formWidgets;
  }
  if (mode != psModeForm) {
#if OPI_SUPPORT
    if (doOPI) {
      writePS("/opiMatrix matrix currentmatrix def\n");
    }
#endif
    if (mode != psModeEPS) {
      writePS("%d %d pdfSetup\n", paperWidth, paperHeight);
    }
    writePS("%%%%EndSetup\n");
  }

  // initialize sequential page number
  seqPage = 1;

#if OPI_SUPPORT
  // initialize OPI nesting levels
  opi13Nest = 0;
  opi20Nest = 0;
#endif
}

PSOutputDev::~PSOutputDev() {
  PSOutCustomColor *cc;
  int i;

  if (f) {
    if (mode == psModeForm) {
      writePS("/Foo exch /Form defineresource pop\n");
    } else  {
      writePS("%%%%Trailer\n");
      writePS("end\n");
      writePS("%%%%DocumentSuppliedResources:\n");
      writePS("%s", embFontList->getCString());
      if (level == psLevel1Sep || level == psLevel2Sep) {
         writePS("%%%%DocumentProcessColors:");
         if (processColors & psProcessCyan) {
	   writePS(" Cyan");
	 }
         if (processColors & psProcessMagenta) {
	   writePS(" Magenta");
	 }
         if (processColors & psProcessYellow) {
	   writePS(" Yellow");
	 }
         if (processColors & psProcessBlack) {
	   writePS(" Black");
	 }
         writePS("\n");
         writePS("%%%%DocumentCustomColors:");
	 for (cc = customColors; cc; cc = cc->next) {
	   writePS(" (%s)", cc->name->getCString());
	 }
         writePS("\n");
         writePS("%%%%CMYKCustomColor:\n");
	 for (cc = customColors; cc; cc = cc->next) {
	   writePS("%%%%+ %g %g %g %g (%s)\n",
		   cc->c, cc->m, cc->y, cc->k, cc->name->getCString());
	 }
      }
      writePS("%%%%EOF\n");
    }
    if (fileType == psFile) {
#ifdef MACOS
      ICS_MapRefNumAndAssign((short)f->handle);
#endif
      fclose(f);
    }
#ifdef HAVE_POPEN
    else if (fileType == psPipe) {
      pclose(f);
#ifndef WIN32
      signal(SIGPIPE, (SignalFunc)SIG_DFL);
#endif
    }
#endif
  }
  if (embFontList) {
    delete embFontList;
  }
  if (fontIDs) {
    gfree(fontIDs);
  }
  if (fontFileIDs) {
    gfree(fontFileIDs);
  }
  if (fontFileNames) {
    for (i = 0; i < fontFileNameLen; ++i) {
      delete fontFileNames[i];
    }
    gfree(fontFileNames);
  }
  while (customColors) {
    cc = customColors;
    customColors = cc->next;
    delete cc;
  }
}

void PSOutputDev::setupResources(Dict *resDict) {
  Object xObjDict, xObj, resObj;
  int i;

  setupFonts(resDict);
  setupImages(resDict);

  resDict->lookup("XObject", &xObjDict);
  if (xObjDict.isDict()) {
    for (i = 0; i < xObjDict.dictGetLength(); ++i) {
      xObjDict.dictGetVal(i, &xObj);
      if (xObj.isStream()) {
	xObj.streamGetDict()->lookup("Resources", &resObj);
	if (resObj.isDict()) {
	  setupResources(resObj.getDict());
	}
	resObj.free();
      }
      xObj.free();
    }
  }
  xObjDict.free();
}

void PSOutputDev::setupFonts(Dict *resDict) {
  Object fontDict;
  GfxFontDict *gfxFontDict;
  GfxFont *font;
  int i;

  resDict->lookup("Font", &fontDict);
  if (fontDict.isDict()) {
    gfxFontDict = new GfxFontDict(xref, fontDict.getDict());
    for (i = 0; i < gfxFontDict->getNumFonts(); ++i) {
      font = gfxFontDict->getFont(i);
      setupFont(font);
    }
    delete gfxFontDict;
  }
  fontDict.free();
}

void PSOutputDev::setupFont(GfxFont *font) {
  Ref fontFileID;
  GString *name;
  char *psName;
  char *charName;
  double xs, ys;
  GBool do16Bit;
  int code;
  double w1, w2;
  double *fm;
  int i, j;

  // check if font is already set up
  for (i = 0; i < fontIDLen; ++i) {
    if (fontIDs[i].num == font->getID().num &&
	fontIDs[i].gen == font->getID().gen)
      return;
  }

  // add entry to fontIDs list
  if (fontIDLen >= fontIDSize) {
    fontIDSize += 64;
    fontIDs = (Ref *)grealloc(fontIDs, fontIDSize * sizeof(Ref));
  }
  fontIDs[fontIDLen++] = font->getID();

  xs = ys = 1;
  do16Bit = gFalse;

  // check for embedded Type 1 font
  if (embedType1 && font->getType() == fontType1 &&
      font->getEmbeddedFontID(&fontFileID)) {
    psName = font->getEmbeddedFontName();
    setupEmbeddedType1Font(&fontFileID, psName);

  // check for external Type 1 font file
  } else if (embedType1 && font->getType() == fontType1 &&
	     font->getExtFontFile()) {
    // this assumes that the PS font name matches the PDF font name
    psName = font->getName()->getCString();
    setupEmbeddedType1Font(font->getExtFontFile(), psName);

  // check for embedded Type 1C font
  } else if (embedType1 && font->getType() == fontType1C &&
	     font->getEmbeddedFontID(&fontFileID)) {
    psName = font->getEmbeddedFontName();
    setupEmbeddedType1CFont(font, &fontFileID, psName);

  // check for embedded TrueType font
  } else if (embedTrueType && font->getType() == fontTrueType &&
	     font->getEmbeddedFontID(&fontFileID)) {
    psName = font->getEmbeddedFontName();
    setupEmbeddedTrueTypeFont(font, &fontFileID, psName);

  // check for Japanese font
  } else if (font->is16Bit() && font->getCharSet16() == font16AdobeJapan12) {
    psName = "Ryumin-Light-RKSJ";
    do16Bit = gTrue;

  // do font substitution
  } else {
    if (!type3Warning && font->getType() == fontType3) {
      error(-1, "This document uses Type 3 fonts - some text may not be correctly printed");
      type3Warning = gTrue;
    }
    name = font->getName();
    psName = NULL;
    if (name) {
      for (i = 0; psFonts[i].name; ++i) {
	if (name->cmp(psFonts[i].name) == 0) {
	  psName = psFonts[i].psName;
	  break;
	}
      }
    }
    if (!psName) {
      if (font->isFixedWidth())
	i = 8;
      else if (font->isSerif())
	i = 4;
      else
	i = 0;
      if (font->isBold())
	i += 2;
      if (font->isItalic())
	i += 1;
      psName = psSubstFonts[i].psName;
      if ((code = font->getCharCode("m")) >= 0) {
	w1 = font->getWidth(code);
      } else {
	w1 = 0;
      }
      w2 = psSubstFonts[i].mWidth;
      xs = w1 / w2;
      if (xs < 0.1) {
	xs = 1;
      }
      if (font->getType() == fontType3) {
	// This is a hack which makes it possible to substitute for some
	// Type 3 fonts.  The problem is that it's impossible to know what
	// the base coordinate system used in the font is without actually
	// rendering the font.
	ys = xs;
	fm = font->getFontMatrix();
	if (fm[0] != 0) {
	  ys *= fm[3] / fm[0];
	}
      } else {
	ys = 1;
      }
    }
  }

  // generate PostScript code to set up the font
  if (do16Bit) {
    writePS("/F%d_%d /%s pdfMakeFont16\n",
	    font->getID().num, font->getID().gen, psName);
  } else {
    writePS("/F%d_%d /%s %g %g\n",
	    font->getID().num, font->getID().gen, psName, xs, ys);
    for (i = 0; i < 256; i += 8) {
      writePS((i == 0) ? "[ " : "  ");
      for (j = 0; j < 8; ++j) {
	charName = font->getCharName(i+j);
	// this is a kludge for broken PDF files that encode char 32
	// as .notdef
	if (i+j == 32 && charName && !strcmp(charName, ".notdef")) {
	  charName = "space";
	}
	writePS("/%s", charName ? charName : ".notdef");
      }
      writePS((i == 256-8) ? "]\n" : "\n");
    }
    writePS("pdfMakeFont\n");
  }
}

void PSOutputDev::setupEmbeddedType1Font(Ref *id, char *psName) {
  static char hexChar[17] = "0123456789abcdef";
  Object refObj, strObj, obj1, obj2;
  Dict *dict;
  int length1, length2;
  int c;
  int start[4];
  GBool binMode;
  int i;

  // check if font is already embedded
  for (i = 0; i < fontFileIDLen; ++i) {
    if (fontFileIDs[i].num == id->num &&
	fontFileIDs[i].gen == id->gen)
      return;
  }

  // add entry to fontFileIDs list
  if (fontFileIDLen >= fontFileIDSize) {
    fontFileIDSize += 64;
    fontFileIDs = (Ref *)grealloc(fontFileIDs, fontFileIDSize * sizeof(Ref));
  }
  fontFileIDs[fontFileIDLen++] = *id;

  // get the font stream and info
  refObj.initRef(id->num, id->gen);
  refObj.fetch(xref, &strObj);
  refObj.free();
  if (!strObj.isStream()) {
    error(-1, "Embedded font file object is not a stream");
    goto err1;
  }
  if (!(dict = strObj.streamGetDict())) {
    error(-1, "Embedded font stream is missing its dictionary");
    goto err1;
  }
  dict->lookup("Length1", &obj1);
  dict->lookup("Length2", &obj2);
  if (!obj1.isInt() || !obj2.isInt()) {
    error(-1, "Missing length fields in embedded font stream dictionary");
    obj1.free();
    obj2.free();
    goto err1;
  }
  length1 = obj1.getInt();
  length2 = obj2.getInt();
  obj1.free();
  obj2.free();

  // beginning comment
  writePS("%%%%BeginResource: font %s\n", psName);
  embFontList->append("%%+ font ");
  embFontList->append(psName);
  embFontList->append("\n");

  // copy ASCII portion of font
  strObj.streamReset();
  for (i = 0; i < length1 && (c = strObj.streamGetChar()) != EOF; ++i)
    fputc(c, f);

  // figure out if encrypted portion is binary or ASCII
  binMode = gFalse;
  for (i = 0; i < 4; ++i) {
    start[i] = strObj.streamGetChar();
    if (start[i] == EOF) {
      error(-1, "Unexpected end of file in embedded font stream");
      goto err1;
    }
    if (!((start[i] >= '0' && start[i] <= '9') ||
	  (start[i] >= 'A' && start[i] <= 'F') ||
	  (start[i] >= 'a' && start[i] <= 'f')))
      binMode = gTrue;
  }

  // convert binary data to ASCII
  if (binMode) {
    for (i = 0; i < 4; ++i) {
      fputc(hexChar[(start[i] >> 4) & 0x0f], f);
      fputc(hexChar[start[i] & 0x0f], f);
    }
    while (i < length2) {
      if ((c = strObj.streamGetChar()) == EOF)
	break;
      fputc(hexChar[(c >> 4) & 0x0f], f);
      fputc(hexChar[c & 0x0f], f);
      if (++i % 32 == 0)
	fputc('\n', f);
    }
    if (i % 32 > 0)
      fputc('\n', f);

  // already in ASCII format -- just copy it
  } else {
    for (i = 0; i < 4; ++i)
      fputc(start[i], f);
    for (i = 4; i < length2; ++i) {
      if ((c = strObj.streamGetChar()) == EOF)
	break;
      fputc(c, f);
    }
  }

  // write padding and "cleartomark"
  for (i = 0; i < 8; ++i)
    writePS("00000000000000000000000000000000"
	    "00000000000000000000000000000000\n");
  writePS("cleartomark\n");

  // ending comment
  writePS("%%%%EndResource\n");

 err1:
  strObj.streamClose();
  strObj.free();
}

//~ This doesn't handle .pfb files or binary eexec data (which only
//~ happens in pfb files?).
void PSOutputDev::setupEmbeddedType1Font(GString *fileName, char *psName) {
  FILE *fontFile;
  int c;
  int i;

  // check if font is already embedded
  for (i = 0; i < fontFileNameLen; ++i) {
    if (!fontFileNames[i]->cmp(fileName)) {
      return;
    }
  }

  // add entry to fontFileNames list
  if (fontFileNameLen >= fontFileNameSize) {
    fontFileNameSize += 64;
    fontFileNames = (GString **)grealloc(fontFileNames,
					 fontFileNameSize * sizeof(GString *));
  }
  fontFileNames[fontFileNameLen++] = fileName->copy();

  // beginning comment
  writePS("%%%%BeginResource: font %s\n", psName);
  embFontList->append("%%+ font ");
  embFontList->append(psName);
  embFontList->append("\n");

  // copy the font file
  if (!(fontFile = fopen(fileName->getCString(), "rb"))) {
    error(-1, "Couldn't open external font file");
    return;
  }
  while ((c = fgetc(fontFile)) != EOF)
    fputc(c, f);
  fclose(fontFile);

  // ending comment
  writePS("%%%%EndResource\n");
}

void PSOutputDev::setupEmbeddedType1CFont(GfxFont *font, Ref *id,
					  char *psName) {
  char *fontBuf;
  int fontLen;
  Type1CFontConverter *cvt;
  int i;

  // check if font is already embedded
  for (i = 0; i < fontFileIDLen; ++i) {
    if (fontFileIDs[i].num == id->num &&
	fontFileIDs[i].gen == id->gen)
      return;
  }

  // add entry to fontFileIDs list
  if (fontFileIDLen >= fontFileIDSize) {
    fontFileIDSize += 64;
    fontFileIDs = (Ref *)grealloc(fontFileIDs, fontFileIDSize * sizeof(Ref));
  }
  fontFileIDs[fontFileIDLen++] = *id;

  // beginning comment
  writePS("%%%%BeginResource: font %s\n", psName);
  embFontList->append("%%+ font ");
  embFontList->append(psName);
  embFontList->append("\n");

  // convert it to a Type 1 font
  fontBuf = font->readEmbFontFile(xref, &fontLen);
  cvt = new Type1CFontConverter(fontBuf, fontLen, f);
  cvt->convert();
  delete cvt;
  gfree(fontBuf);

  // ending comment
  writePS("%%%%EndResource\n");
}

void PSOutputDev::setupEmbeddedTrueTypeFont(GfxFont *font, Ref *id,
					    char *psName) {
  char *fontBuf;
  int fontLen;
  TrueTypeFontFile *ttFile;
  int i;

  // check if font is already embedded
  for (i = 0; i < fontFileIDLen; ++i) {
    if (fontFileIDs[i].num == id->num &&
	fontFileIDs[i].gen == id->gen)
      return;
  }

  // add entry to fontFileIDs list
  if (fontFileIDLen >= fontFileIDSize) {
    fontFileIDSize += 64;
    fontFileIDs = (Ref *)grealloc(fontFileIDs, fontFileIDSize * sizeof(Ref));
  }
  fontFileIDs[fontFileIDLen++] = *id;

  // beginning comment
  writePS("%%%%BeginResource: font %s\n", psName);
  embFontList->append("%%+ font ");
  embFontList->append(psName);
  embFontList->append("\n");

  // convert it to a Type 42 font
  fontBuf = font->readEmbFontFile(xref, &fontLen);
  ttFile = new TrueTypeFontFile(fontBuf, fontLen);
  ttFile->convertToType42(psName, font->getEncoding(), f);
  delete ttFile;
  gfree(fontBuf);

  // ending comment
  writePS("%%%%EndResource\n");
}

void PSOutputDev::setupImages(Dict *resDict) {
  Object xObjDict, xObj, xObjRef, subtypeObj;
  int i;

  if (mode != psModeForm) {
    return;
  }

  resDict->lookup("XObject", &xObjDict);
  if (xObjDict.isDict()) {
    for (i = 0; i < xObjDict.dictGetLength(); ++i) {
      xObjDict.dictGetValNF(i, &xObjRef);
      xObjDict.dictGetVal(i, &xObj);
      if (xObj.isStream()) {
	xObj.streamGetDict()->lookup("Subtype", &subtypeObj);
	if (subtypeObj.isName("Image")) {
	  if (xObjRef.isRef()) {
	    setupImage(xObjRef.getRef(), xObj.getStream());
	  } else {
	    error(-1, "Image in resource dict is not an indirect reference");
	  }
	}
	subtypeObj.free();
      }
      xObj.free();
      xObjRef.free();
    }
  }
  xObjDict.free();
}

void PSOutputDev::setupImage(Ref id, Stream *str) {
  int c;
  int size, line, col, i;

  // construct an encoder stream
  str = new ASCII85Encoder(str);

  // compute image data size
  str->reset();
  col = size = 0;
  do {
    do {
      c = str->getChar();
    } while (c == '\n' || c == '\r');
    if (c == '~' || c == EOF) {
      break;
    }
    if (c == 'z') {
      ++col;
    } else {
      ++col;
      for (i = 1; i <= 4; ++i) {
	do {
	  c = str->getChar();
	} while (c == '\n' || c == '\r');
	if (c == '~' || c == EOF) {
	  break;
	}
	++col;
      }
    }
    if (col > 225) {
      ++size;
      col = 0;
    }
  } while (c != '~' && c != EOF);
  ++size;
  writePS("%d array dup /ImData_%d_%d exch def\n", size, id.num, id.gen);

  // write the data into the array
  str->reset();
  line = col = 0;
  writePS("dup 0 <~");
  do {
    do {
      c = str->getChar();
    } while (c == '\n' || c == '\r');
    if (c == '~' || c == EOF) {
      break;
    }
    if (c == 'z') {
      fputc(c, f);
      ++col;
    } else {
      fputc(c, f);
      ++col;
      for (i = 1; i <= 4; ++i) {
	do {
	  c = str->getChar();
	} while (c == '\n' || c == '\r');
	if (c == '~' || c == EOF) {
	  break;
	}
	fputc(c, f);
	++col;
      }
    }
    // each line is: "dup nnnnn <~...data...~> put<eol>"
    // so max data length = 255 - 20 = 235
    // chunks are 1 or 4 bytes each, so we have to stop at 232
    // but make it 225 just to be safe
    if (col > 225) {
      writePS("~> put\n");
      ++line;
      writePS("dup %d <~", line);
      col = 0;
    }
  } while (c != '~' && c != EOF);
  writePS("~> put\n");
  writePS("pop\n");

  delete str;
}

void PSOutputDev::startPage(int pageNum, GfxState *state) {
  int x1, y1, x2, y2, width, height, t;

  switch (mode) {

  case psModePS:
    writePS("%%%%Page: %d %d\n", pageNum, seqPage);
    writePS("%%%%BeginPageSetup\n");

    // rotate, translate, and scale page
    x1 = (int)(state->getX1() + 0.5);
    y1 = (int)(state->getY1() + 0.5);
    x2 = (int)(state->getX2() + 0.5);
    y2 = (int)(state->getY2() + 0.5);
    width = x2 - x1;
    height = y2 - y1;
    if (width > height && width > paperWidth) {
      landscape = gTrue;
      writePS("%%%%PageOrientation: Landscape\n");
      writePS("pdfStartPage\n");
      writePS("90 rotate\n");
      tx = -x1;
      ty = -(y1 + paperWidth);
      t = width;
      width = height;
      height = t;
    } else {
      landscape = gFalse;
      writePS("%%%%PageOrientation: Portrait\n");
      writePS("pdfStartPage\n");
      tx = -x1;
      ty = -y1;
    }
    if (width < paperWidth) {
      tx += (paperWidth - width) / 2;
    }
    if (height < paperHeight) {
      ty += (paperHeight - height) / 2;
    }
    if (tx != 0 || ty != 0) {
      writePS("%g %g translate\n", tx, ty);
    }
    if (width > paperWidth || height > paperHeight) {
      xScale = (double)paperWidth / (double)width;
      yScale = (double)paperHeight / (double)height;
      if (yScale < xScale) {
	xScale = yScale;
      }
      writePS("%0.4f %0.4f scale\n", xScale, xScale);
    } else {
      xScale = yScale = 1;
    }

    writePS("%%%%EndPageSetup\n");
    ++seqPage;
    break;

  case psModeEPS:
    writePS("pdfStartPage\n");
    tx = ty = 0;
    xScale = yScale = 1;
    landscape = gFalse;
    break;

  case psModeForm:
    writePS("/PaintProc {\n");
    writePS("begin xpdf begin\n");
    writePS("pdfStartPage\n");
    tx = ty = 0;
    xScale = yScale = 1;
    landscape = gFalse;
    break;
  }
}

void PSOutputDev::endPage() {
  if (mode == psModeForm) {
    writePS("pdfEndPage\n");
    writePS("end end\n");
    writePS("} def\n");
    writePS("end end\n");
  } else {
    writePS("showpage\n");
    writePS("%%%%PageTrailer\n");
    writePS("pdfEndPage\n");
  }
}

void PSOutputDev::saveState(GfxState *state) {
  writePS("q\n");
}

void PSOutputDev::restoreState(GfxState *state) {
  writePS("Q\n");
}

void PSOutputDev::updateCTM(GfxState *state, double m11, double m12,
			    double m21, double m22, double m31, double m32) {
  writePS("[%g %g %g %g %g %g] cm\n", m11, m12, m21, m22, m31, m32);
}

void PSOutputDev::updateLineDash(GfxState *state) {
  double *dash;
  double start;
  int length, i;

  state->getLineDash(&dash, &length, &start);
  writePS("[");
  for (i = 0; i < length; ++i)
    writePS("%g%s", dash[i], (i == length-1) ? "" : " ");
  writePS("] %g d\n", start);
}

void PSOutputDev::updateFlatness(GfxState *state) {
  writePS("%d i\n", state->getFlatness());
}

void PSOutputDev::updateLineJoin(GfxState *state) {
  writePS("%d j\n", state->getLineJoin());
}

void PSOutputDev::updateLineCap(GfxState *state) {
  writePS("%d J\n", state->getLineCap());
}

void PSOutputDev::updateMiterLimit(GfxState *state) {
  writePS("%g M\n", state->getMiterLimit());
}

void PSOutputDev::updateLineWidth(GfxState *state) {
  writePS("%g w\n", state->getLineWidth());
}

void PSOutputDev::updateFillColor(GfxState *state) {
  GfxColor color;
  double gray;
  GfxRGB rgb;
  GfxCMYK cmyk;
  GfxSeparationColorSpace *sepCS;

  switch (level) {
  case psLevel1:
    state->getFillGray(&gray);
    writePS("%g g\n", gray);
    break;
  case psLevel1Sep:
    state->getFillCMYK(&cmyk);
    writePS("%g %g %g %g k\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    break;
  case psLevel2:
    if (state->getFillColorSpace()->getMode() == csDeviceCMYK) {
      state->getFillCMYK(&cmyk);
      writePS("%g %g %g %g k\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    } else {
      state->getFillRGB(&rgb);
      if (rgb.r == rgb.g && rgb.g == rgb.b) {
	writePS("%g g\n", rgb.r);
      } else {
	writePS("%g %g %g rg\n", rgb.r, rgb.g, rgb.b);
      }
    }
    break;
  case psLevel2Sep:
    if (state->getFillColorSpace()->getMode() == csSeparation) {
      sepCS = (GfxSeparationColorSpace *)state->getFillColorSpace();
      color.c[0] = 1;
      sepCS->getCMYK(&color, &cmyk);
      writePS("%g %g %g %g %g (%s) ck\n",
	      state->getFillColor()->c[0],
	      cmyk.c, cmyk.m, cmyk.y, cmyk.k,
	      sepCS->getName()->getCString());
      addCustomColor(sepCS);
    } else {
      state->getFillCMYK(&cmyk);
      writePS("%g %g %g %g k\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
      addProcessColor(cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    }
    break;
  }
}

void PSOutputDev::updateStrokeColor(GfxState *state) {
  GfxColor color;
  double gray;
  GfxRGB rgb;
  GfxCMYK cmyk;
  GfxSeparationColorSpace *sepCS;

  switch (level) {
  case psLevel1:
    state->getStrokeGray(&gray);
    writePS("%g G\n", gray);
    break;
  case psLevel1Sep:
    state->getStrokeCMYK(&cmyk);
    writePS("%g %g %g %g K\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    break;
  case psLevel2:
    if (state->getStrokeColorSpace()->getMode() == csDeviceCMYK) {
      state->getStrokeCMYK(&cmyk);
      writePS("%g %g %g %g K\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    } else {
      state->getStrokeRGB(&rgb);
      if (rgb.r == rgb.g && rgb.g == rgb.b) {
	writePS("%g G\n", rgb.r);
      } else {
	writePS("%g %g %g RG\n", rgb.r, rgb.g, rgb.b);
      }
    }
    break;
  case psLevel2Sep:
    if (state->getStrokeColorSpace()->getMode() == csSeparation) {
      sepCS = (GfxSeparationColorSpace *)state->getStrokeColorSpace();
      color.c[0] = 1;
      sepCS->getCMYK(&color, &cmyk);
      writePS("%g %g %g %g %g (%s) CK\n",
	      state->getStrokeColor()->c[0],
	      cmyk.c, cmyk.m, cmyk.y, cmyk.k,
	      sepCS->getName()->getCString());
      addCustomColor(sepCS);
    } else {
      state->getStrokeCMYK(&cmyk);
      writePS("%g %g %g %g K\n", cmyk.c, cmyk.m, cmyk.y, cmyk.k);
      addProcessColor(cmyk.c, cmyk.m, cmyk.y, cmyk.k);
    }
    break;
  }
}

void PSOutputDev::addProcessColor(double c, double m, double y, double k) {
  if (c > 0) {
    processColors |= psProcessCyan;
  }
  if (m > 0) {
    processColors |= psProcessMagenta;
  }
  if (y > 0) {
    processColors |= psProcessYellow;
  }
  if (k > 0) {
    processColors |= psProcessBlack;
  }
}

void PSOutputDev::addCustomColor(GfxSeparationColorSpace *sepCS) {
  PSOutCustomColor *cc;
  GfxColor color;
  GfxCMYK cmyk;

  for (cc = customColors; cc; cc = cc->next) {
    if (!cc->name->cmp(sepCS->getName())) {
      return;
    }
  }
  color.c[0] = 1;
  sepCS->getCMYK(&color, &cmyk);
  cc = new PSOutCustomColor(cmyk.c, cmyk.m, cmyk.y, cmyk.k,
			    sepCS->getName()->copy());
  cc->next = customColors;
  customColors = cc;
}

void PSOutputDev::updateFont(GfxState *state) {
  if (state->getFont()) {
    writePS("/F%d_%d %g Tf\n",
	    state->getFont()->getID().num, state->getFont()->getID().gen,
	    state->getFontSize());
  }
}

void PSOutputDev::updateTextMat(GfxState *state) {
  double *mat;

  mat = state->getTextMat();
  writePS("[%g %g %g %g %g %g] Tm\n",
	  mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
}

void PSOutputDev::updateCharSpace(GfxState *state) {
  writePS("%g Tc\n", state->getCharSpace());
}

void PSOutputDev::updateRender(GfxState *state) {
  writePS("%d Tr\n", state->getRender());
}

void PSOutputDev::updateRise(GfxState *state) {
  writePS("%g Ts\n", state->getRise());
}

void PSOutputDev::updateWordSpace(GfxState *state) {
  writePS("%g Tw\n", state->getWordSpace());
}

void PSOutputDev::updateHorizScaling(GfxState *state) {
  writePS("%g Tz\n", state->getHorizScaling());
}

void PSOutputDev::updateTextPos(GfxState *state) {
  writePS("%g %g Td\n", state->getLineX(), state->getLineY());
}

void PSOutputDev::updateTextShift(GfxState *state, double shift) {
  writePS("%g TJm\n", shift);
}

void PSOutputDev::stroke(GfxState *state) {
  doPath(state->getPath());
  writePS("S\n");
}

void PSOutputDev::fill(GfxState *state) {
  doPath(state->getPath());
  writePS("f\n");
}

void PSOutputDev::eoFill(GfxState *state) {
  doPath(state->getPath());
  writePS("f*\n");
}

void PSOutputDev::clip(GfxState *state) {
  doPath(state->getPath());
  writePS("W\n");
}

void PSOutputDev::eoClip(GfxState *state) {
  doPath(state->getPath());
  writePS("W*\n");
}

void PSOutputDev::doPath(GfxPath *path) {
  GfxSubpath *subpath;
  double x0, y0, x1, y1, x2, y2, x3, y3, x4, y4;
  int n, m, i, j;

  n = path->getNumSubpaths();

  if (n == 1 && path->getSubpath(0)->getNumPoints() == 5) {
    subpath = path->getSubpath(0);
    x0 = subpath->getX(0);
    y0 = subpath->getY(0);
    x4 = subpath->getX(4);
    y4 = subpath->getY(4);
    if (x4 == x0 && y4 == y0) {
      x1 = subpath->getX(1);
      y1 = subpath->getY(1);
      x2 = subpath->getX(2);
      y2 = subpath->getY(2);
      x3 = subpath->getX(3);
      y3 = subpath->getY(3);
      if (x0 == x1 && x2 == x3 && y0 == y3 && y1 == y2) {
	writePS("%g %g %g %g re\n",
		x0 < x2 ? x0 : x2, y0 < y1 ? y0 : y1,
		fabs(x2 - x0), fabs(y1 - y0));
	return;
      } else if (x0 == x3 && x1 == x2 && y0 == y1 && y2 == y3) {
	writePS("%g %g %g %g re\n",
		x0 < x1 ? x0 : x1, y0 < y2 ? y0 : y2,
		fabs(x1 - x0), fabs(y2 - y0));
	return;
      }
    }
  }

  for (i = 0; i < n; ++i) {
    subpath = path->getSubpath(i);
    m = subpath->getNumPoints();
    writePS("%g %g m\n", subpath->getX(0), subpath->getY(0));
    j = 1;
    while (j < m) {
      if (subpath->getCurve(j)) {
	writePS("%g %g %g %g %g %g c\n", subpath->getX(j), subpath->getY(j),
		subpath->getX(j+1), subpath->getY(j+1),
		subpath->getX(j+2), subpath->getY(j+2));
	j += 3;
      } else {
	writePS("%g %g l\n", subpath->getX(j), subpath->getY(j));
	++j;
      }
    }
    if (subpath->isClosed()) {
      writePS("h\n");
    }
  }
}

void PSOutputDev::drawString(GfxState *state, GString *s) {
  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  writePSString(s);
  writePS(" %g Tj\n", state->getFont()->getWidth(s));
}

void PSOutputDev::drawString16(GfxState *state, GString *s) {
  int c1, c2;
  double w;
  int i;

  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  switch (state->getFont()->getCharSet16()) {

  case font16AdobeJapan12:
#if JAPANESE_SUPPORT
    writePS("<");
    w = 0;
    for (i = 0; i < s->getLength(); i += 2) {
      c1 = ((s->getChar(i) & 0xff) << 8) + (s->getChar(i+1) & 0xff);
      if (c1 <= 8285) {
	c2 = japan12ToRKSJ[c1];
      } else {
	c2 = 0x20;
      }
      if (c2 <= 0xff) {
	writePS("%02x", c2);
      } else {
	writePS("%02x%02x", c2 >> 8, c2 & 0xff);
      }
      w += state->getFont()->getWidth16(c1);
    }
    writePS("> %g Tj\n", w);
#endif
    break;

  case font16AdobeGB12:
    break;

  case font16AdobeCNS13:
    break;
  }
}

void PSOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
				int width, int height, GBool invert,
				GBool inlineImg) {
  int len;

  len = height * ((width + 7) / 8);
  if (level == psLevel1 || level == psLevel1Sep) {
    doImageL1(NULL, invert, inlineImg, str, width, height, len);
  } else {
    doImageL2(ref, NULL, invert, inlineImg, str, width, height, len);
  }
}

void PSOutputDev::drawImage(GfxState *state, Object *ref, Stream *str,
			    int width, int height, GfxImageColorMap *colorMap,
			    int *maskColors, GBool inlineImg) {
  int len;

  len = height * ((width * colorMap->getNumPixelComps() *
		   colorMap->getBits() + 7) / 8);
  switch (level) {
  case psLevel1:
    doImageL1(colorMap, gFalse, inlineImg, str, width, height, len);
    break;
  case psLevel1Sep:
    //~ handle indexed, separation, ... color spaces
    doImageL1Sep(colorMap, gFalse, inlineImg, str, width, height, len);
    break;
  case psLevel2:
  case psLevel2Sep:
    doImageL2(ref, colorMap, gFalse, inlineImg, str, width, height, len);
    break;
  }
}

void PSOutputDev::doImageL1(GfxImageColorMap *colorMap,
			    GBool invert, GBool inlineImg,
			    Stream *str, int width, int height, int len) {
  ImageStream *imgStr;
  Guchar pixBuf[gfxColorMaxComps];
  double gray;
  int x, y, i;

  // width, height, matrix, bits per component
  if (colorMap) {
    writePS("%d %d 8 [%d 0 0 %d 0 %d] pdfIm1\n",
	    width, height,
	    width, -height, height);
  } else {
    writePS("%d %d %s [%d 0 0 %d 0 %d] pdfImM1\n",
	    width, height, invert ? "true" : "false",
	    width, -height, height);
  }

  // image
  if (colorMap) {

    // set up to process the data stream
    imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
			     colorMap->getBits());
    imgStr->reset();

    // process the data stream
    i = 0;
    for (y = 0; y < height; ++y) {

      // write the line
      for (x = 0; x < width; ++x) {
	imgStr->getPixel(pixBuf);
	colorMap->getGray(pixBuf, &gray);
	fprintf(f, "%02x", (int)(gray * 255 + 0.5));
	if (++i == 32) {
	  fputc('\n', f);
	  i = 0;
	}
      }
    }
    if (i != 0)
      fputc('\n', f);
    delete imgStr;

  // imagemask
  } else {
    str->reset();
    i = 0;
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; x += 8) {
	fprintf(f, "%02x", str->getChar() & 0xff);
	if (++i == 32) {
	  fputc('\n', f);
	  i = 0;
	}
      }
    }
    if (i != 0)
      fputc('\n', f);
  }
}

void PSOutputDev::doImageL1Sep(GfxImageColorMap *colorMap,
			       GBool invert, GBool inlineImg,
			       Stream *str, int width, int height, int len) {
  ImageStream *imgStr;
  Guchar *lineBuf;
  Guchar pixBuf[gfxColorMaxComps];
  GfxCMYK cmyk;
  int x, y, i, comp;

  // width, height, matrix, bits per component
  writePS("%d %d 8 [%d 0 0 %d 0 %d] pdfIm1Sep\n",
	    width, height,
	    width, -height, height);

  // allocate a line buffer
  lineBuf = (Guchar *)gmalloc(4 * width);

  // set up to process the data stream
  imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
			   colorMap->getBits());
  imgStr->reset();

  // process the data stream
  i = 0;
  for (y = 0; y < height; ++y) {

    // read the line
    for (x = 0; x < width; ++x) {
      imgStr->getPixel(pixBuf);
      colorMap->getCMYK(pixBuf, &cmyk);
      lineBuf[4*x+0] = (int)(255 * cmyk.c + 0.5);
      lineBuf[4*x+1] = (int)(255 * cmyk.m + 0.5);
      lineBuf[4*x+2] = (int)(255 * cmyk.y + 0.5);
      lineBuf[4*x+3] = (int)(255 * cmyk.k + 0.5);
    }

    // write one line of each color component
    for (comp = 0; comp < 4; ++comp) {
      for (x = 0; x < width; ++x) {
	fprintf(f, "%02x", lineBuf[4*x + comp]);
	if (++i == 32) {
	  fputc('\n', f);
	  i = 0;
	}
      }
    }
  }

  if (i != 0) {
    fputc('\n', f);
  }

  delete imgStr;
  gfree(lineBuf);
}

void PSOutputDev::doImageL2(Object *ref, GfxImageColorMap *colorMap,
			    GBool invert, GBool inlineImg,
			    Stream *str, int width, int height, int len) {
  GString *s;
  int n, numComps;
  GBool useRLE, useA85;
  GfxSeparationColorSpace *sepCS;
  GfxColor color;
  GfxCMYK cmyk;
  int c;
  int i;

  // color space
  if (colorMap) {
    dumpColorSpaceL2(colorMap->getColorSpace());
    writePS(" setcolorspace\n");
  }

  // set up to use the array created by setupImages()
  if (mode == psModeForm && !inlineImg) {
    writePS("ImData_%d_%d 0\n", ref->getRefNum(), ref->getRefGen());
  }

  // image dictionary
  writePS("<<\n  /ImageType 1\n");

  // width, height, matrix, bits per component
  writePS("  /Width %d\n", width);
  writePS("  /Height %d\n", height);
  writePS("  /ImageMatrix [%d 0 0 %d 0 %d]\n", width, -height, height);
  writePS("  /BitsPerComponent %d\n",
	  colorMap ? colorMap->getBits() : 1);

  // decode 
  if (colorMap) {
    writePS("  /Decode [");
    if (colorMap->getColorSpace()->getMode() == csSeparation) {
      //~ this is a kludge -- see comment in dumpColorSpaceL2
      n = (1 << colorMap->getBits()) - 1;
      writePS("%g %g", colorMap->getDecodeLow(0) * n,
	      colorMap->getDecodeHigh(0) * n);
    } else {
      numComps = colorMap->getNumPixelComps();
      for (i = 0; i < numComps; ++i) {
	if (i > 0) {
	  writePS(" ");
	}
	writePS("%g %g", colorMap->getDecodeLow(i),
		colorMap->getDecodeHigh(i));
      }
    }
    writePS("]\n");
  } else {
    writePS("  /Decode [%d %d]\n", invert ? 1 : 0, invert ? 0 : 1);
  }

  if (mode == psModeForm) {

    if (inlineImg) {

      // data source
      writePS("  /DataSource <~\n");

      // write image data stream, using ASCII85 encode filter
      str = new FixedLengthEncoder(str, len);
      str = new ASCII85Encoder(str);
      str->reset();
      while ((c = str->getChar()) != EOF) {
	fputc(c, f);
      }
      fputc('\n', f);
      delete str;

    } else {
      writePS("  /DataSource { 2 copy get exch 1 add exch }\n");
    }

    // end of image dictionary
    writePS(">>\n%s\n", colorMap ? "image" : "imagemask");

    // get rid of the array and index
    if (!inlineImg) {
      writePS("pop pop\n");
    }

  } else {

    // data source
    writePS("  /DataSource currentfile\n");
    s = str->getPSFilter("    ");
    if (inlineImg || !s) {
      useRLE = gTrue;
      useA85 = gTrue;
    } else {
      useRLE = gFalse;
      useA85 = str->isBinary();
    }
    if (useA85) {
      writePS("    /ASCII85Decode filter\n");
    }
    if (useRLE) {
      writePS("    /RunLengthDecode filter\n");
    } else {
      writePS("%s", s->getCString());
    }
    if (s) {
      delete s;
    }

    // cut off inline image streams at appropriate length
    if (inlineImg) {
      str = new FixedLengthEncoder(str, len);
    } else if (!useRLE) {
      str = str->getBaseStream();
    }

    // add RunLengthEncode and ASCII85 encode filters
    if (useRLE) {
      str = new RunLengthEncoder(str);
    }
    if (useA85) {
      str = new ASCII85Encoder(str);
    }

    // end of image dictionary
    writePS(">>\n");
#if OPI_SUPPORT
    if (opi13Nest) {
      if (inlineImg) {
	// this can't happen -- OPI dictionaries are in XObjects
	error(-1, "Internal: OPI in inline image");
	n = 0;
      } else {
	// need to read the stream to count characters -- the length
	// is data-dependent (because of A85 and RLE filters)
	str->reset();
	n = 0;
	while ((c = str->getChar()) != EOF) {
	  ++n;
	}
      }
      // +6/7 for "pdfIm\n" / "pdfImM\n"
      // +8 for newline + trailer
      n += colorMap ? 14 : 15;
      writePS("%%%%BeginData: %d Hex Bytes\n", n);
    }
#endif
    if (level == psLevel2Sep && colorMap &&
	colorMap->getColorSpace()->getMode() == csSeparation) {
      color.c[0] = 1;
      sepCS = (GfxSeparationColorSpace *)colorMap->getColorSpace();
      sepCS->getCMYK(&color, &cmyk);
      writePS("%g %g %g %g (%s) pdfImSep\n",
	      cmyk.c, cmyk.m, cmyk.y, cmyk.k, sepCS->getName()->getCString());
    } else {
      writePS("%s\n", colorMap ? "pdfIm" : "pdfImM");
    }

    // copy the stream data
    str->reset();
    while ((c = str->getChar()) != EOF) {
      fputc(c, f);
    }

    // add newline and trailer to the end
    fputc('\n', f);
    fputs("%-EOD-\n", f);
#if OPI_SUPPORT
    if (opi13Nest) {
      writePS("%%%%EndData\n");
    }
#endif

    // delete encoders
    if (useRLE || useA85) {
      delete str;
    }
  }
}

void PSOutputDev::dumpColorSpaceL2(GfxColorSpace *colorSpace) {
  GfxCalGrayColorSpace *calGrayCS;
  GfxCalRGBColorSpace *calRGBCS;
  GfxLabColorSpace *labCS;
  GfxIndexedColorSpace *indexedCS;
  GfxSeparationColorSpace *separationCS;
  Guchar *lookup;
  double x[gfxColorMaxComps], y[gfxColorMaxComps];
  GfxColor color;
  GfxCMYK cmyk;
  int n, numComps;
  int i, j, k;

  switch (colorSpace->getMode()) {

  case csDeviceGray:
    writePS("/DeviceGray");
    processColors |= psProcessBlack;
    break;

  case csCalGray:
    calGrayCS = (GfxCalGrayColorSpace *)colorSpace;
    writePS("[/CIEBasedA <<\n");
    writePS(" /DecodeA {%g exp} bind\n", calGrayCS->getGamma());
    writePS(" /MatrixA [%g %g %g]\n",
	    calGrayCS->getWhiteX(), calGrayCS->getWhiteY(),
	    calGrayCS->getWhiteZ());
    writePS(" /WhitePoint [%g %g %g]\n",
	    calGrayCS->getWhiteX(), calGrayCS->getWhiteY(),
	    calGrayCS->getWhiteZ());
    writePS(" /BlackPoint [%g %g %g]\n",
	    calGrayCS->getBlackX(), calGrayCS->getBlackY(),
	    calGrayCS->getBlackZ());
    writePS(">>]");
    processColors |= psProcessBlack;
    break;

  case csDeviceRGB:
    writePS("/DeviceRGB");
    processColors |= psProcessCMYK;
    break;

  case csCalRGB:
    calRGBCS = (GfxCalRGBColorSpace *)colorSpace;
    writePS("[/CIEBasedABC <<\n");
    writePS(" /DecodeABC [{%g exp} bind {%g exp} bind {%g exp} bind]\n",
	    calRGBCS->getGammaR(), calRGBCS->getGammaG(),
	    calRGBCS->getGammaB());
    writePS(" /MatrixABC [%g %g %g %g %g %g %g %g %g]\n",
	    calRGBCS->getMatrix()[0], calRGBCS->getMatrix()[1],
	    calRGBCS->getMatrix()[2], calRGBCS->getMatrix()[3],
	    calRGBCS->getMatrix()[4], calRGBCS->getMatrix()[5],
	    calRGBCS->getMatrix()[6], calRGBCS->getMatrix()[7],
	    calRGBCS->getMatrix()[8]);
    writePS(" /WhitePoint [%g %g %g]\n",
	    calRGBCS->getWhiteX(), calRGBCS->getWhiteY(),
	    calRGBCS->getWhiteZ());
    writePS(" /BlackPoint [%g %g %g]\n",
	    calRGBCS->getBlackX(), calRGBCS->getBlackY(),
	    calRGBCS->getBlackZ());
    writePS(">>]");
    processColors |= psProcessCMYK;
    break;

  case csDeviceCMYK:
    writePS("/DeviceCMYK");
    processColors |= psProcessCMYK;
    break;

  case csLab:
    labCS = (GfxLabColorSpace *)colorSpace;
    writePS("[/CIEBasedABC <<\n");
    writePS(" /RangeABC [0 100 %g %g %g %g]\n",
	    labCS->getAMin(), labCS->getAMax(),
	    labCS->getBMin(), labCS->getBMax());
    writePS(" /DecodeABC [{16 add 116 div} bind {500 div} bind {200 div} bind]\n");
    writePS(" /MatrixABC [1 1 1 1 0 0 0 0 -1]\n");
    writePS(" /DecodeLMN\n");
    writePS("   [{dup 6 29 div ge {dup dup mul mul}\n");
    writePS("     {4 29 div sub 108 841 div mul } ifelse %g mul} bind\n",
	    labCS->getWhiteX());
    writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
    writePS("     {4 29 div sub 108 841 div mul } ifelse %g mul} bind\n",
	    labCS->getWhiteY());
    writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
    writePS("     {4 29 div sub 108 841 div mul } ifelse %g mul} bind]\n",
	    labCS->getWhiteZ());
    writePS(" /WhitePoint [%g %g %g]\n",
	    labCS->getWhiteX(), labCS->getWhiteY(), labCS->getWhiteZ());
    writePS(" /BlackPoint [%g %g %g]\n",
	    labCS->getBlackX(), labCS->getBlackY(), labCS->getBlackZ());
    writePS(">>]");
    processColors |= psProcessCMYK;
    break;

  case csICCBased:
    dumpColorSpaceL2(((GfxICCBasedColorSpace *)colorSpace)->getAlt());
    break;

  case csIndexed:
    indexedCS = (GfxIndexedColorSpace *)colorSpace;
    writePS("[/Indexed ");
    dumpColorSpaceL2(indexedCS->getBase());
    n = indexedCS->getIndexHigh();
    numComps = indexedCS->getBase()->getNComps();
    lookup = indexedCS->getLookup();
    writePS(" %d <\n", n);
    for (i = 0; i <= n; i += 8) {
      writePS("  ");
      for (j = i; j < i+8 && j <= n; ++j) {
	for (k = 0; k < numComps; ++k) {
	  writePS("%02x", lookup[j * numComps + k]);
	}
	color.c[0] = j;
	indexedCS->getCMYK(&color, &cmyk);
	addProcessColor(cmyk.c, cmyk.m, cmyk.y, cmyk.k);
      }
      writePS("\n");
    }
    writePS(">]");
    break;

  case csSeparation:
    //~ this is a kludge -- the correct thing would to ouput a
    //~ separation color space, with the specified alternate color
    //~ space and tint transform
    separationCS = (GfxSeparationColorSpace *)colorSpace;
    writePS("[/Indexed ");
    dumpColorSpaceL2(separationCS->getAlt());
    writePS(" 255 <\n");
    numComps = separationCS->getAlt()->getNComps();
    for (i = 0; i <= 255; i += 8) {
      writePS("  ");
      for (j = i; j < i+8 && j <= 255; ++j) {
	x[0] = (double)j / 255.0;
	separationCS->getFunc()->transform(x, y);
	for (k = 0; k < numComps; ++k) {
	  writePS("%02x", (int)(255 * y[k] + 0.5));
	}
      }
      writePS("\n");
    }
    writePS(">]");
    addCustomColor(separationCS);
    break;

  case csDeviceN:
    // DeviceN color spaces are a Level 3 PostScript feature.
    dumpColorSpaceL2(((GfxDeviceNColorSpace *)colorSpace)->getAlt());
    break;

  case csPattern:
    //~ unimplemented
    break;

  }
}

#if OPI_SUPPORT
void PSOutputDev::opiBegin(GfxState *state, Dict *opiDict) {
  Object dict;

  if (doOPI) {
    opiDict->lookup("2.0", &dict);
    if (dict.isDict()) {
      opiBegin20(state, dict.getDict());
      dict.free();
    } else {
      dict.free();
      opiDict->lookup("1.3", &dict);
      if (dict.isDict()) {
	opiBegin13(state, dict.getDict());
      }
      dict.free();
    }
  }
}

void PSOutputDev::opiBegin20(GfxState *state, Dict *dict) {
  Object obj1, obj2, obj3, obj4;
  double width, height, left, right, top, bottom;
  int w, h;
  int i;

  writePS("%%%%BeginOPI: 2.0\n");
  writePS("%%%%Distilled\n");

  dict->lookup("F", &obj1);
  if (getFileSpec(&obj1, &obj2)) {
    writePS("%%%%ImageFileName: %s\n",
	    obj2.getString()->getCString());
    obj2.free();
  }
  obj1.free();

  dict->lookup("MainImage", &obj1);
  if (obj1.isString()) {
    writePS("%%%%MainImage: %s\n", obj1.getString()->getCString());
  }
  obj1.free();

  //~ ignoring 'Tags' entry
  //~ need to use writePSString() and deal with >255-char lines

  dict->lookup("Size", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 2) {
    obj1.arrayGet(0, &obj2);
    width = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    height = obj2.getNum();
    obj2.free();
    writePS("%%%%ImageDimensions: %g %g\n", width, height);
  }
  obj1.free();

  dict->lookup("CropRect", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 4) {
    obj1.arrayGet(0, &obj2);
    left = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    top = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    right = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    bottom = obj2.getNum();
    obj2.free();
    writePS("%%%%ImageCropRect: %g %g %g %g\n", left, top, right, bottom);
  }
  obj1.free();

  dict->lookup("Overprint", &obj1);
  if (obj1.isBool()) {
    writePS("%%%%ImageOverprint: %s\n", obj1.getBool() ? "true" : "false");
  }
  obj1.free();

  dict->lookup("Inks", &obj1);
  if (obj1.isName()) {
    writePS("%%%%ImageInks: %s\n", obj1.getName());
  } else if (obj1.isArray() && obj1.arrayGetLength() >= 1) {
    obj1.arrayGet(0, &obj2);
    if (obj2.isName()) {
      writePS("%%%%ImageInks: %s %d",
	      obj2.getName(), (obj1.arrayGetLength() - 1) / 2);
      for (i = 1; i+1 < obj1.arrayGetLength(); i += 2) {
	obj1.arrayGet(i, &obj3);
	obj1.arrayGet(i+1, &obj4);
	if (obj3.isString() && obj4.isNum()) {
	  writePS(" ");
	  writePSString(obj3.getString());
	  writePS(" %g", obj4.getNum());
	}
	obj3.free();
	obj4.free();
      }
      writePS("\n");
    }
    obj2.free();
  }
  obj1.free();

  writePS("gsave\n");

  writePS("%%%%BeginIncludedImage\n");

  dict->lookup("IncludedImageDimensions", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 2) {
    obj1.arrayGet(0, &obj2);
    w = obj2.getInt();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    h = obj2.getInt();
    obj2.free();
    writePS("%%%%IncludedImageDimensions: %d %d\n", w, h);
  }
  obj1.free();

  dict->lookup("IncludedImageQuality", &obj1);
  if (obj1.isNum()) {
    writePS("%%%%IncludedImageQuality: %g\n", obj1.getNum());
  }
  obj1.free();

  ++opi20Nest;
}

void PSOutputDev::opiBegin13(GfxState *state, Dict *dict) {
  Object obj1, obj2;
  int left, right, top, bottom, samples, bits, width, height;
  double c, m, y, k;
  double llx, lly, ulx, uly, urx, ury, lrx, lry;
  double tllx, tlly, tulx, tuly, turx, tury, tlrx, tlry;
  double horiz, vert;
  int i, j;

  writePS("save\n");
  writePS("/opiMatrix2 matrix currentmatrix def\n");
  writePS("opiMatrix setmatrix\n");

  dict->lookup("F", &obj1);
  if (getFileSpec(&obj1, &obj2)) {
    writePS("%%ALDImageFileName: %s\n",
	    obj2.getString()->getCString());
    obj2.free();
  }
  obj1.free();

  dict->lookup("CropRect", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 4) {
    obj1.arrayGet(0, &obj2);
    left = obj2.getInt();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    top = obj2.getInt();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    right = obj2.getInt();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    bottom = obj2.getInt();
    obj2.free();
    writePS("%%ALDImageCropRect: %d %d %d %d\n", left, top, right, bottom);
  }
  obj1.free();

  dict->lookup("Color", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 5) {
    obj1.arrayGet(0, &obj2);
    c = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    m = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    y = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    k = obj2.getNum();
    obj2.free();
    obj1.arrayGet(4, &obj2);
    if (obj2.isString()) {
      writePS("%%ALDImageColor: %g %g %g %g ", c, m, y, k);
      writePSString(obj2.getString());
      writePS("\n");
    }
    obj2.free();
  }
  obj1.free();

  dict->lookup("ColorType", &obj1);
  if (obj1.isName()) {
    writePS("%%ALDImageColorType: %s\n", obj1.getName());
  }
  obj1.free();

  //~ ignores 'Comments' entry
  //~ need to handle multiple lines

  dict->lookup("CropFixed", &obj1);
  if (obj1.isArray()) {
    obj1.arrayGet(0, &obj2);
    ulx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    uly = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    lrx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    lry = obj2.getNum();
    obj2.free();
    writePS("%%ALDImageCropFixed: %g %g %g %g\n", ulx, uly, lrx, lry);
  }
  obj1.free();

  dict->lookup("GrayMap", &obj1);
  if (obj1.isArray()) {
    writePS("%%ALDImageGrayMap:");
    for (i = 0; i < obj1.arrayGetLength(); i += 16) {
      if (i > 0) {
	writePS("\n%%%%+");
      }
      for (j = 0; j < 16 && i+j < obj1.arrayGetLength(); ++j) {
	obj1.arrayGet(i+j, &obj2);
	writePS(" %d", obj2.getInt());
	obj2.free();
      }
    }
    writePS("\n");
  }
  obj1.free();

  dict->lookup("ID", &obj1);
  if (obj1.isString()) {
    writePS("%%ALDImageID: %s\n", obj1.getString()->getCString());
  }
  obj1.free();

  dict->lookup("ImageType", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 2) {
    obj1.arrayGet(0, &obj2);
    samples = obj2.getInt();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    bits = obj2.getInt();
    obj2.free();
    writePS("%%ALDImageType: %d %d\n", samples, bits);
  }
  obj1.free();

  dict->lookup("Overprint", &obj1);
  if (obj1.isBool()) {
    writePS("%%ALDImageOverprint: %s\n", obj1.getBool() ? "true" : "false");
  }
  obj1.free();

  dict->lookup("Position", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 8) {
    obj1.arrayGet(0, &obj2);
    llx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    lly = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    ulx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    uly = obj2.getNum();
    obj2.free();
    obj1.arrayGet(4, &obj2);
    urx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(5, &obj2);
    ury = obj2.getNum();
    obj2.free();
    obj1.arrayGet(6, &obj2);
    lrx = obj2.getNum();
    obj2.free();
    obj1.arrayGet(7, &obj2);
    lry = obj2.getNum();
    obj2.free();
    opiTransform(state, llx, lly, &tllx, &tlly);
    opiTransform(state, ulx, uly, &tulx, &tuly);
    opiTransform(state, urx, ury, &turx, &tury);
    opiTransform(state, lrx, lry, &tlrx, &tlry);
    writePS("%%ALDImagePosition: %g %g %g %g %g %g %g %g\n",
	    tllx, tlly, tulx, tuly, turx, tury, tlrx, tlry);
    obj2.free();
  }
  obj1.free();

  dict->lookup("Resolution", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 2) {
    obj1.arrayGet(0, &obj2);
    horiz = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    vert = obj2.getNum();
    obj2.free();
    writePS("%%ALDImageResoution: %g %g\n", horiz, vert);
    obj2.free();
  }
  obj1.free();

  dict->lookup("Size", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 2) {
    obj1.arrayGet(0, &obj2);
    width = obj2.getInt();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    height = obj2.getInt();
    obj2.free();
    writePS("%%ALDImageDimensions: %d %d\n", width, height);
  }
  obj1.free();

  //~ ignoring 'Tags' entry
  //~ need to use writePSString() and deal with >255-char lines

  dict->lookup("Tint", &obj1);
  if (obj1.isNum()) {
    writePS("%%ALDImageTint: %g\n", obj1.getNum());
  }
  obj1.free();

  dict->lookup("Transparency", &obj1);
  if (obj1.isBool()) {
    writePS("%%ALDImageTransparency: %s\n", obj1.getBool() ? "true" : "false");
  }
  obj1.free();

  writePS("%%%%BeginObject: image\n");
  writePS("opiMatrix2 setmatrix\n");
  ++opi13Nest;
}

// Convert PDF user space coordinates to PostScript default user space
// coordinates.  This has to account for both the PDF CTM and the
// PSOutputDev page-fitting transform.
void PSOutputDev::opiTransform(GfxState *state, double x0, double y0,
			       double *x1, double *y1) {
  double t;

  state->transform(x0, y0, x1, y1);
  *x1 += tx;
  *y1 += ty;
  if (landscape) {
    t = *x1;
    *x1 = -*y1;
    *y1 = t;
  }
  *x1 *= xScale;
  *y1 *= yScale;
}

void PSOutputDev::opiEnd(GfxState *state, Dict *opiDict) {
  Object dict;

  if (doOPI) {
    opiDict->lookup("2.0", &dict);
    if (dict.isDict()) {
      writePS("%%%%EndIncludedImage\n");
      writePS("%%%%EndOPI\n");
      writePS("grestore\n");
      --opi20Nest;
      dict.free();
    } else {
      dict.free();
      opiDict->lookup("1.3", &dict);
      if (dict.isDict()) {
	writePS("%%%%EndObject\n");
	writePS("restore\n");
	--opi13Nest;
      }
      dict.free();
    }
  }
}

GBool PSOutputDev::getFileSpec(Object *fileSpec, Object *fileName) {
  if (fileSpec->isString()) {
    fileSpec->copy(fileName);
    return gTrue;
  }
  if (fileSpec->isDict()) {
    fileSpec->dictLookup("DOS", fileName);
    if (fileName->isString()) {
      return gTrue;
    }
    fileName->free();
    fileSpec->dictLookup("Mac", fileName);
    if (fileName->isString()) {
      return gTrue;
    }
    fileName->free();
    fileSpec->dictLookup("Unix", fileName);
    if (fileName->isString()) {
      return gTrue;
    }
    fileName->free();
    fileSpec->dictLookup("F", fileName);
    if (fileName->isString()) {
      return gTrue;
    }
    fileName->free();
  }
  return gFalse;
}
#endif // OPI_SUPPORT

void PSOutputDev::writePS(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
}

void PSOutputDev::writePSString(GString *s) {
  Guchar *p;
  int n;

  fputc('(', f);
  for (p = (Guchar *)s->getCString(), n = s->getLength(); n; ++p, --n) {
    if (*p == '(' || *p == ')' || *p == '\\')
      fprintf(f, "\\%c", *p);
    else if (*p < 0x20 || *p >= 0x80)
      fprintf(f, "\\%03o", *p);
    else
      fputc(*p, f);
  }
  fputc(')', f);
}
