#include "romloader_jtag_openocd.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "shared_library.h"

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#       define OPENOCD_SHARED_LIBRARY_FILENAME "openocd.dll"
#       include <windows.h>

#elif defined(__GNUC__)
#       define OPENOCD_SHARED_LIBRARY_FILENAME "openocd.so"
#       include <dlfcn.h>
#       include <unistd.h>
#endif

/* NOTE: this must end with a slash. */
#define OPENOCD_SUBFOLDER "openocd/"



romloader_jtag_openocd::romloader_jtag_openocd(void)
 : fJtagDeviceIsConnected(false)
 , m_ptDetected(NULL)
 , m_sizDetectedCnt(0)
 , m_sizDetectedMax(0)
 , m_pcPluginPath(NULL)
 , m_pcOpenocdDataPath(NULL)
 , m_pcOpenocdSharedObjectPath(NULL)
 , m_ptLibUsbContext(NULL)
{
	memset(&m_tJtagDevice, 0, sizeof(ROMLOADER_JTAG_DEVICE_T));

	libusb_init(&m_ptLibUsbContext);
	libusb_set_option(m_ptLibUsbContext, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

	get_plugin_path();
	get_openocd_path();
}



romloader_jtag_openocd::~romloader_jtag_openocd(void)
{
	free_detect_entries();

	if( m_pcPluginPath!=NULL )
	{
		free(m_pcPluginPath);
		m_pcPluginPath = NULL;
	}
	if( m_pcOpenocdDataPath!=NULL )
	{
		free(m_pcOpenocdDataPath);
		m_pcOpenocdDataPath = NULL;
	}
	if( m_pcOpenocdSharedObjectPath!=NULL )
	{
		free(m_pcOpenocdSharedObjectPath);
		m_pcOpenocdSharedObjectPath = NULL;
	}

	if( m_ptLibUsbContext!=NULL )
	{
		libusb_exit(m_ptLibUsbContext);
	}
}



void romloader_jtag_openocd::get_plugin_path(void)
{
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	char *pcPath;
	DWORD dwFlags;
	BOOL fResult;
	LPCTSTR pfnMember;
	HMODULE hModule;
	DWORD dwBufferSize;
	DWORD dwResult;
	char *pcSlash;
	size_t sizPath;


	pcPath = NULL;

	/* Get the module by an address, but do not increase the refcount. */
	dwFlags =   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
	          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	/* Use this function to identify the module. */
	pfnMember = (LPCTSTR)(romloader_jtag_openocd::atOpenOcdResolve);
	fResult = GetModuleHandleEx(dwFlags, pfnMember, &hModule);
	if( fResult==0 )
	{
		fprintf(stderr, "Failed to get the module handle: %d\n", GetLastError());
	}
	else
	{
		dwBufferSize = 0x00010000;
		pcPath = (char*)malloc(dwBufferSize);
		if( pcPath==NULL )
		{
			fprintf(stderr, "Failed to allocate %d bytes for the path buffer.\n", dwBufferSize);
		}
		else
		{
			dwResult = GetModuleFileName(hModule, pcPath, dwBufferSize);
			/* NOTE: dwResult contains the length of the string without the terminating 0.
			 *       If the buffer is too small, the function returns the provided size of
			 *       the buffer, in this case "dwBufferSize".
			 *       Therefore the function failed if the result is dwBufferSize.
			 */
			if( dwResult>0 && dwResult<dwBufferSize )
			{
				fprintf(stderr, "Module path: '%s'\n", pcPath);

				/* Find the last backslash in the path. */
				pcSlash = strrchr(pcPath, '\\');
				if( pcSlash==NULL )
				{
					fprintf(stderr, "Failed to find the end of the path!\n");
					free(pcPath);
					pcPath = NULL;
				}
				else
				{
					/* Terminate the string after the last slash. */
					pcSlash[1] = 0;

					/* Allocate the new buffer. */
					sizPath = strlen(pcPath);
					m_pcPluginPath = (char*)malloc(sizPath + 1);
					if( m_pcPluginPath==NULL )
					{
						fprintf(stderr, "Failed to allocate a buffer for the plugin path.\n");
					}
					else
					{
						memcpy(m_pcPluginPath, pcPath, sizPath + 1);
					}

					free(pcPath);
					pcPath = NULL;
				}
			}
			else
			{
				fprintf(stderr, "Failed to get the module file name: %d\n", dwResult);
				free(pcPath);
				pcPath = NULL;
			}
		}
	}
#elif defined(__GNUC__)
	Dl_info tDlInfo;
	int iResult;
	size_t sizPath;
	const char *pcSlash;
	size_t sizCwdBufferSize;
	size_t sizCwd;
	char *pcCwd;
	char *pcGetCwdResult;
	int iCwdAddSlash;


	iResult = dladdr(romloader_jtag_openocd::atOpenOcdResolve, &tDlInfo);
	if( iResult==0 )
	{
		fprintf(stderr, "Failed to get information about the shared object.\n");
	}
	else
	{
		if( tDlInfo.dli_fname!=NULL )
		{
			fprintf(stderr, "Path to the shared object: '%s'\n", tDlInfo.dli_fname);

			/* Is this an absolute path? */
			iResult = -1;
			if( tDlInfo.dli_fname[0]=='/' )
			{
				/* Yes -> no need to prepend the current working directory. */
				sizCwd = 0;
				pcCwd = NULL;
				iCwdAddSlash = 0;
				iResult = 0;
			}
			else
			{
				/* No, prepend the current working folder. */
				sizCwdBufferSize = 65536;
				pcCwd = (char*)malloc(sizCwdBufferSize);
				if( pcCwd==NULL )
				{
					fprintf(stderr, "Failed to allocate a buffer for the current working folder.\n");
				}
				else
				{
					pcGetCwdResult = getcwd(pcCwd, sizCwdBufferSize);
					if( pcGetCwdResult==NULL )
					{
						fprintf(stderr, "Failed to get the current working folder.\n");
					}
					else
					{
						sizCwd = strlen(pcCwd);
						iCwdAddSlash = 0;
						if( sizCwd>0 && pcCwd[sizCwd-1]!='/' )
						{
							iCwdAddSlash = 1;
						}
						iResult = 0;
					}
				}
			}

			if( iResult==0 )
			{
				/* Find the last backslash in the path. */
				pcSlash = strrchr(tDlInfo.dli_fname, '/');
				if( pcSlash==NULL )
				{
					fprintf(stderr, "Failed to find the end of the path!\n");
					if( pcCwd!=NULL )
					{
						free(pcCwd);
						pcCwd = NULL;
						sizCwd = 0;
					}
				}
				else
				{
					sizPath = (size_t)(pcSlash - tDlInfo.dli_fname) + 1;
					m_pcPluginPath = (char*)malloc(sizCwd + iCwdAddSlash + sizPath + 1);
					if( m_pcPluginPath==NULL )
					{
						fprintf(stderr, "Failed to allocate a buffer for the path.\n");
						if( pcCwd!=NULL )
						{
							free(pcCwd);
							pcCwd = NULL;
							sizCwd = 0;
						}
					}
					else
					{
						if( pcCwd!=NULL && sizCwd!=0 )
						{
							memcpy(m_pcPluginPath, pcCwd, sizCwd);
						}
						if( iCwdAddSlash!=0 )
						{
							m_pcPluginPath[sizCwd] = '/';
						}
						memcpy(m_pcPluginPath + sizCwd + iCwdAddSlash, tDlInfo.dli_fname, sizPath);
						m_pcPluginPath[sizCwd + iCwdAddSlash + sizPath] = 0;
					}
				}
			}
		}
	}
#endif
}



void romloader_jtag_openocd::get_openocd_path(void)
{
	size_t sizOpenOcdSo;
	size_t sizPluginPath;
	size_t sizOpenOcdSubfolder;


	sizOpenOcdSo = sizeof(OPENOCD_SHARED_LIBRARY_FILENAME);

	if( m_pcPluginPath==NULL )
	{
		m_pcOpenocdDataPath = (char*)malloc(2);
		if( m_pcOpenocdDataPath!=NULL )
		{
			m_pcOpenocdDataPath[0] = '.';
			m_pcOpenocdDataPath[1] = 0;

			m_pcOpenocdSharedObjectPath = (char*)malloc(sizOpenOcdSo + 1);
			if( m_pcOpenocdSharedObjectPath==NULL )
			{
				free(m_pcOpenocdDataPath);
				m_pcOpenocdDataPath = NULL;
			}
			else
			{
				/* Initialize the path with the name of the shared object only. */
				memcpy(m_pcOpenocdSharedObjectPath, OPENOCD_SHARED_LIBRARY_FILENAME, sizOpenOcdSo);
				/* Terminate the name with a 0. */
				m_pcOpenocdSharedObjectPath[sizOpenOcdSo] = 0;
			}
		}
	}
	else
	{
		sizPluginPath = strlen(m_pcPluginPath);
		sizOpenOcdSubfolder = strlen(OPENOCD_SUBFOLDER);
		m_pcOpenocdDataPath = (char*)malloc(sizPluginPath + sizOpenOcdSubfolder + 1);
		if( m_pcOpenocdDataPath!=NULL )
		{
			/* Copy the path to this module to the start of the OpenOCD path. */
			memcpy(m_pcOpenocdDataPath, m_pcPluginPath, sizPluginPath);
			/* Append the subfolder. */
			memcpy(m_pcOpenocdDataPath + sizPluginPath, OPENOCD_SUBFOLDER, sizOpenOcdSubfolder);
			/* Terminate the path. */
			m_pcOpenocdDataPath[sizPluginPath + sizOpenOcdSubfolder] = 0;

			m_pcOpenocdSharedObjectPath = (char*)malloc(sizPluginPath + sizOpenOcdSubfolder + sizOpenOcdSo + 1);
			if( m_pcOpenocdSharedObjectPath==NULL )
			{
				free(m_pcOpenocdDataPath);
				m_pcOpenocdDataPath = NULL;
			}
			else
			{
				/* Copy the path to this module to the start of the OpenOCD path. */
				memcpy(m_pcOpenocdSharedObjectPath, m_pcPluginPath, sizPluginPath);
				/* Append the subfolder. */
				memcpy(m_pcOpenocdSharedObjectPath + sizPluginPath, OPENOCD_SUBFOLDER, sizOpenOcdSubfolder);
				/* Append the name of the OpenOCD shared object. */
				memcpy(m_pcOpenocdSharedObjectPath + sizPluginPath + sizOpenOcdSubfolder, OPENOCD_SHARED_LIBRARY_FILENAME, sizOpenOcdSo);
				/* Terminate the name with a 0. */
				m_pcOpenocdSharedObjectPath[sizPluginPath + sizOpenOcdSubfolder + sizOpenOcdSo] = 0;
			}
		}
	}

	if( m_pcOpenocdDataPath!=NULL && m_pcOpenocdSharedObjectPath!=NULL )
	{
		fprintf(stderr, "The path to the OpenOCD data files is:    '%s'\n", m_pcOpenocdDataPath);
		fprintf(stderr, "The path to the OpenOCD shared object is: '%s'\n", m_pcOpenocdSharedObjectPath);
	}
	else
	{
		fprintf(stderr, "Failed to get the path to the OpenOCD shared object.\n");
	}
}



const romloader_jtag_openocd::OPENOCD_NAME_RESOLVE_T romloader_jtag_openocd::atOpenOcdResolve[13] =
{
	{
		"muhkuh_openocd_init",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnInit) / sizeof(void*)
	},
	{
		"muhkuh_openocd_get_result",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnGetResult) / sizeof(void*)
	},
	{
		"muhkuh_openocd_command_run_line",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnCommandRunLine) / sizeof(void*)
	},
	{
		"muhkuh_openocd_uninit",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnUninit) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data08",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData08) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data16",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData16) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_data32",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadData32) / sizeof(void*)
	},
	{
		"muhkuh_openocd_read_image",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnReadImage) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data08",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData08) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data16",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData16) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_data32",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteData32) / sizeof(void*)
	},
	{
		"muhkuh_openocd_write_image",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnWriteImage) / sizeof(void*)
	},
	{
		"muhkuh_openocd_call",
		offsetof(romloader_jtag_openocd::MUHKUH_OPENOCD_FUNCTION_POINTERS_T, pfnCall) / sizeof(void*)
	}
};



