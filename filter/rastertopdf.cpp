/*
 * Raster filter to pdf
 *
 * Copyright © 2022 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 *
 * see <https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf> for complete spec
 * see <https://zlib.net/manual.html> for zlib documentation
 *
 * To build with zlib:
 *    g++ -c -g -Os -o rastertopdf.o rastertopdf.cpp
 *    cc -o rasterToPDF rastertopdf.o -lz -lstdc++ `cups-config --libs`
 *
 * To build without zlib: This will produce very large pdf files.
 *    g++ -DDeflateData=0 -c -g -Os -o rastertopdf.o rastertopdf.cpp
 *    cc -o rasterToPDF rastertopdf.o -lstdc++ `cups-config --libs`
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/backend.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <vector>


#ifndef __has_include
    static_assert(false, "__has_include not supported");
#endif

#if ( !defined(DeflateData) )
    #if __has_include(<zlib.h>)
        #define DeflateData 1
        #include <zlib.h>
    #else
        #define DeflateData 0
    #endif
#elif ( DeflateData )
    #if __has_include(<zlib.h>)
        #include <zlib.h>
    #else
        #warning 'zlib.h' does not exits.
        #undef DeflateData
        #define DeflateData 0
    #endif
#endif

static int Canceled = 0;        /* Has the current job been canceled? */

// MARK: - Misc  -
static int rasterToPDFColorSpace( cups_cspace_t colorSpace, int bitsPerPixel, int *bitsPerComponent, char *cs, size_t csLen )
{
    switch (colorSpace)
    {
        case CUPS_CSPACE_W:
        case CUPS_CSPACE_SW:
            *bitsPerComponent = bitsPerPixel;
            strncpy( cs, "[/CalGray << /Gamma 2.2 /WhitePoint[ 0.9505 1.0 1.089 ] >>]", csLen );
        break;

        case CUPS_CSPACE_RGB:
        case CUPS_CSPACE_SRGB:
            *bitsPerComponent = bitsPerPixel/3;
            strncpy( cs, "[/CalRGB <<\n"
                         "   /Gamma[ 2.2 2.2 2.2 ]\n"
                         "   /Matrix[ 0.4124 0.2126 0.0193\n"
                         "            0.3576 0.7152 0.1192\n"
                         "            0.1805 0.0722 0.9505 ]\n"
                         "   /WhitePoint[ 0.9505 1.0 1.089 ]\n"
                         ">>]", csLen );
        break;

        default :
            // AirPrint only requires sRGB and 2.2 gray.
            // NOTE: This is not a general solution.
            fprintf(stderr, "DEBUG: Unsupported colorspace %u.\n", colorSpace);
            return -1;
    }
    return 0;
}

static void compressImageData(const unsigned char *inData,
                              size_t inSize,
                              unsigned char **outData,
                              size_t *outSize )
{
    if (outData == NULL || outSize == NULL)
    {
        fprintf(stderr, "Invalid Parameters, Line:%d\n", __LINE__);
        exit( EXIT_FAILURE );
    }
#if DeflateData
    int err = ENOMEM;
    
    *outSize = compressBound( (uLongf)inSize );
    *outData = (unsigned char *)malloc( *outSize );
    
    if (*outData != NULL) err = compress( *outData, outSize, inData, inSize );
    
    if (err != 0)
    {
        fprintf( stderr, "Failed to %s data, Line:%d\n", (*outData ? "compress" : "allocate"), __LINE__);

        if (*outData) free( *outData );

        *outData = (unsigned char *)inData;
        *outSize = inSize;
    }
#else
    *outData = (unsigned char *)inData;
    *outSize = inSize;
#endif
}

// MARK: - PDF Stuff -
static long writeImageObject(FILE *pdfFile,
                             unsigned int imageReference,
                             unsigned int width,
                             unsigned int height,
                             int interpolate,
                             int bitsPerComponent,
                             char colorspace[64],
                             const unsigned char *rasterData,
                             size_t rasterDataSize )
{
    unsigned char *data = NULL;
    size_t size;
    long objectOffset = 0;
    
    compressImageData( rasterData, rasterDataSize, &data, &size );

    fprintf(pdfFile, "\n%u 0 obj\n", imageReference );
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /Type /XObject\n"
                     "   /Subtype /Image\n"
                     "   /Width %u\n"
                     "   /Height %u\n"
                     "   /Interpolate %s\n"
                     "   /ColorSpace %s\n"
                     "   /BitsPerComponent %d\n"
                     "   /Length %zu\n", width, height, (interpolate ? "true" : "false"), colorspace, bitsPerComponent, rasterDataSize );

    if (rasterData != data)
        fprintf(pdfFile, "   /Filter /FlateDecode\n");

    fprintf(pdfFile, ">>\nstream\n" );
    fwrite( data, size, 1, pdfFile );
    fprintf(pdfFile, "\nendstream"
                     "\nendobj\n");

    // free the data the was allocated in compressImageData
    if (rasterData != data) free( data );

    return objectOffset;
}

