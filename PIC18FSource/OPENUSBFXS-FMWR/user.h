/********************************************************************
 FileName:	user.h
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

#ifndef USER_H
#define USER_H

/** I N C L U D E S **********************************************************/
#include "GenericTypeDefs.h"
#include "usb_config.h"

//////////////// BEGIN contributed code by avarvit
// avarvit: haven't found these anywhere else; are they needed?
//extern volatile unsigned char usbgen_out[USBGEN_EP_SIZE];
//extern volatile unsigned char usbgen_in[USBGEN_EP_SIZE];
//////////////// END contributed code by avarvit

/** D E F I N I T I O N S ****************************************************/
/* PICDEM FS USB Demo Version */
#define MINOR_VERSION   0x00    //Demo Version 1.00
#define MAJOR_VERSION   0x01

#define FXS_MAJOR_VERSION 0x01	// current FXS version is 1.46.01
#define FXS_MINOR_VERSION 0x2E	//
#define FXS_REVISION_NMBR 0x0F
 
//////////////// BEGIN contributed code by avarvit
// /* Temperature Mode */
// #define TEMP_REAL_TIME  0x00
// #define TEMP_LOGGING    0x01
//////////////// END contributed code by avarvit

typedef enum
{
    READ_VERSION    = 0x00,	// return current firmware version
//////////////// BEGIN contributed code by avarvit
    // avarvit: I have commented these out, they are not implemented on
    // my Open USB FXS board
    /*
    READ_FLASH      = 0x01,
    WRITE_FLASH     = 0x02,
    ERASE_FLASH     = 0x03,
    READ_EEDATA     = 0x04,
    WRITE_EEDATA    = 0x05,
    READ_CONFIG     = 0x06,
    WRITE_CONFIG    = 0x07,
    */

    // various debug commands
    DEBUG_GET_CNT48 = 0x40,
    DEBUG_GET_PSOUT = 0x41,
    DEBUG_GET_PSWRD = 0x42,

    /* not yet inplemented
    DEBUG_RST_PSOUT = 0x43,
    DEBUG_RST_PSWRD = 0x44,
    */

    // return svn revision (as a null-terminated string) 
    GET_FXS_VERSION = 0x60,
    WRITE_SERIAL_NO = 0x61,
    REBOOT_BOOTLOAD = 0x62,

    // set isochronous IO on/off
    START_STOP_ISOV2= 0x7d,	// v2 adds a sequence number for setting DRs
    START_STOP_ISO  = 0x7e,
    // perform SOF calibration
    SOF_PROFILE     = 0x7f,

    // various ProSLIC control commands
    PROSLIC_SCURRENT= 0x80,	// set current index for register enumeration
    PROSLIC_RCURRENT= 0x81,	// read current direct register
    PROSLIC_RDIRECT = 0x82,	// read indicated direct register
    PROSLIC_WDIRECT = 0x83,	// write indicated direct register
    PROSLIC_RDINDIR = 0x84,	// read indicated indirect register
    PROSLIC_WRINDIR = 0x85,	// write indicated indirect register
    PROSLIC_RESET   = 0x8F,	// reset the proslic
//////////////// END contributed code by avarvit


    RESET           = 0xFF	// reboot the board
}TYPE_CMD;

/** S T R U C T U R E S ******************************************************/
typedef union DATA_PACKET
{
    BYTE _byte[USBGEN_EP_SIZE];  //For byte access
    WORD _word[USBGEN_EP_SIZE/2];//For word access(USBGEN_EP_SIZE must be even)
    struct
    {
        BYTE CMD;
        BYTE len;
    };
    struct
    {
        unsigned :8;
        BYTE ID;
    };
    struct
    {
        unsigned :8;
        BYTE led_num;
        BYTE led_status;
    };
    struct
    {
        unsigned :8;
        WORD word_data;
    };
} DATA_PACKET;

/** P U B L I C  P R O T O T Y P E S *****************************************/
void UserInit(void);
void ProcessIO(void);

#endif //USER_H
