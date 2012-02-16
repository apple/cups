/*
 * "$Id$"
 *
 *   Avahi poll implementation for the CUPS scheduler.
 *
 *   Copyright (C) 2010, 2011 Red Hat, Inc.
 *   Authors:
 *    Tim Waugh <twaugh@redhat.com>
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 *   OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#ifdef HAVE_AVAHI
#  include <avahi-client/client.h>
#  include <avahi-client/publish.h>
#endif /* HAVE_AVAHI */

#ifdef HAVE_AUTHORIZATION_H
#  include <Security/Authorization.h>
#endif /* HAVE_AUTHORIZATION_H */


#ifdef HAVE_AVAHI
typedef struct
{
    AvahiPoll api;
    cups_array_t *watched_fds;
    cups_array_t *timeouts;
} AvahiCupsPoll;
#endif /* HAVE_AVAHI */

/*
 * Prototypes...
 */

#ifdef HAVE_AVAHI
extern AvahiCupsPoll *	avahi_cups_poll_new(void);
extern void		avahi_cups_poll_free(AvahiCupsPoll *cups_poll);
extern const AvahiPoll *avahi_cups_poll_get(AvahiCupsPoll *cups_poll);
#endif /* HAVE_AVAHI */


/*
 * End of "$Id$".
 */
