/*
 * "$Id: usb-darwin.c,v 1.1.2.3 2004/05/27 15:37:47 mike Exp $"
 *
 *   USB port on Darwin backend for the Common UNIX Printing System (CUPS).
 *
 *   This file is included from "usb.c" when compiled on MacOS X or Darwin.
 *
 *   Copyright 2004 Apple Computer, Inc. All rights reserved.
 *
 *   IMPORTANT:	 This Apple software is supplied to you by Apple Computer,
 *   Inc. ("Apple") in consideration of your agreement to the following
 *   terms, and your use, installation, modification or redistribution of
 *   this Apple software constitutes acceptance of these terms.	 If you do
 *   not agree with these terms, please do not use, install, modify or
 *   redistribute this Apple software.
 *
 *   In consideration of your agreement to abide by the following terms, and
 *   subject to these terms, Apple grants you a personal, non-exclusive
 *   license, under Apple/s copyrights in this original Apple software (the
 *   "Apple Software"), to use, reproduce, modify and redistribute the Apple
 *   Software, with or without modifications, in source and/or binary forms;
 *   provided that if you redistribute the Apple Software in its entirety and
 *   without modifications, you must retain this notice and the following
 *   text and disclaimers in all such redistributions of the Apple Software. 
 *   Neither the name, trademarks, service marks or logos of Apple Computer,
 *   Inc. may be used to endorse or promote products derived from the Apple
 *   Software without specific prior written permission from Apple.  Except
 *   as expressly stated in this notice, no other rights or licenses, express
 *   or implied, are granted by Apple herein, including but not limited to
 *   any patent rights that may be infringed by your derivative works or by
 *   other works in which the Apple Software may be incorporated.
 *
 *   The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 *   MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 *   THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 *   OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 *
 *   IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 *   OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 *   MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 *   AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 *   STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 *
 * Contents:
 *
 *   print_device() - Send a file to the specified USB port.
 *   list_devices() - List all USB devices.
 */

/*
 * Include necessary headers...
 */

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <mach/mach.h>	
#include <mach/mach_error.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>		/* Used for writegReadMutex */

#ifndef kPMPrinterURI
#  define kPMPrinterURI CFSTR("Printer URI")
#endif

/*
 * Panther/10.3 kIOUSBInterfaceInterfaceID190
 * Jaguar/10.2 kIOUSBInterfaceInterfaceID182
 */

#define USB_INTERFACE_KIND  CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID190)
#define kUSBLanguageEnglish 0x409

/*
 * Section 5.3 USB Printing Class spec
 */

#define kUSBPrintingSubclass		1
#define kUSBPrintingProtocolNoOpen	0
#define kUSBPrintingProtocolUnidirectional  1
#define kUSBPrintingProtocolBidirectional   2

#define kUSBPrintClassGetDeviceID	0
#define kUSBPrintClassGetCentronicsStatus   1
#define kUSBPrintClassSoftReset		2

/*
 *  Apple MacOS X printer-class plugins
 */

#define kUSBPrinterClassTypeID		(CFUUIDGetConstantUUIDWithBytes(NULL, 0x06, 0x04, 0x7D, 0x16, 0x53, 0xA2, 0x11, 0xD6, 0x92, 0x06, 0x00, 0x30, 0x65, 0x52, 0x45, 0x92))

#define kUSBPrinterClassInterfaceID	(CFUUIDGetConstantUUIDWithBytes(NULL, 0x03, 0x34, 0x6D, 0x74, 0x53, 0xA3, 0x11, 0xD6, 0x9E, 0xA1, 0x76, 0x30, 0x65, 0x52, 0x45, 0x92))

#define kUSBGenericPrinterClassDriver	    CFSTR("/System/Library/Printers/Libraries/USBGenericPrintingClass.plugin")
#define kUSBGenericTOPrinterClassDriver	    CFSTR("/System/Library/Printers/Libraries/USBGenericTOPrintingClass.plugin")

#define kUSBClassDriverProperty		CFSTR("USB Printing Class")
#define kUSBPrinterClassDeviceNotOpen	    -9664   /*kPMInvalidIOMContext*/

typedef union
{
  char	    b;
  struct
  {
    unsigned	reserved0 : 2;
    unsigned	paperError : 1;
    unsigned	select : 1;
    unsigned	notError : 1;
    unsigned	reserved1 : 3;
  } status;
} CentronicsStatusByte;

typedef struct
{
  CFStringRef	manufacturer;	/* manufacturer name */
  CFStringRef	product;	/* product name */
  CFStringRef	compatible;	/* compatible product name */
  CFStringRef	serial;		/* serial number */
  CFStringRef	command;	/* command set */
  CFStringRef	ppdURL;		/* url of the selected PPD, if any */
} USBPrinterAddress;

typedef IOUSBInterfaceInterface190 **USBPrinterInterface;

typedef struct 
{
  UInt8	    requestType;
  UInt8	    request;
  UInt16    value;
  UInt16    index;
  UInt16    length;
  void	    *buffer;	
} USBIODeviceRequest;

typedef struct classDriverContext
{
  IUNKNOWN_C_GUTS;
  CFPlugInRef	    plugin;	/* release plugin */
  IUnknownVTbl	    **factory;	
  void		*vendorReference;/* vendor class specific usage */
  UInt32	location;   /* unique location in bus topology */
  UInt8		interfaceNumber;
  UInt16	vendorID;
  UInt16	productID;
  USBPrinterInterface	interface;  /* identify the device to IOKit */
  UInt8		outpipe;    /* mandatory bulkOut pipe */
  UInt8		inpipe;	    /* optional bulkIn pipe */
  /*
  **	general class requests
  */
  kern_return_t	    (*DeviceRequest)( struct classDriverContext **printer, USBIODeviceRequest *iorequest, UInt16 timeout );
  kern_return_t (*GetString)( struct classDriverContext **printer, UInt8 whichString, UInt16 language, UInt16 timeout, CFStringRef *result );
  /*
  **	standard printer class requests
  */
  kern_return_t (*SoftReset)( struct classDriverContext **printer, UInt16 timeout );
  kern_return_t (*GetCentronicsStatus)( struct classDriverContext **printer, CentronicsStatusByte *result, UInt16 timeout );
  kern_return_t (*GetDeviceID)( struct classDriverContext **printer, CFStringRef *devid, UInt16 timeout );
  /*
  **	standard bulk device requests
  */
  kern_return_t	    (*ReadPipe)( struct classDriverContext **printer, UInt8 *buffer, UInt32 *count );
  kern_return_t	    (*WritePipe)( struct classDriverContext **printer, UInt8 *buffer, UInt32 *count, Boolean eoj );
  /*
  **	interface requests
  */
  kern_return_t	    (*Open)( struct classDriverContext **printer, UInt32 location, UInt8 protocol );
  kern_return_t	    (*Abort)( struct classDriverContext **printer );
  kern_return_t	    (*Close)( struct classDriverContext **printer );
  /*
  **	initialize and terminate
  */
  kern_return_t	    (*Initialize)( struct classDriverContext **printer, struct classDriverContext **baseclass );
  kern_return_t	    (*Terminate)( struct classDriverContext **printer );
} USBPrinterClassContext;


typedef struct usbPrinterClassType
{
    USBPrinterClassContext  *classdriver;
    CFUUIDRef		    factoryID;
    UInt32		    refCount;
} USBPrinterClassType;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Constants
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*
    Debugging output to Console
    DEBUG undefined or
    DEBUG=0	production code: suppress console output

    DEBUG=1	report errors (non-zero results)
    DEBUG=2	report all results, generate dumps
*/
#if DEBUG==2
#define DEBUG_ERR(c, x)			    showint(x, c)
#define DEBUG_DUMP( text, buf, len )	    dump( text, buf, len )
#define DEBUG_CFString( text, a )	    showcfstring( text, a )
#define DEBUG_CFCompareString( text, a, b ) cmpcfs( text, a, b )
#elif DEBUG==1
#define DEBUG_ERR(c, x)			    if (c) fprintf(stderr, x, c)
#define DEBUG_DUMP( text, buf, len )
#define DEBUG_CFString( text, a )
#define DEBUG_CFCompareString( text, a, b )
#else
#define DEBUG_ERR(c, x)
#define DEBUG_DUMP( text, buf, len )
#define DEBUG_CFString( text, a )
#define DEBUG_CFCompareString( text, a, b )
#endif

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Type Definitions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

