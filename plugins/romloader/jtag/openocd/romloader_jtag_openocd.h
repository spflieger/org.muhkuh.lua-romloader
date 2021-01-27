#include <stddef.h>
#include <stdint.h>

#ifndef __ROMLOADER_JTAG_OPENOCD_H__
#define __ROMLOADER_JTAG_OPENOCD_H__

#include "../../romloader_def.h"

#include <libusb.h>
#if (!defined(LIBUSB_API_VERSION)) || (defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION<0x01000102)
#       error "This plugin needs at least libusb 1.0.16."
#endif

/*-------------------------------------------------------------------------*/

/* This is the maximum USB path size. For USB3.0 it is 7. */
#define USB_MAX_PATH_ELEMENTS 8
/* This is the maximum number of USB devices.
 * The possible maximum is 127 devices per bus, which would make a total number of 127*256.
 * The limit of 128 seems to be quite reasonable.
 */
#define USB_MAX_DEVICES 128


typedef int (*PFN_MUHKUH_CALL_PRINT_CALLBACK) (void *pvCallbackUserData, uint8_t *pucData, unsigned long ulDataSize);

class romloader_jtag_openocd
{
public:
	romloader_jtag_openocd(void);
	~romloader_jtag_openocd(void);


	typedef struct OPENOCD_NAME_RESOLVE_STRUCT
	{
		const char *pstrSymbolName;
		size_t sizPointerOffset;
	} OPENOCD_NAME_RESOLVE_T;

	typedef struct INTERFACE_SETUP_STRUCT
	{
		const char *pcID;
		const char *pcCode_Setup;
		const char *pcCode_Probe;
		unsigned short usUsbVendorId;
		unsigned short usUsbDeviceId;
	} INTERFACE_SETUP_STRUCT_T;

	typedef struct TARGET_SETUP_STRUCT
	{
		const char *pcID;
		const char *pcCode;
	} TARGET_SETUP_STRUCT_T;

	typedef void * (*PFN_MUHKUH_OPENOCD_INIT_T) (const char *pcScriptSearchDir);
	typedef int (*PFN_MUHKUH_OPENOCD_GET_RESULT_T) (void *pvContext, char *pcBuffer, size_t sizBufferMax);
	typedef int (*PFN_MUHKUH_OPENOCD_COMMAND_RUN_LINE_T) (void *pvContext, const char *pcLine);
	typedef void (*PFN_MUHKUH_OPENOCD_UNINIT_T) (void *pvContext);

	typedef int (*PFN_MUHKUH_OPENOCD_READ_DATA08) (void *pvContext, uint32_t ulNetxAddress, uint8_t *pucData);
	typedef int (*PFN_MUHKUH_OPENOCD_READ_DATA16) (void *pvContext, uint32_t ulNetxAddress, uint16_t *pusData);
	typedef int (*PFN_MUHKUH_OPENOCD_READ_DATA32) (void *pvContext, uint32_t ulNetxAddress, uint32_t *pulData);
	typedef int (*PFN_MUHKUH_OPENOCD_READ_IMAGE) (void *pvContext, uint32_t ulNetxAddress, uint8_t *pucData, uint32_t ulSize);
	typedef int (*PFN_MUHKUH_OPENOCD_WRITE_DATA08) (void *pvContext, uint32_t ulNetxAddress, uint8_t ucData);
	typedef int (*PFN_MUHKUH_OPENOCD_WRITE_DATA16) (void *pvContext, uint32_t ulNetxAddress, uint16_t usData);
	typedef int (*PFN_MUHKUH_OPENOCD_WRITE_DATA32) (void *pvContext, uint32_t ulNetxAddress, uint32_t ulData);
	typedef int (*PFN_MUHKUH_OPENOCD_WRITE_IMAGE) (void *pvContext, uint32_t ulNetxAddress, const uint8_t *pucData, uint32_t ulSize);
	typedef int (*PFN_MUHKUH_OPENOCD_CALL) (void *pvContext, uint32_t ulNetxAddress, uint32_t ulR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData);

	typedef struct MUHKUH_OPENOCD_FUNCTION_POINTERS_UNION
	{
		PFN_MUHKUH_OPENOCD_INIT_T pfnInit;
		PFN_MUHKUH_OPENOCD_GET_RESULT_T pfnGetResult;
		PFN_MUHKUH_OPENOCD_COMMAND_RUN_LINE_T pfnCommandRunLine;
		PFN_MUHKUH_OPENOCD_UNINIT_T pfnUninit;
		PFN_MUHKUH_OPENOCD_READ_DATA08 pfnReadData08;
		PFN_MUHKUH_OPENOCD_READ_DATA16 pfnReadData16;
		PFN_MUHKUH_OPENOCD_READ_DATA32 pfnReadData32;
		PFN_MUHKUH_OPENOCD_READ_IMAGE pfnReadImage;
		PFN_MUHKUH_OPENOCD_WRITE_DATA08 pfnWriteData08;
		PFN_MUHKUH_OPENOCD_WRITE_DATA16 pfnWriteData16;
		PFN_MUHKUH_OPENOCD_WRITE_DATA32 pfnWriteData32;
		PFN_MUHKUH_OPENOCD_WRITE_IMAGE pfnWriteImage;
		PFN_MUHKUH_OPENOCD_CALL pfnCall;
	} MUHKUH_OPENOCD_FUNCTION_POINTERS_T;


