/********************************************************************
 FileName:	main.c
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
 *******************************************************************/


/** INCLUDES *******************************************************/
#include "Compiler.h"
#include "HardwareProfile.h"
#include "GenericTypeDefs.h"
#include "USB/usb_device.h"
#include "USB/usb.h"
#include "USB/usb_function_generic.h"
#include "usb_config.h"
#include "user.h"                              // Modifiable
#include "eeprom.h"

/** CONFIGURATION **************************************************/
// Configuration bits for Open USB FXS board
#pragma config PLLDIV   = 5		// (20 MHz crystal)
#pragma config CPUDIV   = OSC1_PLL2   	// CPU system clock prescaler
#pragma config USBDIV   = 2		// Clock source from 96MHz PLL/2
#pragma config FOSC     = HSPLL_HS	// High-speed clock
#pragma config FCMEN    = OFF		// Fail-safe clock monitor
#pragma config IESO     = OFF		// Internal clock switchover
#pragma config PWRT     = OFF		// Power-up timer
#pragma config BOR      = SOFT		// Brown-out reset (s/w enabled)
#pragma config BORV     = 1		// Brown-out voltage (1V)
#pragma config VREGEN   = ON		// USB Voltage Regulator enabled
#pragma config WDT      = OFF		// Watchdog timer (disabled)
//#pragma config WDTPS    = 32768	// Watchdog prescaler (dontcare)
#pragma config MCLRE    = ON		// Master clear enabled
#pragma config LPT1OSC  = OFF		// Low-power timer1 osc disabled
#pragma config PBADEN   = OFF		// PortB<4:0> not analog inputs
//#pragma config CCP2MX   = ON
#pragma config STVREN   = ON
#pragma config LVP      = ON
//#pragma config ICPRT    = OFF		// Dedicated In-Circuit Dbg/Programming
#pragma config XINST    = OFF		// Extended Instruction Set
#pragma config CP0      = OFF		// Code protect 00800-01FFF off
#pragma config CP1      = OFF		// Code protect 02000-03FFF off
#pragma config CP2      = OFF		// Code protect 04000-05FFF off
#pragma config CP3      = OFF		// Code protect 06000-07FFF off
#pragma config CPB      = OFF		// Code protect boot off
#pragma config CPD      = OFF		// Code protect EEPROM data off
#pragma config WRT0     = OFF		// Table Write protect 00800-01FFF off
#pragma config WRT1     = OFF		// Table Write protect 02000-03FFF off
#pragma config WRT2     = OFF		// Table Write protect 04000-05FFF off
#pragma config WRT3     = OFF		// Table Write protect 06000-07FFF off
#pragma config WRTB     = OFF		// Table Write boot block protect off
#pragma config WRTC     = OFF		// Table Write config protect off 
#pragma config WRTD     = OFF		// Table Write EEPROM data off
#pragma config EBTR0    = OFF		// Table Read protect 00800-01FFF off
#pragma config EBTR1    = OFF		// Table Read protect 02000-03FFF off
#pragma config EBTR2    = OFF		// Table Read protect 04000-05FFF off
#pragma config EBTR3    = OFF		// Table Read protect 06000-07FFF off
#pragma config EBTRB    = OFF		// Table Read boot block protect off



/** VARIABLES ******************************************************/
#pragma udata
extern USB_HANDLE USBGenericOutHandle;
extern USB_HANDLE USBGenericInHandle;
extern DATA_PACKET INPacket;
extern DATA_PACKET OUTPacket;

//////////////// BEGIN contributed code by avarvit
// #pragma udata USB_VARS
extern BYTE OUTPCMData0[16];
extern BYTE OUTPCMData1[16];
extern BYTE IN_PCMData0[16];
extern BYTE IN_PCMData1[16];
// #pragma udata
USB_HANDLE USBIsoOutHandle = 0;
USB_HANDLE USBIsoIn_Handle = 0;
extern BYTE send_iso;
extern BYTE recv_iso;
//////////////// END contributed code by avarvit

