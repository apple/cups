---
title: How Do I Control The Number Of Jobs Kept On My System?
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

The MaxJobs directive controls the maximum number of jobs that are kept in memory. Once the number of jobs reaches the limit, the oldest completed job is automatically purged from the system to make room for the new one. If all of the known jobs are still pending or active then the new job will be rejected.

Setting the maximum to 0 disables this functionality. The default setting is 500.

You can change the MaxJobs directive by editing the cupsd.conf file.


