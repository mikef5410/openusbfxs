#pragma once
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
/********************************************************************************************************************************************************************
This code has been adapted from Microchip's example. I am placing the result under copyright
in order to be able to redistribute it under GPL. Small pieces of code here and there can be
tracked back to the original Microchip's code, for which this copyright may be inapplicable.

In order to compile, the code needs two files from libusb-win32 to be present in the top
directory: usb.h and libusb0.dll (from which the code imports). These are to be found as
part of libusb-win32.

Copyright:	(C) Angelos Varvitsiotis 2009
License:	GPLv3
********************************************************************************************************************************************************************/
//-------------------------------------------------------BEGIN CUT AND PASTE BLOCK-----------------------------------------------------------------------------------


#define LIBUSB_WIN32

//Includes
#include <windows.h>	//Definitions for various common and not so common types like DWORD, PCHAR, HANDLE, etc.
#include <Dbt.h>		//Need this for definitions of WM_DEVICECHANGE messages
#include <time.h>		// remove this if not needed

#if defined(MICROCHIP_USB)
# if (defined(CYPRESS_USB)||defined(LIBUSB_WIN32))
# error "Please define ONLY ONE of MICROCHIP_USB, CYPRESS_USB and LIBUSB_WIN32
# endif
# include "mpusbapi.h"	//Make sure this header file is located in your project directory.

#elif defined(LIBUSB_WIN32)
# if (defined(CYPRESS_USB)||defined(MICROCHIP_USB))
# error "Please define ONLY ONE of MICROCHIP_USB, CYPRESS_USB and LIBUSB_WIN32
# endif
  namespace OpenUSBFXSDriverV3 {
# include "usb.h"
  }

#elif defined(CYPRESS_USB)
# if (defined(LIBUSB_WIN32)||defined(MICROCHIP_USB))
# error "Please define ONLY ONE of MICROCHIP_USB, CYPRESS_USB and LIBUSB_WIN32
# endif
# include "CyAPI.h"
# define PLACEHOLDER true

#else
#error "Please define one of MICROCHIP_USB or LIBUSB_WIN32"
#endif

#include "oufxsbrdcodes.h"	// codes for communicating with the open usb fxs board

//When modifying the firmware and changing the Vendor and Device ID's, make
//sure to update the PC application as well.
//Use the formatting: "Vid_xxxx&Pid_xxxx" where xxxx is a 16-bit hexadecimal number.
//This is the USB device that this program will look for:
#ifdef MICROCHIP_USB
	#define DeviceVID_PID "vid_04d8&pid_000c"
#endif // MICROCHIP_USB
#ifdef LIBUSB_WIN32
	#define Device_VID	0x04d8
	#define Device_PID	0x000c	
#endif // LIBUSB_WIN32
#ifdef CYPRESS_USB
	#define Device_VID	0x04d8
	#define Device_PID	0x000c	
#endif // LIBUSB_WIN32
//-------------------------------------------------------END CUT AND PASTE BLOCK-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------


namespace OpenUSBFXSDriverV3 {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------BEGIN CUT AND PASTE BLOCK-----------------------------------------------------------------------------------
	using namespace System::Threading;	
	using namespace System::Runtime::InteropServices;  //Need this to support "unmanaged" code.

	#ifdef UNICODE
	#define	Seeifdef	Unicode
	#else
	#define Seeifdef	Ansi
	#endif



	#ifdef MICROCHIP_USB
	//See the mpusbapi.dll source code (_mpusbapi.cpp) for API related documentation for these functions.
	//The source code is in the MCHPFSUSB vX.X distributions.
	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBGetDLLVersion")] 
	extern "C" DWORD MPUSBGetDLLVersion(void);
	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBGetDeviceCount")] 
	extern "C" DWORD MPUSBGetDeviceCount(PCHAR pVID_PID);
	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBOpen")]
	extern "C" HANDLE MPUSBOpen(DWORD instance,	//  Input
										PCHAR pVID_PID,	// Input
										PCHAR pEP,		// Input
										DWORD dwDir,	// Input
										DWORD dwReserved);// Input

	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBClose")] 
	extern "C" BOOL MPUSBClose(HANDLE handle);	//Input
	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBRead")] 
	extern "C" DWORD MPUSBRead(HANDLE handle,	// Input
										PVOID pData,	// Output
										DWORD dwLen,	// Input
										PDWORD pLength,	// Output
										DWORD dwMilliseconds);// Input

	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBWrite")] 
	extern "C" DWORD MPUSBWrite(HANDLE handle,	// Input
										PVOID pData,	// Output
										DWORD dwLen,	// Input
										PDWORD pLength,	// Output
										DWORD dwMilliseconds);// Input
	[DllImport("MPUSBAPI.dll" , EntryPoint="_MPUSBReadInt")] 
	extern "C" DWORD MPUSBReadInt(HANDLE handle,	// Input
										PVOID pData,	// Output
										DWORD dwLen,	// Input
										PDWORD pLength,	// Output
										DWORD dwMilliseconds);// Input

	#define WriteBulkToUSB(device,endpoint,buffer,count,lengthp,timeout) \
		MPUSBWrite(endpoint,buffer,count,lengthp,timeout)
	#define ReadBulkFrmUSB(device,endpoint,buffer,count,lengthp,timeout) \
	     MPUSBRead(endpoint,buffer,count,lengthp,timeout)
	#endif // MICROCHIP_USB

	#ifdef LIBUSB_WIN32
	[DllImport("libusb0.dll", EntryPoint="usb_set_debug")]
	extern "C" void usb_set_debug(int);
	[DllImport("libusb0.dll", EntryPoint="usb_init")]
	extern "C" void usb_init(void);
	[DllImport("libusb0.dll", EntryPoint="usb_find_busses")]
	extern "C" int usb_find_busses(void);
	[DllImport("libusb0.dll", EntryPoint="usb_find_devices")]
	extern "C" int usb_find_devices(void);
	[DllImport("libusb0.dll", EntryPoint="usb_get_busses")]
	extern "C" struct usb_bus *usb_get_busses(void);
	[DllImport("libusb0.dll", EntryPoint="usb_set_configuration")]
	extern "C" int usb_set_configuration(usb_dev_handle *,int);
	[DllImport("libusb0.dll", EntryPoint="usb_claim_interface")]
	extern "C" int usb_claim_interface(usb_dev_handle *,int);
	[DllImport("libusb0.dll", EntryPoint="usb_open")]
	extern "C" usb_dev_handle *usb_open(struct usb_device *);
	[DllImport("libusb0.dll", EntryPoint="usb_bulk_read")]
	extern "C" int usb_bulk_read (struct usb_dev_handle *,int,char *,int,int);
	[DllImport("libusb0.dll", EntryPoint="usb_bulk_write")]
	extern "C" int usb_bulk_write(usb_dev_handle *,int,char *,int,int);
	[DllImport("libusb0.dll", EntryPoint="usb_isochronous_setup_async")]
	extern "C" int usb_isochronous_setup_async (usb_dev_handle *, void **, unsigned char, int);
	[DllImport("libusb0.dll", EntryPoint="usb_submit_async")]
	extern "C" int usb_submit_async (void *, char *, int);
	[DllImport("libusb0.dll", EntryPoint="usb_reap_async")]
	extern "C" int usb_reap_async (void *, int);
	[DllImport("libusb0.dll", EntryPoint="usb_reap_async_nocancel")]
	extern "C" int usb_reap_async_nocancel (void *, int);
	[DllImport("libusb0.dll", EntryPoint="usb_free_async")]
	extern "C" int usb_free_async (void **);
	[DllImport("libusb0.dll", EntryPoint="usb_release_interface")]
	extern "C" int usb_release_interface (usb_dev_handle *, int);
	[DllImport("libusb0.dll", EntryPoint="usb_close")]
	extern "C" int usb_close(usb_dev_handle *);

	#define	WriteBulkToUSB(device,endpoint,buffer,count,lengthp,timeout) \
		(device&&((*(lengthp))=usb_bulk_write(device,endpoint,(char *)buffer,count,timeout))==count)
	#define ReadBulkFrmUSB(device,endpoint,buffer,count,lengthp,timeout) \
		 (device&&((*(lengthp))=usb_bulk_read(device,endpoint,(char *)buffer,count,timeout))==count)
	#endif

	#ifdef CYPRESS_USB
	#define	WriteBulkToUSB(device,endpoint,buffer,count,lengthp,timeout) \
		willdo
	#define ReadBulkFrmUSB(device,endpoint,buffer,count,lengthp,timeout) \
		willdo

	#endif

	//Need this function for receiving all of the WM_DEVICECHANGE messages.  See MSDN documentation for
	//description of what this function does/how to use it. Note: name is remapped "RegisterDeviceNotificationUM" to
	//avoid possible build error conflicts.
	[DllImport("user32.dll" , CharSet = CharSet::Seeifdef, EntryPoint="RegisterDeviceNotification")]					
	extern "C" HDEVNOTIFY WINAPI RegisterDeviceNotificationUM(
		HANDLE hRecipient,
		LPVOID NotificationFilter,
		DWORD Flags);




	//----------------Global variables used in this application--------------------------------
	#ifdef MICROCHIP_USB
		HANDLE  EP1INHandle = INVALID_HANDLE_VALUE;
		HANDLE  EP1OUTHandle = INVALID_HANDLE_VALUE;
		HANDLE  EP2INHandle = INVALID_HANDLE_VALUE;
		HANDLE  EP2OUTHandle = INVALID_HANDLE_VALUE;
		#define UsbDevInstance 0
	#endif // MICROCHIP_USB
	#ifdef LIBUSB_WIN32
		usb_dev_handle *UsbDevInstance = NULL;
		// following are not truly handlers, but anyway...
		#define EP1OUTHandle	0x01
		#define EP1INHandle		0x81
		#define EP2OUTHandle	0x02
		#define EP2INHandle		0x82
	#endif // LIBUSB_WIN32
	#ifdef CYPRESS_USB
		CCyUSBDevice UsbDevInstance = new CCyUSBDevice();
		#define EP1OUTHandle	UsbDevInstance->BulkOutEndPt;
		#define EP1INHandle		UsbDevInstance->BulkInEndPt;
		#define EP2OUTHandle	UsbDevInstance->IsocOutEndPt;
		#define EP2INHandle		UsbDevInstance->IsocOutEndPt;
	#endif //CYPRESS_USB
	
	unsigned int RegIndex = 0;
	unsigned int RegValue = 0;
	BOOL RegVHex = false;
	BOOL AttachedState = FALSE;		//Need to keep track of the USB device attachment status for proper plug and play operation.
//-------------------------------------------------------END CUT AND PASTE BLOCK-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------



	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------BEGIN CUT AND PASTE BLOCK-----------------------------------------------------------------------------------
			//Additional constructor code

			//Globally Unique Identifier (GUID). Windows uses GUIDs to identify things.  
			GUID InterfaceClassGuid = {0xa5dcbf10, 0x6530, 0x11d2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}; //Globally Unique Identifier (GUID) for USB peripheral devices

			//Register for WM_DEVICECHANGE notifications:
			DEV_BROADCAST_DEVICEINTERFACE MyDeviceBroadcastHeader;// = new DEV_BROADCAST_HDR;
			MyDeviceBroadcastHeader.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			MyDeviceBroadcastHeader.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
			MyDeviceBroadcastHeader.dbcc_reserved = 0;	//Reserved says not to use...
			MyDeviceBroadcastHeader.dbcc_classguid = InterfaceClassGuid;
			RegisterDeviceNotificationUM((HANDLE)this->Handle, &MyDeviceBroadcastHeader, DEVICE_NOTIFY_WINDOW_HANDLE);

