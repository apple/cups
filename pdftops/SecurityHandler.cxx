//========================================================================
//
// SecurityHandler.cc
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "GString.h"
#include "PDFDoc.h"
#include "Decrypt.h"
#include "Error.h"
#include "GlobalParams.h"
#if HAVE_XPDFCORE
#  include "XPDFCore.h"
#elif HAVE_WINPDFCORE
#  include "WinPDFCore.h"
#endif
#include "XpdfPluginAPI.h"
#include "SecurityHandler.h"

//------------------------------------------------------------------------
// SecurityHandler
//------------------------------------------------------------------------

SecurityHandler *SecurityHandler::make(PDFDoc *docA, Object *encryptDictA) {
  Object filterObj;
  SecurityHandler *secHdlr;
  XpdfSecurityHandler *xsh;

  encryptDictA->dictLookup("Filter", &filterObj);
  if (filterObj.isName("Standard")) {
    secHdlr = new StandardSecurityHandler(docA, encryptDictA);
  } else if (filterObj.isName()) {
    if ((xsh = globalParams->getSecurityHandler(filterObj.getName()))) {
      secHdlr = new ExternalSecurityHandler(docA, encryptDictA, xsh);
    } else {
      error(-1, "Couldn't find the '%s' security handler",
	    filterObj.getName());
      secHdlr = NULL;
    }
  } else {
    error(-1, "Missing or invalid 'Filter' entry in encryption dictionary");
    secHdlr = NULL;
  }
  filterObj.free();
  return secHdlr;
}

SecurityHandler::SecurityHandler(PDFDoc *docA) {
  doc = docA;
}

SecurityHandler::~SecurityHandler() {
}

GBool SecurityHandler::checkEncryption(GString *ownerPassword,
				       GString *userPassword) {
  void *authData;
  GBool ok;
  int i;

  if (ownerPassword || userPassword) {
    authData = makeAuthData(ownerPassword, userPassword);
  } else {
    authData = NULL;
  }
  ok = authorize(authData);
  if (authData) {
    freeAuthData(authData);
  }
  for (i = 0; !ok && i < 3; ++i) {
    if (!(authData = getAuthData())) {
      break;
    }
    ok = authorize(authData);
    if (authData) {
      freeAuthData(authData);
    }
  }
  if (!ok) {
    error(-1, "Incorrect password");
  }
  return ok;
}

//------------------------------------------------------------------------
// StandardSecurityHandler
//------------------------------------------------------------------------

class StandardAuthData {
public:

  StandardAuthData(GString *ownerPasswordA, GString *userPasswordA) {
    ownerPassword = ownerPasswordA;
    userPassword = userPasswordA;
  }

  ~StandardAuthData() {
    if (ownerPassword) {
      delete ownerPassword;
    }
    if (userPassword) {
      delete userPassword;
    }
  }

  GString *ownerPassword;
  GString *userPassword;
};

StandardSecurityHandler::StandardSecurityHandler(PDFDoc *docA,
						 Object *encryptDictA):
  SecurityHandler(docA)
{
  Object versionObj, revisionObj, lengthObj;
  Object ownerKeyObj, userKeyObj, permObj, fileIDObj;
  Object fileIDObj1;

  ok = gFalse;
  fileID = NULL;
  ownerKey = NULL;
  userKey = NULL;

  encryptDictA->dictLookup("V", &versionObj);
  encryptDictA->dictLookup("R", &revisionObj);
  encryptDictA->dictLookup("Length", &lengthObj);
  encryptDictA->dictLookup("O", &ownerKeyObj);
  encryptDictA->dictLookup("U", &userKeyObj);
  encryptDictA->dictLookup("P", &permObj);
  doc->getXRef()->getTrailerDict()->dictLookup("ID", &fileIDObj);
  if (versionObj.isInt() &&
      revisionObj.isInt() &&
      ownerKeyObj.isString() && ownerKeyObj.getString()->getLength() == 32 &&
      userKeyObj.isString() && userKeyObj.getString()->getLength() == 32 &&
      permObj.isInt()) {
    encVersion = versionObj.getInt();
    encRevision = revisionObj.getInt();
    if (lengthObj.isInt()) {
      fileKeyLength = lengthObj.getInt() / 8;
    } else {
      fileKeyLength = 5;
    }
    permFlags = permObj.getInt();
    ownerKey = ownerKeyObj.getString()->copy();
    userKey = userKeyObj.getString()->copy();
    if (encVersion >= 1 && encVersion <= 2 &&
	encRevision >= 2 && encRevision <= 3) {
      if (fileIDObj.isArray()) {
	if (fileIDObj.arrayGet(0, &fileIDObj1)->isString()) {
	  fileID = fileIDObj1.getString()->copy();
	} else {
	  fileID = new GString();
	}
	fileIDObj1.free();
      } else {
	fileID = new GString();
      }
      ok = gTrue;
    } else {
      error(-1, "Unsupported version/revision (%d/%d) of Standard security handler",
	    encVersion, encRevision);
    }
  } else {
    error(-1, "Weird encryption info");
  }
  fileIDObj.free();
  permObj.free();
  userKeyObj.free();
  ownerKeyObj.free();
  lengthObj.free();
  revisionObj.free();
  versionObj.free();
}

