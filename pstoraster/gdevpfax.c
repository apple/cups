/* Copyright (C) 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gdevpfax.c */
/* Generic PostScript system compatible fax support */
#include "gdevprn.h"
#include "gsparam.h"
#include "gxiodev.h"

/* This code defines a %Fax% IODevice, and also provides decoding for */
/* the FaxOptions dictionary in a page device. */
/* It is still fragmentary and incomplete. */

#define limited_string(len)\
  struct { byte data[len]; uint size; }

/* ------ %Fax% implementation ------ */

/* Define the structure for the Fax IODevice state. */
typedef struct gx_io_device_fax_s {
	gx_io_device_common;
	bool ActivityReport;
	bool DefaultCaptionOn;
	bool DefaultConfirmOn;
	bool DefaultCoversOn;
	int DefaultResolution;
	int DefaultRetryCount;
	int DefaultRetryInterval;
	int DialToneWaitPeriod;
	limited_string(20) ID;
	long MaxFaxBuffer;
	limited_string(32) PostScriptPassword;
	bool ReceivePostScript;
	int Rings;
	int ServiceEnable;
	int Speaker;
	const char *StorageDevice;
	bool WaitForDialTone;
} gx_io_device_fax;

private iodev_proc_get_params(fax_get_params);
private iodev_proc_put_params(fax_put_params);
const gx_io_device_fax gs_iodev_Fax =
  { "%Fax%", "Parameters",
     { iodev_no_init, iodev_no_open_device, iodev_no_open_file,
       iodev_os_fopen, iodev_os_fclose,
       iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
       iodev_no_enumerate_files, NULL, NULL,
       fax_get_params, fax_put_params
     },
    false,				/* A */
    true, true, true, 1, 0, 3, 1,	/* D */
    { { 0 }, 0 },			/* I */
    350000,				/* M */
    { { 0 }, 0 },			/* P */
    true, 4 /* ? */,			/* R */
    3, 1, "%ram%",			/* S */
    true				/* W */
  };

/* The following code is shared between get and put parameters. */
typedef struct fax_strings_s {
	gs_param_string id, pwd, sd;
} fax_strings;
private int
fax_xfer_params(register gx_io_device_fax *faxdev, gs_param_list *plist,
  fax_strings *pfs)
{	int code;
	pfs->id.data = faxdev->ID.data, pfs->id.size = faxdev->ID.size,
	  pfs->id.persistent = false;
	pfs->pwd.data = faxdev->PostScriptPassword.data,
	  pfs->pwd.size = faxdev->PostScriptPassword.size,
	  pfs->pwd.persistent = false;
	pfs->sd.data = (const byte *)faxdev->StorageDevice,
	  pfs->sd.size = strlen(pfs->sd.data),
	  pfs->sd.persistent = true;
	if (	(code = param_bool_param(plist, "ActivityReport", &faxdev->ActivityReport)) < 0 ||
		(code = param_bool_param(plist, "DefaultCaptionOn", &faxdev->DefaultCaptionOn)) < 0 ||
		(code = param_bool_param(plist, "DefaultConfirmOn", &faxdev->DefaultConfirmOn)) < 0 ||
		(code = param_bool_param(plist, "DefaultCoversOn", &faxdev->DefaultCoversOn)) < 0 ||
		(code = param_int_param(plist, "DefaultResolution", &faxdev->DefaultResolution)) < 0 ||
		(code = param_int_param(plist, "DefaultRetryCount", &faxdev->DefaultRetryCount)) < 0 ||
		(code = param_int_param(plist, "DefaultRetryInterval", &faxdev->DefaultRetryInterval)) < 0 ||
		(code = param_int_param(plist, "DialToneWaitPeriod", &faxdev->DialToneWaitPeriod)) < 0 ||
		(code = param_string_param(plist, "ID", &pfs->id)) < 0 ||
		(code = param_long_param(plist, "MaxFaxBuffer", &faxdev->MaxFaxBuffer)) < 0 ||
		(code = param_string_param(plist, "PostScriptPassword", &pfs->pwd)) < 0 ||
		(code = param_bool_param(plist, "ReceivePostScript", &faxdev->ReceivePostScript)) < 0 ||
		(code = param_int_param(plist, "Rings", &faxdev->Rings)) < 0 ||
		(code = param_int_param(plist, "ServiceEnable", &faxdev->ServiceEnable)) < 0 ||
		(code = param_int_param(plist, "Speaker", &faxdev->Speaker)) < 0 ||
		(code = param_string_param(plist, "StorageDevice", &pfs->sd)) < 0 ||
		(code = param_bool_param(plist, "WaitForDialTone", &faxdev->WaitForDialTone))
	   )
		return code;
	return 0;
}

