//========================================================================
//
// Stream.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include "gmem.h"
#include "gfile.h"
#include "config.h"
#include "Error.h"
#include "Object.h"
#ifndef NO_DECRYPTION
#include "Decrypt.h"
#endif
#include "Stream.h"
#include "Stream-CCITT.h"

#ifdef __DJGPP__
static GBool setDJSYSFLAGS = gFalse;
#endif

#ifdef VMS
#if (__VMS_VER < 70000000)
extern "C" int unlink(char *filename);
#endif
#ifdef __GNUC__
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#endif

#ifdef MACOS
#include "StuffItEngineLib.h"
#endif

//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

Stream::Stream() {
  ref = 1;
}

Stream::~Stream() {
}

int Stream::getRawChar() {
  error(-1, "Internal: called getRawChar() on non-predictor stream");
  return EOF;
}

char *Stream::getLine(char *buf, int size) {
  int i;
  int c;

  if (lookChar() == EOF)
    return NULL;
  for (i = 0; i < size - 1; ++i) {
    c = getChar();
    if (c == EOF || c == '\n')
      break;
    if (c == '\r') {
      if ((c = lookChar()) == '\n')
	getChar();
      break;
    }
    buf[i] = c;
  }
  buf[i] = '\0';
  return buf;
}

GString *Stream::getPSFilter(char *indent) {
  return new GString();
}

Stream *Stream::addFilters(Object *dict) {
  Object obj, obj2;
  Object params, params2;
  Stream *str;
  int i;

  str = this;
  dict->dictLookup("Filter", &obj);
  if (obj.isNull()) {
    obj.free();
    dict->dictLookup("F", &obj);
  }
  dict->dictLookup("DecodeParms", &params);
  if (params.isNull()) {
    params.free();
    dict->dictLookup("DP", &params);
  }
  if (obj.isName()) {
    str = makeFilter(obj.getName(), str, &params);
  } else if (obj.isArray()) {
    for (i = 0; i < obj.arrayGetLength(); ++i) {
      obj.arrayGet(i, &obj2);
      if (params.isArray())
	params.arrayGet(i, &params2);
      else
	params2.initNull();
      if (obj2.isName()) {
	str = makeFilter(obj2.getName(), str, &params2);
      } else {
	error(getPos(), "Bad filter name");
	str = new EOFStream(str);
      }
      obj2.free();
      params2.free();
    }
  } else if (!obj.isNull()) {
    error(getPos(), "Bad 'Filter' attribute in stream");
  }
  obj.free();
  params.free();

  return str;
}

Stream *Stream::makeFilter(char *name, Stream *str, Object *params) {
  int pred;			// parameters
  int colors;
  int bits;
  int early;
  int encoding;
  GBool endOfLine, byteAlign, endOfBlock, black;
  int columns, rows;
  Object obj;

  if (!strcmp(name, "ASCIIHexDecode") || !strcmp(name, "AHx")) {
    str = new ASCIIHexStream(str);
  } else if (!strcmp(name, "ASCII85Decode") || !strcmp(name, "A85")) {
    str = new ASCII85Stream(str);
  } else if (!strcmp(name, "LZWDecode") || !strcmp(name, "LZW")) {
    pred = 1;
    columns = 1;
    colors = 1;
    bits = 8;
    early = 1;
    if (params->isDict()) {
      params->dictLookup("Predictor", &obj);
      if (obj.isInt())
	pred = obj.getInt();
      obj.free();
      params->dictLookup("Columns", &obj);
      if (obj.isInt())
	columns = obj.getInt();
      obj.free();
      params->dictLookup("Colors", &obj);
      if (obj.isInt())
	colors = obj.getInt();
      obj.free();
      params->dictLookup("BitsPerComponent", &obj);
      if (obj.isInt())
	bits = obj.getInt();
      obj.free();
      params->dictLookup("EarlyChange", &obj);
      if (obj.isInt())
	early = obj.getInt();
      obj.free();
    }
    str = new LZWStream(str, pred, columns, colors, bits, early);
  } else if (!strcmp(name, "RunLengthDecode") || !strcmp(name, "RL")) {
    str = new RunLengthStream(str);
  } else if (!strcmp(name, "CCITTFaxDecode") || !strcmp(name, "CCF")) {
    encoding = 0;
    endOfLine = gFalse;
    byteAlign = gFalse;
    columns = 1728;
    rows = 0;
    endOfBlock = gTrue;
    black = gFalse;
    if (params->isDict()) {
      params->dictLookup("K", &obj);
      if (obj.isInt()) {
	encoding = obj.getInt();
      }
      obj.free();
      params->dictLookup("EndOfLine", &obj);
      if (obj.isBool()) {
	endOfLine = obj.getBool();
      }
      obj.free();
      params->dictLookup("EncodedByteAlign", &obj);
      if (obj.isBool()) {
	byteAlign = obj.getBool();
      }
      obj.free();
      params->dictLookup("Columns", &obj);
      if (obj.isInt()) {
	columns = obj.getInt();
      }
      obj.free();
      params->dictLookup("Rows", &obj);
      if (obj.isInt()) {
	rows = obj.getInt();
      }
      obj.free();
      params->dictLookup("EndOfBlock", &obj);
      if (obj.isBool()) {
	endOfBlock = obj.getBool();
      }
      obj.free();
      params->dictLookup("BlackIs1", &obj);
      if (obj.isBool()) {
	black = obj.getBool();
      }
      obj.free();
    }
    str = new CCITTFaxStream(str, encoding, endOfLine, byteAlign,
			     columns, rows, endOfBlock, black);
  } else if (!strcmp(name, "DCTDecode") || !strcmp(name, "DCT")) {
    str = new DCTStream(str);
  } else if (!strcmp(name, "FlateDecode") || !strcmp(name, "Fl")) {
    pred = 1;
    columns = 1;
    colors = 1;
    bits = 8;
    if (params->isDict()) {
      params->dictLookup("Predictor", &obj);
      if (obj.isInt())
	pred = obj.getInt();
      obj.free();
      params->dictLookup("Columns", &obj);
      if (obj.isInt())
	columns = obj.getInt();
      obj.free();
      params->dictLookup("Colors", &obj);
      if (obj.isInt())
	colors = obj.getInt();
      obj.free();
      params->dictLookup("BitsPerComponent", &obj);
      if (obj.isInt())
	bits = obj.getInt();
      obj.free();
    }
    str = new FlateStream(str, pred, columns, colors, bits);
  } else {
    error(getPos(), "Unknown filter '%s'", name);
    str = new EOFStream(str);
  }
  return str;
}

//------------------------------------------------------------------------
// BaseStream
//------------------------------------------------------------------------

BaseStream::BaseStream(Object *dict) {
  this->dict = *dict;
#ifndef NO_DECRYPTION
  decrypt = NULL;
#endif
}

BaseStream::~BaseStream() {
  dict.free();
#ifndef NO_DECRYPTION
  if (decrypt)
    delete decrypt;
#endif
}

#ifndef NO_DECRYPTION
void BaseStream::doDecryption(Guchar *fileKey, int objNum, int objGen) {
  decrypt = new Decrypt(fileKey, objNum, objGen);
}
#endif

//------------------------------------------------------------------------
// FilterStream
//------------------------------------------------------------------------

FilterStream::FilterStream(Stream *str) {
  this->str = str;
}

FilterStream::~FilterStream() {
}

void FilterStream::setPos(int pos) {
  error(-1, "Internal: called setPos() on FilterStream");
}

//------------------------------------------------------------------------
// ImageStream
//------------------------------------------------------------------------

ImageStream::ImageStream(Stream *str, int width, int nComps, int nBits) {
  int imgLineSize;

  this->str = str;
  this->width = width;
  this->nComps = nComps;
  this->nBits = nBits;

  nVals = width * nComps;
  if (nBits == 1) {
    imgLineSize = (nVals + 7) & ~7;
  } else {
    imgLineSize = nVals;
  }
  imgLine = (Guchar *)gmalloc(imgLineSize * sizeof(Guchar));
  imgIdx = nVals;
}

ImageStream::~ImageStream() {
  gfree(imgLine);
}

void ImageStream::reset() {
  str->reset();
}

GBool ImageStream::getPixel(Guchar *pix) {
  Gulong buf, bitMask;
  int bits;
  int c;
  int i;

  if (imgIdx >= nVals) {

    // read one line of image pixels
    if (nBits == 1) {
      for (i = 0; i < nVals; i += 8) {
	c = str->getChar();
	imgLine[i+0] = (Guchar)((c >> 7) & 1);
	imgLine[i+1] = (Guchar)((c >> 6) & 1);
	imgLine[i+2] = (Guchar)((c >> 5) & 1);
	imgLine[i+3] = (Guchar)((c >> 4) & 1);
	imgLine[i+4] = (Guchar)((c >> 3) & 1);
	imgLine[i+5] = (Guchar)((c >> 2) & 1);
	imgLine[i+6] = (Guchar)((c >> 1) & 1);
	imgLine[i+7] = (Guchar)(c & 1);
      }
    } else if (nBits == 8) {
      for (i = 0; i < nVals; ++i) {
	imgLine[i] = str->getChar();
      }
    } else {
      bitMask = (1 << nBits) - 1;
      buf = 0;
      bits = 0;
      for (i = 0; i < nVals; ++i) {
	if (bits < nBits) {
	  buf = (buf << 8) | (str->getChar() & 0xff);
	  bits += 8;
	}
	imgLine[i] = (Guchar)((buf >> (bits - nBits)) & bitMask);
	bits -= nBits;
      }
    }

    // reset to start of line
    imgIdx = 0;
  }

  for (i = 0; i < nComps; ++i)
    pix[i] = imgLine[imgIdx++];
  return gTrue;
}

void ImageStream::skipLine() {
  int n, i;

  n = (nVals * nBits + 7) >> 3;
  for (i = 0; i < n; ++i) {
    str->getChar();
  }
}

//------------------------------------------------------------------------
// StreamPredictor
//------------------------------------------------------------------------

StreamPredictor::StreamPredictor(Stream *str, int predictor,
				 int width, int nComps, int nBits) {
  this->str = str;
  this->predictor = predictor;
  this->width = width;
  this->nComps = nComps;
  this->nBits = nBits;

  nVals = width * nComps;
  pixBytes = (nComps * nBits + 7) >> 3;
  rowBytes = ((nVals * nBits + 7) >> 3) + pixBytes;
  predLine = (Guchar *)gmalloc(rowBytes);
  memset(predLine, 0, rowBytes);
  predIdx = rowBytes;
}

StreamPredictor::~StreamPredictor() {
  gfree(predLine);
}

int StreamPredictor::lookChar() {
  if (predIdx >= rowBytes) {
    if (!getNextLine()) {
      return EOF;
    }
  }
  return predLine[predIdx];
}

int StreamPredictor::getChar() {
  if (predIdx >= rowBytes) {
    if (!getNextLine()) {
      return EOF;
    }
  }
  return predLine[predIdx++];
}

