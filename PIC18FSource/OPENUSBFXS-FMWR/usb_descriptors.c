/********************************************************************
 FileName:	usb_descriptors.c
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
 $Id$

*********************************************************************
-usb_descriptors.c-
-------------------------------------------------------------------
Filling in the descriptor values in the usb_descriptors.c file:
-------------------------------------------------------------------

[Device Descriptors]
The device descriptor is defined as a USB_DEVICE_DESCRIPTOR type.  
This type is defined in usb_ch9.h  Each entry into this structure
needs to be the correct length for the data type of the entry.

[Configuration Descriptors]
The configuration descriptor was changed in v2.x from a structure
to a BYTE array.  Given that the configuration is now a byte array
each byte of multi-byte fields must be listed individually.  This
means that for fields like the total size of the configuration where
the field is a 16-bit value "64,0," is the correct entry for a
configuration that is only 64 bytes long and not "64," which is one
too few bytes.

The configuration attribute must always have the _DEFAULT
definition at the minimum. Additional options can be ORed
to the _DEFAULT attribute. Available options are _SELF and _RWU.
These definitions are defined in the usb_device.h file. The
_SELF tells the USB host that this device is self-powered. The
_RWU tells the USB host that this device supports Remote Wakeup.

[Endpoint Descriptors]
Like the configuration descriptor, the endpoint descriptors were 
changed in v2.x of the stack from a structure to a BYTE array.  As
endpoint descriptors also has a field that are multi-byte entities,
please be sure to specify both bytes of the field.  For example, for
the endpoint size an endpoint that is 64 bytes needs to have the size
defined as "64,0," instead of "64,"

Take the following example:
    // Endpoint Descriptor //
    0x07,                       //the size of this descriptor //
    USB_DESCRIPTOR_ENDPOINT,    //Endpoint Descriptor
    _EP02_IN,                   //EndpointAddress
    _INT,                       //Attributes
    0x08,0x00,                  //size (note: 2 bytes)
    0x02,                       //Interval

The first two parameters are self-explanatory. They specify the
length of this endpoint descriptor (7) and the descriptor type.
The next parameter identifies the endpoint, the definitions are
defined in usb_device.h and has the following naming
convention:
_EP<##>_<dir>
where ## is the endpoint number and dir is the direction of
transfer. The dir has the value of either 'OUT' or 'IN'.
The next parameter identifies the type of the endpoint. Available
options are _BULK, _INT, _ISO, and _CTRL. The _CTRL is not
typically used because the default control transfer endpoint is
not defined in the USB descriptors. When _ISO option is used,
addition options can be ORed to _ISO. Example:
_ISO|_AD|_FE
This describes the endpoint as an isochronous pipe with adaptive
and feedback attributes. See usb_device.h and the USB
specification for details. The next parameter defines the size of
the endpoint. The last parameter in the polling interval.

-------------------------------------------------------------------
Adding a USB String
-------------------------------------------------------------------
A string descriptor array should have the following format:

rom struct{byte bLength;byte bDscType;word string[size];}sdxxx={
sizeof(sdxxx),DSC_STR,<text>};

The above structure provides a means for the C compiler to
calculate the length of string descriptor sdxxx, where xxx is the
index number. The first two bytes of the descriptor are descriptor
length and type. The rest <text> are string texts which must be
in the unicode format. The unicode format is achieved by declaring
each character as a word type. The whole text string is declared
as a word array with the number of characters equals to <size>.
<size> has to be manually counted and entered into the array
declaration. Let's study this through an example:
if the string is "USB" , then the string descriptor should be:
(Using index 02)
rom struct{byte bLength;byte bDscType;word string[3];}sd002={
sizeof(sd002),DSC_STR,'U','S','B'};

A USB project may have multiple strings and the firmware supports
the management of multiple strings through a look-up table.
The look-up table is defined as:
rom const unsigned char *rom USB_SD_Ptr[]={&sd000,&sd001,&sd002};

The above declaration has 3 strings, sd000, sd001, and sd002.
Strings can be removed or added. sd000 is a specialized string
descriptor. It defines the language code, usually this is
US English (0x0409). The index of the string must match the index
position of the USB_SD_Ptr array, &sd000 must be in position
USB_SD_Ptr[0], &sd001 must be in position USB_SD_Ptr[1] and so on.
The look-up table USB_SD_Ptr is used by the get string handler
function.

-------------------------------------------------------------------

The look-up table scheme also applies to the configuration
descriptor. A USB device may have multiple configuration
descriptors, i.e. CFG01, CFG02, etc. To add a configuration
descriptor, user must implement a structure similar to CFG01.
The next step is to add the configuration descriptor name, i.e.
cfg01, cfg02,.., to the look-up table USB_CD_Ptr. USB_CD_Ptr[0]
is a dummy place holder since configuration 0 is the un-configured
state according to the definition in the USB specification.

********************************************************************/
 
