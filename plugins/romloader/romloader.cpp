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


#include <stdio.h>

#include "romloader.h"


romloader::romloader(const char *pcName, const char *pcTyp, muhkuh_plugin_provider *ptProvider)
 : muhkuh_plugin(pcName, pcTyp, ptProvider)
 , m_tChiptyp(ROMLOADER_CHIPTYP_UNKNOWN)
 , m_tRomcode(ROMLOADER_ROMCODE_UNKNOWN)
{
	printf("%s(%p): created in romloader\n", m_pcName, this);
}


romloader::~romloader(void)
{
	printf("%s(%p): deleted in romloader\n", m_pcName, this);
}


ROMLOADER_CHIPTYP romloader::GetChiptyp(void) const
{
	return m_tChiptyp;
}


ROMLOADER_ROMCODE romloader::GetRomcode(void) const
{
	return m_tRomcode;
}



const char *romloader::GetChiptypName(ROMLOADER_CHIPTYP tChiptyp) const
{
	const char *pcChiptyp;
	const ROMLOADER_RESET_ID_T *ptCnt, *ptEnd;


	// init chip name with unknown name
	pcChiptyp = "unknown chip";

	// loop over all known romcodes and search the romcode typ
	ptCnt = atResIds;
	ptEnd = ptCnt + (sizeof(atResIds)/sizeof(atResIds[0]));
	while( ptCnt<ptEnd )
	{
		if( ptCnt->tChiptyp==tChiptyp )
		{
			pcChiptyp = ptCnt->pcChiptypName;
			break;
		}
	}

	return pcChiptyp;
}


const char *romloader::GetRomcodeName(ROMLOADER_ROMCODE tRomcode) const
{
	const char *pcRomcode;
	const ROMLOADER_RESET_ID_T *ptCnt, *ptEnd;


	// init romcode name with unknown name
	pcRomcode = "unknown romcode";

	// loop over all known romcodes and search the romcode typ
	ptCnt = atResIds;
	ptEnd = ptCnt + (sizeof(atResIds)/sizeof(atResIds[0]));
	while( ptCnt<ptEnd )
	{
		if( ptCnt->tRomcode==tRomcode )
		{
			pcRomcode = ptCnt->pcRomcodeName;
			break;
		}
	}

	return pcRomcode;
}


bool romloader::detect_chiptyp(void)
{
	unsigned long ulResetVector;
	const ROMLOADER_RESET_ID_T *ptRstCnt, *ptRstEnd;
	unsigned long ulVersionAddr;
	unsigned long ulVersion;
	bool fResult;
	const char *pcChiptypName;
	const char *pcRomcodeName;


	m_tChiptyp = ROMLOADER_CHIPTYP_UNKNOWN;
	m_tRomcode = ROMLOADER_ROMCODE_UNKNOWN;

	// read the reset vector at 0x00000000
	ulResetVector = read_data32(0);
	printf("%s(%p): reset vector: 0x%08X", m_pcName, this, ulResetVector);

	// match the reset vector to all known chipfamilies
	ptRstCnt = atResIds;
	ptRstEnd = ptRstCnt + (sizeof(atResIds)/sizeof(atResIds[0]));
	ulVersionAddr = 0xffffffff;
	while( ptRstCnt<ptRstEnd )
	{
		if( ptRstCnt->ulResetVector==ulResetVector )
		{
			ulVersionAddr = ptRstCnt->ulVersionAddress;
			// read version address
			ulVersion = read_data32(ulVersionAddr);
			printf("%s(%p): version value: 0x%08X", m_pcName, this, ulVersion);
			if( ptRstCnt->ulVersionValue==ulVersion )
			{
				// found chip!
				m_tChiptyp = ptRstCnt->tChiptyp;
				m_tRomcode = ptRstCnt->tRomcode;
				break;
			}
		}
		++ptRstCnt;
	}

	// found something?
	fResult = ( m_tChiptyp!=ROMLOADER_CHIPTYP_UNKNOWN && m_tRomcode!=ROMLOADER_ROMCODE_UNKNOWN );

	if( fResult!=true )
	{
		fprintf(stderr, "%s(%p): unknown chip!", m_pcName, this);
	}
	else
	{
		pcChiptypName = GetChiptypName(m_tChiptyp);
		pcRomcodeName = GetRomcodeName(m_tRomcode);
		printf("%s(%p): found chip %s with romcode %s", m_pcName, this, pcChiptypName, pcRomcodeName);
	}

	return fResult;
}