/** PRIVATE PROTOTYPES *********************************************/
static void InitializeSystem(void);
void USBDeviceTasks(void);
void HighPriorityISRCode(void);
void LowPriorityISRCode(void);


/** VECTOR REMAPPING ***********************************************/
//On PIC18 devices, addresses 0x00, 0x08, and 0x18 are used for
//the reset, high priority interrupt, and low priority interrupt
//vectors.  However, the current Microchip USB bootloader 
//examples are intended to occupy addresses 0x00-0x7FF or
//0x00-0xFFF depending on which bootloader is used.  Therefore,
//the bootloader code remaps these vectors to new locations
//as indicated below.  This remapping is only necessary if you
//wish to program the hex file generated from this project with
//the USB bootloader.  If no bootloader is used, edit the
//usb_config.h file and comment out the following defines:
//#define PROGRAMMABLE_WITH_USB_HID_BOOTLOADER
//#define PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER


//////////////// BEGIN contributed code by avarvit
// avarvit note: I am keeping these in to service the RESET vector;
// other than that, the high-priority ISR remap vector address has
// been changed in the bootloader, so this remap vector is not used
// anymore; and, currently, the low-priority ISR is not being used
// anywhere. 

extern void tmr1_isr (void);
//////////////// END contributed code by avarvit

#if defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)	
	#define REMAPPED_RESET_VECTOR_ADDRESS		0x800
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x808
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x818
#else	
	#define REMAPPED_RESET_VECTOR_ADDRESS		0x00
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x08
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x18
#endif

#if defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)

extern void _startup (void);        // See c018i.c in your C18 compiler dir
#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
void _reset (void)
{
    // _asm goto _startup _endasm
    _asm goto chk_mode _endasm
}
#endif

#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
void Remapped_High_ISR (void)
{
     _asm goto HighPriorityISRCode _endasm
}

#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
void Remapped_Low_ISR (void)
{
     _asm goto LowPriorityISRCode _endasm
}

#if defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
//Note: If this project is built while one of the bootloaders has
//been defined, but then the output hex file is not programmed with
//the bootloader, addresses 0x08 and 0x18 would end up programmed with 0xFFFF.
//As a result, if an actual interrupt was enabled and occured, the PC would jump
//to 0x08 (or 0x18) and would begin executing "0xFFFF" (unprogrammed space).  This
//executes as nop instructions, but the PC would eventually reach the REMAPPED_RESET_VECTOR_ADDRESS
//(0x1000 or 0x800, depending upon bootloader), and would execute the "goto _startup".  This
//would effective reset the application.

//To fix this situation, we should always deliberately place a 
//"goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS" at address 0x08, and a
//"goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS" at address 0x18.  When the output
//hex file of this project is programmed with the bootloader, these sections do not
//get bootloaded (as they overlap the bootloader space).  If the output hex file is not
//programmed using the bootloader, then the below goto instructions do get programmed,
//and the hex file still works like normal.  The below section is only required to fix this
//scenario.
#pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR (void)
{
     _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
}
#pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR (void)
{
     _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
}
#endif	//end of "#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)"

#pragma code


//These are your actual interrupt handling routines.
#pragma interrupt HighPriorityISRCode
void HighPriorityISRCode()
{
     // this is not used; if it were used, however, it should jump to
     // the tmr1_isr function
     _asm goto tmr1_isr _endasm

	//Check which interrupt flag caused the interrupt.
	//Service the interrupt
	//Clear the interrupt flag
	//Etc.

}	//This return will be a "retfie fast", since this is in a #pragma interrupt section 
#pragma interruptlow LowPriorityISRCode
void LowPriorityISRCode()
{
	//Check which interrupt flag caused the interrupt.
	//Service the interrupt
	//Clear the interrupt flag
	//Etc.

}	//This return will be a "retfie", since this is in a #pragma interruptlow section 



/** DECLARATIONS ***************************************************/
#pragma code

/******************************************************************************
 * Function:        void main(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        Main program entry point.
 * Note:            None
 *******************************************************************/

