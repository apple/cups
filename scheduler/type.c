/*
 * "$Id: type.c,v 1.6 2000/11/02 22:19:25 mike Exp $"
 *
 *   MIME typing routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   mimeAddType()  - Add a MIME type to a database.
 *   mimeAddRule()  - Add a detection rule for a file type.
 *   mimeFileType() - Determine the type of a file.
 *   mimeType()     - Lookup a file type.
 *   compare()      - Compare two MIME super/type names.
 *   checkrules()   - Check each rule in a list.
 *   patmatch()     - Pattern matching...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>

#include <cups/string.h>
#include "mime.h"


/*
 * Local functions...
 */

static int	compare(mime_type_t **, mime_type_t **);
static int	checkrules(const char *, FILE *, mime_magic_t *);
static int	patmatch(const char *, const char *);


/*
 * 'mimeAddType()' - Add a MIME type to a database.
 */

mime_type_t *			/* O - New (or existing) MIME type */
mimeAddType(mime_t     *mime,	/* I - MIME database */
            const char *super,	/* I - Super-type name */
	    const char *type)	/* I - Type name */
{
  mime_type_t	*temp,		/* New MIME type */
		**types;	/* New MIME types array */


 /*
  * Range check input...
  */

  if (mime == NULL || super == NULL || type == NULL)
    return (NULL);

  if (strlen(super) > (MIME_MAX_SUPER - 1) ||
      strlen(type) > (MIME_MAX_TYPE - 1))
    return (NULL);

 /*
  * See if the type already exists; if so, return the existing type...
  */

  if ((temp = mimeType(mime, super, type)) != NULL)
    return (temp);

 /*
  * The type doesn't exist; add it...
  */

  if ((temp = calloc(1, sizeof(mime_type_t))) == NULL)
    return (NULL);

  if (mime->num_types == 0)
    types = (mime_type_t **)malloc(sizeof(mime_type_t *));
  else
    types = (mime_type_t **)realloc(mime->types, sizeof(mime_type_t *) * (mime->num_types + 1));

  if (types == NULL)
  {
    free(temp);
    return (NULL);
  }

  mime->types = types;
  types += mime->num_types;
  mime->num_types ++;

  *types = temp;
  strncpy(temp->super, super, sizeof(temp->super) - 1);
  strncpy(temp->type, type, sizeof(temp->type) - 1);

  if (mime->num_types > 1)
    qsort(mime->types, mime->num_types, sizeof(mime_type_t *),
          (int (*)(const void *, const void *))compare);

  return (temp);
}


/*
 * 'mimeAddRule()' - Add a detection rule for a file type.
 */

