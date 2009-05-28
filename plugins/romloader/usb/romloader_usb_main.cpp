/***************************************************************************
 *   Copyright (C) 2007 by Christoph Thelen                                *
 *   doc_bacardi@users.sourceforge.net                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <ctype.h>
#include <stdio.h>

#include "romloader_usb_main.h"

#ifdef _WIN32
	#define snprintf _snprintf
#endif

/*-------------------------------------*/

typedef struct
{
	libusb_error eErrNo;
	const char *pcErrMsg;
} LIBUSB_STRERROR_T;

static const LIBUSB_STRERROR_T atStrError[] =
{
	{ LIBUSB_SUCCESS,		"success" },
	{ LIBUSB_ERROR_IO,		"input/output error" },
	{ LIBUSB_ERROR_INVALID_PARAM,	"invalid parameter" },
	{ LIBUSB_ERROR_ACCESS,		"access denied (insufficient permissions)" },
	{ LIBUSB_ERROR_NO_DEVICE,	"no such device (it may have been disconnected)" },
	{ LIBUSB_ERROR_NOT_FOUND,	"entity not found" },
	{ LIBUSB_ERROR_BUSY,		"resource busy" },
	{ LIBUSB_ERROR_TIMEOUT,		"operation timed out" },
	{ LIBUSB_ERROR_OVERFLOW,	"overflow" },
	{ LIBUSB_ERROR_PIPE,		"pipe error" },
	{ LIBUSB_ERROR_INTERRUPTED,	"system call interrupted (perhaps due to signal)" },
	{ LIBUSB_ERROR_NO_MEM,		"insufficient memory" },
	{ LIBUSB_ERROR_NOT_SUPPORTED,	"operation not supported or unimplemented on this platform" },
	{ LIBUSB_ERROR_OTHER,		"other error" },
};


static const char *libusb_strerror(int iError)
{
	const LIBUSB_STRERROR_T *ptCnt, *ptEnd;
	const char *pcMsg;
	const char *pcUnknownError = "unknown error";


	ptCnt = atStrError;
	ptEnd = ptCnt + (sizeof(atStrError)/sizeof(atStrError[0]));
	pcMsg = pcUnknownError;
	while( ptCnt<ptEnd )
	{
		if( ptCnt->eErrNo==iError )
		{
			pcMsg = ptCnt->pcErrMsg;
			break;
		}
		else
		{
			++ptCnt;
		}
	}

	return pcMsg;
}


bool isDeviceNetx(libusb_device *ptDev)
{
	bool fDeviceIsNetx;
	int iResult;
	struct libusb_device_descriptor sDevDesc;


	fDeviceIsNetx = false;
	if( ptDev!=NULL )
	{
		iResult = libusb_get_device_descriptor(ptDev, &sDevDesc);
		if( iResult==LIBUSB_SUCCESS )
		{
			fDeviceIsNetx  = true;
			fDeviceIsNetx &= ( sDevDesc.bDeviceClass==0x00 );
			fDeviceIsNetx &= ( sDevDesc.bDeviceSubClass==0x00 );
			fDeviceIsNetx &= ( sDevDesc.bDeviceProtocol==0x00 );
			fDeviceIsNetx &= ( sDevDesc.idVendor==0x0cc4 );
			fDeviceIsNetx &= ( sDevDesc.idProduct==0x0815 );
			fDeviceIsNetx &= ( sDevDesc.bcdDevice==0x0100 );
		}
	}

	return fDeviceIsNetx;
}

/*-------------------------------------*/

const char *romloader_usb_provider::m_pcPluginNamePattern = "romloader_usb_%02x_%02x";

