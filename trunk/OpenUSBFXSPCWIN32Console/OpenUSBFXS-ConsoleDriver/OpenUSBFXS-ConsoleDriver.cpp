// OpenUSBFXS-ConsoleDriver.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>	//Definitions for various common and not so common types like DWORD, PCHAR, HANDLE, etc.
#include <Dbt.h>		//Need this for definitions of WM_DEVICECHANGE messages
#include <sys/stat.h>
#include <fcntl.h>      /* Needed only for _O_RDWR definition */
#include <io.h>
#include <stdlib.h>
#include "usb.h"
#include "oufxsbrdcodes.h"	// codes for communicating with the open usb fxs board

// Open USB FXS device VID and PID (currently, Microchip's, they must change)
#define Device_VID	0x04d8	// Microchip's VID
#define Device_PID	0xfcf1	// OpenUSBFXS PID, sub-licensed by Microchip
// This used to be 0x000c, same as the PICDEM board
// #define Device_PID	0x000c

// Endpoints used
#define EP1OUTHandle	0x01
#define EP1INHandle		0x81
#define EP2OUTHandle	0x02
#define EP2INHandle		0x82

#define	WriteBulkToUSB(device,endpoint,buffer,count,lengthp,timeout) \
	(device&&((*(lengthp))=usb_bulk_write(device,endpoint,(char *)buffer,count,timeout))==count)
#define ReadBulkFrmUSB(device,endpoint,buffer,count,lengthp,timeout) \
	 (device&&((*(lengthp))=usb_bulk_read(device,endpoint,(char *)buffer,count,timeout))==count)


static usb_dev_handle *UsbDevInstance;
static unsigned int RegValue;


usb_dev_handle *LibUSBGetDevice (unsigned short vid, unsigned short pid) {
	struct usb_bus *UsbBus = NULL;
	struct usb_device *UsbDevice = NULL;
	usb_dev_handle *ret;

	usb_find_busses ();
	usb_find_devices ();

	for (UsbBus = usb_get_busses(); UsbBus; UsbBus = UsbBus->next) {
		for (UsbDevice = UsbBus->devices; UsbDevice; UsbDevice = UsbDevice->next) {
			if (UsbDevice->descriptor.idVendor == vid && UsbDevice->descriptor.idProduct== pid) {
				break;
			}
		}
	}
	if (!UsbDevice) return NULL;
	ret = usb_open (UsbDevice);

	if (usb_set_configuration (ret, 1) < 0) {
		usb_close (ret);
		return NULL;
	}

	if (usb_claim_interface (ret, 0) < 0) {
		usb_close (ret);
		return NULL;
	}

	return ret;
}

bool SofProfile (void) {
	unsigned char Buffer [32];
	DWORD ActualLength;
	int i;
	Buffer[0] = SOF_PROFILE;
	if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 1, &ActualLength, 1000)) { // send the command over USB
		printf ("Error writing SOF_PROFILE command\n");
		return false;
	}
	if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 32, &ActualLength, 1000)) {//Receive the answer from the device firmware through USB
		// signal an error
		printf ("Error receiving SOF_PROFILE result\n");
		return false;
	}
	for (i = 2; i < 16; i++) {
		printf (" %02d  ", i - 1);
	}
	printf ("\n");
	for (i = 2; i < 16; i++) {
		unsigned short t1, t2;
		t2 = (unsigned short) Buffer [2 * i] |
			 (((unsigned short) Buffer [2 * i + 1]) << 8);
		t1 = (unsigned short) Buffer [2 * i - 2] |
			 (((unsigned short) Buffer [2 * i - 1]) << 8);
		printf ("%04X ", (unsigned short) (t2 - t1));
	}
	printf ("\n");
	return true;
}

unsigned char getpassout (void) {
    unsigned char Buffer [4];
	DWORD ActualLength;

	Buffer[0] = DEBUG_GET_PSOUT;
	if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 3, &ActualLength, 1000)) { // send the command over USB
		printf ("Error writing GET_PSOUT command\n");
		return 0;
	}
	if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 4, &ActualLength, 1000)) {//Receive the answer from the device firmware through USB
		// signal an error
		printf ("Error receiving GET_PSOUT result\n");
		return Buffer[2];
	}
}



bool IsValidPROSLICDirectRegister (const unsigned int b) {
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
				case 95:
					return false;
				default:
					if (b > 108) return false;
					return true;
			}
	}

bool IsValidPROSLICIndirectRegister (const unsigned int b) {
			if (b <= 43) return true;
			if (b >= 99 && b <= 104) return true;
			return false;
	}

bool ReadPROSLICDirectRegister (unsigned char Reg, unsigned int *RetVal, int ExpVal) {
			unsigned char Buffer [64];
			DWORD ActualLength;
			Buffer[0] = PROSLIC_RDIRECT;
			Buffer[1] = Reg;
			if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 3, &ActualLength, 1000)) { // send the command over USB
				// signal an error
				*RetVal = 300;
				return false;
			}
			if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 3, &ActualLength, 1000)) { //Receive the answer from the device firmware through USB
				// signal an error
				*RetVal = 400;
				return false;
			}

			*RetVal = Buffer [2];

			if (ExpVal == -1) return true;

			if (Buffer[2] != ExpVal) {
				*RetVal = 500 + Buffer[2];
				return false;
			}
			
			return true;
		 }

