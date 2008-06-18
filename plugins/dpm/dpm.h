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


#include <vector>

#include <wx/wx.h>
#include <wx/dynlib.h>

#include <wxlua/include/wxlua.h>
#include <wxluasocket/include/wxldserv.h>
extern "C"
{
	#include <lualib.h>
}

#include "../muhkuh_plugin_interface.h"


#ifndef __DPM_H__
#define __DPM_H__

/*-----------------------------------*/

/* open the connection to the device */
typedef bool (*dpm_fn_connect)(void *pvHandle);
/* close the connection to the device */
typedef void (*dpm_fn_disconnect)(void *pvHandle);
/* returns the connection state of the device */
typedef bool (*dpm_fn_is_connected)(void *pvHandle);

/* read a byte (8bit) from the dpm */
typedef int (*dpm_fn_read_data08)(void *pvHandle, unsigned long ulNetxAddress, unsigned char *pucData);
/* read a word (16bit) from the dpm */
typedef int (*dpm_fn_read_data16)(void *pvHandle, unsigned long ulNetxAddress, unsigned short *pusData);
/* read a long (32bit) from the dpm */
typedef int (*dpm_fn_read_data32)(void *pvHandle, unsigned long ulNetxAddress, unsigned long *pulData);
/* read a byte array from the dpm */
typedef int (*dpm_fn_read_image)(void *pvHandle, unsigned long ulNetxAddress, char *pcData, unsigned long ulSize, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData);

/* write a byte (8bit) to the dpm */
typedef int (*dpm_fn_write_data08)(void *pvHandle, unsigned long ulNetxAddress, unsigned char ucData);
/* write a word (16bit) to the dpm */
typedef int (*dpm_fn_write_data16)(void *pvHandle, unsigned long ulNetxAddress, unsigned short usData);
/* write a long (32bit) to the dpm */
typedef int (*dpm_fn_write_data32)(void *pvHandle, unsigned long ulNetxAddress, unsigned long ulData);
/* write a byte array to the dpm */
typedef int (*dpm_fn_write_image)(void *pvHandle, unsigned long ulNetxAddress, const char *pcData, unsigned long ulSize, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData);

/*-----------------------------------*/

typedef struct
{
	dpm_fn_connect		fn_connect;
	dpm_fn_disconnect	fn_disconnect;
	dpm_fn_is_connected	fn_is_connected;
	dpm_fn_read_data08	fn_read_data08;
	dpm_fn_read_data16	fn_read_data16;
	dpm_fn_read_data32	fn_read_data32;
	dpm_fn_read_image	fn_read_image;
	dpm_fn_write_data08	fn_write_data08;
	dpm_fn_write_data16	fn_write_data16;
	dpm_fn_write_data32	fn_write_data32;
	dpm_fn_write_image	fn_write_image;
} dpm_functioninterface;

/*-----------------------------------*/

// NOTE: dpm is derived from wxObject to avoid the %encapsulate wrappers for garbage collection in lua
class dpm : public wxObject
{
public:
	dpm(wxString strName, wxString strTyp, const dpm_functioninterface *ptFn, void *pvHandle, muhkuh_plugin_fn_close_instance fn_close, wxLuaState *ptLuaState);
	~dpm(void);

// *** lua interface start ***
	// open the connection to the device
	void connect(void);
	// close the connection to the device
	void disconnect(void);
	// returns the connection state of the device
	bool is_connected(void) const;

	// returns the device name
	wxString get_name(void);
	// returns the device typ
	wxString get_typ(void);

	// read a byte (8bit) from the netx to the pc
	unsigned char read_data08(unsigned long ulNetxAddress);
	// read a word (16bit) from the netx to the pc
	unsigned short read_data16(unsigned long ulNetxAddress);
	// read a long (32bit) from the netx to the pc
	unsigned long read_data32(unsigned long ulNetxAddress);
	// read a byte array from the netx to the pc
	wxString read_image(unsigned long ulNetxAddress, unsigned long ulSize, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData);

	// write a byte (8bit) from the pc to the netx
	void write_data08(unsigned long ulNetxAddress, unsigned char ucData);
	// write a word (16bit) from the pc to the netx
	void write_data16(unsigned long ulNetxAddress, unsigned short usData);
	// write a long (32bit) from the pc to the netx
	void write_data32(unsigned long ulNetxAddress, unsigned long ulData);
	// write a byte array from the pc to the netx
	void write_image(unsigned long ulNetxAddress, wxString strData, lua_State *L, int iLuaCallbackTag, void *pvCallbackUserData);

// *** lua interface end ***

private:
	wxString m_strName;
	wxString m_strTyp;
	dpm_functioninterface m_tFunctionInterface;
	void *m_pvHandle;

	muhkuh_plugin_fn_close_instance m_fn_close;
	// the lua state
	wxLuaState *m_ptLuaState;
};

/*-----------------------------------*/

#endif	/* __DPM_H__ */