/*********************************************************************
 * Descriptor specific type definitions are defined in:
 * usb_device.h
 *
 * Configuration options are defined in:
 * usb_config.h
 ********************************************************************/
#ifndef __USB_DESCRIPTORS_C
#define __USB_DESCRIPTORS_C

/** INCLUDES *******************************************************/
#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "USB/usb_device.h"


/** CONSTANTS ******************************************************/
#if defined(__18CXX)
#pragma romdata
#endif

/* Device Descriptor */
ROM USB_DEVICE_DESCRIPTOR device_dsc=
{
    0x12,                   // Size of this descriptor in bytes
    USB_DESCRIPTOR_DEVICE,  // DEVICE descriptor type
    0x0200,                 // USB Spec Release Number in BCD format
    0x00,                   // Class Code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    USB_EP0_BUFF_SIZE,      // Max packet size for EP0, see usb_config.h
//////////////// BEGIN contributed code by avarvit
    // note: for the time being, I have left my board with the same vendor
    // and product ids as Microchip's PICDEM FS board (that board has two
    // modes, bootloader and demo mode). There is no reason for this (other
    // than lazyness) and this will be definitely corrected in the future
    0x04D8,                 // Vendor ID (Microchip)
    0xFCF1,                 // Product ID (OpenUSBFXS, sublicensed by Microchip)
    // If in doubt, you may change this the PID with 0x000C below and
    // see if the PICDEM tool recognizes the board
    // 0x000C,                 // Product ID: PICDEM FS USB (DEMO Mode)
//////////////// END contributed code by avarvit
    0x0001,                 // Device release number in BCD format
    0x01,                   // Manufacturer string index
    0x02,                   // Product string index
    0x03,                   // Device serial number string index
    0x01                    // Number of possible configurations
};

