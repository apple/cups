/*
 * "$Id$"
 *
 *   Backend support definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_BACKEND_PRIVATE_H_
#  define _CUPS_BACKEND_PRIVATE_H_


/*
 * Include necessary headers.
 */

#  include <cups/backend.h>
#  include <cups/sidechannel.h>
#  include <cups/cups.h>
#  include <cups/debug.h>
#  include <cups/i18n.h>
#  include <cups/snmp.h>
#  include <stdlib.h>
#  include <errno.h>
#  include <cups/string.h>
#  include <signal.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * OID constants...
 */

#define CUPS_OID_mib2				1,3,6,1,2,1

#define CUPS_OID_host				CUPS_OID_mib2,25

#define CUPS_OID_hrSystem			CUPS_OID_host,1

#define CUPS_OID_hrStorage			CUPS_OID_host,2

#define CUPS_OID_hrDevice			CUPS_OID_host,3
#define CUPS_OID_hrDeviceTable			CUPS_OID_hrDevice,2
#define CUPS_OID_hrDeviceEntry			CUPS_OID_hrDeviceTable,1
#define CUPS_OID_hrDeviceType			CUPS_OID_hrDeviceEntry,2
#define CUPS_OID_hrDeviceDescr			CUPS_OID_hrDeviceEntry,3

#define CUPS_OID_printmib			CUPS_OID_mib2,43

#define CUPS_OID_prtGeneral			CUPS_OID_printmib,5
#define CUPS_OID_prtGeneralTable		CUPS_OID_prtGeneral,1
#define CUPS_OID_prtGeneralEntry		CUPS_OID_prtGeneralTable,1
#define CUPS_OID_prtGeneralPrinterName		CUPS_OID_prtGeneralEntry,16
#define CUPS_OID_prtGeneralSerialNumber		CUPS_OID_prtGeneralEntry,17

#define CUPS_OID_prtCover			CUPS_OID_printmib,6
#define CUPS_OID_prtCoverTable			CUPS_OID_prtCover,1
#define CUPS_OID_prtCoverEntry			CUPS_OID_prtCoverTable,1
#define CUPS_OID_prtCoverDescription		CUPS_OID_prtCoverEntry,2
#define CUPS_OID_prtCoverStatus			CUPS_OID_prtCoverEntry,3

#define CUPS_OID_prtMarker			CUPS_OID_printmib,10
#define CUPS_OID_prtMarkerTable			CUPS_OID_prtMarker,2
#define CUPS_OID_prtMarkerEntry			CUPS_OID_prtMarkerTable,1
#define CUPS_OID_prtMarkerLifeCount		CUPS_OID_prtMarkerEntry,4

#define CUPS_OID_prtMarkerSupplies		CUPS_OID_printmib,11
#define CUPS_OID_prtMarkerSuppliesTable		CUPS_OID_prtMarkerSupplies,1
#define CUPS_OID_prtMarkerSuppliesEntry		CUPS_OID_prtMarkerSuppliesTable,1
#define CUPS_OID_prtMarkerSuppliesIndex		CUPS_OID_prtMarkerSuppliesEntry,1
#define CUPS_OID_prtMarkerSuppliesMarkerIndex	CUPS_OID_prtMarkerSuppliesEntry,2
#define CUPS_OID_prtMarkerSuppliesColorantIndex	CUPS_OID_prtMarkerSuppliesEntry,3
#define CUPS_OID_prtMarkerSuppliesClass		CUPS_OID_prtMarkerSuppliesEntry,4
#define CUPS_OID_prtMarkerSuppliesType		CUPS_OID_prtMarkerSuppliesEntry,5
#define CUPS_OID_prtMarkerSuppliesDescription	CUPS_OID_prtMarkerSuppliesEntry,6
#define CUPS_OID_prtMarkerSuppliesSupplyUnit	CUPS_OID_prtMarkerSuppliesEntry,7
#define CUPS_OID_prtMarkerSuppliesMaxCapacity	CUPS_OID_prtMarkerSupliesEntry,8
#define CUPS_OID_prtMarkerSuppliesLevel		CUPS_OID_prtMarkerSupliesEntry,9