static long writePageStream(FILE *pdfFile,
                            unsigned int streamReference,
                            int width,
                            int height,
                            int pageNumber)
{
    long objectOffset = 0;
    char imageStream[64];
    
    snprintf( imageStream, sizeof( imageStream ), "q %d 0 0 %d 0 0 cm /Im%u Do Q", width, height, pageNumber );
    
    fprintf(pdfFile, "\n%u 0 obj\n", streamReference );
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /Length %zu >>\n"
                     "stream\n"
                     "%s"
                     "\nendstream"
                     "\nendobj\n", strlen(imageStream), imageStream );

    return objectOffset;
}

static long writePageObject(FILE *pdfFile,
                            unsigned int pageReference,
                            unsigned int resouceReference,
                            unsigned int contentReference,
                            int width,
                            int height)
{
    long objectOffset = 0;

    fprintf(pdfFile, "\n%u 0 obj\n", pageReference );
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /Type /Page\n"
                     "   /Parent 2 0 R\n"
                     "   /Resources %u 0 R\n"
                     "   /Contents %u 0 R\n"
                     "   /MediaBox [0 0 %d %d]\n"
                     ">>\nendobj\n", resouceReference, contentReference, width, height );

    return objectOffset;
}

static long writeResourceObject(FILE *pdfFile,
                                unsigned int rsrcReference,
                                unsigned int contentReference,
                                unsigned int page )
{
    long objectOffset = 0;

    fprintf(pdfFile, "\n%u 0 obj\n", rsrcReference );
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /ProcSet [ /PDF /ImageB /ImageC /ImageI ] /XObject << /Im%u %u 0 R >> >>\nendobj\n", page, contentReference );

    return objectOffset;
}