GBool StreamPredictor::getNextLine() {
  int curPred;
  Guchar upLeftBuf[4];
  int left, up, upLeft, p, pa, pb, pc;
  int c;
  Gulong inBuf, outBuf, bitMask;
  int inBits, outBits;
  int i, j, k;

  // get PNG optimum predictor number
  if (predictor == 15) {
    if ((curPred = str->getRawChar()) == EOF) {
      return gFalse;
    }
    curPred += 10;
  } else {
    curPred = predictor;
  }

  // read the raw line, apply PNG (byte) predictor
  upLeftBuf[0] = upLeftBuf[1] = upLeftBuf[2] = upLeftBuf[3] = 0;
  for (i = pixBytes; i < rowBytes; ++i) {
    upLeftBuf[3] = upLeftBuf[2];
    upLeftBuf[2] = upLeftBuf[1];
    upLeftBuf[1] = upLeftBuf[0];
    upLeftBuf[0] = predLine[i];
    if ((c = str->getRawChar()) == EOF) {
      break;
    }
    switch (curPred) {
    case 11:			// PNG sub
      predLine[i] = predLine[i - pixBytes] + (Guchar)c;
      break;
    case 12:			// PNG up
      predLine[i] = predLine[i] + (Guchar)c;
      break;
    case 13:			// PNG average
      predLine[i] = ((predLine[i - pixBytes] + predLine[i]) >> 1) +
	            (Guchar)c;
      break;
    case 14:			// PNG Paeth
      left = predLine[i - pixBytes];
      up = predLine[i];
      upLeft = upLeftBuf[pixBytes];
      p = left + up - upLeft;
      if ((pa = p - left) < 0)
	pa = -pa;
      if ((pb = p - up) < 0)
	pb = -pb;
      if ((pc = p - upLeft) < 0)
	pc = -pc;
      if (pa <= pb && pa <= pc)
	predLine[i] = pa + (Guchar)c;
      else if (pb <= pc)
	predLine[i] = pb + (Guchar)c;
      else
	predLine[i] = pc + (Guchar)c;
      break;
    case 10:			// PNG none
    default:			// no predictor or TIFF predictor
      predLine[i] = (Guchar)c;
      break;
    }
  }

  // apply TIFF (component) predictor
  //~ this is completely untested
  if (predictor == 2) {
    if (nBits == 1) {
      inBuf = predLine[pixBytes - 1];
      for (i = pixBytes; i < rowBytes; i += 8) {
	// 1-bit add is just xor
	inBuf = (inBuf << 8) | predLine[i];
	predLine[i] ^= inBuf >> nComps;
      }
    } else if (nBits == 8) {
      for (i = pixBytes; i < rowBytes; ++i) {
	predLine[i] += predLine[i - nComps];
      }
    } else {
      upLeftBuf[0] = upLeftBuf[1] = upLeftBuf[2] = upLeftBuf[3] = 0;
      bitMask = (1 << nBits) - 1;
      inBuf = outBuf = 0;
      inBits = outBits = 0;
      j = k = pixBytes;
      for (i = 0; i < nVals; ++i) {
	if (inBits < nBits) {
	  inBuf = (inBuf << 8) | (predLine[j++] & 0xff);
	  inBits += 8;
	}
	upLeftBuf[3] = upLeftBuf[2];
	upLeftBuf[2] = upLeftBuf[1];
	upLeftBuf[1] = upLeftBuf[0];
	upLeftBuf[0] = (upLeftBuf[nComps] +
			(inBuf >> (inBits - nBits))) & bitMask;
	outBuf = (outBuf << nBits) | upLeftBuf[0];
	inBits -= nBits;
	outBits += nBits;
	if (outBits > 8) {
	  predLine[k++] = (Guchar)(outBuf >> (outBits - 8));
	}
      }
      if (outBits > 0) {
	predLine[k++] = (Guchar)(outBuf << (8 - outBits));
      }
    }
  }

  // reset to start of line
  predIdx = pixBytes;

  return gTrue;
}

//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------

FileStream::FileStream(FILE *f, int start, int length, Object *dict):
    BaseStream(dict) {
  this->f = f;
  this->start = start;
  this->length = length;
  bufPtr = bufEnd = buf;
  bufPos = start;
  savePos = -1;
}

FileStream::~FileStream() {
  if (savePos >= 0) {
    fseek(f, savePos, SEEK_SET);
  }
}

Stream *FileStream::makeSubStream(int start, int length, Object *dict) {
  return new FileStream(f, start, length, dict);
}

void FileStream::reset() {
  savePos = (int)ftell(f);
  fseek(f, start, SEEK_SET);
  bufPtr = bufEnd = buf;
  bufPos = start;
#ifndef NO_DECRYPTION
  if (decrypt)
    decrypt->reset();
#endif
}

GBool FileStream::fillBuf() {
  int n;
#ifndef NO_DECRYPTION
  char *p;
#endif

  bufPos += bufEnd - buf;
  bufPtr = bufEnd = buf;
  if (length >= 0 && bufPos >= start + length)
    return gFalse;
  if (length >= 0 && bufPos + 256 > start + length)
    n = start + length - bufPos;
  else
    n = 256;
  n = fread(buf, 1, n, f);
  bufEnd = buf + n;
  if (bufPtr >= bufEnd)
    return gFalse;
#ifndef NO_DECRYPTION
  if (decrypt) {
    for (p = buf; p < bufEnd; ++p)
      *p = (char)decrypt->decryptByte((Guchar)*p);
  }
#endif
  return gTrue;
}

void FileStream::setPos(int pos1) {
  long size;

  if (pos1 >= 0) {
    fseek(f, pos1, SEEK_SET);
    bufPos = pos1;
  } else {
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (pos1 < -size)
      pos1 = (int)(-size);
    fseek(f, pos1, SEEK_END);
    bufPos = (int)ftell(f);
  }
  bufPtr = bufEnd = buf;
}

void FileStream::moveStart(int delta) {
  this->start += delta;
  bufPtr = bufEnd = buf;
  bufPos = start;
}

//------------------------------------------------------------------------
// EmbedStream
//------------------------------------------------------------------------

EmbedStream::EmbedStream(Stream *str, Object *dict):
    BaseStream(dict) {
  this->str = str;
}

EmbedStream::~EmbedStream() {
}

Stream *EmbedStream::makeSubStream(int start, int length, Object *dict) {
  error(-1, "Internal: called makeSubStream() on EmbedStream");
  return NULL;
}

void EmbedStream::setPos(int pos) {
  error(-1, "Internal: called setPos() on EmbedStream");
}

int EmbedStream::getStart() {
  error(-1, "Internal: called getStart() on EmbedStream");
  return 0;
}

void EmbedStream::moveStart(int start) {
  error(-1, "Internal: called moveStart() on EmbedStream");
}

//------------------------------------------------------------------------
// ASCIIHexStream
//------------------------------------------------------------------------

ASCIIHexStream::ASCIIHexStream(Stream *str):
    FilterStream(str) {
  buf = EOF;
  eof = gFalse;
}

ASCIIHexStream::~ASCIIHexStream() {
  delete str;
}

void ASCIIHexStream::reset() {
  str->reset();
  buf = EOF;
  eof = gFalse;
}

int ASCIIHexStream::lookChar() {
  int c1, c2, x;

  if (buf != EOF)
    return buf;
  if (eof) {
    buf = EOF;
    return EOF;
  }
  do {
    c1 = str->getChar();
  } while (isspace(c1));
  if (c1 == '>') {
    eof = gTrue;
    buf = EOF;
    return buf;
  }
  do {
    c2 = str->getChar();
  } while (isspace(c2));
  if (c2 == '>') {
    eof = gTrue;
    c2 = '0';
  }
  if (c1 >= '0' && c1 <= '9') {
    x = (c1 - '0') << 4;
  } else if (c1 >= 'A' && c1 <= 'F') {
    x = (c1 - 'A' + 10) << 4;
  } else if (c1 >= 'a' && c1 <= 'f') {
    x = (c1 - 'a' + 10) << 4;
  } else if (c1 == EOF) {
    eof = gTrue;
    x = 0;
  } else {
    error(getPos(), "Illegal character <%02x> in ASCIIHex stream", c1);
    x = 0;
  }
  if (c2 >= '0' && c2 <= '9') {
    x += c2 - '0';
  } else if (c2 >= 'A' && c2 <= 'F') {
    x += c2 - 'A' + 10;
  } else if (c2 >= 'a' && c2 <= 'f') {
    x += c2 - 'a' + 10;
  } else if (c2 == EOF) {
    eof = gTrue;
    x = 0;
  } else {
    error(getPos(), "Illegal character <%02x> in ASCIIHex stream", c2);
  }
  buf = x & 0xff;
  return buf;
}

GString *ASCIIHexStream::getPSFilter(char *indent) {
  GString *s;

  s = str->getPSFilter(indent);
  s->append(indent)->append("/ASCIIHexDecode filter\n");
  return s;
}

GBool ASCIIHexStream::isBinary(GBool last) {
  return str->isBinary(gFalse);
}

//------------------------------------------------------------------------
// ASCII85Stream
//------------------------------------------------------------------------

ASCII85Stream::ASCII85Stream(Stream *str):
    FilterStream(str) {
  index = n = 0;
  eof = gFalse;
}

ASCII85Stream::~ASCII85Stream() {
  delete str;
}

void ASCII85Stream::reset() {
  str->reset();
  index = n = 0;
  eof = gFalse;
}

int ASCII85Stream::lookChar() {
  int k;
  Gulong t;

  if (index >= n) {
    if (eof)
      return EOF;
    index = 0;
    do {
      c[0] = str->getChar();
    } while (c[0] == '\n' || c[0] == '\r');
    if (c[0] == '~' || c[0] == EOF) {
      eof = gTrue;
      n = 0;
      return EOF;
    } else if (c[0] == 'z') {
      b[0] = b[1] = b[2] = b[3] = 0;
      n = 4;
    } else {
      for (k = 1; k < 5; ++k) {
	do {
	  c[k] = str->getChar();
	} while (c[k] == '\n' || c[k] == '\r');
	if (c[k] == '~' || c[k] == EOF)
	  break;
      }
      n = k - 1;
      if (k < 5 && (c[k] == '~' || c[k] == EOF)) {
	for (++k; k < 5; ++k)
	  c[k] = 0x21 + 84;
	eof = gTrue;
      }
      t = 0;
      for (k = 0; k < 5; ++k)
	t = t * 85 + (c[k] - 0x21);
      for (k = 3; k >= 0; --k) {
	b[k] = (int)(t & 0xff);
	t >>= 8;
      }
    }
  }
  return b[index];
}

GString *ASCII85Stream::getPSFilter(char *indent) {
  GString *s;

  s = str->getPSFilter(indent);
  s->append(indent)->append("/ASCII85Decode filter\n");
  return s;
}

GBool ASCII85Stream::isBinary(GBool last) {
  return str->isBinary(gFalse);
}

//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------

LZWStream::LZWStream(Stream *str, int predictor1, int columns1, int colors1,
		     int bits1, int early1):
    FilterStream(str) {
  if (predictor1 != 1) {
    pred = new StreamPredictor(this, predictor1, columns1, colors1, bits1);
  } else {
    pred = NULL;
  }
  early = early1;
  zPipe = NULL;
  bufPtr = bufEnd = buf;
}

LZWStream::~LZWStream() {
  if (zPipe) {
#ifdef HAVE_POPEN
    pclose(zPipe);
#else
    fclose(zPipe);
#endif
    zPipe = NULL;
    unlink(zName->getCString());
    delete zName;
  }
  if (pred) {
    delete pred;
  }
  delete str;
}

int LZWStream::getChar() {
  if (pred) {
    return pred->getChar();
  }
  return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
}

int LZWStream::lookChar() {
  if (pred) {
    return pred->lookChar();
  }
  return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
}

int LZWStream::getRawChar() {
  return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
}