bool WritePROSLICDirectRegister (unsigned char Reg, unsigned int *RetVal, unsigned int NewVal, bool MayDiffer) {
			unsigned char Buffer [32];
			DWORD ActualLength;
			Buffer[0] = PROSLIC_WDIRECT;
			Buffer[1] = Reg;
			Buffer[2] = NewVal;
			if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 3, &ActualLength, 1000)) { // send the command over USB
				// signal an error
				*RetVal = 300;
				return false;
			}
			if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 3, &ActualLength, 1000)) {//Receive the answer from the device firmware through USB
				// signal an error
				*RetVal = 400;
				return false;
			}

			*RetVal = Buffer [2];

			if (MayDiffer) return true;

			if (Buffer[2] != NewVal) {
				*RetVal = 500 + Buffer[2];
				return false;
			}
			
			return true;
		 }


bool ReadPROSLICIndirectRegister (unsigned char Reg, unsigned int *RetVal, int ExpVal) {
			unsigned char Buffer [32];
			DWORD ActualLength;
			Buffer[0] = PROSLIC_RDINDIR;
			Buffer[1] = Reg;

			if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 4, &ActualLength, 1000)) { // send the command over USB
				// signal an error
				*RetVal = 300000;
				return false;
			}
			if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 4, &ActualLength, 1000)) {
				// signal an error
				*RetVal = 400000;
				return false;
			}

			*RetVal = Buffer [2] + (Buffer[3] << 8);

			if (ExpVal == -1) return true;

			if (*RetVal != ExpVal) {
				*RetVal += 500000;
				return false;
			}
			
			return true;
		 }


bool WritePROSLICIndirectRegister (unsigned char Reg, unsigned int *RetVal, unsigned int NewVal, bool MayDiffer) {
			unsigned char Buffer [32];
			unsigned short SNewVal = (unsigned short) (NewVal & 0xffff);
			DWORD ActualLength;
			Buffer[0] = PROSLIC_WRINDIR;
			Buffer[1] = Reg;
			Buffer[2] = SNewVal & 0xff;
			Buffer[3] = SNewVal >> 8;

			if (!WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, Buffer, 4, &ActualLength, 1000)) { // send the command over USB
				// signal an error
				*RetVal = 300000;
				return false;
			}
			if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, Buffer, 4, &ActualLength, 1000)) {//Receive the answer from the device firmware through USB
				// signal an error
				*RetVal = 400000;
				return false;
			}

			*RetVal = (unsigned int) Buffer [2] + (((unsigned int)Buffer[3]) << 8);

			if (MayDiffer) return true;

			if (*RetVal != NewVal) {
				*RetVal = 500000 + Buffer[2] + (Buffer[3] << 8);
				return false;
			}
			
			return true;
		 }


void ShowDirectRegisters (void) {
	int c = 0;
	unsigned int RegVal;

	printf ("\nDirect Registers:\n\n");
	printf ("         0   1   2   3   4   5   6   7   8   9\n\n");
	for (unsigned char i = 0; i < 110; i++) {
		if (c == 0) printf ("%2d0    ", i / 10);
		if (IsValidPROSLICDirectRegister (i)) {
			ReadPROSLICDirectRegister (i, &RegVal, -1);
			if (RegVal > 255) {
				printf (" XX\n");
				break;
			}
			else {
				printf (" %02X ", RegVal);
			}
		}
		else {
			printf ("    ");
		}
		if (c++ == 9) {
			printf ("\n");
			c = 0;
		}
	}
	printf ("\n\n");
}


void ShowIndirectRegisters (void) {
	int c = 0;
	unsigned int RegVal;

	printf ("\nIndirect Registers:\n\n");
	printf ("          0     1     2     3     4     5     6     7     8     9\n\n");
	for (unsigned char i = 0; i < 110; i++) {
		if (c == 0) printf ("%2d0    ", i / 10);
		if (IsValidPROSLICIndirectRegister (i)) {
			ReadPROSLICIndirectRegister (i, &RegVal, -1);
			if (RegVal > 65535) {
				printf (" XXXX\n");
				break;
			}
			else {
				printf (" %04X ", RegVal);
			}
		}
		else {
			printf ("      ");
		}
		if (c++ == 9) {
			printf ("\n");
			c = 0;
		}
	}
	printf ("\n\n");
}

void ShowDirectRegister (unsigned char reg) {
	if (!ReadPROSLICDirectRegister (reg, &RegValue, -1)) return;
	printf ("::: DR%3d->%d\n", reg, RegValue);
}


bool WriteAndShowDR (unsigned char reg, unsigned int *RetVal, unsigned int NewVal, bool MayDiffer) {
	if (!WritePROSLICDirectRegister (reg, &RegValue, NewVal, MayDiffer)) {
		if (MayDiffer) {
			printf ("Error writing DR %d, got %d\n", reg, RegValue);
		}
		else {
			printf ("DR %d: expected %d, got %d\n", reg, NewVal, RegValue);
		}
		Sleep (5000);
		return false;
	}
	printf ("OK: DR%3d->%d\n", reg, RegValue);
	return true;
}


