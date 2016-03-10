---
title: Setting Job Priorities
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

CUPS allows you to set job priorities from least important (1) to most important (100). You use the job-priority command to do this.If I wanted to set a job-priority of 100 (most important) to my file job.txt, I'd type:lp -o job-priority=100 job.txt