void LZWStream::reset() {
  FILE *f;
  GString *zCmd;

  //----- close old LZW stream
  if (zPipe) {
#ifdef HAVE_POPEN
    pclose(zPipe);
#else
    fclose(zPipe);
#endif
    zPipe = NULL;
    unlink(zName->getCString());
    delete zName;
  }

  //----- tell Delorie runtime to spawn a new instance of COMMAND.COM
  //      to run gzip
#if __DJGPP__
  if (!setDJSYSFLAGS) {
    setenv("DJSYSFLAGS", "0x0002", 0);
    setDJSYSFLAGS = gTrue;
  }
#endif

  //----- create the .Z file
  if (!openTempFile(&zName, &f, "wb", ".Z")) {
    error(getPos(), "Couldn't create temporary file for LZW stream");
    return;
  }
  dumpFile(f);
  fclose(f);

  //----- execute uncompress / gzip
  zCmd = new GString(uncompressCmd);
  zCmd->append(' ');
  zCmd->append(zName);
#if defined(MACOS)
  long magicCookie;
  // first we open the engine up
  OSErr err = OpenSITEngine(kUseExternalEngine, &magicCookie);
  // if we found it - let's use it!
  if (!err && magicCookie) {
    // make sure we have the correct version of the Engine
    if (GetSITEngineVersion(magicCookie) >= kFirstSupportedEngine) {
      FSSpec myFSS;
      Str255 pName;
      strcpy((char *)pName, zName->getCString());
      c2pstr((char *)pName);
      FSMakeFSSpec(0, 0, pName, &myFSS);
      short ftype = DetermineFileType(magicCookie, &myFSS);
      OSErr expandErr = ExpandFSSpec(magicCookie, ftype, &myFSS,
				     NULL, NULL, kCreateFolderNever,
				     kDeleteOriginal, kTextConvertSmart);
    }
  }
#elif defined(HAVE_POPEN)
  if (!(zPipe = popen(zCmd->getCString(), POPEN_READ_MODE))) {
    error(getPos(), "Couldn't popen '%s'", zCmd->getCString());
    unlink(zName->getCString());
    delete zName;
    return;
  }
#else // HAVE_POPEN
#ifdef VMS
  if (!system(zCmd->getCString())) {
#else
  if (system(zCmd->getCString())) {
#endif
    error(getPos(), "Couldn't execute '%s'", zCmd->getCString());
    unlink(zName->getCString());
    delete zName;
    return;
  }
  zName->del(zName->getLength() - 2, 2);
  if (!(zPipe = fopen(zName->getCString(), "rb"))) {
    error(getPos(), "Couldn't open uncompress file '%s'", zName->getCString());
    unlink(zName->getCString());
    delete zName;
    return;
  }
#endif // HAVE_POPEN

  //----- clean up
  delete zCmd;

  //----- initialize buffer
  bufPtr = bufEnd = buf;
}

void LZWStream::dumpFile(FILE *f) {
  int outCodeBits;		// size of output code
  int outBits;			// max output code
  int outBuf[8];		// output buffer
  int outData;			// temporary output buffer
  int inCode, outCode;		// input and output codes
  int nextCode;			// next code index
  GBool eof;			// set when EOF is reached
  GBool clear;			// set if table needs to be cleared
  GBool first;			// indicates first code word after clear
  int i, j;

  str->reset();

  // magic number
  fputc(0x1f, f);
  fputc(0x9d, f);

  // max code length, block mode flag
  fputc(0x8c, f);

  // init input side
  inCodeBits = 9;
  inputBuf = 0;
  inputBits = 0;
  eof = gFalse;

  // init output side
  outCodeBits = 9;

  // clear table
  first = gTrue;
  nextCode = 258;

  clear = gFalse;
  do {
    for (i = 0; i < 8; ++i) {
      // check for table overflow
      if (nextCode + early > 0x1001) {
	inCode = 256;

      // read input code
      } else {
	do {
	  inCode = getCode();
	  if (inCode == EOF) {
	    eof = gTrue;
	    inCode = 0;
	  }
	} while (first && inCode == 256);
      }

      // compute output code
      if (inCode < 256) {
	outCode = inCode;
      } else if (inCode == 256) {
	outCode = 256;
	clear = gTrue;
      } else if (inCode == 257) {
	outCode = 0;
	eof = gTrue;
      } else {
	outCode = inCode - 1;
      }
      outBuf[i] = outCode;

      // next code index
      if (first)
	first = gFalse;
      else
	++nextCode;

      // check input code size
      if (nextCode + early == 0x200)
	inCodeBits = 10;
      else if (nextCode + early == 0x400) {
	inCodeBits = 11;
      } else if (nextCode + early == 0x800) {
	inCodeBits = 12;
      }

      // check for eof/clear
      if (eof)
	break;
      if (clear) {
	i = 8;
	break;
      }
    }

    // write output block
    outData = 0;
    outBits = 0;
    j = 0;
    while (j < i || outBits > 0) {
      if (outBits < 8 && j < i) {
	outData = outData | (outBuf[j++] << outBits);
	outBits += outCodeBits;
      }
      fputc(outData & 0xff, f);
      outData >>= 8;
      outBits -= 8;
    }

    // check output code size
    if (nextCode - 1 == 512 ||
	nextCode - 1 == 1024 ||
	nextCode - 1 == 2048 ||
	nextCode - 1 == 4096) {
      outCodeBits = inCodeBits;
    }

    // clear table if necessary
    if (clear) {
      inCodeBits = 9;
      outCodeBits = 9;
      first = gTrue;
      nextCode = 258;
      clear = gFalse;
    }
  } while (!eof);
}

int LZWStream::getCode() {
  int c;
  int code;

  while (inputBits < inCodeBits) {
    if ((c = str->getChar()) == EOF)
      return EOF;
    inputBuf = (inputBuf << 8) | (c & 0xff);
    inputBits += 8;
  }
  code = (inputBuf >> (inputBits - inCodeBits)) & ((1 << inCodeBits) - 1);
  inputBits -= inCodeBits;
  return code;
}

GBool LZWStream::fillBuf() {
  int n;

  if (!zPipe)
    return gFalse;
  if ((n = fread(buf, 1, 256, zPipe)) < 256) {
#ifdef HAVE_POPEN
    pclose(zPipe);
#else
    fclose(zPipe);
#endif
    zPipe = NULL;
    unlink(zName->getCString());
    delete zName;
  }
  bufPtr = buf;
  bufEnd = buf + n;
  return n > 0;
}

GString *LZWStream::getPSFilter(char *indent) {
  GString *s;

  if (pred) {
    return NULL;
  }
  s = str->getPSFilter(indent);
  s->append(indent)->append("/LZWDecode filter\n");
  return s;
}

GBool LZWStream::isBinary(GBool last) {
  return str->isBinary(gTrue);
}

//------------------------------------------------------------------------
// RunLengthStream
//------------------------------------------------------------------------

RunLengthStream::RunLengthStream(Stream *str):
    FilterStream(str) {
  bufPtr = bufEnd = buf;
  eof = gFalse;
}

RunLengthStream::~RunLengthStream() {
  delete str;
}

void RunLengthStream::reset() {
  str->reset();
  bufPtr = bufEnd = buf;
  eof = gFalse;
}

GString *RunLengthStream::getPSFilter(char *indent) {
  GString *s;

  s = str->getPSFilter(indent);
  s->append(indent)->append("/RunLengthDecode filter\n");
  return s;
}

GBool RunLengthStream::isBinary(GBool last) {
  return str->isBinary(gTrue);
}

GBool RunLengthStream::fillBuf() {
  int c;
  int n, i;

  if (eof)
    return gFalse;
  c = str->getChar();
  if (c == 0x80 || c == EOF) {
    eof = gTrue;
    return gFalse;
  }
  if (c < 0x80) {
    n = c + 1;
    for (i = 0; i < n; ++i)
      buf[i] = (char)str->getChar();
  } else {
    n = 0x101 - c;
    c = str->getChar();
    for (i = 0; i < n; ++i)
      buf[i] = (char)c;
  }
  bufPtr = buf;
  bufEnd = buf + n;
  return gTrue;
}

//------------------------------------------------------------------------
// CCITTFaxStream
//------------------------------------------------------------------------

CCITTFaxStream::CCITTFaxStream(Stream *str, int encoding, GBool endOfLine,
			       GBool byteAlign, int columns, int rows,
			       GBool endOfBlock, GBool black):
    FilterStream(str) {
  this->encoding = encoding;
  this->endOfLine = endOfLine;
  this->byteAlign = byteAlign;
  this->columns = columns;
  this->rows = rows;
  this->endOfBlock = endOfBlock;
  this->black = black;
  refLine = (short *)gmalloc((columns + 3) * sizeof(short));
  codingLine = (short *)gmalloc((columns + 2) * sizeof(short));

  eof = gFalse;
  row = 0;
  nextLine2D = encoding < 0;
  inputBits = 0;
  codingLine[0] = 0;
  codingLine[1] = refLine[2] = columns;
  a0 = 1;

  buf = EOF;
}

CCITTFaxStream::~CCITTFaxStream() {
  delete str;
  gfree(refLine);
  gfree(codingLine);
}

void CCITTFaxStream::reset() {
  int n;

  str->reset();
  eof = gFalse;
  row = 0;
  nextLine2D = encoding < 0;
  inputBits = 0;
  codingLine[0] = 0;
  codingLine[1] = refLine[2] = columns;
  a0 = 1;
  buf = EOF;

  // get initial end-of-line marker and 2D encoding tag
  if (endOfBlock) {
    if (lookBits(12) == 0x001) {
      eatBits(12);
    }
  } else {
    for (n = 0; n < 11 && lookBits(n) == 0; ++n) ;
    if (n == 11 && lookBits(12) == 0x001) {
      eatBits(12);
    }
  }
  if (encoding > 0) {
    nextLine2D = !lookBits(1);
    eatBits(1);
  }
}

int CCITTFaxStream::lookChar() {
  short code1, code2, code3;
  int a0New;
#if 0 //~
  GBool err;
#endif
  int ret;
  int bits, i, n;

  // if at eof just return EOF
  if (eof && codingLine[a0] >= columns) {
    return EOF;
  }

  // read the next row
#if 0 //~
  err = gFalse;
#endif
  if (codingLine[a0] >= columns) {

    // 2-D encoding
    if (nextLine2D) {
      for (i = 0; codingLine[i] < columns; ++i)
	refLine[i] = codingLine[i];
      refLine[i] = refLine[i + 1] = columns;
      b1 = 1;
      a0New = codingLine[a0 = 0] = 0;
      do {
	code1 = getTwoDimCode();
	switch (code1) {
	case twoDimPass:
	  if (refLine[b1] < columns) {
	    a0New = refLine[b1 + 1];
	    b1 += 2;
	  }
	  break;
	case twoDimHoriz:
	  if ((a0 & 1) == 0) {
	    code1 = code2 = 0;
	    do {
	      code1 += code3 = getWhiteCode();
	    } while (code3 >= 64);
	    do {
	      code2 += code3 = getBlackCode();
	    } while (code3 >= 64);
	  } else {
	    code1 = code2 = 0;
	    do {
	      code1 += code3 = getBlackCode();
	    } while (code3 >= 64);
	    do {
	      code2 += code3 = getWhiteCode();
	    } while (code3 >= 64);
	  }
	  codingLine[a0 + 1] = a0New + code1;
	  ++a0;
	  a0New = codingLine[a0 + 1] = codingLine[a0] + code2;
	  ++a0;
	  while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	    b1 += 2;
	  break;
	case twoDimVert0:
	  a0New = codingLine[++a0] = refLine[b1];
	  if (refLine[b1] < columns) {
	    ++b1;
	    while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	      b1 += 2;
	  }
	  break;
	case twoDimVertR1:
	  a0New = codingLine[++a0] = refLine[b1] + 1;
	  if (refLine[b1] < columns) {
	    ++b1;
	    while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	      b1 += 2;
	  }
	  break;
	case twoDimVertL1:
	  a0New = codingLine[++a0] = refLine[b1] - 1;
	  --b1;
	  while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	    b1 += 2;
	  break;
	case twoDimVertR2:
	  a0New = codingLine[++a0] = refLine[b1] + 2;
	  if (refLine[b1] < columns) {
	    ++b1;
	    while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	      b1 += 2;
	  }
	  break;
	case twoDimVertL2:
	  a0New = codingLine[++a0] = refLine[b1] - 2;
	  --b1;
	  while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	    b1 += 2;
	  break;
	case twoDimVertR3:
	  a0New = codingLine[++a0] = refLine[b1] + 3;
	  if (refLine[b1] < columns) {
	    ++b1;
	    while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	      b1 += 2;
	  }
	  break;
	case twoDimVertL3:
	  a0New = codingLine[++a0] = refLine[b1] - 3;
	  --b1;
	  while (refLine[b1] <= codingLine[a0] && refLine[b1] < columns)
	    b1 += 2;
	  break;
	case EOF:
	  eof = gTrue;
	  codingLine[a0 = 0] = columns;
	  return EOF;
	default:
	  error(getPos(), "Bad 2D code %04x in CCITTFax stream", code1);
#if 0 //~
	  err = gTrue;
	  break;
#else
	  eof = gTrue;
	  return EOF;
#endif
	}
      } while (codingLine[a0] < columns);

    // 1-D encoding
    } else {
      codingLine[a0 = 0] = 0;
      while (1) {
	code1 = 0;
	do {
	  code1 += code3 = getWhiteCode();
	} while (code3 >= 64);
	codingLine[a0+1] = codingLine[a0] + code1;
	++a0;
	if (codingLine[a0] >= columns)
	  break;
	code2 = 0;
	do {
	  code2 += code3 = getBlackCode();
	} while (code3 >= 64);
	codingLine[a0+1] = codingLine[a0] + code2;
	++a0;
	if (codingLine[a0] >= columns)
	  break;
      }
    }

    if (codingLine[a0] != columns) {
      error(getPos(), "CCITTFax row is wrong length (%d)", codingLine[a0]);
#if 0 //~
      err = gTrue;
#endif
    }

    // byte-align the row
    if (byteAlign) {
      inputBits &= ~7;
    }

    // check for end-of-line marker, end-of-block marker, and
    // 2D encoding tag
    if (endOfBlock) {
      code1 = lookBits(12);
      if (code1 == EOF) {
	eof = gTrue;
      } else if (code1 == 0x001) {
	eatBits(12);
	if (encoding > 0) {
	  nextLine2D = !lookBits(1);
	  eatBits(1);
	}
	code1 = lookBits(12);
	if (code1 == 0x001) {
	  eatBits(12);
	  if (encoding > 0) {
	    lookBits(1);
	    eatBits(1);
	  }
	  if (encoding >= 0) {
	    for (i = 0; i < 4; ++i) {
	      code1 = lookBits(12);
	      if (code1 != 0x001) {
		error(getPos(), "Bad RTC code in CCITTFax stream");
	      }
	      eatBits(12);
	      if (encoding > 0) {
		lookBits(1);
		eatBits(1);
	      }
	    }
	  }
	  eof = gTrue;
	}
      } else {
	if (encoding > 0) {
	  nextLine2D = !lookBits(1);
	  eatBits(1);
	}
      }
    } else {
      if (row == rows - 1) {
	eof = gTrue;
      } else {
	for (n = 0; n < 11 && lookBits(n) == 0; ++n) ;
	if (n == 11 && lookBits(12) == 0x001) {
	  eatBits(12);
	}
	if (encoding > 0) {
	  nextLine2D = !lookBits(1);
	  eatBits(1);
	}
      }
    }

#if 0 //~
    // This looks for an end-of-line marker after an error, however
    // some (most?) CCITT streams in PDF files don't use end-of-line
    // markers, and the just-plow-on technique works better in those
    // cases.
    else if (err) {
      do {
	if (code1 == EOF) {
	  eof = gTrue;
	  return EOF;
	}
	eatBits(1);
	code1 = look13Bits();
      } while ((code1 >> 1) != 0x001);
      eatBits(12); 
      codingLine[++a0] = columns;
      if (encoding > 0) {
	eatBits(1);
	nextLine2D = !(code1 & 1);
      }
    }
#endif

    a0 = 0;
    outputBits = codingLine[1] - codingLine[0];
    if (outputBits == 0) {
      a0 = 1;
      outputBits = codingLine[2] - codingLine[1];
    }

    ++row;
  }

  // get a byte
  if (outputBits >= 8) {
    ret = ((a0 & 1) == 0) ? 0xff : 0x00;
    if ((outputBits -= 8) == 0) {
      ++a0;
      if (codingLine[a0] < columns) {
	outputBits = codingLine[a0 + 1] - codingLine[a0];
      }
    }
  } else {
    bits = 8;
    ret = 0;
    do {
      if (outputBits > bits) {
	i = bits;
	bits = 0;
	if ((a0 & 1) == 0) {
	  ret |= 0xff >> (8 - i);
	}
	outputBits -= i;
      } else {
	i = outputBits;
	bits -= outputBits;
	if ((a0 & 1) == 0) {
	  ret |= (0xff >> (8 - i)) << bits;
	}
	outputBits = 0;
	++a0;
	if (codingLine[a0] < columns) {
	  outputBits = codingLine[a0 + 1] - codingLine[a0];
	}
      }
    } while (bits > 0 && codingLine[a0] < columns);
  }
  buf = black ? (ret ^ 0xff) : ret;
  return buf;
}

short CCITTFaxStream::getTwoDimCode() {
  short code;
  CCITTCode *p;
  int n;

  code = 0; // make gcc happy
  if (endOfBlock) {
    code = lookBits(7);
    p = &twoDimTab1[code];
    if (p->bits > 0) {
      eatBits(p->bits);
      return p->n;
    }
  } else {
    for (n = 1; n <= 7; ++n) {
      code = lookBits(n);
      if (n < 7) {
	code <<= 7 - n;
      }
      p = &twoDimTab1[code];
      if (p->bits == n) {
	eatBits(n);
	return p->n;
      }
    }
  }
  error(getPos(), "Bad two dim code (%04x) in CCITTFax stream", code);
  return EOF;
}

short CCITTFaxStream::getWhiteCode() {
  short code;
  CCITTCode *p;
  int n;

  code = 0; // make gcc happy
  if (endOfBlock) {
    code = lookBits(12);
    if ((code >> 5) == 0)
      p = &whiteTab1[code];
    else
      p = &whiteTab2[code >> 3];
    if (p->bits > 0) {
      eatBits(p->bits);
      return p->n;
    }
  } else {
    for (n = 1; n <= 9; ++n) {
      code = lookBits(n);
      if (n < 9) {
	code <<= 9 - n;
      }
      p = &whiteTab2[code];
      if (p->bits == n) {
	eatBits(n);
	return p->n;
      }
    }
    for (n = 11; n <= 12; ++n) {
      code = lookBits(n);
      if (n < 12) {
	code <<= 12 - n;
      }
      p = &whiteTab1[code];
      if (p->bits == n) {
	eatBits(n);
	return p->n;
      }
    }
  }
  error(getPos(), "Bad white code (%04x) in CCITTFax stream", code);
  return EOF;
}

short CCITTFaxStream::getBlackCode() {
  short code;
  CCITTCode *p;
  int n;

  code = 0; // make gcc happy
  if (endOfBlock) {
    code = lookBits(13);
    if ((code >> 7) == 0)
      p = &blackTab1[code];
    else if ((code >> 9) == 0)
      p = &blackTab2[(code >> 1) - 64];
    else
      p = &blackTab3[code >> 7];
    if (p->bits > 0) {
      eatBits(p->bits);
      return p->n;
    }
  } else {
    for (n = 2; n <= 6; ++n) {
      code = lookBits(n);
      if (n < 6) {
	code <<= 6 - n;
      }
      p = &blackTab3[code];
      if (p->bits == n) {
	eatBits(n);
	return p->n;
      }
    }
    for (n = 7; n <= 12; ++n) {
      code = lookBits(n);
      if (n < 12) {
	code <<= 12 - n;
      }
      if (code >= 64) {
	p = &blackTab2[code - 64];
	if (p->bits == n) {
	  eatBits(n);
	  return p->n;
	}
      }
    }
    for (n = 10; n <= 13; ++n) {
      code = lookBits(n);
      if (n < 13) {
	code <<= 13 - n;
      }
      p = &blackTab1[code];
      if (p->bits == n) {
	eatBits(n);
	return p->n;
      }
    }
  }
  error(getPos(), "Bad black code (%04x) in CCITTFax stream", code);
  return EOF;
}

short CCITTFaxStream::lookBits(int n) {
  int c;

  while (inputBits < n) {
    if ((c = str->getChar()) == EOF) {
      if (inputBits == 0)
	return EOF;
      c = 0;
    }
    inputBuf = (inputBuf << 8) + c;
    inputBits += 8;
  }
  return (inputBuf >> (inputBits - n)) & (0xffff >> (16 - n));
}

GString *CCITTFaxStream::getPSFilter(char *indent) {
  GString *s;
  char s1[50];

  s = str->getPSFilter(indent);
  s->append(indent)->append("<< ");
  if (encoding != 0) {
    sprintf(s1, "/K %d ", encoding);
    s->append(s1);
  }
  if (endOfLine) {
    s->append("/EndOfLine true ");
  }
  if (byteAlign) {
    s->append("/EncodedByteAlign true ");
  }
  sprintf(s1, "/Columns %d ", columns);
  s->append(s1);
  if (rows != 0) {
    sprintf(s1, "/Rows %d ", rows);
    s->append(s1);
  }
  if (!endOfBlock) {
    s->append("/EndOfBlock false ");
  }
  if (black) {
    s->append("/BlackIs1 true ");
  }
  s->append(">> /CCITTFaxDecode filter\n");
  return s;
}

GBool CCITTFaxStream::isBinary(GBool last) {
  return str->isBinary(gTrue);
}

//------------------------------------------------------------------------
// DCTStream
//------------------------------------------------------------------------

// IDCT constants (20.12 fixed point format)
#ifndef FP_IDCT
#define dctCos1    4017		// cos(pi/16)
#define dctSin1     799		// sin(pi/16)
#define dctCos3    3406		// cos(3*pi/16)
#define dctSin3    2276		// sin(3*pi/16)
#define dctCos6    1567		// cos(6*pi/16)
#define dctSin6    3784		// sin(6*pi/16)
#define dctSqrt2   5793		// sqrt(2)
#define dctSqrt1d2 2896		// sqrt(2) / 2
#endif

// IDCT constants
#ifdef FP_IDCT
#define dctCos1    0.98078528	// cos(pi/16)
#define dctSin1    0.19509032	// sin(pi/16)
#define dctCos3    0.83146961	// cos(3*pi/16)
#define dctSin3    0.55557023	// sin(3*pi/16)
#define dctCos6    0.38268343	// cos(6*pi/16)
#define dctSin6    0.92387953	// sin(6*pi/16)
#define dctSqrt2   1.41421356	// sqrt(2)
#define dctSqrt1d2 0.70710678	// sqrt(2) / 2
#endif

// color conversion parameters (16.16 fixed point format)
#define dctCrToR   91881	//  1.4020
#define dctCbToG  -22553	// -0.3441363
#define dctCrToG  -46802	// -0.71413636
#define dctCbToB  116130	//  1.772

// clip [-256,511] --> [0,255]
#define dctClipOffset 256
static Guchar dctClip[768];
static int dctClipInit = 0;

// zig zag decode map
static int dctZigZag[64] = {
   0,
   1,  8,
  16,  9,  2,
   3, 10, 17, 24,
  32, 25, 18, 11, 4,
   5, 12, 19, 26, 33, 40,
  48, 41, 34, 27, 20, 13,  6,
   7, 14, 21, 28, 35, 42, 49, 56,
  57, 50, 43, 36, 29, 22, 15,
  23, 30, 37, 44, 51, 58,
  59, 52, 45, 38, 31,
  39, 46, 53, 60,
  61, 54, 47,
  55, 62,
  63
};

DCTStream::DCTStream(Stream *str):
    FilterStream(str) {
  int i, j;

  width = height = 0;
  mcuWidth = mcuHeight = 0;
  numComps = 0;
  comp = 0;
  x = y = dy = 0;
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 32; ++j)
      rowBuf[i][j] = NULL;

  if (!dctClipInit) {
    for (i = -256; i < 0; ++i)
      dctClip[dctClipOffset + i] = 0;
    for (i = 0; i < 256; ++i)
      dctClip[dctClipOffset + i] = i;
    for (i = 256; i < 512; ++i)
      dctClip[dctClipOffset + i] = 255;
    dctClipInit = 1;
  }
}

