/*
 * "$Id: image.c,v 1.28.2.6 2004/06/29 13:15:09 mike Exp $"
 *
 *   Base image support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2004 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ImageOpen()        - Open an image file and read it into memory.
 *   ImageClose()       - Close an image file.
 *   ImageSetMaxTiles() - Set the maximum number of tiles to cache.
 *   ImageGetCol()      - Get a column of pixels from an image.
 *   ImageGetRow()      - Get a row of pixels from an image.
 *   ImagePutCol()      - Put a column of pixels to an image.
 *   ImagePutRow()      - Put a row of pixels to an image.
 *   get_tile()         - Get a cached tile.
 *   flush_tile()       - Flush the least-recently-used tile in the cache.
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <cups/cups.h>


/*
 * Local functions...
 */

static ib_t	*get_tile(image_t *img, int x, int y);
static void	flush_tile(image_t *img);


/*
 * 'ImageOpen()' - Open an image file and read it into memory.
 */

image_t *			/* O - New image */
ImageOpen(char       *filename,	/* I - Filename of image */
          int        primary,	/* I - Primary colorspace needed */
          int        secondary,	/* I - Secondary colorspace if primary no good */
          int        saturation,/* I - Color saturation level */
          int        hue,	/* I - Color hue adjustment */
          const ib_t *lut)	/* I - RGB gamma/brightness LUT */
{
  FILE		*fp;		/* File pointer */
  unsigned char	header[16],	/* First 16 bytes of file */
		header2[16];	/* Bytes 2048-2064 (PhotoCD) */
  image_t	*img;		/* New image buffer */
  int		status;		/* Status of load... */


  fprintf(stderr, "DEBUG: ImageOpen(\"%s\", %d, %d, %d, %d, %p)\n",
          filename ? filename : "(null)", primary, secondary,
	  saturation, hue, lut);

 /*
  * Range check...
  */

  if (filename == NULL)
  {
    fputs("ERROR: Image filename == NULL!\n", stderr);
    return (NULL);
  }

 /*
  * Figure out the file type...
  */

  if ((fp = fopen(filename, "r")) == NULL)
  {
    perror("ERROR: Unable to open image file");
    return (NULL);
  }

  if (fread(header, 1, sizeof(header), fp) == 0)
  {
    perror("ERROR: Unable to read image file header");

    fclose(fp);
    return (NULL);
  }

  fseek(fp, 2048, SEEK_SET);
  memset(header2, 0, sizeof(header2));
  fread(header2, 1, sizeof(header2), fp);
  fseek(fp, 0, SEEK_SET);

 /*
  * Allocate memory...
  */

  img = calloc(sizeof(image_t), 1);

  if (img == NULL)
  {
    perror("ERROR: Unable to allocate memory for image file");
    fclose(fp);
    return (NULL);
  }

 /*
  * Load the image as appropriate...
  */

  img->max_ics = TILE_MINIMUM;
  img->xppi    = 128;
  img->yppi    = 128;

  if (memcmp(header, "GIF87a", 6) == 0 ||
           memcmp(header, "GIF89a", 6) == 0)
    status = ImageReadGIF(img, fp, primary, secondary, saturation, hue, lut);
  else if (memcmp(header, "BM", 2) == 0)
    status = ImageReadBMP(img, fp, primary, secondary, saturation, hue, lut);
  else if (header[0] == 0x01 && header[1] == 0xda)
    status = ImageReadSGI(img, fp, primary, secondary, saturation, hue, lut);
  else if (header[0] == 0x59 && header[1] == 0xa6 &&
           header[2] == 0x6a && header[3] == 0x95)
    status = ImageReadSunRaster(img, fp, primary, secondary, saturation, hue, lut);
  else if (header[0] == 'P' && header[1] >= '1' && header[1] <= '6')
    status = ImageReadPNM(img, fp, primary, secondary, saturation, hue, lut);
  else if (memcmp(header2, "PCD_IPI", 7) == 0)
    status = ImageReadPhotoCD(img, fp, primary, secondary, saturation, hue, lut);
  else if (memcmp(header + 8, "\000\010", 2) == 0 ||
           memcmp(header + 8, "\000\030", 2) == 0)
    status = ImageReadPIX(img, fp, primary, secondary, saturation, hue, lut);
#if defined(HAVE_LIBPNG) && defined(HAVE_LIBZ)
  else if (memcmp(header, "\211PNG", 4) == 0)
    status = ImageReadPNG(img, fp, primary, secondary, saturation, hue, lut);
#endif /* HAVE_LIBPNG && HAVE_LIBZ */
#ifdef HAVE_LIBJPEG
  else if (memcmp(header, "\377\330\377", 3) == 0 &&	/* Start-of-Image */
	   header[3] >= 0xe0 && header[3] <= 0xef)	/* APPn */
    status = ImageReadJPEG(img, fp, primary, secondary, saturation, hue, lut);
#endif /* HAVE_LIBJPEG */
#ifdef HAVE_LIBTIFF
  else if (memcmp(header, "MM", 2) == 0 ||
           memcmp(header, "II", 2) == 0)
    status = ImageReadTIFF(img, fp, primary, secondary, saturation, hue, lut);
#endif /* HAVE_LIBTIFF */
  else
  {
    fputs("ERROR: Unknown image file format!\n", stderr);
    fclose(fp);
    status = -1;
  }

  if (status)
  {
    free(img);
    return (NULL);
  }
  else
    return (img);
}


