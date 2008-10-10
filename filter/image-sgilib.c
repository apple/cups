/*
 * "$Id: image-sgilib.c 7221 2008-01-16 22:20:08Z mike $"
 *
 *   SGI image file format library routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
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
 *   sgiClose()    - Close an SGI image file.
 *   sgiGetRow()   - Get a row of image data from a file.
 *   sgiOpen()     - Open an SGI image file for reading or writing.
 *   sgiOpenFile() - Open an SGI image file for reading or writing.
 *   sgiPutRow()   - Put a row of image data to a file.
 *   getlong()     - Get a 32-bit big-endian integer.
 *   getshort()    - Get a 16-bit big-endian integer.
 *   putlong()     - Put a 32-bit big-endian integer.
 *   putshort()    - Put a 16-bit big-endian integer.
 *   read_rle8()   - Read 8-bit RLE data.
 *   read_rle16()  - Read 16-bit RLE data.
 *   write_rle8()  - Write 8-bit RLE data.
 *   write_rle16() - Write 16-bit RLE data.
 */

#include "image-sgi.h"


/*
 * Local functions...
 */

static int	getlong(FILE *);
static int	getshort(FILE *);
static int	putlong(long, FILE *);
static int	putshort(unsigned short, FILE *);
static int	read_rle8(FILE *, unsigned short *, int);
static int	read_rle16(FILE *, unsigned short *, int);
static int	write_rle8(FILE *, unsigned short *, int);
static int	write_rle16(FILE *, unsigned short *, int);


/*
 * 'sgiClose()' - Close an SGI image file.
 */

int					/* O - 0 on success, -1 on error */
sgiClose(sgi_t *sgip)			/* I - SGI image */
{
  int	i;				/* Return status */
  long	*offset;			/* Looping var for offset table */


  if (sgip == NULL)
    return (-1);

  if (sgip->mode == SGI_WRITE && sgip->comp != SGI_COMP_NONE)
  {
   /*
    * Write the scanline offset table to the file...
    */

    fseek(sgip->file, 512, SEEK_SET);

    for (i = sgip->ysize * sgip->zsize, offset = sgip->table[0];
         i > 0;
         i --, offset ++)
      if (putlong(offset[0], sgip->file) < 0)
        return (-1);

    for (i = sgip->ysize * sgip->zsize, offset = sgip->length[0];
         i > 0;
         i --, offset ++)
      if (putlong(offset[0], sgip->file) < 0)
        return (-1);
  }

  if (sgip->table != NULL)
  {
    free(sgip->table[0]);
    free(sgip->table);
  }

  if (sgip->length != NULL)
  {
    free(sgip->length[0]);
    free(sgip->length);
  }

  if (sgip->comp == SGI_COMP_ARLE)
    free(sgip->arle_row);

  i = fclose(sgip->file);
  free(sgip);

  return (i);
}


/*
 * 'sgiGetRow()' - Get a row of image data from a file.
 */

int					/* O - 0 on success, -1 on error */
sgiGetRow(sgi_t          *sgip,		/* I - SGI image */
          unsigned short *row,		/* O - Row to read */
          int            y,		/* I - Line to read */
          int            z)		/* I - Channel to read */
{
  int	x;				/* X coordinate */
  long	offset;				/* File offset */


  if (sgip == NULL ||
      row == NULL ||
      y < 0 || y >= sgip->ysize ||
      z < 0 || z >= sgip->zsize)
    return (-1);

  switch (sgip->comp)
  {
    case SGI_COMP_NONE :
       /*
        * Seek to the image row - optimize buffering by only seeking if
        * necessary...
        */

        offset = 512 + (y + z * sgip->ysize) * sgip->xsize * sgip->bpp;
        if (offset != ftell(sgip->file))
          fseek(sgip->file, offset, SEEK_SET);

        if (sgip->bpp == 1)
        {
          for (x = sgip->xsize; x > 0; x --, row ++)
            *row = getc(sgip->file);
        }
        else
        {
          for (x = sgip->xsize; x > 0; x --, row ++)
            *row = getshort(sgip->file);
        }
        break;

    case SGI_COMP_RLE :
        offset = sgip->table[z][y];
        if (offset != ftell(sgip->file))
          fseek(sgip->file, offset, SEEK_SET);

        if (sgip->bpp == 1)
          return (read_rle8(sgip->file, row, sgip->xsize));
        else
          return (read_rle16(sgip->file, row, sgip->xsize));
  }

  return (0);
}


