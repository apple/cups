//========================================================================
//
// Array.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef ARRAY_H
#define ARRAY_H

#ifdef __GNUC__
#pragma interface
#endif

#include "Object.h"

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

class Array {
public:

  // Constructor.
  Array();

  // Destructor.
  ~Array();

  // Reference counting.
  int incRef() { return ++ref; }
  int decRef() { return --ref; }

  // Get number of elements.
  int getLength() { return length; }

  // Add an element.
  void add(Object *elem);

  // Accessors.
  Object *get(int i, Object *obj);
  Object *getNF(int i, Object *obj);

private:

  Object *elems;		// array of elements
  int size;			// size of <elems> array
  int length;			// number of elements in array
  int ref;			// reference count
};

#endif
