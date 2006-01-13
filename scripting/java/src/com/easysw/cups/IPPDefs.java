
package com.easysw.cups;

/**
 * @version 1.00 06-NOV-2002
 * @author  Easy Software Products
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 */

import java.io.*;
import java.util.*;

/**
 * <code>IPPDefs</code> is a collection of constants for use
 * in the <code>IPP</code> and <code>CUPS</code> classes.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
public class IPPDefs
{

  /*
   * IPP version string...
   */
  public static final byte VERSION[] = { 1, 0 };


  /*
   * IPP registered port number...  This is the default value - applications
   * should use the ippPort() function so that you can customize things in
   * /etc/services if needed!
   */
  public static final int PORT = 631;


  /*
   * Common limits...
   */
  public static final int MAX_NAME   = 256;
  public static final int MAX_VALUES = 10;    /* Now just an allocation 
                                                 increment */


  /*  
   *  Format tags for attribute formats... 
   */
  public static final int TAG_ZERO = 0x00;
  public static final int TAG_OPERATION = 0x01;
  public static final int TAG_JOB = 0x02;
  public static final int TAG_END = 0x03;
  public static final int TAG_PRINTER = 0x04;
  public static final int TAG_UNSUPPORTED_GROUP = 0x05;
  public static final int TAG_SUBSCRIPTION = 0x06;
  public static final int TAG_EVENT_NOTIFICATION = 0x07;
  public static final int TAG_UNSUPPORTED_VALUE = 0x10;
  public static final int TAG_DEFAULT = 0x11;
  public static final int TAG_UNKNOWN = 0x12;
  public static final int TAG_NOVALUE = 0x13;
  public static final int TAG_NOTSETTABLE = 0x15;
  public static final int TAG_DELETEATTR = 0x16;
  public static final int TAG_ADMINDEFINE = 0x17;
  public static final int TAG_INTEGER = 0x21;
  public static final int TAG_BOOLEAN = 0x22;
  public static final int TAG_ENUM = 0x23;
  public static final int TAG_STRING = 0x30;
  public static final int TAG_DATE = 0x31;
  public static final int TAG_RESOLUTION = 0x32;
  public static final int TAG_RANGE = 0x33;
  public static final int TAG_BEGIN_COLLECTION = 0x34;
  public static final int TAG_TEXTLANG = 0x35;
  public static final int TAG_NAMELANG = 0x36;
  public static final int TAG_END_COLLECTION = 0x37;
  public static final int TAG_TEXT = 0x41;
  public static final int TAG_NAME = 0x42;
  public static final int TAG_KEYWORD = 0x44;
  public static final int TAG_URI = 0x45;
  public static final int TAG_URISCHEME = 0x46;
  public static final int TAG_CHARSET = 0x47;
  public static final int TAG_LANGUAGE = 0x48;
  public static final int TAG_MIMETYPE = 0x49;
  public static final int TAG_MEMBERNAME = 0x4A;
  public static final int TAG_MASK = 0x7FFFFFFF;
  public static final int TAG_COPY = 0x80000001;


  /*  
   *  Resolution units... 
   */
  public static final int RES_PER_INCH = 0x03;
  public static final int RES_PER_CM = 0x04;


  /* 
   *  Finishings... 
   */
  public static final int FINISHINGS_NONE = 0x03;
  public static final int FINISHINGS_STAPLE = 0x04;
  public static final int FINISHINGS_PUNCH = 0x05;
  public static final int FINISHINGS_COVER = 0x06;
  public static final int FINISHINGS_BIND = 0x07;
  public static final int FINISHINGS_SADDLE_STITCH = 0x08;
  public static final int FINISHINGS_EDGE_STITCH = 0x09;
  public static final int FINISHINGS_FOLD = 0x0A;
  public static final int FINISHINGS_TRIM = 0x0B;
  public static final int FINISHINGS_BALE = 0x0C;
  public static final int FINISHINGS_BOOKLET_MAKER = 0x0D;
  public static final int FINISHINGS_JOB_OFFSET = 0x0E;
  public static final int FINISHINGS_STAPLE_TOP_LEFT = 0x14;
  public static final int FINISHINGS_STAPLE_BOTTOM_LEFT = 0x15;
  public static final int FINISHINGS_STAPLE_TOP_RIGHT = 0x16;
  public static final int FINISHINGS_STAPLE_BOTTOM_RIGHT = 0x17;
  public static final int FINISHINGS_EDGE_STITCH_LEFT = 0x18;
  public static final int FINISHINGS_EDGE_STITCH_TOP = 0x19;
  public static final int FINISHINGS_EDGE_STITCH_RIGHT = 0x1A;
  public static final int FINISHINGS_EDGE_STITCH_BOTTOM = 0x1B;
  public static final int FINISHINGS_STAPLE_DUAL_LEFT = 0x1C;
  public static final int FINISHINGS_STAPLE_DUAL_TOP = 0x1D;
  public static final int FINISHINGS_STAPLE_DUAL_RIGHT = 0x1E;
  public static final int FINISHINGS_STAPLE_DUAL_BOTTOM = 0x1F;
  public static final int FINISHINGS_BIND_LEFT = 0x32;
  public static final int FINISHINGS_BIND_TOP = 0x33;
  public static final int FINISHINGS_BIND_RIGHT = 0x34;
  public static final int FINISHINGS_BIND_BOTTOM = 0x35;


  /* 
   *  Orientation... 
   */
  public static final int PORTRAIT = 0x03;
  public static final int LANDSCAPE = 0x04;
  public static final int REVERSE_LANDSCAPE = 0x05;
  public static final int REVERSE_PORTRAIT = 0x06;


  /* 
   *  Qualities... 
   */
  public static final int QUALITY_DRAFT = 0x03;
  public static final int QUALITY_NORMAL = 0x04;
  public static final int QUALITY_HIGH = 0x05;


  /* 
   *  Job States.... 
   */
  public static final int JOB_PENDING = 0x03;
  public static final int JOB_HELD = 0x04;
  public static final int JOB_PROCESSING = 0x05;
  public static final int JOB_STOPPED = 0x06;
  public static final int JOB_CANCELLED = 0x07;
  public static final int JOB_ABORTED = 0x08;
  public static final int JOB_COMPLETED = 0x09;


  /* 
   *  Printer States.... 
   */
  public static final int PRINTER_IDLE = 0x03;
  public static final int PRINTER_PROCESSING = 0x04;
  public static final int PRINTER_STOPPED = 0x05;


  /* 
   *  IPP states... 
   */
  public static final int ERROR = 0xFFFFFFFF;
  public static final int IDLE = 0x00;
  public static final int HEADER = 0x01;
  public static final int ATTRIBUTE = 0x02;
  public static final int DATA = 0x03;


  /* 
   *  IPP operations... 
   */
  public static final int PRINT_JOB = 0x02;
  public static final int PRINT_URI = 0x03;
  public static final int VALIDATE_JOB = 0x04;
  public static final int CREATE_JOB = 0x05;
  public static final int SEND_DOCUMENT = 0x06;
  public static final int SEND_URI = 0x07;
  public static final int CANCEL_JOB = 0x08;
  public static final int GET_JOB_ATTRIBUTES = 0x09;
  public static final int GET_JOBS = 0x0A;
  public static final int GET_PRINTER_ATTRIBUTES = 0x0B;
  public static final int HOLD_JOB = 0x0C;
  public static final int RELEASE_JOB = 0x0D;
  public static final int RESTART_JOB = 0x0E;
  public static final int PAUSE_PRINTER = 0x10;
  public static final int RESUME_PRINTER = 0x11;
  public static final int PURGE_JOBS = 0x12;
  public static final int SET_PRINTER_ATTRIBUTES = 0x13;
  public static final int SET_JOB_ATTRIBUTES = 0x14;
  public static final int GET_PRINTER_SUPPORTED_VALUES = 0x15;
  public static final int CREATE_PRINTER_SUBSCRIPTION = 0x16;
  public static final int CREATE_JOB_SUBSCRIPTION = 0x17;
  public static final int GET_SUBSCRIPTION_ATTRIBUTES = 0x18;
  public static final int GET_SUBSCRIPTIONS = 0x19;
  public static final int RENEW_SUBSCRIPTION = 0x1A;
  public static final int CANCEL_SUBSCRIPTION = 0x1B;
  public static final int GET_NOTIFICATIONS = 0x1C;
  public static final int SEND_NOTIFICATIONS = 0x1D;
  public static final int GET_PRINT_SUPPORT_FILES = 0x21;
  public static final int ENABLE_PRINTER = 0x22;
  public static final int DISABLE_PRINTER = 0x23;
  public static final int PAUSE_PRINTER_AFTER_CURRENT_JOB = 0x24;
  public static final int HOLD_NEW_JOBS = 0x25;
  public static final int RELEASE_HELD_NEW_JOBS = 0x26;
  public static final int DEACTIVATE_PRINTER = 0x27;
  public static final int ACTIVATE_PRINTER = 0x28;
  public static final int RESTART_PRINTER = 0x29;
  public static final int SHUTDOWN_PRINTER = 0x2A;
  public static final int STARTUP_PRINTER = 0x2B;
  public static final int REPROCESS_JOB = 0x2C;
  public static final int CANCEL_CURRENT_JOB = 0x2D;
  public static final int SUSPEND_CURRENT_JOB = 0x2E;
  public static final int RESUME_JOB = 0x2F;
  public static final int PROMOTE_JOB = 0x30;
  public static final int SCHEDULE_JOB_AFTER = 0x31;
  public static final int PRIVATE = 0x4000;
  public static final int CUPS_GET_DEFAULT = 0x4001;
  public static final int CUPS_GET_PRINTERS = 0x4002;
  public static final int CUPS_ADD_PRINTER = 0x4003;
  public static final int CUPS_DELETE_PRINTER = 0x4004;
  public static final int CUPS_GET_CLASSES = 0x4005;
  public static final int CUPS_ADD_CLASS = 0x4006;
  public static final int CUPS_DELETE_CLASS = 0x4007;
  public static final int CUPS_ACCEPT_JOBS = 0x4008;
  public static final int CUPS_REJECT_JOBS = 0x4009;
  public static final int CUPS_SET_DEFAULT = 0x400A;
  public static final int CUPS_GET_DEVICES = 0x400B;
  public static final int CUPS_GET_PPDS = 0x400C;
  public static final int CUPS_MOVE_JOB = 0x400D;
  public static final int CUPS_ADD_DEVICE = 0x400E;
  public static final int CUPS_DELETE_DEVICE = 0x400F;



  /* 
   *  IPP status codes... 
   */
  public static final int OK = 0x00;
  public static final int OK_SUBST = 0x01;
  public static final int OK_CONFLICT = 0x02;
  public static final int OK_IGNORED_SUBSCRIPTIONS = 0x03;
  public static final int OK_IGNORED_NOTIFICATIONS = 0x04;
  public static final int OK_TOO_MANY_EVENTS = 0x05;
  public static final int OK_BUT_CANCEL_SUBSCRIPTION = 0x06;
  public static final int REDIRECTION_OTHER_SITE = 0x300;
  public static final int BAD_REQUEST = 0x400;
  public static final int FORBIDDEN = 0x401;
  public static final int NOT_AUTHENTICATED = 0x402;
  public static final int NOT_AUTHORIZED = 0x403;
  public static final int NOT_POSSIBLE = 0x404;
  public static final int TIMEOUT = 0x405;
  public static final int NOT_FOUND = 0x406;
  public static final int GONE = 0x407;
  public static final int REQUEST_ENTITY = 0x408;
  public static final int REQUEST_VALUE = 0x409;
  public static final int DOCUMENT_FORMAT = 0x40A;
  public static final int ATTRIBUTES = 0x40B;
  public static final int URI_SCHEME = 0x40C;
  public static final int CHARSET = 0x40D;
  public static final int CONFLICT = 0x40E;
  public static final int COMPRESSION_NOT_SUPPORTED = 0x40F;
  public static final int COMPRESSION_ERROR = 0x410;
  public static final int DOCUMENT_FORMAT_ERROR = 0x411;
  public static final int DOCUMENT_ACCESS_ERROR = 0x412;
  public static final int ATTRIBUTES_NOT_SETTABLE = 0x413;
  public static final int IGNORED_ALL_SUBSCRIPTIONS = 0x414;
  public static final int TOO_MANY_SUBSCRIPTIONS = 0x415;
  public static final int IGNORED_ALL_NOTIFICATIONS = 0x416;
  public static final int PRINT_SUPPORT_FILE_NOT_FOUND = 0x417;
  public static final int INTERNAL_ERROR = 0x500;
  public static final int OPERATION_NOT_SUPPORTED = 0x501;
  public static final int SERVICE_UNAVAILABLE = 0x502;
  public static final int VERSION_NOT_SUPPORTED = 0x503;
  public static final int DEVICE_ERROR = 0x504;
  public static final int TEMPORARY_ERROR = 0x505;
  public static final int NOT_ACCEPTING = 0x506;
  public static final int PRINTER_BUSY = 0x507;
  public static final int ERROR_JOB_CANCELLED = 0x508;
  public static final int MULTIPLE_JOBS_NOT_SUPPORTED = 0x509;
  public static final int PRINTER_IS_DEACTIVATED = 0x50A;

}


