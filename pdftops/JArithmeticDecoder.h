//========================================================================
//
// JArithmeticDecoder.h
//
// Arithmetic decoder used by the JBIG2 and JPEG2000 decoders.
//
// Copyright 2002-2004 Glyph & Cog, LLC
//
//========================================================================

#ifndef JARITHMETICDECODER_H
#define JARITHMETICDECODER_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gtypes.h"

class Stream;

//------------------------------------------------------------------------
// JArithmeticDecoderStats
//------------------------------------------------------------------------

class JArithmeticDecoderStats {
public:

  JArithmeticDecoderStats(int contextSizeA);
  ~JArithmeticDecoderStats();
  JArithmeticDecoderStats *copy();
  void reset();
  int getContextSize() { return contextSize; }
  void copyFrom(JArithmeticDecoderStats *stats);
  void setEntry(Guint cx, int i, int mps);

private:

  Guchar *cxTab;		// cxTab[cx] = (i[cx] << 1) + mps[cx]
  int contextSize;

  friend class JArithmeticDecoder;
};

//------------------------------------------------------------------------
// JArithmeticDecoder
//------------------------------------------------------------------------

class JArithmeticDecoder {
public:

  JArithmeticDecoder();
  ~JArithmeticDecoder();
  void setStream(Stream *strA)
    { str = strA; dataLen = -1; }
  void setStream(Stream *strA, int dataLenA)
    { str = strA; dataLen = dataLenA; }
  void start();
  int decodeBit(Guint context, JArithmeticDecoderStats *stats);
  int decodeByte(Guint context, JArithmeticDecoderStats *stats);

  // Returns false for OOB, otherwise sets *<x> and returns true.
  GBool decodeInt(int *x, JArithmeticDecoderStats *stats);

  Guint decodeIAID(Guint codeLen,
		   JArithmeticDecoderStats *stats);

private:

  Guint readByte();
  int decodeIntBit(JArithmeticDecoderStats *stats);
  void byteIn();

  static Guint qeTab[47];
  static int nmpsTab[47];
  static int nlpsTab[47];
  static int switchTab[47];

  Guint buf0, buf1;
  Guint c, a;
  int ct;

  Guint prev;			// for the integer decoder

  Stream *str;
  int dataLen;
};

#endif
