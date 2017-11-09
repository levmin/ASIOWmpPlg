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
#include "StdAfx.h"
#include <asiosys.h>
#include <asio.h>
#include "ASIOPlayer.h"
#include <limits.h>
#include <math.h>
#include "ASIOWmpPlg.h"
#include "SampleConverter.h"
#include <avrt.h>
//#define FILE_DUMP
#ifdef FILE_DUMP
#include "C:\Program Files\Mega-Nerd\libsndfile\include\sndfile.h"
#endif

CriticalSection ASIOSession::Locker::m_section;

//A derivation of SampleDataType to handle ASIO data types
struct ASIOTypeData : public SampleTypeData
{
public:
   ASIOTypeData(ASIOSampleType type){setType(type); }
   void setType(ASIOSampleType type)
   {
      bWrongType=false;
      switch (type)
      {
         case ASIOSTInt16LSB   : container_width=16; sample_width=16; isFloat=false; break;
         case ASIOSTInt24LSB   : container_width=24; sample_width=24; isFloat=false; break;
         case ASIOSTInt32LSB   : container_width=32; sample_width=32; isFloat=false; break;
         case ASIOSTFloat32LSB : container_width=32; sample_width=32; isFloat=true;  break;
         case ASIOSTFloat64LSB : container_width=64; sample_width=64; isFloat=true;  break;
         case ASIOSTInt32LSB16 : container_width=32; sample_width=16; isFloat=false; break;
         case ASIOSTInt32LSB18 : container_width=32; sample_width=18; isFloat=false; break;
         case ASIOSTInt32LSB20 : container_width=32; sample_width=20; isFloat=false; break;
         case ASIOSTInt32LSB24 : container_width=32; sample_width=24; isFloat=false; break;
         default:                container_width=0;  sample_width=0 ; isFloat=false; bWrongType=true;
      }
   }
};

//A derivation of CSampleConverter to handle ASIO data types
class CASIOSampleConverter : public CSampleConverter
{
   ASIOSampleType m_inType;
   ASIOSampleType m_outType;
public:
   CASIOSampleConverter(ASIOSampleType inType, ASIOSampleType outType)
   {
      setInputType(inType);
      setOutputType(outType);
   }

   void setInputType(ASIOSampleType type)    
   {
      if (type!=m_inType)
      {   
         m_inType=type; 
         static_cast<ASIOTypeData *>(&m_inData)->setType(type);
      }
   }
   void setOutputType(ASIOSampleType type)   
   {
      if (type!=m_outType)
      {
         m_outType=type;
         static_cast<ASIOTypeData *>(&m_outData)->setType(type);
      }
   }
};

HWND findAppWindowHandle()
{
      HWND hWnd=FindWindow("eHome Render Window",NULL);  //try to find Windows Media Center
      if (!IsWindow(hWnd))
      {
        hWnd=FindWindow("WMPlayerApp",NULL);  //try to find Window Media Player
        if (!IsWindow(hWnd))
           hWnd=NULL;
      }
      return hWnd;
}

void postError(const char * text, HRESULT hr)
{
      HWND hWnd = findAppWindowHandle();
      char buf[255];
      strcpy_s(buf,sizeof(buf),text);
      if (hr!=S_OK)
      {
         size_t offset=strlen(buf);
         sprintf_s(buf+offset,sizeof(buf)-offset,"; error code %x",hr);
      }
      MessageBox(hWnd,buf,"ASIOWmpPlg plugin",MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
}

/*  ASIOPlayer members
*/

ASIOPlayer g_player;

namespace 
{
    void bufferSwitch(long index, ASIOBool processNow)  
    {
        g_player.bufferSwitch(index,processNow);
    }

    void sampleRateChanged(ASIOSampleRate sRate)        
    {
        g_player.sampleRateChanged(sRate);
    }

    ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow) 
    {
        return g_player.bufferSwitchTimeInfo(timeInfo,index,processNow);
    }

    long asioMessages(long selector, long value, void* message, double* opt)
    {   
        return g_player.asioMessages(selector,value,message,opt);
    }

    ASIOCallbacks callbacks={&bufferSwitch,&sampleRateChanged,&asioMessages,&bufferSwitchTimeInfo}; 
}

#ifdef FILE_DUMP
SNDFILE * s_dump;
vector <BYTE> s_large_buffer;
unsigned s_large_buffer_index;
const int LARGE_BUFFER_SIZE=100000000;
#endif

