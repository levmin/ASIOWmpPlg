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
#include <stdio.h>
#include "ASIOWmpPlg.h"
#include "ASIOWmpPlgPropPage.h"
#include <asio.h>
#include "ASIOPlayer.h"
/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlgProp::CASIOWmpPlgProp
// Constructor

CASIOWmpPlgPropPage::CASIOWmpPlgPropPage()
{
	m_dwTitleID = IDS_TITLEPROPPAGE;
   m_curSel=0;
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlgProp::~CASIOWmpPlgProp
// Destructor

CASIOWmpPlgPropPage::~CASIOWmpPlgPropPage()
{
}

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlgProp::SetObjects
//

STDMETHODIMP CASIOWmpPlgPropPage::SetObjects(ULONG nObjects, IUnknown** ppUnk)
{
    // find our plug-in object, if it was passed in
    for (DWORD i = 0; i < nObjects; i++)
    {
	    CComPtr<IASIOWmpPlg> pPlugin;

        IUnknown    *pUnknown = ppUnk[i];
        if (pUnknown)
        {
            HRESULT hr = pUnknown->QueryInterface(__uuidof(IASIOWmpPlg), (void**)&pPlugin); // Get a pointer to the plug-in.
            if ((SUCCEEDED(hr)) && (pPlugin))
            {
                // save plug-in interface
                m_spASIOWmpPlg = pPlugin;
                break;
            }
        }
    }

    return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlgProp::OnInitDialog
//

LRESULT CASIOWmpPlgPropPage::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    CLSID plgClsid=GUID_NULL;
    if (m_spASIOWmpPlg)
    {
       m_spASIOWmpPlg->getCLSID(&plgClsid);
    }
    CRegKey asioKey;
    long rc=asioKey.Open(HKEY_LOCAL_MACHINE,"SOFTWARE\\ASIO",KEY_READ);
    if (rc != ERROR_SUCCESS)
    {
       postError("ASIOWmpPlg: No ASIO drivers found");
       return -1;
    }
    DWORD length=MAXSTRING;
    int index=0;
    char buf[MAXSTRING]={0},buf2[MAXSTRING]={0};
    rc=asioKey.EnumKey(index,buf,&length);
    m_curSel=(rc==ERROR_SUCCESS)?0:-1;
    while (rc==ERROR_SUCCESS)
    {
       CRegKey asioSubKey;
       rc = asioSubKey.Open(asioKey,buf,KEY_READ);
       if (rc != ERROR_SUCCESS)
       {
          postError("ASIOWmpPlg: Cannot read ASIO registry key",rc);
          break;
       }
       CLSID clsID;
       asioSubKey.QueryGUIDValue("CLSID",clsID);
       asioSubKey.Close();
       m_CLSIDs.push_back(clsID);
       SendDlgItemMessage(IDC_ASIO_DEVICE,CB_ADDSTRING,0,(LPARAM)&buf);
       if (plgClsid!=GUID_NULL && clsID==plgClsid)
       {
         m_curSel=index;
       }
       index++;
       DWORD length=MAXSTRING;
       rc=asioKey.EnumKey(index,buf,&length);
    }

    if (m_curSel>=0)
    {
        SendDlgItemMessage(IDC_ASIO_DEVICE,CB_SETCURSEL,m_curSel,0);
        ::EnableWindow(GetDlgItem(IDC_CONTROL_PANEL),TRUE);
    }
    else
    {
       ::EnableWindow(GetDlgItem(IDC_CONTROL_PANEL),FALSE);
    }
    g_player.shutdown(); //let's shut it down just in case
    bHandled=true;
    return 0;
}

LRESULT CASIOWmpPlgPropPage::OnCBnSelChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    m_curSel=(int)SendDlgItemMessage(IDC_ASIO_DEVICE,CB_GETCURSEL,0,0);
    SetDirty(TRUE); // Enable Apply.
    bHandled=true;
    return 0;
}

LRESULT CASIOWmpPlgPropPage::OnBnClickedControlPanel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   if (m_curSel>=0)
   {
      IASIO * pIASIO=NULL;
      HRESULT hr=CoCreateInstance(m_CLSIDs[m_curSel],NULL,CLSCTX_INPROC_SERVER,m_CLSIDs[m_curSel],(LPVOID*)&pIASIO);
      if (hr==S_OK && pIASIO)
      {
         pIASIO->init(findAppWindowHandle());
         ASIOError err = pIASIO->controlPanel();
         if (err == ASE_NotPresent) {
            postError("This ASIO device does not have a control panel",0);
         }
         else if (err != ASE_OK) {
            postError("ASIO control panel is not accessible",err);
         }
         pIASIO->Release();
      } 
	  else
	  {
         postError("ASIO interface is not accessible",hr);
	  }
   }
   bHandled=TRUE;
   return 0;
}

STDMETHODIMP CASIOWmpPlgPropPage::Apply(void)
{
    // update the plug-in
    if (m_spASIOWmpPlg && m_curSel>=0)
    {
       m_spASIOWmpPlg->putCLSID(m_CLSIDs[m_curSel]);
    }	

    SetDirty(FALSE); // Tell the property page to disable Apply.
	
	 return S_OK;
}
