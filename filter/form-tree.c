/*
 * "$Id: form-tree.c,v 1.2 2000/04/18 19:41:11 mike Exp $"
 *
 *   CUPS form document tree routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include "form.h"


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
formNew(element_t e,			/* I - Element type */
        tree_t    *p)			/* I - Parent node */
{
}


/*
 * 'formRead()' - Read a form tree from a file.
 */

tree_t *				/* O - New form tree */
formRead(FILE   *fp,			/* I - File to read from */
         tree_t *p)			/* I - Parent node */
{
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
 * End of "$Id: form-tree.c,v 1.2 2000/04/18 19:41:11 mike Exp $".
 */