romloader_usb_provider::romloader_usb_provider(swig_type_info *p_romloader_usb, swig_type_info *p_romloader_usb_reference)
 : muhkuh_plugin_provider("romloader_usb")
 , m_ptLibUsbContext(NULL)
{
	int iResult;


	printf("%s(%p): provider create\n", m_pcPluginId, this);

	/* get the romloader_baka lua type */
	m_ptPluginTypeInfo = p_romloader_usb;
	m_ptReferenceTypeInfo = p_romloader_usb_reference;

	/* create a new libusb context */
	iResult = libusb_init(&m_ptLibUsbContext);
	if( iResult!=LIBUSB_SUCCESS )
	{
		/* failed to create the context */
		printf("%s(%p): Failed to create libusb context: %d:%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
	}
}


romloader_usb_provider::~romloader_usb_provider(void)
{
	printf("%s(%p): provider delete\n", m_pcPluginId, this);

	if( m_ptLibUsbContext!=NULL )
	{
		/* free the libusb context */
		libusb_exit(m_ptLibUsbContext);
	}
}


int romloader_usb_provider::DetectInterfaces(lua_State *ptLuaStateForTableAccess)
{
	int iResult;
	libusb_context *ptLibUsbContext;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;
	libusb_device_handle *ptDevHandle;
	unsigned int uiBusNr;
	unsigned int uiDevAdr;
	bool fDeviceIsBusy;
	int iInterfaces;
	bool fDeviceIsNetx;
	size_t sizTable;
	romloader_usb_reference *ptRef;
	const size_t sizMaxName = 32;
	char acName[sizMaxName];


	iInterfaces = 0;

	// get the size of the table
	sizTable = lua_objlen(ptLuaStateForTableAccess, 2);

	/* check the libusb context */
	if( m_ptLibUsbContext==NULL )
	{
		/* libusb was not initialized */
		printf("%s(%p): libusb was not initialized!\n", m_pcPluginId, this);
	}
	else
	{
		/* detect devices */
		ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
		if( ssizDevList<0 )
		{
			/* failed to detect devices */
			printf("%s(%p): failed to detect usb devices: %d:%s\n", m_pcPluginId, this, ssizDevList, libusb_strerror(ssizDevList));
		}
		else
		{
			/* loop over all devices */
			ptDevCnt = ptDeviceList;
			ptDevEnd = ptDevCnt + ssizDevList;
			while( ptDevCnt<ptDevEnd )
			{
				ptDev = *ptDevCnt;
				fDeviceIsNetx = isDeviceNetx(ptDev);
				if( fDeviceIsNetx==true )
				{
					/* construct the name */
					uiBusNr = libusb_get_bus_number(ptDev);
					uiDevAdr = libusb_get_device_address(ptDev);
					snprintf(acName, sizMaxName-1, m_pcPluginNamePattern, uiBusNr, uiDevAdr);

					/* open the device */
					iResult = libusb_open(ptDev, &ptDevHandle);
					if( iResult!=LIBUSB_SUCCESS )
					{
						/* failed to open the interface, do not add it to the list */
						printf("%s(%p): failed to open device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
					}
					else
					{
						/* set the configuration */
						iResult = libusb_set_configuration(ptDevHandle, 1);
						if( iResult!=LIBUSB_SUCCESS )
						{
							/* failed to set the configuration */
							printf("%s(%p): failed to set the configuration of device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
						}
						else
						{
							/* claim the interface, 0 is the interface number */
							iResult = libusb_claim_interface(ptDevHandle, 0);
							if( iResult!=LIBUSB_SUCCESS && iResult!=LIBUSB_ERROR_BUSY )
							{
								/* failed to claim the interface */
								printf("%s(%p): failed to claim the interface of device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
							}
							else
							{
								if( iResult!=LIBUSB_SUCCESS )
								{
									/* the interface is busy! */
									fDeviceIsBusy = true;
								}
								else
								{
									/* ok, claimed the interface -> the device is not busy */
									fDeviceIsBusy = false;
									/* release the interface */
									/* NOTE: The 'busy' information only represents the device state at detection time.
									 * This function _must_not_ keep the claim on the device or other applications will
									 * not be able to use it.
									 */
									iResult = libusb_release_interface(ptDevHandle, 0);
									if( iResult!=LIBUSB_SUCCESS )
									{
										/* failed to release the interface */
										printf("%s(%p): failed to release the interface of device %s after a successful claim: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
									}
								}

								/* create the new instance */
								ptRef = new romloader_usb_reference(acName, m_pcPluginId, fDeviceIsBusy, this);

								SWIG_NewPointerObj(ptLuaStateForTableAccess, ptRef, m_ptReferenceTypeInfo, 1);
								sizTable++;
								lua_rawseti(ptLuaStateForTableAccess, 2, sizTable);
							}
						}

						/* close the device */
						libusb_close(ptDevHandle);
					}
				}
				/* next list item */
				++ptDevCnt;
			}
			/* free the device list */
			libusb_free_device_list(ptDeviceList, 1);
		}
	}

	return iInterfaces;
}


romloader_usb *romloader_usb_provider::ClaimInterface(const muhkuh_plugin_reference *ptReference)
{
	romloader_usb *ptPlugin;
	const char *pcName;
	unsigned int uiBusNr;
	unsigned int uiDevAdr;


	/* expect error */
	ptPlugin = NULL;


	if( ptReference==NULL )
	{
		fprintf(stderr, "%s(%p): claim_interface(): missing reference!\n", m_pcPluginId, this);
	}
	else
	{
		pcName = ptReference->GetName();
		if( pcName==NULL )
		{
			fprintf(stderr, "%s(%p): claim_interface(): missing name!\n", m_pcPluginId, this);
		}
		else if( sscanf(pcName, m_pcPluginNamePattern, &uiBusNr, &uiDevAdr)!=2 )
		{
			fprintf(stderr, "%s(%p): claim_interface(): invalid name: %s\n", m_pcPluginId, this, pcName);
		}
		else if( m_ptLibUsbContext==NULL )
		{
			/* libusb was not initialized */
			printf("%s(%p): libusb was not initialized!\n", m_pcPluginId, this);
		}
		else
		{
			ptPlugin = new romloader_usb(pcName, m_pcPluginId, this, uiBusNr, uiDevAdr);
			printf("%s(%p): claim_interface(): claimed interface %s.\n", m_pcPluginId, this, pcName);
		}
	}

	return ptPlugin;
}


bool romloader_usb_provider::ReleaseInterface(muhkuh_plugin *ptPlugin)
{
	bool fOk;
	const char *pcName;
	unsigned int uiBusNr;
	unsigned int uiDevAdr;


	/* expect error */
	fOk = false;


	if( ptPlugin==NULL )
	{
		fprintf(stderr, "%s(%p): release_interface(): missing plugin!\n", m_pcPluginId, this);
	}
	else
	{
		pcName = ptPlugin->GetName();
		if( pcName==NULL )
		{
			fprintf(stderr, "%s(%p): release_interface(): missing name!\n", m_pcPluginId, this);
		}
		else if( sscanf(pcName, m_pcPluginNamePattern, &uiBusNr, &uiDevAdr)!=2 )
		{
			fprintf(stderr, "%s(%p): release_interface(): invalid name: %s\n", m_pcPluginId, this, pcName);
		}
		else
		{
			printf("%s(%p): released interface %s.\n", m_pcPluginId, this, pcName);
			fOk = true;
		}
	}

	return fOk;
}


/*-------------------------------------*/

romloader_usb::romloader_usb(const char *pcName, const char *pcTyp, romloader_usb_provider *ptProvider, unsigned int uiBusNr, unsigned int uiDeviceAdr)
 : romloader(pcName, pcTyp, ptProvider)
 , m_ptUsbProvider(ptProvider)
 , m_uiBusNr(uiBusNr)
 , m_uiDeviceAdr(uiDeviceAdr)
 , m_ptLibUsbContext(NULL)
 , m_ptUsbDev(NULL)
 , m_ptUsbDevHandle(NULL)
{
	int iResult;


	printf("%s(%p): created in romloader_usb\n", m_pcName, this);

	/* create a new libusb context */
	iResult = libusb_init(&m_ptLibUsbContext);
	if( iResult!=LIBUSB_SUCCESS )
	{
		/* failed to create the context */
		printf("%s(%p): Failed to create libusb context: %d:%s\n", m_pcName, this, iResult, libusb_strerror(iResult));
	}
}


romloader_usb::~romloader_usb(void)
{
	printf("%s(%p): deleted in romloader_usb\n", m_pcName, this);

	if( m_ptLibUsbContext!=NULL )
	{
		/* free the libusb context */
		libusb_exit(m_ptLibUsbContext);
	}
}


bool romloader_usb::chip_init(lua_State *ptClientData)
{
	bool fResult;


	switch( m_tChiptyp )
	{
	case ROMLOADER_CHIPTYP_NETX500:
	case ROMLOADER_CHIPTYP_NETX100:
		switch( m_tRomcode )
		{
		case ROMLOADER_ROMCODE_ABOOT:
			// aboot does not set the serial vectors
			write_data32(ptClientData, 0x10001ff0, 0x00200582|1);		// get: usb_receiveChar
			write_data32(ptClientData, 0x10001ff4, 0x0020054e|1);		// put: usb_sendChar
			write_data32(ptClientData, 0x10001ff8, 0);			// peek: none
			write_data32(ptClientData, 0x10001ffc, 0x00200566|1);		// flush: usb_sendFinish
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_HBOOT:
			// hboot needs no special init
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_UNKNOWN:
			fResult = false;
			break;
		}
		break;

	case ROMLOADER_CHIPTYP_NETX50:
		switch( m_tRomcode )
		{
		case ROMLOADER_ROMCODE_ABOOT:
			// this is an unknown combination
			fResult = false;
			break;
		case ROMLOADER_ROMCODE_HBOOT:
			// hboot needs no special init
			fResult = true;
			break;
		case ROMLOADER_ROMCODE_UNKNOWN:
			fResult = false;
			break;
		}
		break;

	case ROMLOADER_CHIPTYP_UNKNOWN:
		fResult = false;
		break;
	}

	return fResult;
}


/* open the connection to the device */
void romloader_usb::Connect(lua_State *ptClientData)
{
	int iResult;
	DATA_BUFFER_T tBuffer;
	SWIGLUA_REF tRef;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;


	tRef.L = NULL;
	tRef.ref = 0;

	if( m_fIsConnected!=false )
	{
		printf("%s(%p): already connected, ignoring new connect request\n", m_pcName, this);
	}
	else
	{
		m_ptUsbDev = NULL;
		m_ptUsbDevHandle = NULL;

		ptDeviceList = NULL;

		/* search device with bus and address */
		ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
		if( ssizDevList<0 )
		{
			/* failed to detect devices */
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to detect usb devices: %d:%s", m_pcName, this, ssizDevList, libusb_strerror(ssizDevList));
			iResult = (int)ssizDevList;
		}
		else
		{
			/* loop over all devices */
			ptDevCnt = ptDeviceList;
			ptDevEnd = ptDevCnt + ssizDevList;
			while( ptDevCnt<ptDevEnd )
			{
				ptDev = *ptDevCnt;
				if( isDeviceNetx(ptDev)==true && libusb_get_bus_number(ptDev)==m_uiBusNr && libusb_get_device_address(ptDev)==m_uiDeviceAdr )
				{
					m_ptUsbDev = ptDev;
					break;
				}
				else
				{
					++ptDevCnt;
				}
			}

			/* found the requested device? */
			if( m_ptUsbDev==NULL )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): interface not found. Maybe it was plugged out.\n", m_pcName, this);
				iResult = LIBUSB_ERROR_NOT_FOUND;
			}
			else
			{
				iResult = libusb_open(m_ptUsbDev, &m_ptUsbDevHandle);
				if( iResult!=LIBUSB_SUCCESS )
				{
					MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to open the device: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
				}
				else
				{
					if( m_ptUsbDevHandle==NULL )
					{
						MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): device not found. is it still plugged in and powered?", m_pcName, this);
						iResult = LIBUSB_ERROR_NOT_FOUND;
					}
					else
					{
						// get netx welcome message
						iResult = usb_getNetxData(&tBuffer, &tRef, 0);
						if( iResult!=LIBUSB_SUCCESS )
						{
							printf("%s(%p): failed to receive netx response, trying to reset netx: %d:%s\n", m_pcName, this, iResult, libusb_strerror(iResult));

							// try to reset the device and try again
							iResult = libusb_resetDevice();
							if( iResult!=LIBUSB_SUCCESS )
							{
								MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to reset the netx, giving up: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
								libusb_closeDevice();
							}
							else
							{
								printf("%s(%p): reset ok!\n", m_pcName, this);

								libusb_closeDevice();
								m_ptUsbDevHandle = NULL;
								iResult = libusb_open(m_ptUsbDev, &m_ptUsbDevHandle);
								if( iResult==LIBUSB_SUCCESS )
								{
									if( m_ptUsbDevHandle==NULL )
									{
										MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): lost device after reset!");
										iResult = LIBUSB_ERROR_OTHER;
									}
									else
									{
										// get netx welcome message
										iResult = usb_getNetxData(&tBuffer, &tRef, 0);
										if( iResult!=LIBUSB_SUCCESS )
										{
											MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to receive netx response: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
											libusb_closeDevice();
										}
									}
								}
							}
						}

						if( iResult==LIBUSB_SUCCESS )
						{
							printf("%s(%p): netx response: %s\n", m_pcName, this, tBuffer.pucData);
							free(tBuffer.pucData);

							/* NOTE: set m_fIsConnected to true here or detect_chiptyp and chip_init will fail! */
							m_fIsConnected = true;

							if( detect_chiptyp(ptClientData)!=true )
							{
								MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to detect chiptyp!", m_pcName, this);
								m_fIsConnected = false;
								libusb_closeDevice();
							}
							else if( chip_init(ptClientData)!=true )
							{
								MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to init chip!", m_pcName, this);
								m_fIsConnected = false;
								libusb_closeDevice();
							}
						}
					}
				}
			}

			/* free the device list */
			libusb_free_device_list(ptDeviceList, 1);
		}

		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
		}
	}
}


