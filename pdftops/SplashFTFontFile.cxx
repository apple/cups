//========================================================================
//
// SplashFTFontFile.cc
//
//========================================================================

#include <config.h>

#if HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "gmem.h"
#include "SplashFTFontEngine.h"
#include "SplashFTFont.h"
#include "SplashFTFontFile.h"

//------------------------------------------------------------------------
// SplashFTFontFile
//------------------------------------------------------------------------

SplashFontFile *SplashFTFontFile::loadType1Font(SplashFTFontEngine *engineA,
						SplashFontFileID *idA,
						char *fileNameA,
						GBool deleteFileA,
						char **encA) {
  FT_Face faceA;
  Gushort *codeToGIDA;
  char *name;
  int i;

  if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA)) {
    return NULL;
  }
  codeToGIDA = (Gushort *)gmalloc(256 * sizeof(int));
  for (i = 0; i < 256; ++i) {
    codeToGIDA[i] = 0;
    if ((name = encA[i])) {
      codeToGIDA[i] = (Gushort)FT_Get_Name_Index(faceA, name);
    }
  }

  return new SplashFTFontFile(engineA, idA, fileNameA, deleteFileA,
			      faceA, codeToGIDA, 256);
}

SplashFontFile *SplashFTFontFile::loadCIDFont(SplashFTFontEngine *engineA,
					      SplashFontFileID *idA,
					      char *fileNameA,
					      GBool deleteFileA,
					      Gushort *codeToGIDA,
					      int codeToGIDLenA) {
  FT_Face faceA;

  if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA)) {
    return NULL;
  }

  return new SplashFTFontFile(engineA, idA, fileNameA, deleteFileA,
			      faceA, codeToGIDA, codeToGIDLenA);
}

SplashFontFile *SplashFTFontFile::loadTrueTypeFont(SplashFTFontEngine *engineA,
						   SplashFontFileID *idA,
						   char *fileNameA,
						   GBool deleteFileA,
						   Gushort *codeToGIDA,
						   int codeToGIDLenA) {
  FT_Face faceA;

  if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA)) {
    return NULL;
  }

  return new SplashFTFontFile(engineA, idA, fileNameA, deleteFileA,
			      faceA, codeToGIDA, codeToGIDLenA);
}

SplashFTFontFile::SplashFTFontFile(SplashFTFontEngine *engineA,
				   SplashFontFileID *idA,
				   char *fileNameA, GBool deleteFileA,
				   FT_Face faceA,
				   Gushort *codeToGIDA, int codeToGIDLenA):
  SplashFontFile(idA, fileNameA, deleteFileA)
{
  engine = engineA;
  face = faceA;
  codeToGID = codeToGIDA;
  codeToGIDLen = codeToGIDLenA;
}

SplashFTFontFile::~SplashFTFontFile() {
  if (face) {
    FT_Done_Face(face);
  }
  if (codeToGID) {
    gfree(codeToGID);
  }
}

SplashFont *SplashFTFontFile::makeFont(SplashCoord *mat) {
  SplashFont *font;

  font = new SplashFTFont(this, mat);
  font->initCache();
  return font;
}

#endif // HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H
