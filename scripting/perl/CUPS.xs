#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <cups/cups.h>
#include <cups/http.h>
#include <cups/ipp.h>
#include <cups/language.h>
#include <cups/ppd.h>

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static double
constant_PPD_M(char *name, int len, int arg)
{
    if (5 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 3]) {
    case 'L':
	if (strEQ(name + 5, "AX_LINE")) {	/* PPD_M removed */
#ifdef PPD_MAX_LINE
	    return PPD_MAX_LINE;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 5, "AX_NAME")) {	/* PPD_M removed */
#ifdef PPD_MAX_NAME
	    return PPD_MAX_NAME;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 5, "AX_TEXT")) {	/* PPD_M removed */
#ifdef PPD_MAX_TEXT
	    return PPD_MAX_TEXT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_P(char *name, int len, int arg)
{
    if (1 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 3]) {
    case 'M':
	if (!strnEQ(name + 1,"PD_", 3))
	    break;
	return constant_PPD_M(name, len, arg);
    case 'V':
	if (strEQ(name + 1, "PD_VERSION")) {	/* P removed */
#ifdef PPD_VERSION
	    return PPD_VERSION;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_H(char *name, int len, int arg)
{
    if (1 + 8 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 8]) {
    case 'B':
	if (strEQ(name + 1, "TTP_MAX_BUFFER")) {	/* H removed */
#ifdef HTTP_MAX_BUFFER
	    return HTTP_MAX_BUFFER;
#else
	    goto not_there;
#endif
	}
    case 'H':
	if (strEQ(name + 1, "TTP_MAX_HOST")) {	/* H removed */
#ifdef HTTP_MAX_HOST
	    return HTTP_MAX_HOST;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 1, "TTP_MAX_URI")) {	/* H removed */
#ifdef HTTP_MAX_URI
	    return HTTP_MAX_URI;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 1, "TTP_MAX_VALUE")) {	/* H removed */
#ifdef HTTP_MAX_VALUE
	    return HTTP_MAX_VALUE;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_IPP_M(char *name, int len, int arg)
{
    if (5 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 3]) {
    case 'N':
	if (strEQ(name + 5, "AX_NAME")) {	/* IPP_M removed */
#ifdef IPP_MAX_NAME
	    return IPP_MAX_NAME;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 5, "AX_VALUES")) {	/* IPP_M removed */
#ifdef IPP_MAX_VALUES
	    return IPP_MAX_VALUES;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_I(char *name, int len, int arg)
{
    if (1 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 3]) {
    case 'M':
	if (!strnEQ(name + 1,"PP_", 3))
	    break;
	return constant_IPP_M(name, len, arg);
    case 'P':
	if (strEQ(name + 1, "PP_PORT")) {	/* I removed */
#ifdef IPP_PORT
	    return IPP_PORT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_C(char *name, int len, int arg)
{
    if (1 + 4 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 4]) {
    case 'D':
	if (strEQ(name + 1, "UPS_DATE_ANY")) {	/* C removed */
#ifdef CUPS_DATE_ANY
	    return CUPS_DATE_ANY;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 1, "UPS_VERSION")) {	/* C removed */
#ifdef CUPS_VERSION
	    return CUPS_VERSION;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant(char *name, int len, int arg)
{
    errno = 0;
    switch (name[0 + 0]) {
    case 'C':
	return constant_C(name, len, arg);
    case 'H':
	return constant_H(name, len, arg);
    case 'I':
	return constant_I(name, len, arg);
    case 'P':
	return constant_P(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}


MODULE = CUPS		PACKAGE = CUPS		


double
constant(sv,arg)
    PREINIT:
	STRLEN		len;
    INPUT:
	SV *		sv
	char *		s = SvPV(sv, len);
	int		arg
    CODE:
	RETVAL = constant(s,len,arg);
    OUTPUT:
	RETVAL