/*
 * 'ImageClose()' - Close an image file.
 */

void
ImageClose(image_t *img)	/* I - Image to close */
{
  ic_t	*current,		/* Current cached tile */
	*next;			/* Next cached tile */


 /*
  * Wipe the tile cache file (if any)...
  */

  if (img->cachefile != NULL)
  {
    fprintf(stderr, "DEBUG: Closing and removing swap file \"%s\"...\n",
            img->cachename);

    fclose(img->cachefile);
    unlink(img->cachename);
  }

 /*
  * Free the image cache...
  */

  fputs("DEBUG: Freeing memory...\n", stderr);

  for (current = img->first, next = NULL; current != NULL; current = next)
  {
    fprintf(stderr, "DEBUG: Freeing cache (%p, next = %p)...\n",
            current, next);

    next = current->next;
    free(current);
  }

 /*
  * Free the rest of memory...
  */

  if (img->tiles != NULL)
  {
    fprintf(stderr, "DEBUG: Freeing tiles (%p)...\n", img->tiles[0]);

    free(img->tiles[0]);

    fprintf(stderr, "DEBUG: Freeing tile pointers (%p)...\n", img->tiles);

    free(img->tiles);
  }

  free(img);
}


/*
 * 'ImageSetMaxTiles()' - Set the maximum number of tiles to cache.
 *
 * If the "max_tiles" argument is 0 then the maximum number of tiles is
 * computed from the image size or the RIP_CACHE environment variable.
 */