typedef struct
{
    /*
     *	Tagged/Tranparent Binary Communications Protocol
     *	TBCP read
     */
    Boolean		    tbcpQuoteReads;	    /* enable tbcp on reads */
    Boolean		    escapeNextRead;	    /* last char of last read buffer was escape */
    UInt8		    *tbcpReadData;	    /* read buffer */
    UInt32		    readLength;		    /* read buffer length (all used) */
    int			    match_endoffset,	    /* partial match of end TBCP sequence */
			    match_startoffset;	    /* partial match of start TBCP sequence */
    /*
     * TBCP write
     */
    UInt8		    *tbcpWriteData;	    /* write buffer */
    UInt32		    tbcpBufferLength,	    /* write buffer allocated length */
			    tbcpBufferRemaining;    /* write buffer not used */

    Boolean		    sendStatusNextWrite;

} PostScriptData;

typedef struct 
{
    CFPlugInRef		    plugin;	    /* valid until plugin is release  */
    USBPrinterClassContext  **classdriver;  /* usb printer class in user space */
    CFStringRef		    bundle;	    /* class driver URI */
    UInt32		    location;	    /* unique location in USB topology */
    USBPrinterAddress	    address;	    /* topology independent bus address */
    CFURLRef		    reference;	    /* internal use */
} USBPrinterInfo;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*
**  IOKit to CF functions
*/
USBPrinterInfo	    *UsbCopyPrinter( USBPrinterInfo *aPrinter );
CFMutableArrayRef   UsbGetAllPrinters( void );
void		    UsbReleasePrinter( USBPrinterInfo *aPrinter );
void		    UsbReleaseAllPrinters( CFMutableArrayRef printers );
kern_return_t	    UsbRegistryOpen( USBPrinterAddress *usbAddress, USBPrinterInfo **result );
kern_return_t	    UsbUnloadClassDriver( USBPrinterInfo *printer );
kern_return_t	    UsbLoadClassDriver( USBPrinterInfo *printer, CFUUIDRef interfaceID, CFStringRef classDriverBundle );
CFStringRef	    UsbMakeFullUriAddress( USBPrinterInfo *aPrinter );

int	    UsbSamePrinter( const USBPrinterAddress *lastTime, const USBPrinterAddress *thisTime ); 

OSStatus    UsbGetPrinterAddress( USBPrinterInfo *thePrinter, USBPrinterAddress *address, UInt16 timeout );


/*******************************************************************************
    Contains:	Support IEEE-1284 DeviceID as a CFString.

    Copyright 2000-2002 by Apple Computer, Inc., all rights reserved.

    Description:
	IEEE-1284 Device ID is referenced in USB and PPDT (1394.3). It allows
	a computer peripheral to convey information about its required software
	to the host system.
	
	DeviceID is defined as a stream of ASCII bytes, commencing with one 16-bit
	binary integer in Little-Endian format which describes how many bytes
	of data are required by the entire DeviceID.
	
	The stream of bytes is further characterized as a series of
	key-value list pairs. In other words each key can be followed by one
	or more values. Multiple key-value list pairs fill out the DeviceID stream.
	
	Some keys are required: COMMAND SET (or CMD), MANUFACTURER (or MFG),
	and MODEL (or MDL).
	
	One needs to read the first two bytes of DeviceID to allocate storage
	for the complete DeviceID string. Then a second read operation can
	retrieve the entire string.
	
	Often DeviceID is not very large. By allocating a reasonable buffer one
	can fetch most device's DeviceID string on the first read.
    
    A more formal definition of DeviceID.

	<DeviceID> = <Length><Key_ValueList_Pair>+

	<Length> = <low byte of 16 bit integer><high byte of 16 bit integer>
	<Key_ValueList_Pair> = <Key>:<Value>[,<Value>]*;

	<Key> = <ASCII Byte>+
	<Value> = <ASCII Byte>+
	
	Some keys are defined in the standard. The standard specifies that
	keys are case sensitive. White space is allowed in the key.
	
	The standard does not say that values are case-sensitive.
	Lexmark is known to ship printers with mixed-case value:
	    i.e., 'CLASS:Printer'

	Required Keys:
	    'COMMAND SET' or CMD
	    MANUFACTURER or MFG
	    MODEL or MDL
	
	Optional Keys:
	    CLASS
		Value PRINTER is referenced in the standard.
		
	Observed Keys:
	    SN,SERN
		Used by Hewlett-Packard for the serial number.
	
	
*******************************************************************************/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Pragmas
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Constants
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define kDeviceIDKeyCommand		CFSTR("COMMAND SET:")
#define kDeviceIDKeyCommandAbbrev	CFSTR( "CMD:" )

#define kDeviceIDKeyManufacturer	CFSTR("MANUFACTURER:")
#define kDeviceIDKeyManufacturerAbbrev	CFSTR( "MFG:" )

#define kDeviceIDKeyModel		CFSTR("MODEL:")
#define kDeviceIDKeyModelAbbrev		CFSTR( "MDL:" )

#define kDeviceIDKeySerial		CFSTR("SN:")
#define kDeviceIDKeySerialAbbrev	CFSTR("SERN:")

#define kDeviceIDKeyCompatible		CFSTR("COMPATIBLITY ID:")
#define kDeviceIDKeyCompatibleAbbrev	CFSTR("CID:")

/* delimiters */
#define kDeviceIDKeyValuePairDelimiter	CFSTR(";")

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Type definitions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Function prototypes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static CFStringRef CreateEncodedCFString(CFStringRef string);
static CFRange	DelimitSubstring( CFStringRef stringToSearch, CFStringRef delim, CFRange bounds, CFStringCompareFlags options );
static void parseOptions(const char *options, char *serial);

CFStringRef
DeviceIDCreateValueList(    const CFStringRef deviceID,
			    const CFStringRef abbrevKey,
			    const CFStringRef key );

static int addPercentEscapes(const unsigned char* src, char* dst, int dstMax);
static int removePercentEscapes(const char* src, unsigned char* dst, int dstMax);

/* Required to suppress redefinition warnings for these two symbols 
*/
#if defined(TCP_NODELAY)
#undef TCP_NODELAY
#endif
#if defined(TCP_MAXSEG)
#undef TCP_MAXSEG
#endif

#include <cups/cups.h>


#define PRINTER_POLLING_INTERVAL	5   /* seconds */
#define INITIAL_LOG_INTERVAL		(PRINTER_POLLING_INTERVAL)
#define SUBSEQUENT_LOG_INTERVAL		(3*INITIAL_LOG_INTERVAL)

/* WAITEOF_DELAY is number of seconds we'll wait for responses from the printer */
/*  after we've finished sending all the data */
#define WAITEOF_DELAY		7

#define USB_MAX_STR_SIZE	1024


static volatile int done = 0;
static int gWaitEOF = false;
static pthread_cond_t *gReadCompleteConditionPtr = NULL;
static pthread_mutex_t *gReadMutexPtr = NULL;



#if DEBUG==2  

static char
hexdigit( char c )
{
    return ( c < 0 || c > 15 )? '?': (c < 10)? c + '0': c - 10 + 'A';
}

static char
asciidigit( char c )
{
    return (c< 20 || c > 0x7E)? '.': c;
}

void
dump( char *text, void *s, int len )
{
    int i;
    char *p = (char *) s;
    char m[1+2*16+1+16+1];

    fprintf( stderr, "%s pointer %x len %d\n", text, (unsigned int) p, len );

    for ( ; len > 0; len -= 16 )
    {
	char *q = p;
	char *out = m;
	*out++ = '\t';
	for ( i = 0; i < 16 && i < len; ++i, ++p )
	{
	    *out++ = hexdigit( (*p >> 4) & 0x0F );
				 *out++ = hexdigit( *p & 0x0F );
	}
	for ( ;i < 16; ++i )
	{
	    *out++ = ' ';
	    *out++ = ' ';
	}
	*out++ = '\t';
	for ( i = 0; i < 16 && i < len; ++i, ++q )
	    *out++ = asciidigit( *q );
	*out = 0;
	m[ strlen( m ) ] = '\0';
	fprintf( stderr,  "%s\n", m );
    }
}