			//Now perform an initial start up check of the device state (attached or not attached), since we would not have
			//received a WM_DEVICECHANGE notification.
			#ifdef MICROCHIP_USB
			if(MPUSBGetDeviceCount(DeviceVID_PID))	//Check and make sure at least one device with matching VID/PID is attached
			{
				EP1OUTHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP1", MP_WRITE, 0);
				EP1INHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP1", MP_READ, 0);
				EP2OUTHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP2", MP_WRITE, 0);
				EP2INHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP2", MP_READ, 0);

			#endif // MICROCHIP_USB
			#ifdef LIBUSB_WIN32
			usb_init();

			if (UsbDevInstance = LibUSBGetDevice (Device_VID, Device_PID))  {
			#endif // LIBUSB_WIN32
			#ifdef CYPRESS_USB
			if (PLACEHOLDER) {
			#endif // CYPRESS_USB

				AttachedState = TRUE;
				StatusBox_txtbx->Text = "Device Found: AttachedState = TRUE";
				label2->Enabled = true;	//Make the label no longer greyed out
				// avarvit
				label3->Enabled = true;
			}
			else	//Device must not be connected (or not programmed with correct firmware)
			{
				StatusBox_txtbx->Text = "Device Not Detected: Verify Connection/Correct Firmware";
				AttachedState = FALSE;
				label2->Enabled = false;	//Make the label greyed out
				// avarvit
				label3->Text = L"(no value)";
				label3->Enabled = false;
			}


			ReadWriteThread->RunWorkerAsync();	//Recommend performing USB read/write operations in a separate thread.  Otherwise,
												//the Read/Write operations are effectively blocking functions and can lock up the
												//user interface if the I/O operations take a long time to complete.
//-------------------------------------------------------END CUT AND PASTE BLOCK-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------BEGIN CUT AND PASTE BLOCK-----------------------------------------------------------------------------------
		#ifdef MICROCHIP_USB
			//Make sure to close any open handles, before exiting the application
			if (EP1OUTHandle != INVALID_HANDLE_VALUE)
				MPUSBClose (EP1OUTHandle);
			if (EP1INHandle != INVALID_HANDLE_VALUE)
				MPUSBClose (EP1INHandle);
			if (EP2OUTHandle != INVALID_HANDLE_VALUE)
				MPUSBClose (EP2OUTHandle);
			if (EP2INHandle != INVALID_HANDLE_VALUE)
				MPUSBClose (EP2INHandle);
		#endif
		#ifdef LIBUSB_WIN32
			usb_release_interface (UsbDevInstance, 0);
			usb_close (UsbDevInstance);
			UsbDevInstance = NULL;
		#endif
//-------------------------------------------------------END CUT AND PASTE BLOCK-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------

			if (components)
			{
				delete components;
			}
		}

	// private: System::Windows::Forms::ProgressBar^  progressBar1;
	protected: 
	private: System::ComponentModel::BackgroundWorker^  ReadWriteThread;
	private: System::Windows::Forms::Timer^  timer1;

	private: System::Windows::Forms::TextBox^  StatusBox_txtbx;

	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::Label^  label2;
	private: System::Windows::Forms::Label^  label3;

	private: System::Windows::Forms::Label^  labelDirectRegisters;

	private: System::Windows::Forms::Label^  labelDR0;
	private: System::Windows::Forms::Label^  labelDR1;
	private: System::Windows::Forms::Label^  labelDR2;
	private: System::Windows::Forms::Label^  labelDR3;
	private: System::Windows::Forms::Label^  labelDR4;
	private: System::Windows::Forms::Label^  labelDR5;
	private: System::Windows::Forms::Label^  labelDR6;
	private: System::Windows::Forms::Label^  labelDR7;
	private: System::Windows::Forms::Label^  labelDR8;
	private: System::Windows::Forms::Label^  labelDR9;

	private: System::Windows::Forms::Label^  labelDR000;
	private: System::Windows::Forms::TextBox^  textBoxDR0;
	private: System::Windows::Forms::TextBox^  textBoxDR1;
	private: System::Windows::Forms::TextBox^  textBoxDR2;
	private: System::Windows::Forms::TextBox^  textBoxDR3;
	private: System::Windows::Forms::TextBox^  textBoxDR4;
	private: System::Windows::Forms::TextBox^  textBoxDR5;
	private: System::Windows::Forms::TextBox^  textBoxDR6;
	private: System::Windows::Forms::TextBox^  textBoxDR7;
	private: System::Windows::Forms::TextBox^  textBoxDR8;
	private: System::Windows::Forms::TextBox^  textBoxDR9;

	private: System::Windows::Forms::Label^  labelDR010;
	private: System::Windows::Forms::TextBox^  textBoxDR10;
	private: System::Windows::Forms::TextBox^  textBoxDR11;
	private: System::Windows::Forms::TextBox^  textBoxDR12;
	private: System::Windows::Forms::TextBox^  textBoxDR13;
	private: System::Windows::Forms::TextBox^  textBoxDR14;
	private: System::Windows::Forms::TextBox^  textBoxDR15;
	private: System::Windows::Forms::TextBox^  textBoxDR16;
	private: System::Windows::Forms::TextBox^  textBoxDR17;
	private: System::Windows::Forms::TextBox^  textBoxDR18;
	private: System::Windows::Forms::TextBox^  textBoxDR19;

	private: System::Windows::Forms::Label^  labelDR020;
	private: System::Windows::Forms::TextBox^  textBoxDR20;
	private: System::Windows::Forms::TextBox^  textBoxDR21;
	private: System::Windows::Forms::TextBox^  textBoxDR22;
	private: System::Windows::Forms::TextBox^  textBoxDR23;
	private: System::Windows::Forms::TextBox^  textBoxDR24;
	private: System::Windows::Forms::TextBox^  textBoxDR25;
	private: System::Windows::Forms::TextBox^  textBoxDR26;
	private: System::Windows::Forms::TextBox^  textBoxDR27;
	private: System::Windows::Forms::TextBox^  textBoxDR28;
	private: System::Windows::Forms::TextBox^  textBoxDR29;

	private: System::Windows::Forms::Label^  labelDR030;
	private: System::Windows::Forms::TextBox^  textBoxDR30;
	private: System::Windows::Forms::TextBox^  textBoxDR31;
	private: System::Windows::Forms::TextBox^  textBoxDR32;
	private: System::Windows::Forms::TextBox^  textBoxDR33;
	private: System::Windows::Forms::TextBox^  textBoxDR34;
	private: System::Windows::Forms::TextBox^  textBoxDR35;
	private: System::Windows::Forms::TextBox^  textBoxDR36;
	private: System::Windows::Forms::TextBox^  textBoxDR37;
	private: System::Windows::Forms::TextBox^  textBoxDR38;
	private: System::Windows::Forms::TextBox^  textBoxDR39;

	private: System::Windows::Forms::Label^  labelDR040;
	private: System::Windows::Forms::TextBox^  textBoxDR40;
	private: System::Windows::Forms::TextBox^  textBoxDR41;
	private: System::Windows::Forms::TextBox^  textBoxDR42;
	private: System::Windows::Forms::TextBox^  textBoxDR43;
	private: System::Windows::Forms::TextBox^  textBoxDR44;
	private: System::Windows::Forms::TextBox^  textBoxDR45;
	private: System::Windows::Forms::TextBox^  textBoxDR46;
	private: System::Windows::Forms::TextBox^  textBoxDR47;
	private: System::Windows::Forms::TextBox^  textBoxDR48;
	private: System::Windows::Forms::TextBox^  textBoxDR49;

	private: System::Windows::Forms::Label^  labelDR050;
	private: System::Windows::Forms::TextBox^  textBoxDR50;
	private: System::Windows::Forms::TextBox^  textBoxDR51;
	private: System::Windows::Forms::TextBox^  textBoxDR52;
	private: System::Windows::Forms::TextBox^  textBoxDR53;
	private: System::Windows::Forms::TextBox^  textBoxDR54;
	private: System::Windows::Forms::TextBox^  textBoxDR55;
	private: System::Windows::Forms::TextBox^  textBoxDR56;
	private: System::Windows::Forms::TextBox^  textBoxDR57;
	private: System::Windows::Forms::TextBox^  textBoxDR58;
	private: System::Windows::Forms::TextBox^  textBoxDR59;

	private: System::Windows::Forms::Label^  labelDR060;
	private: System::Windows::Forms::TextBox^  textBoxDR60;
	private: System::Windows::Forms::TextBox^  textBoxDR61;
	private: System::Windows::Forms::TextBox^  textBoxDR62;
	private: System::Windows::Forms::TextBox^  textBoxDR63;
	private: System::Windows::Forms::TextBox^  textBoxDR64;
	private: System::Windows::Forms::TextBox^  textBoxDR65;
	private: System::Windows::Forms::TextBox^  textBoxDR66;
	private: System::Windows::Forms::TextBox^  textBoxDR67;
	private: System::Windows::Forms::TextBox^  textBoxDR68;
	private: System::Windows::Forms::TextBox^  textBoxDR69;

	private: System::Windows::Forms::Label^  labelDR070;
	private: System::Windows::Forms::TextBox^  textBoxDR70;
	private: System::Windows::Forms::TextBox^  textBoxDR71;
	private: System::Windows::Forms::TextBox^  textBoxDR72;
	private: System::Windows::Forms::TextBox^  textBoxDR73;
	private: System::Windows::Forms::TextBox^  textBoxDR74;
	private: System::Windows::Forms::TextBox^  textBoxDR75;
	private: System::Windows::Forms::TextBox^  textBoxDR76;
	private: System::Windows::Forms::TextBox^  textBoxDR77;
	private: System::Windows::Forms::TextBox^  textBoxDR78;
	private: System::Windows::Forms::TextBox^  textBoxDR79;

	private: System::Windows::Forms::Label^  labelDR080;
	private: System::Windows::Forms::TextBox^  textBoxDR80;
	private: System::Windows::Forms::TextBox^  textBoxDR81;
	private: System::Windows::Forms::TextBox^  textBoxDR82;
	private: System::Windows::Forms::TextBox^  textBoxDR83;
	private: System::Windows::Forms::TextBox^  textBoxDR84;
	private: System::Windows::Forms::TextBox^  textBoxDR85;
	private: System::Windows::Forms::TextBox^  textBoxDR86;
	private: System::Windows::Forms::TextBox^  textBoxDR87;
	private: System::Windows::Forms::TextBox^  textBoxDR88;
	private: System::Windows::Forms::TextBox^  textBoxDR89;

	private: System::Windows::Forms::Label^  labelDR090;
	private: System::Windows::Forms::TextBox^  textBoxDR90;
	private: System::Windows::Forms::TextBox^  textBoxDR91;
	private: System::Windows::Forms::TextBox^  textBoxDR92;
	private: System::Windows::Forms::TextBox^  textBoxDR93;
	private: System::Windows::Forms::TextBox^  textBoxDR94;
	private: System::Windows::Forms::TextBox^  textBoxDR95;
	private: System::Windows::Forms::TextBox^  textBoxDR96;
	private: System::Windows::Forms::TextBox^  textBoxDR97;
	private: System::Windows::Forms::TextBox^  textBoxDR98;
	private: System::Windows::Forms::TextBox^  textBoxDR99;

	private: System::Windows::Forms::Label^  labelDR100;
	private: System::Windows::Forms::TextBox^  textBoxDR100;
	private: System::Windows::Forms::TextBox^  textBoxDR101;
	private: System::Windows::Forms::TextBox^  textBoxDR102;
	private: System::Windows::Forms::TextBox^  textBoxDR103;
	private: System::Windows::Forms::TextBox^  textBoxDR104;
	private: System::Windows::Forms::TextBox^  textBoxDR105;
	private: System::Windows::Forms::TextBox^  textBoxDR106;
	private: System::Windows::Forms::TextBox^  textBoxDR107;
	private: System::Windows::Forms::TextBox^  textBoxDR108;
	private: System::Windows::Forms::TextBox^  textBoxDR109;

	private: System::Windows::Forms::Label^  labelIR0;
	private: System::Windows::Forms::Label^  labelIR1;
	private: System::Windows::Forms::Label^  labelIR2;
	private: System::Windows::Forms::Label^  labelIR3;
	private: System::Windows::Forms::Label^  labelIR4;
	private: System::Windows::Forms::Label^  labelIR5;
	private: System::Windows::Forms::Label^  labelIR6;
	private: System::Windows::Forms::Label^  labelIR7;
	private: System::Windows::Forms::Label^  labelIR8;
	private: System::Windows::Forms::Label^  labelIR9;

	private: System::Windows::Forms::TextBox^  textBoxIR0;
	private: System::Windows::Forms::TextBox^  textBoxIR1;
	private: System::Windows::Forms::TextBox^  textBoxIR2;
	private: System::Windows::Forms::TextBox^  textBoxIR3;
	private: System::Windows::Forms::TextBox^  textBoxIR4;
	private: System::Windows::Forms::TextBox^  textBoxIR5;
	private: System::Windows::Forms::TextBox^  textBoxIR6;
	private: System::Windows::Forms::TextBox^  textBoxIR7;
	private: System::Windows::Forms::TextBox^  textBoxIR8;
	private: System::Windows::Forms::TextBox^  textBoxIR9;
	private: System::Windows::Forms::TextBox^  textBoxIR10;
	private: System::Windows::Forms::TextBox^  textBoxIR11;
	private: System::Windows::Forms::TextBox^  textBoxIR12;
	private: System::Windows::Forms::TextBox^  textBoxIR13;
	private: System::Windows::Forms::TextBox^  textBoxIR14;
	private: System::Windows::Forms::TextBox^  textBoxIR15;
	private: System::Windows::Forms::TextBox^  textBoxIR16;
	private: System::Windows::Forms::TextBox^  textBoxIR17;
	private: System::Windows::Forms::TextBox^  textBoxIR18;
	private: System::Windows::Forms::TextBox^  textBoxIR19;
	private: System::Windows::Forms::TextBox^  textBoxIR20;
	private: System::Windows::Forms::TextBox^  textBoxIR21;
	private: System::Windows::Forms::TextBox^  textBoxIR22;
	private: System::Windows::Forms::TextBox^  textBoxIR23;
	private: System::Windows::Forms::TextBox^  textBoxIR24;
	private: System::Windows::Forms::TextBox^  textBoxIR25;
	private: System::Windows::Forms::TextBox^  textBoxIR26;
	private: System::Windows::Forms::TextBox^  textBoxIR27;
	private: System::Windows::Forms::TextBox^  textBoxIR28;
	private: System::Windows::Forms::TextBox^  textBoxIR29;
	private: System::Windows::Forms::TextBox^  textBoxIR30;
	private: System::Windows::Forms::TextBox^  textBoxIR31;
	private: System::Windows::Forms::TextBox^  textBoxIR32;
	private: System::Windows::Forms::TextBox^  textBoxIR33;
	private: System::Windows::Forms::TextBox^  textBoxIR34;
	private: System::Windows::Forms::TextBox^  textBoxIR35;
	private: System::Windows::Forms::TextBox^  textBoxIR36;
	private: System::Windows::Forms::TextBox^  textBoxIR37;
	private: System::Windows::Forms::TextBox^  textBoxIR38;
	private: System::Windows::Forms::TextBox^  textBoxIR39;
	private: System::Windows::Forms::TextBox^  textBoxIR40;
	private: System::Windows::Forms::TextBox^  textBoxIR41;
	private: System::Windows::Forms::TextBox^  textBoxIR42;
	private: System::Windows::Forms::TextBox^  textBoxIR43;
	private: System::Windows::Forms::TextBox^  textBoxIR99;
	private: System::Windows::Forms::TextBox^  textBoxIR100;
	private: System::Windows::Forms::TextBox^  textBoxIR101;
	private: System::Windows::Forms::TextBox^  textBoxIR102;
	private: System::Windows::Forms::TextBox^  textBoxIR103;
	private: System::Windows::Forms::TextBox^  textBoxIR104;

	private: System::Windows::Forms::ToolTip^  toolTipRegisterInfo;

	private: System::ComponentModel::IContainer^  components;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->ReadWriteThread = (gcnew System::ComponentModel::BackgroundWorker());
			this->timer1 = (gcnew System::Windows::Forms::Timer(this->components));
			this->StatusBox_txtbx = (gcnew System::Windows::Forms::TextBox());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->labelDirectRegisters = (gcnew System::Windows::Forms::Label());
			this->labelDR0 = (gcnew System::Windows::Forms::Label());
			this->labelDR1 = (gcnew System::Windows::Forms::Label());
			this->labelDR2 = (gcnew System::Windows::Forms::Label());
			this->labelDR3 = (gcnew System::Windows::Forms::Label());
			this->labelDR4 = (gcnew System::Windows::Forms::Label());
			this->labelDR5 = (gcnew System::Windows::Forms::Label());
			this->labelDR6 = (gcnew System::Windows::Forms::Label());
			this->labelDR7 = (gcnew System::Windows::Forms::Label());
			this->labelDR8 = (gcnew System::Windows::Forms::Label());
			this->labelDR9 = (gcnew System::Windows::Forms::Label());
			this->labelDR000 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR0 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR1 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR2 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR3 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR4 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR5 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR6 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR7 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR8 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR9 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR010 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR10 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR11 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR12 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR13 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR14 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR15 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR16 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR17 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR18 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR19 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR020 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR20 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR21 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR22 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR23 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR24 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR25 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR26 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR27 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR28 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR29 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR030 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR30 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR31 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR32 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR33 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR34 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR35 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR36 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR37 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR38 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR39 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR040 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR40 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR41 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR42 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR43 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR44 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR45 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR46 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR47 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR48 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR49 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR050 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR50 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR51 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR52 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR53 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR54 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR55 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR56 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR57 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR58 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR59 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR060 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR60 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR61 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR62 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR63 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR64 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR65 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR66 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR67 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR68 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR69 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR070 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR70 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR71 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR72 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR73 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR74 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR75 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR76 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR77 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR78 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR79 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR080 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR80 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR81 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR82 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR83 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR84 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR85 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR86 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR87 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR88 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR89 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR090 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR90 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR91 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR92 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR93 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR94 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR95 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR96 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR97 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR98 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR99 = (gcnew System::Windows::Forms::TextBox());
			this->labelDR100 = (gcnew System::Windows::Forms::Label());
			this->textBoxDR100 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR101 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR102 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR103 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR104 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR105 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR106 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR107 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR108 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxDR109 = (gcnew System::Windows::Forms::TextBox());
			this->labelIR0 = (gcnew System::Windows::Forms::Label());
			this->labelIR1 = (gcnew System::Windows::Forms::Label());
			this->labelIR2 = (gcnew System::Windows::Forms::Label());
			this->labelIR3 = (gcnew System::Windows::Forms::Label());
			this->labelIR4 = (gcnew System::Windows::Forms::Label());
			this->labelIR5 = (gcnew System::Windows::Forms::Label());
			this->labelIR6 = (gcnew System::Windows::Forms::Label());
			this->labelIR7 = (gcnew System::Windows::Forms::Label());
			this->labelIR8 = (gcnew System::Windows::Forms::Label());
			this->labelIR9 = (gcnew System::Windows::Forms::Label());
			this->textBoxIR0 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR1 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR2 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR3 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR4 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR5 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR6 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR7 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR8 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR9 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR10 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR11 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR12 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR13 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR14 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR15 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR16 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR17 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR18 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR19 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR20 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR21 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR22 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR23 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR24 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR25 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR26 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR27 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR28 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR29 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR30 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR31 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR32 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR33 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR34 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR35 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR36 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR37 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR38 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR39 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR40 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR41 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR42 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR43 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR99 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR100 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR101 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR102 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR103 = (gcnew System::Windows::Forms::TextBox());
			this->textBoxIR104 = (gcnew System::Windows::Forms::TextBox());
			this->toolTipRegisterInfo = (gcnew System::Windows::Forms::ToolTip(this->components));
			this->SuspendLayout();
			// 
			// ReadWriteThread
			// 
			this->ReadWriteThread->WorkerReportsProgress = true;
			this->ReadWriteThread->DoWork += gcnew System::ComponentModel::DoWorkEventHandler(this, &Form1::ReadWriteThread_DoWork);
			// 
			// timer1
			// 
			this->timer1->Enabled = true;
			this->timer1->Interval = 8;
			this->timer1->Tick += gcnew System::EventHandler(this, &Form1::timer1_Tick);
			// 
			// StatusBox_txtbx
			// 
			this->StatusBox_txtbx->Location = System::Drawing::Point(13, 13);
			this->StatusBox_txtbx->Multiline = true;
			this->StatusBox_txtbx->Name = L"StatusBox_txtbx";
			this->StatusBox_txtbx->Size = System::Drawing::Size(298, 20);
			this->StatusBox_txtbx->TabIndex = 0;
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(317, 16);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(58, 13);
			this->label1->TabIndex = 1;
			this->label1->Text = L"Status Box";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Enabled = false;
			this->label2->Location = System::Drawing::Point(12, 36);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(116, 13);
			this->label2->TabIndex = 2;
			this->label2->Text = L"3210 register# -> value";
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Enabled = false;
			this->label3->Location = System::Drawing::Point(154, 36);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(54, 13);
			this->label3->TabIndex = 3;
			this->label3->Text = L"(no value)";
			// 
			// labelDirectRegisters
			// 
			this->labelDirectRegisters->AutoSize = true;
			this->labelDirectRegisters->Location = System::Drawing::Point(140, 58);
			this->labelDirectRegisters->Name = L"labelDirectRegisters";
			this->labelDirectRegisters->Size = System::Drawing::Size(82, 13);
			this->labelDirectRegisters->TabIndex = 4;
			this->labelDirectRegisters->Text = L"Direct Registers";
			// 
			// labelDR0
			// 
			this->labelDR0->AutoSize = true;
			this->labelDR0->Location = System::Drawing::Point(54, 75);
			this->labelDR0->Name = L"labelDR0";
			this->labelDR0->Size = System::Drawing::Size(13, 13);
			this->labelDR0->TabIndex = 5;
			this->labelDR0->Text = L"0";
			// 
			// labelDR1
			// 
			this->labelDR1->AutoSize = true;
			this->labelDR1->Location = System::Drawing::Point(82, 75);
			this->labelDR1->Name = L"labelDR1";
			this->labelDR1->Size = System::Drawing::Size(13, 13);
			this->labelDR1->TabIndex = 6;
			this->labelDR1->Text = L"1";
			// 
			// labelDR2
			// 
			this->labelDR2->AutoSize = true;
			this->labelDR2->Location = System::Drawing::Point(109, 75);
			this->labelDR2->Name = L"labelDR2";
			this->labelDR2->Size = System::Drawing::Size(13, 13);
			this->labelDR2->TabIndex = 7;
			this->labelDR2->Text = L"2";
			// 
			// labelDR3
			// 
			this->labelDR3->AutoSize = true;
			this->labelDR3->Location = System::Drawing::Point(138, 75);
			this->labelDR3->Name = L"labelDR3";
			this->labelDR3->Size = System::Drawing::Size(13, 13);
			this->labelDR3->TabIndex = 8;
			this->labelDR3->Text = L"3";
			// 
			// labelDR4
			// 
			this->labelDR4->AutoSize = true;
			this->labelDR4->Location = System::Drawing::Point(167, 75);
			this->labelDR4->Name = L"labelDR4";
			this->labelDR4->Size = System::Drawing::Size(13, 13);
			this->labelDR4->TabIndex = 9;
			this->labelDR4->Text = L"4";
			// 
			// labelDR5
			// 
			this->labelDR5->AutoSize = true;
			this->labelDR5->Location = System::Drawing::Point(197, 75);
			this->labelDR5->Name = L"labelDR5";
			this->labelDR5->Size = System::Drawing::Size(13, 13);
			this->labelDR5->TabIndex = 10;
			this->labelDR5->Text = L"5";
			// 
			// labelDR6
			// 
			this->labelDR6->AutoSize = true;
			this->labelDR6->Location = System::Drawing::Point(226, 75);
			this->labelDR6->Name = L"labelDR6";
			this->labelDR6->Size = System::Drawing::Size(13, 13);
			this->labelDR6->TabIndex = 11;
			this->labelDR6->Text = L"6";
			// 
			// labelDR7
			// 
			this->labelDR7->AutoSize = true;
			this->labelDR7->Location = System::Drawing::Point(255, 75);
			this->labelDR7->Name = L"labelDR7";
			this->labelDR7->Size = System::Drawing::Size(13, 13);
			this->labelDR7->TabIndex = 12;
			this->labelDR7->Text = L"7";
			// 
			// labelDR8
			// 
			this->labelDR8->AutoSize = true;
			this->labelDR8->Location = System::Drawing::Point(283, 75);
			this->labelDR8->Name = L"labelDR8";
			this->labelDR8->Size = System::Drawing::Size(13, 13);
			this->labelDR8->TabIndex = 13;
			this->labelDR8->Text = L"8";
			// 
			// labelDR9
			// 
			this->labelDR9->AutoSize = true;
			this->labelDR9->Location = System::Drawing::Point(312, 75);
			this->labelDR9->Name = L"labelDR9";
			this->labelDR9->Size = System::Drawing::Size(13, 13);
			this->labelDR9->TabIndex = 14;
			this->labelDR9->Text = L"9";
			// 
			// labelDR000
			// 
			this->labelDR000->AutoSize = true;
			this->labelDR000->Location = System::Drawing::Point(22, 98);
			this->labelDR000->Name = L"labelDR000";
			this->labelDR000->Size = System::Drawing::Size(13, 13);
			this->labelDR000->TabIndex = 15;
			this->labelDR000->Text = L"0";
			// 
			// textBoxDR0
			// 
			this->textBoxDR0->Location = System::Drawing::Point(48, 95);
			this->textBoxDR0->Name = L"textBoxDR0";
			this->textBoxDR0->Size = System::Drawing::Size(24, 20);
			this->textBoxDR0->TabIndex = 26;
			this->textBoxDR0->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR1
			// 
			this->textBoxDR1->Location = System::Drawing::Point(76, 95);
			this->textBoxDR1->Name = L"textBoxDR1";
			this->textBoxDR1->Size = System::Drawing::Size(24, 20);
			this->textBoxDR1->TabIndex = 27;
			this->textBoxDR1->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR2
			// 
			this->textBoxDR2->Location = System::Drawing::Point(104, 95);
			this->textBoxDR2->Name = L"textBoxDR2";
			this->textBoxDR2->Size = System::Drawing::Size(24, 20);
			this->textBoxDR2->TabIndex = 28;
			this->textBoxDR2->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR3
			// 
			this->textBoxDR3->Location = System::Drawing::Point(133, 95);
			this->textBoxDR3->Name = L"textBoxDR3";
			this->textBoxDR3->Size = System::Drawing::Size(24, 20);
			this->textBoxDR3->TabIndex = 29;
			this->textBoxDR3->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR4
			// 
			this->textBoxDR4->Location = System::Drawing::Point(162, 95);
			this->textBoxDR4->Name = L"textBoxDR4";
			this->textBoxDR4->Size = System::Drawing::Size(24, 20);
			this->textBoxDR4->TabIndex = 30;
			this->textBoxDR4->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR5
			// 
			this->textBoxDR5->Location = System::Drawing::Point(191, 95);
			this->textBoxDR5->Name = L"textBoxDR5";
			this->textBoxDR5->Size = System::Drawing::Size(24, 20);
			this->textBoxDR5->TabIndex = 31;
			this->textBoxDR5->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR6
			// 
			this->textBoxDR6->Location = System::Drawing::Point(220, 95);
			this->textBoxDR6->Name = L"textBoxDR6";
			this->textBoxDR6->Size = System::Drawing::Size(24, 20);
			this->textBoxDR6->TabIndex = 32;
			this->textBoxDR6->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR7
			// 
			this->textBoxDR7->Location = System::Drawing::Point(249, 95);
			this->textBoxDR7->Name = L"textBoxDR7";
			this->textBoxDR7->Size = System::Drawing::Size(24, 20);
			this->textBoxDR7->TabIndex = 33;
			this->textBoxDR7->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR8
			// 
			this->textBoxDR8->Location = System::Drawing::Point(278, 95);
			this->textBoxDR8->Name = L"textBoxDR8";
			this->textBoxDR8->Size = System::Drawing::Size(24, 20);
			this->textBoxDR8->TabIndex = 34;
			this->textBoxDR8->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR9
			// 
			this->textBoxDR9->Location = System::Drawing::Point(307, 95);
			this->textBoxDR9->Name = L"textBoxDR9";
			this->textBoxDR9->Size = System::Drawing::Size(24, 20);
			this->textBoxDR9->TabIndex = 35;
			this->textBoxDR9->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR010
			// 
			this->labelDR010->AutoSize = true;
			this->labelDR010->Location = System::Drawing::Point(16, 125);
			this->labelDR010->Name = L"labelDR010";
			this->labelDR010->Size = System::Drawing::Size(19, 13);
			this->labelDR010->TabIndex = 16;
			this->labelDR010->Text = L"10";
			// 
			// textBoxDR10
			// 
			this->textBoxDR10->Location = System::Drawing::Point(48, 122);
			this->textBoxDR10->Name = L"textBoxDR10";
			this->textBoxDR10->Size = System::Drawing::Size(24, 20);
			this->textBoxDR10->TabIndex = 36;
			this->textBoxDR10->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR11
			// 
			this->textBoxDR11->Location = System::Drawing::Point(76, 122);
			this->textBoxDR11->Name = L"textBoxDR11";
			this->textBoxDR11->Size = System::Drawing::Size(24, 20);
			this->textBoxDR11->TabIndex = 37;
			this->textBoxDR11->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR12
			// 
			this->textBoxDR12->Location = System::Drawing::Point(104, 122);
			this->textBoxDR12->Name = L"textBoxDR12";
			this->textBoxDR12->Size = System::Drawing::Size(24, 20);
			this->textBoxDR12->TabIndex = 38;
			this->textBoxDR12->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR13
			// 
			this->textBoxDR13->Location = System::Drawing::Point(133, 122);
			this->textBoxDR13->Name = L"textBoxDR13";
			this->textBoxDR13->Size = System::Drawing::Size(24, 20);
			this->textBoxDR13->TabIndex = 39;
			this->textBoxDR13->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR14
			// 
			this->textBoxDR14->Location = System::Drawing::Point(162, 122);
			this->textBoxDR14->Name = L"textBoxDR14";
			this->textBoxDR14->Size = System::Drawing::Size(24, 20);
			this->textBoxDR14->TabIndex = 40;
			this->textBoxDR14->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR15
			// 
			this->textBoxDR15->Location = System::Drawing::Point(191, 122);
			this->textBoxDR15->Name = L"textBoxDR15";
			this->textBoxDR15->Size = System::Drawing::Size(24, 20);
			this->textBoxDR15->TabIndex = 41;
			this->textBoxDR15->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR16
			// 
			this->textBoxDR16->Location = System::Drawing::Point(220, 122);
			this->textBoxDR16->Name = L"textBoxDR16";
			this->textBoxDR16->Size = System::Drawing::Size(24, 20);
			this->textBoxDR16->TabIndex = 42;
			this->textBoxDR16->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR17
			// 
			this->textBoxDR17->Location = System::Drawing::Point(249, 122);
			this->textBoxDR17->Name = L"textBoxDR17";
			this->textBoxDR17->Size = System::Drawing::Size(24, 20);
			this->textBoxDR17->TabIndex = 43;
			this->textBoxDR17->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR18
			// 
			this->textBoxDR18->Location = System::Drawing::Point(278, 122);
			this->textBoxDR18->Name = L"textBoxDR18";
			this->textBoxDR18->Size = System::Drawing::Size(24, 20);
			this->textBoxDR18->TabIndex = 44;
			this->textBoxDR18->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR19
			// 
			this->textBoxDR19->Location = System::Drawing::Point(307, 122);
			this->textBoxDR19->Name = L"textBoxDR19";
			this->textBoxDR19->Size = System::Drawing::Size(24, 20);
			this->textBoxDR19->TabIndex = 45;
			this->textBoxDR19->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR020
			// 
			this->labelDR020->AutoSize = true;
			this->labelDR020->Location = System::Drawing::Point(16, 152);
			this->labelDR020->Name = L"labelDR020";
			this->labelDR020->Size = System::Drawing::Size(19, 13);
			this->labelDR020->TabIndex = 17;
			this->labelDR020->Text = L"20";
			// 
			// textBoxDR20
			// 
			this->textBoxDR20->Location = System::Drawing::Point(48, 149);
			this->textBoxDR20->Name = L"textBoxDR20";
			this->textBoxDR20->Size = System::Drawing::Size(24, 20);
			this->textBoxDR20->TabIndex = 46;
			this->textBoxDR20->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR21
			// 
			this->textBoxDR21->Location = System::Drawing::Point(76, 149);
			this->textBoxDR21->Name = L"textBoxDR21";
			this->textBoxDR21->Size = System::Drawing::Size(24, 20);
			this->textBoxDR21->TabIndex = 47;
			this->textBoxDR21->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR22
			// 
			this->textBoxDR22->Location = System::Drawing::Point(104, 149);
			this->textBoxDR22->Name = L"textBoxDR22";
			this->textBoxDR22->Size = System::Drawing::Size(24, 20);
			this->textBoxDR22->TabIndex = 48;
			this->textBoxDR22->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR23
			// 
			this->textBoxDR23->Location = System::Drawing::Point(133, 149);
			this->textBoxDR23->Name = L"textBoxDR23";
			this->textBoxDR23->Size = System::Drawing::Size(24, 20);
			this->textBoxDR23->TabIndex = 49;
			this->textBoxDR23->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR24
			// 
			this->textBoxDR24->Location = System::Drawing::Point(162, 149);
			this->textBoxDR24->Name = L"textBoxDR24";
			this->textBoxDR24->Size = System::Drawing::Size(24, 20);
			this->textBoxDR24->TabIndex = 50;
			this->textBoxDR24->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR25
			// 
			this->textBoxDR25->Location = System::Drawing::Point(191, 149);
			this->textBoxDR25->Name = L"textBoxDR25";
			this->textBoxDR25->Size = System::Drawing::Size(24, 20);
			this->textBoxDR25->TabIndex = 51;
			this->textBoxDR25->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR26
			// 
			this->textBoxDR26->Location = System::Drawing::Point(220, 149);
			this->textBoxDR26->Name = L"textBoxDR26";
			this->textBoxDR26->Size = System::Drawing::Size(24, 20);
			this->textBoxDR26->TabIndex = 52;
			this->textBoxDR26->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR27
			// 
			this->textBoxDR27->Location = System::Drawing::Point(249, 149);
			this->textBoxDR27->Name = L"textBoxDR27";
			this->textBoxDR27->Size = System::Drawing::Size(24, 20);
			this->textBoxDR27->TabIndex = 53;
			this->textBoxDR27->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR28
			// 
			this->textBoxDR28->Location = System::Drawing::Point(278, 149);
			this->textBoxDR28->Name = L"textBoxDR28";
			this->textBoxDR28->Size = System::Drawing::Size(24, 20);
			this->textBoxDR28->TabIndex = 54;
			this->textBoxDR28->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR29
			// 
			this->textBoxDR29->Location = System::Drawing::Point(307, 149);
			this->textBoxDR29->Name = L"textBoxDR29";
			this->textBoxDR29->Size = System::Drawing::Size(24, 20);
			this->textBoxDR29->TabIndex = 55;
			this->textBoxDR29->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR030
			// 
			this->labelDR030->AutoSize = true;
			this->labelDR030->Location = System::Drawing::Point(16, 180);
			this->labelDR030->Name = L"labelDR030";
			this->labelDR030->Size = System::Drawing::Size(19, 13);
			this->labelDR030->TabIndex = 18;
			this->labelDR030->Text = L"30";
			// 
			// textBoxDR30
			// 
			this->textBoxDR30->Location = System::Drawing::Point(48, 177);
			this->textBoxDR30->Name = L"textBoxDR30";
			this->textBoxDR30->Size = System::Drawing::Size(24, 20);
			this->textBoxDR30->TabIndex = 56;
			this->textBoxDR30->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR31
			// 
			this->textBoxDR31->Location = System::Drawing::Point(76, 177);
			this->textBoxDR31->Name = L"textBoxDR31";
			this->textBoxDR31->Size = System::Drawing::Size(24, 20);
			this->textBoxDR31->TabIndex = 57;
			this->textBoxDR31->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR32
			// 
			this->textBoxDR32->Location = System::Drawing::Point(104, 177);
			this->textBoxDR32->Name = L"textBoxDR32";
			this->textBoxDR32->Size = System::Drawing::Size(24, 20);
			this->textBoxDR32->TabIndex = 58;
			this->textBoxDR32->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR33
			// 
			this->textBoxDR33->Location = System::Drawing::Point(133, 177);
			this->textBoxDR33->Name = L"textBoxDR33";
			this->textBoxDR33->Size = System::Drawing::Size(24, 20);
			this->textBoxDR33->TabIndex = 59;
			this->textBoxDR33->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR34
			// 
			this->textBoxDR34->Location = System::Drawing::Point(162, 177);
			this->textBoxDR34->Name = L"textBoxDR34";
			this->textBoxDR34->Size = System::Drawing::Size(24, 20);
			this->textBoxDR34->TabIndex = 60;
			this->textBoxDR34->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR35
			// 
			this->textBoxDR35->Location = System::Drawing::Point(191, 177);
			this->textBoxDR35->Name = L"textBoxDR35";
			this->textBoxDR35->Size = System::Drawing::Size(24, 20);
			this->textBoxDR35->TabIndex = 61;
			this->textBoxDR35->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR36
			// 
			this->textBoxDR36->Location = System::Drawing::Point(220, 177);
			this->textBoxDR36->Name = L"textBoxDR36";
			this->textBoxDR36->Size = System::Drawing::Size(24, 20);
			this->textBoxDR36->TabIndex = 62;
			this->textBoxDR36->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR37
			// 
			this->textBoxDR37->Location = System::Drawing::Point(249, 177);
			this->textBoxDR37->Name = L"textBoxDR37";
			this->textBoxDR37->Size = System::Drawing::Size(24, 20);
			this->textBoxDR37->TabIndex = 63;
			this->textBoxDR37->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR38
			// 
			this->textBoxDR38->Location = System::Drawing::Point(278, 177);
			this->textBoxDR38->Name = L"textBoxDR38";
			this->textBoxDR38->Size = System::Drawing::Size(24, 20);
			this->textBoxDR38->TabIndex = 64;
			this->textBoxDR38->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR39
			// 
			this->textBoxDR39->Location = System::Drawing::Point(307, 177);
			this->textBoxDR39->Name = L"textBoxDR39";
			this->textBoxDR39->Size = System::Drawing::Size(24, 20);
			this->textBoxDR39->TabIndex = 65;
			this->textBoxDR39->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR040
			// 
			this->labelDR040->AutoSize = true;
			this->labelDR040->Location = System::Drawing::Point(16, 208);
			this->labelDR040->Name = L"labelDR040";
			this->labelDR040->Size = System::Drawing::Size(19, 13);
			this->labelDR040->TabIndex = 19;
			this->labelDR040->Text = L"40";
			// 
			// textBoxDR40
			// 
			this->textBoxDR40->Location = System::Drawing::Point(48, 205);
			this->textBoxDR40->Name = L"textBoxDR40";
			this->textBoxDR40->Size = System::Drawing::Size(24, 20);
			this->textBoxDR40->TabIndex = 66;
			this->textBoxDR40->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR41
			// 
			this->textBoxDR41->Location = System::Drawing::Point(76, 205);
			this->textBoxDR41->Name = L"textBoxDR41";
			this->textBoxDR41->Size = System::Drawing::Size(24, 20);
			this->textBoxDR41->TabIndex = 67;
			this->textBoxDR41->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR42
			// 
			this->textBoxDR42->Location = System::Drawing::Point(104, 205);
			this->textBoxDR42->Name = L"textBoxDR42";
			this->textBoxDR42->Size = System::Drawing::Size(24, 20);
			this->textBoxDR42->TabIndex = 68;
			this->textBoxDR42->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR43
			// 
			this->textBoxDR43->Location = System::Drawing::Point(133, 205);
			this->textBoxDR43->Name = L"textBoxDR43";
			this->textBoxDR43->Size = System::Drawing::Size(24, 20);
			this->textBoxDR43->TabIndex = 69;
			this->textBoxDR43->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR44
			// 
			this->textBoxDR44->Location = System::Drawing::Point(162, 205);
			this->textBoxDR44->Name = L"textBoxDR44";
			this->textBoxDR44->Size = System::Drawing::Size(24, 20);
			this->textBoxDR44->TabIndex = 70;
			this->textBoxDR44->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR45
			// 
			this->textBoxDR45->Location = System::Drawing::Point(191, 205);
			this->textBoxDR45->Name = L"textBoxDR45";
			this->textBoxDR45->Size = System::Drawing::Size(24, 20);
			this->textBoxDR45->TabIndex = 71;
			this->textBoxDR45->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR46
			// 
			this->textBoxDR46->Location = System::Drawing::Point(220, 205);
			this->textBoxDR46->Name = L"textBoxDR46";
			this->textBoxDR46->Size = System::Drawing::Size(24, 20);
			this->textBoxDR46->TabIndex = 72;
			this->textBoxDR46->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR47
			// 
			this->textBoxDR47->Location = System::Drawing::Point(249, 205);
			this->textBoxDR47->Name = L"textBoxDR47";
			this->textBoxDR47->Size = System::Drawing::Size(24, 20);
			this->textBoxDR47->TabIndex = 73;
			this->textBoxDR47->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR48
			// 
			this->textBoxDR48->Location = System::Drawing::Point(278, 205);
			this->textBoxDR48->Name = L"textBoxDR48";
			this->textBoxDR48->Size = System::Drawing::Size(24, 20);
			this->textBoxDR48->TabIndex = 74;
			this->textBoxDR48->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR49
			// 
			this->textBoxDR49->Location = System::Drawing::Point(307, 205);
			this->textBoxDR49->Name = L"textBoxDR49";
			this->textBoxDR49->Size = System::Drawing::Size(24, 20);
			this->textBoxDR49->TabIndex = 75;
			this->textBoxDR49->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR050
			// 
			this->labelDR050->AutoSize = true;
			this->labelDR050->Location = System::Drawing::Point(16, 236);
			this->labelDR050->Name = L"labelDR050";
			this->labelDR050->Size = System::Drawing::Size(19, 13);
			this->labelDR050->TabIndex = 20;
			this->labelDR050->Text = L"50";
			// 
			// textBoxDR50
			// 
			this->textBoxDR50->Location = System::Drawing::Point(48, 233);
			this->textBoxDR50->Name = L"textBoxDR50";
			this->textBoxDR50->Size = System::Drawing::Size(24, 20);
			this->textBoxDR50->TabIndex = 76;
			this->textBoxDR50->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR51
			// 
			this->textBoxDR51->Location = System::Drawing::Point(76, 233);
			this->textBoxDR51->Name = L"textBoxDR51";
			this->textBoxDR51->Size = System::Drawing::Size(24, 20);
			this->textBoxDR51->TabIndex = 77;
			this->textBoxDR51->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR52
			// 
			this->textBoxDR52->Location = System::Drawing::Point(104, 233);
			this->textBoxDR52->Name = L"textBoxDR52";
			this->textBoxDR52->Size = System::Drawing::Size(24, 20);
			this->textBoxDR52->TabIndex = 78;
			this->textBoxDR52->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR53
			// 
			this->textBoxDR53->Location = System::Drawing::Point(133, 233);
			this->textBoxDR53->Name = L"textBoxDR53";
			this->textBoxDR53->Size = System::Drawing::Size(24, 20);
			this->textBoxDR53->TabIndex = 79;
			this->textBoxDR53->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR54
			// 
			this->textBoxDR54->Location = System::Drawing::Point(162, 233);
			this->textBoxDR54->Name = L"textBoxDR54";
			this->textBoxDR54->Size = System::Drawing::Size(24, 20);
			this->textBoxDR54->TabIndex = 80;
			this->textBoxDR54->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR55
			// 
			this->textBoxDR55->Location = System::Drawing::Point(191, 233);
			this->textBoxDR55->Name = L"textBoxDR55";
			this->textBoxDR55->Size = System::Drawing::Size(24, 20);
			this->textBoxDR55->TabIndex = 81;
			this->textBoxDR55->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR56
			// 
			this->textBoxDR56->Location = System::Drawing::Point(220, 233);
			this->textBoxDR56->Name = L"textBoxDR56";
			this->textBoxDR56->Size = System::Drawing::Size(24, 20);
			this->textBoxDR56->TabIndex = 82;
			this->textBoxDR56->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR57
			// 
			this->textBoxDR57->Location = System::Drawing::Point(249, 233);
			this->textBoxDR57->Name = L"textBoxDR57";
			this->textBoxDR57->Size = System::Drawing::Size(24, 20);
			this->textBoxDR57->TabIndex = 83;
			this->textBoxDR57->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR58
			// 
			this->textBoxDR58->Location = System::Drawing::Point(278, 233);
			this->textBoxDR58->Name = L"textBoxDR58";
			this->textBoxDR58->Size = System::Drawing::Size(24, 20);
			this->textBoxDR58->TabIndex = 84;
			this->textBoxDR58->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR59
			// 
			this->textBoxDR59->Location = System::Drawing::Point(307, 233);
			this->textBoxDR59->Name = L"textBoxDR59";
			this->textBoxDR59->Size = System::Drawing::Size(24, 20);
			this->textBoxDR59->TabIndex = 85;
			this->textBoxDR59->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR060
			// 
			this->labelDR060->AutoSize = true;
			this->labelDR060->Location = System::Drawing::Point(16, 265);
			this->labelDR060->Name = L"labelDR060";
			this->labelDR060->Size = System::Drawing::Size(19, 13);
			this->labelDR060->TabIndex = 21;
			this->labelDR060->Text = L"60";
			// 
			// textBoxDR60
			// 
			this->textBoxDR60->Location = System::Drawing::Point(48, 262);
			this->textBoxDR60->Name = L"textBoxDR60";
			this->textBoxDR60->Size = System::Drawing::Size(24, 20);
			this->textBoxDR60->TabIndex = 86;
			this->textBoxDR60->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR61
			// 
			this->textBoxDR61->Location = System::Drawing::Point(76, 262);
			this->textBoxDR61->Name = L"textBoxDR61";
			this->textBoxDR61->Size = System::Drawing::Size(24, 20);
			this->textBoxDR61->TabIndex = 87;
			this->textBoxDR61->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR62
			// 
			this->textBoxDR62->Location = System::Drawing::Point(104, 262);
			this->textBoxDR62->Name = L"textBoxDR62";
			this->textBoxDR62->Size = System::Drawing::Size(24, 20);
			this->textBoxDR62->TabIndex = 88;
			this->textBoxDR62->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR63
			// 
			this->textBoxDR63->Location = System::Drawing::Point(133, 262);
			this->textBoxDR63->Name = L"textBoxDR63";
			this->textBoxDR63->Size = System::Drawing::Size(24, 20);
			this->textBoxDR63->TabIndex = 89;
			this->textBoxDR63->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR64
			// 
			this->textBoxDR64->Location = System::Drawing::Point(162, 262);
			this->textBoxDR64->Name = L"textBoxDR64";
			this->textBoxDR64->Size = System::Drawing::Size(24, 20);
			this->textBoxDR64->TabIndex = 90;
			this->textBoxDR64->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR65
			// 
			this->textBoxDR65->Location = System::Drawing::Point(191, 262);
			this->textBoxDR65->Name = L"textBoxDR65";
			this->textBoxDR65->Size = System::Drawing::Size(24, 20);
			this->textBoxDR65->TabIndex = 91;
			this->textBoxDR65->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR66
			// 
			this->textBoxDR66->Location = System::Drawing::Point(220, 262);
			this->textBoxDR66->Name = L"textBoxDR66";
			this->textBoxDR66->Size = System::Drawing::Size(24, 20);
			this->textBoxDR66->TabIndex = 92;
			this->textBoxDR66->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR67
			// 
			this->textBoxDR67->Location = System::Drawing::Point(249, 262);
			this->textBoxDR67->Name = L"textBoxDR67";
			this->textBoxDR67->Size = System::Drawing::Size(24, 20);
			this->textBoxDR67->TabIndex = 93;
			this->textBoxDR67->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR68
			// 
			this->textBoxDR68->Location = System::Drawing::Point(278, 262);
			this->textBoxDR68->Name = L"textBoxDR68";
			this->textBoxDR68->Size = System::Drawing::Size(24, 20);
			this->textBoxDR68->TabIndex = 94;
			this->textBoxDR68->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR69
			// 
			this->textBoxDR69->Location = System::Drawing::Point(307, 262);
			this->textBoxDR69->Name = L"textBoxDR69";
			this->textBoxDR69->Size = System::Drawing::Size(24, 20);
			this->textBoxDR69->TabIndex = 95;
			this->textBoxDR69->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR070
			// 
			this->labelDR070->AutoSize = true;
			this->labelDR070->Location = System::Drawing::Point(16, 293);
			this->labelDR070->Name = L"labelDR070";
			this->labelDR070->Size = System::Drawing::Size(19, 13);
			this->labelDR070->TabIndex = 22;
			this->labelDR070->Text = L"70";
			// 
			// textBoxDR70
			// 
			this->textBoxDR70->Location = System::Drawing::Point(48, 291);
			this->textBoxDR70->Name = L"textBoxDR70";
			this->textBoxDR70->Size = System::Drawing::Size(24, 20);
			this->textBoxDR70->TabIndex = 96;
			this->textBoxDR70->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR71
			// 
			this->textBoxDR71->Location = System::Drawing::Point(76, 291);
			this->textBoxDR71->Name = L"textBoxDR71";
			this->textBoxDR71->Size = System::Drawing::Size(24, 20);
			this->textBoxDR71->TabIndex = 97;
			this->textBoxDR71->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR72
			// 
			this->textBoxDR72->Location = System::Drawing::Point(104, 291);
			this->textBoxDR72->Name = L"textBoxDR72";
			this->textBoxDR72->Size = System::Drawing::Size(24, 20);
			this->textBoxDR72->TabIndex = 98;
			this->textBoxDR72->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR73
			// 
			this->textBoxDR73->Location = System::Drawing::Point(133, 291);
			this->textBoxDR73->Name = L"textBoxDR73";
			this->textBoxDR73->Size = System::Drawing::Size(24, 20);
			this->textBoxDR73->TabIndex = 99;
			this->textBoxDR73->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR74
			// 
			this->textBoxDR74->Location = System::Drawing::Point(162, 291);
			this->textBoxDR74->Name = L"textBoxDR74";
			this->textBoxDR74->Size = System::Drawing::Size(24, 20);
			this->textBoxDR74->TabIndex = 100;
			this->textBoxDR74->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR75
			// 
			this->textBoxDR75->Location = System::Drawing::Point(191, 291);
			this->textBoxDR75->Name = L"textBoxDR75";
			this->textBoxDR75->Size = System::Drawing::Size(24, 20);
			this->textBoxDR75->TabIndex = 101;
			this->textBoxDR75->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR76
			// 
			this->textBoxDR76->Location = System::Drawing::Point(220, 291);
			this->textBoxDR76->Name = L"textBoxDR76";
			this->textBoxDR76->Size = System::Drawing::Size(24, 20);
			this->textBoxDR76->TabIndex = 102;
			this->textBoxDR76->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR77
			// 
			this->textBoxDR77->Location = System::Drawing::Point(249, 291);
			this->textBoxDR77->Name = L"textBoxDR77";
			this->textBoxDR77->Size = System::Drawing::Size(24, 20);
			this->textBoxDR77->TabIndex = 103;
			this->textBoxDR77->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR78
			// 
			this->textBoxDR78->Location = System::Drawing::Point(278, 291);
			this->textBoxDR78->Name = L"textBoxDR78";
			this->textBoxDR78->Size = System::Drawing::Size(24, 20);
			this->textBoxDR78->TabIndex = 104;
			this->textBoxDR78->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR79
			// 
			this->textBoxDR79->Location = System::Drawing::Point(307, 291);
			this->textBoxDR79->Name = L"textBoxDR79";
			this->textBoxDR79->Size = System::Drawing::Size(24, 20);
			this->textBoxDR79->TabIndex = 105;
			this->textBoxDR79->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR080
			// 
			this->labelDR080->AutoSize = true;
			this->labelDR080->Location = System::Drawing::Point(16, 321);
			this->labelDR080->Name = L"labelDR080";
			this->labelDR080->Size = System::Drawing::Size(19, 13);
			this->labelDR080->TabIndex = 23;
			this->labelDR080->Text = L"80";
			// 
			// textBoxDR80
			// 
			this->textBoxDR80->Location = System::Drawing::Point(48, 319);
			this->textBoxDR80->Name = L"textBoxDR80";
			this->textBoxDR80->Size = System::Drawing::Size(24, 20);
			this->textBoxDR80->TabIndex = 106;
			this->textBoxDR80->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR81
			// 
			this->textBoxDR81->Location = System::Drawing::Point(76, 319);
			this->textBoxDR81->Name = L"textBoxDR81";
			this->textBoxDR81->Size = System::Drawing::Size(24, 20);
			this->textBoxDR81->TabIndex = 107;
			this->textBoxDR81->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR82
			// 
			this->textBoxDR82->Location = System::Drawing::Point(104, 319);
			this->textBoxDR82->Name = L"textBoxDR82";
			this->textBoxDR82->Size = System::Drawing::Size(24, 20);
			this->textBoxDR82->TabIndex = 108;
			this->textBoxDR82->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR83
			// 
			this->textBoxDR83->Location = System::Drawing::Point(133, 319);
			this->textBoxDR83->Name = L"textBoxDR83";
			this->textBoxDR83->Size = System::Drawing::Size(24, 20);
			this->textBoxDR83->TabIndex = 109;
			this->textBoxDR83->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR84
			// 
			this->textBoxDR84->Location = System::Drawing::Point(162, 319);
			this->textBoxDR84->Name = L"textBoxDR84";
			this->textBoxDR84->Size = System::Drawing::Size(24, 20);
			this->textBoxDR84->TabIndex = 110;
			this->textBoxDR84->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR85
			// 
			this->textBoxDR85->Location = System::Drawing::Point(191, 319);
			this->textBoxDR85->Name = L"textBoxDR85";
			this->textBoxDR85->Size = System::Drawing::Size(24, 20);
			this->textBoxDR85->TabIndex = 111;
			this->textBoxDR85->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR86
			// 
			this->textBoxDR86->Location = System::Drawing::Point(220, 319);
			this->textBoxDR86->Name = L"textBoxDR86";
			this->textBoxDR86->Size = System::Drawing::Size(24, 20);
			this->textBoxDR86->TabIndex = 112;
			this->textBoxDR86->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR87
			// 
			this->textBoxDR87->Location = System::Drawing::Point(249, 319);
			this->textBoxDR87->Name = L"textBoxDR87";
			this->textBoxDR87->Size = System::Drawing::Size(24, 20);
			this->textBoxDR87->TabIndex = 113;
			this->textBoxDR87->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR88
			// 
			this->textBoxDR88->Location = System::Drawing::Point(278, 319);
			this->textBoxDR88->Name = L"textBoxDR88";
			this->textBoxDR88->Size = System::Drawing::Size(24, 20);
			this->textBoxDR88->TabIndex = 114;
			this->textBoxDR88->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR89
			// 
			this->textBoxDR89->Location = System::Drawing::Point(307, 319);
			this->textBoxDR89->Name = L"textBoxDR89";
			this->textBoxDR89->Size = System::Drawing::Size(24, 20);
			this->textBoxDR89->TabIndex = 115;
			this->textBoxDR89->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR090
			// 
			this->labelDR090->AutoSize = true;
			this->labelDR090->Location = System::Drawing::Point(16, 350);
			this->labelDR090->Name = L"labelDR090";
			this->labelDR090->Size = System::Drawing::Size(19, 13);
			this->labelDR090->TabIndex = 24;
			this->labelDR090->Text = L"90";
			// 
			// textBoxDR90
			// 
			this->textBoxDR90->Location = System::Drawing::Point(48, 348);
			this->textBoxDR90->Name = L"textBoxDR90";
			this->textBoxDR90->Size = System::Drawing::Size(24, 20);
			this->textBoxDR90->TabIndex = 116;
			this->textBoxDR90->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR91
			// 
			this->textBoxDR91->Location = System::Drawing::Point(76, 348);
			this->textBoxDR91->Name = L"textBoxDR91";
			this->textBoxDR91->Size = System::Drawing::Size(24, 20);
			this->textBoxDR91->TabIndex = 117;
			this->textBoxDR91->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR92
			// 
			this->textBoxDR92->Location = System::Drawing::Point(104, 348);
			this->textBoxDR92->Name = L"textBoxDR92";
			this->textBoxDR92->Size = System::Drawing::Size(24, 20);
			this->textBoxDR92->TabIndex = 118;
			this->textBoxDR92->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR93
			// 
			this->textBoxDR93->Location = System::Drawing::Point(133, 348);
			this->textBoxDR93->Name = L"textBoxDR93";
			this->textBoxDR93->Size = System::Drawing::Size(24, 20);
			this->textBoxDR93->TabIndex = 119;
			this->textBoxDR93->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR94
			// 
			this->textBoxDR94->Location = System::Drawing::Point(162, 348);
			this->textBoxDR94->Name = L"textBoxDR94";
			this->textBoxDR94->Size = System::Drawing::Size(24, 20);
			this->textBoxDR94->TabIndex = 120;
			this->textBoxDR94->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR95
			// 
			this->textBoxDR95->Location = System::Drawing::Point(191, 348);
			this->textBoxDR95->Name = L"textBoxDR95";
			this->textBoxDR95->Size = System::Drawing::Size(24, 20);
			this->textBoxDR95->TabIndex = 121;
			this->textBoxDR95->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR96
			// 
			this->textBoxDR96->Location = System::Drawing::Point(220, 348);
			this->textBoxDR96->Name = L"textBoxDR96";
			this->textBoxDR96->Size = System::Drawing::Size(24, 20);
			this->textBoxDR96->TabIndex = 122;
			this->textBoxDR96->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR97
			// 
			this->textBoxDR97->Location = System::Drawing::Point(249, 348);
			this->textBoxDR97->Name = L"textBoxDR97";
			this->textBoxDR97->Size = System::Drawing::Size(24, 20);
			this->textBoxDR97->TabIndex = 123;
			this->textBoxDR97->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR98
			// 
			this->textBoxDR98->Location = System::Drawing::Point(278, 348);
			this->textBoxDR98->Name = L"textBoxDR98";
			this->textBoxDR98->Size = System::Drawing::Size(24, 20);
			this->textBoxDR98->TabIndex = 124;
			this->textBoxDR98->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR99
			// 
			this->textBoxDR99->Location = System::Drawing::Point(307, 348);
			this->textBoxDR99->Name = L"textBoxDR99";
			this->textBoxDR99->Size = System::Drawing::Size(24, 20);
			this->textBoxDR99->TabIndex = 125;
			this->textBoxDR99->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelDR100
			// 
			this->labelDR100->AutoSize = true;
			this->labelDR100->Location = System::Drawing::Point(11, 381);
			this->labelDR100->Name = L"labelDR100";
			this->labelDR100->Size = System::Drawing::Size(25, 13);
			this->labelDR100->TabIndex = 25;
			this->labelDR100->Text = L"100";
			// 
			// textBoxDR100
			// 
			this->textBoxDR100->Location = System::Drawing::Point(48, 378);
			this->textBoxDR100->Name = L"textBoxDR100";
			this->textBoxDR100->Size = System::Drawing::Size(24, 20);
			this->textBoxDR100->TabIndex = 126;
			this->textBoxDR100->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR101
			// 
			this->textBoxDR101->Location = System::Drawing::Point(76, 378);
			this->textBoxDR101->Name = L"textBoxDR101";
			this->textBoxDR101->Size = System::Drawing::Size(24, 20);
			this->textBoxDR101->TabIndex = 127;
			this->textBoxDR101->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR102
			// 
			this->textBoxDR102->Location = System::Drawing::Point(104, 378);
			this->textBoxDR102->Name = L"textBoxDR102";
			this->textBoxDR102->Size = System::Drawing::Size(24, 20);
			this->textBoxDR102->TabIndex = 128;
			this->textBoxDR102->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR103
			// 
			this->textBoxDR103->Location = System::Drawing::Point(133, 378);
			this->textBoxDR103->Name = L"textBoxDR103";
			this->textBoxDR103->Size = System::Drawing::Size(24, 20);
			this->textBoxDR103->TabIndex = 129;
			this->textBoxDR103->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR104
			// 
			this->textBoxDR104->Location = System::Drawing::Point(162, 378);
			this->textBoxDR104->Name = L"textBoxDR104";
			this->textBoxDR104->Size = System::Drawing::Size(24, 20);
			this->textBoxDR104->TabIndex = 130;
			this->textBoxDR104->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR105
			// 
			this->textBoxDR105->Location = System::Drawing::Point(191, 378);
			this->textBoxDR105->Name = L"textBoxDR105";
			this->textBoxDR105->Size = System::Drawing::Size(24, 20);
			this->textBoxDR105->TabIndex = 131;
			this->textBoxDR105->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR106
			// 
			this->textBoxDR106->Location = System::Drawing::Point(220, 378);
			this->textBoxDR106->Name = L"textBoxDR106";
			this->textBoxDR106->Size = System::Drawing::Size(24, 20);
			this->textBoxDR106->TabIndex = 132;
			this->textBoxDR106->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR107
			// 
			this->textBoxDR107->Location = System::Drawing::Point(249, 378);
			this->textBoxDR107->Name = L"textBoxDR107";
			this->textBoxDR107->Size = System::Drawing::Size(24, 20);
			this->textBoxDR107->TabIndex = 133;
			this->textBoxDR107->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR108
			// 
			this->textBoxDR108->Location = System::Drawing::Point(278, 378);
			this->textBoxDR108->Name = L"textBoxDR108";
			this->textBoxDR108->Size = System::Drawing::Size(24, 20);
			this->textBoxDR108->TabIndex = 134;
			this->textBoxDR108->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxDR109
			// 
			this->textBoxDR109->Location = System::Drawing::Point(307, 378);
			this->textBoxDR109->Name = L"textBoxDR109";
			this->textBoxDR109->Size = System::Drawing::Size(24, 20);
			this->textBoxDR109->TabIndex = 135;
			this->textBoxDR109->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// labelIR0
			// 
			this->labelIR0->AutoSize = true;
			this->labelIR0->Location = System::Drawing::Point(353, 75);
			this->labelIR0->Name = L"labelIR0";
			this->labelIR0->Size = System::Drawing::Size(13, 13);
			this->labelIR0->TabIndex = 147;
			this->labelIR0->Text = L"0";
			// 
			// labelIR1
			// 
			this->labelIR1->AutoSize = true;
			this->labelIR1->Location = System::Drawing::Point(390, 75);
			this->labelIR1->Name = L"labelIR1";
			this->labelIR1->Size = System::Drawing::Size(13, 13);
			this->labelIR1->TabIndex = 148;
			this->labelIR1->Text = L"1";
			// 
			// labelIR2
			// 
			this->labelIR2->AutoSize = true;
			this->labelIR2->Location = System::Drawing::Point(429, 75);
			this->labelIR2->Name = L"labelIR2";
			this->labelIR2->Size = System::Drawing::Size(13, 13);
			this->labelIR2->TabIndex = 149;
			this->labelIR2->Text = L"2";
			// 
			// labelIR3
			// 
			this->labelIR3->AutoSize = true;
			this->labelIR3->Location = System::Drawing::Point(466, 75);
			this->labelIR3->Name = L"labelIR3";
			this->labelIR3->Size = System::Drawing::Size(13, 13);
			this->labelIR3->TabIndex = 150;
			this->labelIR3->Text = L"3";
			// 
			// labelIR4
			// 
			this->labelIR4->AutoSize = true;
			this->labelIR4->Location = System::Drawing::Point(504, 75);
			this->labelIR4->Name = L"labelIR4";
			this->labelIR4->Size = System::Drawing::Size(13, 13);
			this->labelIR4->TabIndex = 151;
			this->labelIR4->Text = L"4";
			// 
			// labelIR5
			// 
			this->labelIR5->AutoSize = true;
			this->labelIR5->Location = System::Drawing::Point(542, 75);
			this->labelIR5->Name = L"labelIR5";
			this->labelIR5->Size = System::Drawing::Size(13, 13);
			this->labelIR5->TabIndex = 152;
			this->labelIR5->Text = L"5";
			// 
			// labelIR6
			// 
			this->labelIR6->AutoSize = true;
			this->labelIR6->Location = System::Drawing::Point(581, 75);
			this->labelIR6->Name = L"labelIR6";
			this->labelIR6->Size = System::Drawing::Size(13, 13);
			this->labelIR6->TabIndex = 153;
			this->labelIR6->Text = L"6";
			// 
			// labelIR7
			// 
			this->labelIR7->AutoSize = true;
			this->labelIR7->Location = System::Drawing::Point(618, 75);
			this->labelIR7->Name = L"labelIR7";
			this->labelIR7->Size = System::Drawing::Size(13, 13);
			this->labelIR7->TabIndex = 154;
			this->labelIR7->Text = L"7";
			// 
			// labelIR8
			// 
			this->labelIR8->AutoSize = true;
			this->labelIR8->Location = System::Drawing::Point(658, 75);
			this->labelIR8->Name = L"labelIR8";
			this->labelIR8->Size = System::Drawing::Size(13, 13);
			this->labelIR8->TabIndex = 155;
			this->labelIR8->Text = L"8";
			// 
			// labelIR9
			// 
			this->labelIR9->AutoSize = true;
			this->labelIR9->Location = System::Drawing::Point(696, 75);
			this->labelIR9->Name = L"labelIR9";
			this->labelIR9->Size = System::Drawing::Size(13, 13);
			this->labelIR9->TabIndex = 156;
			this->labelIR9->Text = L"9";
			// 
			// textBoxIR0
			// 
			this->textBoxIR0->Location = System::Drawing::Point(342, 95);
			this->textBoxIR0->Name = L"textBoxIR0";
			this->textBoxIR0->Size = System::Drawing::Size(35, 20);
			this->textBoxIR0->TabIndex = 146;
			this->textBoxIR0->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR1
			// 
			this->textBoxIR1->Location = System::Drawing::Point(380, 95);
			this->textBoxIR1->Name = L"textBoxIR1";
			this->textBoxIR1->Size = System::Drawing::Size(35, 20);
			this->textBoxIR1->TabIndex = 157;
			this->textBoxIR1->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR2
			// 
			this->textBoxIR2->Location = System::Drawing::Point(418, 95);
			this->textBoxIR2->Name = L"textBoxIR2";
			this->textBoxIR2->Size = System::Drawing::Size(35, 20);
			this->textBoxIR2->TabIndex = 158;
			this->textBoxIR2->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR3
			// 
			this->textBoxIR3->Location = System::Drawing::Point(456, 95);
			this->textBoxIR3->Name = L"textBoxIR3";
			this->textBoxIR3->Size = System::Drawing::Size(35, 20);
			this->textBoxIR3->TabIndex = 159;
			this->textBoxIR3->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR4
			// 
			this->textBoxIR4->Location = System::Drawing::Point(494, 95);
			this->textBoxIR4->Name = L"textBoxIR4";
			this->textBoxIR4->Size = System::Drawing::Size(35, 20);
			this->textBoxIR4->TabIndex = 160;
			this->textBoxIR4->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR5
			// 
			this->textBoxIR5->Location = System::Drawing::Point(532, 95);
			this->textBoxIR5->Name = L"textBoxIR5";
			this->textBoxIR5->Size = System::Drawing::Size(35, 20);
			this->textBoxIR5->TabIndex = 161;
			this->textBoxIR5->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR6
			// 
			this->textBoxIR6->Location = System::Drawing::Point(570, 95);
			this->textBoxIR6->Name = L"textBoxIR6";
			this->textBoxIR6->Size = System::Drawing::Size(35, 20);
			this->textBoxIR6->TabIndex = 162;
			this->textBoxIR6->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR7
			// 
			this->textBoxIR7->Location = System::Drawing::Point(608, 95);
			this->textBoxIR7->Name = L"textBoxIR7";
			this->textBoxIR7->Size = System::Drawing::Size(35, 20);
			this->textBoxIR7->TabIndex = 163;
			this->textBoxIR7->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR8
			// 
			this->textBoxIR8->Location = System::Drawing::Point(647, 95);
			this->textBoxIR8->Name = L"textBoxIR8";
			this->textBoxIR8->Size = System::Drawing::Size(35, 20);
			this->textBoxIR8->TabIndex = 164;
			this->textBoxIR8->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR9
			// 
			this->textBoxIR9->Location = System::Drawing::Point(686, 95);
			this->textBoxIR9->Name = L"textBoxIR9";
			this->textBoxIR9->Size = System::Drawing::Size(35, 20);
			this->textBoxIR9->TabIndex = 165;
			this->textBoxIR9->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR10
			// 
			this->textBoxIR10->Location = System::Drawing::Point(342, 122);
			this->textBoxIR10->Name = L"textBoxIR10";
			this->textBoxIR10->Size = System::Drawing::Size(35, 20);
			this->textBoxIR10->TabIndex = 166;
			this->textBoxIR10->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR11
			// 
			this->textBoxIR11->Location = System::Drawing::Point(380, 122);
			this->textBoxIR11->Name = L"textBoxIR11";
			this->textBoxIR11->Size = System::Drawing::Size(35, 20);
			this->textBoxIR11->TabIndex = 167;
			this->textBoxIR11->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR12
			// 
			this->textBoxIR12->Location = System::Drawing::Point(418, 122);
			this->textBoxIR12->Name = L"textBoxIR12";
			this->textBoxIR12->Size = System::Drawing::Size(35, 20);
			this->textBoxIR12->TabIndex = 168;
			this->textBoxIR12->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR13
			// 
			this->textBoxIR13->Location = System::Drawing::Point(456, 122);
			this->textBoxIR13->Name = L"textBoxIR13";
			this->textBoxIR13->Size = System::Drawing::Size(35, 20);
			this->textBoxIR13->TabIndex = 169;
			this->textBoxIR13->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR14
			// 
			this->textBoxIR14->Location = System::Drawing::Point(494, 122);
			this->textBoxIR14->Name = L"textBoxIR14";
			this->textBoxIR14->Size = System::Drawing::Size(35, 20);
			this->textBoxIR14->TabIndex = 170;
			this->textBoxIR14->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR15
			// 
			this->textBoxIR15->Location = System::Drawing::Point(532, 122);
			this->textBoxIR15->Name = L"textBoxIR15";
			this->textBoxIR15->Size = System::Drawing::Size(35, 20);
			this->textBoxIR15->TabIndex = 171;
			this->textBoxIR15->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR16
			// 
			this->textBoxIR16->Location = System::Drawing::Point(570, 122);
			this->textBoxIR16->Name = L"textBoxIR16";
			this->textBoxIR16->Size = System::Drawing::Size(35, 20);
			this->textBoxIR16->TabIndex = 172;
			this->textBoxIR16->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR17
			// 
			this->textBoxIR17->Location = System::Drawing::Point(608, 121);
			this->textBoxIR17->Name = L"textBoxIR17";
			this->textBoxIR17->Size = System::Drawing::Size(35, 20);
			this->textBoxIR17->TabIndex = 173;
			this->textBoxIR17->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR18
			// 
			this->textBoxIR18->Location = System::Drawing::Point(647, 121);
			this->textBoxIR18->Name = L"textBoxIR18";
			this->textBoxIR18->Size = System::Drawing::Size(35, 20);
			this->textBoxIR18->TabIndex = 174;
			this->textBoxIR18->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR19
			// 
			this->textBoxIR19->Location = System::Drawing::Point(686, 121);
			this->textBoxIR19->Name = L"textBoxIR19";
			this->textBoxIR19->Size = System::Drawing::Size(35, 20);
			this->textBoxIR19->TabIndex = 175;
			this->textBoxIR19->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR20
			// 
			this->textBoxIR20->Location = System::Drawing::Point(342, 149);
			this->textBoxIR20->Name = L"textBoxIR20";
			this->textBoxIR20->Size = System::Drawing::Size(35, 20);
			this->textBoxIR20->TabIndex = 176;
			this->textBoxIR20->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR21
			// 
			this->textBoxIR21->Location = System::Drawing::Point(380, 149);
			this->textBoxIR21->Name = L"textBoxIR21";
			this->textBoxIR21->Size = System::Drawing::Size(35, 20);
			this->textBoxIR21->TabIndex = 177;
			this->textBoxIR21->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR22
			// 
			this->textBoxIR22->Location = System::Drawing::Point(418, 149);
			this->textBoxIR22->Name = L"textBoxIR22";
			this->textBoxIR22->Size = System::Drawing::Size(35, 20);
			this->textBoxIR22->TabIndex = 178;
			this->textBoxIR22->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR23
			// 
			this->textBoxIR23->Location = System::Drawing::Point(456, 149);
			this->textBoxIR23->Name = L"textBoxIR23";
			this->textBoxIR23->Size = System::Drawing::Size(35, 20);
			this->textBoxIR23->TabIndex = 179;
			this->textBoxIR23->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR24
			// 
			this->textBoxIR24->Location = System::Drawing::Point(494, 149);
			this->textBoxIR24->Name = L"textBoxIR24";
			this->textBoxIR24->Size = System::Drawing::Size(35, 20);
			this->textBoxIR24->TabIndex = 180;
			this->textBoxIR24->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR25
			// 
			this->textBoxIR25->Location = System::Drawing::Point(532, 149);
			this->textBoxIR25->Name = L"textBoxIR25";
			this->textBoxIR25->Size = System::Drawing::Size(35, 20);
			this->textBoxIR25->TabIndex = 181;
			this->textBoxIR25->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR26
			// 
			this->textBoxIR26->Location = System::Drawing::Point(570, 149);
			this->textBoxIR26->Name = L"textBoxIR26";
			this->textBoxIR26->Size = System::Drawing::Size(35, 20);
			this->textBoxIR26->TabIndex = 182;
			this->textBoxIR26->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR27
			// 
			this->textBoxIR27->Location = System::Drawing::Point(608, 149);
			this->textBoxIR27->Name = L"textBoxIR27";
			this->textBoxIR27->Size = System::Drawing::Size(35, 20);
			this->textBoxIR27->TabIndex = 183;
			this->textBoxIR27->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR28
			// 
			this->textBoxIR28->Location = System::Drawing::Point(647, 149);
			this->textBoxIR28->Name = L"textBoxIR28";
			this->textBoxIR28->Size = System::Drawing::Size(35, 20);
			this->textBoxIR28->TabIndex = 184;
			this->textBoxIR28->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR29
			// 
			this->textBoxIR29->Location = System::Drawing::Point(686, 149);
			this->textBoxIR29->Name = L"textBoxIR29";
			this->textBoxIR29->Size = System::Drawing::Size(35, 20);
			this->textBoxIR29->TabIndex = 185;
			this->textBoxIR29->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR30
			// 
			this->textBoxIR30->Location = System::Drawing::Point(342, 177);
			this->textBoxIR30->Name = L"textBoxIR30";
			this->textBoxIR30->Size = System::Drawing::Size(35, 20);
			this->textBoxIR30->TabIndex = 186;
			this->textBoxIR30->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR31
			// 
			this->textBoxIR31->Location = System::Drawing::Point(380, 177);
			this->textBoxIR31->Name = L"textBoxIR31";
			this->textBoxIR31->Size = System::Drawing::Size(35, 20);
			this->textBoxIR31->TabIndex = 187;
			this->textBoxIR31->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR32
			// 
			this->textBoxIR32->Location = System::Drawing::Point(418, 177);
			this->textBoxIR32->Name = L"textBoxIR32";
			this->textBoxIR32->Size = System::Drawing::Size(35, 20);
			this->textBoxIR32->TabIndex = 188;
			this->textBoxIR32->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR33
			// 
			this->textBoxIR33->Location = System::Drawing::Point(456, 177);
			this->textBoxIR33->Name = L"textBoxIR33";
			this->textBoxIR33->Size = System::Drawing::Size(35, 20);
			this->textBoxIR33->TabIndex = 189;
			this->textBoxIR33->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR34
			// 
			this->textBoxIR34->Location = System::Drawing::Point(494, 177);
			this->textBoxIR34->Name = L"textBoxIR34";
			this->textBoxIR34->Size = System::Drawing::Size(35, 20);
			this->textBoxIR34->TabIndex = 190;
			this->textBoxIR34->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR35
			// 
			this->textBoxIR35->Location = System::Drawing::Point(532, 177);
			this->textBoxIR35->Name = L"textBoxIR35";
			this->textBoxIR35->Size = System::Drawing::Size(35, 20);
			this->textBoxIR35->TabIndex = 191;
			this->textBoxIR35->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR36
			// 
			this->textBoxIR36->Location = System::Drawing::Point(570, 177);
			this->textBoxIR36->Name = L"textBoxIR36";
			this->textBoxIR36->Size = System::Drawing::Size(35, 20);
			this->textBoxIR36->TabIndex = 192;
			this->textBoxIR36->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR37
			// 
			this->textBoxIR37->Location = System::Drawing::Point(608, 177);
			this->textBoxIR37->Name = L"textBoxIR37";
			this->textBoxIR37->Size = System::Drawing::Size(35, 20);
			this->textBoxIR37->TabIndex = 193;
			this->textBoxIR37->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR38
			// 
			this->textBoxIR38->Location = System::Drawing::Point(647, 177);
			this->textBoxIR38->Name = L"textBoxIR38";
			this->textBoxIR38->Size = System::Drawing::Size(35, 20);
			this->textBoxIR38->TabIndex = 194;
			this->textBoxIR38->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR39
			// 
			this->textBoxIR39->Location = System::Drawing::Point(686, 177);
			this->textBoxIR39->Name = L"textBoxIR39";
			this->textBoxIR39->Size = System::Drawing::Size(35, 20);
			this->textBoxIR39->TabIndex = 195;
			this->textBoxIR39->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR40
			// 
			this->textBoxIR40->Location = System::Drawing::Point(342, 205);
			this->textBoxIR40->Name = L"textBoxIR40";
			this->textBoxIR40->Size = System::Drawing::Size(35, 20);
			this->textBoxIR40->TabIndex = 196;
			this->textBoxIR40->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR41
			// 
			this->textBoxIR41->Location = System::Drawing::Point(380, 205);
			this->textBoxIR41->Name = L"textBoxIR41";
			this->textBoxIR41->Size = System::Drawing::Size(35, 20);
			this->textBoxIR41->TabIndex = 197;
			this->textBoxIR41->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR42
			// 
			this->textBoxIR42->Location = System::Drawing::Point(418, 205);
			this->textBoxIR42->Name = L"textBoxIR42";
			this->textBoxIR42->Size = System::Drawing::Size(35, 20);
			this->textBoxIR42->TabIndex = 198;
			this->textBoxIR42->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR43
			// 
			this->textBoxIR43->Location = System::Drawing::Point(456, 205);
			this->textBoxIR43->Name = L"textBoxIR43";
			this->textBoxIR43->Size = System::Drawing::Size(35, 20);
			this->textBoxIR43->TabIndex = 199;
			this->textBoxIR43->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR99
			// 
			this->textBoxIR99->Location = System::Drawing::Point(686, 347);
			this->textBoxIR99->Name = L"textBoxIR99";
			this->textBoxIR99->Size = System::Drawing::Size(35, 20);
			this->textBoxIR99->TabIndex = 200;
			this->textBoxIR99->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR100
			// 
			this->textBoxIR100->Location = System::Drawing::Point(342, 378);
			this->textBoxIR100->Name = L"textBoxIR100";
			this->textBoxIR100->Size = System::Drawing::Size(35, 20);
			this->textBoxIR100->TabIndex = 201;
			this->textBoxIR100->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR101
			// 
			this->textBoxIR101->Location = System::Drawing::Point(380, 378);
			this->textBoxIR101->Name = L"textBoxIR101";
			this->textBoxIR101->Size = System::Drawing::Size(35, 20);
			this->textBoxIR101->TabIndex = 202;
			this->textBoxIR101->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR102
			// 
			this->textBoxIR102->Location = System::Drawing::Point(418, 378);
			this->textBoxIR102->Name = L"textBoxIR102";
			this->textBoxIR102->Size = System::Drawing::Size(35, 20);
			this->textBoxIR102->TabIndex = 203;
			this->textBoxIR102->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR103
			// 
			this->textBoxIR103->Location = System::Drawing::Point(456, 378);
			this->textBoxIR103->Name = L"textBoxIR103";
			this->textBoxIR103->Size = System::Drawing::Size(35, 20);
			this->textBoxIR103->TabIndex = 204;
			this->textBoxIR103->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// textBoxIR104
			// 
			this->textBoxIR104->Location = System::Drawing::Point(494, 378);
			this->textBoxIR104->Name = L"textBoxIR104";
			this->textBoxIR104->Size = System::Drawing::Size(35, 20);
			this->textBoxIR104->TabIndex = 205;
			this->textBoxIR104->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
			// 
			// toolTipRegisterInfo
			// 
			this->toolTipRegisterInfo->AutoPopDelay = 10000;
			this->toolTipRegisterInfo->InitialDelay = 500;
			this->toolTipRegisterInfo->ReshowDelay = 100;
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(731, 425);
			this->Controls->Add(this->textBoxIR104);
			this->Controls->Add(this->textBoxIR103);
			this->Controls->Add(this->textBoxIR102);
			this->Controls->Add(this->textBoxIR101);
			this->Controls->Add(this->textBoxIR100);
			this->Controls->Add(this->textBoxIR99);
			this->Controls->Add(this->textBoxIR43);
			this->Controls->Add(this->textBoxIR42);
			this->Controls->Add(this->textBoxIR41);
			this->Controls->Add(this->textBoxIR40);
			this->Controls->Add(this->textBoxIR39);
			this->Controls->Add(this->textBoxIR38);
			this->Controls->Add(this->textBoxIR37);
			this->Controls->Add(this->textBoxIR36);
			this->Controls->Add(this->textBoxIR35);
			this->Controls->Add(this->textBoxIR34);
			this->Controls->Add(this->textBoxIR33);
			this->Controls->Add(this->textBoxIR32);
			this->Controls->Add(this->textBoxIR31);
			this->Controls->Add(this->textBoxIR30);
			this->Controls->Add(this->textBoxIR29);
			this->Controls->Add(this->textBoxIR28);
			this->Controls->Add(this->textBoxIR27);
			this->Controls->Add(this->textBoxIR26);
			this->Controls->Add(this->textBoxIR25);
			this->Controls->Add(this->textBoxIR24);
			this->Controls->Add(this->textBoxIR23);
			this->Controls->Add(this->textBoxIR22);
			this->Controls->Add(this->textBoxIR21);
			this->Controls->Add(this->textBoxIR20);
			this->Controls->Add(this->textBoxIR19);
			this->Controls->Add(this->textBoxIR18);
			this->Controls->Add(this->textBoxIR17);
			this->Controls->Add(this->textBoxIR16);
			this->Controls->Add(this->textBoxIR15);
			this->Controls->Add(this->textBoxIR14);
			this->Controls->Add(this->textBoxIR13);
			this->Controls->Add(this->textBoxIR12);
			this->Controls->Add(this->textBoxIR11);
			this->Controls->Add(this->textBoxIR10);
			this->Controls->Add(this->textBoxIR9);
			this->Controls->Add(this->textBoxIR8);
			this->Controls->Add(this->textBoxIR7);
			this->Controls->Add(this->textBoxIR6);
			this->Controls->Add(this->textBoxIR5);
			this->Controls->Add(this->textBoxIR4);
			this->Controls->Add(this->textBoxIR3);
			this->Controls->Add(this->textBoxIR2);
			this->Controls->Add(this->textBoxIR1);
			this->Controls->Add(this->labelIR9);
			this->Controls->Add(this->labelIR8);
			this->Controls->Add(this->labelIR7);
			this->Controls->Add(this->labelIR6);
			this->Controls->Add(this->labelIR5);
			this->Controls->Add(this->labelIR4);
			this->Controls->Add(this->labelIR3);
			this->Controls->Add(this->labelIR2);
			this->Controls->Add(this->labelIR1);
			this->Controls->Add(this->labelIR0);
			this->Controls->Add(this->textBoxIR0);
			this->Controls->Add(this->labelDR100);
			this->Controls->Add(this->textBoxDR109);
			this->Controls->Add(this->textBoxDR108);
			this->Controls->Add(this->textBoxDR107);
			this->Controls->Add(this->textBoxDR106);
			this->Controls->Add(this->textBoxDR105);
			this->Controls->Add(this->textBoxDR104);
			this->Controls->Add(this->textBoxDR103);
			this->Controls->Add(this->textBoxDR102);
			this->Controls->Add(this->textBoxDR101);
			this->Controls->Add(this->textBoxDR100);
			this->Controls->Add(this->labelDR090);
			this->Controls->Add(this->textBoxDR99);
			this->Controls->Add(this->textBoxDR98);
			this->Controls->Add(this->textBoxDR97);
			this->Controls->Add(this->textBoxDR96);
			this->Controls->Add(this->textBoxDR95);
			this->Controls->Add(this->textBoxDR94);
			this->Controls->Add(this->textBoxDR93);
			this->Controls->Add(this->textBoxDR92);
			this->Controls->Add(this->textBoxDR91);
			this->Controls->Add(this->textBoxDR90);
			this->Controls->Add(this->labelDR080);
			this->Controls->Add(this->textBoxDR89);
			this->Controls->Add(this->textBoxDR88);
			this->Controls->Add(this->textBoxDR87);
			this->Controls->Add(this->textBoxDR86);
			this->Controls->Add(this->textBoxDR85);
			this->Controls->Add(this->textBoxDR84);
			this->Controls->Add(this->textBoxDR83);
			this->Controls->Add(this->textBoxDR82);
			this->Controls->Add(this->textBoxDR81);
			this->Controls->Add(this->textBoxDR80);
			this->Controls->Add(this->labelDR070);
			this->Controls->Add(this->textBoxDR79);
			this->Controls->Add(this->textBoxDR78);
			this->Controls->Add(this->textBoxDR77);
			this->Controls->Add(this->textBoxDR76);
			this->Controls->Add(this->textBoxDR75);
			this->Controls->Add(this->textBoxDR74);
			this->Controls->Add(this->textBoxDR73);
			this->Controls->Add(this->textBoxDR72);
			this->Controls->Add(this->textBoxDR71);
			this->Controls->Add(this->textBoxDR70);
			this->Controls->Add(this->labelDR060);
			this->Controls->Add(this->textBoxDR69);
			this->Controls->Add(this->textBoxDR68);
			this->Controls->Add(this->textBoxDR67);
			this->Controls->Add(this->textBoxDR66);
			this->Controls->Add(this->textBoxDR65);
			this->Controls->Add(this->textBoxDR64);
			this->Controls->Add(this->textBoxDR63);
			this->Controls->Add(this->textBoxDR62);
			this->Controls->Add(this->textBoxDR61);
			this->Controls->Add(this->textBoxDR60);
			this->Controls->Add(this->labelDR050);
			this->Controls->Add(this->textBoxDR59);
			this->Controls->Add(this->textBoxDR58);
			this->Controls->Add(this->textBoxDR57);
			this->Controls->Add(this->textBoxDR56);
			this->Controls->Add(this->textBoxDR55);
			this->Controls->Add(this->textBoxDR54);
			this->Controls->Add(this->textBoxDR53);
			this->Controls->Add(this->textBoxDR52);
			this->Controls->Add(this->textBoxDR51);
			this->Controls->Add(this->textBoxDR50);
			this->Controls->Add(this->labelDR040);
			this->Controls->Add(this->textBoxDR49);
			this->Controls->Add(this->textBoxDR48);
			this->Controls->Add(this->textBoxDR47);
			this->Controls->Add(this->textBoxDR46);
			this->Controls->Add(this->textBoxDR45);
			this->Controls->Add(this->textBoxDR44);
			this->Controls->Add(this->textBoxDR43);
			this->Controls->Add(this->textBoxDR42);
			this->Controls->Add(this->textBoxDR41);
			this->Controls->Add(this->textBoxDR40);
			this->Controls->Add(this->labelDR030);
			this->Controls->Add(this->textBoxDR39);
			this->Controls->Add(this->textBoxDR38);
			this->Controls->Add(this->textBoxDR37);
			this->Controls->Add(this->textBoxDR36);
			this->Controls->Add(this->textBoxDR35);
			this->Controls->Add(this->textBoxDR34);
			this->Controls->Add(this->textBoxDR33);
			this->Controls->Add(this->textBoxDR32);
			this->Controls->Add(this->textBoxDR31);
			this->Controls->Add(this->textBoxDR30);
			this->Controls->Add(this->labelDR020);
			this->Controls->Add(this->textBoxDR29);
			this->Controls->Add(this->textBoxDR28);
			this->Controls->Add(this->textBoxDR27);
			this->Controls->Add(this->textBoxDR26);
			this->Controls->Add(this->textBoxDR25);
			this->Controls->Add(this->textBoxDR24);
			this->Controls->Add(this->textBoxDR23);
			this->Controls->Add(this->textBoxDR22);
			this->Controls->Add(this->textBoxDR21);
			this->Controls->Add(this->textBoxDR20);
			this->Controls->Add(this->labelDR010);
			this->Controls->Add(this->textBoxDR19);
			this->Controls->Add(this->textBoxDR18);
			this->Controls->Add(this->textBoxDR17);
			this->Controls->Add(this->textBoxDR16);
			this->Controls->Add(this->textBoxDR15);
			this->Controls->Add(this->textBoxDR14);
			this->Controls->Add(this->textBoxDR13);
			this->Controls->Add(this->textBoxDR12);
			this->Controls->Add(this->textBoxDR11);
			this->Controls->Add(this->textBoxDR10);
			this->Controls->Add(this->labelDR000);
			this->Controls->Add(this->textBoxDR9);
			this->Controls->Add(this->textBoxDR8);
			this->Controls->Add(this->labelDR9);
			this->Controls->Add(this->labelDR8);
			this->Controls->Add(this->labelDirectRegisters);
			this->Controls->Add(this->textBoxDR7);
			this->Controls->Add(this->textBoxDR6);
			this->Controls->Add(this->textBoxDR5);
			this->Controls->Add(this->textBoxDR4);
			this->Controls->Add(this->textBoxDR3);
			this->Controls->Add(this->textBoxDR2);
			this->Controls->Add(this->textBoxDR1);
			this->Controls->Add(this->textBoxDR0);
			this->Controls->Add(this->labelDR7);
			this->Controls->Add(this->labelDR6);
			this->Controls->Add(this->labelDR5);
			this->Controls->Add(this->labelDR4);
			this->Controls->Add(this->labelDR3);
			this->Controls->Add(this->labelDR2);
			this->Controls->Add(this->labelDR1);
			this->Controls->Add(this->labelDR0);
			this->Controls->Add(this->label3);
			this->Controls->Add(this->label2);
			this->Controls->Add(this->label1);
			this->Controls->Add(this->StatusBox_txtbx);
			this->Name = L"Form1";
			this->Text = L"Open FXS Board Controller";
			this->ResumeLayout(false);
			this->PerformLayout();

		}
		
#pragma endregion




//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef LIBUSB_WIN32
private: usb_dev_handle *LibUSBGetDevice (unsigned short vid, unsigned short pid) {
			struct usb_bus *UsbBus = NULL;
			struct usb_device *UsbDevice = NULL;
			usb_dev_handle *ret;

			usb_find_busses();
			usb_find_devices();

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
#endif

private: bool ReadPROSLICDirectRegister (unsigned char Reg, unsigned int *RetVal, int ExpVal) {
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

private: bool WritePROSLICDirectRegister (unsigned char Reg, unsigned int *RetVal, unsigned int NewVal, bool MayDiffer) {
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




private: bool ReadPROSLICIndirectRegister (unsigned char Reg, unsigned int *RetVal, int ExpVal) {
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

private: bool WritePROSLICIndirectRegister (unsigned char Reg, unsigned int *RetVal, unsigned int NewVal, bool MayDiffer) {
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




//-------------------------------------------------------BEGIN CUT AND PASTE BLOCK-----------------------------------------------------------------------------------

private: System::Void SendAudioFile (System::String ^fileName) {
		System::IO::FileStream ^f;
		cli::array <unsigned char, 1> ^ReadBuf;
		int rbpos = 0;
		int count;
		unsigned char seq = 0;

		f = (gcnew System::IO::FileStream (fileName, System::IO::FileMode::Open));
		ReadBuf = (gcnew cli::array<unsigned char, 1> ((int) f->Length));
		if ((count = f->Read (ReadBuf, 0, (int) f->Length)) < (int) f->Length) {
			f->Close ();
			RegIndex = 987;
			RegValue = 987;
			Sleep (2000);
			return;
		}
		f->Close ();

		int ignore;
		unsigned char isoctl[12];
		isoctl [0] = 0x7E;	// manage isochronous
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

#if 0
		void *wctx;

		unsigned char wbuf [16384];

		// packetize data
		for (unsigned char *q = wbuf + 3; q - wbuf < sizeof (wbuf); q += 16) *q = seq++;
		for (unsigned char *q = wbuf + 8; q - wbuf < sizeof (wbuf);) {
			for (int j = 0; j < 8; j++) {
				if (rbpos >= ReadBuf->Length) break;
				*q++ = ReadBuf[rbpos++];
				// *q++ = 0xAA;
			}
			q += 8;
		}

		WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000);

		usb_isochronous_setup_async (UsbDevInstance, &wctx, EP2OUTHandle, 16);
		usb_submit_async (wctx, (char *) wbuf, sizeof (wbuf));
		while (true) {
			int wofs = usb_reap_async_nocancel (wctx, 40);
			if (wofs < 0) {
				RegIndex = 789;
				RegValue = -wofs;
				Sleep (5000);
				goto outahere;
			}
			wofs <<= 4;
			if (wofs >= sizeof (wbuf) / 16) break;
			RegValue = wofs;
		}
outahere:
		isoctl [0] = 0x7E;	// manage isochronous
		isoctl [1] = 0xFF;		// pause USB I/O
		WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB
		usb_reap_async (wctx, 10);
		usb_free_async (&wctx);

#else

		void *rctx1, *rctx2, *wctx1, *wctx2;
#		define IBSZ 512
		unsigned char rbuf1[IBSZ], rbuf2[IBSZ], wbuf1[IBSZ], wbuf2[IBSZ];

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
				if (rbpos >= ReadBuf->Length) break;
				*q++ = ReadBuf[rbpos++];
				// *q++ = 0xAA;
			}
			if (rbpos >= ReadBuf->Length) break;
			q += 8;
		}

		memset (wbuf2, 0x0, sizeof(wbuf2));
		for (unsigned char *q = wbuf2 + 3; q - wbuf2 < sizeof (wbuf2); q += 16) *q = seq++;
		for (unsigned char *q = wbuf2 + 8; q - wbuf2 < sizeof (wbuf2);) {
			for (int j = 0; j < 8; j++) {
				if (rbpos >= ReadBuf->Length) break;
				*q++ = ReadBuf[rbpos++];
				// *q++ = 0xAA;
			}
			if (rbpos >= ReadBuf->Length) break;
			q += 8;
		}

		RegIndex = 100000;
		RegValue = 0;
		usb_submit_async (rctx1, (char *)rbuf1, sizeof (rbuf1));
		usb_submit_async (wctx1, (char *)wbuf1, sizeof (wbuf1));

		WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB

		usb_submit_async (rctx2, (char *)rbuf2, sizeof (rbuf2));
		usb_submit_async (wctx2, (char *)wbuf2, sizeof (wbuf2));

		unsigned char *wbp = wbuf1;
		void **r = &rctx1;
		void **w = &wctx1;
		int rofs, wofs;

		for (int rpacks = 0, wpacks = 0; RegValue < ((unsigned int) ReadBuf->Length / 8) + 1;) {
			while (true) {
				rofs = usb_reap_async_nocancel (*r, IBSZ/16);
				wofs = usb_reap_async_nocancel (*w, IBSZ/16);
				if (wofs < 0) {
					RegIndex = 789;
					if (wofs == -116) { // timeout?
						RegIndex = -wofs;
						continue;
					}
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
				RegIndex++;
				usb_reap_async (*r, 1);
				// usb_free_async (r);
				usb_reap_async (*w, 1);
				usb_free_async (w);
				// usb_isochronous_setup_async (UsbDevInstance, r, EP2INHandle,  16);
				usb_isochronous_setup_async (UsbDevInstance, w, EP2OUTHandle, 16);
				
				memset (wbp, 0, sizeof(rbuf1));
				for (unsigned char *q = wbp + 3; q - wbp < sizeof (wbuf1); q += 16) *q = seq++;
				for (unsigned char *q = wbp + 8; q - wbp < sizeof (wbuf1);) {
					for (int j = 0; j < 8; j++) {
						if (rbpos >= ReadBuf->Length) break;
						*q++ = ReadBuf[rbpos++];
						// *q++ = 0xAA;
					}
					if (rbpos >= ReadBuf->Length) break;
					q += 8;
				}

				usb_submit_async (*r, (char *)rbuf1, sizeof (rbuf1));
				usb_submit_async (*w, (char *)wbp, sizeof (wbuf1));
				if (wbp == wbuf1) {
					wbp = wbuf2;
					r = &rctx2;
					w = &wctx2;
				}
				else {
					wbp = wbuf1;
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
#endif
	}

private: System::Void ReadWriteThread_DoWork(System::Object^  sender, System::ComponentModel::DoWorkEventArgs^  e) {
			//This thread does the actual USB read/write operations (but only when AttachedState == TRUE).
			//Since this is a separate thread, this code below executes asynchronously from the reset of the
			//code in this application.
			// unsigned char Buffer [32];
			// DWORD ActualLength;
			bool OldAttachedState = FALSE;
			unsigned int JunkRegValue;

tryagain:
			OldAttachedState = FALSE;

			while (true) {


				if (AttachedState == TRUE) {
					if (OldAttachedState == FALSE) {

						// show the user that we are starting
						RegIndex = 777;
						RegValue = 888;
						Sleep (1000);
#if 0
						// compute a moving average of sampled values
						unsigned char cnt[32];
						unsigned int samples [1024];
						unsigned int ignorecnt;
						unsigned int avpsout;
						unsigned int mxpsout = 0;
						unsigned int mnpsout = 65535;
						unsigned int count = 0;
						unsigned int index = 0;
						// execute once disregarding values, in order to reset firmware counters
						cnt[0] = DEBUG_GET_PSWRD;
						WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, cnt, 4, &ignorecnt, 1000); // send the command over USB
						for (int i = 0; i < sizeof (samples) / sizeof (unsigned int); i++) {
							samples [i] = 0;
						}
						if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, cnt, 4, &ignorecnt, 1000)) {
							RegValue = 800;
							Sleep (5000);
						}
						while (true) {
							cnt[0] = DEBUG_GET_PSWRD;
							WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, cnt, 4, &ignorecnt, 1000); // send the command over USB
							if (!ReadBulkFrmUSB(UsbDevInstance, EP1INHandle, cnt, 4, &ignorecnt, 1000)) {
								RegValue = 800;
								Sleep (5000);
							}
							else {
								samples [index] = (unsigned int) cnt [2] + 256 * (unsigned int) cnt [3];
								mxpsout = (mxpsout > samples[index])? mxpsout : samples[index];
								mnpsout = (mnpsout < samples[index])? mnpsout : samples[index];
								count++;
								index++;
								index %= sizeof (samples) / sizeof (unsigned int);
								avpsout = 0;
								for (unsigned int i = 0; i < sizeof (samples) / sizeof (unsigned int); i++) {
									avpsout += 1000 * samples [i];
									if (count < i) break;
								}
								avpsout /= sizeof (samples) / sizeof (unsigned int);
								RegIndex = mnpsout;
								RegValue = mxpsout;
							}
						}
#endif
#if 0		// isochronous read/write test
						void *wctx;
						void *rctx1, *rctx2, *wctx1, *wctx2;
						unsigned char isoctl[12];
						unsigned char rbuf1[512], rbuf2[512], wbuf1[512], wbuf2[512];
						isoctl [0] = 0x7E;	// manage isochronous
						isoctl [1] = 0;		// don't pause
						isoctl [2] = 0;		// receive data from us
						isoctl [3] = 0x01;	// test pattern
						isoctl [4] = 0x02;
						isoctl [5] = 0x03;
						isoctl [6] = 0x04;
						isoctl [7] = 0x11;
						isoctl [8] = 0x12;
						isoctl [9] = 0x13;
						isoctl [10]= 0x14;
						isoctl [11]= 0x15;
						unsigned int ignore;

						usb_isochronous_setup_async (UsbDevInstance, &rctx1, EP2INHandle, 16);
						usb_isochronous_setup_async (UsbDevInstance, &rctx2, EP2INHandle, 16);
						usb_isochronous_setup_async (UsbDevInstance, &wctx1, EP2OUTHandle, 16);
						usb_isochronous_setup_async (UsbDevInstance, &wctx2, EP2OUTHandle, 16);

						memset (rbuf1, 0, sizeof(rbuf1));
						memset (rbuf2, 0, sizeof(rbuf2));
						memset (wbuf1, 0xA, sizeof(wbuf1));
						memset (wbuf2, 0xA, sizeof(wbuf2));

						unsigned char seq = 0;
						for (unsigned char *p = wbuf1 + 3; p < wbuf1 + sizeof (wbuf1); p += 16) *p = seq++;
						for (unsigned char *p = wbuf2 + 3; p < wbuf2 + sizeof (wbuf2); p += 16) *p = seq++;

						WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB
						RegIndex = 1000;
						RegValue = 0;
						usb_submit_async (rctx1, (char *)rbuf1, 512);
						usb_submit_async (wctx1, (char *)wbuf1, 512);

						usb_submit_async (rctx2, (char *)rbuf2, 512);
						usb_submit_async (wctx2, (char *)wbuf2, 512);

						unsigned char *p = rbuf1;
						void **c = &rctx1;
						void **w = &wctx1;
						int wofs;

						for (int rpacks = 0; RegValue < 2048;) {
							while (true) {
								wofs = usb_reap_async_nocancel (*c, 1000);
								wofs = usb_reap_async_nocancel (*w, 1000);
								if (wofs < 0) {
									RegIndex = 789;
									RegValue = -wofs;
									Sleep (5000);
									goto tryagain;
								}
								wofs >>= 4;
								if (wofs >= rpacks) {
									break;
								}
								RegValue = 100000 + rpacks;
								// do nothing
							}
							RegValue += wofs - rpacks;
							rpacks = wofs;
							if (rpacks >= sizeof (rbuf1) / 16) {
								rpacks = 0;
								RegIndex ++;
								usb_reap_async (*c, 10000);
								usb_free_async (c);
								usb_reap_async (*w, 10000);
								usb_free_async (w);
								usb_isochronous_setup_async (UsbDevInstance, c, EP2INHandle, 16);
								usb_isochronous_setup_async (UsbDevInstance, w, EP2OUTHandle, 16);
								memset (p, 0, sizeof(rbuf1));
								usb_submit_async (*c, (char *)rbuf1, 512);
								usb_submit_async (*w, (char *)wbuf1, 512);
								if (p == rbuf1) {
									p = rbuf2;
									c = &rctx2;
									w = &wctx2;
								}
								else {
									p = rbuf1;
									c = &rctx1;
									w = &wctx1;
								}
							}
						}

						isoctl [0] = 0x7E;	// manage isochronous
						isoctl [1] = 1;		// stop sending data back to us
						WriteBulkToUSB(UsbDevInstance, EP1OUTHandle, isoctl, 11, &ignore, 1000); // send the command over USB
						Sleep (20000);
#endif

						ShowDirectRegisters ();

						// if we just entered the attached state, do initialization work
						if (!ReadPROSLICDirectRegister (11, &RegValue, 51)) {
							RegIndex = 11;
							Sleep (5000);
							continue;
						}
						else { 							// show it went OK
							RegIndex = 11;
							// Sleep (600);
						}

						// DR14 <- 0x10 (take DC-DC converter down)
						if (!WritePROSLICDirectRegister (14, &RegValue, 0x10, false)) {
							RegIndex = 14;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 14;
							ShowDirectRegister (14);
							// lots of things change when we turn the converter off, so show them all
							ShowDirectRegisters ();
						}

						// set linefeed (DR64) to 0 (open mode)
						if (!WritePROSLICDirectRegister (64, &RegValue, 0, true)) {
							RegIndex = 64;
							Sleep (2000);
							continue;
						}
						else {
							RegIndex = 64;
							ShowDirectRegister (64);
						}

						// ShowDirectRegisters ();

						// display RegValue in hex during setting indirect registers
						RegVHex = true;

						// most values here are copied from zaptel; don't just use blindly though,
						// check which values make sense

						// {0,255,"DTMF_ROW_0_PEAK",0x55C2},
						RegIndex = 10000; WritePROSLICIndirectRegister (0, &RegValue, 0x55C2, true); Sleep (200);
						// {1,255,"DTMF_ROW_1_PEAK",0x51E6},
						RegIndex = 10001; WritePROSLICIndirectRegister (1, &RegValue, 0x51E6, true); Sleep (200);
						// {2,255,"DTMF_ROW2_PEAK",0x4B85},
						RegIndex = 10002; WritePROSLICIndirectRegister (2, &RegValue, 0x4B85, true); Sleep (200);
						// {3,255,"DTMF_ROW3_PEAK",0x4937},
						RegIndex = 10003; WritePROSLICIndirectRegister (3, &RegValue, 0x4937, true); Sleep (200);
						// {4,255,"DTMF_COL1_PEAK",0x3333},
						RegIndex = 10004; WritePROSLICIndirectRegister (4, &RegValue, 0x3333, true); Sleep (200);
						// {5,255,"DTMF_FWD_TWIST",0x0202},
						RegIndex = 10005; WritePROSLICIndirectRegister (5, &RegValue, 0x0202, true); Sleep (200);
						// {6,255,"DTMF_RVS_TWIST",0x0202},
						RegIndex = 10006; WritePROSLICIndirectRegister (6, &RegValue, 0x0202, true); Sleep (200);
						// {7,255,"DTMF_ROW_RATIO_TRES",0x0198},
						RegIndex = 10007; WritePROSLICIndirectRegister (7, &RegValue, 0x0198, true); Sleep (200);
						// {8,255,"DTMF_COL_RATIO_TRES",0x0198},
						RegIndex = 10008; WritePROSLICIndirectRegister (8, &RegValue, 0x0198, true); Sleep (200);
						// {9,255,"DTMF_ROW_2ND_ARM",0x0611},
						RegIndex = 10009; WritePROSLICIndirectRegister (9, &RegValue, 0x0611, true); Sleep (200);
						// {10,255,"DTMF_COL_2ND_ARM",0x0202},
						RegIndex = 10010; WritePROSLICIndirectRegister (10, &RegValue, 0x0202, true); Sleep (200);
						// {11,255,"DTMF_PWR_MIN_TRES",0x00E5},
						RegIndex = 10011; WritePROSLICIndirectRegister (11, &RegValue, 0x00E5, true); Sleep (200);
						// {12,255,"DTMF_OT_LIM_TRES",0x0A1C},
						RegIndex = 10012; WritePROSLICIndirectRegister (12, &RegValue, 0x0A1C, true); Sleep (200);
						// {13,0,"OSC1_COEF",0x7B30},
						RegIndex = 10013; WritePROSLICIndirectRegister (13, &RegValue, 0x7B30, true); Sleep (200);
						// {14,1,"OSC1X",0x0063},
						RegIndex = 10014; WritePROSLICIndirectRegister (14, &RegValue, 0x0063, true); Sleep (200);
						// {15,2,"OSC1Y",0x0000},
						RegIndex = 10015; WritePROSLICIndirectRegister (15, &RegValue, 0x0000, true); Sleep (200);
						// {16,3,"OSC2_COEF",0x7870},
						RegIndex = 10016; WritePROSLICIndirectRegister (16, &RegValue, 0x7870, true); Sleep (200);
						// {17,4,"OSC2X",0x007D},
						RegIndex = 10017; WritePROSLICIndirectRegister (17, &RegValue, 0x007D, true); Sleep (200);
						// {18,5,"OSC2Y",0x0000},
						RegIndex = 10018; WritePROSLICIndirectRegister (18, &RegValue, 0x0000, true); Sleep (200);
						// {19,6,"RING_V_OFF",0x0000},
						RegIndex = 10019; WritePROSLICIndirectRegister (19, &RegValue, 0x0000, true); Sleep (200);
						// {20,7,"RING_OSC",0x7EF0},
						RegIndex = 10020; WritePROSLICIndirectRegister (20, &RegValue, 0x7EF0, true); Sleep (200);
						// {21,8,"RING_X",0x0160},
						RegIndex = 10021; WritePROSLICIndirectRegister (21, &RegValue, 0x0160, true); Sleep (200);
						// {22,9,"RING_Y",0x0000},
						RegIndex = 10022; WritePROSLICIndirectRegister (22, &RegValue, 0x0000, true); Sleep (200);
						// {23,255,"PULSE_ENVEL",0x2000},
						RegIndex = 10023; WritePROSLICIndirectRegister (23, &RegValue, 0x2000, true); Sleep (200);
						// {24,255,"PULSE_X",0x2000},
						RegIndex = 10024; WritePROSLICIndirectRegister (24, &RegValue, 0x2000, true); Sleep (200);
						// {25,255,"PULSE_Y",0x0000},
						RegIndex = 10025; WritePROSLICIndirectRegister (25, &RegValue, 0x0000, true); Sleep (200);
						// //{26,13,"RECV_DIGITAL_GAIN",0x4000},   // playback volume set lower
						// {26,13,"RECV_DIGITAL_GAIN",0x2000},     // playback volume set lower
						RegIndex = 10026; WritePROSLICIndirectRegister (26, &RegValue, 0x2000, true); Sleep (200);
						// {27,14,"XMIT_DIGITAL_GAIN",0x4000},
						// //{27,14,"XMIT_DIGITAL_GAIN",0x2000},
						RegIndex = 10027; WritePROSLICIndirectRegister (27, &RegValue, 0x4000, true); Sleep (200);
						// {28,15,"LOOP_CLOSE_TRES",0x1000},
						RegIndex = 10028; WritePROSLICIndirectRegister (28, &RegValue, 0x1000, true); Sleep (200);
						// {29,16,"RING_TRIP_TRES",0x3600},
						RegIndex = 10029; WritePROSLICIndirectRegister (29, &RegValue, 0x3600, true); Sleep (200);
						// {30,17,"COMMON_MIN_TRES",0x1000},
						RegIndex = 10030; WritePROSLICIndirectRegister (30, &RegValue, 0x1000, true); Sleep (200);
						// {31,18,"COMMON_MAX_TRES",0x0200},
						RegIndex = 10031; WritePROSLICIndirectRegister (31, &RegValue, 0x0200, true); Sleep (200);
						// {32,19,"PWR_ALARM_Q1Q2",0x07C0},
						// RegIndex = 10032; WritePROSLICIndirectRegister (32, &RegValue, 0x07C0, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10032; WritePROSLICIndirectRegister (32, &RegValue, 0x0FF0, true); Sleep (200);
						// {33,20,"PWR_ALARM_Q3Q4",0x2600},
						// RegIndex = 10033; WritePROSLICIndirectRegister (33, &RegValue, 0x2600, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10033; WritePROSLICIndirectRegister (33, &RegValue, 0x7F80, true); Sleep (200);
						// {34,21,"PWR_ALARM_Q5Q6",0x1B80},
						// RegIndex = 10034; WritePROSLICIndirectRegister (34, &RegValue, 0x1B80, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10034; WritePROSLICIndirectRegister (34, &RegValue, 0x0FF0, true); Sleep (200);
#if 0		// AN35 advises to set IRs 35--39 to 0x8000 now and then set them to their desired values much later
						// {35,22,"LOOP_CLOSURE_FILTER",0x8000},
						RegIndex = 10035; WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); Sleep (200);
						// {36,23,"RING_TRIP_FILTER",0x0320},
						RegIndex = 10036; WritePROSLICIndirectRegister (36, &RegValue, 0x0320, true); Sleep (200);
						// {37,24,"TERM_LP_POLE_Q1Q2",0x008C},
						// RegIndex = 10037; WritePROSLICIndirectRegister (37, &RegValue, 0x008C, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10037; WritePROSLICIndirectRegister (37, &RegValue, 0x0010, true); Sleep (200);
						// {38,25,"TERM_LP_POLE_Q3Q4",0x0100},
						// RegIndex = 10038; WritePROSLICIndirectRegister (38, &RegValue, 0x0100, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10038; WritePROSLICIndirectRegister (38, &RegValue, 0x0010, true); Sleep (200);
						// {39,26,"TERM_LP_POLE_Q5Q6",0x0010},
						// RegIndex = 10039; WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10039; WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); Sleep (200);
#else
						RegIndex = 10035; WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); Sleep (200);
						RegIndex = 10036; WritePROSLICIndirectRegister (36, &RegValue, 0x8000, true); Sleep (200);
						RegIndex = 10037; WritePROSLICIndirectRegister (37, &RegValue, 0x8000, true); Sleep (200);
						RegIndex = 10038; WritePROSLICIndirectRegister (38, &RegValue, 0x8000, true); Sleep (200);
						RegIndex = 10039; WritePROSLICIndirectRegister (39, &RegValue, 0x8000, true); Sleep (200);
#endif
						// {40,27,"CM_BIAS_RINGING",0x0C00},
						// - set elsewhere to 0 RegIndex = 10000; WritePROSLICIndirectRegister (40, &RegValue, 0x0x0C00, true); Sleep (200);
						// {41,64,"DCDC_MIN_V",0x0C00},
						RegIndex = 10041; WritePROSLICIndirectRegister (41, &RegValue, 0x0C00, true); Sleep (200);
						// {42,255,"DCDC_XTRA",0x1000},
						// - don't touch yet RegIndex = 10042; WritePROSLICIndirectRegister (42, &RegValue, 0x55C2, true); Sleep (200);
						// {43,66,"LOOP_CLOSE_TRES_LOW",0x1000},
						RegIndex = 10043; WritePROSLICIndirectRegister (42, &RegValue, 0x1000, true); Sleep (200);
						RegVHex = false;
						ShowIndirectRegisters ();

						// DR8 <- 0 (take SLIC out of "digital loopback" mode)
						if (!WritePROSLICDirectRegister (8, &RegValue, 0, false)) {
							RegIndex = 8;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 8;
							ShowDirectRegister (8);
						}

						// DR108 <- 0xEB (turn on Rev E. features)
						if (!WritePROSLICDirectRegister (108, &RegValue, 0xEB, false)) {
							RegIndex = 108;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 108;
							ShowDirectRegister (108);
						}

#if 0
						// DR67 <- 0x07 (turn off speedup and auto-switching to low battery)
						if (!WritePROSLICDirectRegister (67, &RegValue, 0x07, false)) {
							RegIndex = 67;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 67;
							ShowDirectRegister (67);
						}
#endif

						// DR66 <- 0x01 (keep Vov low, let Vbat track Vring)
						if (!WritePROSLICDirectRegister (66, &RegValue, 0x01, true)) {
							RegIndex = 66;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 66;
							ShowDirectRegister (66);
						}

						// DR92 <- 202 (DC-DC converter PWM period=12.33us ~ 81,109kHz)
						// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
						if (!WritePROSLICDirectRegister (92, &RegValue, 202, false)) {
							RegIndex = 92;
							Sleep (5000);
							continue;
						}
						else {							// show it went OK
							RegIndex = 92;
							ShowDirectRegister (92);
						}


						// DR 93 <- 12 (DC-DC converter min off time=732ns)
						// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
						if (!WritePROSLICDirectRegister (93, &RegValue, 12, false)) {
							RegIndex = 93;
							Sleep (5000);
							continue;
						}
						else {							// show it went OK
							RegIndex = 93;
							ShowDirectRegister (93);
						}

						// DR 74 <- 44 (high battery voltage = 66V)
						// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
						if (!WritePROSLICDirectRegister (74, &RegValue, 44, false)) {
							RegIndex = 74;
							Sleep (5000);
							continue;
						}
						else {							// show it went OK
							RegIndex = 74;
							ShowDirectRegister (74);
						}


						// DR 75 <- 40 (low battery voltage = 60V)
						// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
						if (!WritePROSLICDirectRegister (75, &RegValue, 40, false)) {
							RegIndex = 75;
							Sleep (5000);
							continue;
						}
						else {							// show it went OK
							RegIndex = 75;
							ShowDirectRegister (75);
						}


						// DR71<- 0 (max allowed loop current = 20mA [default value])
						if (!WritePROSLICDirectRegister (71, &RegValue, 0, false)) {
							RegIndex = 71;
							Sleep (5000);
							continue;
						}
						else {							// show it went OK
							RegIndex = 71;
							ShowDirectRegister (71);
						}

						// IR 40 <- 0
						// note: value taken from SiLabs DC-DC convereter param calculation Excel sheet
						// WritePROSLICIndirectRegister (40, &RegValue, 0, true);
						if (!WritePROSLICIndirectRegister (40, &RegValue, 0, true)) {
							RegIndex = 1040;
							Sleep (5000);
							continue;
						}
						else {
							RegIndex = 1040;
							// Sleep (600);
						}

						// ShowDirectRegisters ();

						// show that it all went OK so far
						RegIndex = 999;
						RegValue = 999;
						Sleep (200);

						// display a short countdown from 3 to 0
						RegValue = 3;
						Sleep (500);
						RegValue = 2;
						Sleep (500);
						RegValue = 1;
						Sleep (500);
						RegValue = 0;

						// ShowDirectRegisters ();

						// DR 14 <- 0 : this should bring the DC-DC converter up
						if (!WritePROSLICDirectRegister (14, &RegValue, 0, false)) {
							RegIndex = 14;
							Sleep (2000);
							// I am not issuing a "continue" here, in order to try to power down the DC-DC converter later
							// continue;
						}
						else {
							RegIndex = 14;
							ShowDirectRegister (14);
						}

						ShowDirectRegisters ();

						// check wether we got a decent VBAT value and if so go on;
						// otherwise loop back to start
						for (int i = 0; i < 10; i++) {
							if (!ReadPROSLICDirectRegister (82, &RegValue, -1)) goto tryagain;
							RegIndex = 1000 * i + 82;
							RegValue = RegValue * 376 / 1000;
							ShowDirectRegister (82);
							if (RegValue >= 60) break;
							// Sleep (20);
						}

						// if VBAT is not OK, signal we failed and loop over
						if (RegValue < 60) {
							RegIndex = 666;
							RegValue = 666;
							Sleep (300000);
							goto tryagain;
						}

						// disable all interrupts

						// DR 21 <- 0 : disable all interrupts in Interrupt Enable 1
						// DR 22 <- 0 : disable all interrupts in Interrupt Enable 2
						// DR 23 <- 0 : disable all interrupts in Interrupt Enable 3
						// DR 64 <- 0 : set linefeed to Open

						// I am not implementing these here, because 3210 reset
						// sets all these to zero anyway

						// perform manual calibration (required when we use 3201)
						
						// DR97 <- 0x18 : monitor ADC calibration 1&2, but don't do DAC/ADC/balance calibration
						//if (!WritePROSLICDirectRegister (97, &RegValue, 0x18, false)) {
						if (!WritePROSLICDirectRegister (97, &RegValue, 0x1E, false)) {
							RegIndex = 97;
							Sleep (2000);
							continue;
						}
						else {
							RegIndex = 97;
							ShowDirectRegister (97);
						}
						// DR 96 <- 0x47 : set CAL bit (start calibration), do differential DAC, common-mode DAC and I_LIM calibrations
						if (!WritePROSLICDirectRegister (96, &RegValue, 0x47, true)) {
							RegIndex = 96;
							Sleep (2000);
							continue;
						}
						else {
							RegIndex = 96;
							ShowDirectRegister (96);
						}
						for (int i = 0; i < 10; i++) {
							if (!ReadPROSLICDirectRegister (96, &RegValue, -1)) goto tryagain;
							if (RegValue == 0) break;
							Sleep (200);
							// ShowDirectRegister (96);
						}
						ShowDirectRegister (96);
						if (RegValue != 0) {
							Sleep (3000);
							continue;
						}

						// (might: set again DRs 98 and 99 to their reset values 0x10)

						for (unsigned int j = 0x1f; j > 0; j--) {
							if (!WritePROSLICDirectRegister (98, &RegValue, j, false)) goto tryagain;
							RegIndex = 98;
							// ShowDirectRegister (98);
							Sleep (40);
							if (!ReadPROSLICDirectRegister (88, &RegValue, -1)) goto tryagain;
							RegIndex = 88;
							// ShowDirectRegister (88);
							if (RegValue == 0) break;
						}
						ShowDirectRegister (98);
						ShowDirectRegister (88);

						for (unsigned int j = 0x1f; j > 0; j--) {
							if (!WritePROSLICDirectRegister (99, &RegValue, j, false)) goto tryagain;
							RegIndex = 99;
							// ShowDirectRegister (99);
							Sleep (40);
							if (!ReadPROSLICDirectRegister (89, &RegValue, -1)) goto tryagain;
							RegIndex = 89;
							// ShowDirectRegister (89);
							if (RegValue == 0) break;
						}
						ShowDirectRegister (99);
						ShowDirectRegister (89);

#if 1
						// enable interrupt logic for on/off-hook mode change during calibration
						if (!WritePROSLICDirectRegister (23, &RegValue, 0x04, true)) {
							RegIndex = 23;
							Sleep (2000);
							continue;
						}
						else {
							RegIndex = 23;
							ShowDirectRegister (23);
						}
						ShowDirectRegister (20);
#endif

# if 1
						// (AN35:) check DR 68 to make sure equipment is on-hook
						// Note: I wonder if this works without setting 64 to 0x01...

						while (true) {

							// set linefeed (DR64) to 0x01 (forward active mode)
							if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x01, true)) goto tryagain;
							ShowDirectRegister (64);

							RegIndex = 68;
							if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto tryagain;
							ShowDirectRegister (68);
							if (!(RegValue & 0x1)) break;

							// set linefeed (DR64) back to 0 (open mode)
							if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0, true)) goto tryagain;
							ShowDirectRegister (64);

							// display a sign that we did not like off-hook at this stage, then restart
							RegValue = 666000 + RegValue;
							Sleep (4000);
							goto tryagain;
						}
#endif

#if 1
						// perform longitudinal balance calibration

