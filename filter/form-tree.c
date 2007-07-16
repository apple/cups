/*
 * "$Id: form-tree.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   CUPS form document tree routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "form.h"


/*
 * Local functions...
 */

static int	compare_attr(attr_t *a0, attr_t *a1);
static int	compare_elements(char **e0, char **e1);
static int	parse_attr(tree_t *t, FILE *fp);
static int	parse_element(tree_t *t, FILE *fp);


/*
 * Local globals...
 */

static char	*elements[] =
		{
		  "",
		  "!--",
		  "ARC",
		  "BOX",
		  "BR",
		  "B",
		  "CUPSFORM",
		  "DEFVAR",
		  "FONT",
		  "H1",
		  "H2",
		  "H3",
		  "H4",
		  "H5",
		  "H6",
		  "HEAD",
		  "IMG",
		  "I",
		  "LINE",
		  "PAGE",
		  "PIE",
		  "POLY",
		  "PRE",
		  "P",
		  "RECT",
		  "TEXT",
		  "TT",
		  "VAR"
		};


/*
 * 'formDelete()' - Delete a node and its children.
 */

void
formDelete(tree_t *t)			/* I - Tree node */
{
}


/*
 * 'formGetAttr()' - Get a node attribute value.
 */

char *					/* O - Value or NULL */
formGetAttr(tree_t     *t,		/* I - Tree node */
            const char *name)		/* I - Name of attribute */
{
}


/*
 * 'formNew()' - Create a new form node.
 */

tree_t *				/* O - New tree node */
formNew(tree_t    *p)			/* I - Parent node */
{
  tree_t	*t;			/* New tree node */


 /*
  * Allocate the new node...
  */

  if ((t = (tree_t *)calloc(sizeof(tree_t), 1)) == NULL)
    return (NULL);

 /*
  * Set/copy attributes...
  */

  if (p == NULL)
  {
    t->bg[0]    = 1.0;
    t->bg[1]    = 1.0;
    t->bg[2]    = 1.0;
    t->halign   = HALIGN_LEFT;
    t->valign   = VALIGN_MIDDLE;
    t->typeface = "Courier";
    t->size     = 12.0;
  }
  else
  {
    memcpy(t, p, sizeof(tree_t));

    t->prev       = NULL;
    t->next       = NULL;
    t->child      = NULL;
    t->last_child = NULL;
    t->parent     = NULL;
    t->num_attrs  = 0;
    t->attrs      = NULL;
    t->data       = NULL;
  }

 /*
  * Return the new node...
  */

  return (t);
}


/*
 * 'formRead()' - Read a form tree from a file.
 */

