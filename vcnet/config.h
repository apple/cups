/*
 * "$Id: config.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Configuration file for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifndef _CUPS_CONFIG_H_
#define _CUPS_CONFIG_H_

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <io.h>


/*
 * Microsoft also renames the POSIX functions to _name, and introduces
 * a broken compatibility layer using the original names.  As a result,
 * random crashes can occur when, for example, strdup() allocates memory
 * from a different heap than used by malloc() and free().
 *
 * To avoid moronic problems like this, we #define the POSIX function
 * names to the corresponding non-standard Microsoft names.
 */

#define access		_access
#define close		_close
#define fileno		_fileno
#define lseek		_lseek
#define open		_open
#define read	        _read
#define snprintf 	_snprintf
#define strdup		_strdup
#define unlink		_unlink
#define vsnprintf 	_vsnprintf
#define write		_write


/*
 * Compiler stuff...
 */

#undef const
#undef __CHAR_UNSIGNED__


/*
 * Version of software...
 */

#define CUPS_SVERSION "CUPS v1.4b3"
#define CUPS_MINIMAL "CUPS/1.4b3"


/*
 * Default user and groups...
 */

#define CUPS_DEFAULT_USER	"lp"
#define CUPS_DEFAULT_GROUP	"sys"
#define CUPS_DEFAULT_SYSTEM_GROUPS "admin"
#define CUPS_DEFAULT_PRINTOPERATOR_AUTH "@admin @lpadmin"


/*
 * Default file permissions...
 */

#define CUPS_DEFAULT_CONFIG_FILE_PERM 0644
#define CUPS_DEFAULT_LOG_FILE_PERM 0644


/*
 * Default logging settings...
 */

#define CUPS_DEFAULT_LOG_LEVEL "warn"
#define CUPS_DEFAULT_ACCESS_LOG_LEVEL "actions"


/*
 * Default fatal error settings...
 */

#define CUPS_DEFAULT_FATAL_ERRORS "config"


/*
 * Default browsing settings...
 */

#define CUPS_DEFAULT_BROWSING 1
#define CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS "CUPS dnssd"
#define CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS ""
#define CUPS_DEFAULT_BROWSE_SHORT_NAMES 1
#define CUPS_DEFAULT_DEFAULT_SHARED 1
#define CUPS_DEFAULT_IMPLICIT_CLASSES 1
#define CUPS_DEFAULT_USE_NETWORK_DEFAULT 0


/*
 * Default IPP port...
 */

#define CUPS_DEFAULT_IPP_PORT 631


/*
 * Default printcap file...
 */

#define CUPS_DEFAULT_PRINTCAP ""


/*
 * Default Samba and LPD config files...
 */

#define CUPS_DEFAULT_SMB_CONFIG_FILE ""
#define CUPS_DEFAULT_LPD_CONFIG_FILE ""


/*
 * Default MaxCopies value...
 */

#define CUPS_DEFAULT_MAX_COPIES 9999


/*
 * Do we have domain socket support?
 */

#undef CUPS_DEFAULT_DOMAINSOCKET


/*
 * Where are files stored?
 *
 * Note: These are defaults, which can be overridden by environment
 *       variables at run-time...
 */

#define CUPS_BINDIR "C:/CUPS/bin"
#define CUPS_CACHEDIR "C:/CUPS/cache"
#define CUPS_DATADIR "C:/CUPS/share"
#define CUPS_DOCROOT "C:/CUPS/share/doc"
#define CUPS_FONTPATH "C:/CUPS/share/fonts"
#define CUPS_LOCALEDIR "C:/CUPS/locale"
#define CUPS_LOGDIR "C:/CUPS/logs"
#define CUPS_REQUESTS "C:/CUPS/spool"
#define CUPS_SBINDIR "C:/CUPS/sbin"
#define CUPS_SERVERBIN "C:/CUPS/lib"
#define CUPS_SERVERROOT "C:/CUPS/etc"
#define CUPS_STATEDIR "C:/CUPS/run"


/*
 * Do we have various image libraries?
 */

/* #undef HAVE_LIBPNG */
/* #undef HAVE_LIBZ */
/* #undef HAVE_LIBJPEG */
/* #undef HAVE_LIBTIFF */


/*
 * Do we have PAM stuff?
 */

#ifndef HAVE_LIBPAM
#define HAVE_LIBPAM 0
#endif /* !HAVE_LIBPAM */

/* #undef HAVE_PAM_PAM_APPL_H */
/* #undef HAVE_PAM_SET_ITEM */
/* #undef HAVE_PAM_SETCRED */


/*
 * Do we have <shadow.h>?
 */

/* #undef HAVE_SHADOW_H */


