#
# "$Id: Makefile,v 1.1 1999/04/21 19:32:38 mike Exp $"
#
#   GNU Ghostscript makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-1999 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#
#   This makefile and any derivative of it may be used and distributed
#   freely under the terms of the GNU General Public License when
#   used with GNU Ghostscript or its derivatives.  Use of the makefile
#   (or any derivative of it) with software other than GNU GhostScript
#   (or its derivatives) is governed by the CUPS license agreement.
#

include ../Makedefs

#
# Common dependencies on makefile and makedefs...
#

MAKEFILE	=	Makefile ../Makedefs

#
# Targets...
#

TARGETS		=	echogs genarch genconf geninit gs

#
# Directories and paths for all GNU GhostScript data and support files...
#

GSDIR		=	$(DATADIR)/ghostscript
GS_LIB_DEFAULT	=	$(GSDIR):$(DATADIR)/fonts
GS_INIT		=	gs_init.ps

#
# Language and device features to include.
#

FEATURE_DEVS	=	fzlib.dev level2.dev pdf.dev pipe.dev ttfont.dev \
PLATFORM	=	unix_
DEVICE_DEVS	=	x11.dev
DEVS_ALL	=	$(PLATFORM).dev $(FEATURE_DEVS) $(DEVICE_DEVS)

#
# Configuration files...
#

arch_h		=	$(arch_h)
devs_tr		=	devs.tr
gconfig_h	=	gconfxx.h
gconfigf_h	=	gconfxc.h
ld_tr		=	ld.tr

#
# Compile the .ps initialization files into the executable.
#

COMPILE_INITS	=	1

#
# Macros for constructing the *.dev files that describe features and
# devices.
#

SETDEV		=	./echogs -e .dev -w- -Q-dev -F -s -Q-obj
SETPDEV		=	./echogs -e .dev -w- -Q-dev -F -s -Q-include -Qpage -Q-obj
SETMOD		=	./echogs -e .dev -w- -Q-obj
ADDMOD		=	./echogs -e .dev -a-

#
# Make everything...
#

all:		$(TARGETS)

#
# Clean all config and object files...
#

clean:
	$(RM) $(TARGETS)
	$(RM) *.o *.a core
	$(RM) *.dev *.d_* $(arch_h) devs*.tr gconfig*.h gconfx*.h j*.h o*.tr l*.tr
	$(RM) gconfig*.c gscdefs*.c iconfig*.c
	$(RM) _temp_* _temp_*.* *.map *.sym
	$(RM) gs_init.c

#
# echogs - souped-up echo command for GhostScript...
#

echogs:		echogs.o
	$(CC) $(LDFLAGS) -o echogs echogs.o

#
# genarch - generate the architecture configuration file.
#

genarch:	genarch.o
	$(CC) $(LDFLAGS) -o genarch genarch.o

#
# genconf - generate the device configuration file.
#

genconf:	genconf.o
	$(CC) $(LDFLAGS) -o genconf genconf.o

#
# geninit - generate the initialization file.
#

geninit:	geninit.o
	$(CC) $(LDFLAGS) -o geninit geninit.o


#
# Build the configuration files...
#

$(arch_h):	genarch
	./genarch $(arch_h)

$(devs_tr):	$(MAKEFILE) echogs
	./echogs -w $(devs_tr) - -include $(PLATFORM).dev
	./echogs -a $(devs_tr) - $(FEATURE_DEVS)
	./echogs -a $(devs_tr) - $(DEVICE_DEVS)

$(gconfig_h) $(ld_tr) lib.tr:	$(MAKEFILE) genconf echogs $(devs_tr) \
				$(DEVS_ALL) libcore.dev
	./genconf $(devs_tr) libcore.dev -h $(gconfig_h) \
		-p "%s&s&&" -pl "&-l%s&s&&" -pL "&-L%s&s&&" -ol $(ld_tr)
	./echogs -a $(gconfig_h) -x 23 define GS_LIB_DEFAULT -x 2022 \
		$(GS_LIB_DEFAULT) -x 22
	./echogs -a $(gconfig_h) -x 23 define GS_INIT -x 2022 $(GS_INIT) -x 22



stdpre_h	=	stdpre.h
std_h		=	std.h $(arch_h) $(stdpre_h)
gp_h		=	gp.h
gpcheck_h	=	gpcheck.h

# gconfig*.h are generated dynamically.
gconfig__h	=	gconfig_.h
gconfigv_h	=	gconfigv.h
gscdefs_h	=	gscdefs.h

# standard header translations...
ctype__h	=	ctype_.h $(std_h)
dirent__h	=	dirent_.h $(std_h) $(gconfig__h)
errno__h	=	errno_.h
malloc__h	=	malloc_.h $(std_h)
math__h		=	math_.h $(std_h) $(vmsmath_h)
stat__h		=	stat_.h $(std_h)
stdio__h	=	stdio_.h $(std_h)
string__h	=	string_.h $(std_h)
time__h		=	time_.h $(std_h) $(gconfig__h)

# Miscellaneous

gdebug_h	=	gdebug.h
gsalloc_h	=	gsalloc.h
gserror_h	=	gserror.h
gserrors_h	=	gserrors.h
gsexit_h	=	gsexit.h
gsio_h		=	gsio.h
gsmdebug_h	=	gsmdebug.h
gsmemory_h	=	gsmemory.h
gsrefct_h	=	gsrefct.h
gsstruct_h	=	gsstruct.h
gstypes_h	=	gstypes.h
gx_h		=	gx.h $(stdio__h) $(gdebug_h) $(gserror_h) \
			$(gsio_h) $(gsmemory_h) $(gstypes_h)

gsbitops_h	=	gsbitops.h
gsbittab_h	=	gsbittab.h
gsflip_h	=	gsflip.h
gsuid_h		=	gsuid.h
gsutil_h	=	gsutil.h
gxarith_h	=	gxarith.h
gxbitmap_h	=	gxbitmap.h
gxfarith_h	=	gxfarith.h $(gconfigv_h) $(gxarith_h)
gxfixed_h	=	gxfixed.h
gxobj_h		=	gxobj.h $(gxbitmap_h)
gxalloc_h	=	gxalloc.h $(gsalloc_h) $(gxobj_h)

### Executable code

gsalloc.o: gsalloc.c $(GX) $(memory__h) \
  $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)

gsbitops.o: gsbitops.c $(AK) $(std_h) $(memory__h) $(gsbitops_h)

gsbittab.o: gsbittab.c $(AK) $(stdpre_h) $(gsbittab_h)

# gsflip is not part of the standard configuration: it's rather large,
# and no standard facility requires it.
gsflip.o: gsflip.c $(GX) $(gsflip_h)
	$(CCLEAF) gsflip.c

gsmemory.o: gsmemory.c $(GX) $(malloc__h) $(memory__h) \
  $(gsmdebug_h) $(gsrefct_h) $(gsstruct_h)

gsmisc.o: gsmisc.c $(GXERR) $(gconfigv_h) \
  $(malloc__h) $(math__h) $(memory__h) $(gpcheck_h) $(gxfarith_h) $(gxfixed_h)

gsutil.o: gsutil.c $(AK) $(gconfigv_h) \
  $(std_h) $(gstypes_h) $(gsuid_h) $(gsutil_h)

###### Low-level facilities and utilities

### Include files

gdevbbox_h=gdevbbox.h
gdevmrop_h=gdevmrop.h

gsccode_h=gsccode.h
gsccolor_h=gsccolor.h $(gsstruct_h)
gscolor1_h=gscolor.h
gscoord_h=gscoord.h
gscpm_h=gscpm.h
gsdevice_h=gsdevice.h
gsfont_h=gsfont.h
gshsb_h=gshsb.h
gsht_h=gsht.h
gsht1_h=gsht1.h $(gsht_h)
gsiparam_h=gsiparam.h
gsjconf_h=gsjconf.h $(std_h)
gslib_h=gslib.h $(stdio__h)
gslparam_h=gslparam.h
gsmatrix_h=gsmatrix.h
gspaint_h=gspaint.h
gsparam_h=gsparam.h
gspath2_h=gspath2.h
gspenum_h=gspenum.h
gsropt_h=gsropt.h
gsxfont_h=gsxfont.h
# Out of order
gschar_h=gschar.h $(gsccode_h) $(gscpm_h)
gscolor2_h=gscolor2.h $(gsccolor_h) $(gsuid_h) $(gxbitmap_h)
gsimage_h=gsimage.h $(gsiparam_h)
gsline_h=gsline.h $(gslparam_h)
gspath_h=gspath.h $(gspenum_h)

gxbcache_h=gxbcache.h $(gxbitmap_h)
gxchar_h=gxchar.h $(gschar_h)
gxcindex_h=gxcindex.h
gxcvalue_h=gxcvalue.h
gxclio_h=gxclio.h
gxclip2_h=gxclip2.h
gxcolor2_h=gxcolor2.h $(gscolor2_h) $(gsrefct_h) $(gxbitmap_h)
gxcoord_h=gxcoord.h $(gscoord_h)
gxcpath_h=gxcpath.h
gxdda_h=gxdda.h
gxdevrop_h=gxdevrop.h
gxdevmem_h=gxdevmem.h
gxdht_h=gxdht.h $(gxarith_h)
gxdither_h=gxdither.h
gxfcache_h=gxfcache.h $(gsuid_h) $(gsxfont_h) $(gxbcache_h)
gxfont_h=gxfont.h $(gsfont_h) $(gsuid_h) $(gsstruct_h)
gxfont0_h=gxfont0.h
gxfrac_h=gxfrac.h
gxhttile_h=gxhttile.h
gxiodev_h=gxiodev.h $(stat__h)
gxline_h=gxline.h $(gslparam_h)
gxlum_h=gxlum.h
gxmatrix_h=gxmatrix.h $(gsmatrix_h)
gxpaint_h=gxpaint.h
gxpath_h=gxpath.h $(gscpm_h) $(gspenum_h)
gxpcolor_h=gxpcolor.h
gxstate_h=gxstate.h
gxtmap_h=gxtmap.h
gxxfont_h=gxxfont.h $(gsccode_h) $(gsmatrix_h) $(gsuid_h) $(gsxfont_h)
# The following are out of order because they include other files.
gsdcolor_h=gsdcolor.h $(gxarith_h) $(gxbitmap_h) $(gxcindex_h) $(gxhttile_h)
gxdcolor_h=gxdcolor.h $(gsdcolor_h) $(gsropt_h) $(gsstruct_h)
gxdevice_h=gxdevice.h $(gsdcolor_h) $(gsiparam_h) $(gsmatrix_h) \
  $(gsropt_h) $(gsstruct_h) $(gsxfont_h) \
  $(gxbitmap_h) $(gxcindex_h) $(gxcvalue_h) $(gxfixed_h)
gxctable_h=gxctable.h $(gxfixed_h) $(gxfrac_h)
gscie_h=gscie.h $(gsrefct_h) $(gxctable_h)
gscspace_h=gscspace.h
gxdcconv_h=gxdcconv.h $(gxfrac_h) $(gsccolor_h)
gxfmap_h=gxfmap.h $(gsrefct_h) $(gxfrac_h) $(gxtmap_h)
gxistate_h=gxistate.h $(gsropt_h) $(gxfixed_h) $(gxline_h) $(gxmatrix_h)
gxclist_h=gxclist.h $(gxbcache_h) $(gxclio_h) $(gxistate_h)
gxcmap_h=gxcmap.h $(gxfmap_h)
gxcspace_h=gxcspace.h $(gscspace_h) $(gsccolor_h) $(gsstruct_h) $(gxfrac_h)
gxht_h=gxht.h $(gsht1_h) $(gxtmap_h)
gscolor_h=gscolor.h $(gxtmap_h)
gsstate_h=gsstate.h $(gscolor_h) $(gsdevice_h) $(gsht_h) $(gsline_h)

gzacpath_h=gzacpath.h
gzcpath_h=gzcpath.h $(gxcpath_h)
gzht_h=gzht.h $(gxdht_h) $(gxfmap_h) $(gxht_h) $(gxhttile_h)
gzline_h=gzline.h $(gxline_h)
gzpath_h=gzpath.h $(gsstruct_h) $(gxpath_h)
gzstate_h=gzstate.h $(gsstate_h) $(gxcvalue_h) $(gxdcolor_h) $(gxistate_h) $(gxstate_h) $(gxtmap_h)

gdevprn_h=gdevprn.h $(memory__h) $(string__h) $(gx_h) \
  $(gserrors_h) $(gsmatrix_h) $(gsutil_h) \
  $(gxdevice_h) $(gxdevmem_h) $(gxclist_h)