int					/* O - 0 on success, -1 on failure */
mimeAddTypeRule(mime_type_t *mt,	/* I - Type to add to */
                const char  *rule)	/* I - Rule to add */
{
  int		num_values,		/* Number of values seen */
		op,			/* Operation code */
		logic,			/* Logic for next rule */
		invert;			/* Invert following rule? */
  char		name[255],		/* Name in rule string */
		value[3][255],		/* Value in rule string */
		*ptr,			/* Position in name or value */
		quote;			/* Quote character */
  int		length[3];		/* Length of each parameter */
  mime_magic_t	*temp,			/* New rule */
		*current;  		/* Current rule */


 /*
  * Range check input...
  */

  if (mt == NULL || rule == NULL)
    return (-1);

 /*
  * Find the last rule in the top-level of the rules tree.
  */

  for (current = mt->rules; current != NULL; current = current->next)
    if (current->next == NULL)
      break;

 /*
  * Parse the rules string.  Most rules are either a file extension or a
  * comparison function:
  *
  *    extension
  *    function(parameters)
  */

  logic  = MIME_MAGIC_NOP;
  invert = 0;

  while (*rule != '\0')
  {
    while (isspace(*rule))
      rule ++;

    if (*rule == '(')
    {
      logic = MIME_MAGIC_NOP;
      rule ++;
    }
    else if (*rule == ')')
    {
      if (current == NULL || current->parent == NULL)
        return (-1);

      current = current->parent;

      if (current->parent == NULL)
        logic = MIME_MAGIC_OR;
      else
        logic = current->parent->op;

      rule ++;
    }
    else if (*rule == '+' && current != NULL)
    {
      if (logic != MIME_MAGIC_AND &&
          current != NULL && current->prev != NULL && current->prev->prev != NULL)
      {
       /*
        * OK, we have more than 1 rule in the current tree level...  Make a
	* new group tree and move the previous rule to it...
	*/

	if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	  return (-1);

        temp->op            = MIME_MAGIC_AND;
        temp->child         = current;
        temp->parent        = current->parent;
	current->prev->next = temp;
	temp->prev          = current->prev;

        current->prev   = NULL;
	current->parent = temp;
      }
      else
        current->parent->op = MIME_MAGIC_AND;

      logic = MIME_MAGIC_AND;
      rule ++;
    }
    else if (*rule == ',')
    {
      if (logic != MIME_MAGIC_OR && current != NULL)
      {
       /*
        * OK, we have two possibilities; either this is the top-level rule or
	* we have a bunch of AND rules at this level.
	*/

	if (current->parent == NULL)
	{
	 /*
	  * This is the top-level rule; we have to move *all* of the AND rules
	  * down a level, as AND has precedence over OR.
	  */

	  if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	    return (-1);

          while (current->prev != NULL)
	  {
	    current->parent = temp;
	    current         = current->prev;
	  }

          current->parent = temp;
          temp->op        = MIME_MAGIC_AND;
          temp->child     = current;

          mt->rules = current = temp;
	}
	else
	{
	 /*
	  * This isn't the top rule, so go up one level...
	  */

	  current = current->parent;
	}
      }

      logic = MIME_MAGIC_OR;
      rule ++;
    }
    else if (*rule == '!')
    {
      invert = 1;
      rule ++;
    }
    else if (isalnum(*rule))
    {
     /*
      * Read an extension name or a function...
      */

      for (ptr = name; isalnum(*rule) && (ptr - name) < (sizeof(name) - 1);)
        *ptr++ = *rule++;

      *ptr       = '\0';
      num_values = 0;

      if (*rule == '(')
      {
       /*
        * Read function parameters...
	*/

	rule ++;
	for (num_values = 0; num_values < 2; num_values ++)
	{
	  ptr = value[num_values];

	  while ((ptr - value[num_values]) < (sizeof(value[0]) - 1) &&
	         *rule != '\0' && *rule != ',' && *rule != ')')
	  {
	    if (isspace(*rule))
	    {
	     /*
	      * Ignore whitespace...
	      */

	      rule ++;
	      continue;
	    }
	    else if (*rule == '\"' || *rule == '\'')
	    {
	     /*
	      * Copy quoted strings literally...
	      */

	      quote = *rule++;

	      while (*rule != '\0' && *rule != quote)
	        *ptr++ = *rule++;

              if (*rule == quote)
	        rule ++;
	      else
		return (-1);
	    }
	    else if (*rule == '<')
	    {
	      rule ++;

	      while (*rule != '>' && *rule != '\0')
	      {
	        if (isxdigit(rule[0]) && isxdigit(rule[1]))
		{
		  if (isdigit(*rule))
		    *ptr = (*rule++ - '0') << 4;
		  else
		    *ptr = (tolower(*rule++) - 'a' + 10) << 4;

		  if (isdigit(*rule))
		    *ptr++ |= *rule++ - '0';
		  else
		    *ptr++ |= tolower(*rule++) - 'a' + 10;
		}
		else
	          return (-1);
	      }

              if (*rule == '>')
	        rule ++;
	      else
		return (-1);
	    }
	    else
	      *ptr++ = *rule++;
	  }

          *ptr = '\0';
	  length[num_values] = ptr - value[num_values];

          if (*rule != ',')
	    break;

          rule ++;
	}

        if (*rule != ')')
	  return (-1);

	rule ++;

       /*
        * Figure out the function...
	*/

        if (strcmp(name, "match") == 0)
	  op = MIME_MAGIC_MATCH;
	else if (strcmp(name, "ascii") == 0)
	  op = MIME_MAGIC_ASCII;
	else if (strcmp(name, "printable") == 0)
	  op = MIME_MAGIC_PRINTABLE;
	else if (strcmp(name, "string") == 0)
	  op = MIME_MAGIC_STRING;
	else if (strcmp(name, "char") == 0)
	  op = MIME_MAGIC_CHAR;
	else if (strcmp(name, "short") == 0)
	  op = MIME_MAGIC_SHORT;
	else if (strcmp(name, "int") == 0)
	  op = MIME_MAGIC_INT;
	else if (strcmp(name, "locale") == 0)
	  op = MIME_MAGIC_LOCALE;
	else if (strcmp(name, "contains") == 0)
	  op = MIME_MAGIC_CONTAINS;
	else
	  return (-1);
      }
      else
      {
       /*
        * This is just a filename match on the extension...
	*/

	snprintf(value[0], sizeof(value[0]) - 1, "*.%s", name);
	length[0]  = strlen(value[0]);
	num_values = 1;
	op         = MIME_MAGIC_MATCH;
      }

     /*
      * Add a rule for this operation.
      */

      if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	return (-1);

      temp->invert = invert;
      if (current != NULL)
      {
	temp->parent  = current->parent;
	current->next = temp;
      }
      else
        mt->rules = temp;

      temp->prev = current;

      if (logic == MIME_MAGIC_NOP)
      {
       /*
        * Add parenthetical grouping...
	*/

        temp->op = MIME_MAGIC_OR;

	if ((temp->child = calloc(1, sizeof(mime_magic_t))) == NULL)
	  return (-1);

	temp->child->parent = temp;

	temp  = temp->child;
        logic = MIME_MAGIC_OR;
      }

     /*
      * Fill in data for the rule...
      */

      current  = temp;
      temp->op = op;
      invert   = 0;

      switch (op)
      {
        case MIME_MAGIC_MATCH :
	    if (length[0] > (sizeof(temp->value.matchv) - 1))
	      return (-1);
	    strcpy(temp->value.matchv, value[0]);
	    break;
	case MIME_MAGIC_ASCII :
	case MIME_MAGIC_PRINTABLE :
	    temp->offset = strtol(value[0], NULL, 0);
	    temp->length = strtol(value[1], NULL, 0);
	    if (temp->length > MIME_MAX_BUFFER)
	      temp->length = MIME_MAX_BUFFER;
	    break;
	case MIME_MAGIC_STRING :
	    temp->offset = strtol(value[0], NULL, 0);
	    if (length[1] > sizeof(temp->value.stringv))
	      return (-1);
	    temp->length = length[1];
	    memcpy(temp->value.stringv, value[1], length[1]);
	    break;
	case MIME_MAGIC_CHAR :
	    temp->offset = strtol(value[0], NULL, 0);
	    if (length[1] == 1)
	      temp->value.charv = value[1][0];
	    else
	      temp->value.charv = strtol(value[1], NULL, 0);
	    break;
	case MIME_MAGIC_SHORT :
	    temp->offset       = strtol(value[0], NULL, 0);
	    temp->value.shortv = strtol(value[1], NULL, 0);
	    break;
	case MIME_MAGIC_INT :
	    temp->offset     = strtol(value[0], NULL, 0);
	    temp->value.intv = strtol(value[1], NULL, 0);
	    break;
	case MIME_MAGIC_LOCALE :
	    if (length[0] > (sizeof(temp->value.localev) - 1))
	      return (-1);

	    strcpy(temp->value.localev, value[0]);
	    break;
	case MIME_MAGIC_CONTAINS :
	    temp->offset = strtol(value[0], NULL, 0);
	    temp->region = strtol(value[1], NULL, 0);
	    if (length[2] > sizeof(temp->value.stringv))
	      return (-1);
	    temp->length = length[2];
	    memcpy(temp->value.stringv, value[2], length[2]);
	    break;
      }
    }
    else
      break;
  }

  return (0);
}