void 
printcfs( char *text, CFStringRef s )
{
    char dest[1024];
    if ( s != NULL )
    {
	if ( CFStringGetCString(s, dest, sizeof(dest), kCFStringEncodingUTF8) )
	    sprintf( dest,  "%s <%s>\n", text, dest );
	else
	    sprintf( dest,  "%s [Unknown string]\n", text );
    } else {
       sprintf( dest,  "%s [NULL]\n", text );
    }
    perror( dest );
}

void
cmpcfs( char *text, CFStringRef a, CFStringRef b )
{
    CFRange found = {0, 0};
    
    printcfs( text, a );
    printcfs( " ", b );

    if (a != NULL && b != NULL) {
	found = CFStringFind( a, b, kCFCompareCaseInsensitive );
    
    } else if (a == NULL && b == NULL) {
	found.length = 1;   /* Match */
	found.location = 0;
    } else {
	found.length = 0;   /* No match. */
    }
    
    if ( found.length > 0 )
	fprintf( stderr,  "matched @%d:%d\n", (int) found.location, (int) found.length);
    else
	fprintf( stderr,  "not matched\n" );
}
#endif /*DEBUG==2 */

#ifdef PARSE_PS_ERRORS
static const char *nextLine (const char *buffer);
static void parsePSError (char *sockBuffer, int len);


static const char *nextLine (const char *buffer)
{
    const char *cptr, *lptr = NULL;
    for (cptr = buffer; *cptr && lptr == NULL; cptr++)
	if (*cptr == '\n' || *cptr == '\r')
	    lptr = cptr;
    return lptr;
}

static void parsePSError (char *sockBuffer, int len)
{
    static char	 gErrorBuffer[1024] = "";
    static char *gErrorBufferPtr = gErrorBuffer;
    static char *gErrorBufferEndPtr = gErrorBuffer + sizeof(gErrorBuffer);

    char *pCommentBegin, *pCommentEnd, *pLineEnd;
    char *logLevel;
    char logstr[1024];
    int	 logstrlen;

    if (gErrorBufferPtr + len > gErrorBufferEndPtr - 1)
	gErrorBufferPtr = gErrorBuffer;
    if (len > sizeof(gErrorBuffer) - 1)
	len = sizeof(gErrorBuffer) - 1;

    memcpy(gErrorBufferPtr, (const void *)sockBuffer, len);
    gErrorBufferPtr += len;
    *(gErrorBufferPtr + 1) = '\0';


    pLineEnd = (char *)nextLine((const char *)gErrorBuffer);
    while (pLineEnd != NULL)
    {
	*pLineEnd++ = '\0';

	pCommentBegin = strstr(gErrorBuffer,"%%[");
	pCommentEnd = strstr(gErrorBuffer, "]%%");
	if (pCommentBegin != gErrorBuffer && pCommentEnd != NULL)
	{
	    pCommentEnd += 3;		 /* Skip past "]%%" */
	    *pCommentEnd = '\0';	 /* There's always room for the nul */

	    if (strncasecmp(pCommentBegin, "%%[ Error:", 10) == 0)
		logLevel = "DEBUG";
	    else if (strncasecmp(pCommentBegin, "%%[ Flushing", 12) == 0)
		logLevel = "DEBUG";
	    else
		logLevel = "INFO";
	    
	    if ((logstrlen = snprintf(logstr, sizeof(logstr), "%s: %s\n", logLevel, pCommentBegin)) >= sizeof(logstr))
	    {
		/* If the string was trucnated make sure it has a linefeed before the nul */
		logstrlen = sizeof(logstr) - 1;
		logstr[logstrlen - 1] = '\n';
	    }
	    write(STDERR_FILENO, logstr, logstrlen);
	}

	/* move everything over... */
	strcpy(gErrorBuffer, pLineEnd);
	gErrorBufferPtr = gErrorBuffer;
	pLineEnd = (char *)nextLine((const char *)gErrorBuffer);
    }
}
#endif /* PARSE_PS_ERRORS */

void *
readthread( void *reference )
{
    /*
    **	post a read to the device and write results to stdout
    **	the final pending read will be Aborted in the main thread
    */
    UInt8		    readbuffer[512];
    UInt32		    rbytes;
    kern_return_t	    readstatus;
    USBPrinterClassContext  **classdriver = (USBPrinterClassContext **) reference;

    
    do
    {
	rbytes = sizeof(readbuffer) - 1;
	readstatus = (*classdriver)->ReadPipe( classdriver, readbuffer, &rbytes );
	if ( kIOReturnSuccess == readstatus && rbytes > 0 )
	{
	    write( STDOUT_FILENO, readbuffer, rbytes );
	    /* cntrl-d is echoed by the printer.
	    * NOTES: 
	    *	Xerox Phaser 6250D doesn't echo the cntrl-d.
	    *	Xerox Phaser 6250D doesn't always send the product query.
	    */
	    if (gWaitEOF && readbuffer[rbytes-1] == 0x4)
		break;
#ifdef PARSE_PS_ERRORS
	    parsePSError(readbuffer, rbytes);
#endif
	}
    } while ( gWaitEOF || !done );  /* Abort from main thread tests error here */

    /* Let the other thread (main thread) know that we have
    * completed the read thread...
    */
    pthread_mutex_lock(gReadMutexPtr);
    pthread_cond_signal(gReadCompleteConditionPtr);
    pthread_mutex_unlock(gReadMutexPtr);

    return NULL;
}

/*
* 'print_device()' - Send a file to the specified USB port.
*/