void romloader_jtag_openocd::free_detect_entries(void)
{
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptCnt;
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptEnd;


	if( m_ptDetected!=NULL )
	{
		/* Loop over all entries in the list. */
		ptCnt = m_ptDetected;
		ptEnd = m_ptDetected + m_sizDetectedCnt;
		while( ptCnt<ptEnd )
		{
			if( ptCnt->pcInterface!=NULL )
			{
				free(ptCnt->pcInterface);
			}
			if( ptCnt->pcTarget!=NULL )
			{
				free(ptCnt->pcTarget);
			}
			if( ptCnt->pcLocation!=NULL )
			{
				free(ptCnt->pcLocation);
			}

			++ptCnt;
		}
		free(m_ptDetected);
		m_ptDetected = NULL;
	}
	m_sizDetectedCnt = 0;
	m_sizDetectedMax = 0;

}



int romloader_jtag_openocd::add_detected_entry(const char *pcInterface, const char *pcTarget, const char *pcLocation)
{
	int iResult;
	ROMLOADER_JTAG_DETECT_ENTRY_T *ptDetectedNew;


	/* Be optimistic. */
	iResult = 0;

	/* Is enough space in the array for one more entry? */
	if( m_sizDetectedCnt>=m_sizDetectedMax )
	{
		/* No -> expand the array. */
		m_sizDetectedMax *= 2;
		/* Detect overflow or limitation. */
		if( m_sizDetectedMax<=m_sizDetectedCnt )
		{
			iResult = -1;
		}
		else
		{
			/* Reallocate the array. */
			ptDetectedNew = (ROMLOADER_JTAG_DETECT_ENTRY_T*)realloc(m_ptDetected, m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
			if( ptDetectedNew==NULL )
			{
				iResult = -1;
			}
			else
			{
				m_ptDetected = ptDetectedNew;
			}
		}
	}

	if( iResult==0 )
	{
		m_ptDetected[m_sizDetectedCnt].pcInterface = strdup(pcInterface);
		m_ptDetected[m_sizDetectedCnt].pcTarget = strdup(pcTarget);
		m_ptDetected[m_sizDetectedCnt].pcLocation = strdup(pcLocation);
		++m_sizDetectedCnt;
	}

	return iResult;
}

/*
   Try to open the shared library.
   If successful, resolve method names and initialize the shared library.
 */
int romloader_jtag_openocd::openocd_open(ROMLOADER_JTAG_DEVICE_T *ptDevice)
{
	int iResult;
	void *pvSharedLibraryHandle;
	const OPENOCD_NAME_RESOLVE_T *ptCnt;
	const OPENOCD_NAME_RESOLVE_T *ptEnd;
	void *pvFn;
	void *pvOpenocdContext;
	char acError[1024];


	/* Be optimistic. */
	iResult = 0;

	/* Try to open the shared library. */
	pvSharedLibraryHandle = sharedlib_open(m_pcOpenocdSharedObjectPath);
	if( pvSharedLibraryHandle==NULL )
	{
		/* Failed to open the shared library. */
		sharedlib_get_error(acError, sizeof(acError));
		fprintf(stderr, "Failed to open the shared library %s.\n", m_pcOpenocdSharedObjectPath);
		fprintf(stderr, "Error: %s\n", acError);
		iResult = -1;
	}
	else
	{
		ptDevice->pvSharedLibraryHandle = pvSharedLibraryHandle;

		/* Try to resolve all symbols. */
		ptCnt = atOpenOcdResolve;
		ptEnd = atOpenOcdResolve + (sizeof(atOpenOcdResolve)/sizeof(atOpenOcdResolve[0]));
		while( ptCnt<ptEnd )
		{
			pvFn = sharedlib_resolve_symbol(pvSharedLibraryHandle, ptCnt->pstrSymbolName);
			if( pvFn==NULL )
			{
				iResult = -1;
				break;
			}
			else
			{
				ptDevice->tFunctions.pv[ptCnt->sizPointerOffset] = pvFn;
				++ptCnt;
			}
		}

		if( iResult==0 )
		{
			/* Call the init function and pass the data path as a search path for scripts. */
			pvOpenocdContext = ptDevice->tFunctions.tFn.pfnInit(m_pcOpenocdDataPath);
			if( pvOpenocdContext==NULL )
			{
				fprintf(stderr, "Failed to initialize the OpenOCD device context.\n");
				iResult = -1;
			}
			else
			{
				ptDevice->pvOpenocdContext = pvOpenocdContext;

				fprintf(stderr, "Loading script.\n");
				iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, "source [find jtag_detect_init.tcl]");
				if( iResult!=0 )
				{
					fprintf(stderr, "Failed to load the script: %d\n", iResult);
					iResult = -1;
				}


			}
		}

		if( iResult!=0 )
		{
			/* Close the shared library. */
			sharedlib_close(pvSharedLibraryHandle);
			ptDevice->pvSharedLibraryHandle = NULL;
		}
	}

	return iResult;
}