#if defined(__18CXX)
void main(void)
#else
int main(void)
#endif
{   
    InitializeSystem();

    while(1) {
    // Check bus status and service USB interrupts.
        USBDeviceTasks();
		// Interrupt or polling method. If using polling, must call
		// this function periodically. This function will take care
		// of processing and responding to SETUP transactions 
		// (such as during the enumeration process when you first
		// plug in).  USB hosts require that USB devices should accept
		// and process SETUP packets in a timely fashion.  Therefore,
		// when using polling, this function should be called 
		// frequently (such as once about every 100 microseconds) at any
		// time that a SETUP packet might reasonably be expected to
		// be sent by the host to your device.  In most cases, the
		// USBDeviceTasks() function does not take very long to
		// execute (~50 instruction cycles) before it returns.
    				  

    // Application-specific tasks.
    // Application related code may be added here or in the ProcessIO() function

        ProcessIO();        
    }//end while
}//end main


/********************************************************************
 * Function:        static void InitializeSystem(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        InitializeSystem is a centralized initialization
 *                  routine. All required USB initialization routines
 *                  are called from here.
 *
 *                  User application initialization routine should
 *                  also be called from here.                  
 *
 * Note:            None
 *******************************************************************/
static void InitializeSystem(void)
{
    ADCON1 |= 0x0F;			// Default all pins to digital

// The USB specifications require that USB peripheral devices must never source
// current onto the Vbus pin.  Additionally, USB peripherals should not source
// current on D+ or D- when the host/hub is not actively powering the Vbus line.
// When designing a self powered (as opposed to bus powered) USB peripheral
// device, the firmware should make sure not to turn on the USB module and D+
// or D- pull up resistor unless Vbus is actively powered.  Therefore, the
// firmware needs some means to detect when Vbus is being powered by the host.
// A 5V tolerant I/O pin can be connected to Vbus (through a resistor), and
// can be used to detect when Vbus is high (host actively powering), or low
// (host is shut down or otherwise not supplying power).  The USB firmware
// can then periodically poll this I/O pin to know when it is okay to turn on
// the USB module/D+/D- pull up resistor.  When designing a purely bus powered
// peripheral device, it is not possible to source current on D+ or D- when the
// host is not actively providing power on Vbus. Therefore, implementing this
// bus sense feature is optional.  This firmware can be made to use this bus
// sense feature by making sure "USE_USB_BUS_SENSE_IO" has been defined in the
// HardwareProfile.h file.    
    #if defined(USE_USB_BUS_SENSE_IO)
    tris_usb_bus_sense = INPUT_PIN; // See HardwareProfile.h
    #endif
    
// If the host PC sends a GetStatus (device) request, the firmware must respond
// and let the host know if the USB peripheral device is currently bus powered
// or self powered. See chapter 9 in the official USB specifications for details
// regarding this request.  If the peripheral device is capable of being both
// self and bus powered, it should not return a hard coded value for this reqst.
// Instead, firmware should check if it is currently self or bus powered, and
// respond accordingly.  If the hardware has been configured like demonstrated
// on the PICDEM FS USB Demo Board, an I/O pin can be polled to determine the
// currently selected power source.  On the PICDEM FS USB Demo Board, "RA2" 
// is used for	this purpose.  If using this feature, make sure "USE_SELF_POWER_SENSE_IO"
// has been defined in HardwareProfile.h, and that an appropriate I/O pin has been mapped
// to it in HardwareProfile.h.
    #if defined(USE_SELF_POWER_SENSE_IO)
    tris_self_power = INPUT_PIN;	// See HardwareProfile.h
    #endif
    
    USBDeviceInit();	//usb_device.c. Initializes USB module SFRs and firmware
    					//variables to known states.
    UserInit();				// in user.c

}//end InitializeSystem