static long writePagesObject( FILE *pdfFile, std::vector<unsigned int> pages )
{
    long objectOffset = 0;

    fprintf(pdfFile, "\n2 0 obj\n");
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /Type /Pages /Count %lu /Kids [", (unsigned long)pages.size());
    for (unsigned int i : pages )
    {
        fprintf( pdfFile, " %d 0 R", i );
    }
    fprintf(pdfFile, " ] >>\nendobj\n");

    return objectOffset;
}

static long writeCatalogObject( FILE *pdfFile, unsigned int objectReference )
{
    long objectOffset = 0;

    fprintf(pdfFile, "\n%u 0 obj\n", objectReference);
    objectOffset = ftell(pdfFile);
    fprintf(pdfFile, "<< /Type /Catalog /Pages 2 0 R >>\n");
    fprintf(pdfFile, "endobj\n");

    return objectOffset;
}

static void writeTrailerObject(FILE *pdfFile,
                               unsigned int catalogReference,
                               unsigned long numObjects,
                               long startXOffset)
{
    fprintf( pdfFile, "trailer\n"
                       "<< /Root %u 0 R\n"
                       "   /Size %lu >>\n"
                       "startxref\n"
                       "%ld\n"
                       "%%%%EOF\n", catalogReference, numObjects, startXOffset);
}

static long writeXRefTable( FILE *pdfFile, std::vector<long> offsets, long startOffset )
{
    long objectOffset = ftell(pdfFile);
    fprintf( pdfFile, "xref\n"
                      "0 %lu\n"
                      "0000000000 65535 f\n", (unsigned long)(offsets.size() + 1) );
    for (long offset : offsets )
    {
        fprintf( pdfFile, "%010ld 00000 n\n", offset - startOffset );
    }
    return objectOffset;
}

static long writeHeader( FILE *pdfFile )
{
    fprintf(pdfFile, "%%PDF-1.3\n");

    return ftell(pdfFile);
}

// MARK: - Work -
static int convertCUPSRasterToPDF( int rasterIn )
{
    #define kInitialImageReferenceID 10
    int err = 0;
    int pages = 0;
    unsigned int objectReference = kInitialImageReferenceID;
    unsigned int catalogReference = objectReference++;
    
    long startOffset;
    long offset;

    float width = 0;
    float height = 0;

    size_t largestAllocatedMemory = 0;
    unsigned char *rasterData = NULL;

    std::vector<unsigned int> pageReferences;
    std::vector<long> objectOffsets;
    cups_raster_t *rasterFile = NULL;
    cups_page_header2_t pageHeader;

    FILE *pdfFile = stdout;

    rasterFile = cupsRasterOpen(rasterIn, CUPS_RASTER_READ);
    if (rasterFile == NULL)
    {
        err = errno;
        fprintf(stderr, "ERROR: Error reading raster data.\n");
        perror("DEBUG: cupsRasterOpen failed to open the file");
        goto bail;
    }

    startOffset = writeHeader( pdfFile );
    while ( !Canceled && cupsRasterReadHeader2(rasterFile, &pageHeader) )
    {
        char colorspace[256];
        int bitsPerComponent = 8;

        fprintf(stderr, "PAGE: %d %d\n", pages+1, pageHeader.NumCopies);
        fprintf(stderr, "DEBUG:%04d] pageHeader.colorSpace=%u, .bitsPerPixel=%u, .duplexMode=%u\n",
                pages, pageHeader.cupsColorSpace, pageHeader.cupsBitsPerPixel, pageHeader.Duplex);
        fprintf(stderr, "DEBUG:      pageHeader.width=%u, .height=%u, .resolution=%u x %u\n",
                pageHeader.cupsWidth, pageHeader.cupsHeight, pageHeader.HWResolution[0], pageHeader.HWResolution[1]);

        int status = rasterToPDFColorSpace( pageHeader.cupsColorSpace, pageHeader.cupsBitsPerPixel, &bitsPerComponent, colorspace, sizeof(colorspace) );
        if (status)
        {
            fprintf( stderr, "INFO: Unable to determine a colorspace. skipping this page.\n" );
            continue;
        }

        size_t imageSize = pageHeader.cupsHeight * pageHeader.cupsBytesPerLine;
        if (imageSize > largestAllocatedMemory)
        {
            rasterData = (unsigned char *)(rasterData == NULL ? malloc(imageSize) : realloc(rasterData, imageSize));
            largestAllocatedMemory = imageSize;
        }

        if (rasterData == NULL)
        {
            fprintf(stderr, "ERROR: Unable to allocate memory for page info\n");
            err = -1;
            break;
        }
        
        size_t result = (size_t) cupsRasterReadPixels(rasterFile, rasterData, (unsigned int)imageSize);
        if (result != imageSize)
        {
            err = -2;
            fprintf(stderr, "ERROR: Unable to read print data.\n");
            fprintf(stderr, "DEBUG: cupsRasterReadPixels faild on page:%d (%zu of %zu bytes read)\n", pages+1, result, imageSize );
            break;
        }

        width = 72.0 * pageHeader.cupsWidth / pageHeader.HWResolution[1];
        height = 72.0 * pageHeader.cupsHeight / pageHeader.HWResolution[0];

        unsigned int pageReference  = objectReference++;
        unsigned int rsrcReference  = objectReference++;
        unsigned int streamReference = objectReference++;
        unsigned int imageReference = objectReference++;
        int interpolate = 0;
        
        offset = writePageStream(pdfFile, streamReference, width, height, pages+1 );
        objectOffsets.push_back( offset );

        offset = writePageObject(pdfFile,
                                 pageReference,
                                 rsrcReference,
                                 streamReference,
                                 width,
                                 height);
        objectOffsets.push_back( offset );

        offset = writeResourceObject(pdfFile, rsrcReference, imageReference, pages+1 );
        objectOffsets.push_back( offset );

        offset = writeImageObject(pdfFile,
                                  imageReference,
                                  pageHeader.cupsWidth,
                                  pageHeader.cupsHeight,
                                  interpolate,
                                  bitsPerComponent,
                                  colorspace,
                                  rasterData,
                                  imageSize);
        objectOffsets.push_back( offset );

        pageReferences.push_back( pageReference );
        pages++;
    }

    offset = writePagesObject( pdfFile, pageReferences );
    objectOffsets.push_back( offset );

    offset = writeCatalogObject( pdfFile, catalogReference );
    objectOffsets.push_back( offset );

    offset = writeXRefTable( pdfFile, objectOffsets, startOffset );

    writeTrailerObject( pdfFile, catalogReference,
                       objectOffsets.size() + 1, offset - startOffset );
bail:
    if ( pdfFile != NULL )    fclose( pdfFile );
    if ( rasterFile != NULL ) cupsRasterClose(rasterFile);
    if ( rasterIn != -1 )     close( rasterIn );
    if ( rasterData )         free( rasterData );

    return err;
}

static void sigterm_handler(int sig)
{
  (void)sig;

  Canceled = 1;
}

static void installSignalHandler( void )
{
#ifdef HAVE_SIGSET
    sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
    struct sigaction action;        /* Actions for POSIX signals */
    memset(&action, 0, sizeof(action));
    
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGTERM);
    
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);
#else
    signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */
}

// MARK: -
int main(int argc, const char * argv[])
{
    int err = 0;

    /*
     * Make sure status messages are not buffered...
     */
    setbuf(stderr, NULL);

    /*
     * Check the command-line...
     */
    if (argc < 6 || argc > 7)
    {
        fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
                argv[0]);
        return (CUPS_BACKEND_FAILED);
    }

    /*
     * Register a signal handler to eject the current page if the
     * job is cancelled.
     */
     installSignalHandler();

     int fd = fileno(stdin);
     if (argc == 7)
     {
          if ((fd = open(argv[6], O_RDONLY)) < 0)
          {
               perror("ERROR: Unable to open file");
               return (1);
          }
     }

     err = convertCUPSRasterToPDF( fd );
     return err;
}
