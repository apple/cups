/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsroptab.c */
/* Table of RasterOp procedures */
#include "stdpre.h"
#include "gsropt.h"

/*
 * The H-P documentation (probably copied from Microsoft documentation)
 * specifies RasterOp algorithms using reverse Polish notation, with
 *   a = AND, o = OR, n = NOT, x = XOR
 * We can use preprocessor macros to transcribe these algorithms almost
 * literally into C code.
 */

#ifdef __PROTOTYPES__
#  define rop_proc(pname)\
private rop_operand pname(rop_operand D, rop_operand S, rop_operand T)
#else
#  define rop_proc(pname)\
private rop_operand pname(D,S,T) rop_operand D; rop_operand S; rop_operand T;
#endif
#define r return

#define a(u,v) (u&v)
#define o(u,v) (u|v)
#define x(u,v) (u^v)

rop_proc(rop0) { r 0; }	/* 0 */
rop_proc(rop1) { r ~(D | (S | T)); }	/* DTSoon */
rop_proc(rop2) { r D & (~(S | T)); }	/* DTSona */
rop_proc(rop3) { r ~(S | T); }	/* TSon */
rop_proc(rop4) { r S & (~(D | T)); }	/* SDTona */
rop_proc(rop5) { r ~(D | T); }	/* DTon */
rop_proc(rop6) { r ~(T | (~(D ^ S))); }	/* TDSxnon */
rop_proc(rop7) { r ~(T | (D & S)); }	/* TDSaon */
rop_proc(rop8) { r S & (D & ~T); }	/* SDTnaa */
rop_proc(rop9) { r ~(T | (D ^ S)); }	/* TDSxon */
rop_proc(rop10) { r D & ~T; }		/* DTna */
rop_proc(rop11) { r ~(T | (S & ~D)); }	/* TSDnaon */
rop_proc(rop12) { r S & ~T; }		/* STna */
rop_proc(rop13) { r ~(T | (D & ~S)); }	/* TDSnaon */
rop_proc(rop14) { r ~(T | (~(D | S))); }	/* TDSonon */
rop_proc(rop15) { r ~T; }		/* Tn */
rop_proc(rop16) { r T & (~(D | S)); }	/* TDSona */
rop_proc(rop17) { r ~(D | S); }	/* DSon */
rop_proc(rop18) { r ~(S | (~(D ^ T))); }	/* SDTxnon */
rop_proc(rop19) { r ~(S | (D & T)); }	/* SDTaon */
rop_proc(rop20) { r ~(D | ~(T ^ S)); }	/* DTSxnon */
rop_proc(rop21) { r ~(D | (T & S)); }		/* DTSaon */
rop_proc(rop22) { r (T ^ (S ^ (D & ~(T & S)))); }	/* TSDTSanaxx */
rop_proc(rop23) { r ~(S ^ ((S ^ T) & (D ^ S))); }	/* SSTxDSxaxn */
rop_proc(rop24) { r (S ^ T) & (T ^ D); }	/* STxTDxa */
rop_proc(rop25) { r ~(S ^ (D & ~(T & S))); }	/* SDTSanaxn */
rop_proc(rop26) { r T ^ (D | (S & T)); }	/* TDSTaox */
rop_proc(rop27) { r ~(S ^ (D & (T ^ S))); }	/* SDTSxaxn */
rop_proc(rop28) { r T ^ (S | (D & T)); }	/* TSDTaox */
rop_proc(rop29) { r ~(D ^ (S & (T ^ D))); }	/* DSTDxaxn */
rop_proc(rop30) { r T ^ (D | S); }	/* TDSox */
rop_proc(rop31) { r ~(T & (D | S)); }	/* TDSoan */
rop_proc(rop32) { r D & (T & ~S); }	/* DTSnaa */
rop_proc(rop33) { r ~(S | (D ^ T)); }	/* SDTxon */
rop_proc(rop34) { r D & ~S; }		/* DSna */
rop_proc(rop35) { r ~(S | (T & ~D)); }	/* STDnaon */
rop_proc(rop36) { r (S ^ T) & (D ^ S); }	/* STxDSxa */
rop_proc(rop37) { r ~(T ^ (D & ~(S & T))); }	/* TDSTanaxn */
rop_proc(rop38) { r S ^ (D | (T & S)); }	/* SDTSaox */
rop_proc(rop39) { r S ^ (D | ~(T ^ S)); }	/* SDTSxnox */
rop_proc(rop40) { r D & (T ^ S); }	/* DTSxa */
rop_proc(rop41) { r ~(T ^ (S ^ (D | (T & S)))); }	/* TSDTSaoxxn */
rop_proc(rop42) { r D & ~(T & S); }	/* DTSana */
rop_proc(rop43) { r ~x(a(x(D,T),x(T,S)),S) ; }	/* SSTxTDxaxn */
rop_proc(rop44) { r (S ^ (T & (D | S))); }	/* STDSoax */
rop_proc(rop45) { r T ^ (S | ~D); }		/* TSDnox */
rop_proc(rop46) { r (T ^ (S | (D ^ T))); }	/* TSDTxox */
rop_proc(rop47) { r ~(T & (S | ~D)); }	/* TSDnoan */
rop_proc(rop48) { r T & ~S; }			/* TSna */
rop_proc(rop49) { r ~(S | (D & ~T)); }	/* SDTnaon */
rop_proc(rop50) { r S ^ (D | (T | S)); }	/* SDTSoox */
rop_proc(rop51) { r ~S; }			/* Sn */
rop_proc(rop52) { r S ^ (T |(D & S)); }	/* STDSaox */
rop_proc(rop53) { r S ^ (T | ~(D ^ S)); }	/* STDSxnox */
rop_proc(rop54) { r S ^ (D | T); }		/* SDTox */
rop_proc(rop55) { r ~(S & (D | T)); }		/* SDToan */
rop_proc(rop56) { r T ^ (S & (D | T)); }	/* TSDToax */
rop_proc(rop57) { r S ^ (T | ~D); }		/* STDnox */
rop_proc(rop58) { r S ^ (T | (D ^ S)); }	/* STDSxox */
rop_proc(rop59) { r ~(S & (T | ~D)); }	/* STDnoan */
rop_proc(rop60) { r T ^ S; }			/* TSx */
rop_proc(rop61) { r S ^ (T | ~(D | S)); }	/* STDSonox */
rop_proc(rop62) { r S ^ (T | (D & ~S)); }	/* STDSnaox */
rop_proc(rop63) { r ~(T & S); }	/* TSan */
rop_proc(rop64) { r T & (S & ~D); }	/* TSDnaa */
rop_proc(rop65) { r ~(D | (T ^ S)); }	/* DTSxon */
rop_proc(rop66) { r (S ^ D) & (T ^ D); }	/* SDxTDxa */
rop_proc(rop67) { r ~(S ^ (T & ~(D & S))); }	/* STDSanaxn */
rop_proc(rop68) { r S & ~D; }	/* SDna */
rop_proc(rop69) { r ~(D | (T & ~S)); }	/* DTSnaon */
rop_proc(rop70) { r D ^ (S | (T & D)); }	/* DSTDaox */
rop_proc(rop71) { r ~(T ^ (S & (D ^ T))); }	/* TSDTxaxn */
rop_proc(rop72) { r S & (D ^ T); }	/* SDTxa */
rop_proc(rop73) { r ~(T ^ (D ^ (S | (T & D)))); }	/* TDSTDaoxxn */
rop_proc(rop74) { r D ^ (T & (S | D)); }	/* DTSDoax */
rop_proc(rop75) { r T ^ (D | ~S); }	/* TDSnox */
rop_proc(rop76) { r S & ~(D & T); }	/* SDTana */
rop_proc(rop77) { r ~(S ^ ((S ^ T) | (D ^ S))); }	/* SSTxDSxoxn */
rop_proc(rop78) { r T ^ (D | (S ^ T)); }	/* TDSTxox */
rop_proc(rop79) { r ~(T & (D | ~S)); }	/* TDSnoan */
rop_proc(rop80) { r T & ~D; }	/* TDna */
rop_proc(rop81) { r ~(D | (S & ~T)); }	/* DSTnaon */
rop_proc(rop82) { r D ^ (T | (S & D)); }	/* DTSDaox */
rop_proc(rop83) { r ~(S ^ (T & (D ^ S))); }	/* STDSxaxn */
rop_proc(rop84) { r ~(D | ~(T | S)); }	/* DTSonon */
rop_proc(rop85) { r ~D; }	/* Dn */
rop_proc(rop86) { r D ^ (T | S); }	/* DTSox */
rop_proc(rop87) { r ~(D & (T | S)); }	/* DTSoan */
rop_proc(rop88) { r T ^ (D & (S | T)); }	/* TDSToax */
rop_proc(rop89) { r D ^ (T | ~S); }	/* DTSnox */
rop_proc(rop90) { r D ^ T; }	/* DTx */
rop_proc(rop91) { r D ^ (T | ~(S | D)); }	/* DTSDonox */
rop_proc(rop92) { r D ^ (T | (S ^ D)); }	/* DTSDxox */
rop_proc(rop93) { r ~(D & (T | ~S)); }	/* DTSnoan */
rop_proc(rop94) { r D ^ (T | (S & ~D)); }	/* DTSDnaox */
rop_proc(rop95) { r ~(D & T); }	/* DTan */
rop_proc(rop96) { r T & (D ^ S); }	/* TDSxa */
rop_proc(rop97) { r ~(D ^ (S ^ (T | (D & S)))); }	/* DSTDSaoxxn */
rop_proc(rop98) { r D ^ (S & (T | D)); }	/* DSTDoax */
rop_proc(rop99) { r S ^ (D | ~T); }	/* SDTnox */
rop_proc(rop100) { r S ^ (D & (T | S)); }	/* SDTSoax */
rop_proc(rop101) { r D ^ (S | ~T); }	/* DSTnox */
rop_proc(rop102) { r D ^ S; }	/* DSx */
rop_proc(rop103) { r S ^ (D | ~(T | S)); }	/* SDTSonox */
rop_proc(rop104) { r ~(D ^ (S ^ (T | ~(D | S)))); }	/* DSTDSonoxxn */
rop_proc(rop105) { r ~(T ^ (D ^ S)); }	/* TDSxxn */
rop_proc(rop106) { r D ^ (T & S); }	/* DTSax */
rop_proc(rop107) { r ~(T ^ (S ^ (D & (T | S)))); }	/* TSDTSoaxxn */
rop_proc(rop108) { r (D & T) ^ S; }	/* SDTax */
rop_proc(rop109) { r ~((((T | D) & S) ^ D) ^ T); }	/* TDSTDoaxxn */
rop_proc(rop110) { r ((~S | T) & D) ^ S; }	/* SDTSnoax */
rop_proc(rop111) { r ~(~(D ^ S) & T); }	/* TDSxnan */
rop_proc(rop112) { r ~(D & S) & T; }	/* TDSana */
rop_proc(rop113) { r ~(((S ^ D) & (T ^ D)) ^ S); }	/* SSDxTDxaxn */
rop_proc(rop114) { r ((T ^ S) | D) ^ S; }	/* SDTSxox */
rop_proc(rop115) { r ~((~T | D) & S); }	/* SDTnoan */
rop_proc(rop116) { r ((T ^ D) | S) ^ D; }	/* DSTDxox */
rop_proc(rop117) { r ~((~T | S) & D); }	/* DSTnoan */
rop_proc(rop118) { r ((~S & T) | D) ^ S; }	/* SDTSnaox */
rop_proc(rop119) { r ~(D & S); }	/* DSan */
rop_proc(rop120) { r (D & S) ^ T; }	/* TDSax */
rop_proc(rop121) { r ~((((D | S) & T) ^ S) ^ D); }	/* DSTDSoaxxn */
rop_proc(rop122) { r ((~D | S) & T) ^ D; }	/* DTSDnoax */
rop_proc(rop123) { r ~(~(D ^ T) & S); }	/* SDTxnan */
rop_proc(rop124) { r ((~S | D) & T) ^ S; }	/* STDSnoax */
rop_proc(rop125) { r ~(~(T ^ S) & D); }	/* DTSxnan */
rop_proc(rop126) { r (S ^ T) | (D ^ S); }	/* STxDSxo */
rop_proc(rop127) { r ~((T & S) & D); }	/* DTSaan */
rop_proc(rop128) { r (T & S) & D; }	/* DTSaa */
rop_proc(rop129) { r ~((S ^ T) | (D ^ S)); }	/* STxDSxon */
rop_proc(rop130) { r ~(T ^ S) & D; }	/* DTSxna */
rop_proc(rop131) { r ~(((~S | D) & T) ^ S); }	/* STDSnoaxn */
rop_proc(rop132) { r ~(D ^ T) & S; }	/* SDTxna */
rop_proc(rop133) { r ~(((~T | S) & D) ^ T); }	/* TDSTnoaxn */
rop_proc(rop134) { r (((D | S) & T) ^ S) ^ D; }	/* DSTDSoaxx */
rop_proc(rop135) { r ~((D & S) ^ T); }	/* TDSaxn */
rop_proc(rop136) { r D & S; }	/* DSa */
rop_proc(rop137) { r ~(((~S & T) | D) ^ S); }	/* SDTSnaoxn */
rop_proc(rop138) { r (~T | S) & D; }	/* DSTnoa */
rop_proc(rop139) { r ~(((T ^ D) | S) ^ D); }	/* DSTDxoxn */
rop_proc(rop140) { r (~T | D) & S; }	/* SDTnoa */
rop_proc(rop141) { r ~(((T ^ S) | D) ^ S); }	/* SDTSxoxn */
rop_proc(rop142) { r ((S ^ D) & (T ^ D)) ^ S; }	/* SSDxTDxax */
rop_proc(rop143) { r ~(~(D & S) & T); }	/* TDSanan */
rop_proc(rop144) { r ~(D ^ S) & T; }	/* TDSxna */
rop_proc(rop145) { r ~(((~S | T) & D) ^ S); }	/* SDTSnoaxn */
rop_proc(rop146) { r (((D | T) & S) ^ T) ^ D; }	/* DTSDToaxx */
rop_proc(rop147) { r ~((T & D) ^ S); }	/* STDaxn */
rop_proc(rop148) { r (((T | S) & D) ^ S) ^ T; }	/* TSDTSoaxx */
rop_proc(rop149) { r ~((T & S) ^ D); }	/* DTSaxn */
rop_proc(rop150) { r (T ^ S) ^ D; }	/* DTSxx */
rop_proc(rop151) { r ((~(T | S) | D) ^ S) ^ T; }	/* TSDTSonoxx */
rop_proc(rop152) { r ~((~(T | S) | D) ^ S); }	/* SDTSonoxn */
rop_proc(rop153) { r ~(D ^ S); }	/* DSxn */
rop_proc(rop154) { r (~S & T) ^ D; }	/* DTSnax */
rop_proc(rop155) { r ~(((T | S) & D) ^ S); }	/* SDTSoaxn */
rop_proc(rop156) { r (~D & T) ^ S; }	/* STDnax */
rop_proc(rop157) { r ~(((T | D) & S) ^ D); }	/* DSTDoaxn */
rop_proc(rop158) { r (((D & S) | T) ^ S) ^ D; }	/* DSTDSaoxx */
rop_proc(rop159) { r ~((D ^ S) & T); }	/* TDSxan */
rop_proc(rop160) { r D & T; }	/* DTa */
rop_proc(rop161) { r ~(((~T & S) | D) ^ T); }	/* TDSTnaoxn */
rop_proc(rop162) { r (~S | T) & D; }				/* DTSnoa */
rop_proc(rop163) { r ~(((D ^ S) | T) ^ D); }			/* DTSDxoxn */
rop_proc(rop164) { r ~((~(T | S) | D) ^ T) ; }		/* TDSTonoxn */
rop_proc(rop165) { r ~(D ^ T); }			/* TDxn */
rop_proc(rop166) { r (~T & S) ^ D; }				/* DSTnax */
rop_proc(rop167) { r ~(((T | S) & D) ^ T); }			/* TDSToaxn */
rop_proc(rop168) { r ((S | T) & D); }				/* DTSoa */
rop_proc(rop169) { r ~((S | T) ^ D); }			/* DTSoxn */
rop_proc(rop170) { r D; }				/* D */
rop_proc(rop171) { r ~(S | T) | D; }				/* DTSono */
rop_proc(rop172) { r (((S ^ D) & T) ^ S); }			/* STDSxax */
rop_proc(rop173) { r ~(((D & S) | T) ^ D); }			/* DTSDaoxn */
rop_proc(rop174) { r (~T & S) | D; }			/* DSTnao */
rop_proc(rop175) { r ~T | D; }			/* DTno */
rop_proc(rop176) { r (~S | D) & T; }			/* TDSnoa */
rop_proc(rop177) { r ~(((T ^ S) | D) ^ T) ; }			/* TDSTxoxn */
rop_proc(rop178) { r ((S ^ D) | (S ^ T)) ^ S; }		/* SSTxDSxox */
rop_proc(rop179) { r ~(~(T & D) & S); }			/* SDTanan */
rop_proc(rop180) { r (~D & S) ^ T; }				/* TSDnax */
rop_proc(rop181) { r ~(((D | S) & T) ^ D); }			/* DTSDoaxn */
rop_proc(rop182) { r (((T & D) | S) ^ T) ^ D; }		/* DTSDTaoxx */
rop_proc(rop183) { r ~((T ^ D) & S); }			/* SDTxan */
rop_proc(rop184) { r ((T ^ D) & S) ^ T; }		/* TSDTxax */
rop_proc(rop185) { r (~((D & T) | S) ^ D); }		/* DSTDaoxn */
rop_proc(rop186) { r (~S & T) | D; }			/* DTSnao */
rop_proc(rop187) { r ~S | D; }			/* DSno */
rop_proc(rop188) { r (~(S & D) & T) ^ S; }		/* STDSanax */
rop_proc(rop189) { r ~((D ^ T) & (D ^ S)); }		/* SDxTDxan */
rop_proc(rop190) { r (S ^ T) | D; }			/* DTSxo */
rop_proc(rop191) { r ~(S & T) | D; }			/* DTSano */
rop_proc(rop192) { r T & S; }				/* TSa */
rop_proc(rop193) { r ~(((~S & D) | T)^ S); }		/* STDSnaoxn */
rop_proc(rop194) { r ~x(o(~o(S,D),T),S) ; }		/* STDSonoxn */
rop_proc(rop195) { r ~(S ^ T); }			/* TSxn */
rop_proc(rop196) { r ((~D | T) & S); }			/* STDnoa */
rop_proc(rop197) { r ~(((S ^ D) | T) ^ S); }		/* STDSxoxn */
rop_proc(rop198) { r ((~T & D) ^ S); }		/* SDTnax */
rop_proc(rop199) { r ~(((T | D) & S) ^ T); }		/* TSDToaxn */
rop_proc(rop200) { r ((T | D) & S); }		/* SDToa */
rop_proc(rop201) { r ~((D | T) ^ S); }	/* STDoxn */
rop_proc(rop202) { r ((D ^ S) & T) ^ D; }		/* DTSDxax */
rop_proc(rop203) { r ~(((S & D) | T) ^ S); }		/* STDSaoxn */
rop_proc(rop204) { r S; }		/* S */
rop_proc(rop205) { r ~(T | D) | S; }		/* SDTono */
rop_proc(rop206) { r (~T & D) | S; }		/* SDTnao */
rop_proc(rop207) { r ~T | S; }		/* STno */
rop_proc(rop208) { r (~D | S) & T; }		/* TSDnoa */
rop_proc(rop209) { r ~(((T ^ D) | S) ^ T); }		/* TSDTxoxn */
rop_proc(rop210) { r (~S & D) ^ T; }		/* TDSnax */
rop_proc(rop211) { r ~(((S | D) & T) ^ S); }		/* STDSoaxn */
rop_proc(rop212) { r x(a(x(D,T),x(T,S)),S) ; }	/* SSTxTDxax */
rop_proc(rop213) { r ~(~(S & T) & D); }	/* DTSanan */
rop_proc(rop214) { r ((((S & T) | D) ^ S) ^ T); }	/* TSDTS aoxx */
rop_proc(rop215) { r ~((S ^ T) & D); }	/* DTS xan */ 
rop_proc(rop216) { r ((T ^ S) & D) ^ T; }	/* TDST xax */
rop_proc(rop217) { r ~(((S & T) | D) ^ S); }		/* SDTS aoxn */
rop_proc(rop218) { r x(a(~a(D,S),T),D) ; }	/* DTSD anax */
rop_proc(rop219) { r ~a(x(S,D),x(T,S)) ; }		/* STxDSxan */
rop_proc(rop220) { r (~D & T) | S; }	/* STD nao */
rop_proc(rop221) { r ~D | S; }	/* SDno */
rop_proc(rop222) { r (T ^ D) | S; }	/* SDT xo */
rop_proc(rop223) { r (~(T & D)) | S; }	/* SDT ano */
rop_proc(rop224) { r ((S | D) & T); }		/* TDS oa */
rop_proc(rop225) { r ~((S | D) ^ T); }	/*  TDS oxn */
rop_proc(rop226) { r (((D ^ T) & S) ^ D); }	/* DSTD xax */
rop_proc(rop227) { r ~(((T & D) | S) ^ T); }	/* TSDT aoxn */
rop_proc(rop228) { r ((S ^ T) & D) ^ S; }	/* SDTSxax */
rop_proc(rop229) { r ~(((T & S) | D) ^ T); }		/* TDST aoxn */
rop_proc(rop230) { r (~(S & T) & D) ^ S; }		/* SDTSanax */
rop_proc(rop231) { r ~a(x(D,T),x(T,S)) ; }	/* STxTDxan */
rop_proc(rop232) { r x(a(x(S,D),x(T,S)),S) ; }	/* SS TxD Sxax */
rop_proc(rop233) { r ~x(x(a(~a(S,D),T),S),D) ; }	/* DST DSan axxn   */
rop_proc(rop234) { r (S & T) | D; }		/* DTSao */
rop_proc(rop235) { r ~(S ^ T) | D; }		/* DTSxno */
rop_proc(rop236) { r (T & D) | S; }		/* SDTao */
rop_proc(rop237) { r ~(T ^ D) | S; }		/* SDTxno */
rop_proc(rop238) { r S | D; }		/* DSo */
rop_proc(rop239) { r (~T | D) | S; }		/* SDTnoo */
rop_proc(rop240) { r T; }		/* T */
rop_proc(rop241) { r ~(S | D) | T; }		/* TDSono */
rop_proc(rop242) { r (~S & D) | T; }		/* TDSnao */
rop_proc(rop243) { r ~S | T; }		/* TSno */
rop_proc(rop244) { r (~D & S) | T; }		/* TSDnao */
rop_proc(rop245) { r ~D | T; }		/* TDno */
rop_proc(rop246) { r (S ^ D) | T; }		/* TDSxo */
rop_proc(rop247) { r ~ (S & D) | T; }		/* TDSano */
rop_proc(rop248) { r (S & D) | T; }		/* TDSao */
rop_proc(rop249) { r ~ (S ^ D) | T; }		/* TDSxno */
rop_proc(rop250) { r D | T; }		/* DTo */
rop_proc(rop251) { r (~S | T) | D; }		/* DTSnoo */
rop_proc(rop252) { r S | T; }		/* TSo */
rop_proc(rop253) { r (~D | S) | T; }	/* TSDnoo */
rop_proc(rop254) { r S | T | D; }	/* DTSoo */
rop_proc(rop255) { r ~(rop_operand)0; }	/* 1 */

