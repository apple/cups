/*
 * "$Id: image.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Base image support for the Common UNIX Printing System (CUPS).
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
 *   cupsImageClose()         - Close an image file.
 *   cupsImageGetCol()        - Get a column of pixels from an image.
 *   cupsImageGetColorSpace() - Get the image colorspace.
 *   cupsImageGetDepth()      - Get the number of bytes per pixel.
 *   cupsImageGetHeight()     - Get the height of an image.
 *   cupsImageGetRow()        - Get a row of pixels from an image.
 *   cupsImageGetWidth()      - Get the width of an image.
 *   cupsImageGetXPPI()       - Get the horizontal resolution of an image.
 *   cupsImageGetYPPI()       - Get the vertical resolution of an image.
 *   cupsImageOpen()          - Open an image file and read it into memory.
 *   _cupsImagePutCol()       - Put a column of pixels to an image.
 *   _cupsImagePutRow()       - Put a row of pixels to an image.
 *   cupsImageSetMaxTiles()   - Set the maximum number of tiles to cache.
 *   flush_tile()             - Flush the least-recently-used tile in the cache.
 *   get_tile()               - Get a cached tile.
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"


/*
 * Local functions...
 */

static void		flush_tile(cups_image_t *img);
static cups_ib_t	*get_tile(cups_image_t *img, int x, int y);


/*
 * 'cupsImageClose()' - Close an image file.
 */

void
cupsImageClose(cups_image_t *img)	/* I - Image to close */
{
  cups_ic_t	*current,		/* Current cached tile */
		*next;			/* Next cached tile */


 /*
  * Wipe the tile cache file (if any)...
  */

  if (img->cachefile != NULL)
  {
    DEBUG_printf(("Closing/removing swap file \"%s\"...\n", img->cachename));

    fclose(img->cachefile);
    unlink(img->cachename);
  }

 /*
  * Free the image cache...
  */

  DEBUG_puts("Freeing memory...");

  for (current = img->first, next = NULL; current != NULL; current = next)
  {
    DEBUG_printf(("Freeing cache (%p, next = %p)...\n", current, next));

    next = current->next;
    free(current);
  }

 /*
  * Free the rest of memory...
  */

  if (img->tiles != NULL)
  {
    DEBUG_printf(("Freeing tiles (%p)...\n", img->tiles[0]));

    free(img->tiles[0]);

    DEBUG_printf(("Freeing tile pointers (%p)...\n", img->tiles));

    free(img->tiles);
  }

  free(img);
}


/*
 * 'cupsImageGetCol()' - Get a column of pixels from an image.
 */

