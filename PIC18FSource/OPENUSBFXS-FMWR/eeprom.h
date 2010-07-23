
/* see http://www.microchip.com/forums/tm.aspx?m=384936 and "eeprom.c" */

BYTE eeGet(BYTE /*addr*/);

void eePut(BYTE /*addr*/, BYTE /*data*/);

void eeRead(BYTE /*addr*/, BYTE * /*buff*/, BYTE /*count*/);

void eeWrite(BYTE /*addr*/, BYTE * /*buff*/, BYTE /*count*/);

// note: buff must have a size of (2 * count)
void eeReadHex(BYTE /*addr*/, WORD * /*buff*/, BYTE /*count*/);

#define EE_SERIAL_NO		0x00
#define EE_SERIAL_NO_LEN	0x04