/* Uninitialize and cose the shared library. */
void romloader_jtag_openocd::openocd_close(ROMLOADER_JTAG_DEVICE_T *ptDevice)
{
	if( ptDevice->pvSharedLibraryHandle!=NULL )
	{
		if( ptDevice->pvOpenocdContext!=NULL )
		{
			ptDevice->tFunctions.tFn.pfnUninit(ptDevice->pvOpenocdContext);
			ptDevice->pvOpenocdContext=NULL;
		}

		/* Close the shared library. */
		sharedlib_close(ptDevice->pvSharedLibraryHandle);
		ptDevice->pvSharedLibraryHandle=NULL;
	}
}



/* The romloader_jtag_openocd_init function opens the OpenOCD shared library,
   executes the "version" command and closes the library.

   This is a test if the shared library can be used.
*/
int romloader_jtag_openocd::initialize(void)
{
	int iResult;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	char acResult[256];


	/* Initialize the device handle. */
	memset(&tDevice, 0, sizeof(ROMLOADER_JTAG_DEVICE_T));

	iResult = openocd_open(&tDevice);
	if( iResult==0 )
	{
		/* Run the version command. */
		iResult = tDevice.tFunctions.tFn.pfnCommandRunLine(tDevice.pvOpenocdContext, "version\n");
		if( iResult!=0 )
		{
			fprintf(stderr, "Failed to run the version command!\n");
		}
		else
		{
			iResult = tDevice.tFunctions.tFn.pfnGetResult(tDevice.pvOpenocdContext, acResult, sizeof(acResult));
			if( iResult!=0 )
			{
				fprintf(stderr, "Failed to get the result for the version command.\n");
			}
			else
			{
				fprintf(stderr, "OpenOCD version %s\n", acResult);
			}
		}

		openocd_close(&tDevice);
	}

	return iResult;
}



