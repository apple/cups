//========================================================================
//
// ErrorCodes.h
//
// Copyright 2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef ERRORCODES_H
#define ERRORCODES_H

#define errNone             0	// no error

#define errOpenFile         1	// couldn't open the PDF file

#define errBadCatalog       2	// couldn't read the page catalog

#define errDamaged          3	// PDF file was damaged and couldn't be
				// repaired

#define errEncrypted        4	// file was encrypted and password was
				// incorrect or not supplied

#endif