DCTStream::~DCTStream() {
  int i, j;

  delete str;
  for (i = 0; i < numComps; ++i)
    for (j = 0; j < mcuHeight; ++j)
      gfree(rowBuf[i][j]);
}

void DCTStream::reset() {
  str->reset();
  if (!readHeader()) {
    y = height;
    return;
  }
  restartMarker = 0xd0;
  restart();
}

int DCTStream::getChar() {
  int c;

  c = lookChar();
  if (c == EOF)
    return EOF;
  if (++comp == numComps) {
    comp = 0;
    if (++x == width) {
      x = 0;
      ++y;
      ++dy;
    }
  }
  if (y == height)
    readTrailer();
  return c;
}

int DCTStream::lookChar() {
  if (y >= height)
    return EOF;
  if (dy >= mcuHeight) {
    if (!readMCURow()) {
      y = height;
      return EOF;
    }
    comp = 0;
    x = 0;
    dy = 0;
  }
  return rowBuf[comp][dy][x];
}

void DCTStream::restart() {
  int i;

  inputBits = 0;
  restartCtr = restartInterval;
  for (i = 0; i < numComps; ++i)
    compInfo[i].prevDC = 0;
}

GBool DCTStream::readMCURow() {
  Guchar data[64];
  Guchar *p1, *p2;
  int pY, pCb, pCr, pR, pG, pB;
  int h, v, horiz, vert, hSub, vSub;
  int x1, x2, y2, x3, y3, x4, y4, x5, y5, cc, i;
  int c;

  for (x1 = 0; x1 < width; x1 += mcuWidth) {

    // deal with restart marker
    if (restartInterval > 0 && restartCtr == 0) {
      c = readMarker();
      if (c != restartMarker) {
	error(getPos(), "Bad DCT data: incorrect restart marker");
	return gFalse;
      }
      if (++restartMarker == 0xd8)
	restartMarker = 0xd0;
      restart();
    }

    // read one MCU
    for (cc = 0; cc < numComps; ++cc) {
      h = compInfo[cc].hSample;
      v = compInfo[cc].vSample;
      horiz = mcuWidth / h;
      vert = mcuHeight / v;
      hSub = horiz / 8;
      vSub = vert / 8;
      for (y2 = 0; y2 < mcuHeight; y2 += vert) {
	for (x2 = 0; x2 < mcuWidth; x2 += horiz) {
	  if (!readDataUnit(&dcHuffTables[compInfo[cc].dcHuffTable],
			    &acHuffTables[compInfo[cc].acHuffTable],
			    quantTables[compInfo[cc].quantTable],
			    &compInfo[cc].prevDC,
			    data))
	    return gFalse;
	  if (hSub == 1 && vSub == 1) {
	    for (y3 = 0, i = 0; y3 < 8; ++y3, i += 8) {
	      p1 = &rowBuf[cc][y2+y3][x1+x2];
	      p1[0] = data[i];
	      p1[1] = data[i+1];
	      p1[2] = data[i+2];
	      p1[3] = data[i+3];
	      p1[4] = data[i+4];
	      p1[5] = data[i+5];
	      p1[6] = data[i+6];
	      p1[7] = data[i+7];
	    }
	  } else if (hSub == 2 && vSub == 2) {
	    for (y3 = 0, i = 0; y3 < 16; y3 += 2, i += 8) {
	      p1 = &rowBuf[cc][y2+y3][x1+x2];
	      p2 = &rowBuf[cc][y2+y3+1][x1+x2];
	      p1[0] = p1[1] = p2[0] = p2[1] = data[i];
	      p1[2] = p1[3] = p2[2] = p2[3] = data[i+1];
	      p1[4] = p1[5] = p2[4] = p2[5] = data[i+2];
	      p1[6] = p1[7] = p2[6] = p2[7] = data[i+3];
	      p1[8] = p1[9] = p2[8] = p2[9] = data[i+4];
	      p1[10] = p1[11] = p2[10] = p2[11] = data[i+5];
	      p1[12] = p1[13] = p2[12] = p2[13] = data[i+6];
	      p1[14] = p1[15] = p2[14] = p2[15] = data[i+7];
	    }
	  } else {
	    i = 0;
	    for (y3 = 0, y4 = 0; y3 < 8; ++y3, y4 += vSub) {
	      for (x3 = 0, x4 = 0; x3 < 8; ++x3, x4 += hSub) {
		for (y5 = 0; y5 < vSub; ++y5)
		  for (x5 = 0; x5 < hSub; ++x5)
		    rowBuf[cc][y2+y4+y5][x1+x2+x4+x5] = data[i];
		++i;
	      }
	    }
	  }
	}
      }
    }
    --restartCtr;

    // color space conversion
    if (colorXform) {
      // convert YCbCr to RGB
      if (numComps == 3) {
	for (y2 = 0; y2 < mcuHeight; ++y2) {
	  for (x2 = 0; x2 < mcuWidth; ++x2) {
	    pY = rowBuf[0][y2][x1+x2];
	    pCb = rowBuf[1][y2][x1+x2] - 128;
	    pCr = rowBuf[2][y2][x1+x2] - 128;
	    pR = ((pY << 16) + dctCrToR * pCr + 32768) >> 16;
	    rowBuf[0][y2][x1+x2] = dctClip[dctClipOffset + pR];
	    pG = ((pY << 16) + dctCbToG * pCb + dctCrToG * pCr + 32768) >> 16;
	    rowBuf[1][y2][x1+x2] = dctClip[dctClipOffset + pG];
	    pB = ((pY << 16) + dctCbToB * pCb + 32768) >> 16;
	    rowBuf[2][y2][x1+x2] = dctClip[dctClipOffset + pB];
	  }
	}
      // convert YCbCrK to CMYK (K is passed through unchanged)
      } else if (numComps == 4) {
	for (y2 = 0; y2 < mcuHeight; ++y2) {
	  for (x2 = 0; x2 < mcuWidth; ++x2) {
	    pY = rowBuf[0][y2][x1+x2];
	    pCb = rowBuf[1][y2][x1+x2] - 128;
	    pCr = rowBuf[2][y2][x1+x2] - 128;
	    pR = ((pY << 16) + dctCrToR * pCr + 32768) >> 16;
	    rowBuf[0][y2][x1+x2] = 255 - dctClip[dctClipOffset + pR];
	    pG = ((pY << 16) + dctCbToG * pCb + dctCrToG * pCr + 32678) >> 16;
	    rowBuf[1][y2][x1+x2] = 255 - dctClip[dctClipOffset + pG];
	    pB = ((pY << 16) + dctCbToB * pCb + 32768) >> 16;
	    rowBuf[2][y2][x1+x2] = 255 - dctClip[dctClipOffset + pB];
	  }
	}
      }
    }
  }
  return gTrue;
}

