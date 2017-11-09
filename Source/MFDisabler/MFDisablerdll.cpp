// MFDisablerdll.cpp : Implementation of DLL Exports.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include <VersionHelpers.h>

#include "MFDisabler.h"

#include <crtdbg.h>     // Debug header
#include <uuids.h>      // DirectX SDK media types and subtyes
#include <dmoreg.h>     // DirectX SDK registration

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_MFDisabler, CMFDisabler)
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
    DMO_PARTIAL_MEDIATYPE mt = { 0 };
    mt.type = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_PCM;

    // Register the plug-in with WMP (for legacy pipeline support)
    hr = spRegistrar->WMPRegisterPlayerPlugin(
                    bstrFriendlyName,   // friendly name (for menus, etc)
                    bstrDescription,    // description (for Tools->Options->Plug-ins)
                    NULL,               // path to app that uninstalls the plug-in
                    1,                  // DirectShow priority for this plug-in
                    WMP_PLUGINTYPE_DSP, // Plug-in type
                    CLSID_MFDisabler,// Class ID of plug-in
                    1,                  // No. media types supported by plug-in
                    &mt);               // Array of media types supported by plug-in

    // Also register for out-of-proc playback in the MF pipeline
    // We'll only do this on Windows Vista or later operating systems because
    // WMP 11 and Vista are required at a minimum.
    if (SUCCEEDED(hr) && 
        TRUE == IsWindowsVistaOrGreater())
    {
        hr = spRegistrar->WMPRegisterPlayerPlugin(
                        bstrFriendlyName,   // friendly name (for menus, etc)
                        bstrDescription,    // description (for Tools->Options->Plug-ins)
                        NULL,               // path to app that uninstalls the plug-in
                        1,                  // DirectShow priority for this plug-in
                        WMP_PLUGINTYPE_DSP_OUTOFPROC, // Plug-in type
                        CLSID_MFDisabler,// Class ID of plug-in
                        1,                  // No. media types supported by plug-in
                        &mt);               // Array of media types supported by plug-in
    }

    if (FAILED(hr))
    {
        return hr;
    }

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
    hr = spRegistrar->WMPUnRegisterPlayerPlugin(WMP_PLUGINTYPE_DSP, CLSID_MFDisabler);
 
    if(TRUE == IsWindowsVistaOrGreater())
    {
        // Also unregister from the MF pipeline
        hr = spRegistrar->WMPUnRegisterPlayerPlugin(WMP_PLUGINTYPE_DSP_OUTOFPROC, CLSID_MFDisabler);
    }

    return _Module.UnregisterServer();
}
