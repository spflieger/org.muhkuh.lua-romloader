/***************************************************************************
 *   Copyright (C) 2010 by Christoph Thelen                                *
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
#include <stddef.h>
#include <stdio.h>

#include "romloader_usb_device_libusb.h"
#include "netx/src/usbmonitor_commands.h"

#include <stdlib.h>

#if defined(WIN32)
#	define snprintf _snprintf
#	define SLEEP_MS(ms) Sleep(ms)
#else
#	include <sys/time.h>
#	include <unistd.h>
#	define SLEEP_MS(ms) usleep(ms*1000)
#endif

#include "romloader_usb_main.h"
#include "../uuencoder.h"

#include "netx/targets/usbmon_netx10_intram.h"
#include "netx/targets/usbmon_netx500_intram.h"


#if defined(WIN32)
	const char *romloader_usb_device_libusb::m_pcLibUsb_BusPattern = "bus-%u";
	const char *romloader_usb_device_libusb::m_pcLibUsb_DevicePattern = "\\\\.\\libusb0-%u";
#else
	const char *romloader_usb_device_libusb::m_pcLibUsb_BusPattern = "%u";
	const char *romloader_usb_device_libusb::m_pcLibUsb_DevicePattern = "%u";
#endif


#if ROMLOADER_USB_LIBUSB_VERSION==0
unsigned char libusb_get_bus_number(libusb_device *dev)
{
	return (unsigned char)(dev->bus->location);
}
unsigned char libusb_get_device_address(libusb_device *dev)
{
	return dev->devnum;
}

#define libusb_device_descriptor usb_device_descriptor
int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc)
{
	memcpy(desc, &(dev->descriptor), sizeof(struct libusb_device_descriptor));
	return LIBUSB_SUCCESS;
}

#define libusb_get_descriptor usb_get_descriptor

int libusb_init(libusb_context **ctx)
{
	usb_init();
	*ctx = (libusb_context*)1;
	return LIBUSB_SUCCESS;
}

void libusb_exit(libusb_context *ctx)
{
}

void libusb_set_debug(libusb_context *ctx, int level)
{
}

#define libusb_reset_device usb_reset
#define libusb_close usb_close

int libusb_open(libusb_device *ptDevice, libusb_device_handle **pptDevHandle)
{
	libusb_device_handle *ptDevHandle;
	int iError;


	ptDevHandle = usb_open(ptDevice);
	if( ptDevHandle!=NULL )
	{
		*pptDevHandle = ptDevHandle;
		iError = LIBUSB_SUCCESS;
	}
	else
	{
		*pptDevHandle = NULL;
		iError = LIBUSB_ERROR_INVALID_PARAM;
	}

	return iError;
}

	
#define libusb_claim_interface usb_claim_interface
#define libusb_release_interface usb_release_interface

#define libusb_set_configuration usb_set_configuration

libusb_device *libusb_ref_device(libusb_device *dev)
{
	return dev;
}

void libusb_unref_device(libusb_device *dev)
{
}

int libusb_bulk_transfer(libusb_device_handle *ptDevHandle, unsigned char ucEndPoint, unsigned char *pucData, int iLength, int *piProcessed, unsigned int uiTimeoutMs)
{
	int iError;


	if( ucEndPoint>=0x80 )
	{
		iError = usb_bulk_read(ptDevHandle, ucEndPoint, (char*)pucData, iLength, uiTimeoutMs);
	}
	else
	{
		iError = usb_bulk_write(ptDevHandle, ucEndPoint, (char*)pucData, iLength, uiTimeoutMs);
	}
	if( iError>=0 )
	{
		/* transfer ok! */
		if( piProcessed!=NULL )
		{
			*piProcessed = iError;
		}
		iError = LIBUSB_SUCCESS;
	}
	else
	{
		/* transfer failed */
		if( piProcessed!=NULL )
		{
			*piProcessed = 0;
		}
	}

	return iError;
}

ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
	ssize_t sizDevices;
	ssize_t sizDeviceCnt;
        struct usb_bus *ptBusses;
        struct usb_bus *ptBus;
        struct usb_device *ptDev;
	libusb_device **pptDeviceList;

	usb_find_busses();
	usb_find_devices();

	ptBusses = usb_get_busses();

	/* Count devices. */
	sizDevices = 0;

	/* Loop over all busses. */
	ptBus = ptBusses;
	while( ptBus!=NULL )
	{
		/* loop over all devices */
		ptDev = ptBus->devices;
		while( ptDev!=NULL )
		{
			++sizDevices;
			/* next device */
			ptDev = ptDev->next;
		}
		/* scan next bus */
		ptBus = ptBus->next;
	}

	/* Allocate a list for all devices. */
	pptDeviceList = (libusb_device**)malloc(sizDevices*sizeof(libusb_device**));

	/* Loop over all busses. */
	sizDeviceCnt = 0;
	ptBus = ptBusses;
	while( ptBus!=NULL )
	{
		/* loop over all devices */
		ptDev = ptBus->devices;
		while( ptDev!=NULL )
		{
			pptDeviceList[sizDeviceCnt] = ptDev;
			++sizDeviceCnt;
			/* next device */
			ptDev = ptDev->next;
		}
		/* scan next bus */
		ptBus = ptBus->next;
	}

	*list = pptDeviceList;
	return sizDevices;
}

