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

#pragma once

#include "resource.h"
#include <mediaobj.h>       // The IMediaObject header from the DirectX SDK.
#include <wmpservices.h>    // The header containing the WMP interface definitions.

#ifdef _WIN64
   #include "../ASIOWmpPlgPS/x64/ASIOWmpPlg_h.h"  // Generated from the IDL file during proxy/stub compilation.
#else
   #include "../ASIOWmpPlgPS/Win32/ASIOWmpPlg_h.h"  // Generated from the IDL file during proxy/stub compilation.
#endif

const DWORD UNITS = 10000000;  // 1 sec = 1 * UNITS
const DWORD MAXSTRING = 1024;

// registry location for preferences
const TCHAR kszPrefsRegKey[] = _T("Software\\ASIOWmpPlg\\DSP Plugin");
const TCHAR kszPrefsCLSID[] = _T("CLSID");

// {67FF2DAA-823A-45FE-8BF4-484EC6B9D52C}
DEFINE_GUID(CLSID_ASIOWmpPlg, 0x67ff2daa, 0x823a, 0x45fe, 0x8b, 0xf4, 0x48, 0x4e, 0xc6, 0xb9, 0xd5, 0x2c);

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg
/////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE CASIOWmpPlg : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CASIOWmpPlg, &CLSID_ASIOWmpPlg>,
    public IASIOWmpPlg,
    public IMediaObject,
    public IWMPPluginEnable,
    public ISpecifyPropertyPages,
	public IWMPPlugin
{
public:
    CASIOWmpPlg();
    virtual ~CASIOWmpPlg();

DECLARE_REGISTRY_RESOURCEID(IDR_ASIOWMPPLG)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CASIOWmpPlg)
    COM_INTERFACE_ENTRY(IASIOWmpPlg)
    COM_INTERFACE_ENTRY(IMediaObject)
    COM_INTERFACE_ENTRY(IWMPPluginEnable)
    COM_INTERFACE_ENTRY(ISpecifyPropertyPages)
    COM_INTERFACE_ENTRY(IWMPPlugin)
END_COM_MAP()

    // CComCoClass Overrides
    HRESULT FinalConstruct();
    void    FinalRelease();

    // IASIOWmpPlg methods
    STDMETHOD(getCLSID)(CLSID *pVal);
    STDMETHOD(putCLSID)(CLSID newVal);

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

    STDMETHOD( DoProcessOutput )( 
                    BYTE *pbOutputData,
                    const BYTE *pbInputData,
                    DWORD *cbBytesProcessed);
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

    // ISpecifyPropertyPages methods
    STDMETHOD( GetPages )(CAUUID *pPages);

	// IWMPPlugin methods
    STDMETHOD( Init(DWORD_PTR dwPlaybackContext) ) ;
    STDMETHOD( Shutdown( void) ) ;
    STDMETHOD( GetID( GUID *pGUID) ) ;
    STDMETHOD( GetCaps( DWORD *pdwFlags) ) ;
    STDMETHOD( AdviseWMPServices( IWMPServices *pWMPServices) ) ;
    STDMETHOD( UnAdviseWMPServices( void) ) ;


private:
    HRESULT SendDataToASIO(DWORD dwSamplesToProcess);
    HRESULT ValidateMediaType(
                const DMO_MEDIA_TYPE *pmtTarget,    // target media type to verify
                const DMO_MEDIA_TYPE *pmtPartner);  // partner media type to verify


    DMO_MEDIA_TYPE          m_mtInput;          // Stores the input format structure
    DMO_MEDIA_TYPE          m_mtOutput;         // Stores the output format structure

    CComPtr<IMediaBuffer>   m_spInputBuffer;    // Smart pointer to the input buffer object


    BYTE*                   m_pbInputData;      // Pointer to the data in the input buffer
    DWORD                   m_cbInputLength;    // Length of the data in the input buffer
   
    bool                    m_bValidTime;       // Is timestamp valid?
    REFERENCE_TIME          m_rtTimestamp;      // Stores the input buffer timestamp

    BOOL                    m_bEnabled;         // TRUE if enabled

	IWMPServices * m_pWMPServices;              // Pointer on IWMPServices interface exposed by WMP
};

