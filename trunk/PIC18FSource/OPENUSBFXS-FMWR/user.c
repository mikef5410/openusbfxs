/********************************************************************
 FileName:	user.c
 Dependencies:	See INCLUDES section
 Processor:	PIC18
 Hardware:	The code is intended to be used on the Open USB FXS board
 Compiler:  	Microchip C18

 Note:		This file has been adapted from published Microchip example
 		code. Hence, there is a substantial amount of code that
		cannot be copyrighted as my own. All contributed code is
		marked appropriately as such, and it is this part that I
		am placing under copyright.
 Copyright:	(sections marked with "avarvit") (C) Angelos Varvitsiotis 2009
 License:	GPLv3 (copyrighted part)

 ********************************************************************/

/** INCLUDES *******************************************************/

#include "Compiler.h"
#include "GenericTypeDefs.h"
#include "HardwareProfile.h"
#include "usb_config.h"
#include "USB/usb_device.h"
#include "USB/usb.h"
#include "USB/usb_function_generic.h"
//////////////// BEGIN contributed code by avarvit
#include <spi.h>

#include "user.h"
#include "tmr1_isr.h"
#include "proslic.h"
#include "pcmpacket.h"
#include "eeprom.h"

/** ROM CONSTANTS******************************************************/
rom const char *svnrevstr = "$Revision$";

//////////////// END contributed code by avarvit

/** V A R I A B L E S ********************************************************/
#pragma udata
BYTE old_sw2;
BYTE counter;

#pragma udata USB_VARS
DATA_PACKET INPacket;
DATA_PACKET OUTPacket;
#pragma udata

USB_HANDLE USBGenericOutHandle = 0;
USB_HANDLE USBGenericInHandle = 0;

//////////////// BEGIN contributed code by avarvit
// FIXME: probably move here
extern USB_HANDLE USBIsoOutHandle;
extern USB_HANDLE USBIsoIn_Handle;
extern BYTE OUTPCMData0[16];
extern BYTE OUTPCMData1[16];
extern BYTE IN_PCMData0[16];
extern BYTE IN_PCMData1[16];
//////////////// END contributed code by avarvit

// Timer0 - 1 second interval setup.
// Fosc/4 = 12MHz
// Use /256 prescaler, this brings counter freq down to 46,875 Hz
// Timer0 should = 65536 - 46875 = 18661 or 0x48E5
#define TIMER0L_VAL         0xE5
#define TIMER0H_VAL         0x48

/** P R I V A T E  P R O T O T Y P E S ***************************************/

void BlinkUSBStatus(void);
BOOL Switch2IsPressed(void);
void ResetTempLog(void);
WORD_VAL ReadPOT(void);
void ServiceRequests(void);

/** D E C L A R A T I O N S **************************************************/
#pragma udata
extern BYTE pauseIO, cnt4, cnt8, passout, pasHout, hkdtmf;
static BYTE drsena, drsseq;
#pragma code