void libusb_free_device_list(libusb_device **list, int unref_devices)
{
	free(list);
}

enum libusb_descriptor_type
{ 
	LIBUSB_DT_DEVICE = 0x01,
	LIBUSB_DT_CONFIG = 0x02,
	LIBUSB_DT_STRING = 0x03,
	LIBUSB_DT_INTERFACE = 0x04,
	LIBUSB_DT_ENDPOINT = 0x05,
	LIBUSB_DT_HID = 0x21,
	LIBUSB_DT_REPORT = 0x22,
	LIBUSB_DT_PHYSICAL = 0x23,
	LIBUSB_DT_HUB = 0x29
};

#endif


romloader_usb_device_libusb::romloader_usb_device_libusb(const char *pcPluginId)
 : romloader_usb_device(pcPluginId)
 , m_ucEndpoint_In(0)
 , m_ucEndpoint_Out(0)
 , m_iConfiguration(1)
 , m_iInterface(0)
 , m_ptLibUsbContext(NULL)
 , m_ptDevHandle(NULL)
{
	libusb_init(&m_ptLibUsbContext);
	libusb_set_debug(m_ptLibUsbContext, 3);
}


romloader_usb_device_libusb::~romloader_usb_device_libusb(void)
{
	Disconnect();

	if( m_ptLibUsbContext!=NULL )
	{
		libusb_exit(m_ptLibUsbContext);
	}
}


const char *romloader_usb_device_libusb::m_pcPluginNamePattern = "romloader_usb_%02x_%02x";


int romloader_usb_device_libusb::libusb_reset_and_close_device(void)
{
	int iResult;


	if( m_ptDevHandle!=NULL )
	{
		iResult = libusb_reset_device(m_ptDevHandle);
		if( iResult==LIBUSB_SUCCESS || iResult==LIBUSB_ERROR_NOT_FOUND )
		{
			libusb_close(m_ptDevHandle);
			m_ptDevHandle = NULL;
			iResult = LIBUSB_SUCCESS;
		}
	}
	else
	{
		/* No open device found. */
		iResult = LIBUSB_ERROR_NOT_FOUND;
	}

	return iResult;
}


int romloader_usb_device_libusb::detect_interfaces(romloader_usb_reference ***ppptReferences, size_t *psizReferences, romloader_usb_provider *ptProvider)
{
	int iResult;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;
//	libusb_device_handle *ptDevHandle;
	unsigned int uiBusNr;
	unsigned int uiDevAdr;
	bool fDeviceIsBusy;
	bool fDeviceIsNetx;
	romloader_usb_reference *ptRef;
	const size_t sizMaxName = 32;
	char acName[sizMaxName];

	size_t sizRefCnt;
	size_t sizRefMax;
	romloader_usb_reference **pptRef;
	romloader_usb_reference **pptRefNew;


	/* Expect success. */
	iResult = 0;

	/* Init References array. */
	sizRefCnt = 0;
	sizRefMax = 16;
	pptRef = (romloader_usb_reference**)malloc(sizRefMax*sizeof(romloader_usb_reference*));
	if( pptRef==NULL )
	{
		fprintf(stderr, "out of memory!\n");
		iResult = -1;
	}
	else
	{
		/* detect devices */
		ptDeviceList = NULL;
		ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
		if( ssizDevList<0 )
		{
			/* failed to detect devices */
			fprintf(stderr, "%s(%p): failed to detect usb devices: %d:%s\n", m_pcPluginId, this, ssizDevList, libusb_strerror(ssizDevList));
			iResult = -1;
		}
		else
		{
			/* loop over all devices */
			ptDevCnt = ptDeviceList;
			ptDevEnd = ptDevCnt + ssizDevList;
			while( ptDevCnt<ptDevEnd )
			{
				ptDev = *ptDevCnt;
				fDeviceIsNetx = fIsDeviceNetx(ptDev);
				if( fDeviceIsNetx==true )
				{
					/* construct the name */
					uiBusNr = libusb_get_bus_number(ptDev);
					uiDevAdr = libusb_get_device_address(ptDev);
					snprintf(acName, sizMaxName-1, m_pcPluginNamePattern, uiBusNr, uiDevAdr);

					/* open the device */
					iResult = libusb_open(ptDev, &m_ptDevHandle);
					if( iResult!=LIBUSB_SUCCESS )
					{
						/* failed to open the interface, do not add it to the list */
						fprintf(stderr, "%s(%p): failed to open device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
					}
					else
					{
						/* set the configuration */
						iResult = libusb_set_configuration(m_ptDevHandle, m_iConfiguration);
						if( iResult!=LIBUSB_SUCCESS )
						{
							/* failed to set the configuration */
							fprintf(stderr, "%s(%p): failed to set the configuration of device %s: %d:%s\n", m_pcPluginId, this, acName, iResult, libusb_strerror(iResult));
						}
						else
						{
							/* claim the interface, 0 is the interface number */
							iResult = libusb_claim_interface(m_ptDevHandle, m_iInterface);
							if( iResult!=LIBUSB_SUCCESS && iResult!=LIBUSB_ERROR_BUSY )
							{
								/* failed to claim the interface */
								fprintf(stderr, "%s(%p): failed to claim interface %d of device %s: %d:%s\n", m_pcPluginId, this, m_iInterface, acName, iResult, libusb_strerror(iResult));
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
									iResult = libusb_release_interface(m_ptDevHandle, m_iInterface);
									if( iResult!=LIBUSB_SUCCESS )
									{
										/* failed to release the interface */
										fprintf(stderr, "%s(%p): failed to release interface %d of device %s after a successful claim: %d:%s\n", m_pcPluginId, this, m_iInterface, acName, iResult, libusb_strerror(iResult));
									}
								}

								/* create the new instance */
								ptRef = new romloader_usb_reference(acName, m_pcPluginId, fDeviceIsBusy, ptProvider);
								/* Is enough space in the array for one more entry? */
								if( sizRefCnt>=sizRefMax )
								{
									/* No -> expand the array. */
									sizRefMax *= 2;
									/* Detect overflow or limitation. */
									if( sizRefMax<=sizRefCnt )
									{
										iResult = -1;
										break;
									}
									/* Reallocate the array. */
									pptRefNew = (romloader_usb_reference**)realloc(pptRef, sizRefMax*sizeof(romloader_usb_reference*));
									if( pptRefNew==NULL )
									{
										iResult = -1;
										break;
									}
									pptRef = pptRefNew;
								}
								pptRef[sizRefCnt++] = ptRef;
							}
						}

						/* close the device */
						libusb_close(m_ptDevHandle);
						m_ptDevHandle = NULL;
					}
				}
				/* next list item */
				++ptDevCnt;
			}
			/* free the device list */
			if( ptDeviceList!=NULL )
			{
				libusb_free_device_list(ptDeviceList, 1);
			}
		}
	}

	if( iResult!=0 && pptRef!=NULL )
	{
		/* Free all references in the array. */
		while( sizRefCnt>0 )
		{
			--sizRefCnt;
			ptRef = pptRef[sizRefCnt];
			if( ptRef!=NULL )
			{
				delete ptRef;
			}
		}
		/* Free the array. */
		free(pptRef);
		/* No more array. */
		pptRef = NULL;
		sizRefCnt = 0;
	}

	*ppptReferences = pptRef;
	*psizReferences = sizRefCnt;

	return iResult;
}