scommon_h=scommon.h $(gsmemory_h) $(gstypes_h) $(gsstruct_h)
shc_h=shc.h $(gsbittab_h)
siscale_h=siscale.h $(gconfigv_h)
slzwx_h=slzwx.h
spcxx_h=spcxx.h
spdiffx_h=spdiffx.h
spngpx_h=spngpx.h
srlx_h=srlx.h
strimpl_h=strimpl.h $(scommon_h) $(gstypes_h) $(gsstruct_h)
szlibx_h=szlibx.h
# Out of order
scf_h=scf.h $(shc_h)
scfx_h=scfx.h $(shc_h)
gximage_h=gximage.h $(gsiparam_h) $(gxcspace_h) $(gxdda_h) \
  $(siscale_h) $(strimpl_h)

### Executable code

# gconfig and gscdefs are handled specially.  Currently they go in psbase
# rather than in libcore, which is clearly wrong.
gconfig=gconfig
$(gconfig).o: gconf.c $(GX) \
  $(gscdefs_h) $(gconfig_h) $(gxdevice_h) $(gxiodev_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gconf.c $(gconfig).c
	$(CCC) $(gconfig).c
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c

gscdefs=gscdefs
$(gscdefs).o: gscdef.c $(stdpre_h) $(gscdefs_h) $(gconfig_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gscdef.c $(gscdefs).c
	$(CCC) $(gscdefs).c
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c

gxacpath.o: gxacpath.c $(GXERR) \
  $(gsdcolor_h) $(gsstruct_h) $(gsutil_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxpaint_h) \
  $(gzacpath_h) $(gzcpath_h) $(gzpath_h)

gxbcache.o: gxbcache.c $(GX) $(memory__h) \
  $(gsmdebug_h) $(gxbcache_h)

gxccache.o: gxccache.c $(GXERR) $(gpcheck_h) \
  $(gscspace_h) $(gsimage_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxdevmem_h) $(gxfcache_h) \
  $(gxfixed_h) $(gxfont_h) $(gxhttile_h) $(gxmatrix_h) $(gxxfont_h) \
  $(gzstate_h) $(gzpath_h) $(gzcpath_h) 

gxccman.o: gxccman.c $(GXERR) $(gpcheck_h) \
  $(gsbitops_h) $(gsstruct_h) $(gsutil_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gxdevice_h) $(gxdevmem_h) $(gxfont_h) $(gxfcache_h) $(gxchar_h) \
  $(gxxfont_h) $(gzstate_h) $(gzpath_h)

gxcht.o: gxcht.c $(GXERR) \
  $(gsdcolor_h) $(gsutil_h) \
  $(gxcmap_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gzht_h) $(gzstate_h)

gxcmap.o: gxcmap.c $(GXERR) \
  $(gsccolor_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gxdither_h) \
  $(gxfarith_h) $(gxfrac_h) $(gxlum_h) $(gzstate_h)

gxcpath.o: gxcpath.c $(GXERR) \
  $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gxfixed_h) $(gzpath_h) $(gzcpath_h)

gxdcconv.o: gxdcconv.c $(GX) \
  $(gsdcolor_h) $(gxcmap_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gxfarith_h) $(gxlum_h) $(gzstate_h)

gxdcolor.o: gxdcolor.c $(GX) \
  $(gxdcolor_h) $(gxdevice_h)

gxdither.o: gxdither.c $(GX) \
  $(gsstruct_h) $(gsdcolor_h) \
  $(gxcmap_h) $(gxdevice_h) $(gxdither_h) $(gxlum_h) $(gzht_h)

gxfill.o: gxfill.c $(GXERR) $(math__h) \
  $(gsstruct_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxhttile_h) \
  $(gxistate_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzpath_h)

gxht.o: gxht.c $(GXERR) \
  $(gsbitops_h) $(gsstruct_h) $(gsutil_h) \
  $(gxfixed_h) $(gxdevice_h) $(gzstate_h) $(gzht_h)

gximage.o: gximage.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gsccolor_h) $(gspaint_h) $(gsstruct_h) \
  $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxmatrix_h) \
  $(gxdevice_h) $(gzpath_h) $(gzstate_h) \
  $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h)

gximage0.o: gximage0.c $(GXERR) $(memory__h) \
  $(gxcpath_h) $(gxdevice_h) $(gximage_h)

gximage1.o: gximage1.c $(GXERR) $(gpcheck_h) \
  $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gximage_h) $(gxmatrix_h) \
  $(gzht_h) $(gzpath_h) $(gzstate_h)

gximage2.o: gximage2.c $(GXERR) $(gpcheck_h) \
  $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gximage_h) $(gxmatrix_h) \
  $(gzht_h) $(gzpath_h) $(gzstate_h)

gximage3.o: gximage3.c $(GXERR) $(gpcheck_h) \
  $(gsccolor_h) $(gspaint_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gximage_h) $(gxmatrix_h) \
  $(gzht_h) $(gzpath_h) $(gzstate_h)

gximage4.o: gximage4.c $(GXERR) $(gpcheck_h) \
  $(gsccolor_h) $(gspaint_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gximage_h) $(gxmatrix_h) \
  $(gzht_h) $(gzpath_h) $(gzstate_h)

# gximage5 should be included only if iscale.dev is.
gximage5.o: gximage5.c $(GXERR) $(gpcheck_h) \
  $(gsccolor_h) $(gspaint_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gximage_h) $(gxmatrix_h) \
  $(gzht_h) $(gzpath_h) $(gzstate_h)

gxpaint.o: gxpaint.c $(GX) \
  $(gxdevice_h) $(gxhttile_h) $(gxpaint_h) $(gxpath_h) $(gzstate_h)

gxpath.o: gxpath.c $(GXERR) \
  $(gsstruct_h) $(gxfixed_h) $(gzpath_h)

gxpath2.o: gxpath2.c $(GXERR) $(math__h) \
  $(gxfixed_h) $(gxarith_h) $(gzpath_h)

gxpcopy.o: gxpcopy.c $(GXERR) $(math__h) \
  $(gxarith_h) $(gxfixed_h) $(gzpath_h)

gxpdash.o: gxpdash.c $(GX) $(math__h) \
  $(gscoord_h) $(gsline_h) $(gsmatrix_h) \
  $(gxfixed_h) $(gzline_h) $(gzpath_h)

gxstroke.o: gxstroke.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gscoord_h) $(gsdcolor_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxarith_h) \
  $(gxhttile_h) $(gxistate_h) $(gxmatrix_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzline_h) $(gzpath_h)

###### Higher-level facilities

gschar.o: gschar.c $(GXERR) \
  $(gspath_h) $(gsstruct_h) \
  $(gxfixed_h) $(gxarith_h) $(gxmatrix_h) $(gxcoord_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxfcache_h) $(gzpath_h) $(gzstate_h)

gscolor.o: gscolor.c $(GXERR) \
  $(gsccolor_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzstate_h)

gscoord.o: gscoord.c $(GXERR) $(math__h) \
  $(gsccode_h) $(gxcoord_h) $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) $(gxfont_h) \
  $(gxmatrix_h) $(gxpath_h) $(gzstate_h)

gsdevice.o: gsdevice.c $(GXERR) $(memory__h) \
  $(gscdefs_h) $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstruct_h) \
  $(gxbitmap_h) $(gxcmap_h) $(gxdevice_h) $(gxdevmem_h) $(gzstate_h)

gsdevmem.o: gsdevmem.c $(GXERR) $(math__h) $(memory__h) \
  $(gxarith_h) $(gxdevice_h) $(gxdevmem_h)

gsdparam.o: gsdparam.c $(GXERR) $(memory__h) $(string__h) \
  $(gsparam_h) $(gxdevice_h) $(gxfixed_h)

gsfont.o: gsfont.c $(GXERR) \
  $(gsstruct_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfcache_h) \
  $(gzstate_h)

gsht.o: gsht.c $(GXERR) \
  $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gshtscr.o: gshtscr.c $(GXERR) $(math__h) \
  $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gsimage.o: gsimage.c $(GXERR) $(memory__h)\
  $(gscspace_h) $(gsiparam_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxdevice_h) $(gzstate_h)

gsimpath.o: gsimpath.c $(GXERR) \
  $(gsmatrix_h) $(gsstate_h) $(gspath_h)

gsinit.o: gsinit.c $(memory__h) $(stdio__h) \
  $(gdebug_h) $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmemory_h)

gsiodev.o: gsiodev.c $(GXERR) $(errno__h) $(string__h) \
  $(gp_h) $(gsparam_h) $(gxiodev_h)

gsline.o: gsline.c $(GXERR) $(math__h) \
  $(gsline_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzline_h)

gsmatrix.o: gsmatrix.c $(GXERR) $(math__h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h)

gspaint.o: gspaint.c $(GXERR) $(gpcheck_h) \
  $(gspaint_h) $(gspath_h) $(gsropt_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxpaint_h) \
  $(gzpath_h) $(gzstate_h) $(gxcpath_h) $(gxdevmem_h) $(gximage_h)

gsparam.o: gspaint.c $(GXERR) $(memory__h) $(string__h) \
  $(gsparam_h) $(gsstruct_h)

gspath.o: gspath.c $(GXERR) \
  $(gscoord_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzcpath_h) $(gzpath_h) $(gzstate_h)

gsstate.o: gsstate.c $(GXERR) \
  $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) \
  $(gzstate_h) $(gzht_h) $(gzline_h) $(gzpath_h) $(gzcpath_h)

###### The internal devices

### The default, null, forwarding, bounding box, and render/RGB
### device implementations:

# The bounding box device is not normally a free-standing device,
# but we can configure it as one for testing.
bbox.dev: gdevbbox.o
	$(SETDEV) bbox gdevbbox.o

gdevbbox.o: gdevbbox.c $(GXERR) $(math__h) $(memory__h) \
  $(gdevbbox_h) $(gsdevice_h) $(gsparam_h) \
  $(gxcpath_h) $(gxdevice_h) $(gxistate_h) $(gxpaint_h) $(gxpath_h)

gdevddrw.o: gdevddrw.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)

gdevdflt.o: gdevdflt.c $(GXERR) $(gpcheck_h) \
  $(gxcpath_h) $(gxdevice_h) $(gxdevmem_h)

gdevnfwd.o: gdevnfwd.c $(GX) \
  $(gxdevice_h)

# The render/RGB device is only here as an example, but we can configure
# it as a real device for testing.
rrgb.dev: gdevrrgb.o page.dev
	$(SETPDEV) rrgb gdevrrgb.o

gdevrrgb.o: gdevrrgb.c $(AK)\
  $(gdevprn_h)

### The memory devices:

gdevmem_h=gdevmem.h $(gsbitops_h)

gdevabuf.o: gdevabuf.c $(GXERR) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmem.o: gdevmem.c $(GXERR) \
  $(gsstruct_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm1.o: gdevm1.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm2.o: gdevm2.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm4.o: gdevm4.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm8.o: gdevm8.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm16.o: gdevm16.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm24.o: gdevm24.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm32.o: gdevm32.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmpla.o: gdevmpla.c $(GX) \
  $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

# Create a pseudo-"feature" for the entire graphics library.

LIB1s=gsalloc.o gsbitops.o gsbittab.o
LIB2s=gschar.o gscolor.o gscoord.o gsdevice.o gsdevmem.o
LIB3s=gsdparam.o gsfont.o gsht.o gshtscr.o
LIB4s=gsimage.o gsimpath.o gsinit.o gsiodev.o
LIB6s=gsline.o gsmatrix.o gsmemory.o gsmisc.o
LIB7s=gspaint.o gsparam.o gspath.o gsstate.o gsutil.o
LIB1x=gxacpath.o gxbcache.o
LIB2x=gxccache.o gxccman.o gxcht.o gxcmap.o gxcpath.o
LIB3x=gxdcconv.o gxdcolor.o gxdither.o gxfill.o gxht.o
LIB4x=gximage.o gximage0.o gximage1.o gximage2.o
LIB5x=gximage3.o gximage4.o gximage5.o 
LIB6x=gxpaint.o gxpath.o gxpath2.o gxpcopy.o gxpdash.o
LIB7x=gxstroke.o
LIB1d=gdevabuf.o gdevddrw.o gdevdflt.o gdevnfwd.o
LIB2d=gdevmem.o gdevm1.o gdevm2.o gdevm4.o gdevm8.o
LIB3d=gdevm16.o gdevm24.o gdevm32.o gdevmpla.o
LIBs=$(LIB1s) $(LIB2s) $(LIB3s) $(LIB4s) $(LIB5s) $(LIB6s) $(LIB7s)
LIBx=$(LIB1x) $(LIB2x) $(LIB3x) $(LIB4x) $(LIB5x) $(LIB6x) $(LIB7x)
LIBd=$(LIB1d) $(LIB2d) $(LIB3d)
LIB_ALL=$(LIBs) $(LIBx) $(LIBd)
libs.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBs)
	./echogs -w libs.dev $(LIB1s)
	./echogs -a libs.dev $(LIB2s)
	./echogs -a libs.dev $(LIB3s)
	./echogs -a libs.dev $(LIB4s)
	./echogs -a libs.dev $(LIB5s)
	./echogs -a libs.dev $(LIB6s)
	./echogs -a libs.dev $(LIB7s)
	$(ADDMOD) libs -init gscolor