int print_device(const char *uri, const char *hostname, const char *resource, const char *options, int fd, int copies)
{
    UInt32	wbytes,		/* Number of bytes written */
		buffersize = 2048;
    size_t	nbytes;		/* Number of bytes read */
    off_t	tbytes;		/* Total number of bytes written */
    char	*buffer,	/* Output buffer */
		*bufptr;	/* Pointer into buffer */

   pthread_cond_t   readCompleteCondition;
   pthread_mutex_t  readMutex;
   pthread_t	    thr;
   int		    thread_created = 0;

    USBPrinterInfo	*targetPrinter = NULL;
    CFMutableArrayRef	usbPrinters;
    char		manufacturer_buf[USB_MAX_STR_SIZE],
			product_buf[USB_MAX_STR_SIZE],
			serial_buf[USB_MAX_STR_SIZE];
    CFStringRef		manufacturer;
    CFStringRef		product;
    CFStringRef		serial;

    OSStatus		status = noErr;


    #if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
	struct sigaction action;    /* Actions for POSIX signals */
    #endif /* HAVE_SIGACTION && !HAVE_SIGSET */

    fprintf(stderr, "INFO: Opening the print file and connection...\n");

    parseOptions(options, serial_buf);

    if (resource[0] == '/')
      resource++;

    removePercentEscapes(hostname,	manufacturer_buf,   sizeof(manufacturer_buf));
    removePercentEscapes(resource,	product_buf,	    sizeof(product_buf));

    manufacturer = CFStringCreateWithCString(NULL, manufacturer_buf, kCFStringEncodingUTF8);
    product	 = CFStringCreateWithCString(NULL, product_buf,	     kCFStringEncodingUTF8);
    serial	 = CFStringCreateWithCString(NULL, serial_buf,	     kCFStringEncodingUTF8);

    USBPrinterInfo	    *activePrinter = NULL;
    USBPrinterClassContext  **classdriver;
    int			    countdown = INITIAL_LOG_INTERVAL;
    do
    {
	/* */
	/*  given a manufacturer and product, bind to a specific printer on the bus */
	/* */
	usbPrinters = UsbGetAllPrinters();
	/* */
	/*  if we have at least one element of the URI, find a printer module that matches */
	/* */
	if ( NULL != usbPrinters && (manufacturer || product ) )
	{
	    int i,
		numPrinters =  CFArrayGetCount(usbPrinters);
	    for ( i = 0; i < numPrinters; ++i ) 
	    {
		int		match = FALSE;
		USBPrinterInfo	*printer = (USBPrinterInfo *) CFArrayGetValueAtIndex( usbPrinters, i );
		if ( printer )
		{
		    match = printer->address.manufacturer && manufacturer? CFEqual(printer->address.manufacturer, manufacturer ): FALSE;
		    if ( match )
		    {
			match = printer->address.product && product? CFEqual(printer->address.product, product ): FALSE;
		    }
		    if ( match && serial )  
		    {
			/* Note with old queues (pre Panther) the CUPS uri may have no serial number (serial==NULL). */
			/*  In this case, we will ignore serial number (as before), and we'll match to the first */
			/*  printer that agrees with manufacturer and product. */
			/* If the CUPS uri does include a serial number, we'll enter this clause */
			/*  which requires the printer's serial number to match the CUPS serial number. */
			/* The net effect is that for printers with a serial number, */
			/*  new queues must match the serial number, while old queues match any printer  */
			/*  that satisfies the manufacturer/product match. */
			/* */
			match = printer->address.serial? CFEqual(printer->address.serial, serial ): FALSE;
		    }
		    if ( match )
		    {
			targetPrinter = UsbCopyPrinter( printer );
			break;	/* for, compare partial address to address for each printer on usb bus */
		    }
		}
	    }
	}
	UsbReleaseAllPrinters( usbPrinters );
	if ( NULL != targetPrinter )
	    status = UsbRegistryOpen( &targetPrinter->address, &activePrinter );

	if ( NULL == activePrinter )
	{
	    sleep( PRINTER_POLLING_INTERVAL );
	    countdown -= PRINTER_POLLING_INTERVAL;
	    if ( !countdown )
	    {
		/* periodically, write to the log so someone knows we're waiting */
		if (NULL == targetPrinter)
		    fprintf( stderr, "WARNING: Printer not responding\n" );
		else
		    fprintf( stderr, "INFO: Printer busy\n" );
		countdown = SUBSEQUENT_LOG_INTERVAL;	/* subsequent log entries, every 30 minutes */
	    }
	}
    } while ( NULL == activePrinter );

    classdriver = activePrinter->classdriver;
    if ( NULL == classdriver )
    {
	perror("ERROR: Unable to open USB Printing Class port");
	return (status);
    }
    
    /*
    * Now that we are "connected" to the port, ignore SIGTERM so that we
    * can finish out any page data the driver sends (e.g. to eject the
    * current page...  Only ignore SIGTERM if we are printing data from
    * stdin (otherwise you can't cancel raw jobs...)
    */
    
    if (fd != 0)
    {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
	sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
	memset(&action, 0, sizeof(action));
	
	sigemptyset(&action.sa_mask);
	action.sa_handler = SIG_IGN;
	sigaction(SIGTERM, &action, NULL);
#else
	signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
    }

    buffer = malloc( buffersize );
    if ( !buffer ) {
	fprintf( stderr, "ERROR: Couldn't allocate internal buffer\n" );
	status = -1;
    }
    else
    {
	fprintf(stderr, "INFO: Sending the print file...\n");
	if (pthread_cond_init(&readCompleteCondition, NULL) == 0)
	{
	    gReadCompleteConditionPtr = &readCompleteCondition;
	    
	    if (pthread_mutex_init(&readMutex, NULL) == 0)
	    {
		gReadMutexPtr = &readMutex;

		if (pthread_create(&thr, NULL, readthread, classdriver ) > 0)
		    fprintf(stderr, "WARNING: Couldn't create read channel\n");
		else
		    thread_created = 1;
	    }
	}
    }
    /*
    * the main thread sends the print file...
    */
    while (noErr == status && copies > 0)
    {
	copies --;
	if (STDIN_FILENO != fd)
	{
	    fputs("PAGE: 1 1", stderr);
	    lseek( fd, 0, SEEK_SET );	/* rewind */
	}

	tbytes = 0;
	while (noErr == status && (nbytes = read(fd, buffer, buffersize)) > 0)
	{
	    /*
	    * Write the print data to the printer...
	    */

	    tbytes += nbytes;
	    bufptr = buffer;

	    while (nbytes > 0 && noErr == status )
	    {
		wbytes = nbytes;
		status = (*classdriver)->WritePipe( classdriver, (UInt8*)bufptr, &wbytes, 0 /*nbytes > wbytes? 0: feof(fp)*/ );
		if (wbytes < 0 || noErr != status)
		{
		    OSStatus err;
		    err = (*classdriver)->Abort( classdriver );
		    fprintf(stderr, "ERROR: %ld: Unable to send print file to printer (canceled %ld)\n", status, err );
		    break;
		}

		nbytes -= wbytes;
		bufptr += wbytes;
	    }

	    if (fd != 0 && noErr == status)
		fprintf(stderr, "INFO: Sending print file, %qd bytes...\n", (off_t)tbytes);
	}
    }
    done = 1;	/* stop scheduling reads */

    if ( thread_created )
    {
	/* Give the read thread WAITEOF_DELAY seconds to complete all the data. If
	* we are not signaled in that time then force the thread to exit by setting
	* the waiteof to be false. Plese note that this relies on us using the timeout
	* class driver.
	*/
	struct timespec sleepUntil = { time(NULL) + WAITEOF_DELAY, 0 };
	pthread_mutex_lock(&readMutex);
	if (pthread_cond_timedwait(&readCompleteCondition, &readMutex, (const struct timespec *)&sleepUntil) != 0)
	    gWaitEOF = false;
	pthread_mutex_unlock(&readMutex);
	pthread_join( thr,NULL);		/* wait for the child thread to return */
    }

    (*classdriver)->Close( classdriver );   /* forces the read to stop incase we are doing a blocking read */
    UsbUnloadClassDriver( activePrinter );
    /*
    * Close the socket connection and input file and return...
    */
    free( buffer );

    if (STDIN_FILENO != fd)
	close(fd);

    if (gReadCompleteConditionPtr != NULL)
	pthread_cond_destroy(gReadCompleteConditionPtr);
    if (gReadMutexPtr != NULL)
	pthread_mutex_destroy(gReadMutexPtr);

    return status == kIOReturnSuccess? 0: status;
}

static Boolean
encodecfstr( CFStringRef cfsrc, char *dst, long len )
{
    return CFStringGetCString(cfsrc, dst, len, kCFStringEncodingUTF8 );
}

/*
* 'list_devices()' - List all USB devices.
*/
void list_devices(void)
{
    char		encodedManufacturer[1024];
    char		encodedProduct[1024];
    char		uri[1024];
    CFMutableArrayRef	usbBusPrinters = UsbGetAllPrinters();
    CFIndex		i, numPrinters = NULL != usbBusPrinters? CFArrayGetCount( usbBusPrinters ): 0;
    
    puts("direct usb \"Unknown\" \"USB Printer (usb)\"");
    for ( i = 0;  i < numPrinters; ++i )
    {
	USBPrinterInfo	    *printer = (USBPrinterInfo *) CFArrayGetValueAtIndex( usbBusPrinters, i );

	if ( printer ) 
	{
	    CFStringRef addressRef = UsbMakeFullUriAddress( printer );
	    if ( addressRef )
	    {
		if ( CFStringGetCString(addressRef, uri, sizeof(uri), kCFStringEncodingUTF8) ) {
	    
		    encodecfstr( printer->address.manufacturer, encodedManufacturer, sizeof(encodedManufacturer) );
		    encodecfstr( printer->address.product, encodedProduct, sizeof(encodedProduct) );
		    printf("direct %s \"%s %s\" \"%s\"\n", uri, encodedManufacturer, encodedProduct, encodedProduct);
		}
	    }
	}
    }
    UsbReleaseAllPrinters( usbBusPrinters );
    fflush(NULL);
}


static void parseOptions(const char *options, char *serial)
{
    char    *serialnumber;  /* ?serial=<serial> or ?location=<location> */
    char    optionName[255],	/* Name of option */
	    value[255],		/* Value of option */
	    *ptr;		/* Pointer into name or value */

    if (serial)
	*serial = '\0';

    if (!options)
	return;

    serialnumber = NULL;

    while (*options != '\0')
    {
	/*
	* Get the name...
	*/
	for (ptr = optionName; *options && *options != '=' && *options != '+'; )
	    *ptr++ = *options++;

	*ptr = '\0';
	value[0] = '\0';

	if (*options == '=')
	{
	    /*
	    * Get the value...
	    */
	    
	    options ++;
	    
	    for (ptr = value; *options && *options != '+';)
		*ptr++ = *options++;

	    *ptr = '\0';
	    
	    if (*options == '+')
		options ++;
	}
	else if (*options == '+')
	{
	    options ++;
	}

	/*
	* Process the option...
	*/
	if (strcasecmp(optionName, "waiteof") == 0)
	{
	    if (strcasecmp(value, "on") == 0 ||
		strcasecmp(value, "yes") == 0 ||
		strcasecmp(value, "true") == 0)
	    {
		gWaitEOF = true;
	    }
	    else if (strcasecmp(value, "off") == 0 ||
		    strcasecmp(value, "no") == 0 ||
		    strcasecmp(value, "false") == 0)
	    {
		gWaitEOF = false;
	    }
	    else
	    {
		fprintf(stderr, "WARNING: Boolean expected for waiteof option \"%s\"\n", value);
	    }
	}
	else if (strcasecmp(optionName, "serial") == 0 ||
		 strcasecmp(optionName, "location") == 0 )
	{
	    strcpy(serial, value);
	    serialnumber = serial;
	}
    }

    return;
}