/*
 * Do we have <crypt.h>?
 */

/* #undef HAVE_CRYPT_H */


/*
 * Use <string.h>, <strings.h>, and/or <bstring.h>?
 */

#define HAVE_STRING_H 1
/* #undef HAVE_STRINGS_H */
/* #undef HAVE_BSTRING_H */


/*
 * Do we have the long long type?
 */

/* #undef HAVE_LONG_LONG */

#ifdef HAVE_LONG_LONG
#  define CUPS_LLFMT	"%lld"
#  define CUPS_LLCAST	(long long)
#else
#  define CUPS_LLFMT	"%ld"
#  define CUPS_LLCAST	(long)
#endif /* HAVE_LONG_LONG */


/*
 * Do we have the strtoll() function?
 */

/* #undef HAVE_STRTOLL */

#ifndef HAVE_STRTOLL
#  define strtoll(nptr,endptr,base) strtol((nptr), (endptr), (base))
#endif /* !HAVE_STRTOLL */


/*
 * Do we have the strXXX() functions?
 */

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define HAVE_STRNCASECMP
/* #undef HAVE_STRLCAT */
/* #undef HAVE_STRLCPY */


/*
 * Do we have the geteuid() function?
 */

/* #undef HAVE_GETEUID */


/*
 * Do we have the vsyslog() function?
 */

/* #undef HAVE_VSYSLOG */


/*
 * Do we have the (v)snprintf() functions?
 */

#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1


/*
 * What signal functions to use?
 */

/* #undef HAVE_SIGSET */
/* #undef HAVE_SIGACTION */


/*
 * What wait functions to use?
 */

/* #undef HAVE_WAITPID */
/* #undef HAVE_WAIT3 */


/*
 * Do we have the mallinfo function and malloc.h?
 */

/* #undef HAVE_MALLINFO */
/* #undef HAVE_MALLOC_H */


/*
 * Do we have the POSIX ACL functions?
 */

/* #undef HAVE_ACL_INIT */


/*
 * Do we have the langinfo.h header file?
 */

/* #undef HAVE_LANGINFO_H */


/*
 * Which encryption libraries do we have?
 */

/* #undef HAVE_CDSASSL */
/* #undef HAVE_GNUTLS */
/* #undef HAVE_LIBSSL */
/* #undef HAVE_SSL */


/*
 * What Security framework headers do we have?
 */

/* #undef HAVE_AUTHORIZATION_H */
/* #undef HAVE_SECPOLICY_H */
/* #undef HAVE_SECPOLICYPRIV_H */
/* #undef HAVE_SECBASEPRIV_H */
/* #undef HAVE_SECIDENTITYSEARCHPRIV_H */


/*
 * Do we have the SecIdentitySearchCreateWithPolicy function?
 */

/* #undef HAVE_SECIDENTITYSEARCHCREATEWITHPOLICY */


/*
 * Do we have the SLP library?
 */

/* #undef HAVE_LIBSLP */


/*
 * Do we have an LDAP library?
 */

/* #undef HAVE_LDAP */
/* #undef HAVE_OPENLDAP */
/* #undef HAVE_MOZILLA_LDAP */
/* #undef HAVE_LDAP_SSL_H */
/* #undef HAVE_LDAP_SSL */
/* #undef HAVE_LDAP_REBIND_PROC */


/*
 * Do we have libpaper?
 */

/* #undef HAVE_LIBPAPER */


/*
 * Do we have DNS Service Discovery (aka Bonjour)?
 */

/* #undef HAVE_DNSSD */


/*
 * Do we have <sys/ioctl.h>?
 */

/* #undef HAVE_SYS_IOCTL_H */


/*
 * Does the "tm" structure contain the "tm_gmtoff" member?
 */

/* #undef HAVE_TM_GMTOFF */


/*
 * Do we have rresvport_af()?
 */

/* #undef HAVE_RRESVPORT_AF */


/*
 * Do we have getaddrinfo()?
 */

#define HAVE_GETADDRINFO 1


/*
 * Do we have getnameinfo()?
 */

#define HAVE_GETNAMEINFO 1


/*
 * Do we have getifaddrs()?
 */

/* #undef HAVE_GETIFADDRS */


/*
 * Do we have hstrerror()?
 */

/* #undef HAVE_HSTRERROR */


/*
 * Do we have res_init()?
 */

/* #undef HAVE_RES_INIT */


/*
 * Do we have <resolv.h>
 */

/* #undef HAVE_RESOLV_H */


/*
 * Do we have the <sys/sockio.h> header file?
 */

/* #undef HAVE_SYS_SOCKIO_H */


/*
 * Does the sockaddr structure contain an sa_len parameter?
 */

