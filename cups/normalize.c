/*
 * "$Id$"
 *
 *   Unicode normalization for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are
 *   the property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the
 *   file "LICENSE.txt" which should have been included with this file.
 *   If this file is missing or damaged please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsNormalizeMapsGet()       - Get all norm maps to cache.
 *   cupsNormalizeMapsFree()      - Free all norm maps in cache.
 *   cupsNormalizeMapsFlush()     - Flush all norm maps in cache.
 *   cupsUTF8Normalize()          - Normalize UTF-8 string.
 *   cupsUTF32Normalize()         - Normalize UTF-32 string.
 *   cupsUTF8CaseFold()           - Case fold UTF-8 string.
 *   cupsUTF32CaseFold()          - Case fold UTF-32 string.
 *   cupsUTF8CompareCaseless()    - Compare case folded UTF-8 strings.
 *   cupsUTF32CompareCaseless()   - Compare case folded UTF-32 strings.
 *   cupsUTF8CompareIdentifier()  - Compare folded NFKC UTF-8 strings.
 *   cupsUTF32CompareIdentifier() - Compare folded NFKC UTF-32 strings.
 *   cupsUTF32CharacterProperty() - Get UTF-32 character property.
 *   get_general_category()       - Get UTF-32 Char General Category.
 *   get_bidi_category()          - Get UTF-32 Char Bidi Category.
 *   get_combining_class()        - Get UTF-32 Char Combining Class.
 *   get_break_class()            - Get UTF-32 Char Line Break Class.
 *   get_map_count()              - Count lines in a map file.
 *   get_normmap()                - Get Unicode norm map to cache.
 *   get_foldmap()                - Get Unicode casefold map to cache.
 *   get_propmap()                - Get Unicode property map to cache.
 *   get_combmap()                - Get Unicode combining map to cache.
 *   get_breakmap()               - Get Unicode break map to cache.
 *   compare_compose()            - Compare key for compose match.
 *   compare_decompose()          - Compare key for decompose match.
 *   compare_foldchar()           - Compare key for case fold match.
 *   compare_combchar()           - Compare key for combining match.
 *   compare_breakchar()          - Compare key for line break match.
 *   compare_propchar()           - Compare key for property char match.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#include <time.h>


typedef struct				/**** General Category Index Struct****/
{
  cups_gencat_t	gencat;			/* General Category Value */
  const char	*str;			/* General Category String */
} gencat_t;

static const gencat_t gencat_index[] =	/* General Category Index */
{
  { CUPS_GENCAT_LU, "Lu" },		/* Letter, Uppercase */
  { CUPS_GENCAT_LL, "Ll" },		/* Letter, Lowercase */
  { CUPS_GENCAT_LT, "Lt" },		/* Letter, Titlecase */
  { CUPS_GENCAT_LM, "Lm" },		/* Letter, Modifier */
  { CUPS_GENCAT_LO, "Lo" },		/* Letter, Other */
  { CUPS_GENCAT_MN, "Mn" },		/* Mark, Non-Spacing */
  { CUPS_GENCAT_MC, "Mc" },		/* Mark, Spacing Combining */
  { CUPS_GENCAT_ME, "Me" },		/* Mark, Enclosing */
  { CUPS_GENCAT_ND, "Nd" },		/* Number, Decimal Digit */
  { CUPS_GENCAT_NL, "Nl" },		/* Number, Letter */
  { CUPS_GENCAT_NO, "No" },		/* Number, Other */
  { CUPS_GENCAT_PC, "Pc" },		/* Punctuation, Connector */
  { CUPS_GENCAT_PD, "Pd" },		/* Punctuation, Dash */
  { CUPS_GENCAT_PS, "Ps" },		/* Punctuation, Open (start) */
  { CUPS_GENCAT_PE, "Pe" },		/* Punctuation, Close (end) */
  { CUPS_GENCAT_PI, "Pi" },		/* Punctuation, Initial Quote */
  { CUPS_GENCAT_PF, "Pf" },		/* Punctuation, Final Quote */
  { CUPS_GENCAT_PO, "Po" },		/* Punctuation, Other */
  { CUPS_GENCAT_SM, "Sm" },		/* Symbol, Math */
  { CUPS_GENCAT_SC, "Sc" },		/* Symbol, Currency */
  { CUPS_GENCAT_SK, "Sk" },		/* Symbol, Modifier */
  { CUPS_GENCAT_SO, "So" },		/* Symbol, Other */
  { CUPS_GENCAT_ZS, "Zs" },		/* Separator, Space */
  { CUPS_GENCAT_ZL, "Zl" },		/* Separator, Line */
  { CUPS_GENCAT_ZP, "Zp" },		/* Separator, Paragraph */
  { CUPS_GENCAT_CC, "Cc" },		/* Other, Control */
  { CUPS_GENCAT_CF, "Cf" },		/* Other, Format */
  { CUPS_GENCAT_CS, "Cs" },		/* Other, Surrogate */
  { CUPS_GENCAT_CO, "Co" },		/* Other, Private Use */
  { CUPS_GENCAT_CN, "Cn" },		/* Other, Not Assigned */
  { 0, NULL }
};

static const char * const bidicat_index[] =
					/* Bidi Category Index */
{
  "L",					/* Left-to-Right (Alpha, Syllabic, Ideographic) */
  "LRE",				/* Left-to-Right Embedding (explicit) */
  "LRO",				/* Left-to-Right Override (explicit) */
  "R",					/* Right-to-Left (Hebrew alphabet and most punct) */
  "AL",					/* Right-to-Left Arabic (Arabic, Thaana, Syriac) */
  "RLE",				/* Right-to-Left Embedding (explicit) */
  "RLO",				/* Right-to-Left Override (explicit) */
  "PDF",				/* Pop Directional Format */
  "EN",					/* Euro Number (Euro and East Arabic-Indic digits) */
  "ES",					/* Euro Number Separator (Slash) */
  "ET",					/* Euro Number Termintor (Plus, Minus, Degree, etc) */
  "AN",					/* Arabic Number (Arabic-Indic digits, separators) */
  "CS",					/* Common Number Separator (Colon, Comma, Dot, etc) */
  "NSM",				/* Non-Spacing Mark (category Mn / Me in UCD) */
  "BN",					/* Boundary Neutral (Formatting / Control chars) */
  "B",					/* Paragraph Separator */
  "S",					/* Segment Separator (Tab) */
  "WS",					/* Whitespace Space (Space, Line Separator, etc) */
  "ON",					/* Other Neutrals */
  NULL
};

typedef struct				/**** Line Break Class Index Struct****/
{
  cups_break_class_t	breakclass;	/* Line Break Class Value */
  const char		*str;		/* Line Break Class String */
} _cups_break_t;

