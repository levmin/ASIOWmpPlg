/*  ASIOWmpPlg Windows Media Player Plugin
    Copyright (C) Lev Minkovsky
    
    This file is part of ASIOWmpPlg.

    ASIOWmpPlg is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIOWmpPlg is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIOWmpPlg; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "stdafx.h"
#include "resource.h"
#include <initguid.h>

#include "ASIOWmpPlg.h"
#include "ASIOWmpPlgPropPage.h"
#include "ASIOPlayer.h"

#include <crtdbg.h>     // Debug header
#include <uuids.h>      // DirectX SDK media types and subtyes
#include <dmoreg.h>     // DirectX SDK registration


CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
   OBJECT_ENTRY(CLSID_ASIOWmpPlg, CASIOWmpPlg)
   OBJECT_ENTRY(CLSID_ASIOWmpPlgPropPage, CASIOWmpPlgPropPage)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point
/////////////////////////////////////////////////////////////////////////////

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(ObjectMap, hInstance);
        DisableThreadLibraryCalls(hInstance);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        _Module.Term();
    }

    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE
/////////////////////////////////////////////////////////////////////////////

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type
/////////////////////////////////////////////////////////////////////////////

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)

{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry
/////////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer(void)
{
    CComPtr<IWMPMediaPluginRegistrar> spRegistrar;
    HRESULT hr;

    // Create the registration object
    hr = spRegistrar.CoCreateInstance(CLSID_WMPMediaPluginRegistrar, NULL, CLSCTX_INPROC_SERVER);
    if (FAILED(hr))
    {
        return hr;
    }
    
    // Load friendly name and description strings
    CComBSTR    bstrFriendlyName;
    CComBSTR    bstrDescription;

    bstrFriendlyName.LoadString(IDS_FRIENDLYNAME);
    bstrDescription.LoadString(IDS_DESCRIPTION);

    // Describe the type of data handled by the plug-in
    DMO_PARTIAL_MEDIATYPE mt[2] = { 0,0 };
    mt[0].type = MEDIATYPE_Audio;
    mt[0].subtype = MEDIASUBTYPE_PCM;
    mt[1].type = MEDIATYPE_Audio;
    mt[1].subtype = MEDIASUBTYPE_IEEE_FLOAT;

    // Register the plug-in with WMP (for legacy pipeline support)
    hr = spRegistrar->WMPRegisterPlayerPlugin(
                    bstrFriendlyName,   // friendly name (for menus, etc)
                    bstrDescription,    // description (for Tools->Options->Plug-ins)
                    NULL,               // path to app that uninstalls the plug-in
                    1,                  // DirectShow priority for this plug-in
                    WMP_PLUGINTYPE_DSP, // Plug-in type
                    CLSID_ASIOWmpPlg,// Class ID of plug-in
                    2,                  // No. media types supported by plug-in
                    &mt);               // Array of media types supported by plug-in

	//This plugin does not support Media Foundation pipeline, and should not
    //be registred out-of-proc
    //Remove possible previous out-of-proc registration if there is one
    spRegistrar->WMPUnRegisterPlayerPlugin(WMP_PLUGINTYPE_DSP_OUTOFPROC, CLSID_ASIOWmpPlg);
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer();
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry
/////////////////////////////////////////////////////////////////////////////

STDAPI DllUnregisterServer(void)

{
    CComPtr<IWMPMediaPluginRegistrar> spRegistrar;
    HRESULT hr;

    // Create the registration object
    hr = spRegistrar.CoCreateInstance(CLSID_WMPMediaPluginRegistrar, NULL, CLSCTX_INPROC_SERVER);
    if (FAILED(hr))
    {
        return hr;
    }

    // Tell WMP to remove this plug-in (for legacy pipeline support)
    hr = spRegistrar->WMPUnRegisterPlayerPlugin(WMP_PLUGINTYPE_DSP, CLSID_ASIOWmpPlg);
 
    return _Module.UnregisterServer();
}

