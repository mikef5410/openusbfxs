/*******************************************************************
 FileName:	proslic.h
 Dependencies:	See INCLUDES section below
 Processor:	PIC18
 Hardware:	The code is intended to be used on the Open USB FXS board
 Compiler:	Microchip C18
 Copyright:	(C) Angelos Varvitsiotis 2009
 License:	GPLv3

 *******************************************************************/

#ifndef PROSLIC_H
#define PROSLIC_H
/** I N C L U D E S **********************************************************/

/** V A R I A B L E S ********************************************************/
#pragma udata

/** P U B L I C  P R O T O T Y P E S *****************************************/
#pragma code
BOOL IsValidPROSLICDirectRegister(BYTE);
void WriteProSLICDirectRegister(BYTE,BYTE);
BYTE ReadProSLICDirectRegister(BYTE);
WORD ReadProSLICIndirectRegister(BYTE);
void WriteProSLICIndirectRegister(BYTE,WORD);

#endif // PROSLIC_H

/** EOF proslic.h *************************************************************/