/* This array contains the detection command sequence for all interfaces. All
 * commands for one interface are stored in one string. The commands are
 * separated by newlines.
 *
 * Every detection sequence must set the TCL variable 'IF_CFG_RESULT'. It
 * shows the result of the detection. A value of '1'
 *
 * Use a ridiculously low speed to ensure that the connection is working in
 * absolutely all circumstances (even with Enricos flying wires. ;)
 */

/* FIXME: read this from some kind of configuration file. */

const romloader_jtag_openocd::INTERFACE_SETUP_STRUCT_T romloader_jtag_openocd::atInterfaceCfg[10] =
{
	{"NXJTAG-USB",             "setup_interface NXJTAG-USB %s",            "probe_interface", 0x1939, 0x0023 },
	{"Amontec_JTAGkey",        "setup_interface Amontec_JTAGkey %s",       "probe_interface", 0x0403, 0xcff8 },
	{"Olimex_ARM_USB_TINY_H",  "setup_interface Olimex_ARM_USB_TINY_H %s", "probe_interface", 0x15ba, 0x002a },
	{"NXHX_500/50/51/10",      "setup_interface NXHX_500_50_51_10 %s",     "probe_interface", 0x0640, 0x0028 },
	{"NXHX_90-JTAG",           "setup_interface NXHX_90-JTAG %s",          "probe_interface", 0x1939, 0x002c },
	{"NXHX_90-DRIVE",          "setup_interface NXHX_90-DRIVE %s",         "probe_interface", 0x1939, 0x0031 },
	{"NRPEB_H90-RE",           "setup_interface NRPEB_H90-RE %s",          "probe_interface", 0x1939, 0x0029 },
	{"NXJTAG-4000-USB",        "setup_interface NXJTAG-4000-USB %s",       "probe_interface", 0x1939, 0x0301 },
	{"J-Link",                 "setup_interface J-Link %s",                "probe_interface", 0x1366, 0x0101 },
	{"J-Link",                 "setup_interface J-Link %s",                "probe_interface", 0x1366, 0x0105 },
};


/* FIXME: read this from some kind of configuration file. */
const romloader_jtag_openocd::TARGET_SETUP_STRUCT_T romloader_jtag_openocd::atTargetCfg[6] =
{
	{
		"netX_ARM966",
		"probe_cpu netX_ARM966"
	},

	{
		"netX_ARM926",
		"probe_cpu netX_ARM926"
	},

	{
		"netX4000_R7",
		"probe_cpu netX4000_R7"
	},

	{
		"netX90_MPW_COM",
		"probe_cpu netX90_MPW_COM"
	},

	{
		"netX90_COM",
		"probe_cpu netX90_COM"
	},

	{
		"netIOL",
		"probe_cpu netIOL"
	},
};




