
/* see http://www.microchip.com/forums/tm.aspx?m=384936 and "eeprom.c" */

void chk_mode (void);

BYTE eeGet(BYTE /*addr*/);

void eePut(BYTE /*addr*/, BYTE /*data*/);

void eeRead(BYTE /*addr*/, BYTE * /*buff*/, BYTE /*count*/);

void eeWrite(BYTE /*addr*/, BYTE * /*buff*/, BYTE /*count*/);

// note: buff must have a size of (2 * count)
void eeReadHex(BYTE /*addr*/, WORD * /*buff*/, BYTE /*count*/);

#define EE_SERIAL_NO		0x00
#define EE_SERIAL_NO_LEN	0x04

#define EE_BOOTLOAD_FLAG	(EE_SERIAL_NO + EE_SERIAL_NO_LEN)
#define EE_BOOTLOAD_FLAG_LEN	0x01