libusb_device *romloader_usb_device_libusb::find_netx_device(libusb_device **ptDeviceList, ssize_t ssizDevList, unsigned int uiBusNr, unsigned int uiDeviceAdr)
{
	libusb_device **ptDevCnt;
	libusb_device **ptDevEnd;
	libusb_device *ptDev;
	libusb_device *ptNetxDevice;
	const NETX_USB_DEVICE_T *ptId;


	/* No netx found. */
	ptNetxDevice = NULL;

	/* loop over all devices */
	ptDevCnt = ptDeviceList;
	ptDevEnd = ptDevCnt + ssizDevList;
	while( ptNetxDevice==NULL && ptDevCnt<ptDevEnd )
	{
		ptDev = *ptDevCnt;
		if( libusb_get_bus_number(ptDev)==uiBusNr && libusb_get_device_address(ptDev)==uiDeviceAdr )
		{
			ptId = identifyDevice(ptDev);
			if( ptId!=NULL )
			{
				m_tChiptyp = ptId->tChiptyp;
				m_tRomcode = ptId->tRomcode;
				m_ucEndpoint_In = ptId->ucEndpoint_In;
				m_ucEndpoint_Out = ptId->ucEndpoint_Out;
				ptNetxDevice = ptDev;
			}
			break;
		}

		++ptDevCnt;
	}

	return ptNetxDevice;
}


