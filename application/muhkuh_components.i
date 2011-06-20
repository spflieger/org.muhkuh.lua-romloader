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


%module muhkuh_components_lua

%{
	#include "muhkuh_components.h"
%}

%include muhkuh_typemaps.i

/*
typedef struct
{
	const char *pcPluginName;                                                                                                                                                                                      
	const char *pcPluginId;
	unsigned int uiVersionMajor;                                                                                                                                                                                                     
	unsigned int uiVersionMinor;                                                                                                                                                                                                     
	unsigned int uiVersionSub;
} MUHKUH_PLUGIN_DESCRIPTION_T;
*/

const char *get_version();


/* Plugin functions. */
/* long plugin_add(lua_State *lua_State, const char *pcPluginCfgName) */
/* void plugin_remove(lua_State *lua_State, unsigned long ulIdx) */

int plugin_count(lua_State *lua_State);

/* MUHKUH_PLUGIN_DESCRIPTION_T *plugin_get_description(lua_State *lua_State, unsigned long ulIdx) */
/* const char *plugin_get_config_name(lua_State *lua_State, unsigned long ulIdx); */
bool plugin_is_ok(lua_State *lua_State, unsigned long ulIdx);
/* const char *plugin_get_init_error(lua_State *lua_State, unsigned long ulIdx); */
void plugin_set_enable(lua_State *lua_State, unsigned long ulIdx, bool fPluginIsEnabled);
bool plugin_get_enable(lua_State *lua_State, unsigned long ulIdx);