// *****************************************************************************
// ************** USB Callback Functions ***************************************
// *****************************************************************************
// The USB firmware stack will call the callback functions USBCBxxx()
// in response to certain USB related events.  For example, if the host
// PC is powering down, it will stop sending out Start of Frame (SOF)
// packets to your device.  In response to this, all USB devices are
// supposed to decrease their power consumption from the USB Vbus to
// < 2.5mA each.  The USB module detects this condition (which according
// to the USB specifications is 3+ms of no bus activity/SOF packets) and
// then calls the USBCBSuspend() function.  You should modify these
// callback functions to take appropriate actions for each of these
// conditions.  For example, in the USBCBSuspend(), you may wish to add
// code that will decrease power consumption from Vbus to < 2.5mA (such
// as by clock switching, turning off LEDs, putting the microcontroller
// to sleep, etc.).  Then, in the USBCBWakeFromSuspend() function, you
// may then wish to add code that undoes the power saving things done in
// the USBCBSuspend() function.

// The USBCBSendResume() function is special, in that the USB stack will
// not automatically call this function.  This function is meant to be
// called from the application firmware instead.  See the additional
// comments near the function.

/******************************************************************************
 * Function:        void USBCBSuspend(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        Call back that is invoked when a USB suspend is detected
 * Note:            None
 *****************************************************************************/
void USBCBSuspend(void)
{
//Example power saving code.  Insert appropriate code here for the desired
//application behavior.  If the microcontroller will be put to sleep, a
//process similar to that shown below may be used:
	
	//ConfigureIOPinsForLowPower();
	//SaveStateOfAllInterruptEnableBits();
	//DisableAllInterruptEnableBits();
	//EnableOnlyTheInterruptsWhichWillBeUsedToWakeTheMicro();	//should enable at least USBActivityIF as a wake source
	//Sleep();
	//RestoreStateOfAllPreviouslySavedInterruptEnableBits();	//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.
	//RestoreIOPinsToNormal();									//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.

//IMPORTANT NOTE: Do not clear the USBActivityIF (ACTVIF) bit here.  This bit is
//cleared inside the usb_device.c file.  Clearing USBActivityIF here will cause 
//things to not work as intended.	
	
	// avarvit: removed C30-specific code from here

	// OPENUSBFXS-TODO: I have to make sure that the board does not
	// draw current in this state; a strategy might be to put the PIC
	// to sleep.
}


/******************************************************************************
 * Function:        void _USB1Interrupt(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        This function is called when the USB interrupt bit is set
 *		    In this example the interrupt is only used when the device
 *		    goes to sleep when it receives a USB suspend command
 * Note:            None
 *****************************************************************************/
#if 0
void __attribute__ ((interrupt)) _USB1Interrupt(void)
{
    #if !defined(self_powered)
        if(U1OTGIRbits.ACTVIF)
        {
            LATAbits.LATA7 = 1;
        
            IEC5bits.USB1IE = 0;
            U1OTGIEbits.ACTVIE = 0;
            IFS5bits.USB1IF = 0;
        
            //USBClearInterruptFlag(USBActivityIFReg,USBActivityIFBitNum);
            USBClearInterruptFlag(USBIdleIFReg,USBIdleIFBitNum);
            //USBSuspendControl = 0;
            LATAbits.LATA7 = 0;
        }
    #endif
}
#endif

/******************************************************************************
 * Function:        void USBCBWakeFromSuspend(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The host may put USB peripheral devices in low power
 *		    suspend mode (by "sending" 3+ms of idle).  Once in suspend
 *		    mode, the host may wake the device back up by sending non-
 *		    idle state signalling.
 *		    This call back is invoked when a wakeup from USB suspend 
 *		    is detected.
 * Note:            None
 *****************************************************************************/
void USBCBWakeFromSuspend(void)
{
	// If clock switching or other power savings measures were taken when
	// executing the USBCBSuspend() function, now would be a good time to
	// switch back to normal full power run mode conditions. The host allows
	// a few milliseconds of wakeup time, after which the device must be 
	// fully back to normal, and capable of receiving and processing USB
	// packets.  In order to do this, the USB module must receive proper
	// clocking (IE: 48MHz clock must be available to SIE for full speed USB
	// operation).

	// OPENUSBFXS-TODO: probably it is a good idea to reset everything
	// after a suspend
}

/********************************************************************
 * Function:        void USBCB_SOF_Handler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USB host sends out a SOF packet to full-speed
 *                  devices every 1 ms. This interrupt may be useful
 *                  for isochronous pipes. End designers should
 *                  implement callback routine as necessary.
 * Note:            None
 *******************************************************************/
