#
# "$Id: Makefile,v 1.29 2000/07/17 12:31:58 mike Exp $"
#
#   GNU Ghostscript makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2000 by Easy Software Products.
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
# Object files...
#

LIBOBJS	=	gconfig.o \
		gdevabuf.o \
		gdevbbox.o \
		gdevcups.o \
		gdevdbit.o \
		gdevddrw.o \
		gdevdflt.o \
		gdevdgbr.o \
		gdevhit.o \
		gdevm16.o \
		gdevm1.o \
		gdevm24.o \
		gdevm2.o \
		gdevm32.o \
		gdevm4.o \
		gdevm8.o \
		gdevmem.o \
		gdevmpla.o \
		gdevnfwd.o \
		gdevpipe.o \
		gdevprn.o \
		gp_getnv.o \
		gp_nofb.o \
		gp_nsync.o \
		gp_unifn.o \
		gp_unifs.o \
		gp_unix.o \
		gsalloc.o \
		gsalpha.o \
		gsargs.o \
		gsbitops.o \
		gsbittab.o \
		gscdefs.o \
		gscdevn.o \
		gschar0.o \
		gschar.o \
		gscie.o \
		gsclipsr.o \
		gscolor1.o \
		gscolor2.o \
		gscolor3.o \
		gscolor.o \
		gscoord.o \
		gscparam.o \
		gscpixel.o \
		gscrdp.o \
		gscrd.o \
		gscscie.o \
		gscsepr.o \
		gscspace.o \
		gsdevice.o \
		gsdevmem.o \
		gsdparam.o \
		gsdps1.o \
		gsdsrc.o \
		gsfcmap.o \
		gsfont0.o \
		gsfont.o \
		gsfunc0.o \
		gsfunc3.o \
		gsfunc.o \
		gshsb.o \
		gsht1.o \
		gshtscr.o \
		gsht.o \
		gsimage.o \
		gsimpath.o \
		gsinit.o \
		gsiodev.o \
		gsline.o \
		gsmalloc.o \
		gsmatrix.o \
		gsmemory.o \
		gsmisc.o \
		gsnorop.o \
		gspaint.o \
		gsparams.o \
		gsparam.o \
		gspath1.o \
		gspath.o \
		gspcolor.o \
		gsshade.o \
		gsstate.o \
		gstext.o \
		gstrap.o \
		gstype1.o \
		gstype2.o \
		gstype42.o \
		gsutil.o \
		gxacpath.o \
		gxbcache.o \
		gxccache.o \
		gxccman.o \
		gxcht.o \
		gxclbits.o \
		gxclimag.o \
		gxclip2.o \
		gxclipm.o \
		gxclip.o \
		gxclist.o \
		gxclmem.o \
		gxclpage.o \
		gxclpath.o \
		gxclrast.o \
		gxclread.o \
		gxclrect.o \
		gxclutil.o \
		gxclzlib.o \
		gxcmap.o \
		gxcpath.o \
		gxctable.o \
		gxdcconv.o \
		gxdcolor.o \
		gxdither.o \
		gxfill.o \
		gxhint1.o \
		gxhint2.o \
		gxhint3.o \
		gxht.o \
		gxi12bit.o \
		gxicolor.o \
		gxidata.o \
		gxifast.o \
		gxiinit.o \
		gximage3.o \
		gximage4.o \
		gximono.o \
		gxiscale.o \
		gxmclip.o \
		gxp1fill.o \
		gxpaint.o \
		gxpath2.o \
		gxpath.o \
		gxpcmap.o \
		gxpcopy.o \
		gxpdash.o \
		gxpflat.o \
		gxsample.o \
		gxshade1.o \
		gxshade4.o \
		gxshade6.o \
		gxshade.o \
		gxstroke.o \
		gxtype1.o \
		ialloc.o \
		ibnum.o \
		iccinit0.o \
		iconfig.o \
		icontext.o \
		idebug.o \
		idict.o \
		idparam.o \
		idstack.o \
		igcref.o \
		igcstr.o \
		igc.o \
		iinit.o \
		ilocate.o \
		imainarg.o \
		imain.o \
		iname.o \
		interp.o \
		iparam.o \
		ireclaim.o \
		isave.o \
		iscanbin.o \
		iscannum.o \
		iscan.o \
		istack.o \
		iutil2.o \
		iutil.o \
		sbcp.o \
		sbhc.o \
		sbwbs.o \
		scantab.o \
		scfdtab.o \
		scfd.o \
		scfetab.o \
		scfe.o \
		scfparam.o \
		sdcparam.o \
		sdctc.o \
		sdctd.o \
		sdcte.o \
		sddparam.o \
		sdeparam.o \
		seexec.o \
		sfilter1.o \
		sfilter2.o \
		sfxstdio.o \
		shcgen.o \
		shc.o \
		siscale.o \
		sjpegc.o \
		sjpegd.o \
		sjpegerr.o \
		sjpege.o \
		slzwce.o \
		slzwc.o \
		slzwd.o \
		smtf.o \
		spcxd.o \
		spdiff.o \
		spngp.o \
		srld.o \
		srle.o \
		sstring.o \
		stream.o \
		szlibc.o \
		szlibd.o \
		szlibe.o \
		zarith.o \
		zarray.o \
		zbseq.o \
		zcfont.o \
		zchar1.o \
		zchar2.o \
		zchar32.o \
		zchar42.o \
		zcharout.o \
		zchar.o \
		zcid.o \
		zcie.o \
		zcolor1.o \
		zcolor2.o \
		zcolor.o \
		zcontrol.o \
		zcrd.o \
		zcsdevn.o \
		zcsindex.o \
		zcspixel.o \
		zcssepr.o \
		zdevcal.o \
		zdevice2.o \
		zdevice.o \
		zdict.o \
		zdps1.o \
		zfbcp.o \
		zfcmap.o \
		zfdctd.o \
		zfdcte.o \
		zfdecode.o \
		zfileio.o \
		zfile.o \
		zfilter2.o \
		zfilterx.o \
		zfilter.o \
		zfname.o \
		zfont0.o \
		zfont1.o \
		zfont2.o \
		zfont32.o \
		zfont42.o \
		zfont.o \
		zfproc.o \
		zfreuse.o \
		zfunc0.o \
		zfunc3.o \
		zfunc.o \
		zfzlib.o \
		zgeneric.o \
		zgstate.o \
		zhsb.o \
		zht1.o \
		zht2.o \
		zht.o \
		zimage2.o \
		zimage3.o \
		zimage.o \
		ziodev2.o \
		ziodev.o \
		zmath.o \
		zmatrix.o \
		zmedia2.o \
		zmisc1.o \
		zmisc2.o \
		zmisc3.o \
		zmisc.o \
		zpacked.o \
		zpaint.o \
		zpath1.o \
		zpath.o \
		zpcolor.o \
		zrelbit.o \
		zshade.o \
		zstack.o \
		zstring.o \
		zsysvm.o \
		ztoken.o \
		ztrap.o \
		ztype.o \
		zupath.o \
		zusparam.o \
		zvmem2.o \
		zvmem.o