#pragma code
void UserInit(void) {
//////////////// BEGIN contributed code by avarvit
    
    unsigned short i;			// general-purpose counter

    // designate appropriately various pins as input or output

    tris__cs_3210	= OUTPUT_PIN;	// \CS signal, from PIC to 3210
    tris__reset_3210	= OUTPUT_PIN;	// \RESET signal, from PIC to 3210
    tris__int_3210	= INPUT_PIN;	// \INT signal, from 3210 to PIC

    tris_pcm_3210_pclk	= OUTPUT_PIN;	// PCLK signal, from PIC to 3210
    tris_pcm_3210_drx	= OUTPUT_PIN;	// DRX signal, from PIC to 3210
    tris_pcm_3210_dtx	= INPUT_PIN;	// DTX signal, from 3210 to PIC
    tris_pcm_3210_fsync	= OUTPUT_PIN;	// FSYNC signal, from PIC to 3210

    // put the PCM data into a known initial state
    pcm_3210_pclk  = 0;
    pcm_3210_fsync = 0;
    pcm_3210_drx   = 0;

    // fix configuration for TMR1
      // RD16=1, T1RUN=0, T1CKPS1&0=00, T1OSCEN=0, T1SYNC=0, TMR1CS=0, TMR1ON=1
      // which means:
      // RD16=1           enable writing the timer value as one 16-bit operation
      // T1RUN=0          device clock does not come from TMR1
      // T1CKPS0-1=00     prescale 1:1
      // T1OSCEN=0        TMR1 oscillator is not enabled
      // T1SYNC=0         (ignored because TMR1CS=0)
      // TMR1CS=0         TMR1 source select = Fosc/4 (12MHz)
      // TMR1ON=1         TMR1 is enabled
    T1CON = 0b10000001;


    // fix configuration for TMR3
      // RD16=1, T3CPP2=0, T3CKPS1&0=00, T4CPP1=0, \Ô3SYNC=0, TMR3CS=0, TMR3ON=1
    T3CON = 0b10000001;

    // fix interrupts
    INTCONbits.GIE = 0;                 // temporarily disable interrupts
    RCONbits.IPEN = 0;                  // disable interrupt priorities
    // set to 1 later on
    // INTCONbits.PEIE = 0;		// disable periferal interrupts
    INTCONbits.TMR0IE = 0;		// disable timer0 interrupts
    INTCONbits.RBIE = 0;		// disable PortB change interrupts
    INTCONbits.PEIE = 1;	// why? does it need to be 1 for TMR1??
    INTCONbits.GIE = 1;                 // re-enable interrupts

    // enable TMR1 and map its interrupt to the high-priority vector
    IPR1bits.TMR1IP = 1;		// map TMR to high-p (N/A if IPEN==0)
    PIE1bits.TMR1IE = 1;		// enable TMR1 interrupts
    PIR1bits.TMR1IF = 0;		// clear TMR1 interrupt flag if set
    tmr1_isr_init ();                   // initialize TMR1 data and signals

    // wait for PCLK signal to become available, then reset 3210
    for (i = 0; i < 65535; i++) ;       // wait long enough for TIMR1 to fire
    _cs_3210 = 1;                       // deselect the 3210 chip
    _reset_3210 = 0;                    // reset the 3210 chip
    for (i = 0; i < 4096; i++) ;        // wait some time
    _reset_3210 = 1;                    // set the chip to the not reset state

    // setup SPI 
      // MODE_11 translates to SSPCON1 bits CKE=0 (we transmit on falling edge
      // -- notice how this bit is complemented in the source code) and CKP=1
      // (clock polarity set to idle state high) as required by the 3210, p.53
      // notice also that the 3210 seems to be asserting both its input and its
      // output on rising clock edge, so SMPMID should be OK
    OpenSPI (SPI_FOSC_16, MODE_11, SMPMID);
    // OpenSPI (SPI_FOSC_4, MODE_11, SMPMID);

    mInitAllLEDs();
    mInitAllSwitches();
    old_sw2 = sw2;
    
    // setup tmr0 (enabled by default)
    T0CON = 0b10010111;

    // set the ISR function to transmit data
    pauseIO = 0;

    // disable setting DRs from PCM data
    drsena = 0;
    drsseq = 0;

    // set hkdtmf to a known value
    hkdtmf = 0;
//////////////// END contributed code by avarvit
}//end UserInit

/******************************************************************************
 * Function:        void ProcessIO(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is a place holder for other user routines.
 *                  It is a mixture of both USB and non-USB tasks.
 *
 * Note:            None
 *****************************************************************************/