void USBCB_SOF_Handler(void)
{
//////////////// BEGIN contributed code by avarvit

// Note: with the current defines, all code below is ifdef'ed out.
// I have kept the code around handy just in case I need to compile
// anything in again. In practice, this will not be the case, since
// SOF handling has been moved from this function to the TMR1 ISR
// code (see "tmr1_isr.asm"), where it occurs synchronously to the
// PCM audio I/O and other timed signals to the 3210


#if (USB_MAX_EP_NUMBER>=2)

# if 0 // first try
    // send to host
    if (send_iso && !USBHandleBusy(USBIsoIn_Handle)) {
	USBIsoIn_Handle = USBGenWrite (2, IN_PCMData0, 16);
	// send_iso--;
	IN_PCMData0[7]++;
	if (recv_iso && !USBHandleBusy(USBIsoOutHandle)) {
	    // receive isochronous data
	    USBIsoOutHandle = USBGenRead (2, OUTPCMData0, 16);
	    // recv_iso--;
	}
    }
#endif

#if 0	//second try
    USBIsoIn_Handle = USBGenWrite (2, IN_PCMData0, 16);
    IN_PCMData0[7]++;

    if (recv_iso && !USBHandleBusy(USBIsoOutHandle)) {
	// receive isochronous data
	USBIsoOutHandle = USBGenRead (2, OUTPCMData0, 16);
	// recv_iso--;
    }
#endif
    extern volatile BDT_ENTRY BDT[];
    
    #if (USB_PING_PONG_MODE == USB_PING_PONG__NO_PING_PONG)

    // a code optimization example (assumes NO_PING_PONG)
    IN_PCMData0[7]++;
    BDT[EP(2,IN_TO_HOST,0)].ADR = IN_PCMData0;
    BDT[EP(2,IN_TO_HOST,0)].CNT = 16;
    BDT[EP(2,IN_TO_HOST,0)].STAT.Val &= _DTSMASK;
    BDT[EP(2,IN_TO_HOST,0)].STAT.Val |= _USIE|_DTSEN;

    #elif (USB_PING_PONG_MODE == USB_PING_PONG__FULL_PING_PONG)
    #if 0

    // a code optimization example (assumes FULL_PING_PONG)
    static BYTE turn = 0;

    if (turn) {
    	// odd transaction
    	// IN_PCMData1[7] = IN_PCMData0[7] + 1;
	BDT[EP(2,IN_TO_HOST,1)].ADR = IN_PCMData1;
	BDT[EP(2,IN_TO_HOST,1)].CNT = 16;
	BDT[EP(2,IN_TO_HOST,1)].STAT.Val &= _DTSMASK;
	BDT[EP(2,IN_TO_HOST,1)].STAT.Val |= _USIE|_DTSEN;

	#if 0
	if (BDT[EP(2,OUT_FROM_HOST,1)].STAT.UOWN == 0) {
	    BDT[EP(2,OUT_FROM_HOST,1)].ADR = OUTPCMData1;
	    BDT[EP(2,OUT_FROM_HOST,1)].CNT = 16;
	    BDT[EP(2,OUT_FROM_HOST,1)].STAT.Val &= _DTSMASK;
	    BDT[EP(2,OUT_FROM_HOST,1)].STAT.Val |= _USIE|_DTSEN;
	}
	#endif

	#if 0
	BDT[EP(3,IN_TO_HOST,1)].ADR = IN_PCMData1;
	BDT[EP(3,IN_TO_HOST,1)].CNT = 16;
	BDT[EP(3,IN_TO_HOST,1)].STAT.Val &= _DTSMASK;
	BDT[EP(3,IN_TO_HOST,1)].STAT.Val |= _USIE|_DTSEN;
	#endif
    }
    else {
	// even transaction
	// IN_PCMData0[7] = IN_PCMData1[7] + 1;
	BDT[EP(2,IN_TO_HOST,0)].ADR = IN_PCMData0;
	BDT[EP(2,IN_TO_HOST,0)].CNT = 16;
	BDT[EP(2,IN_TO_HOST,0)].STAT.Val &= _DTSMASK;
	BDT[EP(2,IN_TO_HOST,0)].STAT.Val |= _USIE|_DTSEN;

	#if 0
	if (BDT[EP(2,OUT_FROM_HOST,0)].STAT.UOWN == 0) {
	    BDT[EP(2,OUT_FROM_HOST,0)].ADR = OUTPCMData1;
	    BDT[EP(2,OUT_FROM_HOST,0)].CNT = 16;
	    BDT[EP(2,OUT_FROM_HOST,0)].STAT.Val &= _DTSMASK;
	    BDT[EP(2,OUT_FROM_HOST,0)].STAT.Val |= _USIE|_DTSEN;
	}
	#endif

	#if 0
	BDT[EP(3,IN_TO_HOST,0)].ADR = IN_PCMData0;
	BDT[EP(3,IN_TO_HOST,0)].CNT = 16;
	BDT[EP(3,IN_TO_HOST,0)].STAT.Val &= _DTSMASK;
	BDT[EP(3,IN_TO_HOST,0)].STAT.Val |= _USIE|_DTSEN;
	#endif
    }
    turn ^= 1;
    #endif

    #else
    #error "Only implemented for USB_PING_PONG_MODE set to either USB_PING_PONG__NO_PING_PONG or USB_PING_PONG__FULL_PING_PONG"
    #endif


#endif // (USB_MAX_EP_NUMBER>=2)


    // No need to clear UIRbits.SOFIF to 0 here.
    // Callback caller is already doing that.

    // avarvit: note that caller doesn't clear the flag anymore;
    // check "usb_device.c.patch" for details
//////////////// END contributed code by avarvit
}