libx.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBx)
	./echogs -w libx.dev $(LIB1x)
	./echogs -a libx.dev $(LIB2x)
	./echogs -a libx.dev $(LIB3x)
	./echogs -a libx.dev $(LIB4x)
	./echogs -a libx.dev $(LIB5x)
	./echogs -a libx.dev $(LIB6x)
	./echogs -a libx.dev $(LIB7x)
libd.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBd)
	./echogs -w libd.dev $(LIB1d)
	./echogs -a libd.dev $(LIB2d)
	./echogs -a libd.dev $(LIB3d)
# roplib shouldn't be required....
libcore.dev: $(LIB_MAK) $(ECHOGS_XE)\
  libs.dev libx.dev libd.dev iscale.dev roplib.dev
	$(SETMOD) libcore
	$(ADDMOD) libcore -dev nullpage
	$(ADDMOD) libcore -include libs libx libd iscale roplib

# ---------------- Stream support ---------------- #
# Currently the only thing in the library that uses this is clists.

stream_h=stream.h $(scommon_h)

stream.o: stream.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

# ---------------- CCITTFax filters ---------------- #
# These are used by clists, some drivers, and Level 2 in general.

cfe_=scfe.o scfetab.o shc.o
cfe.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfe_)
	$(SETMOD) cfe $(cfe_)

scfe.o: scfe.c $(AK) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfetab.o: scfetab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

shc.o: shc.c $(AK) $(std_h) $(shc_h)

cfd_=scfd.o scfdtab.o
cfd.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfd_)
	$(SETMOD) cfd $(cfd_)

scfd.o: scfd.c $(AK) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfdtab.o: scfdtab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

# ---------------- LZW filters ---------------- #
# These are used by Level 2 in general.

lzwe_=slzwce.o slzwc.o
#lzwe_=slzwe.o slzwc.o
lzwe.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwe_)
	$(SETMOD) lzwe $(lzwe_)

slzwce.o: slzwce.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwe.o: slzwe.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwc.o: slzwc.c $(AK) $(std_h)\
  $(slzwx_h) $(strimpl_h)

lzwd_=slzwd.o slzwc.o
lzwd.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwd_)
	$(SETMOD) lzwd $(lzwd_)

slzwd.o: slzwd.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

# ---------------- Pixel-difference filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pdiff_=spdiff.o
pdiff.dev: $(LIB_MAK) $(ECHOGS_XE) $(pdiff_)
	$(SETMOD) pdiff $(pdiff_)

spdiff.o: spdiff.c $(AK) $(std_h)\
  $(spdiffx_h) $(strimpl_h)

# ---------------- PNG pixel prediction filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pngp_=spngp.o
pngp.dev: $(LIB_MAK) $(ECHOGS_XE) $(pngp_)
	$(SETMOD) pngp $(pngp_)

spngp.o: spngp.c $(AK) $(memory__h)\
  $(spngpx_h) $(strimpl_h)

# ---------------- RunLength filters ---------------- #
# These are used by clists and also by Level 2 in general.

rle_=srle.o
rle.dev: $(LIB_MAK) $(ECHOGS_XE) $(rle_)
	$(SETMOD) rle $(rle_)

srle.o: srle.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

rld_=srld.o
rld.dev: $(LIB_MAK) $(ECHOGS_XE) $(rld_)
	$(SETMOD) rld $(rld_)

srld.o: srld.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

# ---------------- zlib filters ---------------- #
# These are used by clists and are also available as filters.

szlibc_=szlibc.o

szlibc.o: szlibc.c $(AK) $(std_h) \
  $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(strimpl_h) $(szlibx_h)
	$(CCC) -I$(ZSRCDIR) szlibc.c

szlibe_=$(szlibc_) szlibe.o
szlibe.dev: $(LIB_MAK) $(ECHOGS_XE) zlibe.dev $(szlibe_)
	$(SETMOD) szlibe $(szlibe_)
	$(ADDMOD) szlibe -include zlibe

szlibe.o: szlibe.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCC) -I$(ZSRCDIR) szlibe.c

szlibd_=$(szlibc_) szlibd.o
szlibd.dev: $(LIB_MAK) $(ECHOGS_XE) zlibd.dev $(szlibd_)
	$(SETMOD) szlibd $(szlibd_)
	$(ADDMOD) szlibd -include zlibd

szlibd.o: szlibd.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCC) -I$(ZSRCDIR) szlibd.c

# ---------------- Command lists ---------------- #

gxcldev_h=gxcldev.h $(gxclist_h) $(gsropt_h) $(gxht_h) $(gxtmap_h) $(gxdht_h)\
  $(gxfixed_h) $(strimpl_h) $(scfx_h) $(srlx_h)
gxclpath_h=gxclpath.h $(gxfixed_h)

# Choose between clfile and clmem.  Someday this might be dynamic....
cl_impl=clfile

# Command list package.  Currently the higher-level facilities are required,
# but eventually they will be optional.
clist.dev: $(LIB_MAK) $(ECHOGS_XE) clbase.dev clpath.dev
	$(SETMOD) clist -include clbase clpath

# Base command list facility.
clbase_=gxclist.o gxclbits.o gxclpath.o gxclread.o stream.o
clbase.dev: $(LIB_MAK) $(ECHOGS_XE) $(clbase_) $(cl_impl).dev \
  cfe.dev cfd.dev rle.dev rld.dev
	$(SETMOD) clbase $(clbase_)
	$(ADDMOD) clbase -include $(cl_impl) cfe cfd rle rld

gdevht_h=gdevht.h

gdevht.o: gdevht.c $(GXERR) \
  $(gdevht_h) $(gxdcconv_h) $(gxdcolor_h) $(gxdevice_h) $(gxdither_h)

gxclist.o: gxclist.c $(GXERR) $(memory__h) $(gpcheck_h) \
  $(gsbitops_h) $(gsutil_h) \
  $(gxbitmap_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h)

gxclbits.o: gxclbits.c $(GXERR) $(memory__h) $(gpcheck_h) \
  $(gsbitops_h) \
  $(gxbitmap_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h)

# (gxclread shouldn't need gxclpath.h)
gxclread.o: gxclread.c $(GXERR) $(gpcheck_h) \
  $(gdevht_h) $(gsbitops_h) $(gscspace_h) $(gsdevice_h) $(gsdcolor_h) \
  $(gxbitmap_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxhttile_h) $(gxpaint_h) $(gzacpath_h) $(gzcpath_h) $(gzpath_h) \
  $(stream_h) $(strimpl_h)

# Higher-level command list facilities.
clpath_=gxclimag.o gxclpath.o
clpath.dev: $(LIB_MAK) $(ECHOGS_XE) $(clpath_)
	$(SETMOD) clpath $(clpath_)
	$(ADDMOD) clpath -init climag clpath

gxclimag.o: gxclimag.c $(GXERR) $(math__h) $(memory__h)\
  $(gscspace_h)\
  $(gxarith_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)

gxclpath.o: gxclpath.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h) \
  $(gxbitmap_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzpath_h)

# Implement band lists on files.

clfile_=gxclfile.o
clfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(clfile_)
	$(SETMOD) clfile $(clfile_)

gxclfile.o: gxclfile.c $(stdio__h) $(string__h) \
  $(gp_h) $(gsmemory_h) $(gserror_h) $(gserrors_h) $(gxclio_h)

# Implement band lists in memory (RAM).

clmem_=gxclmem.o
clmem.dev: $(LIB_MAK) $(ECHOGS_XE) $(clmem_) lzwe.dev lzwd.dev
	$(SETMOD) clmem $(clmem_)
	$(ADDMOD) clmem -include lzwe lzwd

gxclmem_h=gxclmem.h $(gxclio_h)

gxclmem.o: gxclmem.c $(GXERR) \
  $(gxclmem_h)

# ---------------- Page devices ---------------- #
# We include this here, rather than in devs.mak, because it is more like
# a feature than a simple device.

page_=gdevprn.o
page.dev: $(LIB_MAK) $(ECHOGS_XE) $(page_) clist.dev
	$(SETMOD) page $(page_)
	$(ADDMOD) page -include clist

gdevprn.o: gdevprn.c $(ctype__h) \
  $(gdevprn_h) $(gp_h) $(gsparam_h) $(gxclio_h)

# ---------------- Image scaling filter ---------------- #
# Currently this module is required even in Level 1 systems,
# but it should be optional in Level 1 and required only in Level 2.

iscale_=siscale.o
iscale.dev: $(LIB_MAK) $(ECHOGS_XE) $(iscale_)
	$(SETMOD) iscale $(iscale_)

siscale.o: siscale.c $(AK) $(math__h) $(memory__h) $(stdio__h) \
  $(siscale_h) $(strimpl_h)

# ---------------- RasterOp et al ---------------- #
# Currently this module is required, but it should be optional.

gsrop_h=gsrop.h $(gsropt_h)

roplib_=gdevmrop.o gsrop.o gsroptab.o
roplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(roplib_)
	$(SETMOD) roplib $(roplib_)
	$(ADDMOD) roplib -init roplib

gdevrun.o: gdevrun.c $(GXERR) $(memory__h) \
  $(gxdevice_h) $(gxdevmem_h)

gdevmrop.o: gdevmrop.c $(GXERR) $(memory__h) \
  $(gsbittab_h) $(gsropt_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h) \
  $(gdevmrop_h)

gsrop.o: gsrop.c $(GXERR) \
  $(gsrop_h) $(gzstate_h)

gsroptab.o: gsroptab.c $(stdpre_h) $(gsropt_h)
	$(CCLEAF) gsroptab.c

# -------- Composite (PostScript Type 0) font support -------- #

psf0lib_=gschar0.o gsfont0.o
psf0lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf0lib_)
	$(SETMOD) psf0lib $(psf0lib_)

gschar0.o: gschar0.c $(GXERR) \
  $(gsstruct_h) $(gxfixed_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfont_h) $(gxfont0_h) $(gxchar_h)

gsfont0.o: gsfont0.c $(GXERR) \
  $(gsmatrix_h) $(gsstruct_h) $(gxfixed_h) $(gxdevmem_h) $(gxfcache_h) \
  $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxdevice_h)

# ---------------- Pattern color ---------------- #

patlib_=gspcolor.o gxclip2.o gxpcmap.o
patlib.dev: $(LIB_MAK) $(ECHOGS_XE) cmyklib.dev psl2lib.dev $(patlib_)
	$(SETMOD) patlib -include cmyklib psl2lib
	$(ADDMOD) patlib -obj $(patlib_)

gspcolor.o: gspcolor.c $(GXERR) $(math__h) \
  $(gsimage_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
  $(gxarith_h) $(gxcolor2_h) $(gxcoord_h) $(gxclip2_h) $(gxcspace_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gxmatrix_h) $(gxpath_h) $(gxpcolor_h) $(gzstate_h)

gxclip2.o: gxclip2.c $(GXERR) $(memory__h) \
  $(gsstruct_h) $(gxclip2_h) $(gxdevice_h) $(gxdevmem_h)

gxpcmap.o: gxpcmap.c $(GXERR) \
  $(gsstruct_h) $(gsutil_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h) $(gzstate_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

psf1lib_=gstype1.o gxhint1.o gxhint2.o gxhint3.o
psf1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf1lib_)
	$(SETMOD) psf1lib $(psf1lib_)

gscrypt1_h=gscrypt1.h
gstype1_h=gstype1.h
gxfont1_h=gxfont1.h
gxop1_h=gxop1.h
gxtype1_h=gxtype1.h $(gscrypt1_h) $(gstype1_h)

gstype1.o: gstype1.c $(GXERR) $(math__h) \
  $(gsline_h) $(gspath_h) $(gsstruct_h) \
  $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h) \
  $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxop1_h) $(gxtype1_h) \
  $(gzpath_h)

gxhint1.o: gxhint1.c $(GXERR) \
  $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxdevmem_h) $(gxchar_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h)

gxhint2.o: gxhint2.c $(GXERR) \
  $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gxop1_h)