						// specify common mode balance calibration (CALCM=1) on calibration control/status register 2 (DR97)
						if (!WritePROSLICDirectRegister (97, &RegValue, 0x01, true)) {
							RegIndex = 97;
							Sleep (2000);
							continue;
						}
						else {
							RegIndex = 97;
							ShowDirectRegister (97);
						}
						ShowDirectRegister (20);
						// then specify start of calibration on calibration control/status register 1 (DR96)
						if (!WritePROSLICDirectRegister (96, &RegValue, 0x40, true)) {
							RegIndex = 96;
							Sleep (2000);
							continue;
						}
						while (true) {
							ShowDirectRegister (20);
							RegIndex = 96;
							if (!ReadPROSLICDirectRegister (96, &RegValue, -1)) goto tryagain;
							ShowDirectRegister (96);
							if (RegValue == 0) break;
						}
#endif

#if 1
						// flush energy accumulators
						for (unsigned char ir = 88; ir <= 95; ir++) {
							RegIndex = 10000 + ir;
							if (!WritePROSLICIndirectRegister (ir, &RegValue, 0, true)) goto tryagain;
							Sleep (50);
						}
						RegIndex = 10097;

						if (!WritePROSLICIndirectRegister (97, &RegValue, 0, true)) goto tryagain;
						Sleep (50);