/*******************************************************************
 * Function:        void USBCBErrorHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The purpose of this callback is mainly for
 *                  debugging during development. Check UEIR to see
 *                  which error causes the interrupt.
 * Note:            None
 *******************************************************************/
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.

	// Typically, user firmware does not need to do anything special
	// if a USB error occurs.  For example, if the host sends an OUT
	// packet to your device, but the packet gets corrupted (ex:
	// because of a bad connection, or the user unplugs the
	// USB cable during the transmission) this will typically set
	// one or more USB error interrupt flags.  Nothing specific
	// needs to be done however, since the SIE will automatically
	// send a "NAK" packet to the host.  In response to this, the
	// host will normally retry to send the packet again, and no
	// data loss occurs.  The system will typically recover
	// automatically, without the need for application firmware
	// intervention.
	
	// Nevertheless, this callback function is provided, such as
	// for debugging purposes.
}


/*******************************************************************
 * Function:        void USBCBCheckOtherReq(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        When SETUP packets arrive from the host, some
 * 		    firmware must process the request and respond
 *		    appropriately to fulfill the request.  Some of
 *		    the SETUP packets will be for standard
 *		    USB "chapter 9" (as in, fulfilling chapter 9 of
 *		    the official USB specifications) requests, while
 *		    others may be specific to the USB device class
 *		    that is being implemented.  For example, a HID
 *		    class device needs to be able to respond to
 *		    "GET REPORT" type of requests.  This
 *		    is not a standard USB chapter 9 request, and 
 *		    therefore not handled by usb_device.c.  Instead
 *		    this request should be handled by class specific 
 *		    firmware, such as that contained in usb_function_hid.c.
 * Note:            None
 *****************************************************************************/
void USBCBCheckOtherReq(void)
{
}//end


/*******************************************************************
 * Function:        void USBCBStdSetDscHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USBCBStdSetDscHandler() callback function is
 *		    called when a SETUP, bRequest: SET_DESCRIPTOR request
 *		    arrives.  Typically SET_DESCRIPTOR requests are
 *		    not used in most applications, and it is
 *		    optional to support this type of request.
 * Note:            None
 *****************************************************************************/
void USBCBStdSetDscHandler(void)
{
    // Must claim session ownership if supporting this request
}//end


