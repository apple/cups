---
title: A Word About Deprecation
layout: post
permalink: /blog/:year-:month-:day-:title.html
excerpt_separator: <!--more-->
---

We periodically deprecate functionality that either is no longer necessary or
prevents us from improving CUPS.  *Deprecated functionality continues to work*,
often for years, as we help users and developers migrate away from it.

<!--more-->

When we deprecate something:

- We announce the deprecation as far in advance as possible,
- We display a warning that the functionality is going away in a future release
  of CUPS, and
- We help developers and users migrate to any replacement functionality, if
  applicable.

Deprecation is a necessary step prior to removal from CUPS.  *Deprecated items
are still functional until removed.*

After a transition period, deprecated items are removed from CUPS.  Deprecated
CUPS APIs are never fully removed from shared libraries - non-functional stubs
remain - in order to preserve binary compatibility.

We've had some hard exceptions over the years:

- Security issues forced us to do a hard transition of some `cupsd.conf`
  directives to `cups-files.conf`,
- Security issues forced us to drop interface script support, and
- Performance and architectural issues forced us to drop CUPS browsing before
  Avahi was fully supported/deployed.

If you have any questions about our project's deprecation process, please feel
free to contact us on the "cups-devel" list.