int romloader_jtag_openocd::setup_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, const INTERFACE_SETUP_STRUCT_T *ptIfCfg, const char *pcLocation)
{
	int iResult;
	char acCommand[256];


	/* Combine the command with the path. */
	snprintf(acCommand, sizeof(acCommand)-1, ptIfCfg->pcCode_Setup, pcLocation);

	/* Run the command chunk. */
	fprintf(stderr, "Run setup chunk for interface %s.\n", ptIfCfg->pcID);
	iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, acCommand);
	if( iResult!=0 )
	{
		fprintf(stderr, "Failed to run the chunk: %d\n", iResult);
		fprintf(stderr, "Failed command: %s\n", acCommand);
	}

	return iResult;
}


/* Probe for a single interface. */
int romloader_jtag_openocd::probe_interface(ROMLOADER_JTAG_DEVICE_T *ptDevice, ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptLocation)
{
	int iResult;
	int sizResult;
	const INTERFACE_SETUP_STRUCT_T *ptIfCfg;
	char strResult[256];


	ptIfCfg = ptLocation->ptIfSetup;

	/* Try to setup the interface. */
	iResult = setup_interface(ptDevice, ptIfCfg, ptLocation->acPathString);
	if( iResult!=0 )
	{
		/* This is no fatal error. It just means that this interface can not be used. */
		iResult = 1;
	}
	else
	{
		/* Run the probe chunk. */
		fprintf(stderr, "Run probe chunk for interface %s.\n", ptIfCfg->pcID);
		iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, ptIfCfg->pcCode_Probe);
		if( iResult!=0 )
		{
			/* This is no fatal error. It just means that this interface can not be used. */
			fprintf(stderr, "Failed to run the chunk: %d\n", iResult);
			iResult = 1;
		}
		else
		{
			iResult = ptDevice->tFunctions.tFn.pfnGetResult(ptDevice->pvOpenocdContext, strResult, sizeof(strResult));
			if( iResult!=0 )
			{
				/* This is a fatal error. */
				fprintf(stderr, "Failed to get the result.\n");
				iResult = -1;
			}
			else
			{
				fprintf(stderr, "Result from probe: %s\n", strResult);
				iResult = strncmp(strResult, "OK", 3);
				if( iResult!=0 )
				{
					/* This is no fatal error. */
					iResult = 1;
				}
				else
				{
					fprintf(stderr, "Found interface %s!\n", ptIfCfg->pcID);
				}
			}
		}
	}

	return iResult;
}


/* Find an interface configuration with the given ID. */
const romloader_jtag_openocd::INTERFACE_SETUP_STRUCT_T *romloader_jtag_openocd::find_interface(const char *pcInterfaceName)
{
	const INTERFACE_SETUP_STRUCT_T *ptCnt;
	const INTERFACE_SETUP_STRUCT_T *ptEnd;
	const INTERFACE_SETUP_STRUCT_T *ptHit;
	int iCompareResult;


	/* No match yet. */
	ptHit = NULL;

	ptCnt = atInterfaceCfg;
	ptEnd = atInterfaceCfg + (sizeof(atInterfaceCfg)/sizeof(atInterfaceCfg[0]));
	while( ptCnt<ptEnd )
	{
		iCompareResult = strcmp(ptCnt->pcID, pcInterfaceName);
		if( iCompareResult==0 )
		{
			ptHit = ptCnt;
			break;
		}

		++ptCnt;
	}

	return ptHit;
}


/* Probe for a single interface/target combination. */
int romloader_jtag_openocd::probe_target(ROMLOADER_JTAG_DEVICE_T *ptDevice, const INTERFACE_SETUP_STRUCT_T *ptIfSetup, const char *pcLocation, const TARGET_SETUP_STRUCT_T *ptTargetCfg)
{
	int iResult;
	int sizResult;
	char strResult[256];


	iResult = setup_interface(ptDevice, ptIfSetup, pcLocation);
	if( iResult!=0 )
	{
		/* This is always a fatal error here as the interface has been used before. */
		iResult = -1;
	}
	else
	{
		fprintf(stderr, "Running detect code for target %s!\n", ptTargetCfg->pcID);
		iResult = ptDevice->tFunctions.tFn.pfnCommandRunLine(ptDevice->pvOpenocdContext, ptTargetCfg->pcCode);
		if( iResult!=0 )
		{
			/* This is no fatal error. It just means that this CPU is not present. */
			fprintf(stderr, "Failed to run the command chunk: %d\n", iResult);
			iResult = 1;
		}
		else
		{
			iResult = ptDevice->tFunctions.tFn.pfnGetResult(ptDevice->pvOpenocdContext, strResult, sizeof(strResult));
			if( iResult!=0 )
			{
				/* This is a fatal error. */
				fprintf(stderr, "Failed to get the result for the code.\n");
				iResult = -1;
			}
			else
			{
				fprintf(stderr, "Result from detect: %s\n", strResult);
				iResult = strncmp(strResult, "OK", 3);
				if( iResult!=0 )
				{
					/* This is no fatal error. */
					iResult = 1;
				}
				else
				{
					fprintf(stderr, "Found target %s!\n", ptTargetCfg->pcID);
				}
			}
		}
	}

	return iResult;
}