/* close the connection to the device */
void romloader_usb::Disconnect(lua_State *ptClientData)
{
	printf("%s(%p): disconnect()\n", m_pcName, this);

	/* NOTE: allow disconnects even if the plugin was already disconnected. */

	if( m_ptUsbDevHandle!=NULL )
	{
		libusb_close(m_ptUsbDevHandle);
	}

	m_fIsConnected = false;
	m_ptUsbDevHandle = NULL;
}


bool romloader_usb::parse_hex_digit(DATA_BUFFER_T *ptBuffer, size_t sizDigits, unsigned long *pulResult)
{
	const unsigned char *pucData;
	unsigned long ulResult;
	bool fOk;
	unsigned char uc;
	unsigned int uiDigit;
	size_t sizCnt;

	if( ptBuffer->sizPos+sizDigits>ptBuffer->sizData )
	{
		// not enough chars left in the buffer -> this can not work!
		fOk = false;
	}
	else
	{
		pucData = ptBuffer->pucData + ptBuffer->sizPos;
		ulResult = 0;
		fOk = true;
		sizCnt = 0;
		while( sizCnt<sizDigits )
		{
			uc = *(pucData++);
			uc = tolower(uc);
			if( uc>='0' && uc<='9' )
			{
				uiDigit = uc - '0';
			}
			else if( uc>='a' && uc<='f' )
			{
				uiDigit = uc - 'a' + 10;
			}
			else
			{
				fOk = false;
				break;
			}
			ulResult <<= 4;
			ulResult |= uiDigit;

			++sizCnt;
		}

		if( fOk==true )
		{
			if( pulResult!=NULL )
			{
				*pulResult = ulResult;
			}
			ptBuffer->sizPos += sizDigits;
		}
	}

	return fOk;
}


