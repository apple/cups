#
# "$Id: Makefile,v 1.9 1999/05/26 20:03:26 mike Exp $"
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
# Object files...
#

OBJS	=	gconf.o gdevabuf.o gdevcups.o gdevddrw.o gdevdflt.o \
		gdevemap.o gdevm1.o gdevm16.o gdevm2.o gdevm24.o \
		gdevm32.o gdevm4.o gdevm8.o gdevmem.o gdevmpla.o \
		gdevmrop.o gdevnfwd.o gdevpipe.o gdevprn.o gp_nofb.o \
		gp_unifn.o gp_unifs.o gp_unix.o gsalloc.o gsbitops.o \
		gsbittab.o gscdef.o gschar.o gschar0.o gscie.o \
		gscolor.o gscolor1.o gscolor2.o gscoord.o gscsepr.o \
		gsdevice.o gsdevmem.o gsdparam.o gsdps1.o gsfont.o \
		gsfont0.o gshsb.o gsht.o gsht1.o gshtscr.o gsimage.o \
		gsimpath.o gsinit.o gsiodev.o gsline.o gsmatrix.o \
		gsmemory.o gsmisc.o gspaint.o gsparam.o gspath.o \
		gspath1.o gspcolor.o gsrop.o gsroptab.o gsstate.o \
		gstype1.o gstype42.o gsutil.o gxacpath.o gxbcache.o \
		gxccache.o gxccman.o gxcht.o gxclbits.o gxclfile.o \
		gxclimag.o gxclip2.o gxclist.o gxclpath.o gxclread.o \
		gxcmap.o gxcpath.o gxctable.o gxdcconv.o gxdcolor.o \
		gxdither.o gxfill.o gxhint1.o gxhint2.o gxhint3.o \
		gxht.o gximage.o gximage0.o gximage1.o gximage2.o \
		gximage3.o gximage4.o gximage5.o gxpaint.o gxpath.o \
		gxpath2.o gxpcmap.o gxpcopy.o gxpdash.o gxstroke.o \
		ialloc.o ibnum.o iccinit0.o iconf.o idebug.o idict.o \
		idparam.o igc.o igcref.o igcstr.o iinit.o ilocate.o \
		imain.o iname.o interp.o iparam.o ireclaim.o isave.o \
		iscan.o iscanbin.o iscannum.o iscantab.o istack.o \
		iutil.o iutil2.o pstoraster.o sbcp.o sbhc.o sbwbs.o \
		scfd.o scfdtab.o scfe.o scfetab.o sdctc.o sdctd.o \
		sdcte.o seexec.o sfile.o sfilter1.o sfilter2.o shc.o \
		shcgen.o siscale.o sjpegc.o sjpegd.o sjpege.o \
		sjpegerr.o slzwc.o slzwce.o slzwd.o smtf.o spcxd.o \
		spdiff.o spngp.o srld.o srle.o sstring.o stream.o \
		szlibc.o szlibd.o szlibe.o zarith.o zarray.o zbseq.o \
		zchar.o zchar1.o zchar2.o zchar42.o zcharout.o zcie.o \
		zcolor.o zcolor1.o zcolor2.o zcontrol.o zcrd.o \
		zcsindex.o zcssepr.o zdevcal.o zdevice.o zdevice2.o \
		zdict.o zdps1.o zfbcp.o zfdctc.o zfdctd.o zfdcte.o \
		zfdecode.o zfile.o zfileio.o zfilter.o zfilter2.o \
		zfilterx.o zfname.o zfont.o zfont0.o zfont1.o zfont2.o \
		zfont42.o zfproc.o zfzlib.o zgeneric.o zgstate.o zhsb.o \
		zht.o zht1.o zht2.o zimage2.o ziodev.o ziodev2.o \
		zmath.o zmatrix.o zmedia2.o zmisc.o zmisc1.o zmisc2.o \
		zpacked.o zpaint.o zpath.o zpath1.o zpcolor.o zrelbit.o \
		zstack.o zstring.o zsysvm.o ztoken.o ztype.o zupath.o \
		zusparam.o zvmem.o zvmem2.o

#
# Data files...
#

DFILES	=	Fontmap gs_btokn.ps gs_ccfnt.ps gs_cidfn.ps gs_cmap.ps \
		gs_cmdl.ps gs_dbt_e.ps gs_diskf.ps gs_dps1.ps \
		gs_fform.ps gs_fonts.ps gs_init.ps gs_iso_e.ps \
		gs_kanji.ps gs_ksb_e.ps gs_l2img.ps gs_lev2.ps \
		gs_mex_e.ps gs_mro_e.ps gs_pdfwr.ps gs_pdf.ps \
		gs_pdf_e.ps gs_pfile.ps gs_res.ps gs_setpd.ps \
		gs_statd.ps gs_std_e.ps gs_sym_e.ps gs_ttf.ps \
		gs_typ42.ps gs_type1.ps gs_wan_e.ps gs_wl1_e.ps \
		gs_wl2_e.ps gs_wl5_e.ps pdf_2ps.ps pdf_base.ps \
		pdf_draw.ps pdf_font.ps pdf_main.ps pdf_sec.ps \
		pfbtogs.ps

#
# Targets...
#

TARGETS	=	genarch arch.h pstoraster

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
	-$(MKDIR) $(SERVERROOT)/filter
	$(CP) pstoraster $(SERVERROOT)/filter
	-$(MKDIR) $(DATADIR)/pstoraster
	$(CP) $(DFILES) $(DATADIR)/pstoraster

#
# genarch - generate the architecture configuration file.
#

genarch:	genarch.o
	$(CC) $(LDFLAGS) -o genarch genarch.o
arch.h:		genarch
	./genarch arch.h

#
# pstoraster - PostScript conversion filter.
#

pstoraster:	$(OBJS) ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o pstoraster $(OBJS) $(IMGLIBS) 

gdevcups.o:	../cups/raster.h
$(OBJS):	arch.h ../config.h ../Makedefs
gconf.o iconf.o gscdef.o:	gconfig.h

#
# End of "$Id: Makefile,v 1.9 1999/05/26 20:03:26 mike Exp $".
#