/*
 * 'sgiOpen()' - Open an SGI image file for reading or writing.
 */

sgi_t *					/* O - New image */
sgiOpen(const char *filename,		/* I - File to open */
        int        mode,		/* I - Open mode (SGI_READ or SGI_WRITE) */
        int        comp,		/* I - Type of compression */
        int        bpp,			/* I - Bytes per pixel */
        int        xsize,		/* I - Width of image in pixels */
        int        ysize,		/* I - Height of image in pixels */
        int        zsize)		/* I - Number of channels */
{
  sgi_t	*sgip;				/* New SGI image file */
  FILE	*file;				/* Image file pointer */


  if (mode == SGI_READ)
    file = fopen(filename, "rb");
  else
    file = fopen(filename, "wb+");

  if (file == NULL)
    return (NULL);

  if ((sgip = sgiOpenFile(file, mode, comp, bpp, xsize, ysize, zsize)) == NULL)
    fclose(file);

  return (sgip);
}


/*
 * 'sgiOpenFile()' - Open an SGI image file for reading or writing.
 */

sgi_t *					/* O - New image */
sgiOpenFile(FILE *file,			/* I - File to open */
            int  mode,			/* I - Open mode (SGI_READ or SGI_WRITE) */
            int  comp,			/* I - Type of compression */
            int  bpp,			/* I - Bytes per pixel */
            int  xsize,			/* I - Width of image in pixels */
            int  ysize,			/* I - Height of image in pixels */
            int  zsize)			/* I - Number of channels */
{
  int	i, j;				/* Looping var */
  char	name[80];			/* Name of file in image header */
  short	magic;				/* Magic number */
  sgi_t	*sgip;				/* New image pointer */


  if ((sgip = calloc(sizeof(sgi_t), 1)) == NULL)
    return (NULL);

  sgip->file = file;

  switch (mode)
  {
    case SGI_READ :
        sgip->mode = SGI_READ;

        magic = getshort(sgip->file);
        if (magic != SGI_MAGIC)
        {
          free(sgip);
          return (NULL);
        }

        sgip->comp  = getc(sgip->file);
        sgip->bpp   = getc(sgip->file);
        getshort(sgip->file);		/* Dimensions */
        sgip->xsize = getshort(sgip->file);
        sgip->ysize = getshort(sgip->file);
        sgip->zsize = getshort(sgip->file);
        getlong(sgip->file);		/* Minimum pixel */
        getlong(sgip->file);		/* Maximum pixel */

        if (sgip->comp)
        {
         /*
          * This file is compressed; read the scanline tables...
          */

          fseek(sgip->file, 512, SEEK_SET);

          if ((sgip->table = calloc(sgip->zsize, sizeof(long *))) == NULL)
	  {
	    free(sgip);
	    return (NULL);
	  }

          if ((sgip->table[0] = calloc(sgip->ysize * sgip->zsize,
	                               sizeof(long))) == NULL)
          {
	    free(sgip->table);
	    free(sgip);
	    return (NULL);
	  }

          for (i = 1; i < sgip->zsize; i ++)
            sgip->table[i] = sgip->table[0] + i * sgip->ysize;

          for (i = 0; i < sgip->zsize; i ++)
            for (j = 0; j < sgip->ysize; j ++)
              sgip->table[i][j] = getlong(sgip->file);
        }
        break;

    case SGI_WRITE :
	if (xsize < 1 ||
	    ysize < 1 ||
	    zsize < 1 ||
	    bpp < 1 || bpp > 2 ||
	    comp < SGI_COMP_NONE || comp > SGI_COMP_ARLE)
        {
          free(sgip);
          return (NULL);
        }

        sgip->mode = SGI_WRITE;

        putshort(SGI_MAGIC, sgip->file);
        putc((sgip->comp = comp) != 0, sgip->file);
        putc(sgip->bpp = bpp, sgip->file);
        putshort(3, sgip->file);		/* Dimensions */
        putshort(sgip->xsize = xsize, sgip->file);
        putshort(sgip->ysize = ysize, sgip->file);
        putshort(sgip->zsize = zsize, sgip->file);
        if (bpp == 1)
        {
          putlong(0, sgip->file);	/* Minimum pixel */
          putlong(255, sgip->file);	/* Maximum pixel */
        }
        else
        {
          putlong(-32768, sgip->file);	/* Minimum pixel */
          putlong(32767, sgip->file);	/* Maximum pixel */
        }
        putlong(0, sgip->file);		/* Reserved */

        memset(name, 0, sizeof(name));
        fwrite(name, sizeof(name), 1, sgip->file);

        for (i = 0; i < 102; i ++)
          putlong(0, sgip->file);

        switch (comp)
        {
          case SGI_COMP_NONE : /* No compression */
             /*
              * This file is uncompressed.  To avoid problems with sparse files,
              * we need to write blank pixels for the entire image...
              */

              if (bpp == 1)
              {
        	for (i = xsize * ysize * zsize; i > 0; i --)
        	  putc(0, sgip->file);
              }
              else
              {
        	for (i = xsize * ysize * zsize; i > 0; i --)
        	  putshort(0, sgip->file);
              }
              break;

          case SGI_COMP_ARLE : /* Aggressive RLE */
              sgip->arle_row    = calloc(xsize, sizeof(unsigned short));
              sgip->arle_offset = 0;

          case SGI_COMP_RLE : /* Run-Length Encoding */
             /*
              * This file is compressed; write the (blank) scanline tables...
              */

              for (i = 2 * ysize * zsize; i > 0; i --)
        	putlong(0, sgip->file);

              sgip->firstrow = ftell(sgip->file);
              sgip->nextrow  = ftell(sgip->file);
              if ((sgip->table = calloc(sgip->zsize, sizeof(long *))) == NULL)
	      {
	        free(sgip);
		return (NULL);
	      }

              if ((sgip->table[0] = calloc(sgip->ysize * sgip->zsize,
	                                   sizeof(long))) == NULL)
              {
	        free(sgip->table);
		free(sgip);
		return (NULL);
	      }

              for (i = 1; i < sgip->zsize; i ++)
        	sgip->table[i] = sgip->table[0] + i * sgip->ysize;

              if ((sgip->length = calloc(sgip->zsize, sizeof(long *))) == NULL)
	      {
	        free(sgip->table);
		free(sgip);
		return (NULL);
	      }

              if ((sgip->length[0] = calloc(sgip->ysize * sgip->zsize,
	                                    sizeof(long))) == NULL)
              {
	        free(sgip->length);
		free(sgip->table);
		free(sgip);
		return (NULL);
	      }

              for (i = 1; i < sgip->zsize; i ++)
        	sgip->length[i] = sgip->length[0] + i * sgip->ysize;
              break;
        }
        break;

    default :
        free(sgip);
        return (NULL);
  }

  return (sgip);
}