static const _cups_break_t break_index[] =	/* Line Break Class Index */
{
  { CUPS_BREAK_AI, "AI" },		/* Ambiguous (Alphabetic or Ideograph) */
  { CUPS_BREAK_AL, "AL" },		/* Ordinary Alpha/Symbol Chars (XP) */
  { CUPS_BREAK_BA, "BA" },		/* Break Opportunity After Chars (A) */
  { CUPS_BREAK_BB, "BB" },		/* Break Opportunities Before Chars (B) */
  { CUPS_BREAK_B2, "B2" },		/* Break Opportunity Either (B/A/XP) */
  { CUPS_BREAK_BK, "BK" },		/* Mandatory Break (A) (norm) */
  { CUPS_BREAK_CB, "CB" },		/* Contingent Break (B/A) (norm) */
  { CUPS_BREAK_CL, "CL" },		/* Closing Punctuation (XB) */
  { CUPS_BREAK_CM, "CM" },		/* Attached/Combining (XB) (norm) */
  { CUPS_BREAK_CR, "CR" },		/* Carriage Return (A) (norm) */
  { CUPS_BREAK_EX, "EX" },		/* Exclamation / Interrogation (XB) */
  { CUPS_BREAK_GL, "GL" },		/* Non-breaking ("Glue") (XB/XA) (norm) */
  { CUPS_BREAK_HY, "HY" },		/* Hyphen (XA) */
  { CUPS_BREAK_ID, "ID" },		/* Ideographic (B/A) */
  { CUPS_BREAK_IN, "IN" },		/* Inseparable chars (XP) */
  { CUPS_BREAK_IS, "IS" },		/* Numeric Separator (Infix) (XB) */
  { CUPS_BREAK_LF, "LF" },		/* Line Feed (A) (norm) */
  { CUPS_BREAK_NS, "NS" },		/* Non-starters (XB) */
  { CUPS_BREAK_NU, "NU" },		/* Numeric (XP) */
  { CUPS_BREAK_OP, "OP" },		/* Opening Punctuation (XA) */
  { CUPS_BREAK_PO, "PO" },		/* Postfix (Numeric) (XB) */
  { CUPS_BREAK_PR, "PR" },		/* Prefix (Numeric) (XA) */
  { CUPS_BREAK_QU, "QU" },		/* Ambiguous Quotation (XB/XA) */
  { CUPS_BREAK_SA, "SA" },		/* Context Dependent (SE Asian) (P) */
  { CUPS_BREAK_SG, "SG" },		/* Surrogates (XP) (norm) */
  { CUPS_BREAK_SP, "SP" },		/* Space (A) (norm) */
  { CUPS_BREAK_SY, "SY" },		/* Symbols Allowing Break After (A) */
  { CUPS_BREAK_XX, "XX" },		/* Unknown (XP) */
  { CUPS_BREAK_ZW, "ZW" },		/* Zero Width Space (A) (norm) */
  { 0, NULL }
};

/*
 * Prototypes...
 */

static int compare_breakchar(const void *k1, const void *k2);
static int compare_combchar(const void *k1, const void *k2);
static int compare_compose(const void *k1, const void *k2);
static int compare_decompose(const void *k1, const void *k2);
static int compare_foldchar(const void *k1, const void *k2);
static int compare_propchar(const void *k1, const void *k2);
static int get_bidi_category(const cups_utf32_t ch);
static int get_break_class(const cups_utf32_t ch);
static int get_breakmap(void);
static int get_combining_class(const cups_utf32_t ch);
static int get_combmap(void);
static int get_foldmap(const cups_folding_t fold);
static int get_general_category(const cups_utf32_t ch);
static int get_map_count(const char *filename);
static int get_normmap(const cups_normalize_t normalize);
static int get_propmap(void);


/*
 * 'cupsNormalizeMapsGet()' - Get all normalization maps to cache.
 */

int					/* O - Zero or -1 on error */
cupsNormalizeMapsGet(void)
{
  _cups_norm_map_t	*nmap;		/* Unicode Normalization Map */
  _cups_fold_map_t	*fmap;		/* Unicode Case Folding Map */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have normalization maps loaded...
  */

  if (cg->normmap_cache)
  {
    for (nmap = cg->normmap_cache; nmap != NULL; nmap = nmap->next)
      nmap->used ++;

    for (fmap = cg->foldmap_cache; fmap != NULL; fmap = fmap->next)
      fmap->used ++;

    if (cg->combmap_cache)
      cg->combmap_cache->used ++;

    if (cg->propmap_cache)
      cg->propmap_cache->used ++;

    if (cg->breakmap_cache)
      cg->breakmap_cache->used ++;

    return (0);
  }

 /*
  * Get normalization maps...
  */

  if (get_normmap(CUPS_NORM_NFD) < 0)
    return (-1);

  if (get_normmap(CUPS_NORM_NFKD) < 0)
    return (-1);

  if (get_normmap(CUPS_NORM_NFC) < 0)
    return (-1);

 /*
  * Get case folding, combining class, character property maps...
  */

  if (get_foldmap(CUPS_FOLD_SIMPLE) < 0)
    return (-1);

  if (get_foldmap(CUPS_FOLD_FULL) < 0)
    return (-1);

  if (get_propmap() < 0)
    return (-1);

  if (get_combmap() < 0)
    return (-1);

  if (get_breakmap() < 0)
    return (-1);

  return (0);
}


/*
 * 'cupsNormalizeMapsFree()' - Free all normalization maps in cache.
 *
 * This does not actually free; use 'cupsNormalizeMapsFlush()' for that.
 */

int					/* O - Zero or -1 on error */
cupsNormalizeMapsFree(void)
{
  _cups_norm_map_t	*nmap;		/* Unicode Normalization Map */
  _cups_fold_map_t	*fmap;		/* Unicode Case Folding Map */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have normalization maps loaded...
  */

  if (cg->normmap_cache == NULL)
    return (-1);

  for (nmap = cg->normmap_cache; nmap != NULL; nmap = nmap->next)
    if (nmap->used > 0)
      nmap->used --;

  for (fmap = cg->foldmap_cache; fmap != NULL; fmap = fmap->next)
    if (fmap->used > 0)
      fmap->used --;

  if (cg->propmap_cache && (cg->propmap_cache->used > 0))
    cg->propmap_cache->used --;

  if (cg->combmap_cache && (cg->combmap_cache->used > 0))
    cg->combmap_cache->used --;

  if (cg->breakmap_cache && (cg->breakmap_cache->used > 0))
    cg->breakmap_cache->used --;

  return (0);
}


/*
 * 'cupsNormalizeMapsFlush()' - Flush all normalization maps in cache.
 */

void
cupsNormalizeMapsFlush(void)
{
  _cups_norm_map_t	*nmap;		/* Unicode Normalization Map */
  _cups_norm_map_t	*nextnorm;	/* Next Unicode Normalization Map */
  _cups_fold_map_t	*fmap;		/* Unicode Case Folding Map */
  _cups_fold_map_t	*nextfold;	/* Next Unicode Case Folding Map */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Flush all normalization maps...
  */

  for (nmap = cg->normmap_cache; nmap != NULL; nmap = nextnorm)
  {
    free(nmap->uni2norm);
    nextnorm = nmap->next;
    free(nmap);
  }

  cg->normmap_cache = NULL;

  for (fmap = cg->foldmap_cache; fmap != NULL; fmap = nextfold)
  {
    free(fmap->uni2fold);
    nextfold = fmap->next;
    free(fmap);
  }

  cg->foldmap_cache = NULL;

  if (cg->propmap_cache)
  {
    free(cg->propmap_cache->uni2prop);
    free(cg->propmap_cache);
    cg->propmap_cache = NULL;
  }

  if (cg->combmap_cache)
  {
    free(cg->combmap_cache->uni2comb);
    free(cg->combmap_cache);
    cg->combmap_cache = NULL;
  }

  if (cg->breakmap_cache)
  {
    free(cg->breakmap_cache->uni2break);
    free(cg->breakmap_cache);
    cg->breakmap_cache = NULL;
  }
}