OBJS	=	$(LIBOBJS) genarch.o pstoraster.o

#
# Data files...
#

DFILES	=	Fontmap \
		gs_btokn.ps gs_ccfnt.ps gs_cff.ps gs_cidfn.ps \
		gs_cmap.ps gs_cmdl.ps gs_dbt_e.ps gs_diskf.ps \
		gs_dpnxt.ps gs_dps1.ps gs_dps2.ps gs_dps.ps gs_epsf.ps \
		gs_fform.ps gs_fonts.ps gs_init.ps gs_iso_e.ps \
		gs_kanji.ps gs_ksb_e.ps gs_lev2.ps gs_ll3.ps \
		gs_mex_e.ps gs_mro_e.ps gs_pfile.ps gs_res.ps \
		gs_setpd.ps gs_statd.ps gs_std_e.ps gs_sym_e.ps \
		gs_ttf.ps gs_typ32.ps gs_typ42.ps gs_type1.ps \
		gs_wan_e.ps gs_wl1_e.ps gs_wl2_e.ps gs_wl5_e.ps


#
# Targets...
#

TARGETS	=	genarch arch.h libgs.a pstoraster


#
# Make everything...
#

all:		$(TARGETS)

#
# Clean all config and object files...
#

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS)


#
# Install files...
#

install:	$(TARGETS)
	-$(MKDIR) $(SERVERBIN)/filter
	$(CHMOD) ugo+rx $(SERVERBIN)
	$(CHMOD) ugo+rx $(SERVERBIN)/filter
	$(INSTALL_BIN) pstoraster $(SERVERBIN)/filter
	-$(MKDIR) $(DATADIR)/pstoraster
	$(CHMOD) ugo+rx $(DATADIR)
	$(CHMOD) ugo+rx $(DATADIR)/pstoraster
	$(INSTALL_DATA) $(DFILES) $(DATADIR)/pstoraster


#
# genarch - generate the architecture configuration file.
#

genarch:	genarch.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o genarch genarch.o
arch.h:		genarch
	echo Generating $@...
	./genarch arch.h


#
# libgs.a - GhostScript interpreter library...
#

libgs.a:	$(LIBOBJS) ../Makedefs
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

gdevcups.o:	../filter/raster.h
$(LIBOBJS):	arch.h ../config.h ../Makedefs


#
# pstoraster - PostScript RIP filter.
#

pstoraster:	pstoraster.o libgs.a ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o pstoraster pstoraster.o libgs.a \
		$(LINKCUPSIMAGE) $(IMGLIBS) $(DSOLIBS) $(LIBS) -lm
pstoraster.o:	arch.h ../config.h ../Makedefs

#
# End of "$Id: Makefile,v 1.29 2000/07/17 12:31:58 mike Exp $".
#
