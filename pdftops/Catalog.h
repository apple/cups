//========================================================================
//
// Catalog.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef CATALOG_H
#define CATALOG_H

#ifdef __GNUC__
#pragma interface
#endif

class Object;
class Page;
class PageAttrs;
struct Ref;
class LinkDest;

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

class Catalog {
public:

  // Constructor.
  Catalog(Object *catDict);

  // Destructor.
  ~Catalog();

  // Is catalog valid?
  GBool isOk() { return ok; }

  // Get number of pages.
  int getNumPages() { return numPages; }

  // Get a page.
  Page *getPage(int i) { return pages[i-1]; }

  // Get the reference for a page object.
  Ref *getPageRef(int i) { return &pageRefs[i-1]; }

  // Return base URI, or NULL if none.
  GString *getBaseURI() { return baseURI; }

  // Find a page, given its object ID.  Returns page number, or 0 if
  // not found.
  int findPage(int num, int gen);

  // Find a named destination.  Returns the link destination, or
  // NULL if <name> is not a destination.
  LinkDest *findDest(GString *name);

private:

  Page **pages;			// array of pages
  Ref *pageRefs;		// object ID for each page
  int numPages;			// number of pages
  int pagesSize;		// size of pages array
  Object dests;			// named destination dictionary
  Object nameTree;		// name tree
  GString *baseURI;		// base URI for URI-type links
  GBool ok;			// true if catalog is valid

  int readPageTree(Dict *pages, PageAttrs *attrs, int start);
  Object *findDestInTree(Object *tree, GString *name, Object *obj);
};

#endif