/*!
 * @function  addPercentEscapes
 * @abstract  Encode a string with percent escapes
 *
 * @param  src	    The source C string
 * @param  dst	    Desination buffer
 * @param  dstMax   Size of desination buffer
 *
 * @result    A non-zero return value for errors
 */
static int addPercentEscapes(const unsigned char* src, char* dst, int dstMax)
{
  unsigned char c;
  char	    *dstEnd = dst + dstMax - 1; /* -1 to leave room for the NUL */

  while (*src)
  {
    c = *src++;

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
	(c >= '0' && c <= '9') || (c == '.' || c == '-'	 || c == '*' || c == '_'))
    {
      if (dst >= dstEnd)
	return -1;

      *dst++ = c;
    }
    else
    {
      if (dst >= dstEnd - 2)
	return -1;

      snprintf(dst, dstEnd - dst, "%%%02x", c);
      dst += 3;
    }
  }

  *dst = '\0';
  return 0;
}


/*!
 * @function	removePercentEscapes
 * @abstract	Returns a string with any percent escape sequences replaced with their equivalent character
 *
 * @param	src	Source buffer
 * @param	srclen	Number of bytes in source buffer
 * @param	dst	Desination buffer
 * @param	dstMax	Size of desination buffer
 *
 * @result	A non-zero return value for errors
 */
static int removePercentEscapes(const char* src, unsigned char* dst, int dstMax)
{
    int c;
    const unsigned char *dstEnd = dst + dstMax;

    while (*src && dst < dstEnd)
    {
	c = *src++;

	if (c == '%')
	{
	    sscanf(src, "%02x", &c);
	    src += 2;
	}
	*dst++ = (char)c;
    }

    if (dst >= dstEnd)
	return -1;

    *dst = '\0';
    return 0;
}

/*-----------------------------------------------------------------------------*

	DelimitSubstring

	Desc:	Search a string from a starting location, looking for a given
		delimiter. Return the range from the start of the search to the
		delimiter, or end of string (whichever is shorter).

	In:	stringToSearch	    string which contains a substring that we search
		delim		    string which marks the end of the string 
		bounds		    start and length of substring of stringToSearch
		options		    case sensitive, anchored, etc.

	Out:	Range up to the delimiter.

*-----------------------------------------------------------------------------*/
static CFRange
DelimitSubstring( CFStringRef stringToSearch, CFStringRef delim, CFRange bounds, CFStringCompareFlags options )
{
    CFRange	where_delim,	/* where the delimiter was found */
		value;
    /* */
    /*	trim leading space by changing bounds */
    /* */
    while ( bounds.length > 0 && CFStringFindWithOptions( stringToSearch, CFSTR(" "), bounds, kCFCompareAnchored, &where_delim ) )
    {
	++bounds.location;  /* drop a leading ' ' */
	--bounds.length;
    }
    value = bounds;	    /* assume match to the end of string, may be NULL */
    /* */
    /*	find the delimiter in the remaining string */
    /* */
    if (  bounds.length > 0 && CFStringFindWithOptions( stringToSearch, delim, bounds, options, &where_delim ) )
    {
	/* */
	/* match to the delimiter */
	/* */
	value.length = where_delim.location /* delim */ - bounds.location /* start of search */;
    }
    DEBUG_CFString( "\tFind target", stringToSearch );
    DEBUG_CFString( "\tFind pattern", delim );
    DEBUG_ERR( (int) value.location, "\t\tFound %d\n" );
    DEBUG_ERR( (int) value.length, " length %d"	 );

    return value;
}


/*-----------------------------------------------------------------------------*

	DeviceIDCreateValueList

	Desc:	Create a new string for the value list of the specified key.
		The key may be specified as two strings (an abbreviated form
		and a standard form). NULL can be passed for either form of 
		the key.
		
		(Although passing NULL for both forms of the key is considered
		 bad form[!] it is handled correctly.)

	In:	deviceID    the device's IEEE-1284 DeviceID key-value list
		abbrevKey   the key we're interested in (NULL allowed)
		key	    

	Out:	CFString    the value list 
		or NULL	    key wasn't found in deviceID

*-----------------------------------------------------------------------------*/
CFStringRef
DeviceIDCreateValueList( const CFStringRef deviceID, const CFStringRef abbrevKey, const CFStringRef key )
{
    CFRange	found = CFRangeMake( -1,0);   /* note CFStringFind sets length 0 if string not found */
    CFStringRef valueList = NULL;

    DEBUG_CFString( "---------DeviceIDCreateValueList DeviceID:", deviceID );
    DEBUG_CFString( "---------DeviceIDCreateValueList key:", key );
    DEBUG_CFString( "---------DeviceIDCreateValueList abbrevkey:", abbrevKey );
   if ( NULL != deviceID && NULL != abbrevKey )
	found = CFStringFind( deviceID, abbrevKey, kCFCompareCaseInsensitive );
    if (  NULL != deviceID && NULL != key && found.length <= 0 )
	found = CFStringFind( deviceID, key, kCFCompareCaseInsensitive );
    if ( found.length > 0 )
    {
	/* the key is at found */
	/* the value follows the key until we reach the semi-colon, or end of string */
	/* */
	CFRange search = CFRangeMake( found.location + found.length,
				  CFStringGetLength( deviceID ) - (found.location + found.length) );
	/* */
	/* finally extract the string */
	/* */
	valueList = CFStringCreateWithSubstring ( kCFAllocatorDefault, deviceID,
						  DelimitSubstring( deviceID, kDeviceIDKeyValuePairDelimiter, search, kCFCompareCaseInsensitive ) );
    DEBUG_CFString( "---------DeviceIDCreateValueList:", valueList );
    }
    return valueList;

}



/******************************************************************************/

/*-----------------------------------------------------------------------------*

CompareSameString

Desc:	Return the CFCompare result for two strings, either or both of which
	can be NULL.

In:
	a	current value
	b	last value

Out:
	0	if the strings match
	non-zero    if the strings don't match

*-----------------------------------------------------------------------------*/
static int
CompareSameString( const CFStringRef a, const CFStringRef b )
{
    if ( NULL == a && NULL == b )
	return 0;
    else if ( NULL != a && NULL != b )
	return CFStringCompare( a, b, kCFCompareAnchored );
    else
	return 1;   /* one of a or b is NULL this time, but wasn't last time */
}