void SendAudioFile (LPCWSTR filename) {
	struct _stat st;
	int f;
	char *ReadBuf = NULL;
	size_t ReadBufLength;

	size_t rbpos = 0;
	unsigned char seq = 0;

	int ignore;
	unsigned char isoctl[12];

#	define IBSZ 512
	void *rctx1, *rctx2, *wctx1, *wctx2;
	unsigned char rbuf1[IBSZ], rbuf2[IBSZ], wbuf1[IBSZ], wbuf2[IBSZ];

	if (_wstat (filename, &st) < 0) {
		printf ("SendAudioFile: could not stat %ws\n", filename);
		return;
	}

	if ((ReadBuf = (char *) malloc (st.st_size)) == NULL) {
		printf ("SendAudioFile: failed to alloc %d bytes\n", st.st_size);
		return;
	}
	
	if ((f = _wopen (filename, O_RDONLY)) < 0) {
		printf ("SendAudioFile: could not open %ws\n", filename);
		free (ReadBuf);
		return;
	}

	if ((ReadBufLength = _read (f, ReadBuf, st.st_size)) < 0) {
		printf ("SendAudioFile: read failed\n", filename);
		free (ReadBuf);
		return;
	}

	_close (f);

	if(!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
		printf ("SendAudioFile: Could not SetPriorityClass");
	}
	printf("SendAudioFile: Current priority class is 0x%x\n", GetPriorityClass (GetCurrentProcess ()));

	isoctl [0] = START_STOP_ISO;	// manage isochronous
	isoctl [1] = 0;		// don't pause USB I/O
	isoctl [2] = 0;		// unused
	isoctl [3] = 0x01;	// test pattern
	isoctl [4] = 0x02;
	isoctl [5] = 0x03;
	isoctl [6] = 0x04;
	isoctl [7] = 0x11;
	isoctl [8] = 0x12;
	isoctl [9] = 0x13;
	isoctl [10]= 0x14;
	isoctl [11]= 0x15;

	usb_isochronous_setup_async (UsbDevInstance, &rctx1, EP2INHandle, 16);
	usb_isochronous_setup_async (UsbDevInstance, &rctx2, EP2INHandle, 16);
	usb_isochronous_setup_async (UsbDevInstance, &wctx1, EP2OUTHandle, 16);
	usb_isochronous_setup_async (UsbDevInstance, &wctx2, EP2OUTHandle, 16);

	memset (rbuf1, 0, sizeof(rbuf1));
	memset (rbuf2, 0, sizeof(rbuf2));

	memset (wbuf1, 0x0, sizeof(wbuf1));
	for (unsigned char *q = wbuf1 + 3; q - wbuf1 < sizeof (wbuf1); q += 16) *q = seq++;
	for (unsigned char *q = wbuf1 + 8; q - wbuf1 < sizeof (wbuf1);) {
		for (int j = 0; j < 8; j++) {
			if (rbpos >= ReadBufLength) break;
			*q++ = ReadBuf[rbpos++];
			// *q++ = 0xAA;
		}
		if (rbpos >= ReadBufLength) break;
		q += 8;
	}

	memset (wbuf2, 0x0, sizeof(wbuf2));
	for (unsigned char *q = wbuf2 + 3; q - wbuf2 < sizeof (wbuf2); q += 16) *q = seq++;
	for (unsigned char *q = wbuf2 + 8; q - wbuf2 < sizeof (wbuf2);) {
		for (int j = 0; j < 8; j++) {
			if (rbpos >= ReadBufLength) break;
			*q++ = ReadBuf[rbpos++];
			// *q++ = 0xAA;
		}
		if (rbpos >= ReadBufLength) break;
		q += 8;
	}

	RegValue = 0;
	usb_submit_async (rctx1, (char *)rbuf1, sizeof (rbuf1));
	usb_submit_async (wctx1, (char *)wbuf1, sizeof (wbuf1));

	WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB

	usb_submit_async (rctx2, (char *)rbuf2, sizeof (rbuf2));
	usb_submit_async (wctx2, (char *)wbuf2, sizeof (wbuf2));

	unsigned char *wbp = wbuf1;
	unsigned char *rbp = rbuf1;
	void **r = &rctx1;
	void **w = &wctx1;
	int rofs, wofs;

	for (int rpacks = 0, wpacks = 0; RegValue < ((unsigned int) ReadBufLength / 8) + 1;) {
		while (true) {
			rofs = usb_reap_async_nocancel (*r, IBSZ/16);
			wofs = usb_reap_async_nocancel (*w, IBSZ/16);
			if (wofs < 0) {
				RegValue = -wofs;
				Sleep (5000);
				goto outahere;
			}
			rofs >>= 4;
			wofs >>= 4;
			if (wofs >= wpacks) {
				break;
			}
			// RegValue = wpacks;
			RegValue = rpacks;
			// do nothing more here, just loop over
		}
		RegValue += wofs - wpacks;
		// RegValue += rofs - rpacks;
		rpacks = rofs;
		wpacks = wofs;
		if (wpacks >= IBSZ/16) {
			wpacks = 0;
			rpacks = 0;
			usb_reap_async (*r, 1);
			// usb_free_async (r);
			usb_reap_async (*w, 1);
			usb_free_async (w);
			// usb_isochronous_setup_async (UsbDevInstance, r, EP2INHandle,  16);
			usb_isochronous_setup_async (UsbDevInstance, w, EP2OUTHandle, 16);
			
			memset (wbp, 0, sizeof(wbuf1));
			for (unsigned char *q = wbp + 3; q - wbp < sizeof (wbuf1); q += 16) *q = seq++;
			for (unsigned char *q = wbp + 8; q - wbp < sizeof (wbuf1);) {
				for (int j = 0; j < 8; j++) {
					if (rbpos >= ReadBufLength) break;
					*q++ = ReadBuf[rbpos++];
					// *q++ = 0xAA;
				}
				if (rbpos >= ReadBufLength) break;
				q += 8;
			}

			usb_submit_async (*r, (char *)rbp, sizeof (rbuf1));
			usb_submit_async (*w, (char *)wbp, sizeof (wbuf1));
			if (wbp == wbuf1) {
				wbp = wbuf2;
				rbp = rbuf2;
				r = &rctx2;
				w = &wctx2;
			}
			else {
				wbp = wbuf1;
				rbp = rbuf1;
				r = &rctx1;
				w = &wctx1;
			}
		}
	}
outahere:
	isoctl [0] = 0x7E;	// manage isochronous
	isoctl [1] = 0xFF;		// pause USB I/O
	WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB

	usb_reap_async (rctx1, 1000);
	usb_reap_async (rctx2, 1000);
	usb_reap_async (wctx1, 1000);
	usb_reap_async (wctx2, 1000);
	usb_free_async (&rctx1);
	usb_free_async (&rctx2);
	usb_free_async (&wctx1);
	usb_free_async (&wctx2);
	free (ReadBuf);
	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
}