						for (unsigned char ir = 193; ir <= 211; ir++) {
							RegIndex = 10000 + ir;
							if (!WritePROSLICIndirectRegister (ir, &RegValue, 0, true)) goto tryagain;
							Sleep (50);
						}
#endif

						// enable and clear interrupts
						for (unsigned char i = 19; i <= 23; i++) {
							RegIndex = i;
							if (!WritePROSLICDirectRegister (i, &RegValue, 0xFF, true)) goto tryagain;
							ShowDirectRegister (i);
						}


						// write DRs 2-5 (PCM clock slots)
						for (unsigned char i = 2; i <= 5; i++) {
							RegIndex = i;
							if (!WritePROSLICDirectRegister (i, &RegValue, 0, false)) goto tryagain;
							ShowDirectRegister (i);
						}

						// write registers 63, 67, 69, 70
						// 63 (Loop Closure Debounce Interval for ringing silent period) - not changed from default 0x54 (105 ms)
						if (!WritePROSLICDirectRegister (63, &JunkRegValue, 0x54, false)) goto tryagain;
						// 67 (Automatic/Manual Control) - not changed from default 0x1F (all set to auto)
						if (!WritePROSLICDirectRegister (67, &JunkRegValue, 0x1F, false)) goto tryagain;
						// 69 (Loop Closure Debounce Interval) - not changed from default 0x0A (12.5 ms)
						if (!WritePROSLICDirectRegister (69, &JunkRegValue, 0x0A, false)) goto tryagain;
						// 70 (Ring Trip Debounce Interval) - not changed from default 0x0A (12.5 ms)
						if (!WritePROSLICDirectRegister (70, &JunkRegValue, 0x0A, false)) goto tryagain;

