/////////////////////////////////////////////////////////////////////////////
//
// MFDisabler.h : Declaration of CMFDisabler
//
// Note: Requires DirectX 8 SDK or later.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
/////////////////////////////////////////////////////////////////////////////
  
#ifndef __CMFT_DISABLER_H_
#define __CMFT_DISABLER_H_

#include "resource.h"
#include <mediaobj.h>       // The IMediaObject header from the DirectX SDK.
#include "wmpservices.h"    // The header containing the WMP interface definitions.

const DWORD UNITS = 10000000;  // 1 sec = 1 * UNITS
const DWORD MAXSTRING = 1024;

// registry location for preferences
const WCHAR kwszPrefsRegKey[] = L"Software\\MFDisabler\\DSP Plugin";

// {7CD5CAAC-CF19-420F-AB4A-91E7AF4B2FE8}
DEFINE_GUID(CLSID_MFDisabler, 0x7cd5caac, 0xcf19, 0x420f, 0xab, 0x4a, 0x91, 0xe7, 0xaf, 0x4b, 0x2f, 0xe8);

/////////////////////////////////////////////////////////////////////////////
// CMFDisabler
/////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE CMFDisabler : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMFDisabler, &CLSID_MFDisabler>,
    public IMediaObject,
    public IWMPPluginEnable
{
public:
	virtual ~CMFDisabler() {}

DECLARE_REGISTRY_RESOURCEID(IDR_MF_DISABLER)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CMFDisabler)
    COM_INTERFACE_ENTRY(IMediaObject)
    COM_INTERFACE_ENTRY(IWMPPluginEnable)
END_COM_MAP()

    // CComCoClass Overrides
    HRESULT FinalConstruct();
    void    FinalRelease();

   // IMediaObject methods
    STDMETHOD( GetStreamCount )( 
                   DWORD *pcInputStreams,
                   DWORD *pcOutputStreams
                   );
    
    STDMETHOD( GetInputStreamInfo )( 
                   DWORD dwInputStreamIndex,
                   DWORD *pdwFlags
                   );
    
    STDMETHOD( GetOutputStreamInfo )( 
                   DWORD dwOutputStreamIndex,
                   DWORD *pdwFlags
                   );
    
    STDMETHOD( GetInputType )( 
                   DWORD dwInputStreamIndex,
                   DWORD dwTypeIndex,
                   DMO_MEDIA_TYPE *pmt
                   );
    
    STDMETHOD( GetOutputType )( 
                   DWORD dwOutputStreamIndex,
                   DWORD dwTypeIndex,
                   DMO_MEDIA_TYPE *pmt
                   );
    
    STDMETHOD( SetInputType )( 
                   DWORD dwInputStreamIndex,
                   const DMO_MEDIA_TYPE *pmt,
                   DWORD dwFlags
                   );
    
    STDMETHOD( SetOutputType )( 
                   DWORD dwOutputStreamIndex,
                   const DMO_MEDIA_TYPE *pmt,
                   DWORD dwFlags
                   );
    
    STDMETHOD( GetInputCurrentType )( 
                   DWORD dwInputStreamIndex,
                   DMO_MEDIA_TYPE *pmt
                   );
    
    STDMETHOD( GetOutputCurrentType )( 
                   DWORD dwOutputStreamIndex,
                   DMO_MEDIA_TYPE *pmt
                   );
    
    STDMETHOD( GetInputSizeInfo )( 
                   DWORD dwInputStreamIndex,
                   DWORD *pcbSize,
                   DWORD *pcbMaxLookahead,
                   DWORD *pcbAlignment
                   );
    
    STDMETHOD( GetOutputSizeInfo )( 
                   DWORD dwOutputStreamIndex,
                   DWORD *pcbSize,
                   DWORD *pcbAlignment
                   );
    
    STDMETHOD( GetInputMaxLatency )( 
                   DWORD dwInputStreamIndex,
                   REFERENCE_TIME *prtMaxLatency
                   );
    
    STDMETHOD( SetInputMaxLatency )( 
                   DWORD dwInputStreamIndex,
                   REFERENCE_TIME rtMaxLatency
                   );
    
    STDMETHOD( Flush )( void );
    
    STDMETHOD( Discontinuity )( 
                   DWORD dwInputStreamIndex
                   );
    
    STDMETHOD( AllocateStreamingResources )( void );
    
    STDMETHOD( FreeStreamingResources )( void );
    
    STDMETHOD( GetInputStatus )( 
                   DWORD dwInputStreamIndex,
                   DWORD *pdwFlags
                   );
    
    STDMETHOD( ProcessInput )( 
                   DWORD dwInputStreamIndex,
                   IMediaBuffer *pBuffer,
                   DWORD dwFlags,
                   REFERENCE_TIME rtTimestamp,
                   REFERENCE_TIME rtTimelength
                   );
    
    STDMETHOD( ProcessOutput )( 
                   DWORD dwFlags,
                   DWORD cOutputBufferCount,
                   DMO_OUTPUT_DATA_BUFFER *pOutputBuffers,
                   DWORD *pdwStatus
                   );

    STDMETHOD( Lock )( LONG bLock );

    // Note: need to override CComObjectRootEx::Lock to avoid
    // ambiguity with IMediaObject::Lock. The override just
    // calls through to the base class implementation.

    // CComObjectRootEx overrides
    void Lock()
    {
        CComObjectRootEx<CComMultiThreadModel>::Lock();
    }


    // IWMPPluginEnable methods
    STDMETHOD( SetEnable )( BOOL fEnable );
    STDMETHOD( GetEnable )( BOOL *pfEnable );


};

#endif //__CMFT_DISABLER_H_
