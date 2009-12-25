#if 0

 FileName:	pcmpacket.h
 Dependencies:	
 Processor:	PIC18
 Hardware:	The code is intended to be used on the Open USB FXS board
 Compiler:  	Microchip C18
 Copyright:	(C) Angelos Varvitsiotis 2009
 License:	GPLv3

 Note: Because this file is included in the assembler as well as in C source,
       I have #if'ed out this comments part. These are the various fields in
       the PCM headers. Fields are different for the even and odd packets; in
       general, odd ones contain more debugging information. The only really
       important field is the DTMF and hook state.

#endif

#define	IN_EVN_MAGIC1	IN_PCMData0 + 0
#define IN_ODD_MAGIC1	IN_PCMData1 + 0
#define IN_EVN_MAGIC2	IN_PCMData0 + 1
#define IN_ODD_MAGIC2	IN_PCMData1 + 1
#define IN_EVN_HKDTMF	IN_PCMData0 + 2
#define IN_ODD_HKDTMF	IN_PCMData1 + 2
#define IN_EVN_MOUTSN	IN_PCMData0 + 3
#define IN_ODD_MOUTSN	IN_PCMData1 + 3
#define IN_EVN_UNUSD4	IN_PCMData0 + 4
#define IN_ODD_TMR3LV	IN_PCMData1 + 4
#define IN_EVN_UNUSD5	IN_PCMData0 + 5
#define IN_ODD_TMR3HV	IN_PCMData1 + 5
#define IN_EVN_UNUSD6	IN_PCMData0 + 6
#define	IN_ODD_LOSSES	IN_PCMData1 + 6
#define IN_EVN_SERIAL	IN_PCMData0 + 7
#define IN_ODD_SERIAL	IN_PCMData1 + 7