const romloader::ROMLOADER_RESET_ID_T romloader::atResIds[3] =
{
	{
		0xea080001,
		0x00200008,
		0x00001000,
		ROMLOADER_CHIPTYP_NETX500,
		"netX500",
		ROMLOADER_ROMCODE_ABOOT,
		"ABoot"
	},

	{
		0xea080002,
		0x00200008,
		0x00003002,
		ROMLOADER_CHIPTYP_NETX100,
		"netX100",
		ROMLOADER_ROMCODE_ABOOT,
		"ABoot"
	},

	{
		0xeac83ffc,
		0x08200008,
		0x00002001,
		ROMLOADER_CHIPTYP_NETX50,
		"netX50",
		ROMLOADER_ROMCODE_HBOOT,
		"HBoot"
	},
};


unsigned int romloader::crc16(unsigned int uCrc, unsigned int uData)
{
	uCrc  = (uCrc >> 8) | ((uCrc & 0xff) << 8);
	uCrc ^= uData;
	uCrc ^= (uCrc & 0xff) >> 4;
	uCrc ^= (uCrc & 0x0f) << 12;
	uCrc ^= ((uCrc & 0xff) << 4) << 1;

	return uCrc;
}


/*
bool romloader::callback_long(lua_State *L, int iLuaCallbackTag, long lProgressData, void *pvCallbackUserData)
{
	bool fStillRunning;
	int iOldTopOfStack;


	// default value
	fStillRunning = false;

	// check lua state and callback tag
	if( L!=NULL && iLuaCallbackTag!=0 )
	{
		// get the current stack position
		iOldTopOfStack = lua_gettop(L);
		// push the function tag on the stack
		lua_rawgeti(L, LUA_REGISTRYINDEX, iLuaCallbackTag);
		// push the arguments on the stack
		lua_pushnumber(L, lProgressData);
		fStillRunning = callback_common(L, pvCallbackUserData, iOldTopOfStack);
	}

	return fStillRunning;
}


bool romloader::callback_string(lua_State *L, int iLuaCallbackTag, wxString strProgressData, void *pvCallbackUserData)
{
	bool fStillRunning;
	int iOldTopOfStack;


	// default value
	fStillRunning = false;

	// check lua state and callback tag
	if( L!=NULL && iLuaCallbackTag!=0 )
	{
		// get the current stack position
		iOldTopOfStack = lua_gettop(L);
		// push the function tag on the stack
		lua_rawgeti(L, LUA_REGISTRYINDEX, iLuaCallbackTag);
		// push the arguments on the stack
		lua_pushlstring(L, strProgressData.fn_str(), strProgressData.Len());
		fStillRunning = callback_common(L, pvCallbackUserData, iOldTopOfStack);
	}

	return fStillRunning;
}


bool romloader::callback_common(lua_State *L, void *pvCallbackUserData, int iOldTopOfStack)
{
	bool fStillRunning;
	int iResult;
	int iLuaType;
	wxString strMsg;


	// check lua state and callback tag
	if( L!=NULL )
	{
		lua_pushnumber(L, (long)pvCallbackUserData);
		// call the function
		iResult = lua_pcall(L, 2, 1, 0);
		if( iResult!=0 )
		{
			switch( iResult )
			{
			case LUA_ERRRUN:
				strMsg = wxT("runtime error");
				break;
			case LUA_ERRMEM:
				strMsg = wxT("memory allocation error");
				break;
			default:
				strMsg.Printf(wxT("unknown errorcode: %d"), iResult);
				break;
			}
			wxLogError(wxT("callback function failed: ") + strMsg);
			strMsg = wxlua_getstringtype(L, -1);
			wxLogError(strMsg);
			wxLogError(wxT("cancel operation"));
			fStillRunning = false;
		}
		else
		{
			// get the function's return value
			iLuaType = lua_type(L, -1);
			if( wxlua_iswxluatype(iLuaType, WXLUA_TBOOLEAN)==false )
			{
				wxLogError(wxT("callback function returned a non-boolean type!"));
				fStillRunning = false;
			}
			else
			{
				if( iLuaType==LUA_TNUMBER )
				{
					iResult = lua_tonumber(L, -1);
				}
				else
				{
					iResult = lua_toboolean(L, -1);
				}
				fStillRunning = (iResult!=0);
			}
		}
		// return old stack top
		lua_settop(L, iOldTopOfStack);
	}
	else
	{
		// no callback function -> keep running
		fStillRunning = true;
	}

	// allow gui updates
	wxTheApp->Yield();

	return fStillRunning;
}
*/