ASIOPlayer::ASIOPlayer():m_nBufferSize(0),m_samplesInOverflowBuffers(0),m_bAccumulationMode(true),m_shutdownInitiated(FALSE),
    m_nLowWaterBufferCount(DEFAULT_LOW_WATER_BUFFER_COUNT),m_pWMPServices(NULL)

{
}

ASIOPlayer::~ASIOPlayer()
{
}

int  ASIOPlayer::m_nWMPServicesCounter=0; 

void ASIOPlayer::maybeAdviseWMPServices(IWMPServices * pWMPServices)
{
	Locker l;
	if (!m_pWMPServices || m_pWMPServices == pWMPServices) 
		m_nWMPServicesCounter++;

	if (!m_pWMPServices)
		m_pWMPServices = pWMPServices;
}

void ASIOPlayer::maybeUnadviseWMPServices(IWMPServices * pWMPServices)
{
	Locker l;
	if (m_pWMPServices == pWMPServices)
	{
		m_nWMPServicesCounter--;
		if (m_nWMPServicesCounter == 0)
			m_pWMPServices = NULL;
	}
}

int ASIOPlayer::getWMPState() 
{
	Locker l;
	if (m_pWMPServices)
	{
		WMPServices_StreamState state = WMPServices_StreamState_Stop;
		m_pWMPServices->GetStreamState(&state);
		return state;
	}
	else
		return -1;
}

DWORD WINAPI ASIOPlayer::StartStopPauseDetectionThreadProc(LPVOID pV)
{
    if (!pV)
        return -1;
    ASIOPlayer * pPlayer = (ASIOPlayer*) pV;
    DWORD rc=0;
    int lastState = -1;
	while ((rc=WaitForSingleObject(pPlayer->m_shutdownInitiated.m_hEvent,100))==WAIT_TIMEOUT)
    {
		int newState = pPlayer->getWMPState();
		if (newState != lastState)
		{
			switch(newState)
			{
				case WMPServices_StreamState_Stop: pPlayer->stop();// MessageBox(NULL,"Stop",NULL,MB_OK); OutputDebugString("stop\n"); 
					break;
				case WMPServices_StreamState_Pause: pPlayer->pause(); //MessageBox(NULL,"Pause",NULL,MB_OK);OutputDebugString("pause\n"); 
					break;
				case WMPServices_StreamState_Play: pPlayer->start(); // MessageBox(NULL,"Start",NULL,MB_OK); OutputDebugString("start\n");
					break;
			}
			lastState = newState;
		}
	}
    return 0;
}

ASIOError ASIOPlayer::start()
{
    Locker l;
    ASIOError rc=ASE_OK;
    if (isOpen() && !isRunning())
    {
		bufferSwitchTimeInfo(NULL,1,false);//prefill the B buffer set
        rc = ASIOSession::start();
    }
    return rc;
}

ASIOError ASIOPlayer::pause()
{
    Locker l;
    ASIOError rc=ASE_OK;
    if (isOpen() && isRunning())
    {
        rc = ASIOSession::stop();
    }
    return rc;
}

ASIOError ASIOPlayer::stop()
{//we need to pause the ASIOSession and initialize the overflow buffers
    Locker l;
    ASIOError rc=pause();
    for (size_t i=0;i<m_infos.size();i++)
    {
        ASIOTypeData data=m_types[i];
        const int cnBufferByteLength=m_nBufferSize * data.container_width / 8;
		//clear both A and B buffers
        BYTE * pBufferStart=(BYTE *)m_infos[i].buffers[0];
        if (pBufferStart) 
            memset(pBufferStart,0,cnBufferByteLength);
        pBufferStart=(BYTE *)m_infos[i].buffers[1];
        if (pBufferStart) 
            memset(pBufferStart,0,cnBufferByteLength);
    }
    m_overflowBuffers.clear();    
    m_overflowBuffers.resize(m_infos.size());
    m_samplesInOverflowBuffers=0;
    m_bAccumulationMode=true;
#ifdef FILE_DUMP
   if (s_dump) {
      sf_count_t rc = sf_write_raw(s_dump,&s_large_buffer[0],s_large_buffer_index);
      if (rc != s_large_buffer_index)
         MessageBox(NULL,"Error writing large buffer",NULL,MB_OK);
      sf_close(s_dump);
      s_dump = NULL;
      s_large_buffer.clear();
      s_large_buffer_index = 0;
   }
#endif
    return rc;
}