void ProcessIO(void)
{   
    // Si3210 interrupt status registers
    BYTE DR19, DR20, DR68, DR24;
    BlinkUSBStatus();
    // User Application USB tasks
    if((USBDeviceState < CONFIGURED_STATE)||(USBSuspendControl==1)) return;

    // service 3210 interrupts that indicate hook change or DTMF events

    if (!_int_3210) {
	// interrupts (should) occur only on hook state changes or DTMF detect

	// Debugging -- don't comment back in
	// BYTE DR18 = ReadProSLICDirectRegister (18);
	// IN_PCMData0[4] = DR18;

	// check for hook state
	DR19 = ReadProSLICDirectRegister (19);
	// Debugging
	// IN_PCMData0[5] = DR19;
	if (DR19) {
	    WriteProSLICDirectRegister (19, DR19);	// clears interrupt
	    if (DR19 & 0x03) {				// hook state changed
	        DR68 = ReadProSLICDirectRegister (68);
		if (DR68 & 0x03) {			// phone is off-hook
		    hkdtmf |= 0x80;			// set or clear bit 7
		}
		else {
		    hkdtmf &= 0x7F;			
		}
	    }
	}
	// check for dtmf state
	DR20 = ReadProSLICDirectRegister (20);
	// Debugging
	// IN_PCMData0[6] = DR20;
	if (DR20) {					
	    WriteProSLICDirectRegister (20, DR20);	// clears interrupt
	    if (DR20 & 0x01) {				// DTMF event
	        DR24 = ReadProSLICDirectRegister (24);	// get DTMF state
		hkdtmf &= 0xe0;				// forget previous state
		hkdtmf |= (DR24 & 0x1f);		// mask into hkdtmf
	    }
	}


	// TODO, some day: other 3210 interrupts, although enabled, remain
	// unserviced; when this happens, the LED flashes very slowly,
	// because the above code is executed over and over again without
	// clearing the interrupt; it would be a good idea to provide a
	// fix to that, by returning a decent hkdtmf value that the
	// driver would understand, so it would do further debugging,
	// or even a converter powerdown if a non-transient power-related
	// interrupt is detected
    }
    else {
    	// if DTMF was detected in previous round, keep probing DR 24
        if (hkdtmf & 0x10) {				// dtmf detected
	    DR24 = ReadProSLICDirectRegister (24);	// get DTMF state
	    if (!(DR24 & 0x10)) {			// still being pressed?
		hkdtmf &= 0xe0;				// no, reset our state
	    }
	}
    }

    // provide DR setting capability via isochronous packets; sequence
    // number for DR setting operation, register # and register value
    // are contained in packet headers
    if (drsena) {
	// quickly sample just OUT_0 packet for sequence number changes
	if (*((BYTE *) OUT_0_DRSSEQ) != drsseq) { // !=, or we risk missing data

	    // do sampling/consistency checks to avoid using unstable data
	    // (remember that we run asynchronously related to tmr1_isr,
	    // and that locking is not really available or feasible, so
	    // robust sampling is the best we can do here)

	    BYTE smplseq, smplreg, smplval;
	    smplseq = *((BYTE *) OUT_0_DRSSEQ);
	    smplreg = *((BYTE *) OUT_0_DRSREG);
	    smplval = *((BYTE *) OUT_0_DRSVAL);
	    if (
	      *((BYTE *) OUT_1_DRSSEQ) == smplseq &&
	      *((BYTE *) OUT_1_DRSREG) == smplreg &&
	      *((BYTE *) OUT_1_DRSVAL) == smplval) {
		drsseq = smplseq;
	    	if (IsValidPROSLICDirectRegister (smplreg)) {
		    WriteProSLICDirectRegister (smplreg, smplval);
		}
	    }
	}
    }

    //respond to any USB commands that might have come over the bus
    ServiceRequests();

    //if the 1 second timer has expired (PIC18 only)
    if(INTCONbits.TMR0IF == 1)
    {
	INTCONbits.TMR0IF = 0;          // Clear flag
	TMR0H = TIMER0H_VAL;
	TMR0L = TIMER0L_VAL;            // Reinit timer value;
    }//end if
}//end ProcessIO

/******************************************************************************
 * Function:        void ServiceRequests(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    USB traffic can be generated
 * Overview:        This function takes in the commands from the PC from the
 *                  application and executes the commands requested
 * Note:            None
 *****************************************************************************/
