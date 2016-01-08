---
title: pkpgcounter 1.74
layout: post
---

A severe bug in the PCLXL parser was fixed : it prevented PCLXL streams containing media size name (e.g. 'LETTER') instead of media size index (e.g. 0 for 'LETTER') from being parsed correctly.