// This IDCT algorithm is taken from:
//   Christoph Loeffler, Adriaan Ligtenberg, George S. Moschytz,
//   "Practical Fast 1-D DCT Algorithms with 11 Multiplications",
//   IEEE Intl. Conf. on Acoustics, Speech & Signal Processing, 1989,
//   988-991.
// The stage numbers mentioned in the comments refer to Figure 1 in this
// paper.
#ifndef FP_IDCT
GBool DCTStream::readDataUnit(DCTHuffTable *dcHuffTable,
			      DCTHuffTable *acHuffTable,
			      Guchar quantTable[64], int *prevDC,
			      Guchar data[64]) {
  int tmp1[64];
  int v0, v1, v2, v3, v4, v5, v6, v7, t;
  int run, size, amp;
  int c;
  int i, j;

  // Huffman decode and dequantize
  size = readHuffSym(dcHuffTable);
  if (size == 9999)
    return gFalse;
  if (size > 0) {
    amp = readAmp(size);
    if (amp == 9999)
      return gFalse;
  } else {
    amp = 0;
  }
  tmp1[0] = (*prevDC += amp) * quantTable[0];
  for (i = 1; i < 64; ++i)
    tmp1[i] = 0;
  i = 1;
  while (i < 64) {
    run = 0;
    while ((c = readHuffSym(acHuffTable)) == 0xf0 && run < 0x30)
      run += 0x10;
    if (c == 9999)
      return gFalse;
    if (c == 0x00) {
      break;
    } else {
      run += (c >> 4) & 0x0f;
      size = c & 0x0f;
      amp = readAmp(size);
      if (amp == 9999)
	return gFalse;
      i += run;
      j = dctZigZag[i++];
      tmp1[j] = amp * quantTable[j];
    }
  }

  // inverse DCT on rows
  for (i = 0; i < 64; i += 8) {

    // stage 4
    v0 = (dctSqrt2 * tmp1[i+0] + 128) >> 8;
    v1 = (dctSqrt2 * tmp1[i+4] + 128) >> 8;
    v2 = tmp1[i+2];
    v3 = tmp1[i+6];
    v4 = (dctSqrt1d2 * (tmp1[i+1] - tmp1[i+7]) + 128) >> 8;
    v7 = (dctSqrt1d2 * (tmp1[i+1] + tmp1[i+7]) + 128) >> 8;
    v5 = tmp1[i+3] << 4;
    v6 = tmp1[i+5] << 4;

    // stage 3
    t = (v0 - v1+ 1) >> 1;
    v0 = (v0 + v1 + 1) >> 1;
    v1 = t;
    t = (v2 * dctSin6 + v3 * dctCos6 + 128) >> 8;
    v2 = (v2 * dctCos6 - v3 * dctSin6 + 128) >> 8;
    v3 = t;
    t = (v4 - v6 + 1) >> 1;
    v4 = (v4 + v6 + 1) >> 1;
    v6 = t;
    t = (v7 + v5 + 1) >> 1;
    v5 = (v7 - v5 + 1) >> 1;
    v7 = t;

    // stage 2
    t = (v0 - v3 + 1) >> 1;
    v0 = (v0 + v3 + 1) >> 1;
    v3 = t;
    t = (v1 - v2 + 1) >> 1;
    v1 = (v1 + v2 + 1) >> 1;
    v2 = t;
    t = (v4 * dctSin3 + v7 * dctCos3 + 2048) >> 12;
    v4 = (v4 * dctCos3 - v7 * dctSin3 + 2048) >> 12;
    v7 = t;
    t = (v5 * dctSin1 + v6 * dctCos1 + 2048) >> 12;
    v5 = (v5 * dctCos1 - v6 * dctSin1 + 2048) >> 12;
    v6 = t;

    // stage 1
    tmp1[i+0] = v0 + v7;
    tmp1[i+7] = v0 - v7;
    tmp1[i+1] = v1 + v6;
    tmp1[i+6] = v1 - v6;
    tmp1[i+2] = v2 + v5;
    tmp1[i+5] = v2 - v5;
    tmp1[i+3] = v3 + v4;
    tmp1[i+4] = v3 - v4;
  }

  // inverse DCT on columns
  for (i = 0; i < 8; ++i) {

    // stage 4
    v0 = (dctSqrt2 * tmp1[0*8+i] + 2048) >> 12;
    v1 = (dctSqrt2 * tmp1[4*8+i] + 2048) >> 12;
    v2 = tmp1[2*8+i];
    v3 = tmp1[6*8+i];
    v4 = (dctSqrt1d2 * (tmp1[1*8+i] - tmp1[7*8+i]) + 2048) >> 12;
    v7 = (dctSqrt1d2 * (tmp1[1*8+i] + tmp1[7*8+i]) + 2048) >> 12;
    v5 = tmp1[3*8+i];
    v6 = tmp1[5*8+i];

    // stage 3
    t = (v0 - v1 + 1) >> 1;
    v0 = (v0 + v1 + 1) >> 1;
    v1 = t;
    t = (v2 * dctSin6 + v3 * dctCos6 + 2048) >> 12;
    v2 = (v2 * dctCos6 - v3 * dctSin6 + 2048) >> 12;
    v3 = t;
    t = (v4 - v6 + 1) >> 1;
    v4 = (v4 + v6 + 1) >> 1;
    v6 = t;
    t = (v7 + v5 + 1) >> 1;
    v5 = (v7 - v5 + 1) >> 1;
    v7 = t;

    // stage 2
    t = (v0 - v3 + 1) >> 1;
    v0 = (v0 + v3 + 1) >> 1;
    v3 = t;
    t = (v1 - v2 + 1) >> 1;
    v1 = (v1 + v2 + 1) >> 1;
    v2 = t;
    t = (v4 * dctSin3 + v7 * dctCos3 + 2048) >> 12;
    v4 = (v4 * dctCos3 - v7 * dctSin3 + 2048) >> 12;
    v7 = t;
    t = (v5 * dctSin1 + v6 * dctCos1 + 2048) >> 12;
    v5 = (v5 * dctCos1 - v6 * dctSin1 + 2048) >> 12;
    v6 = t;

    // stage 1
    tmp1[0*8+i] = v0 + v7;
    tmp1[7*8+i] = v0 - v7;
    tmp1[1*8+i] = v1 + v6;
    tmp1[6*8+i] = v1 - v6;
    tmp1[2*8+i] = v2 + v5;
    tmp1[5*8+i] = v2 - v5;
    tmp1[3*8+i] = v3 + v4;
    tmp1[4*8+i] = v3 - v4;
  }

  // convert to 8-bit integers
  for (i = 0; i < 64; ++i)
    data[i] = dctClip[dctClipOffset + 128 + ((tmp1[i] + 8) >> 4)];

  return gTrue;
}
#endif