gxhint3.o: gxhint3.c $(GXERR) \
  $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gxop1_h) \
  $(gxpath_h)

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=gstype42.o
ttflib.dev: $(LIB_MAK) $(ECHOGS_XE) $(ttflib_)
	$(SETMOD) ttflib $(ttflib_)

gxfont42_h=gxfont42.h

gstype42.o: gstype42.c $(GXERR) $(memory__h) \
  $(gsccode_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxistate_h) $(gxpath_h)

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

cmyklib_=gscolor1.o gsht1.o
cmyklib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmyklib_)
	$(SETMOD) cmyklib $(cmyklib_)
	$(ADDMOD) cmyklib -init gscolor1

gscolor1.o: gscolor1.c $(GXERR) \
  $(gsccolor_h) $(gscolor1_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gzstate_h)

gsht1.o: gsht1.c $(GXERR) \
  $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

# ---------------- HSB color ---------------- #

hsblib_=gshsb.o
hsblib.dev: $(LIB_MAK) $(ECHOGS_XE) $(hsblib_)
	$(SETMOD) hsblib $(hsblib_)

gshsb.o: gshsb.c $(GX) \
  $(gscolor_h) $(gshsb_h) $(gxfrac_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1lib_=gspath1.o
path1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(path1lib_)
	$(SETMOD) path1lib $(path1lib_)

gspath1.o: gspath1.c $(GXERR) $(math__h) \
  $(gscoord_h) $(gspath_h) $(gsstruct_h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) $(gzpath_h)

# ---------------- Level 2 color space support ---------------- #

psl2lib_=gscolor2.o
psl2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2lib_)
	$(SETMOD) psl2lib $(psl2lib_)

gscolor2.o: gscolor2.c $(GXERR) \
  $(gxarith_h) $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h)

# ---------------- Display Postscript / Level 2 support ---------------- #

dps2lib_=gsdps1.o
dps2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dps2lib_)
	$(SETMOD) dps2lib $(dps2lib_)

gsdps1.o: gsdps1.c $(GXERR) $(math__h) \
  $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gspath2_h) \
  $(gxfixed_h) $(gxmatrix_h) $(gzpath_h) $(gzstate_h)

# ---------------- CIE color ---------------- #

cielib_=gscie.o gxctable.o
cielib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cielib_)
	$(SETMOD) cielib $(cielib_)

gscie.o: gscie.c $(GXERR) $(math__h) \
  $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)

gxctable.o: gxctable.c $(GX) \
  $(gxfixed_h) $(gxfrac_h) $(gxctable_h)

# ---------------- Separation colors ---------------- #

seprlib_=gscsepr.o
seprlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(seprlib_)
	$(SETMOD) seprlib $(seprlib_)

gscsepr.o: gscsepr.c $(GXERR) \
  $(gsmatrix_h) $(gsrefct_h) $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h)

# ----------------------- Platform-specific modules ----------------------- #
# Platform-specific code doesn't really belong here: this is code that is
# shared among multiple platforms.

# Unix(-like) file system, also used by Desqview/X.
gp_unifs.o: gp_unifs.c $(AK) $(memory__h) $(string__h) $(gx_h) $(gp_h) \
  $(gsstruct_h) $(gsutil_h) $(stat__h) $(dirent__h)

# Unix(-like) file name syntax, *not* used by Desqview/X.
gp_unifn.o: gp_unifn.c $(AK) $(gx_h) $(gp_h)

# ----------------------------- Main program ------------------------------ #

# Main program for library testing

gslib.o: gslib.c $(AK) $(math__h) \
  $(gx_h) $(gp_h) $(gserrors_h) $(gsmatrix_h) $(gsstate_h) $(gscspace_h) \
  $(gscdefs_h) $(gscolor2_h) $(gscoord_h) $(gslib_h) $(gsparam_h) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
  $(gxalloc_h) $(gxdevice_h)


# ======================== Interpreter support ======================== #

# This is support code for all interpreters, not just PostScript and PDF.
# It knows about the PostScript data types, but isn't supposed to
# depend on anything outside itself.

errors_h=errors.h
idebug_h=idebug.h
idict_h=idict.h
igc_h=igc.h
igcstr_h=igcstr.h
iname_h=iname.h
inamedef_h=inamedef.h $(gconfigv_h) $(iname_h)
ipacked_h=ipacked.h
iref_h=iref.h
isave_h=isave.h
isstate_h=isstate.h
istruct_h=istruct.h $(gsstruct_h)
ivmspace_h=ivmspace.h
opdef_h=opdef.h
# Nested include files
ghost_h=ghost.h $(gx_h) $(iref_h)
imemory_h=imemory.h $(gsalloc_h) $(ivmspace_h)
ialloc_h=ialloc.h $(imemory_h)
iastruct_h=iastruct.h $(gxobj_h) $(ialloc_h)
iastate_h=iastate.h $(gxalloc_h) $(ialloc_h) $(istruct_h)
store_h=store.h $(ialloc_h)

GH=$(AK) $(ghost_h)

isupport1_=ialloc.o igc.o igcref.o igcstr.o
isupport2_=ilocate.o iname.o isave.o
isupport_=$(isupport1_) $(isupport2_)
isupport.dev: $(INT_MAK) $(ECHOGS_XE) $(isupport_)
	$(SETMOD) isupport $(isupport1_)
	$(ADDMOD) isupport -obj $(isupport2_)

ialloc.o: ialloc.c $(AK) $(gx_h) \
  $(errors_h) $(gsstruct_h) $(gxarith_h) \
  $(iastate_h) $(iref_h) $(ivmspace_h) $(store_h)

# igc.c, igcref.c, and igcstr.c should really be in the dpsand2 list,
# but since all the GC enumeration and relocation routines refer to them,
# it's too hard to separate them out from the Level 1 base.
igc.o: igc.c $(GH) \
  $(errors_h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h) $(gsutil_h) \
  $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(inamedef_h) \
  $(ipacked_h) $(isave_h) $(isstate_h) $(istruct_h) $(opdef_h) $(store_h)

igcref.o: igcref.c $(GH) \
  $(gsexit_h) $(iastate_h) $(idebug_h) $(igc_h) $(iname_h) $(ipacked_h)

igcstr.o: igcstr.c $(GH) \
  $(gsmdebug_h) $(gsstruct_h) $(iastate_h) $(igcstr_h)

ilocate.o: ilocate.c $(GH) $(memory__h) \
  $(errors_h) $(gsexit_h) $(gsstruct_h) \
  $(iastate_h) $(igc_h) $(igcstr_h) $(iname_h) $(ipacked_h) $(isstate_h) $(ivmspace_h) \
  $(store_h)

iname.o: iname.c $(GH) $(memory__h) $(string__h) \
  $(gsstruct_h) $(errors_h) $(imemory_h) $(inamedef_h) $(isave_h) $(store_h)

isave.o: isave.c $(GH) \
  $(errors_h) $(gsexit_h) $(gsstruct_h) $(gsutil_h) \
  $(iastate_h) $(inamedef_h) $(isave_h) $(isstate_h) $(ivmspace_h) \
  $(ipacked_h) $(store_h)

### Include files

idparam_h=idparam.h
ilevel_h=ilevel.h
iparam_h=iparam.h $(gsparam_h)
istack_h=istack.h
iutil_h=iutil.h
opcheck_h=opcheck.h
opextern_h=opextern.h
# Nested include files
dstack_h=dstack.h $(istack_h)
estack_h=estack.h $(istack_h)
ostack_h=ostack.h $(istack_h)
oper_h=oper.h $(iutil_h) $(opcheck_h) $(opdef_h) $(opextern_h) $(ostack_h)

idebug.o: idebug.c $(GH) \
  $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(istack_h) $(iutil_h) $(ivmspace_h) \
  $(ostack_h) $(opdef_h) $(ipacked_h) $(store_h)

idict.o: idict.c $(GH) $(errors_h) \
  $(ialloc_h) $(idebug_h) $(ivmspace_h) $(inamedef_h) $(ipacked_h) \
  $(isave_h) $(store_h) $(iutil_h) $(idict_h) $(dstack_h)

idparam.o: idparam.c $(GH) $(errors_h) \
  $(gsmatrix_h) $(gsuid_h) \
  $(idict_h) $(idparam_h) $(ilevel_h) $(imemory_h) $(iname_h) $(iutil_h) \
  $(oper_h) $(store_h)

iparam.o: iparam.c $(GH) \
  $(ialloc_h) $(idict_h) $(iname_h) $(imemory_h) $(iparam_h) $(istack_h) $(iutil_h) $(ivmspace_h) \
  $(opcheck_h) $(store_h)

istack.o: istack.c $(GH) $(memory__h) \
  $(errors_h) $(gsstruct_h) $(gsutil_h) \
  $(ialloc_h) $(istack_h) $(istruct_h) $(iutil_h) $(ivmspace_h) $(store_h)

iutil.o: iutil.c $(GH) $(math__h) $(memory__h) $(string__h) \
  $(errors_h) $(idict_h) $(imemory_h) $(iutil_h) $(ivmspace_h) \
  $(iname_h) $(ipacked_h) $(store_h) \
  $(gsmatrix_h) $(gsutil_h)

# ======================== PostScript Level 1 ======================== #

###### Include files

files_h=files.h
fname_h=fname.h
ichar_h=ichar.h
icharout_h=icharout.h
icolor_h=icolor.h
icsmap_h=icsmap.h
ifont_h=ifont.h $(gsstruct_h)
iimage_h=iimage.h
imain_h=imain.h $(gsexit_h)
imainarg_h=imainarg.h
iminst_h=iminst.h $(imain_h)
interp_h=interp.h
iparray_h=iparray.h
iscannum_h=iscannum.h
istream_h=istream.h
main_h=main.h $(iminst_h)
overlay_h=overlay.h
sa85x_h=sa85x.h
sbtx_h=sbtx.h
sbwbs_h=sbwbs.h
scanchar_h=scanchar.h
sdct_h=sdct.h
sfilter_h=sfilter.h $(gstypes_h)
shcgen_h=shcgen.h
sjpeg_h=sjpeg.h
smtf_h=smtf.h
sstring_h=sstring.h
# Nested include files
bfont_h=bfont.h $(ifont_h)
ifilter_h=ifilter.h $(istream_h) $(ivmspace_h)
igstate_h=igstate.h $(gsstate_h) $(gxstate_h) $(istruct_h)
iscan_h=iscan.h $(sa85x_h) $(sstring_h)
sbhc_h=sbhc.h $(shc_h)
# Include files for optional features
ibnum_h=ibnum.h

### Initialization and scanning

iconfig=iconfig
$(iconfig).o: iconf.c $(stdio__h) \
  $(gconfig_h) $(gscdefs_h) $(gsmemory_h) \
  $(files_h) $(imain_h) $(iref_h) $(ivmspace_h) $(opdef_h) $(stream_h)
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) iconf.c $(iconfig).c
	$(CCC) $(iconfig).c
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c

iinit.o: iinit.c $(GH) \
  $(gscdefs_h) $(gsexit_h) $(gsstruct_h) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h) \
  $(ilevel_h) $(iname_h) $(interp_h) $(opdef_h) \
  $(ipacked_h) $(iparray_h) $(iutil_h) $(ivmspace_h) $(store_h)

iscan.o: iscan.c $(GH) $(ctype__h) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h) $(files_h) \
  $(ilevel_h) $(iutil_h) $(iscan_h) $(iscannum_h) $(istruct_h) $(ivmspace_h) \
  $(iname_h) $(ipacked_h) $(iparray_h) $(istream_h) $(ostack_h) $(store_h) \
  $(stream_h) $(strimpl_h) $(sfilter_h) $(scanchar_h)

iscannum.o: iscannum.c $(GH) $(math__h) \
  $(errors_h) $(iscannum_h) $(scanchar_h) $(store_h) $(stream_h)

iscantab.o: iscantab.c $(AK) \
  $(stdpre_h) $(scommon_h) $(scanchar_h)

### Streams

# Define which implementation of files we are using (sfile or sfileno).
# This can be overridden on the `make' command line.
#sfile_impl=sfileno
sfile_impl=sfile