						// initialize registers 65-66, 71-73
						// 65 (External Bipolar Transistor Control) - not changed from default 0x61)
						// 66 (Battery Feed Control) - two things here, Vov and TRACK, all set before (Vov to low value range, TRACK to 1)
						// 71 (Loop Current Limit) - not changed from default 0x00 (20mA)
						// 72 (On-Hook Line Voltage) - not changed from default 0x20 (48V)
						// 73 (Common-Mode Voltage) - not changed from default 0x02 (3V) - note: zaptel sets this to 6V

						// Write indirect registers 35-39
						// {35,22,"LOOP_CLOSURE_FILTER",0x8000},
						RegIndex = 10035; WritePROSLICIndirectRegister (35, &RegValue, 0x8000, true); Sleep (200);
						// {36,23,"RING_TRIP_FILTER",0x0320},
						RegIndex = 10036; WritePROSLICIndirectRegister (36, &RegValue, 0x0320, true); Sleep (200);
						// {37,24,"TERM_LP_POLE_Q1Q2",0x008C},
						// RegIndex = 10037; WritePROSLICIndirectRegister (37, &RegValue, 0x008C, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10037; WritePROSLICIndirectRegister (37, &RegValue, 0x0010, true); Sleep (200);
						// {38,25,"TERM_LP_POLE_Q3Q4",0x0100},
						// RegIndex = 10038; WritePROSLICIndirectRegister (38, &RegValue, 0x0100, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10038; WritePROSLICIndirectRegister (38, &RegValue, 0x0010, true); Sleep (200);
						// {39,26,"TERM_LP_POLE_Q5Q6",0x0010},
						// RegIndex = 10039; WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); Sleep (200);
						// as per AN47, p.4
						RegIndex = 10039; WritePROSLICIndirectRegister (39, &RegValue, 0x0010, true); Sleep (200);
						// refresh these
						ShowIndirectRegisters ();

					
						// enable PCM u-law, disable PCM I/O
						if (!WritePROSLICDirectRegister (1, &JunkRegValue, 0x08, false)) goto tryagain;
						ShowDirectRegister (1);