ASIOError ASIOPlayer::maybeInitOrReset(int nChannels, int nSampleRate)
{
    Locker l;
	if (isOpen() && m_infos.size() == nChannels)
    {//we are open with the same number of channels, let's check the sample rate
        ASIOSampleRate rate = 0;
        getSampleRate(&rate);
        if (rate == nSampleRate) //same sample rate, no need to call initOrReset
            return ASE_OK;
    }
    return initOrReset(nChannels,nSampleRate);
}

ASIOError ASIOPlayer::initOrReset(int nChannels, int nSampleRate)
{
   Locker l;
   if (isOpen())
      shutdown();
   ResetEvent(m_shutdownInitiated.m_hEvent);
    
   ASIOError rc = open();
   if (rc!=ASE_OK || !m_pIASIO)
       return ASE_NotPresent;
   long nInputChannels=0, nOutputChannels=0;
   rc= m_pIASIO->getChannels(&nInputChannels,&nOutputChannels);
   if (rc!=ASE_OK)
      return ASE_NotPresent;

   //we cannot have more output channels than hardware supports
   nOutputChannels = min(nOutputChannels,nChannels);

   m_infos.resize(nOutputChannels);

   for (int i=0;i<nOutputChannels;i++)
   {
      m_infos[i].isInput=ASIOFalse;
      m_infos[i].channelNum=i;
      m_infos[i].buffers[0]=m_infos[i].buffers[1]=NULL;
   }
   //retrieve recommended ASIO buffer size
   long dummy;
   rc=m_pIASIO->getBufferSize(&dummy,&dummy,&m_nBufferSize,&dummy);
   if (rc!=ASE_OK)
   {
      m_nBufferSize=0;
      return ASE_NotPresent;
   }
   rc=m_pIASIO->createBuffers(&m_infos[0],(long)m_infos.size(),m_nBufferSize,&callbacks);
   if (rc!=ASE_OK)
   {
     m_infos.clear();
     postError("ASIOWmpPlg: Failed to create ASIO buffers",rc);
     return ASE_NotPresent;
   }

   ASIOSampleRate currentRate = 0;
   if (getSampleRate(&currentRate)==ASE_OK && currentRate!=nSampleRate)
   {
       if (g_player.canSampleRate(nSampleRate)!=ASE_OK)
       {//the sample rate is not supported for nOutputChannels
           char buf[255];
           sprintf_s(buf,sizeof(buf),"Sample rate %d is not supported",nSampleRate);
           postError(buf);
           shutdown();
           return ASE_NotPresent;
       }
       setSampleRate(nSampleRate);
       //Many ASIO drivers require reset when the sample rate changes.
	   //We do not handle reset requests passed through a standard ASIO callback mechanism.
	   //Therefore, let's preemptively close and reopen the ASIO session
	   if (close()!=ASE_OK || open()!=ASE_OK)
             return ASE_NotPresent; 
       long dummy;
       rc=m_pIASIO->getBufferSize(&dummy,&dummy,&m_nBufferSize,&dummy);
       if (rc!=ASE_OK)
       {
          m_nBufferSize=0;
          return ASE_NotPresent;
       }
       rc=m_pIASIO->createBuffers(&m_infos[0],(long)m_infos.size(),m_nBufferSize,&callbacks);
       if (rc!=ASE_OK)
       {
         m_infos.clear();
         postError("ASIOWmpPlg: Failed to create ASIO buffers",rc);
         return ASE_NotPresent;
       }
   }

   //we want m_nLowWaterBufferCount to ensure at least 100ms worth of buffers
   if (m_nBufferSize == 0)
       return ASE_NotPresent;
   m_nLowWaterBufferCount = max(m_nLowWaterBufferCount, (int)ceil(nSampleRate*(double)0.1/(double)m_nBufferSize));
   
   for (size_t i=0;i<m_infos.size();i++)
   {
      ASIOChannelInfo info;
      info.channel=(long)i;
      info.isInput=ASIOFalse;
      m_pIASIO->getChannelInfo(&info);
      m_types.push_back(info.type);
      ASIOTypeData data(info.type);
      const int cnBufferByteLength=m_nBufferSize * data.container_width / 8;
	   //clear both A and B buffers
	   BYTE * pBufferStart=(BYTE *)m_infos[i].buffers[0];
	   if (pBufferStart) 
		memset(pBufferStart,0,cnBufferByteLength);
	   pBufferStart=(BYTE *)m_infos[i].buffers[1];
	   if (pBufferStart) 
		   memset(pBufferStart,0,cnBufferByteLength);
   }

   m_overflowBuffers.resize(m_infos.size());
   m_pIASIO->future(kAsioDisableTimeCodeRead,NULL);
   CreateThread(NULL,0,StartStopPauseDetectionThreadProc,this,0,NULL);

   return ASE_OK;
}