bool romloader_usb::expect_string(DATA_BUFFER_T *ptBuffer, const char *pcMatch)
{
	size_t sizMatch;
	const unsigned char *pucMatch;
	const unsigned char *pucData;
	bool fOk;
	unsigned char uc;
	size_t sizCnt;


	sizMatch = strlen(pcMatch);
	pucMatch = (unsigned char*)pcMatch;

	if( ptBuffer->sizPos+sizMatch>ptBuffer->sizData )
	{
		fOk = false;
	}
	else
	{
		pucData = ptBuffer->pucData + ptBuffer->sizPos;
		fOk = true;
		sizCnt = 0;
		do
		{
			uc = *(pucMatch++);
			fOk = ( uc==*(pucData++) );
			++sizCnt;
		} while( fOk==true && sizCnt<sizMatch );
	}

	if( fOk==true )
	{
		ptBuffer->sizPos += sizMatch;
	}

	return fOk;
}


/* read a byte (8bit) from the netx to the pc */
unsigned char romloader_usb::read_data08(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned long ulValue;
	int iResult;
	char acCommand[19];
	DATA_BUFFER_T tBuffer;
	unsigned long ulResponseAddress;
	unsigned long ulResponseValue;
	bool fOk;


	ulValue = 0;

	// expect failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "DUMP %08lX BYTE", ulNetxAddress);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( parse_hex_digit(&tBuffer, 8, &ulResponseAddress)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( ulResponseAddress!=ulNetxAddress )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): address does not match request: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( expect_string(&tBuffer, ": ")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( parse_hex_digit(&tBuffer, 2, &ulResponseValue)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): read_data08: 0x%08lx = 0x%02lx\n", m_pcName, this, ulNetxAddress, ulValue);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return (unsigned char)ulResponseValue;
}


/* read a word (16bit) from the netx to the pc */
unsigned short romloader_usb::read_data16(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned long ulValue;
	int iResult;
	char acCommand[19];
	DATA_BUFFER_T tBuffer;
	unsigned long ulResponseAddress;
	unsigned long ulResponseValue;
	bool fOk;


	ulValue = 0;

	// expect failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "DUMP %08lX WORD", ulNetxAddress);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( parse_hex_digit(&tBuffer, 8, &ulResponseAddress)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( ulResponseAddress!=ulNetxAddress )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): address does not match request: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( expect_string(&tBuffer, ": ")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( parse_hex_digit(&tBuffer, 4, &ulResponseValue)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): read_data16: 0x%08lx = 0x%04lx\n", m_pcName, this, ulNetxAddress, ulValue);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return (unsigned short)ulResponseValue;
}


/* read a long (32bit) from the netx to the pc */
unsigned long romloader_usb::read_data32(lua_State *ptClientData, unsigned long ulNetxAddress)
{
	unsigned long ulValue;
	int iResult;
	char acCommand[19];
	DATA_BUFFER_T tBuffer;
	unsigned long ulResponseAddress;
	unsigned long ulResponseValue;
	bool fOk;


	ulValue = 0;

	// expect failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "DUMP %08lX LONG", ulNetxAddress);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( parse_hex_digit(&tBuffer, 8, &ulResponseAddress)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( ulResponseAddress!=ulNetxAddress )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): address does not match request: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( expect_string(&tBuffer, ": ")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else if( parse_hex_digit(&tBuffer, 8, &ulResponseValue)!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from device: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): read_data32: 0x%08lx = 0x%08lx\n", m_pcName, this, ulNetxAddress, ulValue);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}

	return ulResponseValue;
}