/* Configuration 1 Descriptor */
ROM BYTE configDescriptor1[]={
    /* Configuration Descriptor */
    0x09,//sizeof(USB_CFG_DSC),    // Size of this descriptor in bytes
    USB_DESCRIPTOR_CONFIGURATION,  // CONFIGURATION descriptor type
//////////////// BEGIN contributed code by avarvit
#if (USB_MAX_EP_NUMBER==3)   // also include an INT EP
    0x35,0x00,              // Total length of data for this cfg
#elif (USB_MAX_EP_NUMBER==2) // this is the case with current defines
    0x2E,0x00,              // Total length of data for this cfg
#else                       // by default, USB_MAX_EP_NUMBER is 1
    0x20,0x00,              // Total length of data for this cfg
#endif	// USB_MAX_EP_NUMBER
//////////////// END contributed code by avarvit
    1,                      // Number of interfaces in this cfg
    1,                      // Index value of this configuration
    0,                      // Configuration string index
    _DEFAULT,               // Attributes, see usb_device.h
//////////////// BEGIN contributed code by avarvit
    250,                    // Max power consumption (2X mA)->500mA (OpenUSBFXS)
//////////////// END contributed code by avarvit
							
    /* Interface Descriptor */
    0x09,//sizeof(USB_INTF_DSC),   // Size of this descriptor in bytes
    USB_DESCRIPTOR_INTERFACE,      // INTERFACE descriptor type
    0,                      // Interface Number
    0,                      // Alternate Setting Number
#if (USB_MAX_EP_NUMBER==3)
    5,                      // Number of endpoints in this intf
#elif (USB_MAX_EP_NUMBER==2)
    4,                      // Number of endpoints in this intf
#else	// by default, USB_MAX_EP_NUMBER is 1
    2,
#endif	// USB_MAX_EP_NUMBER
    0x00,                   // Class code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    0,                      // Interface string index
    
    /* Endpoint Descriptor */
    0x07,                   /*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,//Endpoint Descriptor
    _EP01_OUT,              //EndpointAddress
    _BULK,                  //Attributes
    USBGEN_EP_SIZE,0x00,    //size
    1,                      //Interval
    
    0x07,                   /*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,//Endpoint Descriptor
    _EP01_IN,               //EndpointAddress
    _BULK,                  //Attributes
    USBGEN_EP_SIZE,0x00,    //size
    1,                      //Interval

//////////////// BEGIN contributed code by avarvit
/*
  This section adds two more isochronous endpoints (one IN, one OUT). Note
  that formally, the definition of these endpoints, as it is, violates the
  USB standard in the following aspect. According to the standard, devices
  that have isochronous endpoints must not specify the actual endpoint
  packet size in the default configuration. Instead, they must specify a
  zero packet size in the default config (to let the host know about the
  isochronous endpoints' existence) and provide one or more alternative
  configurations with various packet sizes, so that the host can choose
  one of these as it suits the host's needs.  At this stage, this would
  complicate unnecessarily things, so I have chosen on purpose to be
  non-complying, especially since packet size 16 is the only one that will
  be able to work for the time.
*/
#if (USB_MAX_EP_NUMBER>=2)
    0x07,                   /*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,//Endpoint Descriptor
    _EP02_OUT,              //EndpointAddress
    _ISO|_SY|_DE,           //Attributes
    // according to spec, isochronous endpoints must not specify a non-zero
    // payload size?? Otherwise, set to 0x08,0x00
    0x10,0x00,		    //size, 16 bytes
    1,                      //Interval: for ISO, this is 2^(1-1)*1ms = 1ms

    0x07,                   /*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,//Endpoint Descriptor
    _EP02_IN,               //EndpointAddress
    _ISO|_SY|_DE,           //Attributes
    // see note above about endpoint payload size
    0x10,0x00,              //size, 16 bytes
    1,                      //Interval: for ISO, this is 2^(1-1)*1ms = 1ms
#endif	// USB_MAX_EP_NUMBER

#if (USB_MAX_EP_NUMBER==3)
    0x07,                  /*sizeof(USB_EP_DSC)*/
    USB_DESCRIPTOR_ENDPOINT,//Endpont Descriptor
    _EP03_IN,              //EndpointAddress
    _INT,                  //Attributes
    0x10,0x00,             //size, 16 bytes
    1,                     //Interval
#endif
//////////////// END contributed code by avarvit
};


//Language code string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[1];}sd000={
sizeof(sd000),USB_DESCRIPTOR_STRING,{0x0409}};

//Manufacturer string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[20];}sd001={
sizeof(sd001),USB_DESCRIPTOR_STRING,{
'A','n','g','e','l','o','s',' ','V','a','r','v','i','t','s','i','o','t','i','s'
}};

//Product string descriptor
ROM struct{BYTE bLength;BYTE bDscType;WORD string[18];}sd002={
sizeof(sd002),USB_DESCRIPTOR_STRING,{
'O','p','e','n',' ','U','S','B',' ','F','X','S',' ','b','o','a','r','d'
}};

//Product serial number; actual serial is from eeprom, see "usb_device.c"
struct{BYTE bLength;BYTE bDscType;WORD string[8];}sd003={
sizeof(sd003),USB_DESCRIPTOR_STRING,{
'd','e','a','d','b','e','e','f'
}};

//Array of configuration descriptors
ROM BYTE *ROM USB_CD_Ptr[]=
{
    (ROM BYTE *ROM)&configDescriptor1
};
//Array of string descriptors
ROM BYTE *ROM USB_SD_Ptr[]=
{
    (ROM BYTE *ROM)&sd000,
    (ROM BYTE *ROM)&sd001,
    (ROM BYTE *ROM)&sd002,
    (ROM BYTE *)&sd003
};

/** EOF usb_descriptors.c ***************************************************/

#endif
