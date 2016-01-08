---
title: Printing To Windows Servers With Samba
layout: post
---

<b><u>Printing to Windows Servers with Samba</b></u>


1) Download SAMBA: http://samba.org

2) Configure CUPS for Samba:

                  ln âs `which smbspool` /usr/lib/cups/backend/smb

3) The smbspool program is provided with SAMBA starting with SAMBA 2.0.6. Once you have made the link you can configure your printers with one of the following URIs:
                                  smb://servername/sharename
                                  smb://username:password@servername/sharename
                                  smb://ntdomain;username:password@servername/sharename