int _tmain(int argc, _TCHAR* argv[])
{

	usb_init ();
usb_not_detected:
	UsbDevInstance = NULL;
	while ((UsbDevInstance = LibUSBGetDevice (Device_VID, Device_PID)) == NULL) {
		printf ("No OpenUSBFXS device detected, waiting...\n");
		Sleep (2000);
	}
	printf ("OpenUSBFXS device found!\n");
	RegValue = 0;

start_over:
	if (RegValue > 255 && RegValue < 500) goto usb_not_detected;

	printf ("OpenUSBFXS driver: starting up...\n");

#if 0
	SofProfile ();
	SofProfile ();
	SofProfile ();
	SofProfile ();
	exit (0);
#endif

	ShowDirectRegisters ();
	
	// verify PROSLIC working
	if (!ReadPROSLICDirectRegister (11, &RegValue, 51)) {
		printf ("DR %d: expected %d, got %d\n", 11, 51, RegValue);
		Sleep (5000);
		goto start_over;
	}
	printf ("OK: DR 11->%d\n", RegValue);
	// DR14 <- 0x10 (take DC-DC converter down)
	if (!WriteAndShowDR (14, &RegValue, 0x10, false)) goto start_over;
	// set linefeed (DR64) to 0 (open mode)
	if (!WriteAndShowDR (64, &RegValue, 0, true)) goto start_over;

	printf ("OpenUSBFXS driver: done with basic initialization.\n");
	ShowDirectRegisters ();


	printf ("OpenUSBFXS driver: initializing indirect registers...\n");
	// {0,255,"DTMF_ROW_0_PEAK",0x55C2},
	WritePROSLICIndirectRegister (0, &RegValue, 0x55C2, true);  printf ("OK: IR  0->%04X\n", RegValue);
	// {1,255,"DTMF_ROW_1_PEAK",0x51E6},
	WritePROSLICIndirectRegister (1, &RegValue, 0x51E6, true);  printf ("OK: IR  1->%04X\n", RegValue);
	// {2,255,"DTMF_ROW2_PEAK",0x4B85},
	WritePROSLICIndirectRegister (2, &RegValue, 0x4B85, true);  printf ("OK: IR  2->%04X\n", RegValue);
	// {3,255,"DTMF_ROW3_PEAK",0x4937},
	WritePROSLICIndirectRegister (3, &RegValue, 0x4937, true);  printf ("OK: IR  3->%04X\n", RegValue);
	// {4,255,"DTMF_COL1_PEAK",0x3333},
	WritePROSLICIndirectRegister (4, &RegValue, 0x3333, true);  printf ("OK: IR  4->%04X\n", RegValue);
	// {5,255,"DTMF_FWD_TWIST",0x0202},
	WritePROSLICIndirectRegister (5, &RegValue, 0x0202, true);  printf ("OK: IR  5->%04X\n", RegValue);
	// {6,255,"DTMF_RVS_TWIST",0x0202},
	WritePROSLICIndirectRegister (6, &RegValue, 0x0202, true);  printf ("OK: IR  6->%04X\n", RegValue);
	// {7,255,"DTMF_ROW_RATIO_TRES",0x0198},
	WritePROSLICIndirectRegister (7, &RegValue, 0x0198, true);  printf ("OK: IR  7->%04X\n", RegValue);
	// {8,255,"DTMF_COL_RATIO_TRES",0x0198},
	WritePROSLICIndirectRegister (8, &RegValue, 0x0198, true);  printf ("OK: IR  8->%04X\n", RegValue);
	// {9,255,"DTMF_ROW_2ND_ARM",0x0611},
	WritePROSLICIndirectRegister (9, &RegValue, 0x0611, true);  printf ("OK: IR  9->%04X\n", RegValue);
	// {10,255,"DTMF_COL_2ND_ARM",0x0202},
	WritePROSLICIndirectRegister (10, &RegValue, 0x0202, true); printf ("OK: IR 10->%04X\n", RegValue);
	// {11,255,"DTMF_PWR_MIN_TRES",0x00E5},
	WritePROSLICIndirectRegister (11, &RegValue, 0x00E5, true); printf ("OK: IR 11->%04X\n", RegValue);
	// {12,255,"DTMF_OT_LIM_TRES",0x0A1C},
	WritePROSLICIndirectRegister (12, &RegValue, 0x0A1C, true); printf ("OK: IR 12->%04X\n", RegValue);
	// {13,0,"OSC1_COEF",0x7B30},
	WritePROSLICIndirectRegister (13, &RegValue, 0x7B30, true); printf ("OK: IR 13->%04X\n", RegValue);
	// {14,1,"OSC1X",0x0063},
	WritePROSLICIndirectRegister (14, &RegValue, 0x0063, true); printf ("OK: IR 14->%04X\n", RegValue);
	// {15,2,"OSC1Y",0x0000},
	WritePROSLICIndirectRegister (15, &RegValue, 0x0000, true); printf ("OK: IR 15>%04X\n", RegValue);
	// {16,3,"OSC2_COEF",0x7870},
	WritePROSLICIndirectRegister (16, &RegValue, 0x7870, true); printf ("OK: IR 16->%04X\n", RegValue);
	// {17,4,"OSC2X",0x007D},
	WritePROSLICIndirectRegister (17, &RegValue, 0x007D, true); printf ("OK: IR 17->%04X\n", RegValue);
	// {18,5,"OSC2Y",0x0000},
	WritePROSLICIndirectRegister (18, &RegValue, 0x0000, true); printf ("OK: IR 18->%04X\n", RegValue);
	// {19,6,"RING_V_OFF",0x0000},
	WritePROSLICIndirectRegister (19, &RegValue, 0x0000, true); printf ("OK: IR 19->%04X\n", RegValue);
	// {20,7,"RING_OSC",0x7EF0},
	WritePROSLICIndirectRegister (20, &RegValue, 0x7EF0, true); printf ("OK: IR 20->%04X\n", RegValue);
	// {21,8,"RING_X",0x0160},
	WritePROSLICIndirectRegister (21, &RegValue, 0x0160, true); printf ("OK: IR 21->%04X\n", RegValue);
	// {22,9,"RING_Y",0x0000},
	WritePROSLICIndirectRegister (22, &RegValue, 0x0000, true); printf ("OK: IR 22->%04X\n", RegValue);
	// {23,255,"PULSE_ENVEL",0x2000},
	WritePROSLICIndirectRegister (23, &RegValue, 0x2000, true); printf ("OK: IR 23->%04X\n", RegValue);
	// {24,255,"PULSE_X",0x2000},
	WritePROSLICIndirectRegister (24, &RegValue, 0x2000, true); printf ("OK: IR 24->%04X\n", RegValue);
	// {25,255,"PULSE_Y",0x0000},
	WritePROSLICIndirectRegister (25, &RegValue, 0x0000, true); printf ("OK: IR 25->%04X\n", RegValue);
	// //{26,13,"RECV_DIGITAL_GAIN",0x4000},   // playback volume set lower
	// {26,13,"RECV_DIGITAL_GAIN",0x2000},     // playback volume set lower
	WritePROSLICIndirectRegister (26, &RegValue, 0x2000, true); printf ("OK: IR 26->%04X\n", RegValue);
	// {27,14,"XMIT_DIGITAL_GAIN",0x4000},
	// //{27,14,"XMIT_DIGITAL_GAIN",0x2000},
	WritePROSLICIndirectRegister (27, &RegValue, 0x4000, true); printf ("OK: IR 27->%04X\n", RegValue);
	// {28,15,"LOOP_CLOSE_TRES",0x1000},
	WritePROSLICIndirectRegister (28, &RegValue, 0x1000, true); printf ("OK: IR 28->%04X\n", RegValue);
	// {29,16,"RING_TRIP_TRES",0x3600},
	WritePROSLICIndirectRegister (29, &RegValue, 0x3600, true); printf ("OK: IR 29->%04X\n", RegValue);
	// {30,17,"COMMON_MIN_TRES",0x1000},
	WritePROSLICIndirectRegister (30, &RegValue, 0x1000, true); printf ("OK: IR 30->%04X\n", RegValue);
	// {31,18,"COMMON_MAX_TRES",0x0200},
	WritePROSLICIndirectRegister (31, &RegValue, 0x0200, true); printf ("OK: IR 31->%04X\n", RegValue);
	// {32,19,"PWR_ALARM_Q1Q2",0x07C0},
	// RegIndex = 10032; WritePROSLICIndirectRegister (32, &RegValue, 0x07C0, true); printf ("OK: IR 32->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (32, &RegValue, 0x0FF0, true); printf ("OK: IR 32->%04X\n", RegValue);
	// {33,20,"PWR_ALARM_Q3Q4",0x2600},
	// WritePROSLICIndirectRegister (33, &RegValue, 0x2600, true); printf ("OK: IR 33->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (33, &RegValue, 0x7F80, true); printf ("OK: IR 33->%04X\n", RegValue);
	// {34,21,"PWR_ALARM_Q5Q6",0x1B80},
	// WritePROSLICIndirectRegister (34, &RegValue, 0x1B80, true); printf ("OK: IR 34->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (34, &RegValue, 0x0FF0, true); printf ("OK: IR 34->%04X\n", RegValue);
#if 0		// AN35 advises to set IRs 35--39 to 0x8000 now and then set them to their desired values much later
	// {35,22,"LOOP_CLOSURE_FILTER",0x8000},
	WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); printf ("OK: IR 35->%04X\n", RegValue);
	// {36,23,"RING_TRIP_FILTER",0x0320},
	WritePROSLICIndirectRegister (36, &RegValue, 0x0320, true); printf ("OK: IR 36->%04X\n", RegValue);
	// {37,24,"TERM_LP_POLE_Q1Q2",0x008C},
	// WritePROSLICIndirectRegister (37, &RegValue, 0x008C, true); printf ("OK: IR 37->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (37, &RegValue, 0x0010, true); printf ("OK: IR 37->%04X\n", RegValue);
	// {38,25,"TERM_LP_POLE_Q3Q4",0x0100},
	// WritePROSLICIndirectRegister (38, &RegValue, 0x0100, true); printf ("OK: IR 38->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (38, &RegValue, 0x0010, true); printf ("OK: IR 38->%04X\n", RegValue);
	// {39,26,"TERM_LP_POLE_Q5Q6",0x0010},
	// WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); printf ("OK: IR 39->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); printf ("OK: IR 39->%04X\n", RegValue);