						// set the TXS and RXS registers to 1
						if (!WritePROSLICDirectRegister (2, &JunkRegValue, 0x01, false)) goto tryagain;
						ShowDirectRegister (2);
						if (!WritePROSLICDirectRegister (4, &JunkRegValue, 0x01, false)) goto tryagain;
						ShowDirectRegister (4);

#if 1	// ringing test, comment out when done

						if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x04, true)) goto tryagain;
						Sleep (2000);
						if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x00, true)) goto tryagain;
#endif

#if 1	// test, comment out when done

						ShowDirectRegister (19);

						// if (!WritePROSLICDirectRegister (67, &JunkRegValue, 0x1e, false)) goto tryagain;

						// set forward active mode
						if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x01, true)) goto tryagain;
						int q = 0;

						while (true) {
							if (q > 5) q = 0;
							RegIndex = 1000 + q;
							//
							if (!WritePROSLICDirectRegister (19, &JunkRegValue, 0xFF, true)) goto tryagain;
							ShowDirectRegister (19);
							// prepare to monitor Q_q
							if (!WritePROSLICDirectRegister (76, &JunkRegValue, q++, false)) goto tryagain;
							ShowDirectRegister (80);
							ShowDirectRegister (81);

							ShowDirectRegister (19);
							
							for (int t = 0; t < 10; t++) {
								// monitor power values 
								if (!ReadPROSLICDirectRegister (77, &RegValue, -1)) goto tryagain;
								// if (RegValue < JunkRegValue) RegValue = JunkRegValue;
								Sleep (50);
							}
							// if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x00, true)) goto tryagain;


							RegIndex = 68;
							if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto tryagain;
							if (RegValue & 0x01) {

								// set line to off-hook mode
								if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x02, true)) goto tryagain;
								ShowDirectRegister (64);

								// enable PCM I/O using u-law
								if (!WritePROSLICDirectRegister (1, &JunkRegValue, 0x28, false)) goto tryagain;
								//ShowDirectRegister (1);

								// send audio data
								// SendAudioFile (L"C:\\Users\\avarvit\\Desktop\\asterisk-core-sounds-en-ulaw-current\\hello-world.ulaw");
								SendAudioFile (L"C:\\Users\\avarvit\\Desktop\\asterisk-core-sounds-en-ulaw-current\\vm-options.ulaw");

								// "hangup" the line
								if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x01, true)) goto tryagain;

								// disable PCM I/O
								if (!WritePROSLICDirectRegister (1, &JunkRegValue, 0x08, false)) goto tryagain;
							}

							ShowDirectRegisters ();
							Sleep (2000);

							RegIndex = 68;
							if (!ReadPROSLICDirectRegister (68, &RegValue, -1)) goto tryagain;
							if (RegValue & 0x01) {
								if (!WritePROSLICDirectRegister (64, &JunkRegValue, 0x02, true)) goto tryagain;
							}
							// RegIndex = 82;
							// if (!ReadPROSLICDirectRegister (82, &RegValue, -1)) goto tryagain;
							// RegValue = RegValue * 376 / 1000;
							/* for (int t = 0; t < 10; t++) {
								RegIndex = 99900 + q;
								if (!ReadPROSLICDirectRegister (82, &RegValue, -1)) goto tryagain;
								RegValue = RegValue * 376 / 1000;
								RegValue += 82000;
								Sleep (1000);
							}*/
						}