/* Try all available targets with a given interface. */
int romloader_jtag_openocd::detect_target(ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptLocation)
{
	const INTERFACE_SETUP_STRUCT_T *ptIfCfg;
	const TARGET_SETUP_STRUCT_T *ptCnt;
	const TARGET_SETUP_STRUCT_T *ptEnd;
	int iResult;
	int sizResult;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	char strResult[256];


	/* Try to run all command chunks to see which CPU is there. */
	ptCnt = atTargetCfg;
	ptEnd = atTargetCfg + (sizeof(atTargetCfg)/sizeof(atTargetCfg[0]));
	while( ptCnt<ptEnd )
	{
		/* Open the shared library. */
		iResult = openocd_open(&tDevice);
		if( iResult!=0 )
		{
			/* This is a fatal error. */
			break;
		}
		else
		{
			fprintf(stderr, "Detecting target %s\n", ptCnt->pcID);
			iResult = probe_target(&tDevice, ptLocation->ptIfSetup, ptLocation->acPathString, ptCnt);

			openocd_close(&tDevice);
		}

		if( iResult==0 )
		{
			/* Found an entry! */
			iResult = add_detected_entry(ptLocation->ptIfSetup->pcID, ptCnt->pcID, ptLocation->acPathString);
			break;
		}
		else if( iResult<0 )
		{
			/* Fatal error! */
			break;
		}

		/* Move to the next configuration. */
		++ptCnt;
	}

	return iResult;
}


/* Find a target with the given name. */
const romloader_jtag_openocd::TARGET_SETUP_STRUCT_T *romloader_jtag_openocd::find_target(const char *pcTargetName)
{
	const TARGET_SETUP_STRUCT_T *ptCnt;
	const TARGET_SETUP_STRUCT_T *ptEnd;
	const TARGET_SETUP_STRUCT_T *ptHit;
	int iCompareResult;


	/* No match yet. */
	ptHit = NULL;

	ptCnt = atTargetCfg;
	ptEnd = atTargetCfg + (sizeof(atTargetCfg)/sizeof(atTargetCfg[0]));
	while( ptCnt<ptEnd )
	{
		iCompareResult = strcmp(ptCnt->pcID, pcTargetName);
		if( iCompareResult==0 )
		{
			ptHit = ptCnt;
			break;
		}

		++ptCnt;
	}

	return ptHit;
}


/*
   Detect interfaces/targets.
   Probe for all known interfaces. For each interface found, probe for all known targets.
   Returns a list of ROMLOADER_JTAG_DETECT_ENTRY_T in pptEntries/psizEntries.
 */