int					/* O - -1 on error, 0 on success */
cupsImageGetCol(cups_image_t *img,	/* I - Image */
        	int          x,		/* I - Column */
        	int          y,		/* I - Start row */
        	int          height,	/* I - Column height */
        	cups_ib_t    *pixels)	/* O - Pixel data */
{
  int			bpp,		/* Bytes per pixel */
			twidth,		/* Tile width */
			count;		/* Number of pixels to get */
  const cups_ib_t	*ib;		/* Pointer into tile */


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

  bpp    = cupsImageGetDepth(img);
  twidth = bpp * (CUPS_TILE_SIZE - 1);

  while (height > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    count = CUPS_TILE_SIZE - (y & (CUPS_TILE_SIZE - 1));
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
 * 'cupsImageGetColorSpace()' - Get the image colorspace.
 */

cups_icspace_t				/* O - Colorspace */
cupsImageGetColorSpace(
    cups_image_t *img)			/* I - Image */
{
  return (img->colorspace);
}


/*
 * 'cupsImageGetDepth()' - Get the number of bytes per pixel.
 */

int					/* O - Bytes per pixel */
cupsImageGetDepth(cups_image_t *img)	/* I - Image */
{
  return (abs(img->colorspace));
}


/*
 * 'cupsImageGetHeight()' - Get the height of an image.
 */

unsigned				/* O - Height in pixels */
cupsImageGetHeight(cups_image_t *img)	/* I - Image */
{
  return (img->ysize);
}


/*
 * 'cupsImageGetRow()' - Get a row of pixels from an image.
 */

int					/* O - -1 on error, 0 on success */
cupsImageGetRow(cups_image_t *img,	/* I - Image */
                int          x,		/* I - Start column */
                int          y,		/* I - Row */
                int          width,	/* I - Width of row */
                cups_ib_t    *pixels)	/* O - Pixel data */
{
  int			bpp,		/* Bytes per pixel */
			count;		/* Number of pixels to get */
  const cups_ib_t	*ib;		/* Pointer to pixels */


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

    count = CUPS_TILE_SIZE - (x & (CUPS_TILE_SIZE - 1));
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
 * 'cupsImageGetWidth()' - Get the width of an image.
 */

unsigned				/* O - Width in pixels */
cupsImageGetWidth(cups_image_t *img)	/* I - Image */
{
  return (img->xsize);
}


/*
 * 'cupsImageGetXPPI()' - Get the horizontal resolution of an image.
 */

unsigned				/* O - Horizontal PPI */
cupsImageGetXPPI(cups_image_t *img)	/* I - Image */
{
  return (img->xppi);
}


/*
 * 'cupsImageGetYPPI()' - Get the vertical resolution of an image.
 */

unsigned				/* O - Vertical PPI */
cupsImageGetYPPI(cups_image_t *img)	/* I - Image */
{
  return (img->yppi);
}


/*
 * 'cupsImageOpen()' - Open an image file and read it into memory.
 */

cups_image_t *				/* O - New image */
cupsImageOpen(
    const char      *filename,		/* I - Filename of image */
    cups_icspace_t  primary,		/* I - Primary colorspace needed */
    cups_icspace_t  secondary,		/* I - Secondary colorspace if primary no good */
    int             saturation,		/* I - Color saturation level */
    int             hue,		/* I - Color hue adjustment */
    const cups_ib_t *lut)		/* I - RGB gamma/brightness LUT */
{
  FILE		*fp;			/* File pointer */
  unsigned char	header[16],		/* First 16 bytes of file */
		header2[16];		/* Bytes 2048-2064 (PhotoCD) */
  cups_image_t	*img;			/* New image buffer */
  int		status;			/* Status of load... */


  DEBUG_printf(("cupsImageOpen(\"%s\", %d, %d, %d, %d, %p)\n",
        	filename ? filename : "(null)", primary, secondary,
		saturation, hue, lut));

 /*
  * Figure out the file type...
  */

  if ((fp = fopen(filename, "r")) == NULL)
    return (NULL);

  if (fread(header, 1, sizeof(header), fp) == 0)
  {
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

  img = calloc(sizeof(cups_image_t), 1);

  if (img == NULL)
  {
    fclose(fp);
    return (NULL);
  }

 /*
  * Load the image as appropriate...
  */

  img->max_ics = CUPS_TILE_MINIMUM;
  img->xppi    = 128;
  img->yppi    = 128;

  if (!memcmp(header, "GIF87a", 6) || !memcmp(header, "GIF89a", 6))
    status = _cupsImageReadGIF(img, fp, primary, secondary, saturation, hue,
                               lut);
  else if (!memcmp(header, "BM", 2))
    status = _cupsImageReadBMP(img, fp, primary, secondary, saturation, hue,
                               lut);
  else if (header[0] == 0x01 && header[1] == 0xda)
    status = _cupsImageReadSGI(img, fp, primary, secondary, saturation, hue,
                               lut);
  else if (header[0] == 0x59 && header[1] == 0xa6 &&
           header[2] == 0x6a && header[3] == 0x95)
    status = _cupsImageReadSunRaster(img, fp, primary, secondary, saturation,
                                     hue, lut);
  else if (header[0] == 'P' && header[1] >= '1' && header[1] <= '6')
    status = _cupsImageReadPNM(img, fp, primary, secondary, saturation, hue,
                               lut);
  else if (!memcmp(header2, "PCD_IPI", 7))
    status = _cupsImageReadPhotoCD(img, fp, primary, secondary, saturation,
                                   hue, lut);
  else if (!memcmp(header + 8, "\000\010", 2) ||
           !memcmp(header + 8, "\000\030", 2))
    status = _cupsImageReadPIX(img, fp, primary, secondary, saturation, hue,
                               lut);
#if defined(HAVE_LIBPNG) && defined(HAVE_LIBZ)
  else if (!memcmp(header, "\211PNG", 4))
    status = _cupsImageReadPNG(img, fp, primary, secondary, saturation, hue,
                               lut);
#endif /* HAVE_LIBPNG && HAVE_LIBZ */
#ifdef HAVE_LIBJPEG
  else if (!memcmp(header, "\377\330\377", 3) &&	/* Start-of-Image */
	   header[3] >= 0xe0 && header[3] <= 0xef)	/* APPn */
    status = _cupsImageReadJPEG(img, fp, primary, secondary, saturation, hue,
                                lut);
#endif /* HAVE_LIBJPEG */
#ifdef HAVE_LIBTIFF
  else if (!memcmp(header, "MM\000\052", 4) ||
           !memcmp(header, "II\052\000", 4))
    status = _cupsImageReadTIFF(img, fp, primary, secondary, saturation, hue,
                                lut);
#endif /* HAVE_LIBTIFF */
  else
  {
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
 * '_cupsImagePutCol()' - Put a column of pixels to an image.
 */

int					/* O - -1 on error, 0 on success */
_cupsImagePutCol(
    cups_image_t    *img,		/* I - Image */
    int             x,			/* I - Column */
    int             y,			/* I - Start row */
    int             height,		/* I - Column height */
    const cups_ib_t *pixels)		/* I - Pixels to put */
{
  int		bpp,			/* Bytes per pixel */
		twidth,			/* Width of tile */
		count;			/* Number of pixels to put */
  int		tilex,			/* Column within tile */
		tiley;			/* Row within tile */
  cups_ib_t	*ib;			/* Pointer to pixels in tile */


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

  bpp    = cupsImageGetDepth(img);
  twidth = bpp * (CUPS_TILE_SIZE - 1);
  tilex  = x / CUPS_TILE_SIZE;
  tiley  = y / CUPS_TILE_SIZE;

  while (height > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    img->tiles[tiley][tilex].dirty = 1;
    tiley ++;

    count = CUPS_TILE_SIZE - (y & (CUPS_TILE_SIZE - 1));
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
 * '_cupsImagePutRow()' - Put a row of pixels to an image.
 */

int					/* O - -1 on error, 0 on success */
_cupsImagePutRow(
    cups_image_t    *img,		/* I - Image */
    int             x,			/* I - Start column */
    int             y,			/* I - Row */
    int             width,		/* I - Row width */
    const cups_ib_t *pixels)		/* I - Pixel data */
{
  int		bpp,			/* Bytes per pixel */
		count;			/* Number of pixels to put */
  int		tilex,			/* Column within tile */
		tiley;			/* Row within tile */
  cups_ib_t	*ib;			/* Pointer to pixels in tile */


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
  tilex = x / CUPS_TILE_SIZE;
  tiley = y / CUPS_TILE_SIZE;

  while (width > 0)
  {
    ib = get_tile(img, x, y);

    if (ib == NULL)
      return (-1);

    img->tiles[tiley][tilex].dirty = 1;

    count = CUPS_TILE_SIZE - (x & (CUPS_TILE_SIZE - 1));
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
 * 'cupsImageSetMaxTiles()' - Set the maximum number of tiles to cache.
 *
 * If the "max_tiles" argument is 0 then the maximum number of tiles is
 * computed from the image size or the RIP_CACHE environment variable.
 */

void
cupsImageSetMaxTiles(
    cups_image_t *img,			/* I - Image to set */
    int          max_tiles)		/* I - Number of tiles to cache */
{
  int	cache_size,			/* Size of tile cache in bytes */
	min_tiles,			/* Minimum number of tiles to cache */
	max_size;			/* Maximum cache size in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


  min_tiles = max(CUPS_TILE_MINIMUM,
                  1 + max((img->xsize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE,
                          (img->ysize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE));

  if (max_tiles == 0)
    max_tiles = ((img->xsize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE) *
                ((img->ysize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE);

  cache_size = max_tiles * CUPS_TILE_SIZE * CUPS_TILE_SIZE *
               cupsImageGetDepth(img);

  if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
  {
    switch (sscanf(cache_env, "%d%254s", &max_size, cache_units))
    {
      case 0 :
          max_size = 32 * 1024 * 1024;
	  break;
      case 1 :
          max_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
      case 2 :
          if (tolower(cache_units[0] & 255) == 'g')
	    max_size *= 1024 * 1024 * 1024;
          else if (tolower(cache_units[0] & 255) == 'm')
	    max_size *= 1024 * 1024;
	  else if (tolower(cache_units[0] & 255) == 'k')
	    max_size *= 1024;
	  else if (tolower(cache_units[0] & 255) == 't')
	    max_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
    }
  }
  else
    max_size = 32 * 1024 * 1024;

  if (cache_size > max_size)
    max_tiles = max_size / CUPS_TILE_SIZE / CUPS_TILE_SIZE /
                cupsImageGetDepth(img);

  if (max_tiles < min_tiles)
    max_tiles = min_tiles;

  img->max_ics = max_tiles;

  DEBUG_printf(("max_ics=%d...\n", img->max_ics));
}


/*
 * 'flush_tile()' - Flush the least-recently-used tile in the cache.
 */

static void
flush_tile(cups_image_t *img)		/* I - Image */
{
  int		fd;			/* Cache file descriptor */
  int		bpp;			/* Bytes per pixel */
  cups_itile_t	*tile;			/* Pointer to tile */


  bpp  = cupsImageGetDepth(img);
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
/*      perror("ERROR: Unable to create image swap file");
*/      tile->ic    = NULL;
      tile->dirty = 0;
      return;
    }

    DEBUG_printf(("Created swap file \"%s\"...\n", img->cachename));

    if ((img->cachefile = fdopen(fd, "wb+")) == NULL)
    {
/*      perror("ERROR: Unable to create image swap file");
*/      close(fd);
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
/*        perror("ERROR: Unable to seek in swap file");
*/	tile->ic    = NULL;
	tile->dirty = 0;
	return;
      }
  }
  else
  {
    if (fseek(img->cachefile, 0, SEEK_END))
    {
/*      perror("ERROR: Unable to append to swap file");
*/      tile->ic    = NULL;
      tile->dirty = 0;
      return;
    }

    tile->pos = ftell(img->cachefile);
  }


/*  if (fwrite(tile->ic->pixels, bpp, CUPS_TILE_SIZE * CUPS_TILE_SIZE,
             img->cachefile) < 1)
    perror("ERROR: Unable to write tile to swap file");
  else
    DEBUG_printf(("Wrote tile at position %ld...\n", tile->pos));
*/

  fwrite(tile->ic->pixels, bpp, CUPS_TILE_SIZE * CUPS_TILE_SIZE,
         img->cachefile);
  
  tile->ic    = NULL;
  tile->dirty = 0;
}


/*
 * 'get_tile()' - Get a cached tile.
 */

static cups_ib_t *			/* O - Pointer to tile or NULL */
get_tile(cups_image_t *img,		/* I - Image */
         int          x,		/* I - Column in image */
         int          y)		/* I - Row in image */
{
  int		bpp,			/* Bytes per pixel */
		tilex,			/* Column within tile */
		tiley,			/* Row within tile */
		xtiles,			/* Number of tiles horizontally */
		ytiles;			/* Number of tiles vertically */
  cups_ic_t	*ic;			/* Cache pointer */
  cups_itile_t	*tile;			/* Tile pointer */


  if (img->tiles == NULL)
  {
    xtiles = (img->xsize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE;
    ytiles = (img->ysize + CUPS_TILE_SIZE - 1) / CUPS_TILE_SIZE;

    DEBUG_printf(("Creating tile array (%dx%d)\n", xtiles, ytiles));

    if ((img->tiles = calloc(sizeof(cups_itile_t *), ytiles)) == NULL)
      return (NULL);

    if ((tile = calloc(sizeof(cups_itile_t), xtiles * ytiles)) == NULL)
      return (NULL);

    for (tiley = 0; tiley < ytiles; tiley ++)
    {
      img->tiles[tiley] = tile;
      for (tilex = xtiles; tilex > 0; tilex --, tile ++)
        tile->pos = -1;
    }
  }

  bpp   = cupsImageGetDepth(img);
  tilex = x / CUPS_TILE_SIZE;
  tiley = y / CUPS_TILE_SIZE;
  tile  = img->tiles[tiley] + tilex;
  x     &= (CUPS_TILE_SIZE - 1);
  y     &= (CUPS_TILE_SIZE - 1);

  if ((ic = tile->ic) == NULL)
  {
    if (img->num_ics < img->max_ics)
    {
      if ((ic = calloc(sizeof(cups_ic_t) +
                       bpp * CUPS_TILE_SIZE * CUPS_TILE_SIZE, 1)) == NULL)
      {
        if (img->num_ics == 0)
	  return (NULL);

        flush_tile(img);
	ic = img->first;
      }
      else
      {
	ic->pixels = ((cups_ib_t *)ic) + sizeof(cups_ic_t);

	img->num_ics ++;

	DEBUG_printf(("Allocated cache tile %d (%p)...\n", img->num_ics, ic));
      }
    }
    else
    {
      DEBUG_printf(("Flushing old cache tile (%p)...\n", img->first));

      flush_tile(img);
      ic = img->first;
    }

    ic->tile = tile;
    tile->ic = ic;

    if (tile->pos >= 0)
    {
      DEBUG_printf(("Loading cache tile from file position %ld...\n",
                    tile->pos));

      if (ftell(img->cachefile) != tile->pos)
        fseek(img->cachefile, tile->pos, SEEK_SET);
/*        if (fseek(img->cachefile, tile->pos, SEEK_SET))
	  perror("get_tile:");
*/

      fread(ic->pixels, bpp, CUPS_TILE_SIZE * CUPS_TILE_SIZE, img->cachefile);
    }
    else
    {
      DEBUG_puts("Clearing cache tile...");

      memset(ic->pixels, 0, bpp * CUPS_TILE_SIZE * CUPS_TILE_SIZE);
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

  return (ic->pixels + bpp * (y * CUPS_TILE_SIZE + x));
}


/*
 * End of "$Id: image.c 6649 2007-07-11 21:46:42Z mike $".
 */
