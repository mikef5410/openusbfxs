/* these functions were adapted from an example found at
 * http://www.microchip.com/forums/tm.aspx?m=384936
 */

#include "Compiler.h"
#include "GenericTypeDefs.h"
#include "eeprom.h"

extern void _startup (void);        // See c018i.c in your C18 compiler dir

// this is the address in the bootloader right after the sw test
#define BOOT_LOADER_AFTER_SW2_TEST	0x718

void chk_mode (void) {
    
    INTCONbits.GIE = 0;		// disable interrupts (we are allowed to do
    				// this because we run at the very beginning
				// before any other initialization

    /* we are repeating here inline the eeGet and eePut functions:
     * see below for comments on how eeGet/eePut actually operate
     */
    EEADR = EE_BOOTLOAD_FLAG;
    EECON1bits.EEPGD = 0;	// read from eeprom, not program flash
    EECON1bits.CFGS = 0;	// read from eeprom, not configuration settings
    EECON1bits.RD = 1;		// perform a read operation

    /* if EE_BOOTLOAD_FLAG is zero or unset, do a normal start */
    if (EEDATA == 0 || EEDATA == 0xEE) {
        _asm goto _startup _endasm
    }

    /* else, reset EE_BOOTLOAD_FLAG to zero for next boot, then switch to
     * boot loader mode by jumping to the bootloader code, right after the
     * test for the switch press (effectively bypassing the test)
     */

    EEDATA = 0;
    // assuming that EEADR, EEPGD and CFGS do not change by themselves...
    EECON1bits.WREN = 1;	// enable memory write operations

    _asm
      MOVLW	0x55		// perform the required sequence: write 0x55...
      MOVWF	EECON2, ACCESS	// ...to EECON2, then...
      MOVLW	0xAA		// ...write 0xAA...
      MOVWF	EECON2, ACCESS	// ...to EECON2, and finally
    _endasm
    EECON1bits.WR = 1;		// ...set WR bit to begin write

    while (EECON1bits.WR) /* loop */;	// wait for write to complete

    EECON1bits.WREN = 0;	// prevent accidental writes by disbling writes

    _asm goto BOOT_LOADER_AFTER_SW2_TEST _endasm
}

BYTE eeGet(BYTE addr)
{
    EEADR = addr;
    EECON1bits.EEPGD = 0;	// read from eeprom, not program flash
    EECON1bits.CFGS = 0;	// read from eeprom, not configuration settings
    EECON1bits.RD = 1;		// perform a read operation

    return EEDATA;		// that's it, get back data


}

void eePut(BYTE addr, BYTE data)
{
    BYTE gie_org = INTCONbits.GIE;

    EEADR = addr;
    EEDATA = data;

    EECON1bits.EEPGD = 0;	// write to eeprom, not program flash
    EECON1bits.CFGS = 0;	// write to eeprom, not configuration settings
    EECON1bits.WREN = 1;	// enable memory write operations

    INTCONbits.GIE = 0;		// enter critical section
    _asm
      MOVLW	0x55		// perform the required sequence: write 0x55...
      MOVWF	EECON2, ACCESS	// ...to EECON2, then...
      MOVLW	0xAA		// ...write 0xAA...
      MOVWF	EECON2, ACCESS	// ...to EECON2, and finally
    _endasm
    EECON1bits.WR = 1;		// ...set WR bit to begin write

    while (EECON1bits.WR) /* loop */;	// wait for write to complete
    INTCONbits.GIE = gie_org;

    EECON1bits.WREN = 0;	// prevent accidental writes by disbling writes
}

void eeRead(BYTE addr, BYTE *buff, BYTE count)
{
    BYTE x;
    for (x = 0; x < count; x++) {
        buff[x] = eeGet (addr + x);
    }
}

void eeWrite(BYTE addr, BYTE *buff, BYTE count)
{
    BYTE x;
    for (x = 0; x < count; x++) {
        eePut (addr + x, buff[x]);
    }
}

// note: buff must have a size of (2 * count)
void eeReadHex(BYTE addr, WORD *buff, BYTE count)
{
    BYTE b, x;
    const char hex[] = "01234567890ABCDEF";
    for (x = 0; x < count; x++) {
        b = eeGet (addr + x);
	buff [(x << 1)]     = hex [(b & 0xF0) >> 4];
	buff [(x << 1) + 1] = hex [(b & 0x0F)];
    }
}