bool romloader_usb::parseDumpLine(DATA_BUFFER_T *ptBuffer, unsigned long ulAddress, unsigned long ulElements, unsigned char *pucBuffer)
{
/*
	bool fResult;
	int iMatches;
	unsigned long ulResponseAddress;
	unsigned long ulChunkCnt;
	unsigned long ulByte;
	const unsigned char *pucData;
	static const unsigned char aucMatch0[3] = { ':', ' ', 0 };
	static const unsigned char aucMatch1[2] = { ' ', 0 };


	// expect success
	fResult = true;

	pucData = pucLine;

	// is enough input data left?
	if( sizLineLen<(10+ulElements*3) )
	{
		fResult = false;
	}
	else if( parse_hex_digit(&pucData, 8, &ulResponseAddress)!=true )
	{
		fResult = false;
	}
	else if( ulResponseAddress!=ulAddress )
	{
		fResult = false;
	}
	else if( expect_string(&pucData, aucMatch0)!=true )
	{
		fResult = false;
	}
	else
	{
		// get all bytes
		ulChunkCnt = ulElements;
		while( ulChunkCnt!=0 )
		{
			// get one hex digit
			if( parse_hex_digit(&pucData, 2, &ulByte)!=true )
			{
				fResult = false;
				break;
			}
			else if( expect_string(&pucData, aucMatch1)!=true )
			{
				fResult = false;
				break;
			}
			else
			{
				*(pucBuffer++) = (char)ulByte;
				// one number processed
				--ulChunkCnt;
			}
		}
	}

	return fResult;
*/
	return false;
}

/*
int romloader_usb::getLine(wxString &strData)
{
	int iEolCnt;
	size_t sizStartPos;
	char c;
	int iResult;


	iResult = LIBUSB_SUCCESS;

	strData.Empty();

	do
	{
		// save start position
		sizStartPos = sizBufPos;

		// no eol found yet
		iEolCnt = 0;

		// look for eol in current buffer
		while( sizBufPos<sizBufLen )
		{
			c = acBuf[sizBufPos];
			if( c==0x0a || c==0x0d)
			{
				++iEolCnt;
			}
			else if( iEolCnt>0 )
			{
				break;
			}
			++sizBufPos;
		}

		// get end of string
		if( iEolCnt>0 )
		{
			strData.Append(wxString::From8BitData(acBuf+sizStartPos, sizBufPos-sizStartPos-iEolCnt));
		}

		// get beginning of string
		if( iEolCnt==0 && sizStartPos<sizBufPos )
		{
			strData.Append(wxString::From8BitData(acBuf+sizStartPos, sizBufPos-sizStartPos));
		}

		// get more data
		if( iEolCnt==0 || strData.IsEmpty()==true )
		{
			acBuf[0] = 0x00;
			iResult = libusb_exchange((unsigned char*)acBuf, (unsigned char*)acBuf);
			if( iResult!=LIBUSB_SUCCESS )
			{
				strData.Printf(_("failed to receive command response: %s"), libusb_strerror(iResult));
				break;
			}
			else
			{
				sizBufLen = acBuf[0];
				if( sizBufLen==0 )
				{
					fEof = true;
					iEolCnt = 1;
					break;
				}
				else
				{
					sizBufPos = 1;
				}
			}
		}
	} while( iEolCnt==0 );

	return iResult;
}
*/
/* read a byte array from the netx to the pc */
void romloader_usb::read_image(unsigned long ulNetxAddress, unsigned long ulSize, char **ppcOutputData, unsigned long *pulOutputData, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
/*
	int iResult;
	char acCommand[28];
	bool fOk;
	wxString strErrorMsg;
	wxString strResponse;
	wxString strData;
	unsigned char *pucData;
	unsigned char *pucDataCnt;
	unsigned long ulBytesLeft;
	unsigned long ulExpectedAddress;
	unsigned long ulChunkSize;


	// expect error
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "DUMP %08lX %08lX BYTE", ulNetxAddress, ulSize);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_sendCommand(acCommand);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(tLuaFn.L, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			// init the buffer
			sizBufLen = 0;
			sizBufPos = 0;
			fEof = false;

			// alloc buffer
			pucData = (unsigned char*)malloc(ulSize);
			if( pucData==NULL )
			{
				strErrorMsg.Printf(wxT("failed to alloc %d bytes of input buffer!"), ulSize);
			}
			else
			{
				pucDataCnt = pucData;
				// parse the result
				ulBytesLeft = ulSize;
				ulExpectedAddress = ulNetxAddress;
				while( ulBytesLeft>0 )
				{
					fOk = callback_long(L, iLuaCallbackTag, ulSize-ulBytesLeft, pvCallbackUserData);
					if( fOk!=true )
					{
						strErrorMsg = wxT("operation canceled!");
						break;
					}

					// get the response line
					iResult = getLine(strResponse);
					if( iResult!=LIBUSB_SUCCESS )
					{
						fOk = false;
						strErrorMsg = wxT("failed to get dump response from device");
						break;
					}
					else if( fEof==true )
					{
						fOk = false;
						strErrorMsg = wxT("not enough data received from device");
						break;
					}
					else
					{
						// get the number of expected bytes in the next row
						ulChunkSize = 16;
						if( ulChunkSize>ulBytesLeft )
						{
							ulChunkSize = ulBytesLeft;
						}
						fOk = parseDumpLine(strResponse.To8BitData(), strResponse.Len(), ulExpectedAddress, ulChunkSize, pucDataCnt, strErrorMsg);
						if( fOk!=true )
						{
							break;
						}
						else
						{
							ulBytesLeft -= ulChunkSize;
							// inc address
							ulExpectedAddress += ulChunkSize;
							// inc buffer ptr
							pucDataCnt += ulChunkSize;
						}
					}
				}

				if( fOk==true )
				{
					// get data
					strData = wxString::From8BitData((const char*)pucData, ulSize);

					// wait for prompt
					iResult = usb_getNetxData(strResponse, NULL, 0, NULL);
					if( iResult!=LIBUSB_SUCCESS )
					{
						strData.Printf(_("failed to receive command response: %s"), libusb_strerror(iResult));
						fOk = false;
					}
				}

				if( fOk!=true )
				{
					strErrorMsg = strData;
					strData.Empty();
				}

				free(pucData);
			}
		}
	}

	if( fOk!=true )
	{
		muhkuh_log_error("%s%s", m_acMe, strErrorMsg.fn_str());
		m_ptLuaState->wxlua_Error(strErrorMsg);
	}

	return strData;
*/
}