sfile.o: sfile.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfileno.o: sfileno.c $(AK) $(stdio__h) $(errno__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfilter1.o: sfilter1.c $(AK) $(stdio__h) $(memory__h) \
  $(sfilter_h) $(strimpl_h)

sstring.o: sstring.c $(AK) $(stdio__h) $(memory__h) $(string__h) \
  $(scanchar_h) $(sstring_h) $(strimpl_h)

###### Operators

OP=$(GH) $(errors_h) $(oper_h)

### Non-graphics operators

zarith.o: zarith.c $(OP) $(math__h) $(store_h)

zarray.o: zarray.c $(OP) $(ialloc_h) $(ipacked_h) $(store_h)

zcontrol.o: zcontrol.c $(OP) \
  $(estack_h) $(ipacked_h) $(iutil_h) $(store_h)

zdict.o: zdict.c $(OP) \
  $(dstack_h) $(idict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(ivmspace_h) \
  $(store_h)

zfile.o: zfile.c $(OP) $(stat__h) $(gp_h) \
  $(gsstruct_h) $(gxiodev_h) \
  $(ialloc_h) $(estack_h) $(files_h) $(fname_h) $(ilevel_h) $(interp_h) $(iutil_h) \
  $(isave_h) $(main_h) $(sfilter_h) $(stream_h) $(strimpl_h) $(store_h)

zfname.o: zfname.c $(OP) \
  $(fname_h) $(gxiodev_h) $(ialloc_h) $(stream_h)

zfileio.o: zfileio.c $(OP) $(gp_h) \
  $(files_h) $(ifilter_h) $(store_h) $(stream_h) $(strimpl_h) \
  $(gsmatrix_h) $(gxdevice_h) $(gxdevmem_h)

zfilter.o: zfilter.c $(OP) \
  $(gsstruct_h) $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
  $(sfilter_h) $(srlx_h) $(sstring_h) $(stream_h) $(strimpl_h)

zfproc.o: zfproc.c $(GH) $(errors_h) $(oper_h) \
  $(estack_h) $(files_h) $(gsstruct_h) $(ialloc_h) $(ifilter_h) $(istruct_h) \
  $(store_h) $(stream_h) $(strimpl_h)

zgeneric.o: zgeneric.c $(OP) \
  $(idict_h) $(estack_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(store_h)

ziodev.o: ziodev.c $(OP) $(memory__h) $(string__h) $(gp_h) $(gpcheck_h) \
  $(gsstruct_h) $(gxiodev_h) \
  $(files_h) $(ialloc_h) $(ivmspace_h) $(store_h) $(stream_h)

zmath.o: zmath.c $(OP) $(math__h) $(gxfarith_h) $(store_h)

zmisc.o: zmisc.c $(OP) $(gscdefs_h) $(gp_h) \
  $(errno__h) $(memory__h) $(string__h) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(iname_h) $(ivmspace_h) $(ipacked_h) $(store_h)

zpacked.o: zpacked.c $(OP) \
  $(ialloc_h) $(idict_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(iparray_h) \
  $(istack_h) $(store_h)

zrelbit.o: zrelbit.c $(OP) $(gsutil_h) $(store_h) $(idict_h)

zstack.o: zstack.c $(OP) $(ialloc_h) $(istack_h) $(store_h)

zstring.o: zstring.c $(OP) $(gsutil_h) \
  $(ialloc_h) $(iname_h) $(ivmspace_h) $(store_h)

zsysvm.o: zsysvm.c $(OP)

ztoken.o: ztoken.c $(OP) \
  $(estack_h) $(files_h) $(gsstruct_h) $(iscan_h) \
  $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

ztype.o: ztype.c $(OP) $(math__h) \
  $(dstack_h) $(idict_h) $(imemory_h) $(iname_h) \
  $(iscan_h) $(iutil_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

# Note that zusparam includes both Level 1 and Level 2 operators.
zusparam.o: zusparam.c $(OP) \
  $(gscdefs_h) $(gsfont_h) $(gsstruct_h) $(gsutil_h) $(gxht_h) \
  $(ialloc_h) $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h) \
  $(iname_h) $(iutil2_h) $(store_h)

zvmem.o: zvmem.c $(OP) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(estack_h) $(isave_h) $(igstate_h) $(store_h) \
  $(gsmatrix_h) $(gsstate_h) $(gsstruct_h)

### Graphics operators

zchar.o: zchar.c $(OP) \
  $(gsstruct_h) $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gschar_h) $(gxdevice_h) $(gxfont_h) $(gzpath_h) $(gzstate_h) \
  $(ialloc_h) $(ichar_h) $(idict_h) $(ifont_h) $(estack_h) \
  $(ilevel_h) $(iname_h) $(igstate_h) $(ipacked_h) $(store_h)

# zcharout is used for Type 1 and Type 42 fonts only.
zcharout.o: zchar.c $(OP) \
  $(gschar_h) $(gxdevice_h) $(gxfont_h) \
  $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h) \
  $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zcolor.o: zcolor.c $(OP) \
  $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gxdevice_h) $(gxcmap_h) \
  $(ialloc_h) $(icolor_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zdevice.o: zdevice.c $(OP) \
  $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(interp_h) $(iparam_h) $(ivmspace_h) \
  $(gsmatrix_h) $(gsstate_h) $(gxdevice_h) $(store_h)

zfont.o: zfont.c $(OP) \
  $(gsmatrix_h) $(gxdevice_h) $(gxfont_h) $(gxfcache_h) \
  $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(isave_h) $(ivmspace_h) \
  $(bfont_h) $(store_h)

zfont2.o: zfont2.c $(OP) \
  $(gsmatrix_h) $(gxdevice_h) $(gschar_h) $(gxfixed_h) $(gxfont_h) \
  $(ialloc_h) $(bfont_h) $(idict_h) $(idparam_h) $(ilevel_h) $(iname_h) $(istruct_h) \
  $(ipacked_h) $(store_h)

zgstate.o: zgstate.c $(OP) \
  $(gsmatrix_h) $(ialloc_h) $(idict_h) $(igstate_h) $(istruct_h) $(store_h)

zht.o: zht.c $(OP) \
  $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
  $(ialloc_h) $(estack_h) $(igstate_h) $(store_h)

zmatrix.o: zmatrix.c $(OP) $(gsmatrix_h) $(igstate_h) $(gscoord_h) $(store_h)

zpaint.o: zpaint.c $(OP) \
  $(estack_h) $(ialloc_h) $(ifilter_h) $(igstate_h) $(iimage_h) $(ilevel_h) \
  $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gspaint_h) $(gsstruct_h) \
  $(store_h) $(stream_h)

zpath.o: zpath.c $(OP) $(math__h) \
  $(gsmatrix_h) $(gspath_h) $(igstate_h) $(store_h)

# Define the base PostScript language interpreter.
# This is the subset of PostScript Level 1 required by our PDF reader.

INT1=idebug.o idict.o idparam.o iinit.o
INT2=interp.o iparam.o ireclaim.o
INT3=iscan.o iscannum.o iscantab.o istack.o iutil.o
INT4=$(sfile_impl).o sfilter1.o sstring.o stream.o
Z1=zarith.o zarray.o zcontrol.o zdict.o
Z1OPS=zarith zarray zcontrol zdict
Z2=zfile.o zfileio.o zfilter.o zfname.o zfproc.o
Z2OPS=zfile zfileio zfilter zfproc
Z3=zgeneric.o ziodev.o zmath.o zmisc.o zpacked.o
Z3OPS=zgeneric ziodev zmath zmisc zpacked
Z4=zrelbit.o zstack.o zstring.o zsysvm.o
Z4OPS=zrelbit zstack zstring zsysvm
Z5=ztoken.o ztype.o zusparam.o zvmem.o
Z5OPS=ztoken ztype zusparam zvmem
Z6=zchar.o zcolor.o zdevice.o zfont.o zfont2.o
Z6OPS=zchar zcolor zdevice zfont zfont2
Z7=zgstate.o zht.o zmatrix.o zpaint.o zpath.o
Z7OPS=zgstate zht zmatrix zpaint zpath
# We have to be a little underhanded with *config.o so as to avoid
# circular definitions.
INT_OBJS=imainarg.o imain.o \
  $(INT1) $(INT2) $(INT3) $(INT4) \
  $(Z1) $(Z2) $(Z3) $(Z4) $(Z5) $(Z6) $(Z7)
INT_CONFIG=$(gconfig).o $(gscdefs).o $(iconfig).o \
  iccinit$(COMPILE_INITS).o
INT_ALL=$(INT_OBJS) $(INT_CONFIG)
# We omit libcore.dev, which should be included here, because problems
# with the Unix linker require libcore to appear last in the link list
# when libcore is really a library.
# We omit $(INT_CONFIG) from the dependency list because they have special
# dependency requirements and are added to the link list at the very end.
# zfilter.c shouldn't include the RLE and RLD filters, but we don't want to
# change this now.
psbase.dev: $(INT_MAK) $(ECHOGS_XE) $(INT_OBJS) isupport.dev rld.dev rle.dev
	$(SETMOD) psbase imainarg.o imain.o
	$(ADDMOD) psbase -obj $(INT_CONFIG)
	$(ADDMOD) psbase -obj $(INT1)
	$(ADDMOD) psbase -obj $(INT2)
	$(ADDMOD) psbase -obj $(INT3)
	$(ADDMOD) psbase -obj $(INT4)
	$(ADDMOD) psbase -obj $(Z1)
	$(ADDMOD) psbase -oper $(Z1OPS)
	$(ADDMOD) psbase -obj $(Z2)
	$(ADDMOD) psbase -oper $(Z2OPS)
	$(ADDMOD) psbase -obj $(Z3)
	$(ADDMOD) psbase -oper $(Z3OPS)
	$(ADDMOD) psbase -obj $(Z4)
	$(ADDMOD) psbase -oper $(Z4OPS)
	$(ADDMOD) psbase -obj $(Z5)
	$(ADDMOD) psbase -oper $(Z5OPS)
	$(ADDMOD) psbase -obj $(Z6)
	$(ADDMOD) psbase -oper $(Z6OPS)
	$(ADDMOD) psbase -obj $(Z7)
	$(ADDMOD) psbase -oper $(Z7OPS)
	$(ADDMOD) psbase -iodev stdin stdout stderr lineedit statementedit
	$(ADDMOD) psbase -include isupport rld rle

# -------------------------- Feature definitions -------------------------- #

# ---------------- Full Level 1 interpreter ---------------- #

level1.dev: $(INT_MAK) $(ECHOGS_XE) psbase.dev bcp.dev hsb.dev path1.dev type1.dev
	$(SETMOD) level1 -include psbase bcp hsb path1 type1
	$(ADDMOD) level1 -emulator PostScript PostScriptLevel1

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

color.dev: $(INT_MAK) $(ECHOGS_XE) cmyklib.dev cmykread.dev
	$(SETMOD) color -include cmyklib cmykread

cmykread_=zcolor1.o zht1.o
cmykread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmykread_)
	$(SETMOD) cmykread $(cmykread_)
	$(ADDMOD) cmykread -oper zcolor1 zht1

zcolor1.o: zcolor1.c $(OP) \
  $(gscolor1_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) \
  $(ialloc_h) $(icolor_h) $(iimage_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zht1.o: zht1.c $(OP) \
  $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
  $(ialloc_h) $(estack_h) $(igstate_h) $(store_h)

# ---------------- HSB color ---------------- #

hsb_=zhsb.o
hsb.dev: $(INT_MAK) $(ECHOGS_XE) $(hsb_) hsblib.dev
	$(SETMOD) hsb $(hsb_)
	$(ADDMOD) hsb -include hsblib
	$(ADDMOD) hsb -oper zhsb

zhsb.o: zhsb.c $(OP) \
  $(gshsb_h) $(igstate_h) $(store_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1_=zpath1.o
path1.dev: $(INT_MAK) $(ECHOGS_XE) $(path1_) path1lib.dev
	$(SETMOD) path1 $(path1_)
	$(ADDMOD) path1 -include path1lib
	$(ADDMOD) path1 -oper zpath1

zpath1.o: zpath1.c $(OP) \
  $(ialloc_h) $(estack_h) $(gspath_h) $(gsstruct_h) $(igstate_h) $(store_h)

# ================ Level-independent PostScript options ================ #

# ---------------- BCP filters ---------------- #

bcp_=sbcp.o zfbcp.o
bcp.dev: $(INT_MAK) $(ECHOGS_XE) $(bcp_)
	$(SETMOD) bcp $(bcp_)
	$(ADDMOD) bcp -oper zfbcp

sbcp.o: sbcp.c $(AK) $(stdio__h) \
  $(sfilter_h) $(strimpl_h)

zfbcp.o: zfbcp.c $(OP) \
  $(gsstruct_h) $(ialloc_h) $(ifilter_h) \
  $(sfilter_h) $(stream_h) $(strimpl_h)

# ---------------- Incremental font loading ---------------- #
# (This only works for Type 1 fonts without eexec encryption.)

diskfont.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) diskfont -ps gs_diskf

# ---------------- Double-precision floats ---------------- #

double_=zdouble.o
double.dev: $(INT_MAK) $(ECHOGS_XE) $(double_)
	$(SETMOD) double $(double_)
	$(ADDMOD) double -oper zdouble

zdouble.o: zdouble.c $(OP) $(ctype__h) $(math__h) $(memory__h) $(string__h) \
  $(gxfarith_h) $(store_h)

# ---------------- RasterOp ---------------- #
# This should be a separable feature in the core also....

rasterop.dev: $(INT_MAK) $(ECHOGS_XE) roplib.dev ropread.dev
	$(SETMOD) rasterop -include roplib ropread

ropread_=zrop.o
ropread.dev: $(INT_MAK) $(ECHOGS_XE) $(ropread_)
	$(SETMOD) ropread $(ropread_)
	$(ADDMOD) ropread -oper zrop

zrop.o: zrop.c $(OP) $(memory__h) \
  $(gsrop_h) $(gxdevice_h) $(idict_h) $(idparam_h) $(igstate_h) $(store_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1.dev: $(INT_MAK) $(ECHOGS_XE) psf1lib.dev psf1read.dev
	$(SETMOD) type1 -include psf1lib psf1read

psf1read_=seexec.o zchar1.o zcharout.o zfont1.o zmisc1.o
psf1read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf1read_)
	$(SETMOD) psf1read $(psf1read_)
	$(ADDMOD) psf1read -oper zchar1 zfont1 zmisc1
	$(ADDMOD) psf1read -ps gs_type1

seexec.o: seexec.c $(AK) $(stdio__h) \
  $(gscrypt1_h) $(scanchar_h) $(sfilter_h) $(strimpl_h)

zchar1.o: zchar1.c $(OP) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gzstate_h) \
  $(estack_h) $(ialloc_h) $(ichar_h) $(icharout_h) \
  $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zfont1.o: zfont1.c $(OP) \
  $(gsmatrix_h) $(gxdevice_h) $(gschar_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont1_h) \
  $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(store_h)

