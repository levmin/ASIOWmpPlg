/////////////////////////////////////////////////////////////////////////////
//
// MFDisabler.cpp : Implementation of CMFDisabler
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <initguid.h>
#include "MFDisabler.h"
#include <mediaerr.h>   // DirectX SDK media errors
#include <dmort.h>      // DirectX SDK DMO runtime support
#include <uuids.h>      // DirectX SDK media types and subtyes
#include <ks.h>         // required for WAVEFORMATEXTENSIBLE
#include <mmreg.h>
#include <ksmedia.h>
#include <Mfidl.h>
#include <vector>


#pragma warning (disable: 4996) //disable security-related warnings
HRESULT CMFDisabler::FinalConstruct()
{
	static bool bFirstRun=true;
	if (!bFirstRun)
		return S_OK;
	bFirstRun = false;

	//Get the IMFPluginControl interface
	IMFPluginControl * pControl = NULL;
	typedef HRESULT (__stdcall * MFGetPluginControlType)( __out IMFPluginControl **ppPluginControl );
	MFGetPluginControlType pMFGetPluginControl = NULL;
	char * LIB_NAME = "mfplat.dll";
	bool bLibLoaded=false;
	HMODULE hModule = GetModuleHandle(LIB_NAME);
	if (!hModule) 
	{//load the library
		hModule = LoadLibrary(LIB_NAME);
		bLibLoaded = (hModule!=NULL);
	}
	if (hModule) 
		pMFGetPluginControl = (MFGetPluginControlType)GetProcAddress(hModule,"MFGetPluginControl");
	if (pMFGetPluginControl)
		(*pMFGetPluginControl)(&pControl);

	//Disable relevant MF components
	if (pControl)
	{
		CLSID id=GUID_NULL;
		LPWSTR idStrW = NULL;
		LPWSTR selector = NULL;
		const wchar_t * pBSH = L"Byte Stream Handler";
		const wchar_t * pCLSID = L"CLSID\\";
		const size_t BSH_LENGTH = wcslen(pBSH);
		const size_t CLSID_LENGTH = wcslen(pCLSID);
		//Disable all MF byte stream handlers except for the ASF byte stream handler
		for (DWORD index = 0;pControl->GetPreferredClsidByIndex(MF_Plugin_Type_MediaSource,index,&selector,&id) == S_OK;index++)
		{
			//find the user friendly name of the source
			StringFromCLSID(id,&idStrW);
			size_t idLength = wcslen(idStrW);
			std::vector<wchar_t> regKey;
			regKey.resize(CLSID_LENGTH+idLength+1);
			wcscpy(&regKey[0],pCLSID);
			wcscpy(&regKey[CLSID_LENGTH],idStrW);
			CoTaskMemFree(idStrW);
			idStrW = NULL;
			DWORD size = 0;
			RegGetValueW(HKEY_CLASSES_ROOT,&regKey[0],NULL,RRF_RT_REG_SZ,NULL,NULL,&size);
			std::vector<wchar_t> name;
			name.resize(size);
			RegGetValueW(HKEY_CLASSES_ROOT,&regKey[0],NULL,RRF_RT_REG_SZ,NULL,&name[0],&size);
			size_t offset = wcslen(&name[0]) - BSH_LENGTH;
			if(offset >=0 && _wcsicmp(&name[0]+offset,pBSH) == 0)
				if (_wcsicmp(&name[0],L"ASF Byte Stream Handler") != 0) 
					pControl->SetDisabled(MF_Plugin_Type_MediaSource,id,TRUE);
			CoTaskMemFree(selector);
		}
		//disable all MF decoders
		for (DWORD index = 0;pControl->GetPreferredClsidByIndex(MF_Plugin_Type_MFT,index,&selector,&id) == S_OK;index++)
		{
			StringFromCLSID(id,&idStrW);
			size_t idLength = wcslen(idStrW);
			std::vector<wchar_t> regKey;
			regKey.resize(CLSID_LENGTH+idLength+1);
			wcscpy(&regKey[0],pCLSID);
			wcscpy(&regKey[CLSID_LENGTH],idStrW);
			CoTaskMemFree(idStrW);
			idStrW = NULL;
			DWORD size = 0;
			RegGetValueW(HKEY_CLASSES_ROOT,&regKey[0],NULL,RRF_RT_REG_SZ,NULL,NULL,&size);
			if (size)
			{
				std::vector<wchar_t> name;
				name.resize(size);
				RegGetValueW(HKEY_CLASSES_ROOT,&regKey[0],NULL,RRF_RT_REG_SZ,NULL,&name[0],&size);
				_wcslwr(&name[0]);
				if(wcsstr(&name[0],L"decoder"))
					pControl->SetDisabled(MF_Plugin_Type_MFT,id,TRUE);
			}
			CoTaskMemFree(selector);
		}
		pControl->Release();
		pControl = NULL;
	}

	//maybe unload mfplat.dll 
	if (bLibLoaded)
		FreeLibrary(hModule); 

    //Call DwmEnableMMCSS to disable DWM participation in MMCSS
	typedef HRESULT (WINAPI * DwmEnableMMCSSType)(BOOL fEnableMMCSS);	
	DwmEnableMMCSSType pDwmEnableMMCSS = NULL;
	LIB_NAME = "dwmapi.dll";
	bLibLoaded = false;
	hModule = GetModuleHandle(LIB_NAME);
	if (!hModule) 
	{//load the library
		hModule = LoadLibrary(LIB_NAME);
		bLibLoaded = (hModule!=NULL);
	}
	if (hModule) 
		pDwmEnableMMCSS = (DwmEnableMMCSSType)GetProcAddress(hModule,"DwmEnableMMCSS");
	if (pDwmEnableMMCSS)
        (*pDwmEnableMMCSS)(FALSE);

	//maybe unload dwmapi.dll
	if (bLibLoaded)
		FreeLibrary(hModule); 

    //If Windows Media Player is not properly shut down (e.g. in case of a system crash),
    //it disables all "optional" plugins. Overwrite this behavior by removing
    //registry keys under the "Health" key 
    CRegKey key;
    const TCHAR wmpHealthRegKey[] = _T("Software\\Microsoft\\MediaPlayer");
    key.Open(HKEY_CURRENT_USER, wmpHealthRegKey);
    key.RecurseDeleteKey("Health");

	return S_OK;
}