int romloader_jtag_openocd::detect(ROMLOADER_JTAG_DETECT_ENTRY_T **pptEntries, size_t *psizEntries)
{
	int iResult;
	size_t sizLocations;
	ssize_t ssizDevList;
	libusb_device **ptDeviceList;
	libusb_device **ptDevCnt, **ptDevEnd;
	libusb_device *ptDev;
	unsigned short usUsbVendorId;
	unsigned short usUsbDeviceId;
	int iLibUsbResult;
	int iCnt;
	unsigned char ucUsbBusNumber;
	char *pcPathString;
	const INTERFACE_SETUP_STRUCT_T *ptIfSetupCnt;
	const INTERFACE_SETUP_STRUCT_T *ptIfSetupEnd;
	const INTERFACE_SETUP_STRUCT_T *ptIfSetupHit;
	struct libusb_device_descriptor tDeviceDescriptor;
	ROMLOADER_JTAG_DEVICE_T tDevice;
	ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T atUsbLocations[USB_MAX_DEVICES];
	ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptUsbLocationCnt;
	ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T *ptUsbLocationEnd;


	/* Be pessimistic... */
	iResult = -1;

	/* Clear all locations. */
	memset(atUsbLocations, 0, sizeof(atUsbLocations));

	/* Clear any old results. */
	free_detect_entries();

	/* Get a list of all connected USB devices. */
	sizLocations = 0;
	ptDeviceList = NULL;
	ssizDevList = libusb_get_device_list(m_ptLibUsbContext, &ptDeviceList);
	if( ssizDevList<0 )
	{
		/* failed to detect devices */
		fprintf(stderr, "romloader_jtag_openocd(%p): failed to detect usb devices: %ld:%s\n", this, ssizDevList, libusb_strerror((libusb_error)ssizDevList));
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

			iLibUsbResult = libusb_get_device_descriptor(ptDev, &tDeviceDescriptor);
			if( iLibUsbResult==0 )
			{
				/* Get the vendor and device ID. */
				usUsbVendorId = tDeviceDescriptor.idVendor;
				usUsbDeviceId = tDeviceDescriptor.idProduct;

				/* Is this one of the known JTAG interfaces? */
				ptIfSetupCnt = atInterfaceCfg;
				ptIfSetupEnd = atInterfaceCfg + (sizeof(atInterfaceCfg)/sizeof(INTERFACE_SETUP_STRUCT_T));
				ptIfSetupHit = NULL;
				while( ptIfSetupCnt<ptIfSetupEnd )
				{
					if( ptIfSetupCnt->usUsbVendorId==usUsbVendorId && ptIfSetupCnt->usUsbDeviceId==usUsbDeviceId )
					{
						ptIfSetupHit = ptIfSetupCnt;
						break;
					}
					else
					{
						++ptIfSetupCnt;
					}
				}
				if( ptIfSetupHit!=NULL )
				{
					/* Create a new location entry. */
					ptUsbLocationCnt = &(atUsbLocations[sizLocations]);
					iLibUsbResult = libusb_get_port_numbers(ptDev, ptUsbLocationCnt->aucUsbPortNumbers, sizeof(ptUsbLocationCnt->aucUsbPortNumbers));
					if( iLibUsbResult>0 )
					{
						ptUsbLocationCnt->ptIfSetup = ptIfSetupHit;
						ptUsbLocationCnt->sizUsbPortNumbers = (size_t)iLibUsbResult;

						/* Get the bus number. */
						ucUsbBusNumber = libusb_get_bus_number(ptDev);
						ptUsbLocationCnt->ucUsbBusNumber = ucUsbBusNumber;

						/* Build the path string. */
						pcPathString = ptUsbLocationCnt->acPathString;
						sprintf(pcPathString, "%d:", ucUsbBusNumber);
						while( (*pcPathString)!=0 )
						{
							++pcPathString;
						}
						for(iCnt=0; iCnt<iLibUsbResult; ++iCnt)
						{
							sprintf(pcPathString, "%d,", ptUsbLocationCnt->aucUsbPortNumbers[iCnt]);
							while( (*pcPathString)!=0 )
							{
								++pcPathString;
							}
						}
						/* pcPathString points to the terminating 0 now. Remove the last ":" or ",". */
						--pcPathString;
						*pcPathString = 0;

						printf("Found '%s' (VID%04x/PID%04x) at %s.\n", ptIfSetupHit->pcID, usUsbVendorId, usUsbDeviceId, ptUsbLocationCnt->acPathString);

						++sizLocations;
						if( sizLocations>=(sizeof(atUsbLocations)/sizeof(ROMLOADER_JTAG_MATCHING_USB_LOCATIONS_T)) )
						{
							break;
						}
					}
				}
			}

			++ptDevCnt;
		}

		libusb_free_device_list(ptDeviceList, 1);

		/* Initialize the result array. */
		m_sizDetectedCnt = 0;
		m_sizDetectedMax = sizLocations;
		m_ptDetected = (ROMLOADER_JTAG_DETECT_ENTRY_T*)malloc(m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
		if( m_ptDetected==NULL )
		{
			fprintf(stderr, "Failed to allocate %zd bytes of memory for the detection results!\n", m_sizDetectedMax*sizeof(ROMLOADER_JTAG_DETECT_ENTRY_T));
		}
		else
		{
			/* Run the command chunks of all locations to see which interfaces are present. */
			ptUsbLocationCnt = atUsbLocations;
			ptUsbLocationEnd = atUsbLocations + sizLocations;
			while( ptUsbLocationCnt<ptUsbLocationEnd )
			{
				fprintf(stderr, "Detecting interface '%s' on path %s.\n", ptUsbLocationCnt->ptIfSetup->pcID, ptUsbLocationCnt->acPathString);

				/* Open the shared library. */
				iResult = openocd_open(&tDevice);
				if( iResult!=0 )
				{
					/* This is a fatal error. */
					iResult = -1;
					break;
				}
				else
				{
					/* Detect the interface. */
					iResult = probe_interface(&tDevice, ptUsbLocationCnt);

					/* Clean up after the detection. */
					openocd_close(&tDevice);

					if( iResult==0 )
					{
						/* Detect the CPU on this interface. */
						iResult = detect_target(ptUsbLocationCnt);
					}

					/* Ignore non-fatal errors.
					 * They indicate that the current interface could not be detected.
					 */
					if( iResult>0 )
					{
						iResult = 0;
					}
					/* Do not continue with other interfaces if a fatal error occurred. */
					else if( iResult<0 )
					{
						/* This is a fatal error. */
						break;
					}
				}

				/* Move to the next location. */
				++ptUsbLocationCnt;
			}
		}
	}

	/* Discard all results if something really bad happened (like a fatal error). */
	if( iResult<0 )
	{
		free_detect_entries();
	}

	/* Return the result list. */
	*pptEntries = m_ptDetected;
	*psizEntries = m_sizDetectedCnt;

	return iResult;
}


/* Open a connection to a given target on a given interface. */
/* Open the connection to the device. */
int romloader_jtag_openocd::connect(const char *pcInterfaceName, const char *pcTargetName, const char *pcLocation)
{
	int iResult;
	const INTERFACE_SETUP_STRUCT_T *ptInterface;
	const TARGET_SETUP_STRUCT_T *ptTarget;


	if( fJtagDeviceIsConnected==true )
	{
		fprintf(stderr, "romloader_jtag_openocd::connect: Already connected.\n");
	}
	else
	{
		ptInterface = find_interface(pcInterfaceName);
		if( ptInterface==NULL )
		{
			fprintf(stderr, "Could not find interface %s!\n", pcInterfaceName);
		}
		else
		{
			ptTarget = find_target(pcTargetName);
			if( ptTarget==NULL )
			{
				fprintf(stderr, "Could not find target %s!\n", pcTargetName);
			}
			else
			{
				/* Open the shared library. */
				iResult = openocd_open(&m_tJtagDevice);
				if( iResult==0 )
				{
					iResult = probe_target(&m_tJtagDevice, ptInterface, pcLocation, ptTarget);
					if( iResult==0 )
					{
						/* Stop the target. */
						fprintf(stderr, "Running reset code.\n");
						iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, "reset_board");
						if( iResult!=0 )
						{
							fprintf(stderr, "Failed to run the reset code: %d\n", iResult);
							iResult = -1;
						}
						else
						{
							fJtagDeviceIsConnected = true;
						}
					}
				}
			}
		}
	}

	return iResult;
}


/*
   Initialize the chip and prepare it for running code.
   (the equivalent of what the 'run' section did in the old OpenOCD configs)
 */

