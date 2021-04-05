/*
 * IPP data file parsing functions.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "ipp-private.h"
#include "string-private.h"
#include "debug-internal.h"


/*
 * '_ippVarsDeinit()' - Free all memory associated with the IPP variables.
 */

void
_ippVarsDeinit(_ipp_vars_t *v)		/* I - IPP variables */
{
  if (v->uri)
  {
    free(v->uri);
    v->uri = NULL;
  }

  cupsFreeOptions(v->num_vars, v->vars);
  v->num_vars = 0;
  v->vars     = NULL;
}


/*
 * '_ippVarsExpand()' - Expand variables in the source string.
 */

void
_ippVarsExpand(_ipp_vars_t *v,		/* I - IPP variables */
               char        *dst,	/* I - Destination buffer */
               const char  *src,	/* I - Source string */
               size_t      dstsize)	/* I - Destination buffer size */
{
  char		*dstptr,		/* Pointer into destination */
		*dstend,		/* End of destination */
		temp[256],		/* Temporary string */
		*tempptr;		/* Pointer into temporary string */
  const char	*value;			/* Value to substitute */


  dstptr = dst;
  dstend = dst + dstsize - 1;

  while (*src && dstptr < dstend)
  {
    if (*src == '$')
    {
     /*
      * Substitute a string/number...
      */

      if (!strncmp(src, "$$", 2))
      {
        value = "$";
	src   += 2;
      }
      else if (!strncmp(src, "$ENV[", 5))
      {
	strlcpy(temp, src + 5, sizeof(temp));

	for (tempptr = temp; *tempptr; tempptr ++)
	  if (*tempptr == ']')
	    break;

        if (*tempptr)
	  *tempptr++ = '\0';

	value = getenv(temp);
        src   += tempptr - temp + 5;
      }
      else
      {
        if (src[1] == '{')
	{
	  src += 2;
	  strlcpy(temp, src, sizeof(temp));
	  if ((tempptr = strchr(temp, '}')) != NULL)
	    *tempptr = '\0';
	  else
	    tempptr = temp + strlen(temp);
	}
	else
	{
	  strlcpy(temp, src + 1, sizeof(temp));

	  for (tempptr = temp; *tempptr; tempptr ++)
	    if (!isalnum(*tempptr & 255) && *tempptr != '-' && *tempptr != '_')
	      break;

	  if (*tempptr)
	    *tempptr = '\0';
        }

        value = _ippVarsGet(v, temp);

        src += tempptr - temp + 1;
      }

      if (value)
      {
        strlcpy(dstptr, value, (size_t)(dstend - dstptr + 1));
	dstptr += strlen(dstptr);
      }
    }
    else
      *dstptr++ = *src++;
  }

  *dstptr = '\0';
}


/*
 * '_ippVarsGet()' - Get a variable string.
 */

const char *				/* O - Value or @code NULL@ if not set */
_ippVarsGet(_ipp_vars_t *v,		/* I - IPP variables */
            const char  *name)		/* I - Variable name */
{
  if (!v)
    return (NULL);
  else if (!strcmp(name, "uri"))
    return (v->uri);
  else if (!strcmp(name, "uriuser") || !strcmp(name, "username"))
    return (v->username[0] ? v->username : NULL);
  else if (!strcmp(name, "scheme") || !strcmp(name, "method"))
    return (v->scheme);
  else if (!strcmp(name, "hostname"))
    return (v->host);
  else if (!strcmp(name, "port"))
    return (v->portstr);
  else if (!strcmp(name, "resource"))
    return (v->resource);
  else if (!strcmp(name, "user"))
    return (cupsUser());
  else
    return (cupsGetOption(name, v->num_vars, v->vars));
}


/*
 * '_ippVarsInit()' - Initialize .
 */

void
_ippVarsInit(_ipp_vars_t      *v,	/* I - IPP variables */
             _ipp_fattr_cb_t  attrcb,	/* I - Attribute (filter) callback */
             _ipp_ferror_cb_t errorcb,	/* I - Error callback */
             _ipp_ftoken_cb_t tokencb)	/* I - Token callback */
{
  memset(v, 0, sizeof(_ipp_vars_t));

  v->attrcb  = attrcb;
  v->errorcb = errorcb;
  v->tokencb = tokencb;
}


/*
 * '_ippVarsPasswordCB()' - Password callback using the IPP variables.
 */

const char *				/* O - Password string or @code NULL@ */
_ippVarsPasswordCB(
    const char *prompt,			/* I - Prompt string (not used) */
    http_t     *http,			/* I - HTTP connection (not used) */
    const char *method,			/* I - HTTP method (not used) */
    const char *resource,		/* I - Resource path (not used) */
    void       *user_data)		/* I - IPP variables */
{
  _ipp_vars_t	*v = (_ipp_vars_t *)user_data;
					/* I - IPP variables */


  (void)prompt;
  (void)http;
  (void)method;
  (void)resource;

  if (v->username[0] && v->password && v->password_tries < 3)
  {
    v->password_tries ++;

    cupsSetUser(v->username);

    return (v->password);
  }
  else
  {
    return (NULL);
  }
}


/*
 * '_ippVarsSet()' - Set an IPP variable.
 */

int					/* O - 1 on success, 0 on failure */
_ippVarsSet(_ipp_vars_t *v,		/* I - IPP variables */
            const char  *name,		/* I - Variable name */
            const char  *value)		/* I - Variable value */
{
  if (!strcmp(name, "uri"))
  {
    char	uri[1024];		/* New printer URI */
    char	resolved[1024];		/* Resolved mDNS URI */

    if (strstr(value, "._tcp"))
    {
     /*
      * Resolve URI...
      */

      if (!_httpResolveURI(value, resolved, sizeof(resolved), _HTTP_RESOLVE_DEFAULT, NULL, NULL))
        return (0);

      value = resolved;
    }

    if (httpSeparateURI(HTTP_URI_CODING_ALL, value, v->scheme, sizeof(v->scheme), v->username, sizeof(v->username), v->host, sizeof(v->host), &(v->port), v->resource, sizeof(v->resource)) < HTTP_URI_STATUS_OK)
      return (0);

    if (v->username[0])
    {
      if ((v->password = strchr(v->username, ':')) != NULL)
	*(v->password)++ = '\0';
    }

    snprintf(v->portstr, sizeof(v->portstr), "%d", v->port);

    if (v->uri)
      free(v->uri);

    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), v->scheme, NULL, v->host, v->port, v->resource);
    v->uri = strdup(uri);

    return (v->uri != NULL);
  }
  else
  {
    v->num_vars = cupsAddOption(name, value, v->num_vars, &v->vars);
    return (1);
  }
}