/*
 * 'cupsUTF8Normalize()' - Normalize UTF-8 string.
 *
 * Normalize UTF-8 string to Unicode UAX-15 Normalization Form
 * Note - Compatibility Normalization Forms (NFKD/NFKC) are
 * unsafe for subsequent transcoding to legacy charsets
 */

int					/* O - Count or -1 on error */
cupsUTF8Normalize(
    cups_utf8_t            *dest,	/* O - Target string */
    const cups_utf8_t      *src,	/* I - Source string */
    const int              maxout,	/* I - Max output */
    const cups_normalize_t normalize)	/* I - Normalization */
{
  int		len;			/* String length */
  cups_utf32_t	work1[CUPS_MAX_USTRING];/* First internal UCS-4 string */
  cups_utf32_t	work2[CUPS_MAX_USTRING];/* Second internal UCS-4 string */


 /*
  * Check for valid arguments and clear output...
  */

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
    return (-1);

  *dest = 0;

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  len = cupsUTF8ToUTF32(work1, src, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

 /*
  * Normalize internal UCS-4 to second internal UCS-4...
  */

  len = cupsUTF32Normalize(work2, work1, CUPS_MAX_USTRING, normalize);

  if (len < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */

  len = cupsUTF32ToUTF8(dest, work2, maxout);

  return (len);
}


/*
 * 'cupsUTF32Normalize()' - Normalize UTF-32 string.
 *
 * Normalize UTF-32 string to Unicode UAX-15 Normalization Form
 * Note - Compatibility Normalization Forms (NFKD/NFKC) are
 * unsafe for subsequent transcoding to legacy charsets
 */

int					/* O - Count or -1 on error */
cupsUTF32Normalize(
    cups_utf32_t           *dest,	/* O - Target string */
    const cups_utf32_t     *src,	/* I - Source string */
    const int              maxout,	/* I - Max output */
    const cups_normalize_t normalize)	/* I - Normalization */
{
  int			i;		/* Looping variable */
  int			result;		/* Result Value */
  cups_ucs2_t		*mp;		/* Map char pointer */
  int			pass;		/* Pass count for each transform */
  int			hit;		/* Hit count from binary search */
  cups_utf32_t		unichar1;	/* Unicode character value */
  cups_utf32_t		unichar2;	/* Unicode character value */
  _cups_comb_class_t	class1;		/* First Combining Class */
  _cups_comb_class_t	class2;		/* Second Combining Class */
  int			len;		/* String length */
  cups_utf32_t		work1[CUPS_MAX_USTRING];
					/* First internal UCS-4 string */
  cups_utf32_t		work2[CUPS_MAX_USTRING];
					/* Second internal UCS-4 string */
  cups_utf32_t		*p1;		/* First UCS-4 string pointer */
  cups_utf32_t		*p2;		/* Second UCS-4 string pointer */
  _cups_norm_map_t	*nmap;		/* Unicode Normalization Map */
  cups_normalize_t	decompose;	/* Decomposition Type */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid arguments and clear output...
  */

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
    return (-1);

  *dest = 0;

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

 /*
  * Find decomposition map...
  */

  switch (normalize)
  {
    case CUPS_NORM_NFD:
    case CUPS_NORM_NFC:
	decompose = CUPS_NORM_NFD;
	break;

    case CUPS_NORM_NFKD:
    case CUPS_NORM_NFKC:
	decompose = CUPS_NORM_NFKD;
	break;

    default:
	return (-1);
  }

  for (nmap = cg->normmap_cache; nmap != NULL; nmap = nmap->next)
    if (nmap->normalize == decompose)
      break;

  if (nmap == NULL)
    return (-1);

 /*
  * Copy input to internal buffer...
  */

  p1 = &work1[0];

  for (i = 0; i < CUPS_MAX_USTRING; i ++)
  {
    if (*src == 0)
      break;

    *p1 ++ = *src ++;
  }

  *p1 = 0;
  len = i;

 /*
  * Decompose until no further decomposition...
  */

  for (pass = 0; pass < 20; pass ++)
  {
    p1 = &work1[0];
    p2 = &work2[0];

    for (hit = 0; *p1 != 0; p1 ++)
    {
     /*
      * Check for decomposition defined...
      */

      mp = (cups_ucs2_t *)bsearch(p1, nmap->uni2norm, nmap->normcount,
                                  (sizeof(cups_ucs2_t) * 3), compare_decompose);
      if (mp == NULL)
      {
        *p2 ++ = *p1;
        continue;
      }

     /*
      * Decompose input character to one or two output characters...
      */

      hit ++;
      mp ++;
      *p2 ++ = (cups_utf32_t) *mp ++;

      if (*mp != 0)
        *p2 ++ = (cups_utf32_t) *mp;
    }

    *p2 = 0;
    len = (int)(p2 - &work2[0]);

   /*
    * Check for decomposition finished...
    */
    if (hit == 0)
      break;
    memcpy (work1, work2, sizeof(cups_utf32_t) * (len + 1));
  }

 /*
  * Canonical reorder until no further reordering...
  */

  for (pass = 0; pass < 20; pass ++)
  {
    p1 = &work1[0];

    for (hit = 0; *p1 != 0; p1 ++)
    {
     /*
      * Check for combining characters to reorder...
      */

      unichar1 = *p1;
      unichar2 = *(p1 + 1);

      if (unichar2 == 0)
        break;

      class1 = get_combining_class(unichar1);
      class2 = get_combining_class(unichar2);

      if ((class1 < 0) || (class2 < 0))
        return (-1);

      if ((class1 == 0) || (class2 == 0))
        continue;

      if (class1 <= class2)
        continue;

     /*
      * Swap two combining characters...
      */

      *p1 = unichar2;
      p1 ++;
      *p1 = unichar1;
      hit ++;
    }

    if (hit == 0)
      break;
  }

 /*
  * Check for decomposition only...
  */

  if (normalize == CUPS_NORM_NFD || normalize == CUPS_NORM_NFKD)
  {
    memcpy(dest, work1, sizeof(cups_utf32_t) * (len + 1));
    return (len);
  }

 /*
  * Find composition map...
  */

  for (nmap = cg->normmap_cache; nmap != NULL; nmap = nmap->next)
    if (nmap->normalize == CUPS_NORM_NFC)
      break;

  if (nmap == NULL)
    return (-1);

 /*
  * Compose until no further composition...
  */

  for (pass = 0; pass < 20; pass ++)
  {
    p1 = &work1[0];
    p2 = &work2[0];

    for (hit = 0; *p1 != 0; p1 ++)
    {
     /*
      * Check for composition defined...
      */

      unichar1 = *p1;
      unichar2 = *(p1 + 1);

      if (unichar2 == 0)
      {
        *p2 ++ = unichar1;
        break;
      }

      mp = (cups_ucs2_t *)bsearch(p1, nmap->uni2norm, nmap->normcount,
                                  (sizeof(cups_ucs2_t) * 3), compare_compose);
      if (mp == NULL)
      {
        *p2 ++ = *p1;
        continue;
      }

     /*
      * Compose two input characters to one output character...
      */

      hit ++;
      mp += 2;
      *p2 ++ = (cups_utf32_t) *mp;
      p1 ++;
    }

    *p2 = 0;
    len = (int) (p2 - &work2[0]);

   /*
    * Check for composition finished...
    */

    if (hit == 0)
      break;

    memcpy (work1, work2, sizeof(cups_utf32_t) * (len + 1));
  }

  memcpy (dest, work1, sizeof(cups_utf32_t) * (len + 1));

  cupsNormalizeMapsFree();

  return (len);
}


/*
 * 'cupsUTF8CaseFold()' - Case fold UTF-8 string.
 *
 * Case Fold UTF-8 string per Unicode UAX-21 Section 2.3
 * Note - Case folding output is
 * unsafe for subsequent transcoding to legacy charsets
 */

int					/* O - Count or -1 on error */
cupsUTF8CaseFold(
    cups_utf8_t          *dest,		/* O - Target string */
    const cups_utf8_t    *src,		/* I - Source string */
    const int            maxout,	/* I - Max output */
    const cups_folding_t fold)		/* I - Fold Mode */
{
  int		len;			/* String length */
  cups_utf32_t	work1[CUPS_MAX_USTRING];/* First internal UCS-4 string */
  cups_utf32_t	work2[CUPS_MAX_USTRING];/* Second internal UCS-4 string */


 /*
  * Check for valid arguments and clear output...
  */

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
    return (-1);

  *dest = 0;

  if (fold != CUPS_FOLD_SIMPLE && fold != CUPS_FOLD_FULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  len = cupsUTF8ToUTF32(work1, src, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

 /*
  * Case Fold internal UCS-4 to second internal UCS-4...
  */

  len = cupsUTF32CaseFold(work2, work1, CUPS_MAX_USTRING, fold);

  if (len < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */

  len = cupsUTF32ToUTF8(dest, work2, maxout);

  return (len);
}


/*
 * 'cupsUTF32CaseFold()' - Case fold UTF-32 string.
 *
 * Case Fold UTF-32 string per Unicode UAX-21 Section 2.3
 * Note - Case folding output is
 * unsafe for subsequent transcoding to legacy charsets
 */

int					/* O - Count or -1 on error */
cupsUTF32CaseFold(
    cups_utf32_t         *dest,		/* O - Target string */
    const cups_utf32_t   *src,		/* I - Source string */
    const int            maxout,	/* I - Max output */
    const cups_folding_t fold)		/* I - Fold Mode */
{
  cups_utf32_t		*start = dest;	/* Start of destination string */
  int			i;		/* Looping variable */
  int			result;		/* Result Value */
  cups_ucs2_t		*mp;		/* Map char pointer */
  _cups_fold_map_t	*fmap;		/* Unicode Case Folding Map */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid arguments and clear output...
  */

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
    return (-1);

  *dest = 0;

  if (fold != CUPS_FOLD_SIMPLE && fold != CUPS_FOLD_FULL)
    return (-1);

 /*
  * Find case folding map...
  */

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

  for (fmap = cg->foldmap_cache; fmap != NULL; fmap = fmap->next)
    if (fmap->fold == fold)
      break;

  if (fmap == NULL)
    return (-1);

 /*
  * Case fold input string to output string...
  */

  for (i = 0; i < (maxout - 1); i ++, src ++)
  {
   /*
    * Check for case folding defined...
    */

    mp = (cups_ucs2_t *)bsearch(src, fmap->uni2fold, fmap->foldcount,
                                (sizeof(cups_ucs2_t) * 4), compare_foldchar);
    if (mp == NULL)
    {
      *dest ++ = *src;
      continue;
    }

   /*
    * Case fold input character to one or two output characters...
    */

    mp ++;
    *dest ++ = (cups_utf32_t) *mp ++;

    if (*mp != 0 && fold == CUPS_FOLD_FULL)
    {
      i ++;
      if (i >= (maxout - 1))
        break;

      *dest ++ = (cups_utf32_t) *mp;
    }
  }

  *dest = 0;

  cupsNormalizeMapsFree();

  return (dest - start);
}


/*
 * 'cupsUTF8CompareCaseless()' - Compare case folded UTF-8 strings.
 */

int					/* O - Difference of strings */
cupsUTF8CompareCaseless(
    const cups_utf8_t *s1,		/* I - String1 */
    const cups_utf8_t *s2)		/* I - String2 */
{
  int		difference;		/* Difference of two strings */
  int		len;			/* String length */
  cups_utf32_t	work1[CUPS_MAX_USTRING];/* First internal UCS-4 string */
  cups_utf32_t	work2[CUPS_MAX_USTRING];/* Second internal UCS-4 string */


 /*
  * Check for valid arguments...
  */

  if (!s1 || !s2)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  len = cupsUTF8ToUTF32(work1, s1, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

  len = cupsUTF8ToUTF32(work2, s2, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

 /*
  * Compare first internal UCS-4 to second internal UCS-4...
  */

  difference = cupsUTF32CompareCaseless(work1, work2);

  return (difference);
}


/*
 * 'cupsUTF32CompareCaseless()' - Compare case folded UTF-32 strings.
 */

int					/* O - Difference of strings */
cupsUTF32CompareCaseless(
    const cups_utf32_t *s1,		/* I - String1 */
    const cups_utf32_t *s2)		/* I - String2 */
{
  int			difference;	/* Difference of two strings */
  int			len;		/* String length */
  cups_folding_t	fold = CUPS_FOLD_FULL;
					/* Case folding mode */
  cups_utf32_t		fold1[CUPS_MAX_USTRING];
					/* First UCS-4 folded string */
  cups_utf32_t		fold2[CUPS_MAX_USTRING];
					/* Second UCS-4 folded string */
  cups_utf32_t		*p1;		/* First UCS-4 string pointer */
  cups_utf32_t		*p2;		/* Second UCS-4 string pointer */


 /*
  * Check for valid arguments...
  */

  if (!s1 || !s2)
    return (-1);

 /*
  * Case Fold input UTF-32 strings to internal UCS-4 strings...
  */

  len = cupsUTF32CaseFold(fold1, s1, CUPS_MAX_USTRING, fold);

  if (len < 0)
    return (-1);

  len = cupsUTF32CaseFold(fold2, s2, CUPS_MAX_USTRING, fold);

  if (len < 0)
    return (-1);

 /*
  * Compare first internal UCS-4 to second internal UCS-4...
  */

  p1 = &fold1[0];
  p2 = &fold2[0];

  for (;; p1 ++, p2 ++)
  {
    difference = (int) (*p1 - *p2);

    if (difference != 0)
      break;

    if ((*p1 == 0) && (*p2 == 0))
      break;
  }

  return (difference);
}


/*
 * 'cupsUTF8CompareIdentifier()' - Compare folded NFKC UTF-8 strings.
 */

int					/* O - Result of comparison */
cupsUTF8CompareIdentifier(
    const cups_utf8_t *s1,		/* I - String1 */
    const cups_utf8_t *s2)		/* I - String2 */
{
  int		difference;		/* Difference of two strings */
  int		len;			/* String length */
  cups_utf32_t	work1[CUPS_MAX_USTRING];/* First internal UCS-4 string */
  cups_utf32_t	work2[CUPS_MAX_USTRING];/* Second internal UCS-4 string */


 /*
  * Check for valid arguments...
  */

  if (!s1 || !s2)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  len = cupsUTF8ToUTF32(work1, s1, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

  len = cupsUTF8ToUTF32(work2, s2, CUPS_MAX_USTRING);

  if (len < 0)
    return (-1);

 /*
  * Compare first internal UCS-4 to second internal UCS-4...
  */

  difference = cupsUTF32CompareIdentifier(work1, work2);

  return (difference);
}


/*
 * 'cupsUTF32CompareIdentifier()' - Compare folded NFKC UTF-32 strings.
 */

int					/* O - Result of comparison */
cupsUTF32CompareIdentifier(
    const cups_utf32_t *s1,		/* I - String1 */
    const cups_utf32_t *s2)		/* I - String2 */
{
  int			difference;     /* Difference of two strings */
  int			len;            /* String length */
  cups_folding_t	fold = CUPS_FOLD_FULL;
					/* Case folding mode */
  cups_utf32_t		fold1[CUPS_MAX_USTRING];
					/* First UCS-4 folded string */
  cups_utf32_t		fold2[CUPS_MAX_USTRING];
					/* Second UCS-4 folded string */
  cups_normalize_t	normalize = CUPS_NORM_NFKC;
					/* Normalization form */
  cups_utf32_t		norm1[CUPS_MAX_USTRING];
					/* First UCS-4 normalized string */
  cups_utf32_t		norm2[CUPS_MAX_USTRING];
					/* Second UCS-4 normalized string */
  cups_utf32_t		*p1;		/* First UCS-4 string pointer */
  cups_utf32_t		*p2;		/* Second UCS-4 string pointer */


 /*
  * Check for valid arguments...
  */

  if (!s1 || !s2)
    return (-1);

 /*
  * Case Fold input UTF-32 strings to internal UCS-4 strings...
  */

  len = cupsUTF32CaseFold(fold1, s1, CUPS_MAX_USTRING, fold);

  if (len < 0)
    return (-1);

  len = cupsUTF32CaseFold(fold2, s2, CUPS_MAX_USTRING, fold);

  if (len < 0)
    return (-1);

 /*
  * Normalize internal UCS-4 strings to NFKC...
  */

  len = cupsUTF32Normalize(norm1, fold1, CUPS_MAX_USTRING, normalize);

  if (len < 0)
    return (-1);

  len = cupsUTF32Normalize(norm2, fold2, CUPS_MAX_USTRING, normalize);

  if (len < 0)
    return (-1);

 /*
  * Compare first internal UCS-4 to second internal UCS-4...
  */

  p1 = &norm1[0];
  p2 = &norm2[0];

  for (;; p1 ++, p2 ++)
  {
    difference = (int) (*p1 - *p2);

    if (difference != 0)
      break;

    if ((*p1 == 0) && (*p2 == 0))
      break;
  }

  return (difference);
}


/*
 * 'cupsUTF32CharacterProperty()' - Get UTF-32 character property.
 */

int					/* O - Result of comparison */
cupsUTF32CharacterProperty(
    const cups_utf32_t    ch,		/* I - Source char */
    const cups_property_t prop)		/* I - Char Property */
{
  int	result;				/* Result Value */


 /*
  * Check for valid arguments...
  */

  if (ch == 0)
    return (-1);

 /*
  * Find character property...
  */

  switch (prop)
  {
    case CUPS_PROP_GENERAL_CATEGORY:
	result = (get_general_category(ch));
	break;

    case CUPS_PROP_BIDI_CATEGORY:
	result = (get_bidi_category(ch));
	break;

    case CUPS_PROP_COMBINING_CLASS:
	result = (get_combining_class(ch));
	break;
    case CUPS_PROP_BREAK_CLASS:
	result = (get_break_class(ch));
	break;

    default:
        return (-1);
  }

  return (result);
}


/*
 * 'get_general_category()' - Get UTF-32 Character General Category.
 */

static int				/* O - Class or -1 on error */
get_general_category(
    const cups_utf32_t ch)		/* I - Source char */
{
  int			result;		/* Result Value */
  cups_gencat_t		gencat;		/* General Category Value */
  _cups_prop_map_t	*pmap;		/* Unicode Property Map */
  _cups_prop_t		*uni2prop;	/* Unicode Char -> Properties */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid argument...
  */

  if (ch == 0)
    return (-1);

 /*
  * Find property map...
  */

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

  pmap = cg->propmap_cache;

  if (pmap == NULL)
    return (-1);

 /*
  * Find character in map...
  */

  uni2prop = (_cups_prop_t *)bsearch(&ch, pmap->uni2prop, pmap->propcount,
                                    (sizeof(_cups_prop_t)), compare_propchar);

  cupsNormalizeMapsFree();

  if (uni2prop == NULL)
    gencat = CUPS_GENCAT_CN;            /* Other, Not Assigned */
  else
    gencat = (cups_gencat_t)uni2prop->gencat;

  result = (int)gencat;

  return (result);
}


/*
 * 'get_bidi_category()' - Get UTF-32 Character Bidi Category.
 */

static int				/* O - Class or -1 on error */
get_bidi_category(const cups_utf32_t ch)/* I - Source char */
{
  int			result;		/* Result Value */
  cups_bidi_t	bidicat;	/* Bidi Category Value */
  _cups_prop_map_t	*pmap;		/* Unicode Property Map */
  _cups_prop_t		*uni2prop;	/* Unicode Char -> Properties */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid argument...
  */

  if (ch == 0)
    return (-1);

 /*
  * Find property map...
  */

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

  pmap = cg->propmap_cache;

  if (pmap == NULL)
    return (-1);

 /*
  * Find character in map...
  */

  uni2prop = (_cups_prop_t *)bsearch(&ch, pmap->uni2prop, pmap->propcount,
                                    (sizeof(_cups_prop_t)), compare_propchar);

  cupsNormalizeMapsFree();

  if (uni2prop == NULL)
    bidicat = CUPS_BIDI_ON;             /* Other Neutral */
  else
    bidicat = (cups_bidi_t)uni2prop->bidicat;

  result = (int)bidicat;

  return (result);
}

/*
 * 'get_combining_class()' - Get UTF-32 Character Combining Class.
 *
 * Note - Zero is non-combining (base character)
 */

static int				/* O - Class or -1 on error */
get_combining_class(
    const cups_utf32_t ch)		/* I - Source char */
{
  int			result;		/* Result Value */
  _cups_comb_map_t	*cmap;		/* Unicode Combining Class Map */
  _cups_comb_class_t	combclass;	/* Unicode Combining Class */
  _cups_comb_t		*uni2comb;	/* Unicode Char -> Combining Class */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid argument...
  */

  if (ch == 0)
    return (-1);

 /*
  * Find combining class map...
  */

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

  cmap = cg->combmap_cache;

  if (cmap == NULL)
    return (-1);

 /*
  * Find combining character in map...
  */

  uni2comb = (_cups_comb_t *)bsearch(&ch, cmap->uni2comb, cmap->combcount,
                                    (sizeof(_cups_comb_t)), compare_combchar);

  cupsNormalizeMapsFree();

  if (uni2comb == NULL)
    combclass = 0;
  else
    combclass = (_cups_comb_class_t)uni2comb->combclass;

  result = (int)combclass;

  return (result);
}


/*
 * 'get_break_class()' - Get UTF-32 Character Line Break Class.
 */

static int				/* O - Class or -1 on error */
get_break_class(const cups_utf32_t ch)	/* I - Source char */
{
  int			result;		/* Result Value */
  _cups_break_map_t	*bmap;		/* Unicode Line Break Class Map */
  cups_break_class_t	breakclass;	/* Unicode Line Break Class */
  cups_ucs2_t		*uni2break;	/* Unicode -> Line Break Class */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Check for valid argument...
  */

  if (ch == 0)
    return (-1);

 /*
  * Find line break class map...
  */

  result = cupsNormalizeMapsGet();

  if (result < 0)
    return (-1);

  bmap = cg->breakmap_cache;

  if (bmap == NULL)
    return (-1);

 /*
  * Find line break character in map...
  */

  uni2break = (cups_ucs2_t *)bsearch(&ch, bmap->uni2break, bmap->breakcount,
                                     (sizeof(cups_ucs2_t) * 3),
				     compare_breakchar);

  cupsNormalizeMapsFree();

  if (uni2break == NULL)
    breakclass = CUPS_BREAK_AI;
  else
    breakclass = (cups_break_class_t)*(uni2break + 2);

  result = (int)breakclass;

  return (result);
}


/*
 * 'get_map_count()' - Count lines in a map file.
 */

static int				/* O - Count or -1 on error */
get_map_count(const char *filename)	/* I - Map Filename */
{
  int		i;			/* Looping variable */
  cups_file_t	*fp;			/* Map input file pointer */
  char		*s;			/* Line parsing pointer */
  char		line[256];		/* Line from input map file */
  cups_utf32_t	unichar;		/* Unicode character value */


 /*
  * Open map input file...
  */

  if (!filename || !*filename)
    return (-1);

  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Count lines in map input file...
  */

  for (i = 0; i < 50000;)
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if (sscanf(s, "%lx", &unichar) != 1)
      break;
    if (unichar > 0xffff)
      break;
    i ++;
  }
  if (i == 0)
    i = -1;

 /*
  * Close file and return map count (non-comment line count)...
  */

  cupsFileClose(fp);

  return (i);
}


/*
 * 'get_normmap()' - Get Unicode normalization map to cache.
 */

static int				/* O - Zero or -1 on error */
get_normmap(
    const cups_normalize_t normalize)	/* I - Normalization Form */
{
  int			i;		/* Looping variable */
  cups_utf32_t		unichar1;	/* Unicode character value */
  cups_utf32_t		unichar2;	/* Unicode character value */
  cups_utf32_t		unichar3;	/* Unicode character value */
  _cups_norm_map_t	*nmap;		/* Unicode Normalization Map */
  int			normcount;	/* Count of Unicode Source Chars */
  cups_ucs2_t		*uni2norm;	/* Unicode Char -> Normalization */
  char			*datadir;	/* CUPS_DATADIR environment variable */
  char			*mapname;	/* Normalization map name */
  char			filename[1024];	/* Filename for charset map file */
  cups_file_t		*fp;		/* Normalization map file pointer */
  char			*s;		/* Line parsing pointer */
  char			line[256];	/* Line from input map file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have this normalization map loaded...
  */

  for (nmap = cg->normmap_cache; nmap != NULL; nmap = nmap->next)
    if (nmap->normalize == normalize)
      return (0);

 /*
  * Get the data directory and mapping name...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  switch (normalize)
  {
    case CUPS_NORM_NFD:         /* Canonical Decomposition */
	mapname = "uni-nfd.txt";
	break;

    case CUPS_NORM_NFKD:        /* Compatibility Decomposition */
	mapname = "uni-nfkd.txt";
	break;

    case CUPS_NORM_NFC:         /* Canonical Composition */
	mapname = "uni-nfc.txt";
	break;

    case CUPS_NORM_NFKC:        /* no such map file... */
    default:
        return (-1);
  }

 /*
  * Open normalization map input file...
  */

  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);
  if ((normcount = get_map_count(filename)) <= 0)
    return (-1);

  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Allocate memory for normalization map and add to cache...
  */

  nmap = (_cups_norm_map_t *)calloc(1, sizeof(_cups_norm_map_t));
  if (nmap == NULL)
  {
    cupsFileClose(fp);
    return (-1);
  }

  uni2norm = (cups_ucs2_t *)calloc(1, sizeof(cups_ucs2_t) * 3 * normcount);
  if (uni2norm == NULL)
  {
    free(nmap);
    cupsFileClose(fp);
    return (-1);
  }
  nmap->next = cg->normmap_cache;
  cg->normmap_cache = nmap;
  nmap->used ++;
  nmap->normalize = normalize;
  nmap->normcount = normcount;
  nmap->uni2norm = uni2norm;

 /*
  * Save normalization map into memory for later use...
  */
  for (i = 0; i < normcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (sscanf(s, "%lx %lx %lx", &unichar1, &unichar2, &unichar3) != 3)
       break;
    if ((unichar1 > 0xffff)
    || (unichar2 > 0xffff)
    || (unichar3 > 0xffff))
      break;
    *uni2norm ++ = (cups_ucs2_t) unichar1;
    *uni2norm ++ = (cups_ucs2_t) unichar2;
    *uni2norm ++ = (cups_ucs2_t) unichar3;
    i ++;
  }
  if (i < normcount)
    nmap->normcount = i;
  cupsFileClose(fp);
  return (0);
}


/*
 * 'get_foldmap()' - Get Unicode case folding map to cache.
 */

static int				/* O - Zero or -1 on error */
get_foldmap(const cups_folding_t fold)	/* I - Case folding type */
{
  int			i;		/* Looping variable */
  cups_utf32_t		unichar1;	/* Unicode character value */
  cups_utf32_t		unichar2;	/* Unicode character value */
  cups_utf32_t		unichar3;	/* Unicode character value */
  cups_utf32_t		unichar4;	/* Unicode character value */
  _cups_fold_map_t	*fmap;		/* Unicode Case Folding Map */
  int			foldcount;	/* Count of Unicode Source Chars */
  cups_ucs2_t		*uni2fold;	/* Unicode -> Folded Char(s) */
  char			*datadir;	/* CUPS_DATADIR env variable */
  char			*mapname;	/* Case Folding map name */
  char			filename[1024];	/* Filename for charset map file */
  cups_file_t		*fp;		/* Case Folding map file pointer */
  char			*s;		/* Line parsing pointer */
  char			line[256];	/* Line from input map file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have this case folding map loaded...
  */

  for (fmap = cg->foldmap_cache; fmap != NULL; fmap = fmap->next)
    if (fmap->fold == fold)
      return (0);

 /*
  * Get the data directory and mapping name...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  switch (fold)
  {
    case CUPS_FOLD_SIMPLE:      /* Simple case folding */
	mapname = "uni-fold.txt";
	break;
    case CUPS_FOLD_FULL:        /* Full case folding */
	mapname = "uni-full.txt";
	break;
    default:
	return (-1);
  }

 /*
  * Open case folding map input file...
  */

  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);
  if ((foldcount = get_map_count(filename)) <= 0)
    return (-1);
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Allocate memory for case folding map and add to cache...
  */
  fmap = (_cups_fold_map_t *)calloc(1, sizeof(_cups_fold_map_t));
  if (fmap == NULL)
  {
    cupsFileClose(fp);
    return (-1);
  }
  uni2fold = (cups_ucs2_t *)calloc(1, sizeof(cups_ucs2_t) * 4 * foldcount);
  if (uni2fold == NULL)
  {
    free(fmap);
    cupsFileClose(fp);
    return (-1);
  }
  fmap->next = cg->foldmap_cache;
  cg->foldmap_cache = fmap;
  fmap->used ++;
  fmap->fold = fold;
  fmap->foldcount = foldcount;
  fmap->uni2fold = uni2fold;

 /*
  * Save case folding map into memory for later use...
  */

  for (i = 0; i < foldcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    unichar1 = unichar2 = unichar3 = unichar4 = 0;
    if ((fold == CUPS_FOLD_SIMPLE)
    && (sscanf(s, "%lx %lx", &unichar1, &unichar2) != 2))
      break;
    if ((fold == CUPS_FOLD_FULL)
    && (sscanf(s, "%lx %lx %lx %lx",
               &unichar1, &unichar2, &unichar3, &unichar4) != 4))
      break;
    if ((unichar1 > 0xffff)
    || (unichar2 > 0xffff)
    || (unichar3 > 0xffff)
    || (unichar4 > 0xffff))
      break;
    *uni2fold ++ = (cups_ucs2_t) unichar1;
    *uni2fold ++ = (cups_ucs2_t) unichar2;
    *uni2fold ++ = (cups_ucs2_t) unichar3;
    *uni2fold ++ = (cups_ucs2_t) unichar4;
    i ++;
  }
  if (i < foldcount)
    fmap->foldcount = i;
  cupsFileClose(fp);
  return (0);
}

/*
 * 'get_propmap()' - Get Unicode character property map to cache.
 */

static int				/* O - Zero or -1 on error */
get_propmap(void)
{
  int			i, j;		/* Looping variables */
  int			len;		/* String length */
  cups_utf32_t		unichar;	/* Unicode character value */
  cups_gencat_t		gencat;		/* General Category Value */
  cups_bidi_t	bidicat;	/* Bidi Category Value */
  _cups_prop_map_t	*pmap;		/* Unicode Char Property Map */
  int			propcount;	/* Count of Unicode Source Chars */
  _cups_prop_t		*uni2prop;	/* Unicode Char -> Properties */
  char			*datadir;	/* CUPS_DATADIR environment variable */
  char			*mapname;	/* Char Property map name */
  char			filename[1024];	/* Filename for charset map file */
  cups_file_t		*fp;		/* Char Property map file pointer */
  char			*s;		/* Line parsing pointer */
  char			line[256];	/* Line from input map file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have this char properties map loaded...
  */

  if ((pmap = cg->propmap_cache) != NULL)
    return (0);

 /*
  * Get the data directory and mapping name...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  mapname = "uni-prop.txt";

 /*
  * Open char properties map input file...
  */
  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);
  if ((propcount = get_map_count(filename)) <= 0)
    return (-1);
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Allocate memory for char properties map and add to cache...
  */
  pmap = (_cups_prop_map_t *)calloc(1, sizeof(_cups_prop_map_t));
  if (pmap == NULL)
  {
    cupsFileClose(fp);
    return (-1);
  }
  uni2prop = (_cups_prop_t *)calloc(1, sizeof(_cups_prop_t) * propcount);
  if (uni2prop == NULL)
  {
    free(pmap);
    cupsFileClose(fp);
    return (-1);
  }
  cg->propmap_cache = pmap;
  pmap->used ++;
  pmap->propcount = propcount;
  pmap->uni2prop = uni2prop;

 /*
  * Save char properties map into memory for later use...
  */
  for (i = 0; i < propcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if (strlen(s) > 0)
      *(s + strlen(s) - 1) = '\0';
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (sscanf(s, "%lx", &unichar) != 1)
       break;
    if (unichar > 0xffff)
      break;
    while ((*s != '\0') && (*s != ';'))
      s ++;
    if (*s != ';')
      break;
    s ++;
    for (j = 0; gencat_index[j].str != NULL; j ++)
    {
      len = strlen(gencat_index[j].str);
      if (strncmp (s, gencat_index[j].str, len) == 0)
        break;
    }
    if (gencat_index[j].str == NULL)
      return (-1);
    gencat = gencat_index[j].gencat;
    while ((*s != '\0') && (*s != ';'))
      s ++;
    if (*s != ';')
      break;
    s ++;
    for (j = 0; bidicat_index[j] != NULL; j ++)
    {
      len = strlen(bidicat_index[j]);
      if (strncmp (s, bidicat_index[j], len) == 0)
        break;
    }
    if (bidicat_index[j] == NULL)
      return (-1);
    bidicat = (cups_bidi_t) j;
    uni2prop->ch = (cups_ucs2_t) unichar;
    uni2prop->gencat = (unsigned char) gencat;
    uni2prop->bidicat = (unsigned char) bidicat;
    uni2prop ++;
    i ++;
  }
  if (i < propcount)
    pmap->propcount = i;
  cupsFileClose(fp);
  return (0);
}


/*
 * 'get_combmap()' - Get Unicode combining class map to cache.
 */

static int				/* O - Zero or -1 on error */
get_combmap(void)
{
  int			i;		/* Looping variable */
  cups_utf32_t		unichar;	/* Unicode character value */
  int			combclass;	/* Unicode char combining class */
  _cups_comb_map_t	*cmap;		/* Unicode Comb Class Map */
  int			combcount;	/* Count of Unicode Source Chars */
  _cups_comb_t		*uni2comb;	/* Unicode Char -> Combining Class */
  char			*datadir;	/* CUPS_DATADIR environment variable */
  char			*mapname;	/* Comb Class map name */
  char			filename[1024];	/* Filename for charset map file */
  cups_file_t		*fp;		/* Comb Class map file pointer */
  char			*s;		/* Line parsing pointer */
  char			line[256];	/* Line from input map file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have this combining class map loaded...
  */

  if ((cmap = cg->combmap_cache) != NULL)
    return (0);

 /*
  * Get the data directory and mapping name...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  mapname = "uni-comb.txt";

 /*
  * Open combining class map input file...
  */

  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);
  if ((combcount = get_map_count(filename)) <= 0)
    return (-1);
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Allocate memory for combining class map and add to cache...
  */

  cmap = (_cups_comb_map_t *)calloc(1, sizeof(_cups_comb_map_t));
  if (cmap == NULL)
  {
    cupsFileClose(fp);
    return (-1);
  }

  uni2comb = (_cups_comb_t *)calloc(1, sizeof(_cups_comb_t) * combcount);
  if (uni2comb == NULL)
  {
    free(cmap);
    cupsFileClose(fp);
    return (-1);
  }
  cg->combmap_cache = cmap;
  cmap->used ++;
  cmap->combcount = combcount;
  cmap->uni2comb = uni2comb;

 /*
  * Save combining class map into memory for later use...
  */
  for (i = 0; i < combcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (sscanf(s, "%lx", &unichar) != 1)
       break;
    if (unichar > 0xffff)
      break;
    while ((*s != '\0') && (*s != ';'))
      s ++;
    if (*s != ';')
      break;
    s ++;
    if (sscanf(s, "%d", &combclass) != 1)
       break;
    uni2comb->ch = (cups_ucs2_t) unichar;
    uni2comb->combclass = (unsigned char) combclass;
    uni2comb ++;
    i ++;
  }
  if (i < combcount)
    cmap->combcount = i;
  cupsFileClose(fp);
  return (0);
}


/*
 * 'get_breakmap()' - Get Unicode line break class map to cache.
 */

static int				/* O - Zero or -1 on error */
get_breakmap(void)
{
  int			i, j;		/* Looping variables */
  int			len;		/* String length */
  cups_utf32_t		unichar1;	/* Unicode character value */
  cups_utf32_t		unichar2;	/* Unicode character value */
  cups_break_class_t	breakclass;	/* Unicode char line break class */
  _cups_break_map_t	*bmap;		/* Unicode Line Break Class Map */
  int			breakcount;	/* Count of Unicode Source Chars */
  cups_ucs2_t		*uni2break;	/* Unicode -> Line Break Class */
  char			*datadir;	/* CUPS_DATADIR environment variable */
  char			*mapname;	/* Comb Class map name */
  char			filename[1024];	/* Filename for charset map file */
  cups_file_t		*fp;		/* Comb Class map file pointer */
  char			*s;		/* Line parsing pointer */
  char			line[256];	/* Line from input map file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * See if we already have this line break class map loaded...
  */

  if ((bmap = cg->breakmap_cache) != NULL)
    return (0);

 /*
  * Get the data directory and mapping name...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  mapname = "uni-line.txt";

 /*
  * Open line break class map input file...
  */

  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);
  if ((breakcount = get_map_count(filename)) <= 0)
    return (-1);
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Allocate memory for line break class map and add to cache...
  */

  bmap = (_cups_break_map_t *)calloc(1, sizeof(_cups_break_map_t));
  if (bmap == NULL)
  {
    cupsFileClose(fp);
    return (-1);
  }

  uni2break = (cups_ucs2_t *)calloc(1, sizeof(cups_ucs2_t) * 3 * breakcount);
  if (uni2break == NULL)
  {
    free(bmap);
    cupsFileClose(fp);
    return (-1);
  }
  cg->breakmap_cache = bmap;
  bmap->used ++;
  bmap->breakcount = breakcount;
  bmap->uni2break = uni2break;

 /*
  * Save line break class map into memory for later use...
  */
  for (i = 0; i < breakcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if (strlen(s) > 0)
      *(s + strlen(s) - 1) = '\0';
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (sscanf(s, "%lx %lx", &unichar1, &unichar2) != 2)
       break;
    if ((unichar1 > 0xffff)
    || (unichar2 > 0xffff))
      break;
    while ((*s != '\0') && (*s != ';'))
      s ++;
    if (*s != ';')
      break;
    s ++;
    for (j = 0; break_index[j].str != NULL; j ++)
    {
      len = strlen (break_index[j].str);
      if (strncmp (s, break_index[j].str, len) == 0)
        break;
    }
    if (break_index[j].str == NULL)
      return (-1);
    breakclass = break_index[j].breakclass;
    *uni2break ++ = (cups_ucs2_t) unichar1;
    *uni2break ++ = (cups_ucs2_t) unichar2;
    *uni2break ++ = (cups_ucs2_t) breakclass;
    i ++;
  }
  if (i < breakcount)
    bmap->breakcount = i;
  cupsFileClose(fp);
  return (0);
}