/* write a byte (8bit) from the pc to the netx */
void romloader_usb::write_data08(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned char ucData)
{
	int iResult;
	char acCommand[22];
	unsigned char *pucData;
	DATA_BUFFER_T tBuffer;
	unsigned int uiDataLen;
	bool fOk;


	// assume failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "FILL %08lX %02X BYTE", ulNetxAddress, ucData);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( expect_string(&tBuffer, "\n>")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from netx: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): write_data08: 0x%08lx = 0x%02x\n", m_pcName, this, ulNetxAddress, ucData);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


/* write a word (16bit) from the pc to the netx */
void romloader_usb::write_data16(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned short usData)
{
	int iResult;
	char acCommand[24];
	DATA_BUFFER_T tBuffer;
	unsigned int uiDataLen;
	bool fOk;


	// assume failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "FILL %08lX %04X WORD", ulNetxAddress, usData);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( expect_string(&tBuffer, "\n>")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from netx: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): write_data16: 0x%08lx = 0x%04x\n", m_pcName, this, ulNetxAddress, usData);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


/* write a long (32bit) from the pc to the netx */
void romloader_usb::write_data32(lua_State *ptClientData, unsigned long ulNetxAddress, unsigned long ulData)
{
	int iResult;
	char acCommand[28];
	DATA_BUFFER_T tBuffer;
	unsigned int uiDataLen;
	bool fOk;


	// assume failure
	fOk = false;

	// construct the command
	snprintf(acCommand, sizeof(acCommand), "FILL %08lX %08X LONG", ulNetxAddress, ulData);

	if( m_fIsConnected==false )
	{
		MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): not connected!", m_pcName, this);
	}
	else
	{
		// send the command
		iResult = usb_executeCommand(acCommand, &tBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): failed to send command: %d:%s", m_pcName, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			if( expect_string(&tBuffer, "\n>")!=true )
			{
				MUHKUH_PLUGIN_PUSH_ERROR(ptClientData, "%s(%p): strange response from netx: %s", m_pcName, this, tBuffer.pucData);
			}
			else
			{
				printf("%s(%p): write_data32: 0x%08lx = 0x%08x\n", m_pcName, this, ulNetxAddress, ulData);
				fOk = true;
			}

			free(tBuffer.pucData);
		}
	}

	if( fOk!=true )
	{
		MUHKUH_PLUGIN_EXIT_ERROR(ptClientData);
	}
}


/* write a byte array from the pc to the netx */
void romloader_usb::write_image(unsigned long ulNetxAddress, const char *pcInputData, unsigned long ulInputData, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
/*
	unsigned long ulNetxAddress;
	int iResult;
	bool fOk;
	wxString strErrorMsg;
	wxString strResponse;


	ulNetxAddress = (unsigned long)dNetxAddress;

	// expect error
	fOk = false;

	if( m_fIsConnected==false )
	{
		strErrorMsg = _("not connected!");
	}
	else
	{
		// send the command
		iResult = usb_load(strData.To8BitData(), strData.Len(), ulNetxAddress, L, iLuaCallbackTag, pvCallbackUserData);
		if( iResult!=LIBUSB_SUCCESS )
		{
			strErrorMsg.Printf(_("failed to send load command: %s"), libusb_strerror(iResult));
		}
		else
		{
			// get the response
			iResult = usb_getNetxData(strResponse, NULL, 0, NULL);
			if( iResult!=LIBUSB_SUCCESS )
			{
				strErrorMsg.Printf(_("failed to get command response: %s"), libusb_strerror(iResult));
			}
			else
			{
				// check the response
				if( strResponse.Cmp(wxT("\n>"))==0 )
				{
					// ok!
					fOk = true;
				}
				else
				{
					strErrorMsg = wxT("strange response from netx: ") + strResponse;
				}
			}
		}
	}

	if( fOk!=true )
	{
		muhkuh_log_error("%s%s", m_acMe, strErrorMsg.fn_str());
		m_ptLuaState->wxlua_Error(strErrorMsg);
	}
*/
}


/* call routine */
void romloader_usb::call(unsigned long ulNetxAddress, unsigned long ulParameterR0, SWIGLUA_REF tLuaFn, long lCallbackUserData)
{
/*
	int iResult;
	wxString strErrorMsg;
	wxString strResponse;
	bool fOk;


	fOk = false;

	if( m_fIsConnected==false )
	{
		strErrorMsg = _("not connected!");
	}
	else
	{
		// send the command
		iResult = usb_call(ulNetxAddress, ulParameterR0, L, iLuaCallbackTag, pvCallbackUserData);
		if( iResult!=LIBUSB_SUCCESS )
		{
			strErrorMsg.Printf((_("%sfailed to send command: %s"), m_acMe, libusb_strerror(iResult)));
		}
		else
		{
			fOk = true;
		}
	}

	if( fOk!=true )
	{
		muhkuh_log_error("%s%s", m_acMe, strErrorMsg.fn_str());
		m_ptLuaState->wxlua_Error(strErrorMsg);
	}
*/
}