/* #undef HAVE_STRUCT_SOCKADDR_SA_LEN */


/*
 * Do we have the AIX usersec.h header file?
 */

/* #undef HAVE_USERSEC_H */


/*
 * Do we have pthread support?
 */

/* #undef HAVE_PTHREAD_H */


/*
 * Do we have launchd support?
 */

/* #undef HAVE_LAUNCH_H */
/* #undef HAVE_LAUNCHD */
#define CUPS_DEFAULT_LAUNCHD_CONF ""


/*
 * Various scripting languages...
 */

/* #undef HAVE_JAVA */
#define CUPS_JAVA	""
/* #undef HAVE_PERL */
#define CUPS_PERL	""
/* #undef HAVE_PHP */
#define CUPS_PHP	""
/* #undef HAVE_PYTHON */
#define CUPS_PYTHON	""


/*
 * Location of the poppler/Xpdf pdftops program...
 */

/* #undef HAVE_PDFTOPS */
#define CUPS_PDFTOPS ""


/*
 * Location of the Ghostscript gs program...
 */

/* #undef HAVE_GHOSTSCRIPT */
#define CUPS_GHOSTSCRIPT ""


/*
 * Do we have Darwin's CoreFoundation and SystemConfiguration frameworks?
 */

/* #undef HAVE_COREFOUNDATION */
/* #undef HAVE_SYSTEMCONFIGURATION */


/*
 * Do we have CoreFoundation public and private headers?
 */

/* #undef HAVE_COREFOUNDATION_H */
/* #undef HAVE_CFPRIV_H */
/* #undef HAVE_CFBUNDLEPRIV_H */


/*
 * Do we have MacOSX 10.4's mbr_XXX functions()?
 */

/* #undef HAVE_MEMBERSHIP_H */
/* #undef HAVE_MEMBERSHIPPRIV_H */
/* #undef HAVE_MBR_UID_TO_UUID */


/*
 * Do we have Darwin's notify_post() header and function?
 */

/* #undef HAVE_NOTIFY_H */
/* #undef HAVE_NOTIFY_POST */


/*
 * Do we have DBUS?
 */

/* #undef HAVE_DBUS */
/* #undef HAVE_DBUS_MESSAGE_ITER_INIT_APPEND */


/*
 * Do we have the AppleTalk/at_proto.h header?
 */

/* #undef HAVE_APPLETALK_AT_PROTO_H */


/*
 * Do we have the GSSAPI support library (for Kerberos support)?
 */

/* #undef HAVE_GSSAPI */
/* #undef HAVE_GSSAPI_H */
/* #undef HAVE_GSSAPI_GSSAPI_H */
/* #undef HAVE_GSSAPI_GSSAPI_GENERIC_H */
/* #undef HAVE_GSSAPI_GSSAPI_KRB5_H */
/* #undef HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY */
/* #undef HAVE_GSS_C_NT_HOSTBASED_SERVICE */
/* #undef HAVE_KRB5_CC_NEW_UNIQUE */
/* #undef HAVE_KRB5_IPC_CLIENT_SET_TARGET_UID */
/* #undef HAVE_KRB5_H */
/* #undef HAVE_HEIMDAL */


/*
 * Default GSS service name...
 */

#define CUPS_DEFAULT_GSSSERVICENAME "ipp"


/*
 * Select/poll interfaces...
 */

/* #undef HAVE_POLL */
/* #undef HAVE_EPOLL */
/* #undef HAVE_KQUEUE */


/*
 * Do we have the <dlfcn.h> header?
 */

/* #undef HAVE_DLFCN_H */


/*
 * Do we have <sys/param.h>?
 */

/* #undef HAVE_SYS_PARAM_H */


/*
 * Do we have <sys/ucred.h>?
 */

/* #undef HAVE_SYS_UCRED_H */


/*
 * Do we have removefile()?
 */

/* #undef HAVE_REMOVEFILE */


/*
 * Do we have <sandbox.h>?
 */

/* #undef HAVE_SANDBOX_H */


/*
 * Which random number generator function to use...
 */

/* #undef HAVE_RANDOM */
/* #undef HAVE_MRAND48 */
/* #undef HAVE_LRAND48 */


/*
 * Do we have vproc_transaction_begin/end?
 */

/* #undef HAVE_VPROC_TRANSACTION_BEGIN */


/*
 * Do we have libusb?
 */

/* #undef HAVE_USB_H */


/*
 * Do we have libwrap and tcpd.h?
 */

/* #undef HAVE_TCPD_H */


#endif /* !_CUPS_CONFIG_H_ */

/*
 * End of "$Id: config.h 6649 2007-07-11 21:46:42Z mike $".
 */