#ifdef FP_IDCT
GBool DCTStream::readDataUnit(DCTHuffTable *dcHuffTable,
			      DCTHuffTable *acHuffTable,
			      Guchar quantTable[64], int *prevDC,
			      Guchar data[64]) {
  double tmp1[64];
  double v0, v1, v2, v3, v4, v5, v6, v7, t;
  int run, size, amp;
  int c;
  int i, j;

  // Huffman decode and dequantize
  size = readHuffSym(dcHuffTable);
  if (size == 9999)
    return gFalse;
  if (size > 0) {
    amp = readAmp(size);
    if (amp == 9999)
      return gFalse;
  } else {
    amp = 0;
  }
  tmp1[0] = (*prevDC += amp) * quantTable[0];
  for (i = 1; i < 64; ++i)
    tmp1[i] = 0;
  i = 1;
  while (i < 64) {
    run = 0;
    while ((c = readHuffSym(acHuffTable)) == 0xf0 && run < 0x30)
      run += 0x10;
    if (c == 9999)
      return gFalse;
    if (c == 0x00) {
      break;
    } else {
      run += (c >> 4) & 0x0f;
      size = c & 0x0f;
      amp = readAmp(size);
      if (amp == 9999)
	return gFalse;
      i += run;
      j = dctZigZag[i++];
      tmp1[j] = amp * quantTable[j];
    }
  }

  // inverse DCT on rows
  for (i = 0; i < 64; i += 8) {

    // stage 4
    v0 = dctSqrt2 * tmp1[i+0];
    v1 = dctSqrt2 * tmp1[i+4];
    v2 = tmp1[i+2];
    v3 = tmp1[i+6];
    v4 = dctSqrt1d2 * (tmp1[i+1] - tmp1[i+7]);
    v7 = dctSqrt1d2 * (tmp1[i+1] + tmp1[i+7]);
    v5 = tmp1[i+3];
    v6 = tmp1[i+5];

    // stage 3
    t = 0.5 * (v0 - v1);
    v0 = 0.5 * (v0 + v1);
    v1 = t;
    t = v2 * dctSin6 + v3 * dctCos6;
    v2 = v2 * dctCos6 - v3 * dctSin6;
    v3 = t;
    t = 0.5 * (v4 - v6);
    v4 = 0.5 * (v4 + v6);
    v6 = t;
    t = 0.5 * (v7 + v5);
    v5 = 0.5 * (v7 - v5);
    v7 = t;

    // stage 2
    t = 0.5 * (v0 - v3);
    v0 = 0.5 * (v0 + v3);
    v3 = t;
    t = 0.5 * (v1 - v2);
    v1 = 0.5 * (v1 + v2);
    v2 = t;
    t = v4 * dctSin3 + v7 * dctCos3;
    v4 = v4 * dctCos3 - v7 * dctSin3;
    v7 = t;
    t = v5 * dctSin1 + v6 * dctCos1;
    v5 = v5 * dctCos1 - v6 * dctSin1;
    v6 = t;

    // stage 1
    tmp1[i+0] = v0 + v7;
    tmp1[i+7] = v0 - v7;
    tmp1[i+1] = v1 + v6;
    tmp1[i+6] = v1 - v6;
    tmp1[i+2] = v2 + v5;
    tmp1[i+5] = v2 - v5;
    tmp1[i+3] = v3 + v4;
    tmp1[i+4] = v3 - v4;
  }

  // inverse DCT on columns
  for (i = 0; i < 8; ++i) {

    // stage 4
    v0 = dctSqrt2 * tmp1[0*8+i];
    v1 = dctSqrt2 * tmp1[4*8+i];
    v2 = tmp1[2*8+i];
    v3 = tmp1[6*8+i];
    v4 = dctSqrt1d2 * (tmp1[1*8+i] - tmp1[7*8+i]);
    v7 = dctSqrt1d2 * (tmp1[1*8+i] + tmp1[7*8+i]);
    v5 = tmp1[3*8+i];
    v6 = tmp1[5*8+i];

    // stage 3
    t = 0.5 * (v0 - v1);
    v0 = 0.5 * (v0 + v1);
    v1 = t;
    t = v2 * dctSin6 + v3 * dctCos6;
    v2 = v2 * dctCos6 - v3 * dctSin6;
    v3 = t;
    t = 0.5 * (v4 - v6);
    v4 = 0.5 * (v4 + v6);
    v6 = t;
    t = 0.5 * (v7 + v5);
    v5 = 0.5 * (v7 - v5);
    v7 = t;

    // stage 2
    t = 0.5 * (v0 - v3);
    v0 = 0.5 * (v0 + v3);
    v3 = t;
    t = 0.5 * (v1 - v2);
    v1 = 0.5 * (v1 + v2);
    v2 = t;
    t = v4 * dctSin3 + v7 * dctCos3;
    v4 = v4 * dctCos3 - v7 * dctSin3;
    v7 = t;
    t = v5 * dctSin1 + v6 * dctCos1;
    v5 = v5 * dctCos1 - v6 * dctSin1;
    v6 = t;

    // stage 1
    tmp1[0*8+i] = v0 + v7;
    tmp1[7*8+i] = v0 - v7;
    tmp1[1*8+i] = v1 + v6;
    tmp1[6*8+i] = v1 - v6;
    tmp1[2*8+i] = v2 + v5;
    tmp1[5*8+i] = v2 - v5;
    tmp1[3*8+i] = v3 + v4;
    tmp1[4*8+i] = v3 - v4;
  }

  // convert to 8-bit integers
  for (i = 0; i < 64; ++i)
    data[i] = dctClip[dctClipOffset + (int)(tmp1[i] + 128.5)];

  return gTrue;
}
#endif

int DCTStream::readHuffSym(DCTHuffTable *table) {
  Gushort code;
  int bit;
  int codeBits;

  code = 0;
  codeBits = 0;
  do {
    // add a bit to the code
    if ((bit = readBit()) == EOF)
      return 9999;
    code = (code << 1) + bit;
    ++codeBits;

    // look up code
    if (code - table->firstCode[codeBits] < table->numCodes[codeBits]) {
      code -= table->firstCode[codeBits];
      return table->sym[table->firstSym[codeBits] + code];
    }
  } while (codeBits < 16);

  error(getPos(), "Bad Huffman code in DCT stream");
  return 9999;
}

int DCTStream::readAmp(int size) {
  int amp, bit;
  int bits;

  amp = 0;
  for (bits = 0; bits < size; ++bits) {
    if ((bit = readBit()) == EOF)
      return 9999;
    amp = (amp << 1) + bit;
  }
  if (amp < (1 << (size - 1)))
    amp -= (1 << size) - 1;
  return amp;
}

int DCTStream::readBit() {
  int bit;
  int c, c2;

  if (inputBits == 0) {
    if ((c = str->getChar()) == EOF)
      return EOF;
    if (c == 0xff) {
      do {
	c2 = str->getChar();
      } while (c2 == 0xff);
      if (c2 != 0x00) {
	error(getPos(), "Bad DCT data: missing 00 after ff");
	return EOF;
      }
    }
    inputBuf = c;
    inputBits = 8;
  }
  bit = (inputBuf >> (inputBits - 1)) & 1;
  --inputBits;
  return bit;
}

GBool DCTStream::readHeader() {
  GBool doScan;
  int minHSample, minVSample;
  int bufWidth;
  int n;
  int c = 0;
  int i, j;

  width = height = 0;
  numComps = 0;
  numQuantTables = 0;
  numDCHuffTables = 0;
  numACHuffTables = 0;
  colorXform = 0;
  gotAdobeMarker = gFalse;
  restartInterval = 0;

  // read headers
  doScan = gFalse;
  while (!doScan) {
    c = readMarker();
    switch (c) {
    case 0xc0:			// SOF0
      if (!readFrameInfo())
	return gFalse;
      break;
    case 0xc4:			// DHT
      if (!readHuffmanTables())
	return gFalse;
      break;
    case 0xd8:			// SOI
      break;
    case 0xda:			// SOS
      if (!readScanInfo())
	return gFalse;
      doScan = gTrue;
      break;
    case 0xdb:			// DQT
      if (!readQuantTables())
	return gFalse;
      break;
    case 0xdd:			// DRI
      if (!readRestartInterval())
	return gFalse;
      break;
    case 0xee:			// APP14
      if (!readAdobeMarker())
	return gFalse;
      break;
    case EOF:
      error(getPos(), "Bad DCT header");
      return gFalse;
    default:
      // skip APPn / COM / etc.
      if (c >= 0xe0) {
	n = read16() - 2;
	for (i = 0; i < n; ++i)
	  str->getChar();
      } else {
	error(getPos(), "Unknown DCT marker <%02x>", c);
	return gFalse;
      }
      break;
    }
  }

  // compute MCU size
  mcuWidth = minHSample = compInfo[0].hSample;
  mcuHeight = minVSample = compInfo[0].vSample;
  for (i = 1; i < numComps; ++i) {
    if (compInfo[i].hSample < minHSample)
      minHSample = compInfo[i].hSample;
    if (compInfo[i].vSample < minVSample)
      minVSample = compInfo[i].vSample;
    if (compInfo[i].hSample > mcuWidth)
      mcuWidth = compInfo[i].hSample;
    if (compInfo[i].vSample > mcuHeight)
      mcuHeight = compInfo[i].vSample;
  }
  for (i = 0; i < numComps; ++i) {
    compInfo[i].hSample /= minHSample;
    compInfo[i].vSample /= minVSample;
  }
  mcuWidth = (mcuWidth / minHSample) * 8;
  mcuHeight = (mcuHeight / minVSample) * 8;

  // allocate buffers
  bufWidth = ((width + mcuWidth - 1) / mcuWidth) * mcuWidth;
  for (i = 0; i < numComps; ++i)
    for (j = 0; j < mcuHeight; ++j)
      rowBuf[i][j] = (Guchar *)gmalloc(bufWidth * sizeof(Guchar));

  // figure out color transform
  if (!gotAdobeMarker && numComps == 3) {
    if (compInfo[0].id == 1 && compInfo[1].id == 2 && compInfo[2].id == 3) {
      colorXform = 1;
    }
  }

  // initialize counters
  comp = 0;
  x = 0;
  y = 0;
  dy = mcuHeight;

  return gTrue;
}

