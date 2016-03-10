---
title: New standalone ipptool binaries released
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

The latest version of the standalone binaries for CUPS ipptool is now available for download from:

    http://www.cups.org/software.html

The new release includes more IPP conformance tests and documents, provides a summary of results at the end of a test run, adds the ability to repeat tests when certain criteria are met, and fixes a number of bugs. Changes include:

- Test output now includes a summary and overall score at the end.
- The MATCH-VALUE predicate now correctly deals with a failed EXPECT condition.
- The IPP/1.1 test suite now looks for legacy media names and uses them if the corresponding PWG standard names are not present.
- The IPP/1.1 test suite now tests the Print-Job+Release-Job when the printer supports the job-hold-until attribute, Hold-Job operation, and Release-Job operation.

Changes from the previously unannounced release include:

- Fixes for HTTP chunking, timeout, and encryption issues reported by various users.
- Greatly improved IPP tests with added IPP/2.2 tests.
- New test documents - 1-page and 4-page mixed A4/Letter PDF/PS and a couple JPEGs.
- New REPEAT directives to programmatically repeat tests as needed.

Enjoy!