#define CUPS_OID_prtMarkerColorant		CUPS_OID_printmib,12
#define CUPS_OID_prtMarkerColorantTable		CUPS_OID_prtMarkerColorant,1
#define CUPS_OID_prtMarkerColorantEntry		CUPS_OID_prtMarkerColorantTable,1
#define CUPS_OID_prtMarkerColorantIndex		CUPS_OID_prtMarkerColorantEntry,1
#define CUPS_OID_prtMarkerColorantMarkerIndex	CUPS_OID_prtMarkerColorantEntry,2
#define CUPS_OID_prtMarkerColorantRole		CUPS_OID_prtMarkerColorantEntry,3
#define CUPS_OID_prtMarkerColorantValue		CUPS_OID_prtMarkerColorantEntry,4
#define CUPS_OID_prtMarkerColorantTonality	CUPS_OID_prtMarkerColorantEntry,5

#define CUPS_OID_prtInterpreter			CUPS_OID_printmib,15
#define CUPS_OID_prtInterpreterTable		CUPS_OID_prtInterpreter,1
#define CUPS_OID_prtInterpreterEntry		CUPS_OID_prtInterpreterTable,1
#define CUPS_OID_prtInterpreterLangFamily	CUPS_OID_prtInterpreterEntry,2
#define CUPS_OID_prtInterpreterLangLevel	CUPS_OID_prtInterpreterEntry,3


/*
 * State constants...
 */

#define CUPS_TC_other				1
#define CUPS_TC_unknown				2

#define CUPS_TC_prtCoverStatus_coverOpen	3
#define CUPS_TC_prtCoverStatus_coverClosed	4
#define CUPS_TC_prtCoverStatus_interlockOpen	5
#define CUPS_TC_prtCoverStatus_interlockClosed	6

#define CUPS_TC_langPCL				3
#define CUPS_TC_langHPGL			4
#define CUPS_TC_langPJL				5
#define CUPS_TC_langPS				6
#define CUPS_TC_langEscapeP			9
#define CUPS_TC_langCCITT			26
#define CUPS_TC_langLIPS			39
#define CUPS_TC_langTIFF			40
#define CUPS_TC_langPCLXL			47
#define CUPS_TC_langPDF				54
#define CUPS_TC_langJPEG			61

#define CUPS_TC_toner				3
#define CUPS_TC_wasteToner			4
#define CUPS_TC_ink				5
#define CUPS_TC_inkCartridge			6
#define CUPS_TC_inkRibbon			7
#define CUPS_TC_wasteInk			8
#define CUPS_TC_opc				9
#define CUPS_TC_developer			10
#define CUPS_TC_fuserOil			11
#define CUPS_TC_solidWax			12
#define CUPS_TC_ribbonWax			13
#define CUPS_TC_wasteWax			14
#define CUPS_TC_fuser				15
#define CUPS_TC_coronaWire			16
#define CUPS_TC_fuserOilWick			17
#define CUPS_TC_cleanerUnit			18
#define CUPS_TC_fuserCleaningPad		19
#define CUPS_TC_transferUnit			20
#define CUPS_TC_tonerCartridge			21
#define CUPS_TC_fuserOiler			22
#define CUPS_TC_water				23
#define CUPS_TC_wasteWater			24
#define CUPS_TC_glueWaterAdditive		25
#define CUPS_TC_wastePaper			26
#define CUPS_TC_bindingSupply			27
#define CUPS_TC_bandingSupply			28
#define CUPS_TC_stitchingWire			29
#define CUPS_TC_shrinkWrap			30
#define CUPS_TC_paperWrap			31
#define CUPS_TC_staples				32
#define CUPS_TC_inserts				33
#define CUPS_TC_covers				34


/*
 * Prototypes...
 */

extern int	backendDrainOutput(int print_fd, int device_fd);
extern int	backendGetDeviceID(int fd, char *device_id, int device_id_size,
		                   char *make_model, int make_model_size,
				   const char *scheme, char *uri, int uri_size);
extern int	backendGetMakeModel(const char *device_id, char *make_model,
			            int make_model_size);
extern ssize_t	backendRunLoop(int print_fd, int device_fd, int use_bc,
		               void (*side_cb)(int print_fd, int device_fd,
			                       int use_bc));


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_BACKEND_PRIVATE_H_ */


/*
 * End of "$Id$".
 */
