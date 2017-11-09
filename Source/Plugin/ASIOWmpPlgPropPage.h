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
#include "ASIOWmpPlg.h"

// {35C3E643-0EFB-4650-AB99-FF7AC04D1EE5}
DEFINE_GUID(CLSID_ASIOWmpPlgPropPage, 0x35c3e643, 0xefb, 0x4650, 0xab, 0x99, 0xff, 0x7a, 0xc0, 0x4d, 0x1e, 0xe5);

/////////////////////////////////////////////////////////////////////////////
// CASIOWmpPlgPropPage
class ATL_NO_VTABLE CASIOWmpPlgPropPage :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CASIOWmpPlgPropPage, &CLSID_ASIOWmpPlgPropPage>,
	public IPropertyPageImpl<CASIOWmpPlgPropPage>,
	public CDialogImpl<CASIOWmpPlgPropPage>
{
public:
	        CASIOWmpPlgPropPage(); 
	virtual ~CASIOWmpPlgPropPage(); 
	

	enum {IDD = IDD_ASIOWMPPLGPROPPAGE};
	

DECLARE_REGISTRY_RESOURCEID(IDR_ASIOWMPPLGPROPPAGE)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CASIOWmpPlgPropPage) 
	COM_INTERFACE_ENTRY(IPropertyPage)
END_COM_MAP()

BEGIN_MSG_MAP(CASIOWmpPlgPropPage)
   COMMAND_HANDLER(IDC_CONTROL_PANEL, BN_CLICKED, OnBnClickedControlPanel)
   COMMAND_HANDLER(IDC_ASIO_DEVICE,CBN_SELCHANGE,OnCBnSelChange);
   CHAIN_MSG_MAP(IPropertyPageImpl<CASIOWmpPlgPropPage>)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
END_MSG_MAP()

   STDMETHOD(SetObjects)(ULONG nObjects, IUnknown** ppUnk);
   STDMETHOD(Apply)(void);

	LRESULT (OnInitDialog)(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

private:
    CComPtr<IASIOWmpPlg> m_spASIOWmpPlg;  // pointer to plug-in interface
    vector<CLSID> m_CLSIDs; 
    int m_curSel;
public:
   LRESULT OnBnClickedControlPanel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
   LRESULT OnCBnSelChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
};