tree_t *				/* O - New form tree */
formRead(FILE   *fp,			/* I - File to read from */
         tree_t *p)			/* I - Parent node */
{
  int		ch,			/* Character from file */
		closech,		/* Closing character */
		have_whitespace;	/* Leading whitespace? */
  static char	s[10240];		/* String from file */
  uchar		*ptr,			/* Pointer in string */
		glyph[16],		/* Glyph name (&#nnn;) */
		*glyphptr;		/* Pointer in glyph string */
  tree_t	*tree,			/* "top" of this tree */
		*t,			/* New tree node */
		*prev,			/* Previous tree node */
		*temp;			/* Temporary looping var */
  uchar		*face,			/* Typeface for FONT tag */
		*color,			/* Color for FONT tag */
		*size;			/* Size for FONT tag */


 /*
  * Start off with no previous tree node...
  */

  prev = NULL;
  tree = NULL;

 /*
  * Parse data until we hit end-of-file...
  */

  while ((ch = getc(fp)) != EOF)
  {
   /*
    * Ignore leading whitespace...
    */

    have_whitespace = 0;
    closech         = '/';

    if (p == NULL || !p->preformatted)
    {
      while (isspace(ch & 255))
      {
        have_whitespace = 1;
        ch              = getc(fp);
      }

      if (ch == EOF)
        break;
    }

   /*
    * Allocate a new tree node - use calloc() to get zeroed data...
    */

    t = formNew(p);

   /*
    * See what the character was...
    */

    if (ch == '<')
    {
     /*
      * Markup char; grab the next char to see if this is a /...
      */

      ch = getc(fp);
      if (ch == ' ')
      {
       /*
        * Illegal lone "<"!  Ignore it...
	*/

	free(t);
	continue;
      }
      
      if (ch != '/')
        ungetc(ch, fp);

      if (parse_element(t, fp) < 0)
      {
        free(t);
        break;
      }

      if ((closech = getc(fp)) == '/')
        getc(fp);

     /*
      * If this is the matching close mark, or if we are starting the same
      * element, or if we've completed a list, we're done!
      */

      if (ch == '/')
      {
       /*
        * Close element; find matching element...
        */

        for (temp = p; temp != NULL; temp = temp->p)
          if (temp->element == t->element)
            break;

        free(t);

	if (temp != NULL)
          break;
	else
	  continue;
      }
    }
    else if (t->preformatted)
    {
     /*
      * Read a pre-formatted string into the current tree node...
      */

      ptr = s;
      while (ch != '<' && ch != EOF && ptr < (s + sizeof(s) - 1))
      {
        if (ch == '&')
        {
          for (glyphptr = glyph;
               (ch = getc(fp)) != EOF && (glyphptr - glyph) < 15;
               glyphptr ++)
            if (!isalnum(ch & 255))
              break;
            else
              *glyphptr = ch;

          *glyphptr = '\0';
	  if (atoi(glyph) > 0)
	    ch = atoi(glyph);
	  else if (strcmp(glyph, "lt") == 0)
	    ch = '<';
	  else if (strcmp(glyph, "gt") == 0)
	    ch = '>';
	  else if (strcmp(glyph, "quot") == 0)
	    ch = '\'';
	  else if (strcmp(glyph, "nbsp") == 0)
	    ch = ' ';
	  else
	    ch = '&';
        }

        if (ch != 0)
          *ptr++ = ch;

        if (ch == '\n')
          break;

        ch = getc(fp);
      }

      *ptr = '\0';

      if (ch == '<')
        ungetc(ch, fp);

      t->element = ELEMENT_FRAGMENT;
      t->data    = strdup(s);
    }
    else
    {
     /*
      * Read the next string fragment...
      */

      ptr = s;
      if (have_whitespace)
        *ptr++ = ' ';

      while (!isspace(ch & 255) && ch != '<' && ch != EOF && ptr < (s + sizeof(s) - 1))
      {
        if (ch == '&')
        {
          for (glyphptr = glyph;
               (ch = getc(fp)) != EOF && (glyphptr - glyph) < 15;
               glyphptr ++)
            if (!isalnum(ch & 255))
              break;
            else
              *glyphptr = ch;

          *glyphptr = '\0';
	  if (atoi(glyph) > 0)
	    ch = atoi(glyph);
	  else if (strcmp(glyph, "lt") == 0)
	    ch = '<';
	  else if (strcmp(glyph, "gt") == 0)
	    ch = '>';
	  else if (strcmp(glyph, "quot") == 0)
	    ch = '\'';
	  else if (strcmp(glyph, "nbsp") == 0)
	    ch = ' ';
	  else
	    ch = '&';
        }

        if (ch != 0)
          *ptr++ = ch;

        ch = getc(fp);
      }

      if (isspace(ch & 255))
        *ptr++ = ' ';

      *ptr = '\0';

      if (ch == '<')
        ungetc(ch, fp);

      t->element = ELEMENT_FRAGMENT;
      t->data    = strdup(s);
    }

   /*
    * If the p tree pointer is not NULL and this is the first
    * entry we've read, set the child pointer...
    */

    if (p != NULL && prev == NULL)
      p->child = t;

    if (p != NULL)
      p->last_child = t;

   /*
    * Do the prev/next links...
    */

    t->parent = p;
    t->prev   = prev;
    if (prev != NULL)
      prev->next = t;
    else
      tree = t;

    prev = t;

   /*
    * Do child stuff as needed...
    */

    if (closech == '>')
      t->child = formRead(t, fp);
  }  

  return (tree);
}


/*
 * 'formSetAttr()' - Set a node attribute.
 */

void
formSetAttr(tree_t     *t,		/* I - Tree node */
            const char *name,		/* I - Attribute name */
	    const char *value)		/* I - Attribute value */
{
}


/*
 * 'compare_attr()' - Compare two attributes.
 */

static int				/* O - -1 if a0 < a1, etc. */
compare_attr(attr_t *a0,		/* I - First attribute */
             attr_t *a1)		/* I - Second attribute */
{
  return (strcasecmp(a0->name, a1->name));
}


