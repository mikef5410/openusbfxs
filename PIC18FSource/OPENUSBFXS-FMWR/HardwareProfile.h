/********************************************************************
 FileName:	HardwareProfile.h
 Dependencies:	None
 Processor:	PIC18
 Hardware:	The code is intended to be used on the Open USB FXS board
 Compiler:  	Microchip C18
 Copyright:	(C) Angelos Varvitsiotis 2009
 License:	GPLv3 (copyrighted part)

 *******************************************************************/

#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

/** TRIS ***********************************************************/
#define INPUT_PIN           1
#define OUTPUT_PIN          0

/** USB ************************************************************/


// Open USB FXS is programmable with a MCHPUSB(-compatible) bootloader
#define PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER

// on Open USB FXS, USB_BUS_SENSE is always true
//#define USB_BUS_SENSE PORTAbits.RA1
#define USB_BUS_SENSE	1

// on Open USB FXS, self_power is always false (board is USB-powered)
//#define self_power PORTAbits.RA2
#define self_power	0

    
/** LEDs ************************************************************/

// on the Open USB FXS board, the only existing LED is on A5 (Port A, bit 5)
#define mInitAllLEDs()	{LATA &= 0xDF; TRISA &= 0xDF;}
#define mLED_1		LATAbits.LATA5
#define mLED_1_On()	(mLED_1 = 1)
#define mLED_1_Off()	(mLED_1 = 0)
#define mLED_1_Toggle()	(mLED_1 = !mLED_1)

// TODO: these are NOPs; remove when no longer needed
#define mLED_2_On()
#define mLED_2_Off()
#define mLED_2_Toggle()


/** SWITCH *********************************************************/

// on the Open USB FXS board, the user switch is on C6 (Port C, bit 6)
#define mInitAllSwitches()	TRISCbits.TRISC6=1
#define sw2			PORTCbits.RC6

/** 3210 SPI Chip Select, Reset and PCM bus Lines ******************/

// 3210 \chip select, \reset and \int lines
#define tris__cs_3210		TRISBbits.TRISB3	// output pin, set to 0
#define tris__reset_3210	TRISAbits.TRISA0	// output pin, set to 0
#define tris__int_3210		TRISCbits.TRISC2	// input pin, set to 1

// PCM bus lines (PCLK, DRX, DTX, FSYNC)
#define tris_pcm_3210_pclk	TRISAbits.TRISA4	// output pin, set to 0
#define tris_pcm_3210_drx	TRISAbits.TRISA3	// output pin(!), setto0
#define tris_pcm_3210_dtx	TRISAbits.TRISA2	// input pin(!), setto 1
#define	tris_pcm_3210_fsync	TRISAbits.TRISA1	// output pin, set to 0

// define handy signal names here

#define _reset_3210		LATAbits.LATA0
#define	_cs_3210		LATBbits.LATB3
#define	_int_3210		PORTCbits.RC2
#define pcm_3210_pclk		LATAbits.LATA4
#define pcm_3210_drx		LATAbits.LATA3
#define pcm_3210_dtx		PORTAbits.RA2
#define pcm_3210_fsync		LATAbits.LATA1

#endif  //HARDWARE_PROFILE_H
