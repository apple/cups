//
// "$Id: ppdc-array.cxx 11558 2014-02-06 18:33:34Z msweet $"
//
// Array class for the CUPS PPD Compiler.
//
// Copyright 2007-2014 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// These coded instructions, statements, and computer programs are the
// property of Apple Inc. and are protected by Federal copyright
// law.  Distribution and use rights are outlined in the file "LICENSE.txt"
// which should have been included with this file.  If this file is
// file is missing or damaged, see the license at "http://www.cups.org/".
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcArray::ppdcArray()' - Create a new array.
//

ppdcArray::ppdcArray(ppdcArray *a)
  : ppdcShared()
{
  PPDC_NEW;

  if (a)
  {
    count = a->count;
    alloc = count;

    if (count)
    {
      // Make a copy of the array...
      data = new ppdcShared *[count];

      memcpy(data, a->data, (size_t)count * sizeof(ppdcShared *));

      for (int i = 0; i < count; i ++)
        data[i]->retain();
    }
    else
      data = 0;
  }
  else
  {
    count = 0;
    alloc = 0;
    data  = 0;
  }

  current = 0;
}


//
// 'ppdcArray::~ppdcArray()' - Destroy an array.
//

ppdcArray::~ppdcArray()
{
  PPDC_DELETE;

  for (int i = 0; i < count; i ++)
    data[i]->release();

  if (alloc)
    delete[] data;
}


//
// 'ppdcArray::add()' - Add an element to an array.
//

void
ppdcArray::add(ppdcShared *d)
{
  ppdcShared	**temp;


  if (count >= alloc)
  {
    alloc += 10;
    temp  = new ppdcShared *[alloc];

    memcpy(temp, data, (size_t)count * sizeof(ppdcShared *));

    delete[] data;
    data = temp;
  }

  data[count++] = d;
}


//
// 'ppdcArray::first()' - Return the first element in the array.
//

ppdcShared *
ppdcArray::first()
{
  current = 0;

  if (current >= count)
    return (0);
  else
    return (data[current ++]);
}


//
// 'ppdcArray::next()' - Return the next element in the array.
//

ppdcShared *
ppdcArray::next()
{
  if (current >= count)
    return (0);
  else
    return (data[current ++]);
}


//
// 'ppdcArray::remove()' - Remove an element from the array.
//

void
ppdcArray::remove(ppdcShared *d)		// I - Data element
{
  int	i;					// Looping var


  for (i = 0; i < count; i ++)
    if (d == data[i])
      break;

  if (i >= count)
    return;

  count --;
  d->release();

  if (i < count)
    memmove(data + i, data + i + 1, (size_t)(count - i) * sizeof(ppdcShared *));
}


//
// End of "$Id: ppdc-array.cxx 11558 2014-02-06 18:33:34Z msweet $".
//