#else
	WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); printf ("OK: IR 35->%04X\n", RegValue);
	WritePROSLICIndirectRegister (36, &RegValue, 0x8000, true); printf ("OK: IR 36->%04X\n", RegValue);
	WritePROSLICIndirectRegister (37, &RegValue, 0x8000, true); printf ("OK: IR 37->%04X\n", RegValue);
	WritePROSLICIndirectRegister (38, &RegValue, 0x8000, true); printf ("OK: IR 38->%04X\n", RegValue);
	WritePROSLICIndirectRegister (39, &RegValue, 0x8000, true); printf ("OK: IR 39->%04X\n", RegValue);
#endif
	// {40,27,"CM_BIAS_RINGING",0x0C00},
	// - set elsewhere to 0 RegIndex = 10000; WritePROSLICIndirectRegister (40, &RegValue, 0x0x0C00, true); printf ("OK: IR 40->%04X\n", RegValue);
	// {41,64,"DCDC_MIN_V",0x0C00},
	WritePROSLICIndirectRegister (41, &RegValue, 0x0C00, true); printf ("OK: IR 41->%04X\n", RegValue);
	// {42,255,"DCDC_XTRA",0x1000},
	// - don't touch yet RegIndex = 10042; WritePROSLICIndirectRegister (42, &RegValue, 0x55C2, true); printf ("OK: IR 42->%04X\n", RegValue);
	// {43,66,"LOOP_CLOSE_TRES_LOW",0x1000},
	WritePROSLICIndirectRegister (43, &RegValue, 0x1000, true); printf ("OK: IR 43->%04X\n", RegValue);
	ShowIndirectRegisters ();


	printf ("OpenUSBFXS Driver: setting up DC parameters...\n");

	// DR  8 <- 0 (take SLIC out of "digital loopback" mode)
	if (!WriteAndShowDR (8, &RegValue, 0, false)) goto start_over;
	// DR108 <- 0xEB (turn on Rev E. features)
	if (!WriteAndShowDR (108, &RegValue, 0xEB, false)) goto start_over;
	// DR 66 <- 0x01 (keep Vov low, let Vbat track Vring)
	if (!WriteAndShowDR (66, &RegValue, 0x01, true)) goto start_over;
	// DR 92 <- 202 (DC-DC converter PWM period=12.33us ~ 81,109kHz)
	// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
	if (!WriteAndShowDR (92, &RegValue, 202, false)) goto start_over;	
	// DR 93 <- 12 (DC-DC converter min off time=732ns)
	// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
	if (!WriteAndShowDR (93, &RegValue, 12, false)) goto start_over;
	// DR 74 <- 44 (high battery voltage = 66V)
	// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
	if (!WriteAndShowDR (74, &RegValue, 44, false)) goto start_over;
	// DR 75 <- 40 (low battery voltage = 60V)
	// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
	if (!WriteAndShowDR (75, &RegValue, 40, false)) goto start_over;
	// DR71<- 0 (max allowed loop current = 20mA [default value])
	if (!WriteAndShowDR (71, &RegValue, 0, false)) goto start_over;
	// IR 40 <- 0
	// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
	WritePROSLICIndirectRegister (40, &RegValue, 0x0, true); printf ("OK: IR 40->%04X\n", RegValue);

	// show a summary of changes
	ShowDirectRegisters ();

	printf ("OpenUSBFXS Driver: bringing up DC-DC converter...\n");

	// DR 14 <- 0 : this should bring the DC-DC converter up
	if (!WriteAndShowDR (14, &RegValue, 0, false)) goto start_over;
	// many things change when the converter goes up, so show again a summary
	ShowDirectRegisters ();
	// check wether we got a decent VBAT value and if so go on;
	// otherwise loop back to start
	for (int i = 0; i < 10; i++) {
		if (!ReadPROSLICDirectRegister (82, &RegValue, -1)) goto start_over;
		RegValue = RegValue * 376 / 1000;
		if (RegValue >= 60) break;
		printf ("Waiting for DC-DC converter to come up (iteration %d of 10, measured %d volts)\n", i + 1, RegValue);
	}
	printf ("DC-DC converter output: %d volts\n", RegValue);
	// if VBAT is not OK, signal we failed and loop over
	if (RegValue < 60) {
		printf ("DC-DC converter failed to power up, restarting in 10 seconds\n");
		printf ("Press ^C to leave the board in its current state (e.g. to perform measurements)\n");
		Sleep (10000);
		goto start_over;
	}
	printf ("OpenUSBFXS Driver: done with DC-DC converter!\n");

	printf ("OpenUSBFXS Driver: performing calibrations...\n");
	// DR 21 <- 0 : disable all interrupts in Interrupt Enable 1
	if (!WriteAndShowDR (21, &RegValue, 0, false)) goto start_over;
	// DR 22 <- 0 : disable all interrupts in Interrupt Enable 2
	if (!WriteAndShowDR (22, &RegValue, 0, false)) goto start_over;
	// DR 23 <- 0 : disable all interrupts in Interrupt Enable 3
	if (!WriteAndShowDR (23, &RegValue, 0, false)) goto start_over;
	// DR 64 <- 0 : set linefeed to Open
	if (!WriteAndShowDR (64, &RegValue, 0, false)) goto start_over;
	// DR97 <- 0x18 : monitor ADC calibration 1&2, but don't do DAC/ADC/balance calibration
	if (!WriteAndShowDR (97, &RegValue, 0x1E, false)) goto start_over;
	// DR 96 <- 0x47 : set CAL bit (start calibration), do differential DAC, common-mode DAC and I_LIM calibrations
	if (!WriteAndShowDR (96, &RegValue, 0x47, true)) goto start_over;
	for (int i = 0; i < 10; i++) {
		if (!ReadPROSLICDirectRegister (96, &RegValue, -1)) goto start_over;
		if (RegValue == 0) break;
		Sleep (200);
	}
	ShowDirectRegister (96);
	if (RegValue != 0) {
		printf ("differential DAC, common-mode DAC or I_LIM calibration(s) , restarting in 10 seconds\n");
		printf ("Press ^C to leave the board in its current state (e.g. to perform measurements)\n");
		Sleep (10000);
		goto start_over;
	}
	// (might: set again DRs 98 and 99 to their reset values 0x10)
	// perform manual calibration for Q5 and Q6 (required, since we are using 3201)
	// manual calibration (Q5 current)
	for (unsigned int j = 0x1f; j > 0; j--) {
		if (!WritePROSLICDirectRegister (98, &RegValue, j, false)) goto start_over;
		Sleep (40);
		if (!ReadPROSLICDirectRegister (88, &RegValue, -1)) goto start_over;
		if (RegValue == 0) break;
	}
	ShowDirectRegister (98);
	ShowDirectRegister (88);
	// manual calibration (Q6 current)
	for (unsigned int j = 0x1f; j > 0; j--) {
		if (!WritePROSLICDirectRegister (99, &RegValue, j, false)) goto start_over;
		Sleep (40);
		if (!ReadPROSLICDirectRegister (89, &RegValue, -1)) goto start_over;
		if (RegValue == 0) break;
	}
	ShowDirectRegister (99);
	ShowDirectRegister (89);

	// enable interrupt logic for on/off-hook mode change during calibration
	if (!WriteAndShowDR (23, &RegValue, 0x04, true)) goto start_over;
	ShowDirectRegister (20);
	// (AN35:) check DR 68 to make sure equipment is on-hook
	// Note: I wonder if this works without setting 64 to 0x01...
	while (true) {
		// set linefeed (DR64) to 0x01 (forward active mode)
		if (!WriteAndShowDR (64, &RegValue, 0x01, true)) goto start_over;
		if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto start_over;
		ShowDirectRegister (68);
		if (!(RegValue & 0x1)) break;
		// set linefeed (DR64) back to 0 (open mode)
		if (!WriteAndShowDR (64, &RegValue, 0, true)) goto start_over;
		// warn that we did not like off-hook at this stage, then restart
		printf ("Error: off-hook found during longitudinal balance calibration - restarting...\n");
		Sleep (4000);
		goto start_over;
	}
	// perform longitudinal balance calibration
	// specify common mode balance calibration (CALCM=1) on calibration control/status register 2 (DR97)
	if (!WriteAndShowDR (97, &RegValue, 0x01, true)) goto start_over;
	ShowDirectRegister (20);
	// then specify start of calibration on calibration control/status register 1 (DR96)
	if (!WriteAndShowDR (96, &RegValue, 0x40, true)) goto start_over;
	// loop waiting calibration to finish
	while (true) {
		ShowDirectRegister (20);
		if (!ReadPROSLICDirectRegister (96, &RegValue, -1)) goto start_over;
		ShowDirectRegister (96);
		Sleep (1);
		if (RegValue == 0) break;
	}
	printf ("OpenUSBFXS Driver: done with calibrations!\n");

	printf ("OpenUSBFXS Driver: starting miscellaneous initializations...\n");
	// flush energy accumulators
	for (unsigned char ir = 88; ir <= 95; ir++) {
		if (!WritePROSLICIndirectRegister (ir, &RegValue, 0, true)) goto start_over;
	}
	if (!WritePROSLICIndirectRegister (97, &RegValue, 0, true)) goto start_over;
	for (unsigned char ir = 193; ir <= 211; ir++) {
		if (!WritePROSLICIndirectRegister (ir, &RegValue, 0, true)) goto start_over;
	}
	// clear all pending interrupts while no interrupts are enabled
	if (!WriteAndShowDR (18, &RegValue, 0xFF, true)) goto start_over;
	if (!WriteAndShowDR (19, &RegValue, 0xFF, true)) goto start_over;
	if (!WriteAndShowDR (20, &RegValue, 0xFF, true)) goto start_over;
	// enable selected interrupts
	if (!WriteAndShowDR (21, &RegValue, 0x00, true)) goto start_over; // none here
	if (!WriteAndShowDR (22, &RegValue, 0xFF, true)) goto start_over; // all here
	// if (!WriteAndShowDR (22, &RegValue, 0x03, true)) goto start_over; // LCP/RTIP only here
	if (!WriteAndShowDR (23, &RegValue, 0x01, true)) goto start_over; // only DTMF here
	//for (unsigned char i = 19; i <= 23; i++) {
	//	if (!WriteAndShowDR (i, &RegValue, 0xFF, true)) goto start_over;
	//}
	// write DRs 2-5 (PCM clock slots)
	for (unsigned char i = 2; i <= 5; i++) {
		if (!WriteAndShowDR (i, &RegValue, 0, false)) goto start_over;
	}
	// write registers 63, 67, 69, 70
	// 63 (Loop Closure Debounce Interval for ringing silent period) - not changed from default 0x54 (105 ms)
	if (!WriteAndShowDR (63, &RegValue, 0x54, false)) goto start_over;
	// 67 (Automatic/Manual Control) - not changed from default 0x1F (all set to auto)
	if (!WriteAndShowDR (67, &RegValue, 0x1F, false)) goto start_over;
	// 69 (Loop Closure Debounce Interval) - not changed from default 0x0A (12.5 ms)
	if (!WriteAndShowDR (69, &RegValue, 0x0A, false)) goto start_over;
	// 70 (Ring Trip Debounce Interval) - not changed from default 0x0A (12.5 ms)
	if (!WriteAndShowDR (70, &RegValue, 0x0A, false)) goto start_over;
	// initialize registers 65-66, 71-73
	// 65 (External Bipolar Transistor Control) - not changed from default 0x61)
	// 66 (Battery Feed Control) - two things here, Vov and TRACK, all set before (Vov to low value range, TRACK to 1)
	// 71 (Loop Current Limit) - not changed from default 0x00 (20mA)
	// 72 (On-Hook Line Voltage) - not changed from default 0x20 (48V)
	// 73 (Common-Mode Voltage) - not changed from default 0x02 (3V) - note: zaptel sets this to 6V
	// Write indirect registers 35-39
	// {35,22,"LOOP_CLOSURE_FILTER",0x8000},
	WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); printf ("OK: IR 35->%04X\n", RegValue);
	// {36,23,"RING_TRIP_FILTER",0x0320},
	WritePROSLICIndirectRegister (36, &RegValue, 0x0320, true); printf ("OK: IR 36->%04X\n", RegValue);
	// {37,24,"TERM_LP_POLE_Q1Q2",0x008C},
	// WritePROSLICIndirectRegister (37, &RegValue, 0x008C, true);  printf ("OK: IR 37->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (37, &RegValue, 0x0010, true); printf ("OK: IR 37->%04X\n", RegValue);
	// {38,25,"TERM_LP_POLE_Q3Q4",0x0100},
	// WritePROSLICIndirectRegister (38, &RegValue, 0x0100, true); printf ("OK: IR 38->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (38, &RegValue, 0x0010, true); printf ("OK: IR 38->%04X\n", RegValue);
	// {39,26,"TERM_LP_POLE_Q5Q6",0x0010},
	// WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); printf ("OK: IR 39->%04X\n", RegValue);
	// as per AN47, p.4
	WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); printf ("OK: IR 39->%04X\n", RegValue);
	// refresh these
	ShowIndirectRegisters ();
	printf ("OpenUSBFXS Driver: done with miscellaneous initializations!\n");

	printf ("OpenUSBFXS Driver: initializing PCM...\n");
	// enable PCM u-law, disable PCM I/O
	if (!WriteAndShowDR (1, &RegValue, 0x08, false)) goto start_over;
	// set the TXS and RXS registers to 1
	if (!WriteAndShowDR (2, &RegValue, 0x01, false)) goto start_over;
	if (!WriteAndShowDR (4, &RegValue, 0x01, false)) goto start_over;
	printf ("OpenUSBFXS Driver: done with PCM initializations.\n");

	printf ("\n\nOpenUSBFXS Driver: all intializations complete!\n\n");