GBool DCTStream::readFrameInfo() {
  int length;
  int prec;
  int i;
  int c;

  length = read16() - 2;
  prec = str->getChar();
  height = read16();
  width = read16();
  numComps = str->getChar();
  length -= 6;
  if (prec != 8) {
    error(getPos(), "Bad DCT precision %d", prec);
    return gFalse;
  }
  for (i = 0; i < numComps; ++i) {
    compInfo[i].id = str->getChar();
    compInfo[i].inScan = gFalse;
    c = str->getChar();
    compInfo[i].hSample = (c >> 4) & 0x0f;
    compInfo[i].vSample = c & 0x0f;
    compInfo[i].quantTable = str->getChar();
    compInfo[i].dcHuffTable = 0;
    compInfo[i].acHuffTable = 0;
  }
  return gTrue;
}

GBool DCTStream::readScanInfo() {
  int length;
  int scanComps, id, c;
  int i, j;

  length = read16() - 2;
  scanComps = str->getChar();
  --length;
  if (length != 2 * scanComps + 3) {
    error(getPos(), "Bad DCT scan info block");
    return gFalse;
  }
  for (i = 0; i < scanComps; ++i) {
    id = str->getChar();
    for (j = 0; j < numComps; ++j) {
      if (id == compInfo[j].id)
	break;
    }
    if (j == numComps) {
      error(getPos(), "Bad DCT component ID in scan info block");
      return gFalse;
    }
    compInfo[j].inScan = gTrue;
    c = str->getChar();
    compInfo[j].dcHuffTable = (c >> 4) & 0x0f;
    compInfo[j].acHuffTable = c & 0x0f;
  }
  str->getChar();
  str->getChar();
  str->getChar();
  return gTrue;
}

GBool DCTStream::readQuantTables() {
  int length;
  int i;
  int index;

  length = read16() - 2;
  while (length > 0) {
    index = str->getChar();
    if ((index & 0xf0) || index >= 4) {
      error(getPos(), "Bad DCT quantization table");
      return gFalse;
    }
    if (index == numQuantTables)
      numQuantTables = index + 1;
    for (i = 0; i < 64; ++i)
      quantTables[index][dctZigZag[i]] = str->getChar();
    length -= 65;
  }
  return gTrue;
}

GBool DCTStream::readHuffmanTables() {
  DCTHuffTable *tbl;
  int length;
  int index;
  Gushort code;
  Guchar sym;
  int i;
  int c;

  length = read16() - 2;
  while (length > 0) {
    index = str->getChar();
    --length;
    if ((index & 0x0f) >= 4) {
      error(getPos(), "Bad DCT Huffman table");
      return gFalse;
    }
    if (index & 0x10) {
      index &= 0x0f;
      if (index >= numACHuffTables)
	numACHuffTables = index+1;
      tbl = &acHuffTables[index];
    } else {
      if (index >= numDCHuffTables)
	numDCHuffTables = index+1;
      tbl = &dcHuffTables[index];
    }
    sym = 0;
    code = 0;
    for (i = 1; i <= 16; ++i) {
      c = str->getChar();
      tbl->firstSym[i] = sym;
      tbl->firstCode[i] = code;
      tbl->numCodes[i] = c;
      sym += c;
      code = (code + c) << 1;
    }
    length -= 16;
    for (i = 0; i < sym; ++i)
      tbl->sym[i] = str->getChar();
    length -= sym;
  }
  return gTrue;
}

GBool DCTStream::readRestartInterval() {
  int length;

  length = read16();
  if (length != 4) {
    error(getPos(), "Bad DCT restart interval");
    return gFalse;
  }
  restartInterval = read16();
  return gTrue;
}

GBool DCTStream::readAdobeMarker() {
  int length, i;
  char buf[12];
  int c;

  length = read16();
  if (length != 14)
    goto err;
  for (i = 0; i < 12; ++i) {
    if ((c = str->getChar()) == EOF)
      goto err;
    buf[i] = c;
  }
  if (strncmp(buf, "Adobe", 5))
    goto err;
  colorXform = buf[11];
  gotAdobeMarker = gTrue;
  return gTrue;

 err:
  error(getPos(), "Bad DCT Adobe APP14 marker");
  return gFalse;
}

GBool DCTStream::readTrailer() {
  int c;

  c = readMarker();
  if (c != 0xd9) {		// EOI
    error(getPos(), "Bad DCT trailer");
    return gFalse;
  }
  return gTrue;
}

int DCTStream::readMarker() {
  int c;

  do {
    do {
      c = str->getChar();
    } while (c != 0xff);
    do {
      c = str->getChar();
    } while (c == 0xff);
  } while (c == 0x00);
  return c;
}

int DCTStream::read16() {
  int c1, c2;

  if ((c1 = str->getChar()) == EOF)
    return EOF;
  if ((c2 = str->getChar()) == EOF)
    return EOF;
  return (c1 << 8) + c2;
}

GString *DCTStream::getPSFilter(char *indent) {
  GString *s;

  s = str->getPSFilter(indent);
  s->append(indent)->append("<< >> /DCTDecode filter\n");
  return s;
}

GBool DCTStream::isBinary(GBool last) {
  return str->isBinary(gTrue);
}

//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

int FlateStream::codeLenCodeMap[flateMaxCodeLenCodes] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

FlateDecode FlateStream::lengthDecode[flateMaxLitCodes-257] = {
  {0,   3},
  {0,   4},
  {0,   5},
  {0,   6},
  {0,   7},
  {0,   8},
  {0,   9},
  {0,  10},
  {1,  11},
  {1,  13},
  {1,  15},
  {1,  17},
  {2,  19},
  {2,  23},
  {2,  27},
  {2,  31},
  {3,  35},
  {3,  43},
  {3,  51},
  {3,  59},
  {4,  67},
  {4,  83},
  {4,  99},
  {4, 115},
  {5, 131},
  {5, 163},
  {5, 195},
  {5, 227},
  {0, 258}
};

FlateDecode FlateStream::distDecode[flateMaxDistCodes] = {
  { 0,     1},
  { 0,     2},
  { 0,     3},
  { 0,     4},
  { 1,     5},
  { 1,     7},
  { 2,     9},
  { 2,    13},
  { 3,    17},
  { 3,    25},
  { 4,    33},
  { 4,    49},
  { 5,    65},
  { 5,    97},
  { 6,   129},
  { 6,   193},
  { 7,   257},
  { 7,   385},
  { 8,   513},
  { 8,   769},
  { 9,  1025},
  { 9,  1537},
  {10,  2049},
  {10,  3073},
  {11,  4097},
  {11,  6145},
  {12,  8193},
  {12, 12289},
  {13, 16385},
  {13, 24577}
};

FlateStream::FlateStream(Stream *str, int predictor1, int columns1,
			 int colors1, int bits1):
    FilterStream(str) {
  if (predictor1 != 1) {
    pred = new StreamPredictor(this, predictor1, columns1, colors1, bits1);
  } else {
    pred = NULL;
  }
}

FlateStream::~FlateStream() {
  if (pred) {
    delete pred;
  }
  delete str;
}

void FlateStream::reset() {
  int cmf, flg;

  str->reset();

  // read header
  //~ need to look at window size?
  endOfBlock = eof = gTrue;
  cmf = str->getChar();
  flg = str->getChar();
  if (cmf == EOF || flg == EOF)
    return;
  if ((cmf & 0x0f) != 0x08) {
    error(getPos(), "Unknown compression method in flate stream");
    return;
  }
  if ((((cmf << 8) + flg) % 31) != 0) {
    error(getPos(), "Bad FCHECK in flate stream");
    return;
  }
  if (flg & 0x20) {
    error(getPos(), "FDICT bit set in flate stream");
    return;
  }

  // initialize
  index = 0;
  remain = 0;
  codeBuf = 0;
  codeSize = 0;
  compressedBlock = gFalse;
  endOfBlock = gTrue;
  eof = gFalse;
}

int FlateStream::getChar() {
  int c;

  if (pred) {
    return pred->getChar();
  }
  while (remain == 0) {
    if (endOfBlock && eof)
      return EOF;
    readSome();
  }
  c = buf[index];
  index = (index + 1) & flateMask;
  --remain;
  return c;
}

int FlateStream::lookChar() {
  int c;

  if (pred) {
    return pred->lookChar();
  }
  while (remain == 0) {
    if (endOfBlock && eof)
      return EOF;
    readSome();
  }
  c = buf[index];
  return c;
}

int FlateStream::getRawChar() {
  int c;

  while (remain == 0) {
    if (endOfBlock && eof)
      return EOF;
    readSome();
  }
  c = buf[index];
  index = (index + 1) & flateMask;
  --remain;
  return c;
}

GString *FlateStream::getPSFilter(char *indent) {
  return NULL;
}

GBool FlateStream::isBinary(GBool last) {
  return str->isBinary(gTrue);
}

void FlateStream::readSome() {
  int code1, code2;
  int len, dist;
  int i, j, k;
  int c;

  if (endOfBlock) {
    if (!startBlock())
      return;
  }

  if (compressedBlock) {
    if ((code1 = getHuffmanCodeWord(&litCodeTab)) == EOF)
      goto err;
    if (code1 < 256) {
      buf[index] = code1;
      remain = 1;
    } else if (code1 == 256) {
      endOfBlock = gTrue;
      remain = 0;
    } else {
      code1 -= 257;
      code2 = lengthDecode[code1].bits;
      if (code2 > 0 && (code2 = getCodeWord(code2)) == EOF)
	goto err;
      len = lengthDecode[code1].first + code2;
      if ((code1 = getHuffmanCodeWord(&distCodeTab)) == EOF)
	goto err;
      code2 = distDecode[code1].bits;
      if (code2 > 0 && (code2 = getCodeWord(code2)) == EOF)
	goto err;
      dist = distDecode[code1].first + code2;
      i = index;
      j = (index - dist) & flateMask;
      for (k = 0; k < len; ++k) {
	buf[i] = buf[j];
	i = (i + 1) & flateMask;
	j = (j + 1) & flateMask;
      }
      remain = len;
    }

  } else {
    len = (blockLen < flateWindow) ? blockLen : flateWindow;
    for (i = 0, j = index; i < len; ++i, j = (j + 1) & flateMask) {
      if ((c = str->getChar()) == EOF) {
	endOfBlock = eof = gTrue;
	break;
      }
      buf[j] = c & 0xff;
    }
    remain = i;
    blockLen -= len;
    if (blockLen == 0)
      endOfBlock = gTrue;
  }

  return;

err:
  error(getPos(), "Unexpected end of file in flate stream");
  endOfBlock = eof = gTrue;
  remain = 0;
}

GBool FlateStream::startBlock() {
  int blockHdr;
  int c;
  int check;

  // read block header
  blockHdr = getCodeWord(3);
  if (blockHdr & 1)
    eof = gTrue;
  blockHdr >>= 1;

  // uncompressed block
  if (blockHdr == 0) {
    compressedBlock = gFalse;
    if ((c = str->getChar()) == EOF)
      goto err;
    blockLen = c & 0xff;
    if ((c = str->getChar()) == EOF)
      goto err;
    blockLen |= (c & 0xff) << 8;
    if ((c = str->getChar()) == EOF)
      goto err;
    check = c & 0xff;
    if ((c = str->getChar()) == EOF)
      goto err;
    check |= (c & 0xff) << 8;
    if (check != (~blockLen & 0xffff))
      error(getPos(), "Bad uncompressed block length in flate stream");
    codeBuf = 0;
    codeSize = 0;

  // compressed block with fixed codes
  } else if (blockHdr == 1) {
    compressedBlock = gTrue;
    loadFixedCodes();

  // compressed block with dynamic codes
  } else if (blockHdr == 2) {
    compressedBlock = gTrue;
    if (!readDynamicCodes())
      goto err;

  // unknown block type
  } else {
    goto err;
  }

  endOfBlock = gFalse;
  return gTrue;

err:
  error(getPos(), "Bad block header in flate stream");
  endOfBlock = eof = gTrue;
  return gFalse;
}