/*
 * 'compare_compose()' - Compare key for compose match.
 *
 * Note - This function cannot be easily modified for 32-bit Unicode.
 */

static int				/* O - Result of comparison */
compare_compose(const void *k1,		/* I - Key char */
		const void *k2)		/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  cups_ucs2_t	*mp = (cups_ucs2_t *)k2;/* Map char pointer */
  unsigned long	key;			/* Pair of key characters */
  unsigned long	map;			/* Pair of map characters */
  int		result;			/* Result Value */


  key = (*kp << 16);
  key |= *(kp + 1);
  map = (unsigned long) (*mp << 16);
  map |= (unsigned long) *(mp + 1);

  if (key >= map)
    result = (int) (key - map);
  else
    result = -1 * ((int) (map - key));

  return (result);
}


/*
 * 'compare_decompose()' - Compare key for decompose match.
 */

static int				/* O - Result of comparison */
compare_decompose(const void *k1,	/* I - Key char */
		  const void *k2)	/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  cups_ucs2_t	*mp = (cups_ucs2_t *)k2;/* Map char pointer */
  cups_ucs2_t	ch;			/* Key char as UCS-2 */
  int		result;			/* Result Value */


  ch = (cups_ucs2_t) *kp;

  if (ch >= *mp)
    result = (int) (ch - *mp);
  else
    result = -1 * ((int) (*mp - ch));

  return (result);
}