void CMFDisabler::FinalRelease()
{
    FreeStreamingResources();  // In case client does not call this.
}

STDMETHODIMP CMFDisabler::GetStreamCount( 
               DWORD *pcInputStreams,
               DWORD *pcOutputStreams)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputStreamInfo( 
               DWORD dwInputStreamIndex,
               DWORD *pdwFlags)
{    
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetOutputStreamInfo( 
               DWORD dwOutputStreamIndex,
               DWORD *pdwFlags)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputType ( 
               DWORD dwInputStreamIndex,
               DWORD dwTypeIndex,
               DMO_MEDIA_TYPE *pmt)
{
	return E_NOTIMPL;
}

STDMETHODIMP CMFDisabler::GetOutputType( 
               DWORD dwOutputStreamIndex,
               DWORD dwTypeIndex,
               DMO_MEDIA_TYPE *pmt)
{
	return E_NOTIMPL;
}

STDMETHODIMP CMFDisabler::SetInputType( 
               DWORD dwInputStreamIndex,
               const DMO_MEDIA_TYPE *pmt,
               DWORD dwFlags)
{
	return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::SetOutputType( 
               DWORD dwOutputStreamIndex,
               const DMO_MEDIA_TYPE *pmt,
               DWORD dwFlags)
{ 
	return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputCurrentType( 
               DWORD dwInputStreamIndex,
               DMO_MEDIA_TYPE *pmt)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetOutputCurrentType( 
               DWORD dwOutputStreamIndex,
               DMO_MEDIA_TYPE *pmt)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputSizeInfo( 
               DWORD dwInputStreamIndex,
               DWORD *pcbSize,
               DWORD *pcbMaxLookahead,
               DWORD *pcbAlignment)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetOutputSizeInfo( 
               DWORD dwOutputStreamIndex,
               DWORD *pcbSize,
               DWORD *pcbAlignment)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputMaxLatency( 
               DWORD dwInputStreamIndex,
               REFERENCE_TIME *prtMaxLatency)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::SetInputMaxLatency( 
               DWORD dwInputStreamIndex,
               REFERENCE_TIME rtMaxLatency)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::Flush( void )
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::Discontinuity( 
               DWORD dwInputStreamIndex)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::AllocateStreamingResources ( void )
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::FreeStreamingResources( void )
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetInputStatus( 
           DWORD dwInputStreamIndex,
           DWORD *pdwFlags)
{ 
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::ProcessInput( 
               DWORD dwInputStreamIndex,
               IMediaBuffer *pBuffer,
               DWORD dwFlags,
               REFERENCE_TIME rtTimestamp,
               REFERENCE_TIME rtTimelength)
{ 
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::ProcessOutput( 
               DWORD dwFlags,
               DWORD cOutputBufferCount,
               DMO_OUTPUT_DATA_BUFFER *pOutputBuffers,
               DWORD *pdwStatus)
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::Lock( LONG bLock )
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::SetEnable( BOOL fEnable )
{
    return E_NOTIMPL; 
}

STDMETHODIMP CMFDisabler::GetEnable( BOOL *pfEnable )
{
    return E_NOTIMPL; 
}

