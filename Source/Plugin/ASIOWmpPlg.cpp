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
#include <initguid.h>
#include "ASIOWmpPlg.h"
#include "ASIOWmpPlgPropPage.h"
#define KS_NO_CREATE_FUNCTIONS
#include <ks.h>         // required for WAVEFORMATEXTENSIBLE
#include <mediaerr.h>   // DirectX SDK media errors
#include <dmort.h>      // DirectX SDK DMO runtime support
#include <uuids.h>      // DirectX SDK media types and subtyes
#include <mmreg.h>
#include <ksmedia.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <math.h>
#include <limits.h>
#include "ASIOPlayer.h"


/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::CASIOWmpPlg
//
// Constructor
/////////////////////////////////////////////////////////////////////////////

CASIOWmpPlg::CASIOWmpPlg()
{
    g_player.incrementPluginCounter();
    m_cbInputLength = 0;
    m_pbInputData = NULL;
    m_bValidTime = false;
    m_rtTimestamp = 0;
    m_bEnabled = TRUE;
    ::ZeroMemory(&m_mtInput, sizeof(m_mtInput));
    ::ZeroMemory(&m_mtOutput, sizeof(m_mtOutput));
	m_pWMPServices = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::~CASIOWmpPlg
//
// Destructor
/////////////////////////////////////////////////////////////////////////////

CASIOWmpPlg::~CASIOWmpPlg()
{
    g_player.maybeShutdown();
    ::MoFreeMediaType(&m_mtInput);
    ::MoFreeMediaType(&m_mtOutput);
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::FinalConstruct
//
// Called when an plug-in is first loaded. Use this function to do one-time
// intializations that could fail instead of doing this in the constructor,
// which cannot return an error.
/////////////////////////////////////////////////////////////////////////////

HRESULT CASIOWmpPlg::FinalConstruct()
{
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg:::FinalRelease
//
// Called when an plug-in is unloaded. Use this function to free any
// resources allocated.
/////////////////////////////////////////////////////////////////////////////

void CASIOWmpPlg::FinalRelease()
{
   FreeStreamingResources();  // In case client does not call this.        
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetStreamCount
//
// Implementation of IMediaObject::GetStreamCount
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::GetStreamCount( 
               DWORD *pcInputStreams,
               DWORD *pcOutputStreams)
{
    HRESULT hr = S_OK;

    if ( NULL == pcInputStreams )
    {
        return E_POINTER;
    }

    if ( NULL == pcOutputStreams )
    {
        return E_POINTER;
    }

    // The plug-in uses one stream in each direction.
    *pcInputStreams = 1;
    *pcOutputStreams = 1;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputStreamInfo
//
// Implementation of IMediaObject::GetInputStreamInfo
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetInputStreamInfo( 
               DWORD dwInputStreamIndex,
               DWORD *pdwFlags)
{    
    if ( NULL == pdwFlags )
    {
        return E_POINTER;
    }

    // The stream index must be zero.
    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    // Use the default input stream configuration (a single stream). 
    *pdwFlags = 0;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetOutputStreamInfo
//
// Implementation of IMediaObject::GetOutputStreamInfo
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetOutputStreamInfo( 
               DWORD dwOutputStreamIndex,
               DWORD *pdwFlags)
{
    if ( NULL == pdwFlags )
    {
        return E_POINTER;
    }

    // The stream index must be zero.
    if ( 0 != dwOutputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    // Use the default output stream configuration (a single stream).
    *pdwFlags = 0;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputType
//
// Implementation of IMediaObject::GetInputType
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetInputType ( 
               DWORD dwInputStreamIndex,
               DWORD dwTypeIndex,
               DMO_MEDIA_TYPE *pmt)
{
    HRESULT hr = S_OK;

    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX ;
    }

    // only support one preferred type
    if ( 0 != dwTypeIndex )
    {
        return DMO_E_NO_MORE_ITEMS;
    }

    if ( NULL == pmt )
    {
       return E_POINTER;
    
    }

    // if output type has been defined, use that as input type
    if (GUID_NULL != m_mtOutput.majortype)
    {
        hr = ::MoCopyMediaType( pmt, &m_mtOutput );
    }
    else // otherwise use default for this plug-in
    {
        ::ZeroMemory( pmt, sizeof( DMO_MEDIA_TYPE ) );
        pmt->majortype = MEDIATYPE_Audio;
        pmt->subtype = MEDIASUBTYPE_PCM;
    }

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetOutputType
//
// Implementation of IMediaObject::GetOutputType
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetOutputType( 
               DWORD dwOutputStreamIndex,
               DWORD dwTypeIndex,
               DMO_MEDIA_TYPE *pmt)
{
    HRESULT hr = S_OK;

    if ( 0 != dwOutputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    // only support one preferred type
    if ( 0 != dwTypeIndex )
    {
        return DMO_E_NO_MORE_ITEMS;
    
    }

    if ( NULL == pmt )
    {
       return E_POINTER;
    
    }

    // if input type has been defined, use that as output type
    if (GUID_NULL != m_mtInput.majortype)
    {
        hr = ::MoCopyMediaType( pmt, &m_mtInput );
    }
    else // other use default for this plug-in
    {
        ::ZeroMemory( pmt, sizeof( DMO_MEDIA_TYPE ) );
        pmt->majortype = MEDIATYPE_Audio;
        pmt->subtype = MEDIASUBTYPE_PCM;
    }

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::SetInputType
//
// Implementation of IMediaObject::SetInputType
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::SetInputType( 
               DWORD dwInputStreamIndex,
               const DMO_MEDIA_TYPE *pmt,
               DWORD dwFlags)
{
    HRESULT hr = S_OK;

    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( DMO_SET_TYPEF_CLEAR & dwFlags ) 
    {
        ::MoFreeMediaType(&m_mtInput);
        ::ZeroMemory(&m_mtInput, sizeof(m_mtInput));

        return S_OK;
    }

    if ( NULL == pmt )
    {
       return E_POINTER;
    }

    // validate that the input media type matches our requirements and
    // and matches our output type (if set)
    hr = ValidateMediaType(pmt, &m_mtOutput);

    if (FAILED(hr))
    {
        if( DMO_SET_TYPEF_TEST_ONLY & dwFlags )
        {
            hr = S_FALSE;
        }
    }
    else if ( 0 == dwFlags )
    {
        // free existing media type
        ::MoFreeMediaType(&m_mtInput);
        ::ZeroMemory(&m_mtInput, sizeof(m_mtInput));

        // copy new media type
        hr = ::MoCopyMediaType( &m_mtInput, pmt );
    }

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::SetOutputType
//
// Implementation of IMediaObject::SetOutputType
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::SetOutputType( 
               DWORD dwOutputStreamIndex,
               const DMO_MEDIA_TYPE *pmt,
               DWORD dwFlags)
{ 
    HRESULT hr = S_OK;

    if ( 0 != dwOutputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if( DMO_SET_TYPEF_CLEAR & dwFlags )
    {
        ::MoFreeMediaType( &m_mtOutput );
        ::ZeroMemory(&m_mtOutput, sizeof(m_mtOutput));

        return S_OK;
    }

    if ( NULL == pmt )
    {
        return E_POINTER;
    }

    WAVEFORMATEX *pWave = (WAVEFORMATEX *) pmt->pbFormat;

    // validate that the output media type matches our requirements and
    // and matches our input type (if set)
    hr = ValidateMediaType(pmt, &m_mtInput);

    if (FAILED(hr))
    {
        if( DMO_SET_TYPEF_TEST_ONLY & dwFlags )
        {
            hr = S_FALSE;
        }
    }
    else if ( 0 == dwFlags )
    {
        // free existing media type
        ::MoFreeMediaType(&m_mtOutput);
        ::ZeroMemory(&m_mtOutput, sizeof(m_mtOutput));

        // copy new media type
        hr = ::MoCopyMediaType( &m_mtOutput, pmt );
    }

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputCurrentType
//
// Implementation of IMediaObject::GetInputCurrentType
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::GetInputCurrentType( 
               DWORD dwInputStreamIndex,
               DMO_MEDIA_TYPE *pmt)
{
    HRESULT hr = S_OK;

    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pmt )
    {
        return E_POINTER;
    }

    if (GUID_NULL == m_mtInput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    hr = ::MoCopyMediaType( pmt, &m_mtInput );

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetOutputCurrentType
//
// Implementation of IMediaObject::GetOutputCurrentType
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetOutputCurrentType( 
               DWORD dwOutputStreamIndex,
               DMO_MEDIA_TYPE *pmt)
{
    HRESULT hr = S_OK;

    if ( 0 != dwOutputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pmt )
    {
        return E_POINTER;
    }

    if (GUID_NULL == m_mtOutput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    hr = ::MoCopyMediaType( pmt, &m_mtOutput );

    return hr;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputSizeInfo
//
// Implementation of IMediaObject::GetInputSizeInfo
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetInputSizeInfo( 
               DWORD dwInputStreamIndex,
               DWORD *pcbSize,
               DWORD *pcbMaxLookahead,
               DWORD *pcbAlignment)
{
    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pcbSize )
    {
       return E_POINTER;
    }

    if ( NULL == pcbMaxLookahead )
    {
        return E_POINTER;
    }

    if ( NULL == pcbAlignment )
    {
       return E_POINTER;
    }

    if (GUID_NULL == m_mtInput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    // Return the input sample size, in bytes.
    *pcbSize = m_mtInput.lSampleSize;

    // This plug-in doesn't perform lookahead. Return zero.
    *pcbMaxLookahead = 0;

    // Get the pointer to the input format structure.
    WAVEFORMATEX *pWave = ( WAVEFORMATEX * ) m_mtInput.pbFormat;

    // Return the input buffer alignment, in bytes.
    *pcbAlignment = pWave->nBlockAlign;
  
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetOutputSizeInfo
//
// Implementation of IMediaObject::GetOutputSizeInfo
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetOutputSizeInfo( 
               DWORD dwOutputStreamIndex,
               DWORD *pcbSize,
               DWORD *pcbAlignment)
{
    if ( 0 != dwOutputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pcbSize )
    {
        return E_POINTER;
    }

    if ( NULL == pcbAlignment )
    {
        return E_POINTER;
    }

    if (GUID_NULL == m_mtOutput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    // Return the output sample size, in bytes.
    *pcbSize = m_mtOutput.lSampleSize;

    // Get the pointer to the output format structure.
    WAVEFORMATEX *pWave = ( WAVEFORMATEX * ) m_mtOutput.pbFormat;

    // Return the output buffer aligment, in bytes.
    *pcbAlignment = pWave->nBlockAlign;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputMaxLatency
//
// Implementation of IMediaObject::GetInputMaxLatency
/////////////////////////////////////////////////////////////////////////////
   
STDMETHODIMP CASIOWmpPlg::GetInputMaxLatency( 
               DWORD dwInputStreamIndex,
               REFERENCE_TIME *prtMaxLatency)
{
    return E_NOTIMPL; // Not dealing with latency in this plug-in.
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::SetInputMaxLatency
//
// Implementation of IMediaObject::SetInputMaxLatency
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::SetInputMaxLatency( 
               DWORD dwInputStreamIndex,
               REFERENCE_TIME rtMaxLatency)
{
    return E_NOTIMPL; // Not dealing with latency in this plug-in.
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::Flush
//
// Implementation of IMediaObject::Flush
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::Flush( void )
{
    
    m_spInputBuffer = NULL;  // release smart pointer
    m_cbInputLength = 0;
    m_pbInputData = NULL;
    m_bValidTime = false;
    m_rtTimestamp = 0;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::Discontinuity
//
// Implementation of IMediaObject::Discontinuity
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::Discontinuity( 
               DWORD dwInputStreamIndex)
{
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::AllocateStreamingResources
//
// Implementation of IMediaObject::AllocateStreamingResources
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::AllocateStreamingResources ( void )
{
    // Allocate any buffers need to process the stream. This plug-in does
    // all processing in-place, so it requires no extra buffers.

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::FreeStreamingResources
//
// Implementation of IMediaObject::FreeStreamingResources
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::FreeStreamingResources( void )
{
    m_spInputBuffer = NULL; // release smart pointer
    m_cbInputLength = 0;
    m_pbInputData = NULL;
    m_bValidTime = false;
    m_rtTimestamp = 0;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetInputStatus
//
// Implementation of IMediaObject::GetInputStatus
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::GetInputStatus( 
           DWORD dwInputStreamIndex,
           DWORD *pdwFlags)
{ 
    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pdwFlags )
    {
        return E_POINTER;
    }

    if ( m_spInputBuffer )
    {
        *pdwFlags = 0; //The buffer still contains data; return zero.
    }
    else
    {
        *pdwFlags = DMO_INPUT_STATUSF_ACCEPT_DATA; // OK to call ProcessInput.
    }

    return S_OK;
}



/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::ProcessInput
//
// Implementation of IMediaObject::ProcessInput
/////////////////////////////////////////////////////////////////////////////
    
STDMETHODIMP CASIOWmpPlg::ProcessInput( 
               DWORD dwInputStreamIndex,
               IMediaBuffer *pBuffer,
               DWORD dwFlags,
               REFERENCE_TIME rtTimestamp,
               REFERENCE_TIME rtTimelength)
{ 
    HRESULT hr = S_OK;

    if ( 0 != dwInputStreamIndex )
    {
        return DMO_E_INVALIDSTREAMINDEX;
    }

    if ( NULL == pBuffer )
    {
        return E_POINTER;
    }

    if (GUID_NULL == m_mtInput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    // Get a pointer to the actual data and length information.
    BYTE    *pbInputData = NULL;
    DWORD   cbInputLength = 0;
    hr = pBuffer->GetBufferAndLength(&pbInputData, &cbInputLength);
    if (FAILED(hr))
    {
        return hr;
    }

    // Hold on to the buffer using a smart pointer.
    m_spInputBuffer = pBuffer;
    m_pbInputData = pbInputData;
    m_cbInputLength = cbInputLength;

    //Verify that buffer's time stamp is valid.
    if (dwFlags & DMO_INPUT_DATA_BUFFERF_TIME)
    {
        m_bValidTime = true;
        m_rtTimestamp = rtTimestamp;
    }
    else
    {
        m_bValidTime = false;
    }

    return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::ProcessOutput
//
// Implementation of IMediaObject::ProcessOutput
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::ProcessOutput( 
               DWORD dwFlags,
               DWORD cOutputBufferCount,
               DMO_OUTPUT_DATA_BUFFER *pOutputBuffers,
               DWORD *pdwStatus)
{
    HRESULT hr = S_OK;

    if ( NULL == pOutputBuffers )
    {
        return E_POINTER;
    }

    // this plug-in only supports one output buffer
    if (1 != cOutputBufferCount)
    {
        return E_INVALIDARG;
    }

    if (GUID_NULL == m_mtOutput.majortype)
    {
        return DMO_E_TYPE_NOT_SET;
    }

    if (pdwStatus)
    {
        *pdwStatus = 0;
    }

    // make sure input and output buffer exist
    IMediaBuffer *pOutputBuffer = pOutputBuffers[0].pBuffer;

    if ((!m_spInputBuffer) || (!pOutputBuffer))
    {
        if (pOutputBuffer)
        {
            pOutputBuffer->SetLength(0);
        }

        pOutputBuffers[0].dwStatus = 0;

        return S_FALSE;
    }
    
    BYTE         *pbOutputData = NULL;
    DWORD        cbOutputMaxLength = 0;
    DWORD        cbBytesProcessed = 0;

    // Get current length of output buffer
    hr = pOutputBuffer->GetBufferAndLength(&pbOutputData, &cbOutputMaxLength);
    if (FAILED(hr))
    {
        return hr;
    }

    // Get max length of output buffer
    hr = pOutputBuffer->GetMaxLength(&cbOutputMaxLength);
    if (FAILED(hr))
    {
        return hr;
    }
    //put some nice silence there
    ::ZeroMemory(pbOutputData,cbOutputMaxLength);

    // Calculate how many bytes we can process
    bool bComplete = false; // The entire buffer is not yet processed.

    if (m_cbInputLength > cbOutputMaxLength)
    {
        cbBytesProcessed = cbOutputMaxLength; // only process as much of the input as can fit in the output
    }
    else
    {
        cbBytesProcessed = m_cbInputLength; // process entire input buffer
        bComplete = true;                   // the entire input buffer has been processed. 
    }

    // Call the internal processing method, which returns the no. bytes processed
    hr = DoProcessOutput(pbOutputData, m_pbInputData, &cbBytesProcessed);
    if (FAILED(hr))
    {
        return hr;
    }

    // Set the size of the valid data in the output buffer.
    hr = pOutputBuffer->SetLength(cbBytesProcessed);
    if (FAILED(hr))
    {
        return hr;
    }
    // Update the DMO_OUTPUT_DATA_BUFFER information for the output buffer.
    pOutputBuffers[0].dwStatus = 0;

    if (m_bValidTime)
    {
        // store start time of output buffer
        pOutputBuffers[0].dwStatus |= DMO_OUTPUT_DATA_BUFFERF_TIME;
        pOutputBuffers[0].rtTimestamp = m_rtTimestamp;
    
        // Get the pointer to the output format structure.
        WAVEFORMATEX *pWave = ( WAVEFORMATEX * ) m_mtOutput.pbFormat;

        // estimate time length of output buffer
        pOutputBuffers[0].dwStatus |= DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH;
        pOutputBuffers[0].rtTimelength = ::MulDiv(cbBytesProcessed, UNITS, pWave->nAvgBytesPerSec);

        // this much time has been consumed, so move the time stamp accordingly
        m_rtTimestamp += pOutputBuffers[0].rtTimelength;
    }

    if (bComplete) 
    {
        m_spInputBuffer = NULL;   // Release smart pointer
        m_cbInputLength = 0;
        m_pbInputData = NULL;
        m_bValidTime = false;
        m_rtTimestamp = 0;
    }
    else 
    {
        // Let the client know there is still data that needs processing 
        // in the input buffer.
        pOutputBuffers[0].dwStatus |= DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE;
        m_pbInputData += cbBytesProcessed;
        m_cbInputLength -= cbBytesProcessed;
    }
 
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CWMPDSPPlugin::DoProcessOutput
//
// Convert the input buffer to the output buffer
/////////////////////////////////////////////////////////////////////////////

HRESULT CASIOWmpPlg::DoProcessOutput(
    BYTE *pbOutputData,const BYTE *pbInputData, DWORD *cbBytesProcessed)
{
    WAVEFORMATEX *pWave = ( WAVEFORMATEX * ) m_mtInput.pbFormat;

    DWORD dwSamplesToProcess = (*cbBytesProcessed / pWave->nBlockAlign) * pWave->nChannels;
    *cbBytesProcessed = dwSamplesToProcess * pWave->wBitsPerSample /8;
    if (m_bEnabled)
        SendDataToASIO(dwSamplesToProcess);
    return S_OK;
}





/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::Lock
//
// Implementation of IMediaObject::Lock
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::Lock( LONG bLock )
{
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::SetEnable
//
// Implementation of IWMPPluginEnable::SetEnable
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::SetEnable( BOOL fEnable )
{
    // This function is called when the plug-in is being enabled or disabled,
    // typically by user action. Once a plug-in is disabled, it will still be
    // loaded into the graph but ProcessInput and ProcessOutput will not be called.

    // This function allows any state or UI associated with the plug-in to reflect the
    // enabled/disable state of the plug-in

    m_bEnabled = fEnable;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetEnable
//
// Implementation of IWMPPluginEnable::GetEnable
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::GetEnable( BOOL *pfEnable )
{
    if ( NULL == pfEnable )
    {
        return E_POINTER;
    }

    *pfEnable = m_bEnabled;

    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::GetPages
//
// Implementation of ISpecifyPropertyPages::GetPages
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CASIOWmpPlg::GetPages(CAUUID *pPages)
{
    // Only one property page is required for the plug-in.
    pPages->cElems = 1;
    pPages->pElems = (GUID *) (CoTaskMemAlloc(sizeof(GUID)));

    // Make sure memory is allocated for pPages->pElems
    if (NULL == pPages->pElems)
    {
        return E_OUTOFMEMORY;
    }

    // Return the property page's class ID
    *(pPages->pElems) = CLSID_ASIOWmpPlgPropPage;

    return S_OK;
}

/*        IWMPPlugin methods               */

HRESULT CASIOWmpPlg::Init(DWORD_PTR dwPlaybackContext) 
{
	return S_OK;
}

HRESULT CASIOWmpPlg::Shutdown( void) 
{
	return S_OK;
}

HRESULT CASIOWmpPlg::GetID( GUID *pGUID) 
{
	if (pGUID)
		*pGUID = CLSID_ASIOWmpPlg;
	return S_OK;
}

HRESULT CASIOWmpPlg::GetCaps( DWORD *pdwFlags) 
{
	if (pdwFlags)
		*pdwFlags = 0; //we can convert formats
	return S_OK;
}

HRESULT CASIOWmpPlg::AdviseWMPServices( IWMPServices *pWMPServices) 
{
	m_pWMPServices = pWMPServices;
	g_player.maybeAdviseWMPServices(m_pWMPServices);
	return S_OK;
}

HRESULT CASIOWmpPlg::UnAdviseWMPServices( void)
{
	g_player.maybeUnadviseWMPServices(m_pWMPServices);
	m_pWMPServices = NULL;
	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::SendDataToASIO
//
// Takes the data from the input buffer and queues it for ASIO streaming
/////////////////////////////////////////////////////////////////////////////

HRESULT CASIOWmpPlg::SendDataToASIO(DWORD dwSamplesToProcess)
{
    WAVEFORMATEX *pWave = ( WAVEFORMATEX * ) m_mtInput.pbFormat;
	if (g_player.maybeInitOrReset(pWave->nChannels,pWave->nSamplesPerSec) != ASE_OK)
    {
        m_bEnabled=false;
        return E_FAIL;
    }

    const BYTE *pbInputData=m_pbInputData;
    ASIOSampleType type = ASIOSTLastEntry;

    //calculate the type from the pWave structure
    switch (pWave->wFormatTag)
    {
    case WAVE_FORMAT_IEEE_FLOAT: 
        type = ASIOSTFloat32LSB; 
        break;
    case WAVE_FORMAT_PCM:        
        switch (pWave->wBitsPerSample)
        {
        case 16: 
            type = ASIOSTInt16LSB; 
            break;
        case 24: 
            type = ASIOSTInt24LSB; 
            break;
        case 32: 
            type = ASIOSTInt32LSB; 
            break;
        }
        break;
    case WAVE_FORMAT_EXTENSIBLE:
        {
            WAVEFORMATEXTENSIBLE *pWaveXT = (WAVEFORMATEXTENSIBLE *) pWave;

            if (pWaveXT->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) 
                type = ASIOSTFloat32LSB;
            else if (pWaveXT->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) 
            {
                if (pWave->wBitsPerSample == 32)
                    switch (pWaveXT->Samples.wValidBitsPerSample)
                    {
                    case 16: 
                        type=ASIOSTInt32LSB16; 
                        break;
                    case 20: 
                        type=ASIOSTInt32LSB20; 
                        break;
                    case 24: 
                        type=ASIOSTInt32LSB24; 
                        break;
                    case 32: 
                        type=ASIOSTInt32LSB;   
                        break;
                    }
                else if (pWave->wBitsPerSample == pWaveXT->Samples.wValidBitsPerSample)
                    switch (pWave->wBitsPerSample)
                    {
                    case 16: 
                        type = ASIOSTInt16LSB; 
                        break;
                    case 24: 
                        type = ASIOSTInt24LSB; 
                        break;
                    }
            }
        }
    }

    if (type!=ASIOSTLastEntry)
    {
        g_player.queueForPlayback(type,dwSamplesToProcess/pWave->nChannels,(LPVOID)pbInputData);
        return S_OK;
    }
    else 
        return E_FAIL;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlg::ValidateMediaType
//
// Validate that the media type is acceptable
/////////////////////////////////////////////////////////////////////////////

HRESULT CASIOWmpPlg::ValidateMediaType(const DMO_MEDIA_TYPE *pmtTarget, const DMO_MEDIA_TYPE *pmtPartner)
{
    // make sure the target media type has the fields we require
    if( ( MEDIATYPE_Audio != pmtTarget->majortype ) || 
        ( FORMAT_WaveFormatEx != pmtTarget->formattype ) ||
        ( pmtTarget->cbFormat < sizeof( WAVEFORMATEX )) ||
        ( NULL == pmtTarget->pbFormat) )
    {
        return DMO_E_TYPE_NOT_ACCEPTED;
    }

    // make sure the wave header has the fields we require
    WAVEFORMATEX *pWave = (WAVEFORMATEX *) pmtTarget->pbFormat;

    if ((0 == pWave->nChannels) ||
        (0 == pWave->nSamplesPerSec) ||
        (0 == pWave->nAvgBytesPerSec) ||
        (0 == pWave->nBlockAlign) ||
        (0 == pWave->wBitsPerSample))
    {
        return DMO_E_TYPE_NOT_ACCEPTED;
    }

    // make sure this is a supported container size
    if ((8  != pWave->wBitsPerSample) &&
        (16 != pWave->wBitsPerSample) &&
        (24 != pWave->wBitsPerSample) &&
        (32 != pWave->wBitsPerSample))
    {
        return DMO_E_TYPE_NOT_ACCEPTED;
    }

    // make sure the wave format is acceptable
    switch (pWave->wFormatTag)
    {
    case WAVE_FORMAT_PCM:

        if ((8  != pWave->wBitsPerSample) &&
            (16 != pWave->wBitsPerSample) &&
            (24 != pWave->wBitsPerSample) &&
            (32 != pWave->wBitsPerSample) )
        {
            return DMO_E_TYPE_NOT_ACCEPTED;
        }
        break;

    case WAVE_FORMAT_IEEE_FLOAT:

        // make sure the input is sane
        if (32 != pWave->wBitsPerSample)
        {
            return DMO_E_TYPE_NOT_ACCEPTED;
        }
        break;

    case WAVE_FORMAT_EXTENSIBLE:
        {
            WAVEFORMATEXTENSIBLE *pWaveXT = (WAVEFORMATEXTENSIBLE *) pWave;

            // make sure the wave format extensible has the fields we require
            if ((KSDATAFORMAT_SUBTYPE_PCM != pWaveXT->SubFormat &&
                 KSDATAFORMAT_SUBTYPE_IEEE_FLOAT != pWaveXT->SubFormat) ||
                (0 == pWaveXT->Samples.wSamplesPerBlock) ||
                (pWaveXT->Samples.wValidBitsPerSample > pWave->wBitsPerSample))
            {
                return DMO_E_TYPE_NOT_ACCEPTED;
            }

            // for 8 or 16-bit, the container and sample size must match
            if ((8  == pWave->wBitsPerSample) ||
                (16 == pWave->wBitsPerSample))
            {
                if (pWave->wBitsPerSample != pWaveXT->Samples.wValidBitsPerSample)
                {
                    return DMO_E_TYPE_NOT_ACCEPTED;
                }
            }
            else 
            {
                // for any other container size, make sure the valid
                // bits per sample is a value we support
                if ((16 != pWaveXT->Samples.wValidBitsPerSample) &&
                    (20 != pWaveXT->Samples.wValidBitsPerSample) &&
                    (24 != pWaveXT->Samples.wValidBitsPerSample) &&
                    (32 != pWaveXT->Samples.wValidBitsPerSample))
                {
                    return DMO_E_TYPE_NOT_ACCEPTED;
                }
            }
        }
        break;

    default:
        return DMO_E_TYPE_NOT_ACCEPTED;
        break;
    }

    // if the partner media type is configured, make sure it matches the target.
    // this is done because this plug-in requires the same input and output types
    if (GUID_NULL != pmtPartner->majortype && 0)
    {
        if ((pmtTarget->majortype != pmtPartner->majortype) ||
            (pmtTarget->subtype   != pmtPartner->subtype))
        {
            return DMO_E_TYPE_NOT_ACCEPTED;
        }

        // make sure the wave headers for the target and the partner match
        WAVEFORMATEX *pPartnerWave = (WAVEFORMATEX *) pmtPartner->pbFormat;

        if ((pWave->nChannels != pPartnerWave->nChannels) ||
            (pWave->nSamplesPerSec != pPartnerWave->nSamplesPerSec) ||
            (pWave->nAvgBytesPerSec != pPartnerWave->nAvgBytesPerSec) ||
            (pWave->nBlockAlign != pPartnerWave->nBlockAlign) ||
            (pWave->wBitsPerSample != pPartnerWave->wBitsPerSample) ||
            (pWave->wFormatTag != pPartnerWave->wFormatTag))
        {
            return DMO_E_TYPE_NOT_ACCEPTED;
        }
       
        // make sure the waveformatextensible types are the same
        if (pWave->wFormatTag == WAVE_FORMAT_EXTENSIBLE) 
        {
            WAVEFORMATEXTENSIBLE *pWaveXT = (WAVEFORMATEXTENSIBLE *) pWave;
            WAVEFORMATEXTENSIBLE *pPartnerWaveXT = (WAVEFORMATEXTENSIBLE *) pPartnerWave;
            if (pWaveXT->SubFormat != pPartnerWaveXT->SubFormat)
            {
                return DMO_E_TYPE_NOT_ACCEPTED;
            }
        }
    }

    // media type is valid
    return S_OK;
}

HRESULT CASIOWmpPlg::getCLSID(CLSID *pVal)
{
    if ( NULL == pVal )
        return E_POINTER;

    *pVal = g_player.getCLSID();

    return S_OK;
}

HRESULT CASIOWmpPlg::putCLSID(CLSID newVal)
{
   g_player.putCLSID(newVal);
    return S_OK;
}