int romloader_usb_device_libusb::setup_netx_device(libusb_device *ptNetxDevice)
{
	int iResult;


//	printf("%s(%p): open device.\n", m_pcPluginId, this);
	iResult = libusb_open(ptNetxDevice, &m_ptDevHandle);
	if( iResult!=LIBUSB_SUCCESS )
	{
		fprintf(stderr, "%s(%p): failed to open the device: %d:%s", m_pcPluginId, this, iResult, libusb_strerror(iResult));
	}
	else
	{
		/* Set the configuration. */
//		printf("%s(%p): set device configuration to %d.\n", m_pcPluginId, this, m_iConfiguration);

		iResult = libusb_set_configuration(m_ptDevHandle, m_iConfiguration);
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* failed to set the configuration */
			fprintf(stderr, "%s(%p): failed to set the configuration of device: %d:%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
		}
		else
		{
			/* Claim the interface. */
//			printf("%s(%p): claim interface %d.\n", m_pcPluginId, this, m_iInterface);
			iResult = libusb_claim_interface(m_ptDevHandle, m_iInterface);
			if( iResult!=LIBUSB_SUCCESS )
			{
				/* Failed to claim the interface. */
				fprintf(stderr, "%s(%p): failed to claim interface %d: %d:%s\n", m_pcPluginId, this, m_iInterface, iResult, libusb_strerror(iResult));
			}
		}

		/* Cleanup in error case. */
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* Close the device. */
			libusb_close(m_ptDevHandle);
			m_ptDevHandle = NULL;
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::Connect(unsigned int uiBusNr, unsigned int uiDeviceAdr)
{
	int iResult;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device *ptUsbDevice;
	libusb_device *ptUpdatedNetxDevice;


	ptDeviceList = NULL;

	/* Search device with bus and address. */
	ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
	if( ssizDevList<0 )
	{
		/* Failed to detect devices. */
		fprintf(stderr, "%s(%p): failed to detect usb devices: %d:%s\n", m_pcPluginId, this, ssizDevList, libusb_strerror(ssizDevList));
		iResult = (int)ssizDevList;
	}
	else
	{
		ptUsbDevice = find_netx_device(ptDeviceList, ssizDevList, uiBusNr, uiDeviceAdr);
		if( ptUsbDevice==NULL )
		{
			fprintf(stderr, "%s(%p): interface not found. Maybe it was plugged out.\n", m_pcPluginId, this);
			iResult = LIBUSB_ERROR_NOT_FOUND;
		}
		else
		{
			/* Reference the device before freeing the list. */
			libusb_ref_device(ptUsbDevice);
			iResult = LIBUSB_SUCCESS;
		}

		/* free the device list */
		libusb_free_device_list(ptDeviceList, 1);

		if( iResult==LIBUSB_SUCCESS )
		{
			/* Does this device need an update? */
			iResult = LIBUSB_ERROR_OTHER;
			switch(m_tRomcode)
			{
			case ROMLOADER_ROMCODE_UNKNOWN:
				iResult = LIBUSB_ERROR_OTHER;
				break;

			case ROMLOADER_ROMCODE_ABOOT:
			case ROMLOADER_ROMCODE_HBOOT:
				iResult = update_old_netx_device(ptUsbDevice, &ptUpdatedNetxDevice);
				if( iResult==LIBUSB_SUCCESS )
				{
					/* Free the old device. */
					libusb_unref_device(ptUsbDevice);
					/* Use the updated device. */
					ptUsbDevice = ptUpdatedNetxDevice;
				}
				break;

			case ROMLOADER_ROMCODE_HBOOT2_SOFT:
				/* The device uses the hboot v2 protocol. */
				iResult = LIBUSB_SUCCESS;
				break;
			}

			if( iResult==LIBUSB_SUCCESS )
			{
				iResult = setup_netx_device(ptUsbDevice);
				if( iResult!=LIBUSB_SUCCESS )
				{
					fprintf(stderr, "%s(%p): failed to setup the device, trying to reset it.\n", m_pcPluginId, this);

					/* Reset the device and try again. */
					iResult = libusb_reset_and_close_device();
					if( iResult!=LIBUSB_SUCCESS )
					{
						fprintf(stderr, "%s(%p): failed to reset the netx, giving up: %d:%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
						libusb_release_interface(m_ptDevHandle, m_iInterface);
						libusb_close(m_ptDevHandle);
						m_ptDevHandle = NULL;
					}
					else
					{
						fprintf(stderr, "%s(%p): reset ok!\n", m_pcPluginId, this);

						iResult = setup_netx_device(ptUsbDevice);
						if( iResult!=LIBUSB_SUCCESS )
						{
							/* Free the old device. */
							fprintf(stderr, "%s(%p): lost device after reset!\n", m_pcPluginId, this);
							iResult = LIBUSB_ERROR_OTHER;
						}
					}

					libusb_unref_device(ptUsbDevice);
				}
			}
		}
	}

	return iResult;
}


void romloader_usb_device_libusb::Disconnect(void)
{
	if( m_ptDevHandle!=NULL )
	{
		libusb_release_interface(m_ptDevHandle, m_iInterface);
		libusb_close(m_ptDevHandle);
		m_ptDevHandle = NULL;
	}
	m_ptDevHandle = NULL;
}


bool romloader_usb_device_libusb::fIsDeviceNetx(libusb_device *ptDevice) const
{
	bool fDeviceIsNetx;
	int iResult;
	struct libusb_device_descriptor sDevDesc;
	const NETX_USB_DEVICE_T *ptDevCnt;
	const NETX_USB_DEVICE_T *ptDevEnd;


	/* Be pessimistic. */
	fDeviceIsNetx = false;

	if( ptDevice!=NULL )
	{
		iResult = libusb_get_device_descriptor(ptDevice, &sDevDesc);
		if( iResult==LIBUSB_SUCCESS )
		{
			ptDevCnt = atNetxUsbDevices;
			ptDevEnd = atNetxUsbDevices + (sizeof(atNetxUsbDevices)/sizeof(atNetxUsbDevices[0]));
			while( ptDevCnt<ptDevEnd )
			{
				if(
					sDevDesc.idVendor==ptDevCnt->usVendorId &&
					sDevDesc.idProduct==ptDevCnt->usDeviceId &&
					sDevDesc.bcdDevice==ptDevCnt->usBcdDevice
				)
				{
					/* Found a matching device. */
					fDeviceIsNetx = true;
					break;
				}
				else
				{
					++ptDevCnt;
				}
			}
		}
	}

	return fDeviceIsNetx;
}


const NETX_USB_DEVICE_T *romloader_usb_device_libusb::identifyDevice(libusb_device *ptDevice) const
{
	const NETX_USB_DEVICE_T *ptDevHit;
	int iResult;
	struct libusb_device_descriptor sDevDesc;
	const NETX_USB_DEVICE_T *ptDevCnt;
	const NETX_USB_DEVICE_T *ptDevEnd;
	libusb_device_handle *ptDevHandle;


	ptDevHit = NULL;
	if( ptDevice!=NULL )
	{
		/* Try to open the device. */
		iResult = libusb_open(ptDevice, &ptDevHandle);
		if( iResult==LIBUSB_SUCCESS )
		{
			iResult = libusb_get_descriptor(ptDevHandle, LIBUSB_DT_DEVICE, 0, &sDevDesc, sizeof(struct libusb_device_descriptor));
			if( iResult==sizeof(struct libusb_device_descriptor) )
			{
				ptDevCnt = atNetxUsbDevices;
				ptDevEnd = atNetxUsbDevices + (sizeof(atNetxUsbDevices)/sizeof(atNetxUsbDevices[0]));
				while( ptDevCnt<ptDevEnd )
				{
					if(
						sDevDesc.idVendor==ptDevCnt->usVendorId &&
						sDevDesc.idProduct==ptDevCnt->usDeviceId &&
						sDevDesc.bcdDevice==ptDevCnt->usBcdDevice
					)
					{
						/* Found a matching device. */
						ptDevHit = ptDevCnt;
						break;
					}
					else
					{
						++ptDevCnt;
					}
				}
			}
			libusb_close(ptDevHandle);
		}
	}

	return ptDevHit;
}


const romloader_usb_device_libusb::LIBUSB_STRERROR_T romloader_usb_device_libusb::atStrError[14] =
{
	{ LIBUSB_SUCCESS,               "success" },
	{ LIBUSB_ERROR_IO,              "input/output error" },
	{ LIBUSB_ERROR_INVALID_PARAM,   "invalid parameter" },
	{ LIBUSB_ERROR_ACCESS,          "access denied (insufficient permissions)" },
	{ LIBUSB_ERROR_NO_DEVICE,       "no such device (it may have been disconnected)" },
	{ LIBUSB_ERROR_NOT_FOUND,       "entity not found" },
	{ LIBUSB_ERROR_BUSY,            "resource busy" },
	{ LIBUSB_ERROR_TIMEOUT,         "operation timed out" },
	{ LIBUSB_ERROR_OVERFLOW,        "overflow" },
	{ LIBUSB_ERROR_PIPE,            "pipe error" },
	{ LIBUSB_ERROR_INTERRUPTED,     "system call interrupted (perhaps due to signal)" },
	{ LIBUSB_ERROR_NO_MEM,          "insufficient memory" },
	{ LIBUSB_ERROR_NOT_SUPPORTED,   "operation not supported or unimplemented on this platform" },
	{ LIBUSB_ERROR_OTHER,           "other error" }
};


const char *romloader_usb_device_libusb::libusb_strerror(int iError)
{
	const char *pcMessage = "unknown error";
	const LIBUSB_STRERROR_T *ptCnt;
	const LIBUSB_STRERROR_T *ptEnd;


	ptCnt = atStrError;
	ptEnd = atStrError + (sizeof(atStrError)/sizeof(atStrError[0]));
	while( ptCnt<ptEnd )
	{
		if(ptCnt->iError==iError)
		{
			pcMessage = ptCnt->pcMessage;
			break;
		}
		++ptCnt;
	}

	return pcMessage;
}


int romloader_usb_device_libusb::netx10_discard_until_timeout(libusb_device_handle *ptDevHandle)
{
	unsigned char aucBuffer[64];
	int iResult;
	int iProcessed;
	unsigned int uiTimeoutMs;


	uiTimeoutMs = 100;

	do
	{
		iProcessed=0;
		iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_In, aucBuffer, sizeof(aucBuffer), &iProcessed, uiTimeoutMs);
		if( iResult==LIBUSB_ERROR_TIMEOUT )
		{
			/* Timeouts are great! */
			iResult = 0;
			break;
		}
		else if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to receive data: (%d)%s\n", m_pcPluginId, this, iResult, libusb_strerror(iResult));
			break;
		}
	} while( iResult==0 );

	return iResult;
}


int romloader_usb_device_libusb::netx500_exchange_data(libusb_device_handle *ptDevHandle, const unsigned char *pucOutBuffer, unsigned char *pucInBuffer)
{
	int iResult;
	int iProcessed;
	const unsigned int uiTimeoutMs = 100;
	/* The packet size is fixed to 64 bytes in this protocol. */
	const int iPacketSize = 64;


	iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_Out, (unsigned char*)pucOutBuffer, iPacketSize, &iProcessed, uiTimeoutMs);
	if( iResult==0 )
	{
		iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_In, pucInBuffer, iPacketSize, &iProcessed, uiTimeoutMs);
	}

	return iResult;
}