/*
 * 'sgiPutRow()' - Put a row of image data to a file.
 */

int					/* O - 0 on success, -1 on error */
sgiPutRow(sgi_t          *sgip,		/* I - SGI image */
          unsigned short *row,		/* I - Row to write */
          int            y,		/* I - Line to write */
          int            z)		/* I - Channel to write */
{
  int	x;				/* X coordinate */
  long	offset;				/* File offset */


  if (sgip == NULL ||
      row == NULL ||
      y < 0 || y >= sgip->ysize ||
      z < 0 || z >= sgip->zsize)
    return (-1);

  switch (sgip->comp)
  {
    case SGI_COMP_NONE :
       /*
        * Seek to the image row - optimize buffering by only seeking if
        * necessary...
        */

        offset = 512 + (y + z * sgip->ysize) * sgip->xsize * sgip->bpp;
        if (offset != ftell(sgip->file))
          fseek(sgip->file, offset, SEEK_SET);

        if (sgip->bpp == 1)
        {
          for (x = sgip->xsize; x > 0; x --, row ++)
            putc(*row, sgip->file);
        }
        else
        {
          for (x = sgip->xsize; x > 0; x --, row ++)
            putshort(*row, sgip->file);
        }
        break;

    case SGI_COMP_ARLE :
        if (sgip->table[z][y] != 0)
          return (-1);

       /*
        * First check the last row written...
        */

        if (sgip->arle_offset > 0)
        {
          for (x = 0; x < sgip->xsize; x ++)
            if (row[x] != sgip->arle_row[x])
              break;

          if (x == sgip->xsize)
          {
            sgip->table[z][y]  = sgip->arle_offset;
            sgip->length[z][y] = sgip->arle_length;
            return (0);
          }
        }

       /*
        * If that didn't match, search all the previous rows...
        */

        fseek(sgip->file, sgip->firstrow, SEEK_SET);

        if (sgip->bpp == 1)
        {
          for (;;)
          {
            sgip->arle_offset = ftell(sgip->file);
            if ((sgip->arle_length = read_rle8(sgip->file, sgip->arle_row, sgip->xsize)) < 0)
            {
              x = 0;
              break;
            }

            if (memcmp(row, sgip->arle_row, sgip->xsize * sizeof(unsigned short)) == 0)
	    {
	      x = sgip->xsize;
	      break;
	    }
          }
        }
        else
        {
          for (;;)
          {
            sgip->arle_offset = ftell(sgip->file);
            if ((sgip->arle_length = read_rle16(sgip->file, sgip->arle_row, sgip->xsize)) < 0)
            {
              x = 0;
              break;
            }

            if (memcmp(row, sgip->arle_row, sgip->xsize * sizeof(unsigned short)) == 0)
	    {
	      x = sgip->xsize;
	      break;
	    }
          }
        }

	if (x == sgip->xsize)
	{
          sgip->table[z][y]  = sgip->arle_offset;
          sgip->length[z][y] = sgip->arle_length;
          return (0);
	}
	else
	  fseek(sgip->file, 0, SEEK_END);	/* Clear EOF */

    case SGI_COMP_RLE :
        if (sgip->table[z][y] != 0)
          return (-1);

        offset = sgip->table[z][y] = sgip->nextrow;

        if (offset != ftell(sgip->file))
          fseek(sgip->file, offset, SEEK_SET);

        if (sgip->bpp == 1)
          x = write_rle8(sgip->file, row, sgip->xsize);
        else
          x = write_rle16(sgip->file, row, sgip->xsize);

        if (sgip->comp == SGI_COMP_ARLE)
        {
          sgip->arle_offset = offset;
          sgip->arle_length = x;
          memcpy(sgip->arle_row, row, sgip->xsize * sizeof(unsigned short));
        }

        sgip->nextrow      = ftell(sgip->file);
        sgip->length[z][y] = x;

        return (x);
  }

  return (0);
}


