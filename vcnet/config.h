/*
 * "$Id: config.h 12998 2015-12-02 15:09:04Z msweet $"
 *
 * Configuration file for CUPS on Windows.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
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
#include <io.h>
#include <direct.h>


/*
 * Microsoft renames the POSIX functions to _name, and introduces
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
#define mkdir(d,p)	_mkdir(d)
#define open		_open
#define read	        _read
#define rmdir		_rmdir
#define snprintf 	_snprintf
#define strdup		_strdup
#define unlink		_unlink
#define vsnprintf 	_vsnprintf
#define write		_write


/*
 * Map the POSIX strcasecmp() and strncasecmp() functions to the Win32 stricmp()
 * and strnicmp() functions...
 */

#define strcasecmp	stricmp
#define strncasecmp	strnicmp


/*
 * Map the POSIX sleep() and usleep() functions to the Win32 Sleep() function...
 */

typedef unsigned long useconds_t;
#define sleep(X)	Sleep(1000 * (X))
#define usleep(X)	Sleep((X)/1000)


/*
 * Map various parameters to Posix style system calls
 */

#  define F_OK		00
#  define W_OK		02
#  define R_OK		04
#  define O_RDONLY	_O_RDONLY
#  define O_WRONLY	_O_WRONLY
#  define O_CREAT	_O_CREAT
#  define O_TRUNC	_O_TRUNC


/*
 * Compiler stuff...
 */

#undef const
#undef __CHAR_UNSIGNED__


/*
 * Version of software...
 */

#define CUPS_SVERSION "CUPS v2.1.2"
#define CUPS_MINIMAL "CUPS/2.1.2"


/*
 * Default user and groups...
 */

#define CUPS_DEFAULT_USER	""
#define CUPS_DEFAULT_GROUP	""
#define CUPS_DEFAULT_SYSTEM_GROUPS ""
#define CUPS_DEFAULT_PRINTOPERATOR_AUTH ""


/*
 * Default file permissions...
 */

#define CUPS_DEFAULT_CONFIG_FILE_PERM 0640
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
#define CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS ""
#define CUPS_DEFAULT_DEFAULT_SHARED 1


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
 * Do we have domain socket support, and if so what is the default one?
 */

#undef CUPS_DEFAULT_DOMAINSOCKET


/*
 * Default WebInterface value...
 */

#undef CUPS_DEFAULT_WEBIF


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
 * Do we have posix_spawn?
 */

/* #undef HAVE_POSIX_SPAWN */


/*
 * Do we have ZLIB?
 */

#define HAVE_LIBZ 1
#define HAVE_INFLATECOPY 1


/*
 * Do we have PAM stuff?
 */

#define HAVE_LIBPAM 0
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
 * Use <stdint.h>?
 */

/* #undef HAVE_STDINT_H */


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

#define HAVE_STRDUP 1
/* #undef HAVE_STRLCAT */
/* #undef HAVE_STRLCPY */


/*
 * Do we have the geteuid() function?
 */

/* #undef HAVE_GETEUID */


/*
 * Do we have the setpgid() function?
 */

/* #undef HAVE_SETPGID */


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
#define HAVE_SSPISSL
#define HAVE_SSL


/*
 * Do we have the gnutls_transport_set_pull_timeout_function function?
 */

/* #undef HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION */


/*
 * Do we have the gnutls_priority_set_direct function?
 */

/* #undef HAVE_GNUTLS_PRIORITY_SET_DIRECT */


/*
 * What Security framework headers do we have?
 */

/* #undef HAVE_AUTHORIZATION_H */
/* #undef HAVE_SECBASEPRIV_H */
/* #undef HAVE_SECCERTIFICATE_H */
/* #undef HAVE_SECIDENTITYSEARCHPRIV_H */
/* #undef HAVE_SECITEM_H */
/* #undef HAVE_SECITEMPRIV_H */
/* #undef HAVE_SECPOLICY_H */
/* #undef HAVE_SECPOLICYPRIV_H */
/* #undef HAVE_SECURETRANSPORTPRIV_H */


/*
 * Do we have the cssmErrorString function?
 */

/* #undef HAVE_CSSMERRORSTRING */


/*
 * Do we have the SecGenerateSelfSignedCertificate function?
 */

/* #undef HAVE_SECGENERATESELFSIGNEDCERTIFICATE */


/*
 * Do we have the SecKeychainOpen function?
 */

/* #undef HAVE_SECKEYCHAINOPEN */


/*
 * Do we have libpaper?
 */

/* #undef HAVE_LIBPAPER */


/*
 * Do we have mDNSResponder for DNS Service Discovery (aka Bonjour)?
 */

#define HAVE_DNSSD 1


/*
 * Do we have Avahi for DNS Service Discovery (aka Bonjour)?
 */

#undef HAVE_AVAHI


/*
 * Do we have <sys/ioctl.h>?
 */

#undef HAVE_SYS_IOCTL_H


/*
 * Does the "stat" structure contain the "st_gen" member?
 */

/* #undef HAVE_ST_GEN */


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
 * Do we have pthread support?
 */

/* #undef HAVE_PTHREAD_H */


