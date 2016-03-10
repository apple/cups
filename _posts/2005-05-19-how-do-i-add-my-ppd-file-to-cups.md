---
title: How Do I Add My PPD File To CUPS?
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

There are two ways you can add a PPD file:1) Use the lpadmin command: lpadmin -p printer -E -v device-uri -i filename.ppd                  OR2) Copy the file to /usr/share/cups/model and restart cupsd.
