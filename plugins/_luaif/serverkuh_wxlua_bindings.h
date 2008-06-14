// ---------------------------------------------------------------------------
// serverkuh_lua.h - headers and wxLua types for wxLua binding
//
// This file was generated by genwxbind.lua 
// Any changes made to this file will be lost when the file is regenerated
// ---------------------------------------------------------------------------

#ifndef __HOOK_WXLUA_serverkuh_lua_H__
#define __HOOK_WXLUA_serverkuh_lua_H__


#include "wxlua/include/wxlstate.h"
#include "wxlua/include/wxlbind.h"

// ---------------------------------------------------------------------------
// Check if the version of binding generator used to create this is older than
//   the current version of the bindings.
//   See 'bindings/genwxbind.lua' and 'modules/wxlua/include/wxldefs.h'
#if WXLUA_BINDING_VERSION > 23
#   error "The WXLUA_BINDING_VERSION in the bindings is too old, regenerate bindings."
#endif //WXLUA_BINDING_VERSION > 23
// ---------------------------------------------------------------------------

// binding class
class WXLUA_NO_DLLIMPEXP wxLuaBinding_serverkuh_lua : public wxLuaBinding
{
public:
    wxLuaBinding_serverkuh_lua();


private:
    DECLARE_DYNAMIC_CLASS(wxLuaBinding_serverkuh_lua)
};


// initialize wxLuaBinding_serverkuh_lua for all wxLuaStates
extern WXLUA_NO_DLLIMPEXP bool wxLuaBinding_serverkuh_lua_init();

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------

#include "application/muhkuh_lua_interface.h"

// ---------------------------------------------------------------------------
// Lua Tag Method Values and Tables for each Class
// ---------------------------------------------------------------------------

extern WXLUA_NO_DLLIMPEXP_DATA(int) wxluatype_muhkuh_plugin_instance;
extern WXLUA_NO_DLLIMPEXP wxLuaBindMethod muhkuh_plugin_instance_methods[];
extern WXLUA_NO_DLLIMPEXP_DATA(int) muhkuh_plugin_instance_methodCount;
extern WXLUA_NO_DLLIMPEXP_DATA(int) wxluatype_muhkuh_wrap_xml;
extern WXLUA_NO_DLLIMPEXP wxLuaBindMethod muhkuh_wrap_xml_methods[];
extern WXLUA_NO_DLLIMPEXP_DATA(int) muhkuh_wrap_xml_methodCount;


// ---------------------------------------------------------------------------
// Encapsulation Declarations - need to be public for other bindings.
// ---------------------------------------------------------------------------


#endif // __HOOK_WXLUA_serverkuh_lua_H__