/*
 * 'mimeFileType()' - Determine the type of a file.
 */

mime_type_t *				/* O - Type of file */
mimeFileType(mime_t     *mime,		/* I - MIME database */
             const char *pathname)	/* I - Name of file to check */
{
  int		i;		/* Looping var */
  FILE		*fp;		/* File pointer */
  mime_type_t	**types;	/* File types */
  const char	*filename;	/* Base filename of file */


 /*
  * Range check input parameters...
  */

  if (mime == NULL || pathname == NULL)
    return (NULL);

 /*
  * Try to open the file...
  */

  if ((fp = fopen(pathname, "r")) == NULL)
    return (NULL);

 /*
  * Figure out the filename (without directory portion)...
  */

  if ((filename = strrchr(pathname, '/')) != NULL)
    filename ++;
  else
    filename = pathname;

 /*
  * Then check it against all known types...
  */

  for (i = mime->num_types, types = mime->types; i > 0; i --, types ++)
    if (checkrules(filename, fp, (*types)->rules))
      break;

 /*
  * Finally, close the file and return a match (if any)...
  */

  fclose(fp);

  if (i > 0)
    return (*types);
  else
    return (NULL);
}


/*
 * 'mimeType()' - Lookup a file type.
 */

mime_type_t *			/* O - Matching file type definition */
mimeType(mime_t     *mime,	/* I - MIME database */
         const char *super,	/* I - Super-type name */
	 const char *type)	/* I - Type name */
{
  mime_type_t	key,		/* MIME type search key*/
		*keyptr,	/* Key pointer... */
		**match;	/* Matching pointer */

 /*
  * Range check input...
  */

  if (mime == NULL || super == NULL || type == NULL)
    return (NULL);

  if (strlen(super) > (MIME_MAX_SUPER - 1) ||
      strlen(type) > (MIME_MAX_TYPE - 1))
    return (NULL);

  if (mime->num_types == 0)
    return (NULL);

 /*
  * Lookup the type in the array...
  */

  strncpy(key.super, super, sizeof(key.super) - 1);
  key.super[sizeof(key.super) - 1] = '\0';
  strncpy(key.type, type, sizeof(key.type) - 1);
  key.type[sizeof(key.type) - 1] = '\0';

  keyptr = &key;

  match = (mime_type_t **)bsearch(&keyptr, mime->types, mime->num_types,
                                  sizeof(mime_type_t *),
                                  (int (*)(const void *, const void *))compare);

  if (match == NULL)
    return (NULL);
  else
    return (*match);
}