#endif
						// DR 14 <- 16 : this should bring the DC-DC converter down
						if (!WritePROSLICDirectRegister (14, &RegValue, 16, false)) {
							RegIndex = 14;
							Sleep (5000);
							continue;
						}
						RegIndex = 999;
						RegValue = 999;
						Sleep (2000);
						RegIndex = 39;

						OldAttachedState = TRUE;		// so as not to re-execute the initialization sequence
					}

					RegIndex++;
					RegIndex %= 105;
					ReadPROSLICIndirectRegister (RegIndex, &RegValue, -1);
				}
				else {
					OldAttachedState = FALSE;
				}

				Sleep(990);
				Sleep(10);	//Add a small delay.  Otherwise, this while(true) loop executes at warp speed and can cause 
							//tangible CPU utilization increase, with no particular benefit to the application.
			}

		}

private: static array <System::Windows::Forms::Control ^> ^textBoxes;
         static bool textBoxesInitialized = false;
		 static array <System::Windows::Forms::Control ^> ^irTextBoxes;
		 static bool irTextBoxesInitialized = false;

private: void ShowDirectRegisters (void) {
			unsigned int RegVal;
			System::String^ compName;
			array <System::Windows::Forms::Control ^> ^textBox;

			if (!textBoxesInitialized) {
				textBoxes = gcnew array <System::Windows::Forms::Control ^>(110);
			}

			for (unsigned char i = 0; i < 110; i++) {
				if (!textBoxesInitialized) {
					compName = gcnew System::String (L"textBoxDR");
					compName += i;
					textBox = this->Controls->Find (compName, true);
					textBoxes [i] = textBox [0];
				}
				if (IsValidPROSLICDirectRegister (i)) {
					ReadPROSLICDirectRegister (i, &RegVal, -1);
					if (RegVal > 255) {
						textBoxes [i]->Text = L"XX";
						return;
					}
					else {
						textBoxes [i]->Text = System::String::Format (L"{0:X2}", RegVal);
					}
				}
				else {
					// textBox [0]->Enabled = false;
					textBoxes [i]->Enabled = false;
				}
			}
			textBoxesInitialized = true;
		 }

