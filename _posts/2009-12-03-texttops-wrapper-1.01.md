---
title: texttops wrapper 1.01
layout: post
---

The nstexttopdf filter seems to accept only MacRoman encoded text. 

To adapt the wrapper script to this "feature", change the statement

binmode (FILO, ":utf8");

to

binmode (FILO, ":encoding(MacRoman)");

Be aware that unicode characters which cannot mapped to a mac roman code point are output as "\x{hhll}" where hhll is the hex code of the utf8 character.
