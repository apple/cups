//========================================================================
//
// Decrypt.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef DECRYPT_H
#define DECRYPT_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"
#include "GString.h"

//------------------------------------------------------------------------
// Decrypt
//------------------------------------------------------------------------

class Decrypt {
public:

  // Initialize the decryptor object.
  Decrypt(Guchar *fileKey, int objNum, int objGen);

  // Reset decryption.
  void reset();

  // Decrypt one byte.
  Guchar decryptByte(Guchar c);

  // Generate a file key.  The <fileKey> buffer must have space for
  // at least 16 bytes.  Checks user key and returns gTrue if okay.
  // <userPassword> may be NULL.
  static GBool makeFileKey(GString *ownerKey, GString *userKey,
			   int permissions, GString *fileID,
			   GString *userPassword, Guchar *fileKey);

private:

  Guchar objKey[16];
  Guchar state[256];
  Guchar x, y;
};

#endif