/*-------------------------------------*/
/*
int romloader_usb::usb_load(const char *pcData, size_t sizDataLen, unsigned long ulLoadAdr, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData)
{
	const unsigned char *pucDataCnt, *pucDataEnd;
	int iResult;
	unsigned int uiCrc;
	wxString strCommand;
	unsigned char aucBufSend[64];
	unsigned char aucBufRec[64];
	size_t sizChunkSize;
	bool fIsRunning;
	long lBytesProcessed;


	pucDataCnt = (const unsigned char*)pcData;
	pucDataEnd = pucDataCnt + sizDataLen;
	// generate crc checksum
	uiCrc = 0xffff;
	// loop over all bytes
	while( pucDataCnt<pucDataEnd )
	{
		uiCrc = crc16(uiCrc, *(pucDataCnt++));
	}

	// generate load command
	strCommand.Printf(wxT("LOAD %08lX %08X %04X"), ulLoadAdr, sizDataLen, uiCrc);

	// send the command
	iResult = usb_sendCommand(strCommand);
	if( iResult==LIBUSB_SUCCESS )
	{
		// now send the data part
		pucDataCnt = (const unsigned char*)pcData;
		lBytesProcessed = 0;
		while( pucDataCnt<pucDataEnd )
		{
			// get the size of the next data chunk
			sizChunkSize = pucDataEnd - pucDataCnt;
			if( sizChunkSize>63 )
			{
				sizChunkSize = 63;
			}
			// copy data to the packet
			memcpy(aucBufSend+1, pucDataCnt, sizChunkSize);
			aucBufSend[0] = sizChunkSize+1;

			fIsRunning = callback_long(L, iLuaCallbackTag, lBytesProcessed, pvCallbackUserData);
			if( fIsRunning!=true )
			{
				iResult = LIBUSB_ERROR_INTERRUPTED;
				break;
			}

			iResult = libusb_exchange(aucBufSend, aucBufRec);
			if( iResult!=LIBUSB_SUCCESS )
			{
				break;
			}
			pucDataCnt += sizChunkSize;
			lBytesProcessed += sizChunkSize;
		}

		if( pucDataCnt==pucDataEnd )
		{
			iResult = LIBUSB_SUCCESS;
		}
	}

	return iResult;
}


int romloader_usb::usb_call(unsigned long ulNetxAddress, unsigned long ulParameterR0, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData)
{
	int iResult;
	wxString strCommand;
	unsigned char aucSend[64];
	unsigned char aucRec[64];
	bool fIsRunning;
	wxString strResponse;
	wxString strCallbackData;
	size_t sizChunkRead;
	unsigned char *pucBuf;
	unsigned char *pucCnt, *pucEnd;
	unsigned char aucSbuf[2] = { 0, 0 };


	// construct the "call" command
	strCommand.Printf(wxT("CALL %08lX %08X"), ulNetxAddress, ulParameterR0);

	// send the command
	iResult = usb_sendCommand(strCommand);
	if( iResult==LIBUSB_SUCCESS )
	{
		// wait for the call to finish
		do
		{
			// send handshake
			aucSend[0] = 0x00;
			iResult = libusb_writeBlock(aucSend, 64, 200);
			if( iResult==LIBUSB_SUCCESS )
			{
				do
				{
					// execute callback
					fIsRunning = callback_string(L, iLuaCallbackTag, strCallbackData, pvCallbackUserData);
					strCallbackData.Empty();
					if( fIsRunning!=true )
					{
						iResult = LIBUSB_ERROR_INTERRUPTED;
					}
					else
					{
						// look for data from netx
						iResult = libusb_readBlock(aucRec, 64, 200);
						if( iResult==LIBUSB_SUCCESS )
						{
							iResult = LIBUSB_ERROR_TIMEOUT;

							// received netx data, check for prompt
							sizChunkRead = aucRec[0];
							if( sizChunkRead>1 && sizChunkRead<=64 )
							{
								// get data
								// NOTE: use Append to make a real copy
								strCallbackData.Append( wxString::From8BitData((const char*)(aucRec+1), sizChunkRead-1) );

								// last packet has '\n>' at the end
								if( sizChunkRead>2 && aucRec[sizChunkRead-2]=='\n' && aucRec[sizChunkRead-1]=='>' )
								{
									// send the rest of the data
									callback_string(L, iLuaCallbackTag, strCallbackData, pvCallbackUserData);
									iResult = LIBUSB_SUCCESS;
								}
							}
							break;
						}
					}
				} while( iResult==LIBUSB_ERROR_TIMEOUT );
			}
		} while( iResult==LIBUSB_ERROR_TIMEOUT );
	}

	if( iResult==LIBUSB_SUCCESS )
	{
		aucSend[0] = 0x00;
		iResult = libusb_exchange(aucSend, aucRec);
	}

	return iResult;
}
*/

int romloader_usb::usb_sendCommand(const char *pcCommand)
{
	int iResult;
	size_t sizCmdLen;
	unsigned char abSend[64];
	unsigned char abRec[64];
	int iCmdLen;


	// check the command size
	// Commands must fit into one usb packet of 64 bytes, the first byte
	// is the length and the last byte must be 0x0a. This means the max
	// command size is 62 bytes.
	sizCmdLen = strlen(pcCommand);
	if( sizCmdLen>62 )
	{
		printf("%s(%p): command exceeds maximum length of 62 chars: %s\n", m_pcName, this, pcCommand);
		iResult = LIBUSB_ERROR_OVERFLOW;
	}
	else
	{
		printf("%s(%p): send command '%s'\n", m_pcName, this, pcCommand);

		// construct the command
		memcpy(abSend+1, pcCommand, sizCmdLen);
		abSend[0] = sizCmdLen+2;
		abSend[sizCmdLen+1] = 0x0a;

		// send the command
		iResult = libusb_exchange(abSend, abRec);
		if( iResult==LIBUSB_SUCCESS )
		{
			// terminate command
			abSend[0] = 0x00;
			iResult = libusb_exchange(abSend, abRec);
		}
	}

	return iResult;
}