ASIOError ASIOPlayer::shutdown()
{
   Locker l;
   //signal the event waiting thread to exit
   SetEvent(m_shutdownInitiated.m_hEvent);
   Sleep(0);
   ResetEvent(m_shutdownInitiated.m_hEvent);
   stop();
   close();
   m_infos.clear();
   m_types.clear();
   m_overflowBuffers.clear();
   m_nLowWaterBufferCount = DEFAULT_LOW_WATER_BUFFER_COUNT;
   return ASE_OK;
}

ASIOTime *ASIOPlayer::bufferSwitchTimeInfo(ASIOTime *, long index, ASIOBool )
{	
      Locker l;
      //check that we are still running
      if (!isOpen())
         return NULL;
      //possibly raise thread's priority
      m_raiser.raise(true);
#ifdef FILE_DUMP
      if (!s_dump && m_infos.size())
      {
         SF_INFO info = {0}; 
         info.channels = m_infos.size();
         ASIOTypeData data(m_types[0]);
         info.format = SF_FORMAT_WAV|(data.container_width/8);
         ASIOSampleRate rate = 88200;
         getSampleRate(&rate);
         info.samplerate = (int)rate;
         s_dump = sf_open("C:\\temp\\dump.wav",SFM_WRITE,&info);
         if (!s_dump)
         {
            MessageBox(NULL,sf_strerror(NULL),NULL,MB_OK);
         }
         s_large_buffer.resize(LARGE_BUFFER_SIZE);
      }
#endif
      //clear the index buffers
      for (unsigned i=0;i<m_infos.size();i++)
      {    
         COverflowBuffer& buf=m_overflowBuffers[i];
         BYTE * pBufferStart=(BYTE *)m_infos[i].buffers[index];
         memset(pBufferStart,0,m_nBufferSize*buf.m_nContainerSize);
      }
      //every overflow buffer has the same number of samples equal to
      //the number of available frames
      long lFramesAvailable=m_samplesInOverflowBuffers;
      long lFramesToWrite=0;
      /* When the ASIOPlayer is initialized, the accumulation mode is set on. 
         In this mode, we wait until the number of available frames gets equal or exceeds
         m_nLowWaterBufferCount ASIO frame buffers. After that, the accumulation mode
         is turned off and stays off until the shutdown() call.
      */
      if (!m_bAccumulationMode || (lFramesAvailable >= m_nLowWaterBufferCount*m_nBufferSize))
        lFramesToWrite=min(m_nBufferSize,lFramesAvailable);
      if  (lFramesToWrite>0)
      {
         m_bAccumulationMode=false;
         for (unsigned i=0;i<m_infos.size();i++)
         {    
            COverflowBuffer& buf=m_overflowBuffers[i];
            long totalBytesToWrite=min(buf.m_nValidBytes,lFramesToWrite*buf.m_nContainerSize);
            BYTE * pASIOBuf = (BYTE *)m_infos[i].buffers[index];
            //write the first chunk, taking the data from buf.m_nStartPos
            long bytesToWrite1=min(totalBytesToWrite,(long)buf.m_buffer.size()-buf.m_nStartPos);
            if (bytesToWrite1 > 0) {
                memcpy_s(pASIOBuf,bytesToWrite1,&buf.m_buffer[buf.m_nStartPos],bytesToWrite1);
            }
            if (bytesToWrite1<totalBytesToWrite)
            {
                //write the second chunk, taking the data from the beginning of the buf
               long bytesToWrite2=totalBytesToWrite-bytesToWrite1;
               memcpy_s(pASIOBuf+bytesToWrite1,bytesToWrite2,&buf.m_buffer[0],bytesToWrite2);
            }
            //move the m_nStartPos and update the m_nValidBytes
            buf.m_nStartPos=(buf.m_nStartPos+totalBytesToWrite)%buf.m_buffer.size();
            buf.m_nValidBytes-=totalBytesToWrite;
         }
         m_samplesInOverflowBuffers-=lFramesToWrite;
      }
#ifdef FILE_DUMP
      if (m_infos.size())
      {
         COverflowBuffer& buf=m_overflowBuffers[0];
         unsigned frame_size = buf.m_nContainerSize*m_infos.size();
         vector<BYTE> frame(frame_size);
         for (int i=0;i<m_nBufferSize;i++)
         {   
            for (size_t j=0;j<m_infos.size();j++)
            {
               BYTE * pASIOBuf=(BYTE *)m_infos[j].buffers[index];
               memcpy_s(&frame[0]+buf.m_nContainerSize*j,buf.m_nContainerSize,pASIOBuf+i*buf.m_nContainerSize,buf.m_nContainerSize);
            }
            if (s_large_buffer_index + frame_size < s_large_buffer.size())
            {
               memcpy_s(&s_large_buffer[s_large_buffer_index],frame_size,&frame[0],frame_size);
               s_large_buffer_index += frame_size;
            }
         }
      }
#endif
   outputReady();
   return NULL;
}