/*
 * 'compare()' - Compare two MIME super/type names.
 */

static int			/* O - Result of comparison */
compare(mime_type_t **t0,	/* I - First type */
        mime_type_t **t1)	/* I - Second type */
{
  int	i;			/* Result of comparison */


  if ((i = strcasecmp((*t0)->super, (*t1)->super)) == 0)
    i = strcasecmp((*t0)->type, (*t1)->type);

  return (i);
}


/*
 * 'checkrules()' - Check each rule in a list.
 */

static int				/* O - 1 if match, 0 if no match */
checkrules(const char   *filename,	/* I - Filename */
           FILE         *fp,		/* I - File to check */
           mime_magic_t *rules)		/* I - Rules to check */
{
  int		n;			/* Looping var */
  int		region;			/* Region to look at */
  int		logic,			/* Logic to apply */
		result,			/* Result of test */
		intv;			/* Integer value */
  short		shortv;			/* Short value */
  unsigned char	buffer[MIME_MAX_BUFFER],/* Input buffer */
		*bufptr;		/* Current buffer position */
  int		bufoffset,		/* Offset in file for buffer */
		buflength;		/* Length of data in buffer */


  if (rules == NULL)
    return (0);

  if (rules->parent == NULL)
    logic = MIME_MAGIC_OR;
  else
    logic = rules->parent->op;

  bufoffset = -1;
  buflength = 0;
  result    = 0;

  while (rules != NULL)
  {
   /*
    * Compute the result of this rule...
    */

    switch (rules->op)
    {
      case MIME_MAGIC_MATCH :
          result = patmatch(filename, rules->value.matchv);
	  break;

      case MIME_MAGIC_ASCII :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + rules->length) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

         /*
	  * Test for ASCII printable characters plus standard control chars.
	  */

	  if ((rules->offset + rules->length) > (bufoffset + buflength))
	    n = bufoffset + buflength - rules->offset;
	  else
	    n = rules->length;

          bufptr = buffer + rules->offset - bufoffset;
	  while (n > 0)
	    if ((*bufptr >= 32 && *bufptr <= 126) ||
	        (*bufptr >= 8 && *bufptr <= 13) ||
		*bufptr == 26 || *bufptr == 27)
	    {
	      n --;
	      bufptr ++;
	    }
	    else
	      break;

	  result = (n == 0);
	  break;

      case MIME_MAGIC_PRINTABLE :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + rules->length) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

         /*
	  * Test for 8-bit printable characters plus standard control chars.
	  */

	  if ((rules->offset + rules->length) > (bufoffset + buflength))
	    n = bufoffset + buflength - rules->offset;
	  else
	    n = rules->length;

          bufptr = buffer + rules->offset - bufoffset;

	  while (n > 0)
	    if (*bufptr >= 128 ||
	        (*bufptr >= 32 && *bufptr <= 126) ||
	        (*bufptr >= 8 && *bufptr <= 13) ||
		*bufptr == 26 || *bufptr == 27)
	    {
	      n --;
	      bufptr ++;
	    }
	    else
	      break;

	  result = (n == 0);
	  break;

      case MIME_MAGIC_STRING :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + rules->length) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

	  if ((rules->offset + rules->length) > (bufoffset + buflength))
	    result = 0;
	  else
            result = (memcmp(buffer + rules->offset - bufoffset,
	                     rules->value.stringv, rules->length) == 0);
	  break;

      case MIME_MAGIC_CHAR :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset)
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

	 /*
	  * Compare the character values; if the file is too short, it
	  * can't match...
	  */

	  if (buflength < 1)
	    result = 0;
	  else
	    result = (buffer[rules->offset - bufoffset] == rules->value.charv);
	  break;

      case MIME_MAGIC_SHORT :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + 2) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

	 /*
	  * Compare the short values; if the file is too short, it
	  * can't match...
	  */

	  if (buflength < 2)
	    result = 0;
	  else
	  {
	    bufptr = buffer + rules->offset - bufoffset;
	    shortv = (bufptr[0] << 8) | bufptr[1];
	    result = (shortv == rules->value.shortv);
	  }
	  break;

      case MIME_MAGIC_INT :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + 4) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

	 /*
	  * Compare the int values; if the file is too short, it
	  * can't match...
	  */

	  if (buflength < 4)
	    result = 0;
	  else
	  {
	    bufptr = buffer + rules->offset - bufoffset;
	    intv   = (((((bufptr[0] << 8) | bufptr[1]) << 8) | bufptr[2]) << 8) |
	             bufptr[3];;
	    result = (intv == rules->value.intv);
	  }
	  break;

      case MIME_MAGIC_LOCALE :
          result = (strcmp(rules->value.localev, setlocale(LC_ALL, NULL)) == 0);
	  break;

      case MIME_MAGIC_CONTAINS :
         /*
	  * Load the buffer if necessary...
	  */

          if (bufoffset < 0 || rules->offset < bufoffset ||
	      (rules->offset + rules->region) > (bufoffset + buflength))
	  {
	   /*
	    * Reload file buffer...
	    */

            fseek(fp, rules->offset, SEEK_SET);
	    buflength = fread(buffer, 1, sizeof(buffer), fp);
	    bufoffset = rules->offset;
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

	  if ((rules->offset + rules->length) > (bufoffset + buflength))
	    result = 0;
	  else
	  {
	    if (buflength > rules->region)
	      region = rules->region - rules->length;
	    else
	      region = buflength - rules->length;

	    for (n = 0; n < region; n ++)
	      if ((result = (memcmp(buffer + rules->offset - bufoffset + n,
	                            rules->value.stringv, rules->length) == 0)) != 0)
		break;
          }
	  break;

      default :
          if (rules->child != NULL)
	    result = checkrules(filename, fp, rules->child);
	  else
	    result = 0;
	  break;
    }

   /*
    * If the logic is inverted, invert the result...
    */

    if (rules->invert)
      result = !result;

   /*
    * OK, now if the current logic is OR and this result is true, the this
    * rule set is true.  If the current logic is AND and this result is false,
    * the the rule set is false...
    */

    if ((result && logic == MIME_MAGIC_OR) ||
        (!result && logic == MIME_MAGIC_AND))
      return (result);

   /*
    * Otherwise the jury is still out on this one, so move to the next rule.
    */

    rules = rules->next;
  }

  return (result);
}


