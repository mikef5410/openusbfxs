/*******************************************************************
 FileName:	proslic.c
 Dependencies:	See INCLUDES section below
 Processor:	PIC18
 Compiler:	Microchip C18
 Copyright:	(C) Angelos Varvitsiotis 2009
 License:	GPLv3

 *******************************************************************/

/** I N C L U D E S **********************************************************/
#include "Compiler.h"
#include "GenericTypeDefs.h"
#include <spi.h>
#include "HardwareProfile.h"
#include "proslic.h"

/** V A R I A B L E S ********************************************************/
#pragma udata

/** P R I V A T E  P R O T O T Y P E S ***************************************/

BOOL IsValidPROSLICDirectRegister(BYTE);
void ProSLICSPIDelay (void);
void ProSLICWriteByte (BYTE);
BYTE ProSLICReadByte (void);
int  WaitForIndirectAccess (void);

/** D E C L A R A T I O N S **************************************************/

#pragma code
BOOL IsValidPROSLICDirectRegister (BYTE b) {
    switch (b) {
      case 07:
      case 12:
      case 13:
      case 16:
      case 17:
      case 25:
      case 26:
      case 27:
      case 28:
      case 29:
      case 30:
      case 31:
      case 53:
      case 54:
      case 55:
      case 56:
      case 57:
      case 58:
      case 59:
      case 60:
      case 61:
      case 62:
      case 90:
      case 91:
	return FALSE;
      default:
	if (b > 108) return FALSE;
	return TRUE;
    }
}

void ProSLICSPIDelay (void) {
    int SPI_delay = 10U;
    while (SPI_delay--);
}

void ProSLICWriteByte (BYTE b) {
    _cs_3210 = 0;		// selected
    WriteSPI (b);
    _cs_3210 = 1;		// deselected
}

BYTE ProSLICReadByte (void) {
    BYTE ret;

    _cs_3210 = 0;		// selected
    ret = ReadSPI ();
    _cs_3210 = 1;		// deselected
    return ret;
}

void WriteProSLICDirectRegister (BYTE reg, BYTE val) {

    ProSLICWriteByte (reg);
    ProSLICSPIDelay ();
    ProSLICWriteByte (val);
}

BYTE ReadProSLICDirectRegister (BYTE reg) {
    ProSLICWriteByte (0x80 | reg);
    ProSLICSPIDelay ();
    return (ProSLICReadByte ());
}

int WaitForIndirectAccess (void) {
    int i;
    for (i = 0; i < 1000; i++) {
        if (ReadProSLICDirectRegister (31) == 0) // 0x01 if indir I/O is pending
	    return 0;
    }
    return 1;
}

WORD ReadProSLICIndirectRegister (BYTE reg) {
    WORD ret = 0;

    if (WaitForIndirectAccess ()) {
        return 0xDead;
    }
    WriteProSLICDirectRegister (30, reg);
    if (WaitForIndirectAccess ()) {
        return 0xBeef;
    }
    ret = ReadProSLICDirectRegister (29);
    ret <<= 8;
    ret |= ReadProSLICDirectRegister (28);
    return ret;
}

void WriteProSLICIndirectRegister (BYTE reg, WORD val) {
    if (WaitForIndirectAccess ()) {
        return;
    }
    WriteProSLICDirectRegister (28, (BYTE) (val & 0xff));
    WriteProSLICDirectRegister (29, (BYTE) ((val & 0xff00) >> 8));
    WriteProSLICDirectRegister (30, reg);
}


/** EOF proslic.c *************************************************************/