void ServiceRequests(void) {
    BYTE index;
    WORD indval;
    unsigned short i;			// general-purpose counter
    BYTE *bp;
    rom char *cp;
    
    //Check to see if data has arrived
    if(!USBHandleBusy(USBGenericOutHandle)) {        
        //if the handle is no longer busy then the last
        //transmission is complete
       
        counter = 0;

        INPacket.CMD=OUTPacket.CMD;
        INPacket.len=OUTPacket.len;

        //process the command
        switch(OUTPacket.CMD) {
            case READ_VERSION:
                //dataPacket._byte[1] is len
                INPacket._byte[2] = MINOR_VERSION;
                INPacket._byte[3] = MAJOR_VERSION;
                counter=0x04;
                break;

//////////////// BEGIN contributed code by avarvit
	    // Note: the READ_VERSION message is the same as for the PICDEM
	    // board. This is intentional, so that the PICDEM tool can see
	    // our board in demo mode. Same holds for the RESET message at
	    // the end of the switch() statement.


	    case PROSLIC_SCURRENT:	// FIXME: do these
	    case PROSLIC_RCURRENT:	// read current ProSLIC direct register
	        Nop();
		break;

	    case PROSLIC_WDIRECT:	// write indicated direct register
	    	if (!IsValidPROSLICDirectRegister (OUTPacket._byte[1])) {
		    // will not reply requests for invalid registers
		    break;
		}
		WriteProSLICDirectRegister (OUTPacket._byte[1],
		  OUTPacket._byte[2]);

		// we slip through intentionally to the next 'case', in order
		// to return the value read back by the DR just written to
		// (the two values may differ, depending on the specific
		// register's function)

	    case PROSLIC_RDIRECT:	// read indicated direct register
	    	if (!IsValidPROSLICDirectRegister (OUTPacket._byte[1])) {
		    // will not reply requests for invalid registers
		    break;
		}
		INPacket._byte[2] = 
		  ReadProSLICDirectRegister (OUTPacket._byte [1]);

		counter = 0x03;
		break;
	    
	    case PROSLIC_WRINDIR:
		// fix this by adding a type in user.h
	    	indval = OUTPacket._byte[2] | (((WORD)OUTPacket._byte[3] << 8));
		WriteProSLICIndirectRegister (OUTPacket._byte[1], indval);

		// we slip through intentionally to the next 'case', in order
		// to return the value read back by the IR just written to
		// (the two values may differ, depending on the specific
		// register's function)

	    case PROSLIC_RDINDIR:
	    	// fix this by adding a WORD packet type in user.h
		indval = ReadProSLICIndirectRegister (OUTPacket._byte[1]);
		INPacket._byte[2] = indval & 0xff;
		INPacket._byte[3] = (indval & 0xff00) >> 8;

		counter = 0x04;
		break;

	    case DEBUG_GET_CNT48:
	    	INPacket._byte[2] = cnt4;
		INPacket._byte[3] = cnt8;
		counter = 0x04;
		break;

	    case DEBUG_GET_PSOUT:
	    	INPacket._byte[2] = passout;
		// passout = 0;
		counter = 0x03;
		break;

	    case DEBUG_GET_PSWRD:
	    	INPacket._byte[2] = passout;
	    	INPacket._byte[3] = pasHout;
		// passout = 0;
		// pasHout = 0;
		counter = 0x04;
		break;

	    case GET_FXS_VERSION:
		INPacket._byte[2] = FXS_MAJOR_VERSION;
		INPacket._byte[3] = FXS_MINOR_VERSION;
		INPacket._byte[4] = FXS_REVISION_NMBR;
		counter = 0x05;
		break;

	    case WRITE_SERIAL_NO:
		// note: this primitive expects the serial number as a four-byte
		// binary string; this is converted into hex when reported to
		// the host during USB device enumeration
	        eeWrite (EE_SERIAL_NO, &OUTPacket._byte[2], EE_SERIAL_NO_LEN);
		eeRead (EE_SERIAL_NO,
		  &INPacket._byte[2], EE_SERIAL_NO_LEN); // verify it
		counter = 2 + EE_SERIAL_NO_LEN;
		break;

	    case REBOOT_BOOTLOAD:
	        eeWrite(EE_BOOTLOAD_FLAG, &OUTPacket.CMD, EE_BOOTLOAD_FLAG_LEN);
		Reset();
		

	    case START_STOP_ISOV2:
	        drsena = 1;
		drsseq = OUTPacket._byte[2];	// set the next DR set seq #
		// intentional slip through

	    case START_STOP_ISO:
		if (OUTPacket._byte[1]) {
		    pauseIO = 0xFF;		// pause PCM I/O
		    drsena = 0;			// disable setting DRs
		}
		else {
		    IN_PCMData0[0] = 0xBA;	// magic number (low), even pck
		    IN_PCMData1[0] = 0xBA;	// magic number (low), odd pck
		    IN_PCMData0[1] = 0xEE;	// magic number (high)
		    IN_PCMData1[1] = 0xDD;	// magic number (high)
		    IN_PCMData0[2] = 0;		// DTMF and hook state
		    IN_PCMData1[2] = 0;		// DTMF and hook state
		    IN_PCMData0[3] = 0;		// USB_DEBUG?mirrored serial OUT
		    IN_PCMData1[3] = 0;		// USB_DEBUG?mirrored serial OUT
		    IN_PCMData0[4] = 0;		// (unused on even packets)
		    IN_PCMData1[4] = 0;		// USB_DEBUG? TMR3L
		    IN_PCMData0[5] = 0;		// (unused on even packets)
		    IN_PCMData1[5] = 0;		// USB_DEBUG? TMR3H
		    IN_PCMData0[6] = 0;		// (ubused on even packets)
		    IN_PCMData1[6] = 0;		// # of OUT non-receipts
		    IN_PCMData0[7] = 0;		// own serial (even packets)
		    IN_PCMData1[7] = 0;		// own serial (odd packets)
		    OUTPCMData0[8]  = 0xFF;
		    OUTPCMData0[9]  = 0xFF;
		    OUTPCMData0[10] = 0xFF;
		    OUTPCMData0[11] = 0xFF;
		    OUTPCMData0[12] = 0xFF;
		    OUTPCMData0[13] = 0xFF;
		    OUTPCMData0[14] = 0xFF;
		    OUTPCMData0[15] = 0xFF;
		    OUTPCMData1[8]  = 0xFF;
		    OUTPCMData1[9]  = 0xFF;
		    OUTPCMData1[10] = 0xFF;
		    OUTPCMData1[11] = 0xFF;
		    OUTPCMData1[12] = 0xFF;
		    OUTPCMData1[13] = 0xFF;
		    OUTPCMData1[14] = 0xFF;
		    OUTPCMData1[15] = 0xFF;

		    pauseIO = 0x00;		// don't pause PCM IO
		    passout = 0x00;		// reset debug value
		}


		break;

	    case SOF_PROFILE:
		PIE1bits.TMR1IE = 0;	// temporarily disable TMR1 interrupts
		for (i = 0; i < 65535; i++) ;       // wait for TIMR1 to expire
		
		INPacket.len = USBGEN_EP_SIZE;
		for (bp = &INPacket._byte[2]; bp < &INPacket._byte[USBGEN_EP_SIZE]; bp++) {
		    UIRbits.SOFIF = 0;		// clear SOF interrupt flag
		    while (!(UIRbits.SOFIF));	// wait until it is reasserted
		    *bp = TMR3L;		// and note down TMR3's value
		    bp++;
		    *bp = TMR3H;
		}
    		
		PIE1bits.TMR1IE = 1;	// re-enable TMR1 interrupts
		PIR1bits.TMR1IF = 0;	// clear TMR1 interrupt flag if set
		tmr1_isr_init ();	// re-initialize TMR1 data and signals

		// wait for PCLK signal to become available, then reset 3210
		for (i = 0; i < 65535; i++) ;       // wait for TIMR1 to fire
		_cs_3210 = 1;		// deselect the 3210 chip
		_reset_3210 = 0;	// reset the 3210 chip
		for (i = 0; i < 4096; i++) ;        // wait some time
		_reset_3210 = 1;	// set the chip to the not reset state

		counter = USBGEN_EP_SIZE;

	    	break;

//////////////// END contributed code by avarvit

            case RESET:
                Reset();
                break;
                
            default:
                Nop();
                break;
        }//end switch()
        if(counter != 0) {
            if(!USBHandleBusy(USBGenericInHandle)) {
		// could add this to synchronize sending (after ISO)
	    	// while ((!pauseIO) && (cnt4 < 3)) {/* wait */}
                USBGenericInHandle = USBGenWrite(USBGEN_EP_NUM,(BYTE*)&INPacket,counter);
            }
        }//end if
        
        //Re-arm the OUT endpoint for the next packet
        USBGenericOutHandle = USBGenRead(USBGEN_EP_NUM,(BYTE*)&OUTPacket,USBGEN_EP_SIZE);
    }//end if

}//end ServiceRequests