/*
 * 'compare_elements()' - Compare two elements.
 */

static int				/* O - -1 if e0 < e1, etc. */
compare_elements(char **e0,		/* I - First element */
                 char **e1)		/* I - Second element */
{
  return (strcasecmp(*e0, *e1));
}


/*
 * 'parse_attr()' - Parse an element attribute string.
 */

static int				/* O - -1 on error, 0 on success */
parse_attr(tree_t *t,			/* I - Current tree node */
           FILE   *fp)			/* I - Input file */
{
  char	name[1024],			/* Name of attr */
	value[10240],			/* Value of attr */
	*ptr;				/* Temporary pointer */
  int	ch;				/* Character from file */


  ptr = name;
  while ((ch = getc(fp)) != EOF)
    if (isalnum(ch & 255))
    {
      if (ptr < (name + sizeof(name) - 1))
        *ptr++ = ch;
    }
    else
      break;

  *ptr = '\0';

  while (isspace(ch & 255) || ch == '\r')
    ch = getc(fp);

  switch (ch)
  {
    default :
        ungetc(ch, fp);
        return (formSetAttr(t, name, NULL));
    case EOF :
        return (-1);
    case '=' :
        ptr = value;
        ch  = getc(fp);

        while (isspace(ch & 255) || ch == '\r')
          ch = getc(fp);

        if (ch == EOF)
          return (-1);

        if (ch == '\'')
        {
          while ((ch = getc(fp)) != EOF)
            if (ch == '\'')
              break;
            else if (ptr < (value + sizeof(value) - 1))
              *ptr++ = ch;

          *ptr = '\0';
        }
        else if (ch == '\"')
        {
          while ((ch = getc(fp)) != EOF)
            if (ch == '\"')
              break;
            else if (ptr < (value + sizeof(value) - 1))
              *ptr++ = ch;

          *ptr = '\0';
        }
        else
        {
          *ptr++ = ch;
          while ((ch = getc(fp)) != EOF)
            if (isspace(ch & 255) || ch == '>' || ch == '/' || ch == '\r')
              break;
            else if (ptr < (value + sizeof(value) - 1))
              *ptr++ = ch;

          *ptr = '\0';
          if (ch == '>' || ch == '/')
            ungetc(ch, fp);
        }

        return (formSetAttr(t, name, value));
  }
}


/*
 * 'parse_element()' - Parse an element.
 */

static int				/* O - -1 on error or ELEMENT_nnnn */
parse_element(tree_t *t,		/* I - Current tree node */
              FILE   *fp)		/* I - Input file */
{
  int	ch;				/* Character from file */
  char	element[255],			/* Element string... */
	*eptr,				/* Current character... */
	comment[10240],			/* Comment string */
	*cptr,				/* Current char... */
	**temp;				/* Element variable entry */


  eptr = element;

  while ((ch = getc(fp)) != EOF && eptr < (element + sizeof(element) - 1))
    if (ch == '>' || ch == '/' || isspace(ch & 255))
      break;
    else
      *eptr++ = ch;

  *eptr = '\0';

  if (ch == EOF)
    return (ELEMENT_ERROR);

  eptr = element;
  temp = bsearch(&mptr, elements, sizeof(elements) / sizeof(elements[0]),
                 sizeof(elements[0]),
                 (int (*)(const void *, const void *))compare_elements);

  if (temp == NULL)
  {
   /*
    * Unrecognized element stuff...
    */

    t->element = ELEMENT_COMMENT;
    strcpy(comment, element);
    cptr = comment + strlen(comment);
  }
  else
  {
    t->element = (element_t)((char **)temp - elements);
    cptr       = comment;
  }

  if (t->element == ELEMENT_COMMENT)
  {
    while (ch != EOF && ch != '>' && cptr < (comment + sizeof(comment) - 1))
    {
      *cptr++ = ch;
      ch = getc(fp);
    }

    *cptr   = '\0';
    t->data = strdup(comment);
  }
  else
  {
    while (ch != EOF && ch != '>' && ch != '/')
    {
      if (!isspace(ch & 255))
      {
        ungetc(ch, fp);
        parse_variable(t, fp);
      }

      ch = getc(fp);
    }

    if (ch != EOF)
      ungetc(ch, fp);
  }

  return (t->element);
}


/*
 * End of "$Id: form-tree.c 6649 2007-07-11 21:46:42Z mike $".
 */