/******************************************************************************/
kern_return_t
UsbLoadClassDriver( USBPrinterInfo *printer, CFUUIDRef interfaceID, CFStringRef classDriverBundle )
{
    kern_return_t   kr = kUSBPrinterClassDeviceNotOpen;
    if ( NULL != classDriverBundle )
	printer->bundle = classDriverBundle;	/* vendor-specific class override */
    else
#ifdef TIMEOUT
	classDriverBundle = kUSBGenericTOPrinterClassDriver;	/*  supply the generic TIMEOUT class driver */
#else
	classDriverBundle = kUSBGenericPrinterClassDriver;  /*	supply the generic  class driver */
#endif
    DEBUG_CFString( "UsbLoadClassDriver classDriverBundle", classDriverBundle );
    if ( NULL != classDriverBundle )
    {
	USBPrinterClassContext	**classdriver = NULL;
	CFURLRef		classDriverURL = CFURLCreateWithFileSystemPath( NULL, classDriverBundle, kCFURLPOSIXPathStyle, TRUE );
	CFPlugInRef		plugin = NULL == classDriverURL? NULL: CFPlugInCreate( NULL, classDriverURL );
	if ( NULL != plugin)
	{
	    /* See if this plug-in implements the Test type. */
	    CFArrayRef factories =  CFPlugInFindFactoriesForPlugInTypeInPlugIn( kUSBPrinterClassTypeID, plugin );

	    /* If there are factories for the requested type, attempt to */
	    /* get the IUnknown interface. */
	    DEBUG_ERR( 0, "UsbLoadClassDriver plugin %x\n" );
	    if (NULL != factories && CFArrayGetCount(factories) > 0) 
	    {
		/* Get the factory ID for the first location in the array of IDs. */
		CFUUIDRef factoryID = CFArrayGetValueAtIndex( factories, 0 );
		/* Use the factory ID to get an IUnknown interface. */
		/* Here the code for the PlugIn is loaded. */
		IUnknownVTbl **iunknown = CFPlugInInstanceCreate( NULL, factoryID, kUSBPrinterClassTypeID );
		/* If this is an IUnknown interface, query for the Test interface. */
		DEBUG_ERR( 0, "UsbLoadClassDriver factories %x\n" );
		if (NULL != iunknown)
		{
		    DEBUG_ERR( 0, "UsbLoadClassDriver CFPlugInInstanceCreate %x\n" );
		    kr = (*iunknown)->QueryInterface( iunknown, CFUUIDGetUUIDBytes(interfaceID), (LPVOID *) &classdriver );

		    (*iunknown)->Release( iunknown );
		    if ( S_OK == kr && NULL != classdriver )
		    {
			DEBUG_ERR( kr, "UsbLoadClassDriver QueryInterface %x\n" );
			printer->plugin = plugin;
			kr = (*classdriver)->Initialize( classdriver, printer->classdriver );
			
			kr = kIOReturnSuccess;
			printer->classdriver = classdriver;
		    }
		    else
		    {
			DEBUG_ERR( kr, "UsbLoadClassDriver QueryInterface FAILED %x\n" );
		    }
		}
		else
		{
		    DEBUG_ERR( kr, "UsbLoadClassDriver CFPlugInInstanceCreate FAILED %x\n" );
		}
	    }
	    else
	    {
		DEBUG_ERR( kr, "UsbLoadClassDriver factories FAILED %x\n" );
	    }
	}
	else
	{
	    DEBUG_ERR( kr, "UsbLoadClassDriver plugin FAILED %x\n" );
	}
	if ( kr != kIOReturnSuccess || NULL == plugin || NULL == classdriver )
	{
	    UsbUnloadClassDriver( printer );
	}
    }
    
    return kr;
}


kern_return_t
UsbUnloadClassDriver( USBPrinterInfo *printer )
{
    DEBUG_ERR( kIOReturnSuccess, "UsbUnloadClassDriver %x\n" );
    if ( NULL != printer->classdriver )
	(*printer->classdriver)->Release( printer->classdriver );
    printer->classdriver = NULL;
    
    if ( NULL != printer->plugin )
	CFRelease( printer->plugin );
    printer->plugin = NULL;
    
    return kIOReturnSuccess;
}


/*-----------------------------------------------------------------------------*

    UsbAddressDispose

    Desc:   deallocates anything used to create a persistent printer address

    In: address	    the printer address we've created

    Out:    <none>

*-----------------------------------------------------------------------------*/
void
UsbAddressDispose( USBPrinterAddress *address )
{
    if ( address->product != NULL ) CFRelease( address->product );
    if ( address->manufacturer != NULL ) CFRelease( address->manufacturer );
    if ( address->serial != NULL ) CFRelease( address->serial );
    if ( address->command != NULL ) CFRelease( address->command );

    address->product =
    address->manufacturer =
    address->serial =
    address->command = NULL;

}

/*-----------------------------------------------------------------------------*

    UsbGetPrinterAddress

    Desc:   Given a printer we're enumerating, discover it's persistent
    reference.

    A "persistent reference" is one which enables us to identify
    a printer regardless of where it resides on the USB topology,
    and enumeration sequence.

    To do this, we actually construct a reference from information
    buried inside the printer. First we look at the USB device
    descripton: an ideally defined device will support strings for
    manufacturer and product id, and serial number. The serial number
    will be unique for each printer.

    Our prefered identification fetches the IEEE-1284 device id string.
    This transparently handled IEEE-1284 compatible printers which
    connected over a USB-parallel cable. Only if we can't get all the
    information to uniquely identify the printer do we try the strings
    referenced in the printer's USB device descriptor. (These strings
    are typically absent in a USB-parallel cable.)

    If a device doesn't support serial numbers we have a problem:
    we can't distinguish between two identical printers. Unique serial
    numbers allow us to distinguish between two same-model, same-manufacturer
    USB printers.

    In:
	thePrinter	iterator required for fetching device descriptor
	devRefNum	required to configure the interface

    Out:
	address->manufacturer
	address->product
	address->serial
		Any (and all) of these may be NULL if we can't retrieve
		information for IEEE1284 DeviceID or the USB device
		descriptor. Caller should be prepared to handle such a case.
	address->command
		May be updated.

*-----------------------------------------------------------------------------*/
OSStatus
UsbGetPrinterAddress( USBPrinterInfo *thePrinter, USBPrinterAddress *address, UInt16 timeout )
{

    /* */
    /*	start by assuming the device is not IEEE-1284 compliant */
    /*	and that we can't read in the required strings. */
    /* */
    OSStatus		    err;
    CFStringRef		    deviceId = NULL;
    USBPrinterClassContext  **printer = NULL == thePrinter? NULL: thePrinter->classdriver;
    
    address->manufacturer =
    address->product =
    address->compatible =
    address->serial =
    address->command = NULL;

    DEBUG_DUMP( "UsbGetPrinterAddress thePrinter", thePrinter, sizeof(USBPrinterInfo) );

    err = (*printer)->GetDeviceID( printer, &deviceId, timeout );
    if ( noErr == err && NULL != deviceId )
    {
	/* the strings embedded here are defined in the IEEE1284 spec */
	/* */
	/*  use the MFG/MANUFACTURER for the manufacturer */
	/*  and the MDL/MODEL for the product */
	/*  there is no serial number defined in IEEE1284 */
	/*	but it's been observed in recent HP printers */
	/* */
	address->command = DeviceIDCreateValueList( deviceId, kDeviceIDKeyCommandAbbrev, kDeviceIDKeyCommand );

	address->product = DeviceIDCreateValueList( deviceId, kDeviceIDKeyModelAbbrev, kDeviceIDKeyModel );
	address->compatible = DeviceIDCreateValueList( deviceId, kDeviceIDKeyCompatibleAbbrev, kDeviceIDKeyCompatible );

	address->manufacturer = DeviceIDCreateValueList( deviceId, kDeviceIDKeyManufacturerAbbrev, kDeviceIDKeyManufacturer );

	address->serial = DeviceIDCreateValueList( deviceId, kDeviceIDKeySerialAbbrev, kDeviceIDKeySerial );
	CFRelease( deviceId );
    }
    DEBUG_CFString( "UsbGetPrinterAddress DeviceID address->product", address->product );
    DEBUG_CFString( "UsbGetPrinterAddress DeviceID address->compatible", address->compatible );
    DEBUG_CFString( "UsbGetPrinterAddress DeviceID address->manufacturer", address->manufacturer );
    DEBUG_CFString( "UsbGetPrinterAddress DeviceID address->serial", address->serial );

    if ( NULL == address->product || NULL == address->manufacturer || NULL == address->serial )
    {
	/* */
	/*  if the manufacturer or the product or serial number were not specified in DeviceID */
	/*	try to construct the address using USB English string descriptors */
	/* */
	IOUSBDeviceDescriptor	desc;
	USBIODeviceRequest	request;
				
	request.requestType = USBmakebmRequestType( kUSBIn,  kUSBStandard, kUSBDevice );
	request.request = kUSBRqGetDescriptor;
	request.value = (kUSBDeviceDesc << 8) | 0;
	request.index = 0;  /* not kUSBLanguageEnglish*/
	request.length = sizeof(desc);
	request.buffer = &desc;
	err = (*printer)->DeviceRequest( printer, &request, timeout );
	DEBUG_ERR( (kern_return_t) err, "UsbGetPrinterAddress: GetDescriptor %x" );
	if ( kIOReturnSuccess == err )
	{
	    /* once we've retrieved the device descriptor */
	    /*	try to fill in missing pieces of information */
	    /* */
	    /*	Don't override any information already retrieved from DeviceID. */

	    if ( NULL == address->product)
	    {
		err = (*printer)->GetString( printer, desc.iProduct, kUSBLanguageEnglish, timeout, &address->product );
		if ( kIOReturnSuccess != err || address->product == NULL) {
		    address->product = CFSTR("Unknown");
		}		 
	    }
	    DEBUG_CFString( "UsbGetPrinterAddress: UsbGetString address->product\n", address->product );

	    if ( NULL == address->manufacturer )
	    {
		err = (*printer)->GetString( printer, desc.iManufacturer, kUSBLanguageEnglish, timeout, &address->manufacturer );
		if (kIOReturnSuccess != err || address->manufacturer == NULL) {
		    address->manufacturer = CFSTR("Unknown");
		}
	    }
	    DEBUG_CFString( "UsbGetPrinterAddress: UsbGetString address->manufacturer\n", address->manufacturer );

	    if ( NULL == address->serial )
	    {
		/* if the printer doesn't have a serial number, use locationId */
		if ( 0 == desc.iSerialNumber )
		{
		    address->serial = CFStringCreateWithFormat( NULL, NULL, CFSTR("%lx"), (*printer)->location );
		}
		else
		{
		    err = (*printer)->GetString( printer, desc.iSerialNumber, kUSBLanguageEnglish, timeout, &address->serial );
		    /* trailing NULs aren't handled correctly in URI */
		    if ( address->serial )
		    {
			UniChar	    nulbyte = { 0 };
			CFStringRef trim = CFStringCreateWithCharacters(NULL, &nulbyte, 1);
			CFMutableStringRef newserial = CFStringCreateMutableCopy(NULL, 0, address->serial);

			CFStringTrim( newserial, trim );

			CFRelease(trim);
			CFRelease( address->serial );

			address->serial = newserial;
		    }
		}
	    }
	    DEBUG_CFString( "UsbGetPrinterAddress: UsbGetString address->serial\n", address->serial );
	}
    }
    if ( NULL != address->product)
	CFRetain(address->product);	    /* UsbGetString is really a UsbCopyString. */
    if ( NULL != address->manufacturer )
	CFRetain( address->manufacturer );
    if ( NULL != address->serial )
	CFRetain( address->serial );
    return err;
}