void ASIOPlayer::bufferSwitch(long index, ASIOBool processNow)
{
	bufferSwitchTimeInfo (NULL, index, processNow);
}


void ASIOPlayer::sampleRateChanged(ASIOSampleRate sRate)
{
    //tough luck, ignore for now
}


long ASIOPlayer::asioMessages(long selector, long value, void*, double*)
{
	long ret = 0;
    if (selector == kAsioSelectorSupported)
        selector  = value;
    switch(selector)
	{
		case kAsioResetRequest:
            //tough luck, ignore for now
			break;
		case kAsioResyncRequest:
			ret = 1;
			break;
		case kAsioLatenciesChanged:
			ret = 1;
			break;
		case kAsioEngineVersion:
			ret = 1;
			break;
		case kAsioSupportsTimeInfo:
			ret = 1;
			break;
	}
	return ret;
}

void ASIOPlayer::queueForPlayback(ASIOSampleType inType,long nFramesToWrite,void * data)
{
    Locker l;
   if (nFramesToWrite==0)
      return;
   static CASIOSampleConverter converter(ASIOSTInt16LSB,ASIOSTInt16LSB);
   if (m_infos.size()==0 || m_types.size()==0 )
      return;
   
   m_raiser2.raise(false);

   converter.setInputType(inType);
   short inContainerSize=converter.getInputContainerWidth()/8;
   size_t nChannels=m_infos.size();
   size_t inFrameSize=inContainerSize*nChannels;
   BYTE * inData=(BYTE *) data;
   if (nFramesToWrite)
   {//write the frames into the overflow circular buffer
      for (unsigned i=0;i<nChannels;i++)
      {    
            converter.setOutputType(m_types[i]);
            short outContainerSize=converter.getOutputContainerWidth()/8;
            COverflowBuffer& buf=m_overflowBuffers[i];
            buf.m_nContainerSize=outContainerSize;
            long lIncreaseNeeded=buf.m_nValidBytes+outContainerSize*nFramesToWrite-(long)buf.m_buffer.size();
            if (lIncreaseNeeded>0)
            {
               //insert zeros at the start position   
               buf.m_buffer.insert(buf.m_buffer.begin()+buf.m_nStartPos,lIncreaseNeeded,0);
               //adjust the start position accordingly
               buf.m_nStartPos+=lIncreaseNeeded;
            }
            BYTE * pCurrentInSample= inData+inContainerSize*i; 
            BYTE * pCurrentOutSample=&buf.m_buffer[buf.getEndPos()];
            /* We need to write the incoming data after the existing valid bytes in the overflow buffer.
               The end of valid bytes is at buf.getEndPos().
            */
            //write the first chunk, until the buffer's end
            long nFramesToWriteUntilTheBufferEnd=min(nFramesToWrite,((long)buf.m_buffer.size()-buf.getEndPos())/outContainerSize);
            for (long j=0;j<nFramesToWriteUntilTheBufferEnd;j++)
            {
               converter.convert(pCurrentInSample,pCurrentOutSample);
               pCurrentInSample+=inFrameSize;
               pCurrentOutSample+=outContainerSize;
            }
            buf.m_nValidBytes+=nFramesToWriteUntilTheBufferEnd*outContainerSize;
            if (nFramesToWrite>nFramesToWriteUntilTheBufferEnd)
            {
               //write the second chunk into the beginning of the buffer
               pCurrentOutSample=&buf.m_buffer[0];
               for (long j=0;j<nFramesToWrite-nFramesToWriteUntilTheBufferEnd;j++)
               {
                  converter.convert(pCurrentInSample,pCurrentOutSample);
                  pCurrentInSample+=inFrameSize;
                  pCurrentOutSample+=outContainerSize;
               }
               buf.m_nValidBytes+=(nFramesToWrite-nFramesToWriteUntilTheBufferEnd)*outContainerSize;
            }
      }
      //adjust the m_samplesInOverflowBuffers
      m_samplesInOverflowBuffers+=nFramesToWrite;
   }
}