/*
 * 'compare_foldchar()' - Compare key for case fold match.
 */

static int				/* O - Result of comparison */
compare_foldchar(const void *k1,	/* I - Key char */
		 const void *k2)	/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  cups_ucs2_t	*mp = (cups_ucs2_t *)k2;/* Map char pointer */
  cups_ucs2_t	ch;			/* Key char as UCS-2 */
  int		result;			/* Result Value */


  ch = (cups_ucs2_t) *kp;

  if (ch >= *mp)
    result = (int) (ch - *mp);
  else
    result = -1 * ((int) (*mp - ch));

  return (result);
}


/*
 * 'compare_combchar()' - Compare key for combining char match.
 */

static int				/* O - Result of comparison */
compare_combchar(const void *k1,	/* I - Key char */
                 const void *k2)	/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  _cups_comb_t	*cp = (_cups_comb_t *)k2;/* Combining map row pointer */
  cups_ucs2_t	ch;			/* Key char as UCS-2 */
  int		result;			/* Result Value */


  ch = (cups_ucs2_t) *kp;

  if (ch >= cp->ch)
    result = (int) (ch - cp->ch);
  else
    result = -1 * ((int) (cp->ch - ch));

  return (result);
}


/*
 * 'compare_breakchar()' - Compare key for line break char match.
 */

