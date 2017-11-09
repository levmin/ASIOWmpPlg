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

//ASIO playback helper class

#pragma once

//This file is in ASIOSDK2\common directory
#include <IASIODrv.h>


//Windows critical section wrapper
struct CriticalSection: public CRITICAL_SECTION 
{   
  CriticalSection() {   ::InitializeCriticalSection(this); }
  ~CriticalSection(){   ::DeleteCriticalSection(this);     }
}; 

//Windows event object wrapper
struct CEvent
{
  CEvent(BOOL bInitialState) {m_hEvent=CreateEvent(NULL,TRUE,bInitialState,NULL);}
  ~CEvent() {CloseHandle(m_hEvent);}
  HANDLE m_hEvent;
};

//The plugin receives audio data from the WMP and stores them in a set of 
//circular overflow buffers, one per output channel. The ASIO bufferSwitchTimeInfo callback moves that 
//data into ASIO buffers. Each audio sample is stored in a byte-aligned container. The currently
//stored data chunk starts at a m_nStartPos position and may wrap around the end of the buffer
//and continue from its beginning.
struct COverflowBuffer
{
   short m_nContainerSize; 
   long m_nStartPos;       //in bytes
   long m_nValidBytes;     //length of the current data chunk in bytes possibly including 
						   //a wrap-around section at the beginning of the buffer
   std::vector<BYTE> m_buffer;
   COverflowBuffer():m_nContainerSize(0),m_nStartPos(0),m_nValidBytes(0){}
   long getEndPos() const 
   {
      return m_buffer.size()?(m_nStartPos+m_nValidBytes)%m_buffer.size():0;
   }
};

HWND findAppWindowHandle();
void postError(const char * text, HRESULT hr=S_OK);

class CThreadPriorityRaiser
{
      typedef HANDLE (__stdcall * AvSetMmThreadCharacteristicsType)(__in     LPCTSTR TaskName,__inout  LPDWORD TaskIndex);
      typedef BOOL (__stdcall * AvSetMmThreadPriorityType)(__in  HANDLE AvrtHandle, __in  int Priority);
      AvSetMmThreadCharacteristicsType m_func;
      AvSetMmThreadPriorityType m_func2;
      bool m_bCalled;
public:      
      CThreadPriorityRaiser();
      void raise(bool bCritical);
};

/* A low-level wrapper around the ASIO COM interface. An instance of this
   interface is created and released in a dedicated thread to improve compatibility
   with the COM apartment thread model. The access to this instance is serialized by
   the Locker object inside the session class.
*/
class ASIOSession 
{
    CLSID  m_clsid;      //class ID of ASIO interface
    bool m_bOpen;        //true if the COM interface is created
    static DWORD WINAPI ASIOThreadProc(LPVOID pV);
    HANDLE m_hASIOThread; //thread handle
    DWORD  m_dwASIOThreadId;//thread id
    CEvent m_ASIOCreated; //event is signalled when the ASIO thread creates the ASIO interface           
protected:
    IASIO* m_pIASIO;      //pointer to the ASIO COM interface               
    CEvent m_stopped;     //the event is set when the session is running and reset otherwise
    class Locker
    {
        static CriticalSection m_section;
    public:      
        Locker()      {   EnterCriticalSection(&m_section);      }
        ~Locker()      {   LeaveCriticalSection(&m_section);      }
    };

public:

	ASIOSession();
    ~ASIOSession();
    const CLSID& getCLSID() const {return m_clsid;}
    void putCLSID(CLSID newVal);
    ASIOError open();    //start the ASIO thread
    ASIOError close();   //close the ASIO thread
    //next 7 functions are thin wrappers around ASIO functions
	ASIOError start();  
    ASIOError stop();
    ASIOError canSampleRate(ASIOSampleRate rate);
    ASIOError setSampleRate(ASIOSampleRate rate);
    ASIOError getSampleRate(ASIOSampleRate * rate);
    ASIOError future(long selector, void *opt);
    ASIOError outputReady();

    bool isOpen();   //checks if ASIO interface is allocated
    bool isRunning();
};

struct IWMPServices;
//The ASIOPlayer is an global ASIOSession singleton that takes audio data
//from the plugin and steams it through the ASIO interface
class ASIOPlayer : public ASIOSession
{
    //the data does not get written into ASIO interface until we have at least
	//m_nLowWaterBufferCount worth of ASIO buffers in m_overflowBuffers
	static const int DEFAULT_LOW_WATER_BUFFER_COUNT=4;
    int m_nLowWaterBufferCount;
    long   m_nBufferSize;      //ASIO buffer size in samples
    vector<ASIOBufferInfo> m_infos; //information about ASIO buffer configuration
    vector<ASIOSampleType> m_types; //the same size as m_infos 
    vector< COverflowBuffer> m_overflowBuffers; //the same size as m_infos 
    long m_samplesInOverflowBuffers;//all buffers have the same number of samples
    //every overflow buffer has m_nValidBytes equal to m_samplesInOverflowBuffers multiplied by its m_nContainerSize
    bool m_bAccumulationMode;  //true if we are still under m_nLowWaterBufferCount mark 
    static int  m_nPluginCounter; //a reference counter; used to shut ASIO down when the last plugin is destructed
	static DWORD WINAPI StartStopPauseDetectionThreadProc(LPVOID pV);
    IWMPServices * m_pWMPServices;
	static int  m_nWMPServicesCounter; //a reference counter for m_pWMPServices
	ASIOError initOrReset(int nChannels,int nSampleRate);

	CEvent m_shutdownInitiated;//set inside the shutdown function; signals the StartStopPauseDetectionThreadProc thread to quit

    CThreadPriorityRaiser m_raiser;  //for ASIO thread
    CThreadPriorityRaiser m_raiser2;  //for queueForPlayback thread

public:
      ASIOPlayer();
      ~ASIOPlayer();
	  ASIOError maybeInitOrReset(int nChannels,int nSampleRate);
      ASIOError shutdown();
      void incrementPluginCounter();
      //there is no decrementPluginCounter; the decrement is done in maybeShutdown
	  ASIOError maybeShutdown();
	  ASIOError start();
      ASIOError stop();
      ASIOError pause();
      void queueForPlayback(ASIOSampleType inType,long nFramesToWrite,void * data);
	  void maybeAdviseWMPServices(IWMPServices * pWMPServices);
	  void maybeUnadviseWMPServices(IWMPServices * pWMPServices);
	  int  getWMPState();

      //ASIO callback implementation
	  void bufferSwitch(long index, ASIOBool processNow);
      void sampleRateChanged(ASIOSampleRate sRate);
      ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow);
      long asioMessages(long selector, long value, void* message, double* opt);

};

extern ASIOPlayer g_player;