zmisc1.o: zmisc1.c $(OP) \
  $(gscrypt1_h) $(ifilter_h) $(sfilter_h) $(stream_h) $(strimpl_h)

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

# Native TrueType support
ttfont.dev: $(INT_MAK) $(ECHOGS_XE) type42.dev
	$(SETMOD) ttfont -include type42
	$(ADDMOD) ttfont -ps gs_mro_e gs_wan_e gs_ttf

# Type 42 (embedded TrueType) support
type42read_=zchar42.o zcharout.o zfont42.o
type42.dev: $(INT_MAK) $(ECHOGS_XE) $(type42read_) ttflib.dev
	$(SETMOD) type42 $(type42read_)
	$(ADDMOD) type42 -include ttflib	
	$(ADDMOD) type42 -oper zchar42 zfont42
	$(ADDMOD) type42 -ps gs_typ42

zchar42.o: zchar42.c $(OP) \
  $(gsmatrix_h) $(gspaint_h) $(gspath_h) \
  $(gxfixed_h) $(gxchar_h) $(gxfont_h) $(gxfont42_h) \
  $(gxistate_h) $(gxpath_h) $(gzstate_h) \
  $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h) \
  $(ifont_h) $(igstate_h) $(store_h)

zfont42.o: zfont42.c $(OP) \
  $(gsccode_h) $(gsmatrix_h) $(gxfont_h) $(gxfont42_h) \
  $(bfont_h) $(idict_h) $(idparam_h) $(store_h)

# ======================== Precompilation options ======================== #

# ---------------- Precompiled fonts ---------------- #
# See fonts.txt for more information.

ccfont_h=ccfont.h $(std_h) $(gsmemory_h) $(iref_h) $(ivmspace_h) $(store_h)

CCFONT=$(OP) $(ccfont_h)

# List the fonts we are going to compile.
# Because of intrinsic limitations in `make', we have to list
# the object file names and the font names separately.
# Because of limitations in the DOS shell, we have to break the fonts up
# into lists that will fit on a single line (120 characters).
# The rules for constructing the .c files from the fonts themselves,
# and for compiling the .c files, are in cfonts.mak, not here.
# For example, to compile the Courier fonts, you should invoke
#	make -f cfonts.mak Courier_o
# By convention, the names of the 35 standard compiled fonts use '0' for
# the foundry name.  This allows users to substitute different foundries
# without having to change this makefile.
ccfonts_ps=gs_ccfnt
ccfonts1_=0agk.o 0agko.o 0agd.o 0agdo.o
ccfonts1=agk agko agd agdo
ccfonts2_=0bkl.o 0bkli.o 0bkd.o 0bkdi.o
ccfonts2=bkl bkli bkd bkdi
ccfonts3_=0crr.o 0cri.o 0crb.o 0crbi.o
ccfonts3=crr cri crb crbi
ccfonts4_=0hvr.o 0hvro.o 0hvb.o 0hvbo.o
ccfonts4=hvr hvro hvb hvbo
ccfonts5_=0hvrrn.o 0hvrorn.o 0hvbrn.o 0hvborn.o
ccfonts5=hvrrn hvrorn hvbrn hvborn
ccfonts6_=0ncr.o 0ncri.o 0ncb.o 0ncbi.o
ccfonts6=ncr ncri ncb ncbi
ccfonts7_=0plr.o 0plri.o 0plb.o 0plbi.o
ccfonts7=plr plri plb plbi
ccfonts8_=0tmr.o 0tmri.o 0tmb.o 0tmbi.o
ccfonts8=tmr tmri tmb tmbi
ccfonts9_=0syr.o 0zcmi.o 0zdr.o
ccfonts9=syr zcmi zdr
# The free distribution includes Bitstream Charter, Utopia, and
# freeware Cyrillic and Kana fonts.  We only provide for compiling
# Charter and Utopia.
ccfonts10free_=bchr.o bchri.o bchb.o bchbi.o
ccfonts10free=chr chri chb chbi
ccfonts11free_=putr.o putri.o putb.o putbi.o
ccfonts11free=utr utri utb utbi
# Uncomment the next 2 lines if you want Charter and Utopia compiled in.
#ccfonts10_=$(ccfonts10free_)
#ccfonts10=$(ccfonts10free)
#ccfonts11_=$(ccfonts11free_)
#ccfonts11=$(ccfonts11free)
# Add your own fonts here if desired.
ccfonts12_=
ccfonts12=
ccfonts13_=
ccfonts13=
ccfonts14_=
ccfonts14=
ccfonts15_=
ccfonts15=
# Select either standard or free configuration here.
# By default, we select the standard configuration.
#ccfonts3_=$(ccfonts3free_)
#ccfonts3=$(ccfonts3free)
#ccfonts9_=$(ccfonts9free_)
#ccfonts9=$(ccfonts9free)
ccfonts3_=$(ccfonts3std_)
ccfonts3=$(ccfonts3std)
ccfonts9_=$(ccfonts9std_)
ccfonts9=$(ccfonts9std)

# It's OK for ccfonts_.dev not to be CONFIG-dependent, because it only
# exists during the execution of the following rule.
$(gconfigf_h): $(MAKEFILE) $(INT_MAK) $(GENCONF_XE)
	$(SETMOD) ccfonts_ -font $(ccfonts1)
	$(ADDMOD) ccfonts_ -font $(ccfonts2)
	$(ADDMOD) ccfonts_ -font $(ccfonts3)
	$(ADDMOD) ccfonts_ -font $(ccfonts4)
	$(ADDMOD) ccfonts_ -font $(ccfonts5)
	$(ADDMOD) ccfonts_ -font $(ccfonts6)
	$(ADDMOD) ccfonts_ -font $(ccfonts7)
	$(ADDMOD) ccfonts_ -font $(ccfonts8)
	$(ADDMOD) ccfonts_ -font $(ccfonts9)
	$(ADDMOD) ccfonts_ -font $(ccfonts10)
	$(ADDMOD) ccfonts_ -font $(ccfonts11)
	$(ADDMOD) ccfonts_ -font $(ccfonts12)
	$(ADDMOD) ccfonts_ -font $(ccfonts13)
	$(ADDMOD) ccfonts_ -font $(ccfonts14)
	$(ADDMOD) ccfonts_ -font $(ccfonts15)
	./genconf ccfonts_.dev -f $(gconfigf_h)

# We separate icfontab.dev from ccfonts.dev so that a customer can put
# compiled fonts into a separate shared library.

icfontab=icfontab

# Define ccfont_table separately, so it can be set from the command line
# to select an alternate compiled font table.
ccfont_table=$(icfontab)

$(icfontab).dev: $(MAKEFILE) $(INT_MAK) $(ECHOGS_XE) $(icfontab).o \
  $(ccfonts1_) $(ccfonts2_) $(ccfonts3_) $(ccfonts4_) $(ccfonts5_) \
  $(ccfonts6_) $(ccfonts7_) $(ccfonts8_) $(ccfonts9_) $(ccfonts10_) \
  $(ccfonts11_) $(ccfonts12_) $(ccfonts13_) $(ccfonts14_) $(ccfonts15_)
	$(SETMOD) $(icfontab) -obj $(icfontab).o
	$(ADDMOD) $(icfontab) -obj $(ccfonts1_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts2_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts3_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts4_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts5_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts6_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts7_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts8_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts9_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts10_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts11_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts12_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts13_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts14_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts15_)

$(icfontab).o: icfontab.c $(AK) $(ccfont_h) $(gconfigf_h)
	$(CP_) $(gconfigf_h) gconfigf.h
	$(CCCF) icfontab.c

# Strictly speaking, ccfonts shouldn't need to include type1,
# since one could choose to precompile only Type 0 fonts,
# but getting this exactly right would be too much work.
ccfonts=ccfonts
$(ccfonts).dev: $(MAKEFILE) $(INT_MAK) type1.dev iccfont.o \
  $(ccfont_table).dev
	$(SETMOD) $(ccfonts) -include type1
	$(ADDMOD) $(ccfonts) -include $(ccfont_table)
	$(ADDMOD) $(ccfonts) -obj iccfont.o
	$(ADDMOD) $(ccfonts) -oper ccfonts
	$(ADDMOD) $(ccfonts) -ps $(ccfonts_ps)

iccfont.o: iccfont.c $(GH) \
  $(gsstruct_h) $(ccfont_h) $(errors_h) \
  $(ialloc_h) $(idict_h) $(ifont_h) $(iname_h) $(isave_h) $(iutil_h) \
  $(oper_h) $(ostack_h) $(store_h) $(stream_h) $(strimpl_h) $(sfilter_h) $(iscan_h)
	$(CCCF) iccfont.c

# ---------------- Compiled initialization code ---------------- #

# We select either iccinit0 or iccinit1 depending on COMPILE_INITS.

iccinit0.o: iccinit0.c $(stdpre_h)
	$(CCCF) iccinit0.c

iccinit1.o: gs_init.o
	$(CP_) gs_init.o iccinit1.o

# All the gs_*.ps files should be prerequisites of gs_init.c,
# but we don't have any convenient list of them.
gs_init.c: $(GS_INIT) $(GENINIT_XE) $(gconfig_h)
	./geninit $(GS_INIT) $(gconfig_h) -c gs_init.c

gs_init.o: gs_init.c $(stdpre_h)
	$(CCCF) gs_init.c

# ======================== PostScript Level 2 ======================== #

###### Include files

iutil2_h=iutil2.h

### In addition to the true Level 2 configuration, we define a 'minimal'
### Level 2 that can be used with -dDEBUG in the 16-bit Windows environment.
### This also may require trimming down the sizes of the stacks in interp.c.

lev2min.dev: $(INT_MAK) $(ECHOGS_XE) \
  psbase.dev devctrl.dev color.dev \
  dps2lib.dev dps2read.dev fdecode.dev path1.dev type1.dev \
  psl2lib.dev psl2read.dev
	$(SETMOD) lev2min -include psbase devctrl color
	$(ADDMOD) lev2min -include dps2lib dps2read fdecode path1 type1
	$(ADDMOD) lev2min -include psl2lib psl2read
	$(ADDMOD) lev2min -emulator PostScript PostScriptLevel1 PostScriptLevel2

level2.dev: $(INT_MAK) $(ECHOGS_XE) \
  cie.dev compfont.dev dct.dev devctrl.dev dpsand2.dev filter.dev \
  level1.dev pattern.dev psl2lib.dev psl2read.dev sepr.dev type42.dev \
  xfilter.dev
	$(SETMOD) level2 -include cie compfont dct devctrl dpsand2 filter
	$(ADDMOD) level2 -include level1 pattern psl2lib psl2read sepr xfilter
	$(ADDMOD) level2 -emulator PostScript PostScriptLevel2