int    ASIOPlayer::m_nPluginCounter=0;

void ASIOPlayer::incrementPluginCounter() 
{
    Locker l;
    m_nPluginCounter++;    
}

ASIOError ASIOPlayer::maybeShutdown()
{
    Locker l;
    m_nPluginCounter--;    
    return (m_nPluginCounter==0)?shutdown():ASE_OK;
}

/* ASIOSession members
*/

ASIOSession::ASIOSession() : m_clsid(GUID_NULL),m_pIASIO(NULL), m_stopped(TRUE),
    m_bOpen(FALSE),m_ASIOCreated(FALSE),m_hASIOThread(NULL),m_dwASIOThreadId(0)
{
   Locker l;
   CRegKey key;
   DWORD   dwValue = 0;
   m_clsid=GUID_NULL;
   DWORD lResult = key.Open(HKEY_CURRENT_USER, kszPrefsRegKey, KEY_READ);
   if (ERROR_SUCCESS == lResult)
   {
       key.QueryGUIDValue(kszPrefsCLSID, m_clsid);
       key.Close();
   }
   if (m_clsid==GUID_NULL)
   {//try to find it
      CRegKey asioKey;
      asioKey.Open(HKEY_LOCAL_MACHINE,"SOFTWARE\\ASIO",KEY_READ);
      DWORD length=MAXSTRING;
      char buf[MAXSTRING]={0};
      long rc=asioKey.EnumKey(0,buf,&length);
      if (rc==ERROR_SUCCESS)
      {
         CRegKey asioSubKey;
         asioSubKey.Open(asioKey,buf);
         asioSubKey.QueryGUIDValue("CLSID",m_clsid);
         asioSubKey.Close();
      }
      asioKey.Close();
   }
}

ASIOSession::~ASIOSession()
{
   Locker l;
   close();
}

ASIOError ASIOSession::stop()
{
   Locker l;
   if (isOpen())
   {
      ASIOError rc=ASE_OK;
      if (isRunning())
      {
          ASIOError rc=m_pIASIO->stop();
          SetEvent(m_stopped.m_hEvent);
      }
      return rc;
   }
   else
      return ASE_NotPresent;
}

ASIOError ASIOSession::start()
{
   Locker l;
   if (isOpen())
   {
       ASIOError rc=ASE_OK;
       if (!isRunning())
          {
	      rc=m_pIASIO->start();
          ResetEvent(m_stopped.m_hEvent);
          }
      return rc;
   }
   else
      return ASE_NotPresent;
}

ASIOError ASIOSession::canSampleRate(ASIOSampleRate rate)
{
   Locker l;
   return (isOpen())?m_pIASIO->canSampleRate(rate):ASE_NotPresent;
}

ASIOError ASIOSession::getSampleRate(ASIOSampleRate * rate)
{
   if (!rate)
      return ASE_InvalidParameter;
   Locker l;
   return (isOpen())?m_pIASIO->getSampleRate(rate):ASE_NotPresent;
}

ASIOError ASIOSession::setSampleRate(ASIOSampleRate rate)
{
   Locker l;
   return (isOpen())?m_pIASIO->setSampleRate(rate):ASE_NotPresent;
}

ASIOError ASIOSession::outputReady()
{
   Locker l; 
   return (isOpen())?m_pIASIO->outputReady():ASE_NotPresent;
}

ASIOError ASIOSession::future(long selector, void *opt)
{
   Locker l;
   return (isOpen())?m_pIASIO->future(selector,opt):ASE_NotPresent;
}

bool ASIOSession::isOpen()
{
   Locker l;
   return m_pIASIO!=NULL && m_bOpen;
}