/*
 * 'patmatch()' - Pattern matching...
 */

static int			/* O - 1 if match, 0 if no match */
patmatch(const char *s,		/* I - String to match against */
         const char *pat)	/* I - Pattern to match against */
{
 /*
  * Range check the input...
  */

  if (s == NULL || pat == NULL)
    return (0);

 /*
  * Loop through the pattern and match strings, and stop if we come to a
  * point where the strings don't match or we find a complete match.
  */

  while (*s != '\0' && *pat != '\0')
  {
    if (*pat == '*')
    {
     /*
      * Wildcard - 0 or more characters...
      */

      pat ++;
      if (*pat == '\0')
        return (1);	/* Last pattern char is *, so everything matches now... */

     /*
      * Test all remaining combinations until we get to the end of the string.
      */

      while (*s != '\0')
      {
        if (patmatch(s, pat))
	  return (1);

	s ++;
      }
    }
    else if (*pat == '?')
    {
     /*
      * Wildcard - 1 character...
      */

      pat ++;
      s ++;
      continue;
    }
    else if (*pat == '[')
    {
     /*
      * Match a character from the input set [chars]...
      */

      pat ++;
      while (*pat != ']' && *pat != '\0')
        if (*s == *pat)
	  break;
	else
	  pat ++;

      if (*pat == ']' || *pat == '\0')
        return (0);

      while (*pat != ']' && *pat != '\0')
        pat ++;

      if (*pat == ']')
        pat ++;

      continue;
    }
    else if (*pat == '\\')
    {
     /*
      * Handle quoted characters...
      */

      pat ++;
    }

   /*
    * Stop if the pattern and string don't match...
    */

    if (*pat++ != *s++)
      return (0);
  }

 /*
  * Done parsing the pattern and string; return 1 if the last character matches
  * and 0 otherwise...
  */

  return (*s == *pat);
}


/*
 * End of "$Id: type.c,v 1.6 2000/11/02 22:19:25 mike Exp $".
 */