psl2read1_=iutil2.o zcolor2.o zcsindex.o
psl2read2_=zht2.o zimage2.o zmisc2.o
psl2read_=$(psl2read1_) $(psl2read2_)
# Note that zmisc2 includes both Level 1 and Level 2 operators.
psl2read.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2read_)
	$(SETMOD) psl2read $(psl2read1_)
	$(ADDMOD) psl2read -obj $(psl2read2_)
	$(ADDMOD) psl2read -oper zmisc2
	$(ADDMOD) psl2read -oper zcolor2_l2 zcsindex_l2
	$(ADDMOD) psl2read -oper zht2_l2 zimage2_l2
	$(ADDMOD) psl2read -ps gs_lev2 gs_res

iutil2.o: iutil2.c $(GXERR) $(memory__h) \
  $(gsparam_h) $(gsutil_h) \
  $(opcheck_h) $(idict_h) $(imemory_h) $(iutil_h) $(iutil2_h)

zcolor2.o: zcolor2.c $(OP) \
  $(gscolor_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h) \
  $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h) \
  $(store_h)

zcsindex.o: zcsindex.c $(OP) $(memory__h) \
  $(gscolor_h) $(gsstruct_h) $(gxfixed_h) $(gxcolor2_h) $(gxcspace_h) $(gsmatrix_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

zht2.o: zht2.c $(OP) \
  $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
  $(estack_h) $(ialloc_h) $(icolor_h) $(idict_h) $(idparam_h) $(igstate_h) \
  $(store_h)

zimage2.o: zimage2.c $(OP) $(math__h) \
  $(gscolor_h) $(gscolor2_h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h) \
  $(idict_h) $(idparam_h) $(iimage_h) $(ilevel_h) $(igstate_h)

zmisc2.o: zmisc2.c $(OP) \
  $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h) \
  $(ilevel_h) $(iname_h) $(iutil2_h) $(ivmspace_h) $(store_h)

# ---------------- Device control ---------------- #
# This is a catch-all for setpagedevice and IODevices.

devctrl_=zdevice2.o ziodev2.o zmedia2.o zdevcal.o
devctrl.dev: $(INT_MAK) $(ECHOGS_XE) $(devctrl_)
	$(SETMOD) devctrl $(devctrl_)
	$(ADDMOD) devctrl -oper zdevice2_l2 ziodev2_l2 zmedia2_l2
	$(ADDMOD) devctrl -iodev null ram calendar
	$(ADDMOD) devctrl -ps gs_setpd

zdevice2.o: zdevice2.c $(OP) $(math__h) \
  $(dstack_h) $(estack_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h) $(store_h) \
  $(gxdevice_h) $(gsstate_h)

ziodev2.o: ziodev2.c $(OP) \
  $(gxiodev_h) $(stream_h) $(files_h) $(iparam_h) $(iutil2_h) $(store_h)

zmedia2.o: zmedia2.c $(OP) $(math__h) $(memory__h) \
  $(gsmatrix_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h)

zdevcal.o: zdevcal.c $(GH) $(time__h) \
  $(gxiodev_h) $(iparam_h) $(istack_h)

# ---------------- Filters other than the ones in sfilter.c ---------------- #

# Standard Level 2 decoding filters only.  The PDF configuration uses this.
fdecode_=sfilter2.o zfdecode.o
fdecode.dev: $(INT_MAK) $(ECHOGS_XE) $(fdecode_) cfd.dev lzwd.dev pdiff.dev pngp.dev rld.dev
	$(SETMOD) fdecode $(fdecode_)
	$(ADDMOD) fdecode -include cfd lzwd pdiff pngp rld
	$(ADDMOD) fdecode -oper zfdecode

sfilter2.o: sfilter2.c $(AK) $(stdio__h)\
  $(sa85x_h) $(scanchar_h) $(sbtx_h) $(sfilter_h) $(strimpl_h)

zfdecode.o: zfdecode.c $(OP) $(memory__h) \
  $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
  $(sa85x_h) $(scf_h) $(scfx_h) $(sfilter_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) \
  $(store_h) $(strimpl_h)

# Complete Level 2 filter capability.
filter_=zfilter2.o
filter.dev: $(INT_MAK) $(ECHOGS_XE) fdecode.dev $(filter_) cfe.dev lzwe.dev rle.dev
	$(SETMOD) filter -include fdecode
	$(ADDMOD) filter -obj $(filter_)
	$(ADDMOD) filter -include cfe lzwe rle
	$(ADDMOD) filter -oper zfilter2

zfilter2.o: zfilter2.c $(OP) \
  $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(store_h) \
  $(sfilter_h) $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(strimpl_h)

# Extensions beyond Level 2 standard.
xfilter_=sbhc.o sbwbs.o shcgen.o smtf.o \
 zfilterx.o
xfilter.dev: $(INT_MAK) $(ECHOGS_XE) $(xfilter_) pcxd.dev pngp.dev
	$(SETMOD) xfilter $(xfilter_)
	$(ADDMOD) xfilter -include pcxd
	$(ADDMOD) xfilter -oper zfilterx

sbhc.o: sbhc.c $(AK) $(stdio__h) \
  $(gdebug_h) $(sbhc_h) $(shcgen_h) $(strimpl_h)

sbwbs.o: sbwbs.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(sbwbs_h) $(sfilter_h) $(strimpl_h)

shcgen.o: shcgen.c $(AK) $(stdio__h) \
  $(gdebug_h) $(gserror_h) $(gserrors_h) \
  $(scommon_h) $(shc_h) $(shcgen_h)

smtf.o: smtf.c $(AK) $(stdio__h) \
  $(smtf_h) $(strimpl_h)

zfilterx.o: zfilterx.c $(OP) \
  $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
  $(store_h) $(sfilter_h) $(sbhc_h) $(sbtx_h) $(sbwbs_h) $(shcgen_h) \
  $(smtf_h) $(strimpl_h)

# ---------------- Binary tokens ---------------- #

btoken_=iscanbin.o zbseq.o
btoken.dev: $(INT_MAK) $(ECHOGS_XE) $(btoken_)
	$(SETMOD) btoken $(btoken_)
	$(ADDMOD) btoken -oper zbseq_l2
	$(ADDMOD) btoken -ps gs_btokn

bseq_h=bseq.h
btoken_h=btoken.h

iscanbin.o: iscanbin.c $(GH) $(math__h) $(errors_h) \
  $(gsutil_h) $(ialloc_h) $(ibnum_h) $(idict_h) $(iname_h) \
  $(iscan_h) $(iutil_h) $(ivmspace_h) \
  $(bseq_h) $(btoken_h) $(dstack_h) $(ostack_h) \
  $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

zbseq.o: zbseq.c $(OP) \
  $(ialloc_h) $(idict_h) $(isave_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h) \
  $(iname_h) $(ibnum_h) $(btoken_h) $(bseq_h)

# ---------------- User paths & insideness testing ---------------- #

upath_=zupath.o ibnum.o
upath.dev: $(INT_MAK) $(ECHOGS_XE) $(upath_)
	$(SETMOD) upath $(upath_)
	$(ADDMOD) upath -oper zupath_l2

zupath.o: zupath.c $(OP) \
  $(idict_h) $(dstack_h) $(iutil_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h) \
  $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h) \
  $(gxfixed_h) $(gxdevice_h) $(gzpath_h) $(gzstate_h)

# -------- Additions common to Display PostScript and Level 2 -------- #

dpsand2.dev: $(INT_MAK) $(ECHOGS_XE) btoken.dev color.dev upath.dev dps2lib.dev dps2read.dev
	$(SETMOD) dpsand2 -include btoken color upath dps2lib dps2read

dps2read_=ibnum.o zchar2.o zdps1.o zvmem2.o
# Note that zvmem2 includes both Level 1 and Level 2 operators.
dps2read.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2read_)
	$(SETMOD) dps2read $(dps2read_)
	$(ADDMOD) dps2read -oper zvmem2
	$(ADDMOD) dps2read -oper ireclaim_l2 zchar2_l2 zdps1_l2
	$(ADDMOD) dps2read -ps gs_dps1

ibnum.o: ibnum.c $(GH) $(math__h) \
  $(errors_h) $(stream_h) $(ibnum_h) $(imemory_h) $(iutil_h)

zchar2.o: zchar2.c $(OP) \
  $(gschar_h) $(gsmatrix_h) $(gspath_h) $(gsstruct_h) $(gxfixed_h) $(gxfont_h) \
  $(ialloc_h) $(ichar_h) $(estack_h) $(ifont_h) $(iname_h) $(igstate_h) \
  $(store_h) $(stream_h) $(ibnum_h)

zdps1.o: zdps1.c $(OP) \
  $(gsmatrix_h) $(gspath_h) $(gspath2_h) $(gsstate_h) \
  $(ialloc_h) $(ivmspace_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h)

zvmem2.o: zvmem2.c $(OP) \
  $(estack_h) $(ialloc_h) $(ivmspace_h) $(store_h)

# ---------------- Display PostScript ---------------- #

# We should include zcontext, but it isn't in good enough shape yet:
#	$(ADDMOD) dps -oper zcontext_l2
dps_=
dps.dev: $(INT_MAK) $(ECHOGS_XE) dpsand2.dev $(dps_)
	$(SETMOD) dps -include dpsand2
	$(ADDMOD) dps -obj $(dps_)

zcontext.o: zcontext.c $(OP) \
  $(gsstruct_h) $(gsutil_h) \
  $(idict_h) $(istruct_h) $(dstack_h) $(estack_h) $(igstate_h) $(store_h)

# -------- Composite (PostScript Type 0) font support -------- #

compfont.dev: $(INT_MAK) $(ECHOGS_XE) psf0lib.dev psf0read.dev
	$(SETMOD) compfont -include psf0lib psf0read

psf0read_=zchar2.o zfont0.o
psf0read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf0read_)
	$(SETMOD) psf0read $(psf0read_)
	$(ADDMOD) psf0read -oper zfont0 zchar2

zfont0.o: zfont0.c $(OP) \
  $(gsmatrix_h) $(gsstruct_h) $(gxdevice_h) $(gxfont_h) $(gxfont0_h) \
  $(ialloc_h) $(bfont_h) $(idict_h) $(igstate_h) $(store_h)

# ---------------- CID/CMap font support ---------------- #
#**************** NOT REAL YET.  DON'T USE. ****************#

cidfont.dev: $(INT_MAK) $(ECHOGS_XE) compfont.dev psf1read.dev
	$(SETMOD) cidfont -include compfont psf1read
	$(ADDMOD) cidfont -ps gs_cidfn gs_cmap

# ---------------- CIE color ---------------- #

cieread_=zcie.o zcrd.o
cie.dev: $(INT_MAK) $(ECHOGS_XE) $(cieread_) cielib.dev
	$(SETMOD) cie $(cieread_)
	$(ADDMOD) cie -oper zcie_l2 zcrd_l2
	$(ADDMOD) cie -include cielib

icie_h=icie.h