void FlateStream::loadFixedCodes() {
  int i;

  // set up code arrays
  litCodeTab.codes = allCodes;
  distCodeTab.codes = allCodes + flateMaxLitCodes;

  // initialize literal code table
  for (i = 0; i <= 143; ++i)
    litCodeTab.codes[i].len = 8;
  for (i = 144; i <= 255; ++i)
    litCodeTab.codes[i].len = 9;
  for (i = 256; i <= 279; ++i)
    litCodeTab.codes[i].len = 7;
  for (i = 280; i <= 287; ++i)
    litCodeTab.codes[i].len = 8;
  compHuffmanCodes(&litCodeTab, flateMaxLitCodes);

  // initialize distance code table
  for (i = 0; i <= 5; ++i) {
    distCodeTab.start[i] = 0;
  }
  for (i = 6; i <= flateMaxHuffman+1; ++i) {
    distCodeTab.start[i] = flateMaxDistCodes;
  }
  for (i = 0; i < flateMaxDistCodes; ++i) {
    distCodeTab.codes[i].len = 5;
    distCodeTab.codes[i].code = i;
    distCodeTab.codes[i].val = i;
  }
}

GBool FlateStream::readDynamicCodes() {
  int numCodeLenCodes;
  int numLitCodes;
  int numDistCodes;
  FlateCode codeLenCodes[flateMaxCodeLenCodes];
  FlateHuffmanTab codeLenCodeTab;
  int len, repeat, code;
  int i;

  // read lengths
  if ((numLitCodes = getCodeWord(5)) == EOF)
    goto err;
  numLitCodes += 257;
  if ((numDistCodes = getCodeWord(5)) == EOF)
    goto err;
  numDistCodes += 1;
  if ((numCodeLenCodes = getCodeWord(4)) == EOF)
    goto err;
  numCodeLenCodes += 4;
  if (numLitCodes > flateMaxLitCodes ||
      numDistCodes > flateMaxDistCodes ||
      numCodeLenCodes > flateMaxCodeLenCodes)
    goto err;

  // read code length code table
  codeLenCodeTab.codes = codeLenCodes;
  for (i = 0; i < flateMaxCodeLenCodes; ++i)
    codeLenCodes[i].len = 0;
  for (i = 0; i < numCodeLenCodes; ++i) {
    if ((codeLenCodes[codeLenCodeMap[i]].len = getCodeWord(3)) == -1)
      goto err;
  }
  compHuffmanCodes(&codeLenCodeTab, flateMaxCodeLenCodes);

  // set up code arrays
  litCodeTab.codes = allCodes;
  distCodeTab.codes = allCodes + numLitCodes;

  // read literal and distance code tables
  len = 0;
  repeat = 0;
  i = 0;
  while (i < numLitCodes + numDistCodes) {
    if ((code = getHuffmanCodeWord(&codeLenCodeTab)) == EOF)
      goto err;
    if (code == 16) {
      if ((repeat = getCodeWord(2)) == EOF)
	goto err;
      for (repeat += 3; repeat > 0; --repeat)
	allCodes[i++].len = len;
    } else if (code == 17) {
      if ((repeat = getCodeWord(3)) == EOF)
	goto err;
      len = 0;
      for (repeat += 3; repeat > 0; --repeat)
	allCodes[i++].len = 0;
    } else if (code == 18) {
      if ((repeat = getCodeWord(7)) == EOF)
	goto err;
      len = 0;
      for (repeat += 11; repeat > 0; --repeat)
	allCodes[i++].len = 0;
    } else {
      allCodes[i++].len = len = code;
    }
  }
  compHuffmanCodes(&litCodeTab, numLitCodes);
  compHuffmanCodes(&distCodeTab, numDistCodes);

  return gTrue;

err:
  error(getPos(), "Bad dynamic code table in flate stream");
  return gFalse;
}

// On entry, the <tab->codes> array contains the lengths of each code,
// stored in code value order.  This function computes the code words.
// The result is sorted in order of (1) code length and (2) code word.
// The length values are no longer valid.  The <tab->start> array is
// filled with the indexes of the first code of each length.
void FlateStream::compHuffmanCodes(FlateHuffmanTab *tab, int n) {
  int numLengths[flateMaxHuffman+1];
  int nextCode[flateMaxHuffman+1];
  int nextIndex[flateMaxHuffman+2];
  int code;
  int i, j;

  // count number of codes for each code length
  for (i = 0; i <= flateMaxHuffman; ++i)
    numLengths[i] = 0;
  for (i = 0; i < n; ++i)
    ++numLengths[tab->codes[i].len];

  // compute first index for each length
  tab->start[0] = nextIndex[0] = 0;
  for (i = 1; i <= flateMaxHuffman + 1; ++i)
    tab->start[i] = nextIndex[i] = tab->start[i-1] + numLengths[i-1];

  // compute first code for each length
  code = 0;
  numLengths[0] = 0;
  for (i = 1; i <= flateMaxHuffman; ++i) {
    code = (code + numLengths[i-1]) << 1;
    nextCode[i] = code;
  }

  // compute the codes -- this permutes the codes array from value
  // order to length/code order
  for (i = 0; i < n; ++i) {
    j = nextIndex[tab->codes[i].len]++;
    if (tab->codes[i].len == 0)
      tab->codes[j].code = 0;
    else
      tab->codes[j].code = nextCode[tab->codes[i].len]++;
    tab->codes[j].val = i;
  }
}

int FlateStream::getHuffmanCodeWord(FlateHuffmanTab *tab) {
  int len;
  int code;
  int c;
  int i, j;

  code = 0;
  for (len = 1; len <= flateMaxHuffman; ++len) {

    // add a bit to the code
    if (codeSize == 0) {
      if ((c = str->getChar()) == EOF)
	return EOF;
      codeBuf = c & 0xff;
      codeSize = 8;
    }
    code = (code << 1) | (codeBuf & 1);
    codeBuf >>= 1;
    --codeSize;

    // look for code
    i = tab->start[len];
    j = tab->start[len + 1];
    if (i < j && code >= tab->codes[i].code && code <= tab->codes[j-1].code) {
      i += code - tab->codes[i].code;
      return tab->codes[i].val;
    }
  }

  // not found
  error(getPos(), "Bad code (%04x) in flate stream", code);
  return EOF;
}

int FlateStream::getCodeWord(int bits) {
  int c;

  while (codeSize < bits) {
    if ((c = str->getChar()) == EOF)
      return EOF;
    codeBuf |= (c & 0xff) << codeSize;
    codeSize += 8;
  }
  c = codeBuf & ((1 << bits) - 1);
  codeBuf >>= bits;
  codeSize -= bits;
  return c;
}

//------------------------------------------------------------------------
// EOFStream
//------------------------------------------------------------------------

EOFStream::EOFStream(Stream *str):
    FilterStream(str) {
}

EOFStream::~EOFStream() {
  delete str;
}

//------------------------------------------------------------------------
// FixedLengthEncoder
//------------------------------------------------------------------------

FixedLengthEncoder::FixedLengthEncoder(Stream *str, int length1):
    FilterStream(str) {
  length = length1;
  count = 0;
}

FixedLengthEncoder::~FixedLengthEncoder() {
  if (str->isEncoder())
    delete str;
}

void FixedLengthEncoder::reset() {
  str->reset();
  count = 0;
}

int FixedLengthEncoder::getChar() {
  if (length >= 0 && count >= length)
    return EOF;
  ++count;
  return str->getChar();
}

int FixedLengthEncoder::lookChar() {
  if (length >= 0 && count >= length)
    return EOF;
  return str->getChar();
}

//------------------------------------------------------------------------
// ASCII85Encoder
//------------------------------------------------------------------------

ASCII85Encoder::ASCII85Encoder(Stream *str):
    FilterStream(str) {
  bufPtr = bufEnd = buf;
  lineLen = 0;
  eof = gFalse;
}

ASCII85Encoder::~ASCII85Encoder() {
  if (str->isEncoder())
    delete str;
}

void ASCII85Encoder::reset() {
  str->reset();
  bufPtr = bufEnd = buf;
  lineLen = 0;
  eof = gFalse;
}

GBool ASCII85Encoder::fillBuf() {
  Gulong t;
  char buf1[5];
  int c;
  int n, i;

  if (eof)
    return gFalse;
  t = 0;
  for (n = 0; n < 4; ++n) {
    if ((c = str->getChar()) == EOF)
      break;
    t = (t << 8) + c;
  }
  bufPtr = bufEnd = buf;
  if (n > 0) {
    if (n == 4 && t == 0) {
      *bufEnd++ = 'z';
      if (++lineLen == 65) {
	*bufEnd++ = '\n';
	lineLen = 0;
      }
    } else {
      if (n < 4)
	t <<= 8 * (4 - n);
      for (i = 4; i >= 0; --i) {
	buf1[i] = (char)(t % 85 + 0x21);
	t /= 85;
      }
      for (i = 0; i <= n; ++i) {
	*bufEnd++ = buf1[i];
	if (++lineLen == 65) {
	  *bufEnd++ = '\n';
	  lineLen = 0;
	}
      }
    }
  }
  if (n < 4) {
    *bufEnd++ = '~';
    *bufEnd++ = '>';
    eof = gTrue;
  }
  return bufPtr < bufEnd;
}

//------------------------------------------------------------------------
// RunLengthEncoder
//------------------------------------------------------------------------

RunLengthEncoder::RunLengthEncoder(Stream *str):
    FilterStream(str) {
  bufPtr = bufEnd = nextEnd = buf;
  eof = gFalse;
}

RunLengthEncoder::~RunLengthEncoder() {
  if (str->isEncoder())
    delete str;
}

void RunLengthEncoder::reset() {
  str->reset();
  bufPtr = bufEnd = nextEnd = buf;
  eof = gFalse;
}

//
// When fillBuf finishes, buf[] looks like this:
//   +-----+--------------+-----------------+--
//   + tag | ... data ... | next 0, 1, or 2 |
//   +-----+--------------+-----------------+--
//    ^                    ^                 ^
//    bufPtr               bufEnd            nextEnd
//
GBool RunLengthEncoder::fillBuf() {
  int c, c1, c2;
  int n;

  // already hit EOF?
  if (eof)
    return gFalse;

  // grab two bytes
  if (nextEnd < bufEnd + 1) {
    if ((c1 = str->getChar()) == EOF) {
      eof = gTrue;
      return gFalse;
    }
  } else {
    c1 = bufEnd[0] & 0xff;
  }
  if (nextEnd < bufEnd + 2) {
    if ((c2 = str->getChar()) == EOF) {
      eof = gTrue;
      buf[0] = 0;
      buf[1] = c1;
      bufPtr = buf;
      bufEnd = &buf[2];
      return gTrue;
    }
  } else {
    c2 = bufEnd[1] & 0xff;
  }

  // check for repeat
  c = 0; // make gcc happy
  if (c1 == c2) {
    n = 2;
    c = 0; // suppress bogus compiler warning
    while (n < 128 && (c = str->getChar()) == c1)
      ++n;
    buf[0] = (char)(257 - n);
    buf[1] = c1;
    bufEnd = &buf[2];
    if (c == EOF) {
      eof = gTrue;
    } else if (n < 128) {
      buf[2] = c;
      nextEnd = &buf[3];
    } else {
      nextEnd = bufEnd;
    }

  // get up to 128 chars
  } else {
    buf[1] = c1;
    buf[2] = c2;
    n = 2;
    while (n < 128) {
      if ((c = str->getChar()) == EOF) {
	eof = gTrue;
	break;
      }
      ++n;
      buf[n] = c;
      if (buf[n] == buf[n-1])
	break;
    }
    if (buf[n] == buf[n-1]) {
      buf[0] = (char)(n-2-1);
      bufEnd = &buf[n-1];
      nextEnd = &buf[n+1];
    } else {
      buf[0] = (char)(n-1);
      bufEnd = nextEnd = &buf[n+1];
    }
  }
  bufPtr = buf;
  return gTrue;
}