#if 1	// ringing test, comment out when done
	printf ("OpenUSBFXS TEST: the phone should ring now once.\n");
	if (!WritePROSLICDirectRegister (64, &RegValue, 0x04, true)) goto start_over;
	Sleep (2000);
	if (!WritePROSLICDirectRegister (64, &RegValue, 0x00, true)) goto start_over;
#endif

#if 1
	WCHAR lwUserProfile [2048];
	WCHAR lwAudioFile [2048];
	DWORD dwLength;

	dwLength = GetEnvironmentVariable (L"userprofile", lwUserProfile, sizeof (lwUserProfile));
	if (dwLength == 0) {
		printf ("OpenUSBFXS TEST ERROR: %%USERPROFILE%% is not set!\n");
		exit (1);
	}
	else if (dwLength >= 2047) {
		printf ("OpenUSBFXS TEST ERROR: %%USERPROFILE%% is too long!\n");
		exit (1);
	}
	else if (_wchdir (lwUserProfile) != 0) {
		printf ("OpenUSBFXS TEST ERROR: cannot chdir to %%USERPROFILE%%!\n");
		exit (1);
	}
	dwLength = GetEnvironmentVariable (L"audiofile", lwAudioFile, sizeof (lwAudioFile));

	printf ("OpenUSBFXS TEST: pick up phone to listen to audio file.\n");

	// if (!WritePROSLICDirectRegister (67, &RegValue, 0x1e, false)) goto start_over;

	// set forward active mode
	if (!WriteAndShowDR (64, &RegValue, 0x01, true)) goto start_over;


	while (true) {
		if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto start_over;
		ShowDirectRegister (68);

		if (RegValue & 0x01) { // phone picked up

			// set line to off-hook mode
			if (!WriteAndShowDR (64, &RegValue, 0x02, true)) goto start_over;
			// enable PCM I/O using u-law
			if (!WriteAndShowDR (1, &RegValue, 0x28, false)) goto start_over;

			// send audio data from file
			if (dwLength > 0) {
				SendAudioFile (lwAudioFile);
			}
			else {
				SendAudioFile (L".\\Desktop\\asterisk-core-sounds-en-ulaw-current\\vm-options.ulaw");
			}

			// "hangup" the line
			if (!WriteAndShowDR (64, &RegValue, 0x01, true)) goto start_over;

			// disable PCM I/O
			if (!WriteAndShowDR (1, &RegValue, 0x08, false)) goto start_over;
		}

		ShowDirectRegisters ();
		Sleep (2000);

		if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto start_over;
		ShowDirectRegister (68);
		if (RegValue & 0x01) {
			if (!WriteAndShowDR (64, &RegValue, 0x02, true)) goto start_over;
		}
	}
#endif

	return 0;
}

