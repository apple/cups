/*
 * "$Id: backend-private.h 7810 2008-07-29 01:11:15Z mike $"
 *
 *   Backend support definitions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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

#  include <cups/cups-private.h>
#  include <cups/snmp-private.h>
#  include <cups/backend.h>
#  include <cups/sidechannel.h>
#  include <signal.h>

#  ifdef __linux
#    include <sys/ioctl.h>
#    include <linux/lp.h>
#    define IOCNR_GET_DEVICE_ID		1
#    define LPIOC_GET_DEVICE_ID(len)	_IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
#    include <linux/parport.h>
#    include <linux/ppdev.h>
#    include <unistd.h>
#    include <fcntl.h>
#  endif /* __linux */

#  ifdef __sun
#    ifdef __sparc
#      include <sys/ecppio.h>
#    else
#      include <sys/ioccom.h>
#      include <sys/ecppsys.h>
#    endif /* __sparc */
#  endif /* __sun */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * OID constants...
 */

/* Host MIB */
#define CUPS_OID_mib2				1,3,6,1,2,1

#define CUPS_OID_system				CUPS_OID_mib2,1
#define CUPS_OID_sysLocation			CUPS_OID_system,6

#define CUPS_OID_host				CUPS_OID_mib2,25

#define CUPS_OID_hrSystem			CUPS_OID_host,1

#define CUPS_OID_hrStorage			CUPS_OID_host,2

#define CUPS_OID_hrDevice			CUPS_OID_host,3
#define CUPS_OID_hrDeviceTable			CUPS_OID_hrDevice,2
#define CUPS_OID_hrDeviceEntry			CUPS_OID_hrDeviceTable,1
#define CUPS_OID_hrDeviceIndex			CUPS_OID_hrDeviceEntry,1
#define CUPS_OID_hrDeviceType			CUPS_OID_hrDeviceEntry,2
#define CUPS_OID_hrDeviceDescr			CUPS_OID_hrDeviceEntry,3

#define CUPS_OID_hrPrinterTable			CUPS_OID_hrDevice,5
#define CUPS_OID_hrPrinterEntry			CUPS_OID_hrPrinterTable,1
#define CUPS_OID_hrPrinterStatus		CUPS_OID_hrPrinterEntry,1
#define CUPS_OID_hrPrinterDetectedErrorState	CUPS_OID_hrPrinterEntry,2

/* Printer MIB */
#define CUPS_OID_printmib			CUPS_OID_mib2,43

#define CUPS_OID_prtGeneral			CUPS_OID_printmib,5
#define CUPS_OID_prtGeneralTable		CUPS_OID_prtGeneral,1
#define CUPS_OID_prtGeneralEntry		CUPS_OID_prtGeneralTable,1
#define CUPS_OID_prtGeneralCurrentLocalization	CUPS_OID_prtGeneralEntry,2
#define CUPS_OID_prtGeneralPrinterName		CUPS_OID_prtGeneralEntry,16
#define CUPS_OID_prtGeneralSerialNumber		CUPS_OID_prtGeneralEntry,17

#define CUPS_OID_prtCover			CUPS_OID_printmib,6
#define CUPS_OID_prtCoverTable			CUPS_OID_prtCover,1
#define CUPS_OID_prtCoverEntry			CUPS_OID_prtCoverTable,1
#define CUPS_OID_prtCoverDescription		CUPS_OID_prtCoverEntry,2
#define CUPS_OID_prtCoverStatus			CUPS_OID_prtCoverEntry,3

#define CUPS_OID_prtLocalization		CUPS_OID_printmib,7
#define CUPS_OID_prtLocalizationTable		CUPS_OID_prtLocalization,1
#define CUPS_OID_prtLocalizationEntry		CUPS_OID_prtLocalizationTable,1
#define CUPS_OID_prtLocalizationCharacterSet	CUPS_OID_prtLocalizationEntry,4

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
#define CUPS_OID_prtMarkerSuppliesMaxCapacity	CUPS_OID_prtMarkerSuppliesEntry,8
#define CUPS_OID_prtMarkerSuppliesLevel		CUPS_OID_prtMarkerSuppliesEntry,9

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

/* Printer Port Monitor MIB */
#define CUPS_OID_enterprises			1,3,6,1,4,1
#define CUPS_OID_pwg				CUPS_OID_enterprises,2699,1
#define CUPS_OID_ppmMIB				CUPS_OID_pwg,2
#define CUPS_OID_ppmMIBObjects			CUPS_OID_ppmMIB,1

#define CUPS_OID_ppmGeneral			CUPS_OID_ppmMIBObjects,1

#define CUPS_OID_ppmPrinter			CUPS_OID_ppmMIBObjects,2
#define CUPS_OID_ppmPrinterTable		CUPS_OID_ppmPrinter,1
#define CUPS_OID_ppmPrinterEntry		CUPS_OID_ppmPrinterTable,1
#define CUPS_OID_ppmPrinterIndex		CUPS_OID_ppmPrinterEntry,1
#define CUPS_OID_ppmPrinterName			CUPS_OID_ppmPrinterEntry,2
#define CUPS_OID_ppmPrinterIEEE1284DeviceId	CUPS_OID_ppmPrinterEntry,3
#define CUPS_OID_ppmPrinterNumberOfPorts	CUPS_OID_ppmPrinterEntry,4
#define CUPS_OID_ppmPrinterPreferredPortIndex	CUPS_OID_ppmPrinterEntry,5
#define CUPS_OID_ppmPrinterHrDeviceIndex	CUPS_OID_ppmPrinterEntry,6
#define CUPS_OID_ppmPrinterSnmpCommunityName	CUPS_OID_ppmPrinterEntry,7
#define CUPS_OID_ppmPrinterSnmpQueryEnabled	CUPS_OID_ppmPrinterEntry,8

