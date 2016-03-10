---
title: Changing The Printing Prioity For A Queued Job
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Use the following format to accomplish this: lp -q <priority> -i <jobid>The job priority can be set from 1 to 100, where 1 is the lowest priority and 100 is the highest.The default in CUPS is 50.So, if you want to make job 12345 a job priority of 75, you'd type: lp -q 75 -i 12345
