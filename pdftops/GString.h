//========================================================================
//
// GString.h
//
// Simple variable-length string type.
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef GSTRING_H
#define GSTRING_H

#ifdef __GNUC__
#pragma interface
#endif

#include <string.h>

class GString {
public:

  // Create an empty string.
  GString();

  // Create a string from a C string.
  GString(const char *s1);

  // Create a string from <length1> chars at <s1>.  This string
  // can contain null characters.
  GString (const char *s1, int length1);

  // Copy a string.
  GString(GString *str);
  GString *copy() { return new GString(this); }

  // Concatenate two strings.
  GString(GString *str1, GString *str2);

  // Convert an integer to a string.
  static GString *fromInt(int x);

  // Destructor.
  ~GString();

  // Get length.
  int getLength() { return length; }

  // Get C string.
  char *getCString() { return s; }

  // Get <i>th character.
  char getChar(int i) { return s[i]; }

  // Change <i>th character.
  void setChar(int i, char c) { s[i] = c; }

  // Clear string to zero length.
  GString *clear();

  // Append a character or string.
  GString *append(char c);
  GString *append(GString *str);
  GString *append(const char *str);
  GString *append(const char *str, int length1);

  // Insert a character or string.
  GString *insert(int i, char c);
  GString *insert(int i, GString *str);
  GString *insert(int i, const char *str);
  GString *insert(int i, const char *str, int length1);

  // Delete a character or range of characters.
  GString *del(int i, int n = 1);

  // Convert string to all-upper/all-lower case.
  GString *upperCase();
  GString *lowerCase();

  // Compare two strings:  -1:<  0:=  +1:>
  // These functions assume the strings do not contain null characters.
  int cmp(GString *str) { return strcmp(s, str->getCString()); }
  int cmpN(GString *str, int n) { return strncmp(s, str->getCString(), n); }
  int cmp(const char *s1) { return strcmp(s, s1); }
  int cmpN(const char *s1, int n) { return strncmp(s, s1, n); }

private:

  int length;
  char *s;

  void resize(int length1);
};

#endif