int romloader_jtag_openocd::init_chip(ROMLOADER_CHIPTYP tChiptyp)
{
	int iResult;
	char strCmd[32];

	fprintf(stderr, "Loading chip init script.\n");
	iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, "source [find chip_init.tcl]");
	if( iResult!=0 )
	{
		fprintf(stderr, "Failed to load the chip init script: %d\n", iResult);
		iResult = -1;
	}
	else
	{
		fprintf(stderr, "Running init_chip script.\n");
		memset(strCmd, 0, sizeof(strCmd));
		snprintf(strCmd, sizeof(strCmd)-1, "init_chip %d", tChiptyp);
		iResult = m_tJtagDevice.tFunctions.tFn.pfnCommandRunLine(m_tJtagDevice.pvOpenocdContext, strCmd);
		if( iResult!=0 )
		{
			fprintf(stderr, "Failed to run the init_chip script: %d\n", iResult);
			iResult = -1;
		}
		else
		{
			fprintf(stderr, "Chip init complete.\n");
			iResult = 0;
		}

	}

	return iResult;
}


/* Close the connection to the device. */
void romloader_jtag_openocd::disconnect(void)
{
	openocd_close(&m_tJtagDevice);
	fJtagDeviceIsConnected = false;
}



/* Read a byte (8bit) from the netX. */
int romloader_jtag_openocd::read_data08(uint32_t ulNetxAddress, uint8_t *pucData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData08!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData08(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData);
		if( iResult!=0 )
		{
			fprintf(stderr, "read_data08: Failed to read address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a word (16bit) from the netX. */
int romloader_jtag_openocd::read_data16(uint32_t ulNetxAddress, uint16_t *pusData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData16!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData16(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pusData);
		if( iResult!=0 )
		{
			fprintf(stderr, "read_data16: Failed to read address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a long (32bit) from the netX. */
int romloader_jtag_openocd::read_data32(uint32_t ulNetxAddress, uint32_t *pulData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadData32!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadData32(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pulData);
		if( iResult!=0 )
		{
			fprintf(stderr, "read_data32: Failed to read address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Read a byte array from the netX. */
int romloader_jtag_openocd::read_image(uint32_t ulNetxAddress, uint8_t *pucData, uint32_t sizData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnReadImage!=NULL )
	{
		/* Read memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnReadImage(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData, sizData);
		if( iResult!=0 )
		{
			fprintf(stderr, "read_image: Failed to read address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a byte (8bit) to the netX. */
int romloader_jtag_openocd::write_data08(uint32_t ulNetxAddress, uint8_t ucData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData08!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData08(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ucData);
		if( iResult!=0 )
		{
			fprintf(stderr, "write_data08: Failed to write address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a word (16bit) to the netX. */
int romloader_jtag_openocd::write_data16(uint32_t ulNetxAddress, uint16_t usData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData16!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData16(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, usData);
		if( iResult!=0 )
		{
			fprintf(stderr, "write_data16: Failed to write address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a long (32bit) to the netX. */
int romloader_jtag_openocd::write_data32(uint32_t ulNetxAddress, uint32_t ulData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteData32!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteData32(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ulData);
		if( iResult!=0 )
		{
			fprintf(stderr, "write_data32: Failed to write address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}



/* Write a byte array to the netX. */
int romloader_jtag_openocd::write_image(uint32_t ulNetxAddress, const uint8_t *pucData, uint32_t sizData)
{
	int iResult;


	/* Be pessimistic. */
	iResult = -1;

	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnWriteImage!=NULL )
	{
		/* Write memory. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnWriteImage(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, pucData, sizData);
		if( iResult!=0 )
		{
			fprintf(stderr, "write_image: Failed to write address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}

	return iResult;
}

/* int muhkuh_openocd_call(void *pvContext, uint32_t ulNetxAddress, uint32_t ulR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData) */
int romloader_jtag_openocd::call(uint32_t ulNetxAddress, uint32_t ulParameterR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData)
{
	int iResult;

	/* Be pessimistic. */
	iResult = -1;


	if( fJtagDeviceIsConnected==true && m_tJtagDevice.pvOpenocdContext!=NULL && m_tJtagDevice.tFunctions.tFn.pfnCall!=NULL )
	{
		/* Call code on netX. */
		iResult = m_tJtagDevice.tFunctions.tFn.pfnCall(m_tJtagDevice.pvOpenocdContext, ulNetxAddress, ulParameterR0, pfnCallback, pvCallbackUserData);
		if( iResult!=0 )
		{
			fprintf(stderr, "call: Failed to call code at address 0x%08x: %d\n", ulNetxAddress, iResult);
			iResult = -1;
		}
	}


	return iResult;
}


uint32_t romloader_jtag_openocd::get_image_chunk_size(void)
{
	/* This is the suggested size of a data chunk before executing the LUA callback. */
	/* TODO: Is a fixed number OK or should this depend on some parameters like the JTAG speed?
	 *       Maybe this could also be one fixed option for every interface/target pair - just like the reset code.
	 */
	return 1024;
}



void romloader_jtag_openocd::uninit(void)
{
#if 0
	struct command_context *ptCmdCtx;


	if( ptJtagDevice!=NULL )
	{
		ptCmdCtx = (struct command_context *)(ptJtagDevice->pvOpenocdContext);
		if( ptCmdCtx!=NULL )
		{
			unregister_all_commands(ptCmdCtx, NULL);

			/* free commandline interface */
			command_done(ptCmdCtx);

			adapter_quit();

			ptJtagDevice->pvOpenocdContext = NULL;
		}
	}
#endif
}