#define faxdev ((gx_io_device_fax *)iodev)

/* Get parameters from device. */
private int
fax_get_params(gx_io_device *iodev, gs_param_list *plist)
{	fax_strings fs;
	return fax_xfer_params(faxdev, plist, &fs);
}

/* Put parameters to device. */
private int
fax_put_params(gx_io_device *iodev, gs_param_list *plist)
{	gx_io_device_fax tdev;
	fax_strings fs;
	int code;
	gx_io_device *sdev;
	tdev = *faxdev;
	code = fax_xfer_params(&tdev, plist, &fs);
	if ( code < 0 )
		return code;
#define between(v, lo, hi) (tdev.v >= (lo) && tdev.v <= (hi))
	if ( !(	between(DefaultResolution, 0, 1) &&
		between(DefaultRetryCount, 0, 100) &&
		between(DefaultRetryInterval, 1, 60) &&
		between(DialToneWaitPeriod, 1, 10) &&
		fs.id.size > 20 ||
		tdev.MaxFaxBuffer >= 350000 &&
		fs.pwd.size > 32 ||
		between(Rings, 1, 30) &&
		between(ServiceEnable, 0, 3) &&
		between(Speaker, 0, 2) &&
		(sdev = gs_findiodevice(fs.sd.data, fs.sd.size)) != 0
	      )
	   )
		return_error(gs_error_rangecheck);
#undef between
	memcpy(tdev.ID.data, fs.id.data, fs.id.size);
	  tdev.ID.size = fs.id.size;
	memcpy(tdev.PostScriptPassword.data, fs.pwd.data, fs.pwd.size);
	  tdev.PostScriptPassword.size = fs.pwd.size;
	tdev.StorageDevice = sdev->dname;
	*faxdev = tdev;
	return 0;
}

/* ------ FaxOptions decoding ------ */

typedef struct fax_options_s fax_options;
typedef struct fax_custom_params_s fax_custom_params;
typedef int (*fax_custom_proc)(P2(const fax_options *, const fax_custom_params *));
struct fax_options_s {
	gs_param_string CalleePhone;
	limited_string(20) CallerID;
	gs_param_string CallerPhone;
	fax_custom_proc Confirmation;
	struct { fax_options *options; uint size; } Copies;
	/* CoverNote */
	fax_custom_proc CoverSheet;
	bool CoverSheetOnly;
	limited_string(100) DialCallee;
	bool ErrorCorrect;
	int FaxType;
	int MailingTime[6];
	int MaxRetries;
	int nPages;
	fax_custom_proc PageCaption;
	limited_string(32) PostScriptPassword;
	void *ProcInfo;
	gs_param_string RecipientID;
	gs_param_string RecipientMailStop;
	gs_param_string RecipientName;
	gs_param_string RecipientOrg;
	gs_param_string RecipientPhone;
	gs_param_string Regarding;
	int RetryInterval;
	bool RevertToRaster;
	gs_param_string SenderID;
	gs_param_string SenderMailStop;
	gs_param_string SenderName;
	gs_param_string SenderOrg;
	gs_param_string SenderPhone;
	bool TrimWhite;
};

/* ------ Custom fax procedure parameters ------ */

struct fax_custom_params_s {
	limited_string(20) CalleeID;
	int CallLength;
	int CoverType;
	int CurrentPageNo;
	/* ErrorArray */
	int ErrorIndex;
	bool IncludesFinalPage;
	int InitialPage, LimitPage;
	int NumberOfCalls;
	int PagesSent;
	bool SendPostScript;
	int TimeSent[6];
};
