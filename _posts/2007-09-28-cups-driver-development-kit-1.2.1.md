---
title: CUPS Driver Development Kit 1.2.1
layout: post
---

CUPS DDK 1.2.1 is now available for download from the CUPS web site and fixes a number of issues in the PPD compiler. Changes include:

- "#include foo" was (incorrectly) treated like "{ #include foo }" (Issue #2514)
- Color profiles and other localizable attributes were not localized in generated PPD files (Issue #2507)
- Page sizes were not properly localized in multi-language PPD files (Issue #2524)
- "#include <file name>" did not work (Issue #2506)
- The ppdpo utility did not include localizable attributes (Issue #2479)
- The ppdc utility did not add a newline at the end of boolean and keyword attributes (Issue #2481)
- The ppdc utility incorrectly wrote Product attributes twice.