/*-----------------------------------------------------------------------------*

UsbSamePrinter

	Desc:	match two Usb printer address; return TRUE if they are the same.

	In:	a   the persistent address found last time
		b   the persistent address found this time

	Out:	non-zero iff the addresses are the same

*-----------------------------------------------------------------------------*/
int
UsbSamePrinter( const USBPrinterAddress *a, const USBPrinterAddress *b )
{
    int result = 0;
    DEBUG_CFCompareString( "UsbSamePrinter serial", a->serial, b->serial );
    DEBUG_CFCompareString( "UsbSamePrinter product", a->product, b->product );
    DEBUG_CFCompareString( "UsbSamePrinter manufacturer", a->manufacturer, b->manufacturer );

    result = !CompareSameString( a->serial, b->serial );
    if ( result )  result = !CompareSameString( a->product, b->product );
    if ( result ) result = !CompareSameString( a->manufacturer, b->manufacturer );

    return result;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Method:	UsbMakeFullUriAddress

    Input Parameters:

    Output Parameters:

    Description:
	Fill in missing address information

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
CFStringRef
UsbMakeFullUriAddress( USBPrinterInfo *printer )
{
    /* */
    /*	fill in missing address information. */
    /* */
    CFMutableStringRef printerUri = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("usb://") );
    if ( NULL != printerUri )
    {
	CFStringRef serial = printer->address.serial;

	CFStringAppend(printerUri, printer->address.manufacturer? CreateEncodedCFString( printer->address.manufacturer ): CFSTR("Unknown") );
	CFStringAppend(printerUri, CFSTR("/") );

	CFStringAppend(printerUri, printer->address.product? CreateEncodedCFString( printer->address.product ): CFSTR("Unknown") );

	/*Handle the common case where there is no serial number (S450?) */
	CFStringAppend(printerUri, serial == NULL? CFSTR("?location="): CFSTR("?serial=") );
	if ( serial == NULL)
	    serial = CFStringCreateWithFormat( NULL, NULL, CFSTR("%lx"), printer->location );

	 CFStringAppend(printerUri,  serial? CreateEncodedCFString( serial ): CFSTR("Unknown") );
    }
    
    return printerUri;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Method:	UsbGetAllPrinters

    Input Parameters:

    Output Parameters:
	array of all USB printers on the system

    Description:
	Build a list of USB printers by iterating IOKit USB objects

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
CFMutableArrayRef
UsbGetAllPrinters( void )
{
    kern_return_t	kr;	/* kernel errors */
    mach_port_t		master_device_port = 0;
    io_service_t	usbInterface = 0;
    io_iterator_t	iter = 0;
    CFMutableArrayRef	printers = CFArrayCreateMutable( NULL, 0, NULL );   /* all printers */

    do
    {

	kr = IOMasterPort( bootstrap_port, &master_device_port );
	DEBUG_ERR( kr, "UsbGetAllPrinters IOMasterPort %x\n" );
	if(kIOReturnSuccess != kr)  break;

	{
	    CFDictionaryRef	usbMatch = NULL;
	    
	    /* iterate over all interfaces.  */
	    usbMatch = IOServiceMatching(kIOUSBInterfaceClassName);
	    if ( !usbMatch ) break;
	    DEBUG_ERR( kr, "UsbGetAllPrinters IOServiceMatching %x\n" );
	
	    /* IOServiceGetMatchingServices() consumes the usbMatch reference so we don't need to release it. */
	    kr = IOServiceGetMatchingServices(master_device_port, usbMatch, &iter);
	    usbMatch = NULL;
	    
	    DEBUG_ERR( kr, "UsbGetAllPrinters IOServiceGetMatchingServices %x\n" );
	    if(kIOReturnSuccess != kr || iter == NULL)	break;
	}
	
	while (	 NULL != (usbInterface = IOIteratorNext(iter))	)
	{
	    IOCFPlugInInterface	    **iodev;
	    USBPrinterInterface	    intf;
	    HRESULT		    res;
	    SInt32		    score;
	    CFMutableDictionaryRef  properties;
	    CFStringRef		    classDriver = NULL;

	    kr = IORegistryEntryCreateCFProperties( usbInterface, &properties, kCFAllocatorDefault, kNilOptions);
	    if ( kIOReturnSuccess == kr && NULL != properties)
	    {
		classDriver = (CFStringRef) CFDictionaryGetValue( properties, kUSBClassDriverProperty );
		if ( NULL != classDriver )
		    CFRetain( classDriver );
		CFRelease( properties );
	    }	 

	    kr = IOCreatePlugInInterfaceForService( usbInterface,
							kIOUSBInterfaceUserClientTypeID, 
							kIOCFPlugInInterfaceID,
							&iodev,
							&score);
		
	    DEBUG_ERR( kr, "UsbGetAllPrinters IOCreatePlugInInterfaceForService %x\n" );
	    if ( kIOReturnSuccess == kr )
	    {
		UInt8		    intfClass = 0;
		UInt8		    intfSubClass = 0;
 
		res = (*iodev)->QueryInterface( iodev, USB_INTERFACE_KIND, (LPVOID *) &intf);
		DEBUG_ERR( (kern_return_t) res, "UsbGetAllPrinters QueryInterface %x\n" );

	       (*iodev)->Release(iodev);
		if ( noErr != res ) break;
 
		kr = (*intf)->GetInterfaceClass(intf, &intfClass);
		DEBUG_ERR(kr, "UsbGetAllPrinters GetInterfaceClass %x\n");
		if ( kIOReturnSuccess == kr )
		    kr = (*intf)->GetInterfaceSubClass(intf, &intfSubClass);
		DEBUG_ERR(kr, "UsbGetAllPrinters GetInterfaceSubClass %x\n");
		
		if ( kIOReturnSuccess == kr &&
			kUSBPrintingClass == intfClass &&
			kUSBPrintingSubclass == intfSubClass )
		{

		    USBPrinterInfo	    printer,
					    *printerInfo;
		    /*
		    For each type of printer specified in the lookup spec array, find
		    all of that type of printer and add the results to the list of found
		    printers.
		    */
		    /* create this printer's persistent address */
		    memset( &printer, 0, sizeof(USBPrinterInfo) );
		    kr = (*intf)->GetLocationID(intf, &printer.location);
		    DEBUG_ERR(kr, "UsbGetAllPrinters GetLocationID %x\n");
		    if ( kIOReturnSuccess == kr )
		    {
			kr = UsbLoadClassDriver( &printer, kUSBPrinterClassInterfaceID, classDriver );
			DEBUG_ERR(kr, "UsbGetAllPrinters UsbLoadClassDriver %x\n");
			if ( kIOReturnSuccess == kr && printer.classdriver )
			{
			    (*(printer.classdriver))->interface = intf;
			    kr = UsbGetPrinterAddress( &printer, &printer.address, 60000L );
			    { 
				/* always unload the driver */
				/*  but don't mask last error */
				kern_return_t unload_err = UsbUnloadClassDriver( &printer );
				if ( kIOReturnSuccess == kr )
				    kr = unload_err;
			    }
			}
		    }
		    
		    printerInfo = UsbCopyPrinter( &printer );
		    if ( NULL != printerInfo )
			CFArrayAppendValue( printers, (const void *) printerInfo );	/* keep track of it */

		 } /* if there's a printer */
		kr = (*intf)->Release(intf);
	    } /* if IOCreatePlugInInterfaceForService */
	    
	    IOObjectRelease(usbInterface);
	    usbInterface = NULL;
	    
	} /* while there's an interface */
    } while ( 0 );

    if (iter) 
    {
	IOObjectRelease(iter);
	iter = 0;
    }

    if (master_device_port) 
    {
	mach_port_deallocate(mach_task_self(), master_device_port);
	master_device_port = 0;
    }
    return printers;

} /* UsbGetAllPrinters */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Method:	UsbReleasePrinter

    Input Parameters:

    Output Parameters:

    Description:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void
UsbReleasePrinter( USBPrinterInfo *printer )
{
    if ( printer )
    {
	UsbUnloadClassDriver( printer );
	if ( NULL != printer->address.manufacturer )
	    CFRelease( printer->address.manufacturer );
	if ( NULL != printer->address.product )
	    CFRelease( printer->address.product );
	if ( NULL != printer->address.serial )
	    CFRelease( printer->address.serial );
	if ( NULL != printer->address.command )
	    CFRelease( printer->address.command );
	if ( NULL != printer->bundle )
	    CFRelease( printer->bundle );
	free( printer );
   }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Method:	UsbReleaseAllPrinters

    Input Parameters:

    Output Parameters:

    Description:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void
UsbReleaseAllPrinters( CFMutableArrayRef printers )
{
    if ( NULL != printers )
    {
	CFIndex i,
		numPrinters = CFArrayGetCount(printers);
	for ( i = 0; i < numPrinters; ++i ) 
	    UsbReleasePrinter( (USBPrinterInfo *) CFArrayGetValueAtIndex( printers, i ) );
	CFRelease( printers );		
    }
}

USBPrinterInfo *
UsbCopyPrinter( USBPrinterInfo *aPrinter )
{
    /* */
    /*	note this does not copy interface information, just address information */
    /* */
    USBPrinterInfo *printerInfo = (USBPrinterInfo *) calloc( 1, sizeof(USBPrinterInfo));
    if ( NULL != printerInfo && NULL != aPrinter )
    {
	printerInfo->location = aPrinter->location;
	if ( NULL != (printerInfo->address.manufacturer = aPrinter->address.manufacturer) )
	    CFRetain( printerInfo->address.manufacturer );
	if ( NULL != (printerInfo->address.product = aPrinter->address.product) )
	    CFRetain( printerInfo->address.product );
	if ( NULL != (printerInfo->address.serial = aPrinter->address.serial) )
	    CFRetain( printerInfo->address.serial );
	if ( NULL != (printerInfo->address.command = aPrinter->address.command) )
	    CFRetain( printerInfo->address.command );
	if ( NULL != (printerInfo->bundle = aPrinter->bundle) )
	    CFRetain( printerInfo->bundle );
    }
    
    return printerInfo;
}

/*-----------------------------------------------------------------------------*

	UsbRegistryOpen

	Desc:	opens the USB printer which matches the supplied printerAddress

	In:	myContext->printerAddress   persistent name which identifies the printer

	Out:	myContext->usbDeviceRef	    current IOKit address of this printer
*-----------------------------------------------------------------------------*/
kern_return_t
UsbRegistryOpen( USBPrinterAddress *usbAddress, USBPrinterInfo **result )
{
    kern_return_t	kr = -1;    /* indeterminate failure */
    CFMutableArrayRef	printers = UsbGetAllPrinters();
    CFIndex		numPrinters = NULL != printers? CFArrayGetCount( printers): 0;
    CFIndex		i;

    *result = NULL; /* nothing matched */
    for ( i = 0; i < numPrinters; ++i )
    {
	USBPrinterInfo	*thisPrinter = (USBPrinterInfo *) CFArrayGetValueAtIndex( printers, i );
	if (  NULL != thisPrinter && UsbSamePrinter( usbAddress, &thisPrinter->address ) ) 
	{
	    *result = UsbCopyPrinter( thisPrinter );	/* retains reference */
	    if ( NULL != *result )
	    {
		/* */
		/*  if we can't find a bi-di interface, settle for a known uni-directional interface */
		/* */
		USBPrinterClassContext **printer = NULL;
		/* */
		/*  setup the default class driver */
		/*  If one is specified, allow the vendor driver to override our default implementation */
		/* */
		kr = UsbLoadClassDriver( *result, kUSBPrinterClassInterfaceID, NULL );
		if ( kIOReturnSuccess == kr && (*result)->bundle )
		    kr = UsbLoadClassDriver( *result, kUSBPrinterClassInterfaceID, (*result)->bundle );
		if ( kIOReturnSuccess == kr && NULL != (*result)->classdriver )
		{
		    printer = (*result)->classdriver;
		    kr = (*printer)->Open( printer, (*result)->location, kUSBPrintingProtocolBidirectional );
		    if ( kIOReturnSuccess != kr || NULL == (*printer)->interface )
			kr = (*printer)->Open( printer, (*result)->location, kUSBPrintingProtocolUnidirectional );
		    /*	it's possible kIOReturnSuccess == kr && NULL == (*printer)->interface */
		    /*	    in the event that we can't open either Bidirectional or Unidirectional interface */
		    if ( kIOReturnSuccess == kr )
		    {
			if ( NULL == (*printer)->interface )
			{
			    (*printer)->Close( printer );
			    UsbReleasePrinter( *result );
			    *result = NULL;
			}
		    }
		}
	    }
	    break;
	}
    }
    UsbReleaseAllPrinters( printers ); /* but, copied printer is retained */
    DEBUG_ERR( kr, "UsbRegistryOpen return %x\n" );

    return kr;
}

/*!
 * @function	CreateEncodedCFString
 *
 * @abstract	Create an encoded version of the string parameter 
 *		so that it can be included in a URI.
 *
 * @param   string  A CFStringRef of the string to be encoded.
 * @result  An encoded CFString.
 *
 * @discussion	This function will change all characters in string into URL acceptable format
 *		by encoding the text using the US-ASCII coded character set.  The following
 *		are invalid characters: the octets 00-1F, 7F, and 80-FF hex.  Also called out
 *		are the chars "<", ">", """, "#", "{", "}", "|", "\", "^", "~", "[", "]", "`".
 *		The reserved characters for URL syntax are also to be encoded: (so don't pass
 *		in a full URL here!) ";", "/", "?", ":", "@", "=", "%", and "&".
 */
static CFStringRef CreateEncodedCFString(CFStringRef string)
{
    CFStringRef result = NULL;
    char *bufferUTF8 = NULL;
    char *bufferEncoded = NULL;

    if (string != NULL)
    {
	CFIndex bufferSizeUTF8 = (3 * CFStringGetLength(string));
	if ((bufferUTF8 = (char*)malloc(bufferSizeUTF8)) != NULL)
	{
	    CFStringGetCString(string, bufferUTF8, bufferSizeUTF8, kCFStringEncodingUTF8);
	    {
		UInt16 bufferSizeEncoded = (3 * strlen(bufferUTF8)) + 1;
		if ((bufferEncoded = (char*)malloc(bufferSizeEncoded)) != NULL)
		{
		    addPercentEscapes(bufferUTF8, bufferEncoded, bufferSizeEncoded);
		    result = CFStringCreateWithCString(kCFAllocatorDefault, bufferEncoded, kCFStringEncodingUTF8);
		}
	    }
	}
    }

    if (bufferUTF8)	free(bufferUTF8);
    if (bufferEncoded)	free(bufferEncoded);

    return result;
}

/*
 * End of "$Id: usb-darwin.c,v 1.1.2.3 2004/05/27 15:37:47 mike Exp $".
 */