int romloader_usb_device_libusb::netx500_discard_until_timeout(libusb_device_handle *ptDevHandle)
{
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};
	int iResult;


	/* Throw away all data until the end. */
	do
	{
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
		if( iResult!=LIBUSB_SUCCESS )
		{
			/* Error! */
			break;
		}
//		printf("discard packet:\n");
//		hexdump(aucInBuffer, 64);
	} while( aucInBuffer[0]!=0 );

	return iResult;
}


int romloader_usb_device_libusb::netx500_load_code(libusb_device_handle *ptDevHandle, const unsigned char* pucNetxCode, size_t sizNetxCode)
{
	unsigned short usCrc;
	size_t sizLine;
	unsigned long ulLoadAddress;
	int iResult;
	const unsigned char *pucDataCnt;
	const unsigned char *pucDataEnd;
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};
	size_t sizChunkSize;


	/* Be optimistic. */
	iResult = 0;

	if( pucNetxCode[0x00]!='m' || pucNetxCode[0x01]!='o' || pucNetxCode[0x02]!='o' || pucNetxCode[0x03]!='h' )
	{
		fprintf(stderr, "%s(%p): Invalid netx code, header missing.\n", m_pcPluginId, this);
		iResult = -1;
	}
	else
	{
		/* Get the load address. */
		ulLoadAddress  = pucNetxCode[0x04];
		ulLoadAddress |= pucNetxCode[0x05]<<8;
		ulLoadAddress |= pucNetxCode[0x06]<<16;
		ulLoadAddress |= pucNetxCode[0x07]<<24;

//		printf("Load 0x%08x bytes to 0x%08x.\n", sizNetxCode, ulLoadAddress);

		/* Generate crc16 checksum. */
		usCrc = crc16(pucNetxCode, sizNetxCode);

		/* Generate load command. */
		sizLine = snprintf((char*)(aucOutBuffer+1), sizeof(aucOutBuffer), "load %lx %x %04X\n", ulLoadAddress, sizNetxCode, usCrc);
		/* Set the length. */
		aucOutBuffer[0] = (unsigned char)(sizLine + 1);

		/* Send the command. */
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
		if( iResult==LIBUSB_SUCCESS )
		{
			/* Terminate the command. */
			aucOutBuffer[0] = 0;
			iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
			if( iResult==LIBUSB_SUCCESS )
			{
				/* Now send the data part. */
				pucDataCnt = pucNetxCode;
				pucDataEnd = pucNetxCode + sizNetxCode;
				while( pucDataCnt<pucDataEnd )
				{
					/* Get the size of the next data chunk. */
					sizChunkSize = pucDataEnd - pucDataCnt;
					if( sizChunkSize>63 )
					{
						sizChunkSize = 63;
					}
					/* Copy data to the packet. */
					memcpy(aucOutBuffer+1, pucDataCnt, sizChunkSize);
					aucOutBuffer[0] = (unsigned char)(sizChunkSize+1);

					iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
					if( iResult!=LIBUSB_SUCCESS )
					{
						break;
					}
					pucDataCnt += sizChunkSize;
				}
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx10_load_code(libusb_device_handle* ptDevHandle, const unsigned char* pucNetxCode, size_t sizNetxCode)
{
	size_t sizLine;
	unsigned long ulLoadAddress;
	unsigned char aucBuffer[64];
	unsigned int uiTimeoutMs;
	int iProcessed;
	int iResult;
	uuencoder tUuencoder;


	/* Be optimistic. */
	iResult = 0;

	uiTimeoutMs = 100;

	if( pucNetxCode[0x00]!='m' || pucNetxCode[0x01]!='o' || pucNetxCode[0x02]!='o' || pucNetxCode[0x03]!='h' )
	{
		fprintf(stderr, "%s(%p): Invalid netx code, header missing.\n", m_pcPluginId, this);
		iResult = -1;
	}
	else
	{
		/* Construct the command. */
		ulLoadAddress  = pucNetxCode[0x04];
		ulLoadAddress |= pucNetxCode[0x05]<<8U;
		ulLoadAddress |= pucNetxCode[0x06]<<16U;
		ulLoadAddress |= pucNetxCode[0x07]<<24U;
		sizLine = snprintf((char*)aucBuffer, sizeof(aucBuffer), "l %lx\n", ulLoadAddress);
		iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_Out, aucBuffer, sizLine, &iProcessed, uiTimeoutMs);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
			iResult = -1;
		}
		else if( sizLine!=iProcessed )
		{
			fprintf(stderr, "%s(%p): Requested to send %d bytes, but only %d were processed!\n", m_pcPluginId, this, sizLine, iProcessed);
			iResult = -1;
		}
		else
		{
			netx10_discard_until_timeout(ptDevHandle);

			tUuencoder.set_data(pucNetxCode, sizNetxCode);

			/* Send the data line by line with a delay of 10ms. */
			do
			{
				sizLine = tUuencoder.process((char*)aucBuffer, sizeof(aucBuffer));
				if( sizLine!=0 )
				{
					uiTimeoutMs = 100;

					iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_Out, aucBuffer, sizLine, &iProcessed, uiTimeoutMs);
					if( iResult!=0 )
					{
						fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
						iResult = -1;
						break;
					}
					else if( sizLine!=iProcessed )
					{
						fprintf(stderr, "%s(%p): Requested to send %d bytes, but only %d were processed!\n", m_pcPluginId, this, sizLine, iProcessed);
						iResult = -1;
						break;
					}

					SLEEP_MS(10);
				}
			} while( tUuencoder.isFinished()==false );

			if( iResult==0 )
			{
				netx10_discard_until_timeout(ptDevHandle);
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx500_start_code(libusb_device_handle *ptDevHandle, const unsigned char *pucNetxCode)
{
	int iResult;
	unsigned long ulExecAddress;
	size_t sizLine;
	unsigned char aucOutBuffer[64] = {0};
	unsigned char aucInBuffer[64] = {0};


	ulExecAddress  = pucNetxCode[0x08];
	ulExecAddress |= pucNetxCode[0x09]<<8U;
	ulExecAddress |= pucNetxCode[0x0a]<<16U;
	ulExecAddress |= pucNetxCode[0x0b]<<24U;

	/* Generate call command. */
	sizLine = snprintf((char*)(aucOutBuffer+1), sizeof(aucOutBuffer), "call %lx 0\n", ulExecAddress);
	/* Set the length. */
	aucOutBuffer[0] = (unsigned char)(sizLine + 1);

//	printf("Start code at 0x%08x.\n", ulExecAddress);

	/* Send the command. */
	iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
	if( iResult==LIBUSB_SUCCESS )
	{
		/* Terminate the command. */
		aucOutBuffer[0] = 0;
		iResult = netx500_exchange_data(ptDevHandle, aucOutBuffer, aucInBuffer);
	}

	return iResult;
}


int romloader_usb_device_libusb::netx10_start_code(libusb_device_handle *ptDevHandle, const unsigned char *pucNetxCode)
{
	unsigned char aucBuffer[64];
	size_t sizBlock;
	int iResult;
	int iProcessed;
	unsigned int uiTimeoutMs;
	unsigned long ulExecAddress;


	/* Construct the command. */
	ulExecAddress  = pucNetxCode[0x08];
	ulExecAddress |= pucNetxCode[0x09]<<8;
	ulExecAddress |= pucNetxCode[0x0a]<<16;
	ulExecAddress |= pucNetxCode[0x0b]<<24;

	/* Construct the command. */
	sizBlock = sprintf((char*)aucBuffer, "g %lx 0\n", ulExecAddress);

	uiTimeoutMs = 1000;
	iResult = libusb_bulk_transfer(ptDevHandle, m_ucEndpoint_Out, aucBuffer, sizBlock, &iProcessed, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send packet!\n", m_pcPluginId, this);
		iResult = -1;
	}
	else if( sizBlock!=iProcessed )
	{
		fprintf(stderr, "%s(%p): Requested to send %d bytes, but only %d were processed!\n", m_pcPluginId, this, sizBlock, iProcessed);
		iResult = -1;
	}
	else
	{
		netx10_discard_until_timeout(ptDevHandle);

		iResult = 0;
	}

	return iResult;
}


int romloader_usb_device_libusb::netx10_upgrade_romcode(libusb_device *ptDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	libusb_device_handle *ptDevHandle;


//	printf(". Found old netX10 romcode, starting download.\n");

	iResult = libusb_open(ptDevice, &ptDevHandle);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to open the device: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		/* set the configuration */
		iResult = libusb_set_configuration(ptDevHandle, 1);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to set config 1: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
		}
		else
		{
			iResult = libusb_claim_interface(ptDevHandle, 0);
			if( iResult!=0 )
			{
				fprintf(stderr, "%s(%p): Failed to claim interface 0: %s\n", m_pcPluginId, this, libusb_strerror(iResult));

				libusb_close(ptDevHandle);
			}
			else
			{
				/* Read data until 'newline''>' or timeout. */
				netx10_discard_until_timeout(ptDevHandle);

				/* Load data. */
				netx10_load_code(ptDevHandle, auc_usbmon_netx10_intram, sizeof(auc_usbmon_netx10_intram));

				/* Start the code. */
				netx10_start_code(ptDevHandle, auc_usbmon_netx10_intram);

				libusb_release_interface(ptDevHandle, m_iInterface);
				libusb_close(ptDevHandle);

				*pptUpdatedNetxDevice = ptDevice;

				iResult = 0;
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::netx500_upgrade_romcode(libusb_device *ptDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	libusb_device_handle *ptDevHandle;


	printf(". Found old netX500 romcode, starting download.\n");

	iResult = libusb_open(ptDevice, &ptDevHandle);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to open the device: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		/* set the configuration */
		iResult = libusb_set_configuration(ptDevHandle, 1);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to set config 1: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
		}
		else
		{
			iResult = libusb_claim_interface(ptDevHandle, 0);
			if( iResult!=0 )
			{
				fprintf(stderr, "%s(%p): Failed to claim interface 0: %s\n", m_pcPluginId, this, libusb_strerror(iResult));

				libusb_close(ptDevHandle);
			}
			else
			{
				/* Read data until 'newline''>' or timeout. */
				netx500_discard_until_timeout(ptDevHandle);

				/* Load data. */
				netx500_load_code(ptDevHandle, auc_usbmon_netx500_intram, sizeof(auc_usbmon_netx500_intram));
				/* Discard load response. */
				netx500_discard_until_timeout(ptDevHandle);

				/* Start the code parameter. */
				netx500_start_code(ptDevHandle, auc_usbmon_netx500_intram);

				/* FIXME: Why is this dummy read necessary? */
				{
					unsigned char aucOutBuf[64];
					unsigned char aucInBuf[64];
					size_t sizInBuf;
					int iRes;

					m_ptDevHandle = ptDevHandle;
					aucOutBuf[0x00] = USBMON_COMMAND_Read|(USBMON_ACCESSSIZE_Long<<6U);
					aucOutBuf[0x01] = 4;
					aucOutBuf[0x02] = 0;
					aucOutBuf[0x03] = 0;
					aucOutBuf[0x04] = 0;
					aucOutBuf[0x05] = 0;
					iRes = execute_command(aucOutBuf, 6, aucInBuf, &sizInBuf);
					//printf("Dummy exec: %d %d 0x%02x\n", iRes, sizInBuf, aucInBuf[0]);
				}

				/* Release the interface. */
				libusb_release_interface(ptDevHandle, m_iInterface);

				libusb_close(ptDevHandle);

				*pptUpdatedNetxDevice = ptDevice;
				iResult = 0;
			}
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::send_packet(const unsigned char *aucOutBuf, size_t sizOutBuf, unsigned int uiTimeoutMs)
{
	int iResult;
	int iProcessed;


	iResult = libusb_bulk_transfer(m_ptDevHandle, m_ucEndpoint_Out, (unsigned char*)aucOutBuf, sizOutBuf, &iProcessed, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send data: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else if( sizOutBuf!=iProcessed )
	{
		fprintf(stderr, "%s(%p): Requested to send %d bytes, but only %d were processed!\n", m_pcPluginId, this, sizOutBuf, iProcessed);
		iResult = 1;
	}

	return iResult;
}


int romloader_usb_device_libusb::receive_packet(unsigned char *aucInBuf, size_t *psizInBuf, unsigned int uiTimeoutMs)
{
	int iResult;
	int iProcessed;


	iResult = libusb_bulk_transfer(m_ptDevHandle, m_ucEndpoint_In, aucInBuf, 64, &iProcessed, uiTimeoutMs);
	if( iResult==0 )
	{
		if( iProcessed==0 )
		{
			fprintf(stderr, "%s(%p): Received empty packet!\n", m_pcPluginId, this);
			iResult = 1;
		}
		else
		{
			*psizInBuf = iProcessed;
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::execute_command(const unsigned char *aucOutBuf, size_t sizOutBuf, unsigned char *aucInBuf, size_t *psizInBuf)
{
	int iResult;
	size_t sizProcessed;
	unsigned int uiTimeoutMs;


	uiTimeoutMs = 500; // 100

	iResult = send_packet(aucOutBuf, sizOutBuf, uiTimeoutMs);
	if( iResult!=0 )
	{
		fprintf(stderr, "%s(%p): Failed to send data: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
	}
	else
	{
		
		iResult = receive_packet(aucInBuf, &sizProcessed, uiTimeoutMs);
		if( iResult!=0 )
		{
			fprintf(stderr, "%s(%p): Failed to receive data: %s\n", m_pcPluginId, this, libusb_strerror(iResult));
		}
		else
		{
//			printf("received:\n");
//			hexdump(aucInBuf, iProcessed);
			*psizInBuf = sizProcessed;
		}
	}

	return iResult;
}


int romloader_usb_device_libusb::update_old_netx_device(libusb_device *ptNetxDevice, libusb_device **pptUpdatedNetxDevice)
{
	int iResult;
	const NETX_USB_DEVICE_T *ptId;
	libusb_device *ptUpdatedDevice;


	switch(m_tChiptyp)
	{
	case ROMLOADER_CHIPTYP_UNKNOWN:
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX500:
		iResult = netx500_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;

	case ROMLOADER_CHIPTYP_NETX100:
		iResult = netx500_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;

	case ROMLOADER_CHIPTYP_NETX50:
		/* TODO: insert update code for the netX10. */
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX5:
		/* TODO: insert update code for the netX5. */
		iResult = LIBUSB_ERROR_OTHER;
		break;

	case ROMLOADER_CHIPTYP_NETX10:
		iResult = netx10_upgrade_romcode(ptNetxDevice, &ptUpdatedDevice);
		break;
	}

	/* Did the update succeed? */
	if( iResult==LIBUSB_SUCCESS )
	{
		/* Yes -> try to identify the device again. */
		ptId = identifyDevice(ptUpdatedDevice);
		if( ptId==NULL )
		{
			fprintf(stderr, "%s(%p): Failed to identify the updated device!\n", m_pcPluginId, this);
			iResult = LIBUSB_ERROR_OTHER;
		}
		else
		{
//			printf("After update: %s\n", ptId->pcName);
			/* Update the settings. */
			m_tChiptyp = ptId->tChiptyp;
			m_tRomcode = ptId->tRomcode;
			m_ucEndpoint_In = ptId->ucEndpoint_In;
			m_ucEndpoint_Out = ptId->ucEndpoint_Out;

			*pptUpdatedNetxDevice = ptUpdatedDevice;
		}
	}

	return iResult;
}


unsigned short romloader_usb_device_libusb::crc16(const unsigned char *pucData, size_t sizData)
{
	const unsigned char *pucDataCnt;
	const unsigned char *pucDataEnd;
	unsigned int uiCrc;


	pucDataCnt = pucData;
	pucDataEnd = pucData + sizData;

	uiCrc = 0xffffU;
	while( pucDataCnt<pucDataEnd )
	{
		uiCrc  = (uiCrc >> 8U) | ((uiCrc & 0xff) << 8U);
		uiCrc ^= *(pucDataCnt++);
		uiCrc ^= (uiCrc & 0xffU) >> 4U;
		uiCrc ^= (uiCrc & 0x0fU) << 12U;
		uiCrc ^= ((uiCrc & 0xffU) << 4U) << 1U;
	}

	return (unsigned short)uiCrc;
}