/******************************************************************************
 * Function:        void USBCBInitEP(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        This function is called when the device becomes
 *                  initialized, which occurs after the host sends a
 * 		    SET_CONFIGURATION (wValue not = 0) request.  This 
 *		    callback function should initialize the endpoints 
 *		    for the device's usage according to the current 
 *		    configuration.
 * Note:            None
 *****************************************************************************/
void USBCBInitEP(void)
{
    USBEnableEndpoint(USBGEN_EP_NUM,USB_OUT_ENABLED|USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    // avarvit: this read "arms" the receiver so that it becomes
    // ready to receive any frame that the USB host sends to the EP
    USBGenericOutHandle = USBGenRead(USBGEN_EP_NUM,(BYTE*)&OUTPacket,USBGEN_EP_SIZE);

#if (USB_MAX_EP_NUMBER>=2) // avarvit
    //test
    //USBEnableEndpoint(2,USB_OUT_ENABLED|USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    USBEnableEndpoint(2,USB_OUT_ENABLED|USB_IN_ENABLED|USB_HANDSHAKE_DISABLED|USB_DISALLOW_SETUP);
    // avarvit: see note above about "arming" a read
    USBIsoOutHandle = USBGenRead(2,OUTPCMData0,16);
#endif // (USB_MAX_EP_NUMBER==2)
}

/********************************************************************
 * Function:        void USBCBSendResume(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USB specifications allow some types of USB
 * 		    peripheral devices to wake up a host PC (such
 *		    as if it is in a low power suspend to RAM state).
 *		    This can be a very useful feature in some
 *		    USB applications, such as an Infrared remote
 *		    control receiver.  If a user presses the "power"
 *		    button on a remote control, it is nice that the
 *		    IR receiver can detect this signalling, and then
 *		    send a USB "command" to the PC to wake up.
 *		    The USBCBSendResume() "callback" function is used
 *		    to send this special USB signalling which wakes 
 *		    up the PC.  This function may be called by
 *		    application firmware to wake up the PC.  This
 *		    function should only be called when:
 *		    1.  The USB driver used on the host PC supports
 *		        the remote wakeup capability.
 *		    2.  The USB configuration descriptor indicates
 *		        the device is remote wakeup capable in the
 *		        bmAttributes field.
 *		    3.  The USB host PC is currently sleeping,
 *		        and has previously sent your device a SET 
 *		        FEATURE setup packet which "armed" the
 *		        remote wakeup capability.   
 *		    This callback should send a RESUME signal that
 *                  has the period of 1-15ms.
 * Note:            Interrupt vs. Polling
 *                  -Primary clock
 *                  -Secondary clock ***** MAKE NOTES ABOUT THIS *******
 *                  Can switch to primary first by calling
 *		    USBCBWakeFromSuspend()
 *                  The modifiable section in this routine should be changed
 *                  to meet the application needs. Current implementation
 *                  temporary blocks other functions from executing for a
 *                  period of 1-13 ms depending on the core frequency.
 *                  According to USB 2.0 specification section 7.1.7.7,
 *                  "The remote wakeup device must hold the resume signaling
 *                  for at lest 1 ms but for no more than 15 ms."
 *                  The idea here is to use a delay counter loop, using a
 *                  common value that would work over a wide range of core
 *                  frequencies.
 *                  That value selected is 1800. See table below:
 *                  ==========================================================
 *                  Core Freq(MHz)      MIP         RESUME Signal Period (ms)
 *                  ==========================================================
 *                      48              12          1.05
 *                       4              1           12.6
 *                  ==========================================================
 *                  * These timing could be incorrect when using code
 *                    optimization or extended instruction mode,
 *                    or when having other interrupts enabled.
 *                    Make sure to verify using the MPLAB SIM's Stopwatch
 *                    and verify the actual signal on an oscilloscope.
 *******************************************************************/
void USBCBSendResume(void)
{
    static WORD delay_count;
    
    USBResumeControl = 1;                // Start RESUME signaling
    
    delay_count = 1800U;                // Set RESUME line for 1-13 ms
    do {
        delay_count--;
    } while(delay_count);
    USBResumeControl = 0;
}

/** EOF main.c ***************************************************************/