bool ASIOSession::isRunning()
{
    Locker l;
    return WaitForSingleObject(m_stopped.m_hEvent,0)==WAIT_TIMEOUT;//event is not signalled
}

DWORD WINAPI ASIOSession::ASIOThreadProc(LPVOID pV)
{
    /*This code section is executed while ASIOSession::open() is waiting for it, so no Locker is needed
    */
    if (!pV)
        return E_FAIL;
    ASIOSession *pSession = static_cast<ASIOSession*>(pV);
    if (pSession->m_clsid==GUID_NULL)
      return E_FAIL;

    //we need to create a message queue and initialize COM
    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);

    HRESULT hr =CoCreateInstance(pSession->m_clsid,NULL,CLSCTX_INPROC_SERVER,pSession->m_clsid,(LPVOID *)&pSession->m_pIASIO);
    if (!(hr==ERROR_SUCCESS && pSession->m_pIASIO))
    {
        postError("ASIOWmpPlg: Failed to create ASIO interface",hr);
        pSession->m_pIASIO=NULL;      
        return E_FAIL;
    }
    SetEvent(pSession->m_ASIOCreated.m_hEvent);
    /* end of ASIOSession::open() code section
    */
    while (GetMessage(&msg, NULL, 0, 0))
	{
        DispatchMessage(&msg);
    }
    /*The rest of the thread proc is executed while ASIOSession::close() is waiting for it, so no Locker is needed
    */
    if (pSession->m_pIASIO)
    {
        pSession->m_pIASIO->Release();
        pSession->m_pIASIO = NULL;
    }
    ResetEvent(pSession->m_ASIOCreated.m_hEvent);    
    return S_OK;
}

ASIOError ASIOSession::open()
{
    Locker l;
    if (isOpen())
       return ASE_OK;
    HANDLE objects[2]={NULL,NULL};
    if (m_hASIOThread == NULL)
        m_hASIOThread = CreateThread(NULL,0,ASIOThreadProc,this,0,&m_dwASIOThreadId);
    objects[0]=m_ASIOCreated.m_hEvent;
    objects[1]=m_hASIOThread;
    if (WaitForMultipleObjects(2,objects,FALSE,INFINITE) != WAIT_OBJECT_0)
    {//thread has ended    
        m_hASIOThread=NULL;
        m_dwASIOThreadId=0;
        return ASE_NotPresent;
    }
    if (!m_pIASIO->init(findAppWindowHandle())) 
        return ASE_NotPresent;
    m_bOpen=true;
    return ASE_OK;
}

ASIOError ASIOSession::close()
{
   Locker l;
   if (isOpen())
   {
      stop();
      Sleep(0);//let's yield the time slice(is it still needed?)
      m_pIASIO->disposeBuffers();
      PostThreadMessage(m_dwASIOThreadId,WM_QUIT,0,0);
      WaitForSingleObject(m_hASIOThread,INFINITE);
      m_hASIOThread=NULL;
      m_dwASIOThreadId=0;
      m_pIASIO=NULL;
      m_bOpen=false;
   }
   return ASE_OK;
}

void ASIOSession::putCLSID(CLSID newVal)
{
    Locker l;
    m_clsid=newVal;
    //let's persist it
    CRegKey key;
    key.Create(HKEY_CURRENT_USER,kszPrefsRegKey);
    key.SetGUIDValue(kszPrefsCLSID,m_clsid);
    key.Close();
}

CThreadPriorityRaiser::CThreadPriorityRaiser():m_bCalled(false),m_func(NULL),m_func2(NULL)
{
    const char * LIB_NAME = "Avrt.dll";
    HMODULE hModule = GetModuleHandle(LIB_NAME);
    if (hModule) {
        m_func  = (AvSetMmThreadCharacteristicsType)GetProcAddress(hModule,"AvSetMmThreadCharacteristicsA");
        m_func2 = (AvSetMmThreadPriorityType)       GetProcAddress(hModule,"AvSetMmThreadPriority");
    }
}

void CThreadPriorityRaiser::raise(bool bCritical)
{                              
    if (!m_bCalled)
    {
        if (m_func)
        {
            DWORD taskIndex = 0;
            HANDLE hHandle = (*m_func)("Pro Audio", &taskIndex);
            if (bCritical)
            {
                if (m_func2) 
                    (*m_func2)(hHandle,AVRT_PRIORITY_CRITICAL);
                SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
            }
        }
        m_bCalled=true;
    }
}