/*
 * Do we have launchd support?
 */

/* #undef HAVE_LAUNCH_H */
/* #undef HAVE_LAUNCHD */


/*
 * Do we have systemd support?
 */

/* #undef HAVE_SYSTEMD */


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
/* #undef HAVE_PDFTOPS_WITH_ORIGPAGESIZES */
#define CUPS_PDFTOPS ""


/*
 * Location of the Ghostscript gs program...
 */

/* #undef HAVE_GHOSTSCRIPT */
#define CUPS_GHOSTSCRIPT ""


/*
 * Do we have CoreFoundation public and private headers?
 */

/* #undef HAVE_COREFOUNDATION_H */
/* #undef HAVE_CFPRIV_H */
/* #undef HAVE_CFBUNDLEPRIV_H */


/*
 * Do we have ApplicationServices public headers?
 */

/* #undef HAVE_APPLICATIONSERVICES_H */


/*
 * Do we have the SCDynamicStoreCopyComputerName function?
 */

/* #undef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME */


/*
 * Do we have OS X 10.4's mbr_XXX functions?
 */

/* #undef HAVE_MEMBERSHIP_H */
/* #undef HAVE_MEMBERSHIPPRIV_H */
/* #undef HAVE_MBR_UID_TO_UUID */


/*
 * Do we have Darwin's notify_post header and function?
 */

/* #undef HAVE_NOTIFY_H */
/* #undef HAVE_NOTIFY_POST */


/*
 * Do we have DBUS?
 */

/* #undef HAVE_DBUS */
/* #undef HAVE_DBUS_MESSAGE_ITER_INIT_APPEND */
/* #undef HAVE_DBUS_THREADS_INIT */


/*
 * Do we have the GSSAPI support library (for Kerberos support)?
 */

/* #undef HAVE_GSS_ACQUIRE_CRED_EX_F */
/* #undef HAVE_GSS_C_NT_HOSTBASED_SERVICE */
/* #undef HAVE_GSS_GSSAPI_H */
/* #undef HAVE_GSS_GSSAPI_SPI_H */
/* #undef HAVE_GSSAPI */
/* #undef HAVE_GSSAPI_GSSAPI_H */
/* #undef HAVE_GSSAPI_H */


/*
 * Default GSS service name...
 */

#define CUPS_DEFAULT_GSSSERVICENAME "host"


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

/* #undef HAVE_ARC4RANDOM */
/* #undef HAVE_RANDOM */
/* #undef HAVE_LRAND48 */

#ifdef HAVE_ARC4RANDOM
#  define CUPS_RAND() arc4random()
#  define CUPS_SRAND(v) arc4random_stir()
#elif defined(HAVE_RANDOM)
#  define CUPS_RAND() random()
#  define CUPS_SRAND(v) srandom(v)
#elif defined(HAVE_LRAND48)
#  define CUPS_RAND() lrand48()
#  define CUPS_SRAND(v) srand48(v)
#else
#  define CUPS_RAND() rand()
#  define CUPS_SRAND(v) srand(v)
#endif /* HAVE_ARC4RANDOM */


/*
 * Do we have vproc_transaction_begin/end?
 */

/* #undef HAVE_VPROC_TRANSACTION_BEGIN */


/*
 * Do we have libusb?
 */

/* #undef HAVE_LIBUSB */


/*
 * Do we have libwrap and tcpd.h?
 */

/* #undef HAVE_TCPD_H */


/*
 * Do we have <iconv.h>?
 */

/* #undef HAVE_ICONV_H */


/*
 * Do we have statfs or statvfs and one of the corresponding headers?
 */

/* #undef HAVE_STATFS */
/* #undef HAVE_STATVFS */
/* #undef HAVE_SYS_MOUNT_H */
/* #undef HAVE_SYS_STATFS_H */
/* #undef HAVE_SYS_STATVFS_H */
/* #undef HAVE_SYS_VFS_H */


/*
 * Location of OS X localization bundle, if any.
 */

/* #undef CUPS_BUNDLEDIR */


/*
 * Do we have XPC?
 */

/* #undef HAVE_XPC */
/* #undef HAVE_XPC_PRIVATE_H */


/*
 * Do we have Mini-XML?
 */

/* #undef HAVE_MXML_H */


/*
 * Do we have the C99 abs() function?
 */

/* #undef HAVE_ABS */
#if !defined(HAVE_ABS) && !defined(abs)
#  if defined(__GNUC__) || __STDC_VERSION__ >= 199901L
#    define abs(x) _cups_abs(x)
static inline int _cups_abs(int i) { return (i < 0 ? -i : i); }
#  elif defined(_MSC_VER)
#    define abs(x) _cups_abs(x)
static __inline int _cups_abs(int i) { return (i < 0 ? -i : i); }
#  else
#    define abs(x) ((x) < 0 ? -(x) : (x))
#  endif /* __GNUC__ || __STDC_VERSION__ */
#endif /* !HAVE_ABS && !abs */

#endif /* !_CUPS_CONFIG_H_ */

/*
 * End of "$Id: config.h 12998 2015-12-02 15:09:04Z msweet $".
 */