/*
 * 'getlong()' - Get a 32-bit big-endian integer.
 */

static int				/* O - Long value */
getlong(FILE *fp)			/* I - File to read from */
{
  unsigned char	b[4];			/* Bytes from file */


  fread(b, 4, 1, fp);
  return ((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}


/*
 * 'getshort()' - Get a 16-bit big-endian integer.
 */

static int				/* O - Short value */
getshort(FILE *fp)			/* I - File to read from */
{
  unsigned char	b[2];			/* Bytes from file */


  fread(b, 2, 1, fp);
  return ((b[0] << 8) | b[1]);
}


/*
 * 'putlong()' - Put a 32-bit big-endian integer.
 */

static int				/* O - 0 on success, -1 on error */
putlong(long n,				/* I - Long to write */
        FILE *fp)			/* I - File to write to */
{
  if (putc(n >> 24, fp) == EOF)
    return (EOF);
  if (putc(n >> 16, fp) == EOF)
    return (EOF);
  if (putc(n >> 8, fp) == EOF)
    return (EOF);
  if (putc(n, fp) == EOF)
    return (EOF);
  else
    return (0);
}


/*
 * 'putshort()' - Put a 16-bit big-endian integer.
 */

static int				/* O - 0 on success, -1 on error */
putshort(unsigned short n,		/* I - Short to write */
         FILE           *fp)		/* I - File to write to */
{
  if (putc(n >> 8, fp) == EOF)
    return (EOF);
  if (putc(n, fp) == EOF)
    return (EOF);
  else
    return (0);
}


/*
 * 'read_rle8()' - Read 8-bit RLE data.
 */

static int				/* O - Value on success, -1 on error */
read_rle8(FILE           *fp,		/* I - File to read from */
          unsigned short *row,		/* O - Data */
          int            xsize)		/* I - Width of data in pixels */
{
  int	i,				/* Looping var */
	ch,				/* Current character */
	count,				/* RLE count */
	length;				/* Number of bytes read... */


  length = 0;

  while (xsize > 0)
  {
    if ((ch = getc(fp)) == EOF)
      return (-1);
    length ++;

    count = ch & 127;
    if (count == 0)
      break;

    if (ch & 128)
    {
      for (i = 0; i < count; i ++, row ++, xsize --, length ++)
        if (xsize > 0)
	  *row = getc(fp);
    }
    else
    {
      ch = getc(fp);
      length ++;
      for (i = 0; i < count && xsize > 0; i ++, row ++, xsize --)
        *row = ch;
    }
  }

  return (xsize > 0 ? -1 : length);
}


/*
 * 'read_rle16()' - Read 16-bit RLE data.
 */

static int				/* O - Value on success, -1 on error */
read_rle16(FILE           *fp,		/* I - File to read from */
           unsigned short *row,		/* O - Data */
           int            xsize)	/* I - Width of data in pixels */
{
  int	i,				/* Looping var */
	ch,				/* Current character */
	count,				/* RLE count */
	length;				/* Number of bytes read... */


  length = 0;

  while (xsize > 0)
  {
    if ((ch = getshort(fp)) == EOF)
      return (-1);
    length ++;

    count = ch & 127;
    if (count == 0)
      break;

    if (ch & 128)
    {
      for (i = 0; i < count; i ++, row ++, xsize --, length ++)
        if (xsize > 0)
	  *row = getshort(fp);
    }
    else
    {
      ch = getshort(fp);
      length ++;
      for (i = 0; i < count && xsize > 0; i ++, row ++, xsize --)
	*row = ch;
    }
  }

  return (xsize > 0 ? -1 : length * 2);
}


/*
 * 'write_rle8()' - Write 8-bit RLE data.
 */

static int				/* O - Length on success, -1 on error */
write_rle8(FILE           *fp,		/* I - File to write to */
           unsigned short *row,		/* I - Data */
           int            xsize)	/* I - Width of data in pixels */
{
  int			length,		/* Length in bytes */
			count,		/* Number of repeating pixels */
			i,		/* Looping var */
			x;		/* Current column */
  unsigned short	*start,		/* Start of current sequence */
			repeat;		/* Repeated pixel */


  for (x = xsize, length = 0; x > 0;)
  {
    start = row;
    row   += 2;
    x     -= 2;

    while (x > 0 && (row[-2] != row[-1] || row[-1] != row[0]))
    {
      row ++;
      x --;
    }

    row -= 2;
    x   += 2;

    count = row - start;
    while (count > 0)
    {
      i     = count > 126 ? 126 : count;
      count -= i;

      if (putc(128 | i, fp) == EOF)
        return (-1);
      length ++;

      while (i > 0)
      {
	if (putc(*start, fp) == EOF)
          return (-1);
        start ++;
        i --;
        length ++;
      }
    }

    if (x <= 0)
      break;

    start  = row;
    repeat = row[0];

    row ++;
    x --;

    while (x > 0 && *row == repeat)
    {
      row ++;
      x --;
    }

    count = row - start;
    while (count > 0)
    {
      i     = count > 126 ? 126 : count;
      count -= i;

      if (putc(i, fp) == EOF)
        return (-1);
      length ++;

      if (putc(repeat, fp) == EOF)
        return (-1);
      length ++;
    }
  }

  length ++;

  if (putc(0, fp) == EOF)
    return (-1);
  else
    return (length);
}


/*
 * 'write_rle16()' - Write 16-bit RLE data.
 */

static int				/* O - Length in words */
write_rle16(FILE           *fp,		/* I - File to write to */
            unsigned short *row,	/* I - Data */
            int            xsize)	/* I - Width of data in pixels */
{
  int			length,		/* Length in words */
			count,		/* Number of repeating pixels */
			i,		/* Looping var */
			x;		/* Current column */
  unsigned short	*start,		/* Start of current sequence */
			repeat;		/* Repeated pixel */


  for (x = xsize, length = 0; x > 0;)
  {
    start = row;
    row   += 2;
    x     -= 2;

    while (x > 0 && (row[-2] != row[-1] || row[-1] != row[0]))
    {
      row ++;
      x --;
    }

    row -= 2;
    x   += 2;

    count = row - start;
    while (count > 0)
    {
      i     = count > 126 ? 126 : count;
      count -= i;

      if (putshort(128 | i, fp) == EOF)
        return (-1);
      length ++;

      while (i > 0)
      {
	if (putshort(*start, fp) == EOF)
          return (-1);
        start ++;
        i --;
        length ++;
      }
    }

    if (x <= 0)
      break;

    start  = row;
    repeat = row[0];

    row ++;
    x --;

    while (x > 0 && *row == repeat)
    {
      row ++;
      x --;
    }

    count = row - start;
    while (count > 0)
    {
      i     = count > 126 ? 126 : count;
      count -= i;

      if (putshort(i, fp) == EOF)
        return (-1);
      length ++;

      if (putshort(repeat, fp) == EOF)
        return (-1);
      length ++;
    }
  }

  length ++;

  if (putshort(0, fp) == EOF)
    return (-1);
  else
    return (2 * length);
}


/*
 * End of "$Id: image-sgilib.c 7221 2008-01-16 22:20:08Z mike $".
 */