int romloader_usb::usb_getNetxData(DATA_BUFFER_T *ptBuffer, SWIGLUA_REF *ptLuaFn, long lCallbackUserData)
{
	int iResult;
	unsigned char aucSendBuf[64];
	unsigned char aucRecBuf[64];
	size_t sizChunk;
	size_t sizBuffer;
	size_t sizNewBuffer;
	size_t sizBufferPos;
	unsigned char *pucBuffer;
	unsigned char *pucNewBuffer;


	// the buffer is empty
	sizBufferPos = 0;

	// clear the send buffer
	memset(aucSendBuf, 0, sizeof(aucSendBuf));

	// init buffer with 4096 bytes
	sizBuffer = 4096;
	pucBuffer = (unsigned char*)malloc(sizBuffer);
	if( pucBuffer==NULL )
	{
		iResult = LIBUSB_ERROR_NO_MEM;
	}
	else
	{
		// receive netx data
		do
		{
			iResult = libusb_exchange(aucSendBuf, aucRecBuf);
			if( iResult!=LIBUSB_SUCCESS )
			{
				break;
			}
			else
			{
				sizChunk = aucRecBuf[0];

				if( sizChunk!=0 )
				{
					// is still enough space in the buffer left?
					if( sizBufferPos+sizChunk>sizBuffer )
					{
						// no -> double the buffer
						sizNewBuffer *= 2;
						if( sizBuffer>sizNewBuffer )
						{
							iResult = LIBUSB_ERROR_NO_MEM;
							break;
						}
						else
						{
							pucNewBuffer = (unsigned char*)realloc(pucBuffer, sizBuffer);
							if( pucNewBuffer==NULL )
							{
								// out of memory
								iResult = LIBUSB_ERROR_NO_MEM;
								break;
							}
							else
							{
								pucBuffer = pucNewBuffer;
								sizBuffer = sizNewBuffer;
							}
						}
					}

					// copy the new data chunk to the buffer
					memcpy(pucBuffer+sizBufferPos, aucRecBuf+1, sizChunk);
					sizBufferPos += sizChunk;
				}
			}
		} while( sizChunk!=0 );

		if( iResult==LIBUSB_SUCCESS )
		{
			// shrink buffer to optimal size
			if( sizBufferPos<sizBuffer )
			{
				// do not resize with length=0, this would free the memory!
				if( sizBufferPos==0 )
				{
					sizBufferPos = 1;
				}

				pucNewBuffer = (unsigned char*)realloc(pucBuffer, sizBufferPos);
				if( pucNewBuffer!=NULL )
				{
					pucBuffer = pucNewBuffer;
					sizBuffer = sizBufferPos;
				}
			}
		}
		else
		{
			if( pucBuffer!=NULL )
			{
				free(pucBuffer);
				pucBuffer = NULL;
				sizBufferPos = 0;
			}
		}
	}

	ptBuffer->pucData = pucBuffer;
	ptBuffer->sizData = sizBufferPos;
	ptBuffer->sizPos = 0;

	return iResult;
}


int romloader_usb::usb_executeCommand(const char *pcCommand, DATA_BUFFER_T *ptBuffer)
{
	int iResult;
	SWIGLUA_REF tRef;


	tRef.L = NULL;
	tRef.ref = 0;

	/* send the command */
	iResult = usb_sendCommand(pcCommand);
	if( iResult==LIBUSB_SUCCESS )
	{
		/* get the response */
		iResult = usb_getNetxData(ptBuffer, &tRef, 0);
	}

	return iResult;
}


/*-------------------------------------*/


int romloader_usb::libusb_closeDevice(void)
{
	int iResult;


	/* release the interface */
	iResult = libusb_release_interface(m_ptUsbDevHandle, 0);
	if( iResult!=LIBUSB_SUCCESS )
	{
		/* failed to release interface */
		printf("%s(%p): failed to release the usb interface: %d:%s\n", m_pcName, this, iResult, libusb_strerror(iResult));
	}
	else
	{
		/* close the netx device */
		libusb_close(m_ptUsbDevHandle);
	}

	return iResult;
}


int romloader_usb::libusb_resetDevice(void)
{
	int iResult;


	iResult = libusb_reset_device(m_ptUsbDevHandle);
	if( iResult==LIBUSB_SUCCESS )
	{
		libusb_close(m_ptUsbDevHandle);
	}
	else if( iResult==LIBUSB_ERROR_NOT_FOUND )
	{
		// the old device is already gone -> that's good, ignore the error
		libusb_close(m_ptUsbDevHandle);
		iResult = LIBUSB_SUCCESS;
	}

	return iResult;
}


int romloader_usb::libusb_readBlock(unsigned char *pucReceiveBuffer, unsigned int uiSize, int iTimeoutMs)
{
	int iRet;
	int iSize;
	int iTransfered;


	iSize = (int)uiSize;

	iRet = libusb_bulk_transfer(m_ptUsbDevHandle, 0x81, pucReceiveBuffer, iSize, &iTransfered, iTimeoutMs);
	return iRet;
}


int romloader_usb::libusb_writeBlock(unsigned char *pucSendBuffer, unsigned int uiSize, int iTimeoutMs)
{
	int iRet;
	int iSize;
	int iTransfered;


	iSize = (int)uiSize;

	iRet = libusb_bulk_transfer(m_ptUsbDevHandle, 0x01, pucSendBuffer, iSize, &iTransfered, iTimeoutMs);
	return iRet;
}


int romloader_usb::libusb_exchange(unsigned char *pucSendBuffer, unsigned char *pucReceiveBuffer)
{
	int iResult;


	iResult = libusb_writeBlock(pucSendBuffer, 64, 200);
	if( iResult==LIBUSB_SUCCESS )
	{
		iResult = libusb_readBlock(pucReceiveBuffer, 64, 200);
	}
	return iResult;
}


/*-------------------------------------*/


romloader_usb_reference::romloader_usb_reference(void)
 : muhkuh_plugin_reference()
{
}


romloader_usb_reference::romloader_usb_reference(const char *pcName, const char *pcTyp, bool fIsUsed, romloader_usb_provider *ptProvider)
 : muhkuh_plugin_reference(pcName, pcTyp, fIsUsed, ptProvider)
{
}


romloader_usb_reference::romloader_usb_reference(const romloader_usb_reference *ptCloneMe)
 : muhkuh_plugin_reference(ptCloneMe)
{
}


/*-------------------------------------*/