StandardSecurityHandler::~StandardSecurityHandler() {
  if (fileID) {
    delete fileID;
  }
  if (ownerKey) {
    delete ownerKey;
  }
  if (userKey) {
    delete userKey;
  }
}

void *StandardSecurityHandler::makeAuthData(GString *ownerPassword,
					    GString *userPassword) {
  return new StandardAuthData(ownerPassword ? ownerPassword->copy()
			                    : (GString *)NULL,
			      userPassword ? userPassword->copy()
			                   : (GString *)NULL);
}

void *StandardSecurityHandler::getAuthData() {
#if HAVE_XPDFCORE
  XPDFCore *core;
  GString *password;

  if (!(core = (XPDFCore *)doc->getGUIData()) ||
      !(password = core->getPassword())) {
    return NULL;
  }
  return new StandardAuthData(password, password->copy());
#elif HAVE_WINPDFCORE
  WinPDFCore *core;
  GString *password;

  if (!(core = (WinPDFCore *)doc->getGUIData()) ||
      !(password = core->getPassword())) {
    return NULL;
  }
  return new StandardAuthData(password, password->copy());
#else
  return NULL;
#endif
}

void StandardSecurityHandler::freeAuthData(void *authData) {
  delete (StandardAuthData *)authData;
}

GBool StandardSecurityHandler::authorize(void *authData) {
  GString *ownerPassword, *userPassword;

  if (!ok) {
    return gFalse;
  }
  if (authData) {
    ownerPassword = ((StandardAuthData *)authData)->ownerPassword;
    userPassword = ((StandardAuthData *)authData)->userPassword;
  } else {
    ownerPassword = NULL;
    userPassword = NULL;
  }
  if (!Decrypt::makeFileKey(encVersion, encRevision, fileKeyLength,
			    ownerKey, userKey, permFlags, fileID,
			    ownerPassword, userPassword, fileKey,
			    &ownerPasswordOk)) {
    return gFalse;
  }
  return gTrue;
}

//------------------------------------------------------------------------
// ExternalSecurityHandler
//------------------------------------------------------------------------

ExternalSecurityHandler::ExternalSecurityHandler(PDFDoc *docA,
						 Object *encryptDictA,
						 XpdfSecurityHandler *xshA):
  SecurityHandler(docA)
{
  encryptDictA->copy(&encryptDict);
  xsh = xshA;
  ok = gFalse;

  if (!(*xsh->newDoc)(xsh->handlerData, (XpdfDoc)docA,
		      (XpdfObject)encryptDictA, &docData)) {
    return;
  }

  ok = gTrue;
}

ExternalSecurityHandler::~ExternalSecurityHandler() {
  (*xsh->freeDoc)(xsh->handlerData, docData);
  encryptDict.free();
}

void *ExternalSecurityHandler::makeAuthData(GString *ownerPassword,
					    GString *userPassword) {
  char *opw, *upw;
  void *authData;

  opw = ownerPassword ? ownerPassword->getCString() : (char *)NULL;
  upw = userPassword ? userPassword->getCString() : (char *)NULL;
  if (!(*xsh->makeAuthData)(xsh->handlerData, docData, opw, upw, &authData)) {
    return NULL;
  }
  return authData;
}

void *ExternalSecurityHandler::getAuthData() {
  void *authData;

  if (!(*xsh->getAuthData)(xsh->handlerData, docData, &authData)) {
    return NULL;
  }
  return authData;
}

void ExternalSecurityHandler::freeAuthData(void *authData) {
  (*xsh->freeAuthData)(xsh->handlerData, docData, authData);
}

GBool ExternalSecurityHandler::authorize(void *authData) {
  char *key;
  int length;

  if (!ok) {
    return gFalse;
  }
  permFlags = (*xsh->authorize)(xsh->handlerData, docData, authData);
  if (!(permFlags & xpdfPermissionOpen)) {
    return gFalse;
  }
  if (!(*xsh->getKey)(xsh->handlerData, docData, &key, &length, &encVersion)) {
    return gFalse;
  }
  if ((fileKeyLength = length) > 16) {
    fileKeyLength = 16;
  }
  memcpy(fileKey, key, fileKeyLength);
  (*xsh->freeKey)(xsh->handlerData, docData, key, length);
  return gTrue;
}