static int				/* O - Result of comparison */
compare_breakchar(const void *k1,	/* I - Key char */
                  const void *k2)	/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  cups_ucs2_t	*mp = (cups_ucs2_t *)k2;/* Map char pointer */
  cups_ucs2_t	ch;			/* Key char as UCS-2 */
  int		result;			/* Result Value */


  ch = (cups_ucs2_t) *kp;

  if (ch < *mp)
    result = -1 * (int) (*mp - ch);
  else if (ch > *(mp + 1))
    result = (int) (ch - *(mp + 1));
  else
    result = 0;

  return (result);
}


/*
 * 'compare_propchar()' - Compare key for property char match.
 */

static int				/* O - Result of comparison */
compare_propchar(const void *k1,	/* I - Key char */
		 const void *k2)	/* I - Map char */
{
  cups_utf32_t	*kp = (cups_utf32_t *)k1;
					/* Key char pointer */
  _cups_prop_t	*pp = (_cups_prop_t *)k2;/* Property map row pointer */
  cups_ucs2_t	ch;			/* Key char as UCS-2 */
  int		result;			/* Result Value */


  ch = (cups_ucs2_t) *kp;

  if (ch >= pp->ch)
    result = (int) (ch - pp->ch);
  else
    result = -1 * ((int) (pp->ch - ch));

  return (result);
}


/*
 * End of "$Id$"
 */