private: void ShowIndirectRegisters (void) {
			unsigned int RegVal;
			System::String^ compName;
			array <System::Windows::Forms::Control ^> ^irTextBox;
			if (!irTextBoxesInitialized) {
				irTextBoxes = gcnew array <System::Windows::Forms::Control ^>(110);
			}
			for (unsigned char i = 0; i < 110; i++) {
				if (IsValidPROSLICIndirectRegister (i)) {
					if (!irTextBoxesInitialized) {
						compName = gcnew System::String (L"textBoxIR");
						compName += i;
						irTextBox = this->Controls->Find (compName, true);
						irTextBoxes [i] = irTextBox [0];
					}

					ReadPROSLICIndirectRegister (i, &RegVal, -1);
					if (RegVal > 65535) {
						irTextBoxes [i]->Text = L"XXXX";
						return;
					}
					else {
						irTextBoxes [i]->Text = System::String::Format (L"{0:X4}", RegVal);
					}
				}
			}
			irTextBoxesInitialized = true;
		}


private: void ShowDirectRegister (int i) {
			unsigned int RegVal;
			int j;
			System::String^ compName;
			array <System::Windows::Forms::Control ^> ^textBox;
			System::Drawing::Color exColor;
			if (IsValidPROSLICDirectRegister (i)) {
				compName = gcnew System::String (L"textBoxDR");
				compName += i;
				textBox = this->Controls->Find (compName, true);
				ReadPROSLICDirectRegister (i, &RegVal, -1);
				textBox [0]->Text = System::String::Format (L"{0:X2}", RegVal);
				exColor = textBox[0]->ForeColor;
				for (j = 0; j < 2; j++) {
					// flash it
					textBox [0]->ForeColor = textBox [0]->BackColor;
					Sleep (50);
					textBox [0]->ForeColor = exColor;
					Sleep (50);
				}
			}
		}

private: bool IsValidPROSLICDirectRegister (const unsigned int b) {
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

private: bool IsValidPROSLICIndirectRegister (const unsigned int b) {
			if (b <= 43) return true;
			if (b >= 99 && b <= 104) return true;
			return false;
	}

private: System::Void timer1_Tick(System::Object^  sender, System::EventArgs^  e) {
			//When the timer goes off on the main form, update the user inferface with the new data obtained from the thread.
			/*
			if(RegValue > (unsigned int)progressBar1->Maximum)
			{
				RegValue = progressBar1->Maximum;
			}
			progressBar1->Value = RegValue;
			*/
			 if (AttachedState == TRUE) label3->Text = RegIndex.ToString() + L"->"+ (RegVHex? System::String::Format (L"{0:X4}", RegValue) : RegValue.ToString());
		 }

protected: virtual void WndProc( Message% m ) override{
		 //This is a callback function that gets called when a Windows message is received by the form.

		 // Listen for Windows messages.  We will receive various different types of messages, but the ones we really want to use are the WM_DEVICECHANGE messages.
		 if(m.Msg == WM_DEVICECHANGE)
		 {
			 if(((int)m.WParam == DBT_DEVICEARRIVAL) || ((int)m.WParam == DBT_DEVICEREMOVEPENDING) || ((int)m.WParam == DBT_DEVICEREMOVECOMPLETE) || ((int)m.WParam == DBT_CONFIGCHANGED) )
			 {

				//WM_DEVICECHANGE messages by themselves are quite generic, and can be caused by a number of different
				//sources, not just your USB hardware device.  Therefore, must check to find out if any changes relavant
				//to your device (with known VID/PID) took place before doing any kind of opening or closing of endpoints.
				//(the message could have been totally unrelated to your application/USB device)
				#ifdef MICROCHIP_USB
				if(MPUSBGetDeviceCount(DeviceVID_PID))	{//Check and make sure at least one device with matching VID/PID is attached
				#endif // MICROCHIP_USB
				#ifdef LIBUSB_WIN32
				if (UsbDevInstance = LibUSBGetDevice (Device_VID, Device_PID)) {
				#endif // LIBUSB_WIN32
				#ifdef CYPRESS_USB
				if (PLACEHOLDER) {
				#endif // CYPRESS_USB

					if(AttachedState == FALSE) {
				#ifdef MICROCHIP_USB
						EP1OUTHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP1", MP_WRITE, 0);
						EP1INHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP1", MP_READ, 0);
						EP2OUTHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP2", MP_WRITE, 0);
						EP2INHandle = MPUSBOpen(0, DeviceVID_PID, "\\MCHP_EP2", MP_READ, 0);
				#endif
						AttachedState = TRUE;
						StatusBox_txtbx->Text = "Device Found: AttachedState = TRUE";
						label2->Enabled = true;	//Make the label no longer greyed out
						label3->Enabled = true; // same
					}
					//else AttachedState == TRUE, in this case, do not try to re-open already open and functioning endpoints.  Doing
					//so will break them and make them no longer work.
				}
				else	//No devices attached, WM_DEVICECHANGE message must have been caused by your USB device falling off the bus (IE: user unplugged/powered down)
				{
					#ifdef MICROCHIP_USB
					if(MPUSBClose(EP1OUTHandle))
						EP1OUTHandle = INVALID_HANDLE_VALUE;
					if(MPUSBClose(EP1INHandle))
						EP1INHandle = INVALID_HANDLE_VALUE;
					if(MPUSBClose(EP2OUTHandle))
						EP2OUTHandle = INVALID_HANDLE_VALUE;
					if(MPUSBClose(EP2INHandle))
						EP2INHandle = INVALID_HANDLE_VALUE;
					#endif
					#ifdef LIBUSB_WIN32
					if (UsbDevInstance) {
						usb_release_interface (UsbDevInstance, 0);
						usb_close (UsbDevInstance);
						UsbDevInstance = NULL;
					}
					#endif

					AttachedState = FALSE;
					StatusBox_txtbx->Text = "Device Not Detected: Verify Connection/Correct Firmware";
					label2->Enabled = false;	//Make the label greyed out
					label3->Enabled = false;	//Same

				}
			 }
		 }

		 Form::WndProc( m );
	}
//-------------------------------------------------------END CUT AND PASTE BLOCK-------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------


};
}