#undef rop_proc
const far_data rop_proc rop_proc_tab[256] =
{
  rop0, rop1, rop2, rop3, rop4, rop5, rop6, rop7,
  rop8, rop9, rop10, rop11, rop12, rop13, rop14, rop15,
  rop16, rop17, rop18, rop19, rop20, rop21, rop22, rop23,
  rop24, rop25, rop26, rop27, rop28, rop29, rop30, rop31,
  rop32, rop33, rop34, rop35, rop36, rop37, rop38, rop39,
  rop40, rop41, rop42, rop43, rop44, rop45, rop46, rop47,
  rop48, rop49, rop50, rop51, rop52, rop53, rop54, rop55,
  rop56, rop57, rop58, rop59, rop60, rop61, rop62, rop63,
  rop64, rop65, rop66, rop67, rop68, rop69, rop70, rop71,
  rop72, rop73, rop74, rop75, rop76, rop77, rop78, rop79,
  rop80, rop81, rop82, rop83, rop84, rop85, rop86, rop87,
  rop88, rop89, rop90, rop91, rop92, rop93, rop94, rop95,
  rop96, rop97, rop98, rop99, rop100, rop101, rop102, rop103,
  rop104, rop105, rop106, rop107, rop108, rop109, rop110, rop111,
  rop112, rop113, rop114, rop115, rop116, rop117, rop118, rop119,
  rop120, rop121, rop122, rop123, rop124, rop125, rop126, rop127,
  rop128, rop129, rop130, rop131, rop132, rop133, rop134, rop135,
  rop136, rop137, rop138, rop139, rop140, rop141, rop142, rop143,
  rop144, rop145, rop146, rop147, rop148, rop149, rop150, rop151,
  rop152, rop153, rop154, rop155, rop156, rop157, rop158, rop159,
  rop160, rop161, rop162, rop163, rop164, rop165, rop166, rop167,
  rop168, rop169, rop170, rop171, rop172, rop173, rop174, rop175,
  rop176, rop177, rop178, rop179, rop180, rop181, rop182, rop183,
  rop184, rop185, rop186, rop187, rop188, rop189, rop190, rop191,
  rop192, rop193, rop194, rop195, rop196, rop197, rop198, rop199,
  rop200, rop201, rop202, rop203, rop204, rop205, rop206, rop207,
  rop208, rop209, rop210, rop211, rop212, rop213, rop214, rop215,
  rop216, rop217, rop218, rop219, rop220, rop221, rop222, rop223,
  rop224, rop225, rop226, rop227, rop228, rop229, rop230, rop231,
  rop232, rop233, rop234, rop235, rop236, rop237, rop238, rop239,
  rop240, rop241, rop242, rop243, rop244, rop245, rop246, rop247,
  rop248, rop249, rop250, rop251, rop252, rop253, rop254, rop255
};