void
ImageSetMaxTiles(image_t *img,		/* I - Image to set */
                 int     max_tiles)	/* I - Number of tiles to cache */
{
  int	cache_size,			/* Size of tile cache in bytes */
	min_tiles,			/* Minimum number of tiles to cache */
	max_size;			/* Maximum cache size in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


  min_tiles = max(TILE_MINIMUM,
                  1 + max((img->xsize + TILE_SIZE - 1) / TILE_SIZE,
                          (img->ysize + TILE_SIZE - 1) / TILE_SIZE));

  if (max_tiles == 0)
    max_tiles = ((img->xsize + TILE_SIZE - 1) / TILE_SIZE) *
                ((img->ysize + TILE_SIZE - 1) / TILE_SIZE);

  cache_size = max_tiles * TILE_SIZE * TILE_SIZE * ImageGetDepth(img);

  if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
  {
    switch (sscanf(cache_env, "%d%254s", &max_size, cache_units))
    {
      case 0 :
          max_size = 32 * 1024 * 1024;
	  break;
      case 1 :
          max_size *= 4 * TILE_SIZE * TILE_SIZE;
	  break;
      case 2 :
          if (tolower(cache_units[0] & 255) == 'g')
	    max_size *= 1024 * 1024 * 1024;
          else if (tolower(cache_units[0] & 255) == 'm')
	    max_size *= 1024 * 1024;
	  else if (tolower(cache_units[0] & 255) == 'k')
	    max_size *= 1024;
	  else if (tolower(cache_units[0] & 255) == 't')
	    max_size *= 4 * TILE_SIZE * TILE_SIZE;
	  break;
    }
  }
  else
    max_size = 32 * 1024 * 1024;

  if (cache_size > max_size)
    max_tiles = max_size / TILE_SIZE / TILE_SIZE / ImageGetDepth(img);

  if (max_tiles < min_tiles)
    max_tiles = min_tiles;

  img->max_ics = max_tiles;

  fprintf(stderr, "DEBUG: max_ics=%d...\n", img->max_ics);
}


/*
 * 'ImageGetCol()' - Get a column of pixels from an image.
 */

int				/* O - -1 on error, 0 on success */
ImageGetCol(image_t *img,	/* I - Image */
            int     x,		/* I - Column */
            int     y,		/* I - Start row */
            int     height,	/* I - Column height */
            ib_t    *pixels)	/* O - Pixel data */
{
  int		bpp,		/* Bytes per pixel */
		twidth,		/* Tile width */
		count;		/* Number of pixels to get */
  const ib_t	*ib;		/* Pointer into tile */


  if (img == NULL || x < 0 || x >= img->xsize || y >= img->ysize)
    return (-1);

  if (y < 0)
  {
    height += y;
    y = 0;
  }

  if ((y + height) > img->ysize)
    height = img->ysize - y;

  if (height < 1)
    return (-1);

  bpp    = ImageGetDepth(img);
  twidth = bpp * (TILE_SIZE - 1);

  while (height > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    count = TILE_SIZE - (y & (TILE_SIZE - 1));
    if (count > height)
      count = height;

    y      += count;
    height -= count;

    for (; count > 0; count --, ib += twidth)
      switch (bpp)
      {
        case 4 :
            *pixels++ = *ib++;
        case 3 :
            *pixels++ = *ib++;
            *pixels++ = *ib++;
        case 1 :
            *pixels++ = *ib++;
            break;
      }
  }

  return (0);
}


/*
 * 'ImageGetRow()' - Get a row of pixels from an image.
 */

int				/* O - -1 on error, 0 on success */
ImageGetRow(image_t *img,	/* I - Image */
            int     x,		/* I - Start column */
            int     y,		/* I - Row */
            int     width,	/* I - Width of row */
            ib_t    *pixels)	/* O - Pixel data */
{
  int		bpp,		/* Bytes per pixel */
		count;		/* Number of pixels to get */
  const ib_t	*ib;		/* Pointer to pixels */


  if (img == NULL || y < 0 || y >= img->ysize || x >= img->xsize)
    return (-1);

  if (x < 0)
  {
    width += x;
    x = 0;
  }

  if ((x + width) > img->xsize)
    width = img->xsize - x;

  if (width < 1)
    return (-1);

  bpp = img->colorspace < 0 ? -img->colorspace : img->colorspace;

  while (width > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    count = TILE_SIZE - (x & (TILE_SIZE - 1));
    if (count > width)
      count = width;
    memcpy(pixels, ib, count * bpp);
    pixels += count * bpp;
    x      += count;
    width  -= count;
  }

  return (0);
}


/*
 * 'ImagePutCol()' - Put a column of pixels to an image.
 */

int				/* O - -1 on error, 0 on success */
ImagePutCol(image_t    *img,	/* I - Image */
            int        x,	/* I - Column */
            int        y,	/* I - Start row */
            int        height,	/* I - Column height */
            const ib_t *pixels)	/* I - Pixels to put */
{
  int	bpp,			/* Bytes per pixel */
	twidth,			/* Width of tile */
	count;			/* Number of pixels to put */
  int	tilex,			/* Column within tile */
	tiley;			/* Row within tile */
  ib_t	*ib;			/* Pointer to pixels in tile */


  if (img == NULL || x < 0 || x >= img->xsize || y >= img->ysize)
    return (-1);

  if (y < 0)
  {
    height += y;
    y = 0;
  }

  if ((y + height) > img->ysize)
    height = img->ysize - y;

  if (height < 1)
    return (-1);

  bpp    = ImageGetDepth(img);
  twidth = bpp * (TILE_SIZE - 1);
  tilex  = x / TILE_SIZE;
  tiley  = y / TILE_SIZE;

  while (height > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    img->tiles[tiley][tilex].dirty = 1;
    tiley ++;

    count = TILE_SIZE - (y & (TILE_SIZE - 1));
    if (count > height)
      count = height;

    y      += count;
    height -= count;

    for (; count > 0; count --, ib += twidth)
      switch (bpp)
      {
        case 4 :
            *ib++ = *pixels++;
        case 3 :
            *ib++ = *pixels++;
            *ib++ = *pixels++;
        case 1 :
            *ib++ = *pixels++;
            break;
      }
  }

  return (0);
}


/*
 * 'ImagePutRow()' - Put a row of pixels to an image.
 */

int				/* O - -1 on error, 0 on success */
ImagePutRow(image_t    *img,	/* I - Image */
            int        x,	/* I - Start column */
            int        y,	/* I - Row */
            int        width,	/* I - Row width */
            const ib_t *pixels)	/* I - Pixel data */
{
  int	bpp,			/* Bytes per pixel */
	count;			/* Number of pixels to put */
  int	tilex,			/* Column within tile */
	tiley;			/* Row within tile */
  ib_t	*ib;			/* Pointer to pixels in tile */


  if (img == NULL || y < 0 || y >= img->ysize || x >= img->xsize)
    return (-1);

  if (x < 0)
  {
    width += x;
    x = 0;
  }

  if ((x + width) > img->xsize)
    width = img->xsize - x;

  if (width < 1)
    return (-1);

  bpp   = img->colorspace < 0 ? -img->colorspace : img->colorspace;
  tilex = x / TILE_SIZE;
  tiley = y / TILE_SIZE;

  while (width > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    img->tiles[tiley][tilex].dirty = 1;

    count = TILE_SIZE - (x & (TILE_SIZE - 1));
    if (count > width)
      count = width;
    memcpy(ib, pixels, count * bpp);
    pixels += count * bpp;
    x      += count;
    width  -= count;
    tilex  ++;
  }

  return (0);
}


/*
 * 'get_tile()' - Get a cached tile.
 */

static ib_t *		/* O - Pointer to tile or NULL */
get_tile(image_t *img,	/* I - Image */
         int     x,	/* I - Column in image */
         int     y)	/* I - Row in image */
{
  int		bpp,	/* Bytes per pixel */
		tilex,	/* Column within tile */
		tiley,	/* Row within tile */
		xtiles,	/* Number of tiles horizontally */
		ytiles;	/* Number of tiles vertically */
  ic_t		*ic;	/* Cache pointer */
  itile_t	*tile;	/* Tile pointer */


  if (x >= img->xsize || y >= img->ysize)
  {
    fprintf(stderr, "ERROR: Internal image RIP error - %d,%d is outside of %dx%d\n",
            x, y, img->xsize, img->ysize);
    return (NULL);
  }

  if (img->tiles == NULL)
  {
    xtiles = (img->xsize + TILE_SIZE - 1) / TILE_SIZE;
    ytiles = (img->ysize + TILE_SIZE - 1) / TILE_SIZE;

    fprintf(stderr, "DEBUG: Creating tile array (%dx%d)\n", xtiles, ytiles);

    img->tiles = calloc(sizeof(itile_t *), ytiles);
    tile       = calloc(sizeof(itile_t), xtiles * ytiles);

    for (tiley = 0; tiley < ytiles; tiley ++)
    {
      img->tiles[tiley] = tile;
      for (tilex = xtiles; tilex > 0; tilex --, tile ++)
        tile->pos = -1;
    }
  }

  bpp   = ImageGetDepth(img);
  tilex = x / TILE_SIZE;
  tiley = y / TILE_SIZE;
  x     &= (TILE_SIZE - 1);
  y     &= (TILE_SIZE - 1);

  tile = img->tiles[tiley] + tilex;

  if ((ic = tile->ic) == NULL)
  {
    if (img->num_ics < img->max_ics)
    {
      ic         = calloc(sizeof(ic_t) + bpp * TILE_SIZE * TILE_SIZE, 1);
      ic->pixels = ((ib_t *)ic) + sizeof(ic_t);

      img->num_ics ++;

      fprintf(stderr, "DEBUG: Allocated cache tile %d (%p)...\n",
              img->num_ics, ic);
    }
    else
    {
      fprintf(stderr, "DEBUG: Flushing old cache tile (%p)...\n",
              img->first);

      flush_tile(img);
      ic = img->first;
    }

    ic->tile = tile;
    tile->ic = ic;

    if (tile->pos >= 0)
    {
      fprintf(stderr, "DEBUG: Loading cache tile from file position %ld...\n",
              tile->pos);

      if (ftell(img->cachefile) != tile->pos)
        if (fseek(img->cachefile, tile->pos, SEEK_SET))
	  perror("get_tile:");

      fread(ic->pixels, bpp, TILE_SIZE * TILE_SIZE, img->cachefile);
    }
    else
    {
      fputs("DEBUG: Clearing cache tile...\n", stderr);

      memset(ic->pixels, 0, bpp * TILE_SIZE * TILE_SIZE);
    }
  }

  if (ic == img->first)
  {
    if (ic->next != NULL)
      ic->next->prev = NULL;

    img->first = ic->next;
    ic->next   = NULL;
    ic->prev   = NULL;
  }
  else if (img->first == NULL)
    img->first = ic;

  if (ic != img->last)
  {
   /*
    * Remove the cache entry from the list...
    */

    if (ic->prev != NULL)
      ic->prev->next = ic->next;
    if (ic->next != NULL)
      ic->next->prev = ic->prev;

   /*
    * And add it to the end...
    */

    if (img->last != NULL)
      img->last->next = ic;

    ic->prev  = img->last;
    img->last = ic;
  }

  ic->next = NULL;

  return (ic->pixels + bpp * (y * TILE_SIZE + x));
}


/*
 * 'flush_tile()' - Flush the least-recently-used tile in the cache.
 */

static void
flush_tile(image_t *img)	/* I - Image */
{
  int		fd;		/* Cache file descriptor */
  int		bpp;		/* Bytes per pixel */
  itile_t	*tile;		/* Pointer to tile */


  bpp  = ImageGetDepth(img);
  tile = img->first->tile;

  if (!tile->dirty)
  {
    tile->ic = NULL;
    return;
  }

  if (img->cachefile == NULL)
  {
    if ((fd = cupsTempFd(img->cachename, sizeof(img->cachename))) < 0)
    {
      perror("ERROR: Unable to create image swap file");
      tile->ic    = NULL;
      tile->dirty = 0;
      return;
    }

    fprintf(stderr, "DEBUG: Created swap file \"%s\"...\n", img->cachename);

    if ((img->cachefile = fdopen(fd, "wb+")) == NULL)
    {
      perror("ERROR: Unable to create image swap file");
      close(fd);
      unlink(img->cachename);
      tile->ic    = NULL;
      tile->dirty = 0;
      return;
    }
  }

  if (tile->pos >= 0)
  {
    if (ftell(img->cachefile) != tile->pos)
      if (fseek(img->cachefile, tile->pos, SEEK_SET))
      {
        perror("ERROR: Unable to seek in swap file");
	tile->ic    = NULL;
	tile->dirty = 0;
	return;
      }
  }
  else
  {
    if (fseek(img->cachefile, 0, SEEK_END))
    {
      perror("ERROR: Unable to append to swap file");
      tile->ic    = NULL;
      tile->dirty = 0;
      return;
    }

    tile->pos = ftell(img->cachefile);
  }


  if (fwrite(tile->ic->pixels, bpp, TILE_SIZE * TILE_SIZE, img->cachefile) < 1)
    perror("ERROR: Unable to write tile to swap file");
  else
    fprintf(stderr, "DEBUG: Wrote tile at position %ld...\n", tile->pos);

  tile->ic    = NULL;
  tile->dirty = 0;
}


/*
 * End of "$Id: image.c,v 1.28.2.6 2004/06/29 13:15:09 mike Exp $".
 */