zcie.o: zcie.c $(OP) $(math__h) $(memory__h) \
  $(gscolor2_h) $(gscie_h) $(gsstruct_h) $(gxcspace_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

zcrd.o: zcrd.c $(OP) $(math__h) \
  $(gscspace_h) $(gscolor2_h) $(gscie_h) $(gsstruct_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- Pattern color ---------------- #

pattern.dev: $(INT_MAK) $(ECHOGS_XE) patlib.dev patread.dev
	$(SETMOD) pattern -include patlib patread

patread_=zpcolor.o
patread.dev: $(INT_MAK) $(ECHOGS_XE) $(patread_)
	$(SETMOD) patread $(patread_)
	$(ADDMOD) patread -oper zpcolor_l2

zpcolor.o: zpcolor.c $(OP) \
  $(gscolor_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h) \
  $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h) \
  $(store_h)

# ---------------- Separation color ---------------- #

seprread_=zcssepr.o
sepr.dev: $(INT_MAK) $(ECHOGS_XE) $(seprread_) seprlib.dev
	$(SETMOD) sepr $(seprread_)
	$(ADDMOD) sepr -oper zcssepr_l2
	$(ADDMOD) sepr -include seprlib

zcssepr.o: zcssepr.c $(OP) \
  $(gscolor_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- DCT filters ---------------- #
# The definitions for jpeg*.dev are in jpeg.mak.

dct.dev: $(INT_MAK) $(ECHOGS_XE) dcte.dev dctd.dev
	$(SETMOD) dct -include dcte dctd

# Common code

dctc_=sdctc.o sjpegc.o zfdctc.o

sdctc.o: sdctc.c $(AK) $(stdio__h) \
  $(sdct_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpegc.o: sjpegc.c $(AK) $(stdio__h) $(gx_h)\
  $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
  jerror.h jpeglib.h

zfdctc.o: zfdctc.c $(GH) $(errors_h) $(opcheck_h) \
  $(idict_h) $(idparam_h) $(imemory_h) \
  $(ipacked_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jpeglib.h

# Encoding (compression)

dcte_=$(dctc_) sdcte.o sjpege.o zfdcte.o
dcte.dev: $(INT_MAK) $(ECHOGS_XE) jpege.dev $(dcte_)
	$(SETMOD) dcte -include jpege
	$(ADDMOD) dcte -obj $(dcte_)
	$(ADDMOD) dcte -oper zfdcte

sdcte.o: sdcte.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpege.o: sjpege.c $(AK) $(stdio__h) $(gx_h)\
  $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
  jerror.h jpeglib.h

zfdcte.o: zfdcte.c $(OP) \
  $(idict_h) $(idparam_h) $(ifilter_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jpeglib.h

# Decoding (decompression)

dctd_=$(dctc_) sdctd.o sjpegd.o zfdctd.o
dctd.dev: $(INT_MAK) $(ECHOGS_XE) jpegd.dev $(dctd_)
	$(SETMOD) dctd -include jpegd
	$(ADDMOD) dctd -obj $(dctd_)
	$(ADDMOD) dctd -oper zfdctd

sdctd.o: sdctd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpegd.o: sjpegd.c $(AK) $(stdio__h) $(gx_h)\
  $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
  jerror.h jpeglib.h

zfdctd.o: zfdctd.c $(OP) \
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jpeglib.h

# ---------------- zlib/Flate filters ---------------- #

fzlib.dev: $(INT_MAK) $(ECHOGS_XE) zfzlib.o szlibe.dev szlibd.dev
	$(SETMOD) fzlib -include szlibe szlibd
	$(ADDMOD) fzlib -obj zfzlib.o
	$(ADDMOD) fzlib -oper zfzlib

zfzlib.o: zfzlib.c $(OP) \
  $(errors_h) $(idict_h) $(ifilter_h) \
  $(spdiffx_h) $(spngpx_h) $(strimpl_h) $(szlibx_h)
	$(CCC) -I$(ZSRCDIR) zfzlib.c

# ================================ PDF ================================ #

# We need most of the Level 2 interpreter to do PDF, but not all of it.
# In fact, we don't even need all of a Level 1 interpreter.

# Because of the way the PDF encodings are defined, they must get loaded
# before we install the Level 2 resource machinery.
# On the other hand, the PDF .ps files must get loaded after
# level2dict is defined.
pdfmin.dev: $(INT_MAK) $(ECHOGS_XE) \
  psbase.dev color.dev dps2lib.dev dps2read.dev \
  fdecode.dev type1.dev pdffonts.dev psl2lib.dev psl2read.dev pdfread.dev
	$(SETMOD) pdfmin -include psbase color dps2lib dps2read
	$(ADDMOD) pdfmin -include fdecode type1
	$(ADDMOD) pdfmin -include pdffonts psl2lib psl2read pdfread
	$(ADDMOD) pdfmin -emulator PDF

pdf.dev: $(INT_MAK) $(ECHOGS_XE) \
  pdfmin.dev cie.dev dctd.dev
	$(SETMOD) pdf -include pdfmin cie dctd

# Reader only

pdffonts.dev: $(INT_MAK) $(ECHOGS_XE) \
  gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps
	$(SETMOD) pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

# pdf_2ps must be the last .ps file loaded.
pdfread.dev: $(INT_MAK) $(ECHOGS_XE) fzlib.dev
	$(SETMOD) pdfread -include fzlib
	$(ADDMOD) pdfread -ps gs_pdf gs_l2img
	$(ADDMOD) pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec
	$(ADDMOD) pdfread -ps pdf_2ps

# ============================= Main program ============================== #

gs.o: gs.c $(GH) \
  $(imain_h) $(imainarg_h) $(iminst_h)

imainarg.o: imainarg.c $(GH) $(ctype__h) $(memory__h) $(string__h) \
  $(gp_h) \
  $(gscdefs_h) $(gsdevice_h) $(gsmdebug_h) $(gxdevice_h) $(gxdevmem_h) \
  $(errors_h) $(estack_h) $(files_h) \
  $(ialloc_h) $(imain_h) $(imainarg_h) $(iminst_h) \
  $(iname_h) $(interp_h) $(iscan_h) $(iutil_h) $(ivmspace_h) \
  $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

imain.o: imain.c $(GH) \
  $(gp_h) $(gslib_h) $(gsmatrix_h) $(gsutil_h) $(gxdevice_h) \
  $(dstack_h) $(errors_h) $(estack_h) $(files_h) \
  $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(interp_h) \
  $(isave_h) $(iscan_h) $(ivmspace_h) \
  $(main_h) $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(strimpl_h)

interp.o: interp.c $(GH) $(memory__h) $(string__h) \
  $(dstack_h) $(errors_h) $(estack_h) $(files_h) \
  $(ialloc_h) $(iastruct_h) $(inamedef_h) $(idict_h) $(interp_h) $(ipacked_h) \
  $(iscan_h) $(isave_h) $(istack_h) $(iutil_h) $(ivmspace_h) \
  $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(CCINT) interp.c

ireclaim.o: ireclaim.c $(GH) \
  $(errors_h) $(gsstruct_h) $(iastate_h) $(opdef_h) $(store_h) \
  $(dstack_h) $(estack_h) $(ostack_h)


# Encoding (compression) code.

jpege.dev: jpege$(JVERSION).dev
	$(CP_) jpege$(JVERSION).dev jpege.dev

# Decompression code

jpegd.dev:	$(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpegd6) $(jpegd_1) $(jpegd_2) $(jpegd_3)
	$(SETMOD) jpegd6 $(jpegd6)
	$(ADDMOD) jpegd6 -include jpegc
	$(ADDMOD) jpegd6 -obj $(jpegd_1)
	$(ADDMOD) jpegd6 -obj $(jpegd_2)
	$(ADDMOD) jpegd6 -obj $(jpegd_3)

# Define the version of libpng.dev that we are actually using.
libpng.dev: $(LIBPNG_MAK) $(ECHOGS_XE)
	$(SETMOD) libpng_1 -lib png

# Code common to compression and decompression.

zlibc_=zutil.o
zlibc.dev: $(ZLIB_MAK) $(ECHOGS_XE) $(zlibc_)
	$(SETMOD) zlibc $(zlibc_)

zutil.o: $(ZSRC)zutil.c $(ZDEP)
	$(CCCZ) $(ZSRC)zutil.c

# Encoding (compression) code.

zlibe.dev: $(MAKEFILE) zlibe_$(SHARE_ZLIB).dev
	$(CP_) zlibe_$(SHARE_ZLIB).dev zlibe.dev

zlibe_1.dev: $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) zlibe_1 -lib gz


###### ----------------------- Device support ----------------------- ######

# Provide a mapping between StandardEncoding and ISOLatin1Encoding.
gdevemap.o: gdevemap.c $(AK) $(std_h)

# Implement dynamic color management for 8-bit mapped color displays.
gdev8bcm.o: gdev8bcm.c $(AK) \
  $(gx_h) $(gxdevice_h) $(gdev8bcm_h)

### -------------------------- The X11 device -------------------------- ###

# Aladdin Enterprises does not support Ghostview.  For more information
# about Ghostview, please contact Tim Theisen (ghostview@cs.wisc.edu).

# See the main makefile for the definition of XLIBS.
x11_=gdevx.o gdevxini.o gdevxxf.o gdevemap.o
x11.dev: $(x11_)
	$(SETDEV) x11 $(x11_)
	$(ADDMOD) x11 -lib $(XLIBS)

# See the main makefile for the definition of XINCLUDE.
GDEVX=$(GDEV) x_.h gdevx.h $(MAKEFILE)
gdevx.o: gdevx.c $(GDEVX) $(gsparam_h)
	$(CCC) $(XINCLUDE) gdevx.c

gdevxini.o: gdevxini.c $(GDEVX) $(ctype__h)
	$(CCC) $(XINCLUDE) gdevxini.c

gdevxxf.o: gdevxxf.c $(GDEVX) $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(CCC) $(XINCLUDE) gdevxxf.c

# Alternate X11-based devices to help debug other drivers.
# x11alpha pretends to have 4 bits of alpha channel.
# x11cmyk pretends to be a CMYK device with 1 bit each of C,M,Y,K.
# x11mono pretends to be a black-and-white device.
x11alt_=$(x11_) gdevxalt.o
x11alpha.dev: $(x11alt_)
	$(SETDEV) x11alpha $(x11alt_)
	$(ADDMOD) x11alpha -lib $(XLIBS)

x11cmyk.dev: $(x11alt_)
	$(SETDEV) x11cmyk $(x11alt_)
	$(ADDMOD) x11cmyk -lib $(XLIBS)

x11mono.dev: $(x11alt_)
	$(SETDEV) x11mono $(x11alt_)
	$(ADDMOD) x11mono -lib $(XLIBS)

gdevxalt.o: gdevxalt.c $(GDEVX) $(PDEVH)
	$(CCC) $(XINCLUDE) gdevxalt.c

### ------------------------- DEC sixel displays ------------------------ ###
### Note: this driver was contributed by a user: please contact           ###
###   Phil Keegstra (keegstra@tonga.gsfc.nasa.gov) if you have questions. ###

# This is a "printer" device, but it probably shouldn't be.
# I don't know why the implementor chose to do it this way.
sxlcrt_=gdevln03.o
sxlcrt.dev: $(sxlcrt_) page.dev
	$(SETPDEV) sxlcrt $(sxlcrt_)

###### --------------- Memory-buffered printer devices --------------- ######

### ------------- The CalComp Raster Format ----------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Ernst Muellner (ernst.muellner@oenzl.siemens.de) if you have    ###
###       questions.                                                      ###

ccr_=gdevccr.o
ccr.dev: $(ccr_) page.dev
	$(SETPDEV) ccr $(ccr_)

gdevccr.o: gdevccr.c $(PDEVH)


# -------------------------------- Library -------------------------------- #

## The Unix platforms

# We have to include a test for the existence of sys/time.h,
# because some System V platforms don't have it.

# Define pipes as a separable feature.

pipe_=gdevpipe.o
pipe.dev: $(UNIXTAIL_MAK) $(ECHOGS_XE) $(pipe_)
	$(SETMOD) pipe $(pipe_)
	$(ADDMOD) pipe -iodev pipe

gdevpipe.o: gdevpipe.c $(AK) $(errno__h) $(stdio__h) $(string__h) \
  $(gserror_h) $(gsmemory_h) $(gstypes_h) $(gxiodev_h) $(stream_h)

# Unix platforms other than System V, and also System V Release 4
# (SVR4) platforms.
unix__=gp_nofb.o gp_unix.o gp_unifs.o gp_unifn.o
unix_.dev: $(unix__)
	$(SETMOD) unix_ $(unix__)

gp_unix.o: gp_unix.c $(AK) $(string__h) $(gx_h) $(gsexit_h) $(gp_h) \
  $(time__h)

# System V platforms other than SVR4, which lack some system calls,
# but have pipes.
sysv__=gp_nofb.o gp_unix.o gp_unifs.o gp_unifn.o gp_sysv.o
sysv_.dev: $(sysv__)
	$(SETMOD) sysv_ $(sysv__)

gp_sysv.o: gp_sysv.c $(time__h) $(AK)

# Interpreter main program

NONDEVS_ALL=gs.o imainarg.o imain.o $(INT_ALL) gconfig.o gscdefs.o

# Here is the final link step.  The stuff with LD_RUN_PATH is for SVR4
# systems with dynamic library loading; I believe it's harmless elsewhere.
# The resetting of the environment variables to empty strings is for SCO Unix,
# which has limited environment space.
gs: ld.tr echogs $(NONDEVS_ALL) $(DEVS_ALL)
	./echogs -w ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GS_XE)
	./echogs -a ldt.tr -n -s gs.o -s
	cat ld.tr >>ldt.tr
	./echogs -a ldt.tr -s - $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= \
	$(SH) <ldt.tr

# The rule for gconfigv.h is here because it is shared between Unix and
# DV/X environments.
gconfigv.h: unix-end.mak $(MAKEFILE) $(ECHOGS_XE)
	./echogs -w gconfigv.h -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	./echogs -a gconfigv.h -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	./echogs -a gconfigv.h -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)

#
# End of "$Id: Makefile,v 1.1 1999/04/21 19:32:38 mike Exp $".
#