#define CUPS_OID_ppmPort			CUPS_OID_ppmMIBObjects,3
#define CUPS_OID_ppmPortTable			CUPS_OID_ppmPort,1
#define CUPS_OID_ppmPortEntry			CUPS_OID_ppmPortTable,1
#define CUPS_OID_ppmPortIndex			CUPS_OID_ppmPortEntry,1
#define CUPS_OID_ppmPortEnabled			CUPS_OID_ppmPortEntry,2
#define CUPS_OID_ppmPortName			CUPS_OID_ppmPortEntry,3
#define CUPS_OID_ppmPortServiceNameOrURI	CUPS_OID_ppmPortEntry,4
#define CUPS_OID_ppmPortProtocolType		CUPS_OID_ppmPortEntry,5
#define CUPS_OID_ppmPortProtocolTargetPort	CUPS_OID_ppmPortEntry,6
#define CUPS_OID_ppmPortProtocolAltSourceEnabled CUPS_OID_ppmPortEntry,7
#define CUPS_OID_ppmPortPrtChannelIndex		CUPS_OID_ppmPortEntry,8
#define CUPS_OID_ppmPortLprByteCountEnabled	CUPS_OID_ppmPortEntry,9


/*
 * State constants...
 */

#define CUPS_TC_other				1
#define CUPS_TC_unknown				2

#define CUPS_TC_idle				3
#define CUPS_TC_printing			4
#define CUPS_TC_warmup				5

/* These come from the hrPrinterDetectedErrorState OCTET-STRING */
#define CUPS_TC_lowPaper			0x8000
#define CUPS_TC_noPaper				0x4000
#define CUPS_TC_lowToner			0x2000
#define CUPS_TC_noToner				0x1000
#define CUPS_TC_doorOpen			0x0800
#define CUPS_TC_jammed				0x0400
#define CUPS_TC_offline				0x0200
#define CUPS_TC_serviceRequested		0x0100
#define CUPS_TC_inputTrayMissing		0x0080
#define CUPS_TC_outputTrayMissing		0x0040
#define CUPS_TC_markerSupplyMissing		0x0020
#define CUPS_TC_outputNearFull			0x0010
#define CUPS_TC_outputFull			0x0008
#define CUPS_TC_inputTrayEmpty			0x0004
#define CUPS_TC_overduePreventMaint		0x0002

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

#define CUPS_TC_supplyThatIsConsumed		3
#define CUPS_TC_receptacleThatIsFilled		4

#define CUPS_TC_process				3
#define CUPS_TC_spot				4

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

/* These come from RFC 3808 to define character sets we support */
/* Also see http://www.iana.org/assignments/character-sets */
#define CUPS_TC_csASCII				3
#define CUPS_TC_csISOLatin1			4
#define CUPS_TC_csShiftJIS			17
#define CUPS_TC_csUTF8				106
#define CUPS_TC_csUnicode			1000 /* UCS2 BE */
#define CUPS_TC_csUCS4				1001 /* UCS4 BE */
#define CUPS_TC_csUnicodeASCII			1002
#define CUPS_TC_csUnicodeLatin1			1003
#define CUPS_TC_csUTF16BE			1013
#define CUPS_TC_csUTF16LE			1014
#define CUPS_TC_csUTF32				1017
#define CUPS_TC_csUTF32BE			1018
#define CUPS_TC_csUTF32LE			1019


/*
 * Types...
 */

typedef int (*_cups_sccb_t)(int print_fd, int device_fd, int snmp_fd,
			    http_addr_t *addr, int use_bc);


/*
 * Prototypes...
 */

extern void		backendCheckSideChannel(int snmp_fd, http_addr_t *addr);
extern int		backendDrainOutput(int print_fd, int device_fd);
extern int		backendGetDeviceID(int fd, char *device_id,
			                   int device_id_size,
			                   char *make_model,
					   int make_model_size,
					   const char *scheme, char *uri,
					   int uri_size);
extern int		backendGetMakeModel(const char *device_id,
			                    char *make_model,
				            int make_model_size);
extern int		backendNetworkSideCB(int print_fd, int device_fd,
			                     int snmp_fd, http_addr_t *addr,
					     int use_bc);
extern ssize_t		backendRunLoop(int print_fd, int device_fd, int snmp_fd,
			               http_addr_t *addr, int use_bc,
			               int update_state, _cups_sccb_t side_cb);
extern int		backendSNMPSupplies(int snmp_fd, http_addr_t *addr,
			                    int *page_count,
					    int *printer_state);
extern int		backendWaitLoop(int snmp_fd, http_addr_t *addr,
			                int use_bc, _cups_sccb_t side_cb);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_BACKEND_PRIVATE_H_ */


/*
 * End of "$Id: backend-private.h 7810 2008-07-29 01:11:15Z mike $".
 */