	typedef union MUHKUH_OPENOCD_FUNCTIONS_UNION
	{
		MUHKUH_OPENOCD_FUNCTION_POINTERS_T tFn;
		void *pv[sizeof(MUHKUH_OPENOCD_FUNCTION_POINTERS_T)/sizeof(void*)];
	} MUHKUH_OPENOCD_FUNCTIONS_T;


	typedef struct ROMLOADER_JTAG_DEVICE_STRUCT
	{
		void *pvSharedLibraryHandle;
		MUHKUH_OPENOCD_FUNCTIONS_T tFunctions;
		void *pvOpenocdContext;
	} ROMLOADER_JTAG_DEVICE_T;


	typedef struct ROMLOADER_JTAG_DETECT_ENTRY_STRUCT
	{
		char *pcInterface;
		char *pcTarget;
		char *pcLocation;
	} ROMLOADER_JTAG_DETECT_ENTRY_T;

	typedef struct ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_STRUCT
	{
		const INTERFACE_SETUP_STRUCT_T *ptIfSetup;
		unsigned char ucUsbBusNumber;
		size_t sizUsbPortNumbers;
		unsigned char aucUsbPortNumbers[USB_MAX_PATH_ELEMENTS];
		char acPathString[USB_MAX_PATH_ELEMENTS * 4 + 4] = {0};
	} ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T;

	int initialize(void);
	int detect(ROMLOADER_JTAG_DETECT_ENTRY_T **pptEntries, size_t *psizEntries);
	void uninit(void);

	/* Open the connection to the device. */
	int connect(const char *pcInterfaceName, const char *pcTargetName, const char *pcLocation);

	/* initialize the chip. */
	int init_chip(ROMLOADER_CHIPTYP tChiptyp);

	/* Close the connection to the device. */
	void disconnect(void);

	/* Read a byte (8bit) from the netX. */
	int read_data08(uint32_t ulNetxAddress, uint8_t *pucData);
	/* Read a word (16bit) from the netX. */
	int read_data16(uint32_t ulNetxAddress, uint16_t *pusData);
	/* Read a long (32bit) from the netX. */
	int read_data32(uint32_t ulNetxAddress, uint32_t *pulData);
	/* Read a byte array from the netX. */
	int read_image(uint32_t ulNetxAddress, uint8_t *pucData, uint32_t ulSize);

	/* Write a byte (8bit) to the netX. */
	int write_data08(uint32_t ulNetxAddress, uint8_t ucData);
	/* Write a word (16bit) to the netX. */
	int write_data16(uint32_t ulNetxAddress, uint16_t usData);
	/* Write a long (32bit) to the netX. */
	int write_data32(uint32_t ulNetxAddress, uint32_t ulData);
	/* Write a byte array to the netX. */
	int write_image(uint32_t ulNetxAddress, const uint8_t *pucData, uint32_t sizData);

	/* Call routine. */
	int call(uint32_t ulNetxAddress, uint32_t ulParameterR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData);

	/* Get the suggested chunk size for read_image and write_image. */
	uint32_t get_image_chunk_size(void);

private:
	static const OPENOCD_NAME_RESOLVE_T atOpenOcdResolve[13];

	static const INTERFACE_SETUP_STRUCT_T atInterfaceCfg[10];

	static const TARGET_SETUP_STRUCT_T atTargetCfg[6];

	static const char *pcResetCode;

	/* This is the path to the folder where the romloader_jtag plugin, the
	 * openocd shared object and the TCL scripts live.
	 */
	char *m_pcPluginPath;
	/* This is the path to the openocd data files. */
	char *m_pcOpenocdDataPath;
	/* This is the full path to the openocd shared object. */
	char *m_pcOpenocdSharedObjectPath;

	bool fJtagDeviceIsConnected;
	ROMLOADER_JTAG_DEVICE_T m_tJtagDevice;


	/* This is the list of detected devices as interface/target name pairs.
	 * The list will be cleared before each new detect.
	 */
	ROMLOADER_JTAG_DETECT_ENTRY_T *m_ptDetected;
	size_t m_sizDetectedCnt;
	size_t m_sizDetectedMax;

	libusb_context *m_ptLibUsbContext;

	void get_plugin_path(void);
	void get_openocd_path(void);

	void free_detect_entries(void);
	int add_detected_entry(const char *pcInterface, const char *pcTarget, const char *pcLocation);

	int openocd_open(ROMLOADER_JTAG_DEVICE_T *ptDevice);
	void openocd_close(ROMLOADER_JTAG_DEVICE_T *ptDevice);
	int romloader_jtag_openocd_init(void);

	int setup_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, const INTERFACE_SETUP_STRUCT_T *ptIfCfg, const char *pcLocation);
	int probe_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptLocation);
	const INTERFACE_SETUP_STRUCT_T *find_interface(const char *pcInterfaceName);
	int probe_target(ROMLOADER_JTAG_DEVICE_T *ptDevice, const INTERFACE_SETUP_STRUCT_T *ptIfSetup, const char *pcLocation, const TARGET_SETUP_STRUCT_T *ptTargetCfg);
	int detect_target(ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptLocation);
	const TARGET_SETUP_STRUCT_T *find_target(const char *pcTargetName);
};


#endif  /* __ROMLOADER_JTAG_OPENOCD_H__ */