/********************************************************************
 * Function:        void BlinkUSBStatus(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        BlinkUSBStatus turns on and off LEDs 
 *                  corresponding to the USB device state.
 * Note:            mLED macros can be found in HardwareProfile.h
 *                  USBDeviceState is declared and updated in
 *                  usb_device.c.
 *******************************************************************/
void BlinkUSBStatus(void) {
    static WORD led_count=0;
    
    if(led_count == 0)led_count = 10000U;
    led_count--;

    #define mLED_Both_Off()         {mLED_1_Off();mLED_2_Off();}
    #define mLED_Both_On()          {mLED_1_On();mLED_2_On();}
    #define mLED_Only_1_On()        {mLED_1_On();mLED_2_Off();}
    #define mLED_Only_2_On()        {mLED_1_Off();mLED_2_On();}

    if(USBSuspendControl == 1) {
        if(led_count==0) {
            mLED_1_Toggle();
            // mLED_2 = mLED_1;        // Both blink at the same time
        }//end if
    }
    else {
        if(USBDeviceState == DETACHED_STATE) {
            mLED_Both_Off();
        }
        else if(USBDeviceState == ATTACHED_STATE) {
            mLED_Both_On();
        }
        else if(USBDeviceState == POWERED_STATE) {
            mLED_Only_1_On();
        }
        else if(USBDeviceState == DEFAULT_STATE) {
            mLED_Only_2_On();
        }
        else if(USBDeviceState == ADDRESS_STATE) {
            if(led_count == 0) {
                mLED_1_Toggle();
                mLED_2_Off();
            }//end if
        }
        else if(USBDeviceState == CONFIGURED_STATE) {
            if(led_count==0) {
                mLED_1_Toggle();
                // mLED_2 = !mLED_1;       // Alternate blink                
            }//end if
        }//end if(...)
    }//end if(UCONbits.SUSPND...)

}//end BlinkUSBStatus


/******************************************************************************
 * Function:        BOOL Switch2IsPressed(void)
 * PreCondition:    None
 * Input:           None
 * Output:          BOOL - TRUE if the SW2 was pressed and FALSE otherwise
 * Side Effects:    None
 * Overview:        returns TRUE if the SW2 was pressed and FALSE otherwise
 * Note:            None
 *****************************************************************************/
BOOL Switch2IsPressed(void) {
    if(sw2 != old_sw2) {
        old_sw2 = sw2;                  // Save new value
        if(sw2 == 0)                    // If pressed
            return TRUE;                // Was pressed
    }//end if
    return FALSE;                       // Was not pressed
}//end Switch2IsPressed

/** EOF user.c ***************************************************************/
