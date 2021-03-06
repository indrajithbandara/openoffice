/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// SOActiveX.h : Declaration of the CSOActiveX

#ifndef __SOACTIVEX_H_
#define __SOACTIVEX_H_

#include "resource.h"       // main symbols
#include <ExDispID.h>
#include <ExDisp.h>
#include <shlguid.h>
#include <atlctl.h>

#include "so_activex.h"

/////////////////////////////////////////////////////////////////////////////
// CSOActiveX
class ATL_NO_VTABLE CSOActiveX : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public IDispatchImpl<ISOActiveX, &IID_ISOActiveX, &LIBID_SO_ACTIVEXLib>,
	public CComControl<CSOActiveX>,
	public IPersistStreamInitImpl<CSOActiveX>,
	public IOleControlImpl<CSOActiveX>,
	public IOleObjectImpl<CSOActiveX>,
	public IOleInPlaceActiveObjectImpl<CSOActiveX>,
	public IViewObjectExImpl<CSOActiveX>,
	public IOleInPlaceObjectWindowlessImpl<CSOActiveX>,
//	public IConnectionPointContainerImpl<CSOActiveX>,
	public CComCoClass<CSOActiveX, &CLSID_SOActiveX>,
//	public CProxy_ItryPluginEvents< CSOActiveX >,
	public IPersistPropertyBagImpl< CSOActiveX >,
	public IProvideClassInfo2Impl<	&CLSID_SOActiveX,
									&DIID__ISOActiveXEvents,
									&LIBID_SO_ACTIVEXLib >,
    public IObjectSafetyImpl< CSOActiveX, 
                              INTERFACESAFE_FOR_UNTRUSTED_DATA >
{
protected:
	CComPtr<IWebBrowser2>	mWebBrowser2;
	DWORD					mCookie;

	CComPtr<IDispatch> 		mpDispFactory;
	CComPtr<IDispatch> 		mpDispFrame;
	CComPtr<IDispatch> 		mpDispWin;
    OLECHAR*          		mCurFileUrl;
	BOOL					mbLoad;
	BOOL					mbViewOnly;
    WNDCLASS                mPWinClass;
	HWND					mParentWin;
	HWND					mOffWin;
public:
	CSOActiveX();
	~CSOActiveX();

DECLARE_REGISTRY_RESOURCEID(IDR_SOACTIVEX)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSOActiveX)
	COM_INTERFACE_ENTRY(ISOActiveX)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IViewObjectEx)
	COM_INTERFACE_ENTRY(IViewObject2)
	COM_INTERFACE_ENTRY(IViewObject)
	COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
	COM_INTERFACE_ENTRY(IOleInPlaceObject)
	COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
	COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
	COM_INTERFACE_ENTRY(IOleControl)
	COM_INTERFACE_ENTRY(IOleObject)
	COM_INTERFACE_ENTRY(IPersistStreamInit)
	COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
//	COM_INTERFACE_ENTRY(IConnectionPointContainer)
	COM_INTERFACE_ENTRY(IProvideClassInfo)
	COM_INTERFACE_ENTRY(IProvideClassInfo2)
	COM_INTERFACE_ENTRY(IPersistPropertyBag)
	COM_INTERFACE_ENTRY(IObjectSafety)
END_COM_MAP()

BEGIN_PROP_MAP(CSOActiveX)
	PROP_DATA_ENTRY("_cx", m_sizeExtent.cx, VT_UI4)
	PROP_DATA_ENTRY("_cy", m_sizeExtent.cy, VT_UI4)
	// Example entries
	// PROP_ENTRY("Property Description", dispid, clsid)
	// PROP_PAGE(CLSID_StockColorPage)
END_PROP_MAP()

BEGIN_CONNECTION_POINT_MAP(CSOActiveX)
END_CONNECTION_POINT_MAP()

BEGIN_MSG_MAP(CSOActiveX)
	CHAIN_MSG_MAP(CComControl<CSOActiveX>)
	DEFAULT_REFLECTION_HANDLER()
END_MSG_MAP()
// Handler prototypes:
//  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
//  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);



// IViewObjectEx
	DECLARE_VIEW_STATUS(VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_OPAQUE)

// ISOActiveX
public:
    
	STDMETHOD(SetClientSite)( IOleClientSite* aClientSite );
	STDMETHOD(Invoke)(  DISPID dispidMember, 
						REFIID riid, 
						LCID lcid, 
                        WORD wFlags, 
						DISPPARAMS* pDispParams,
                        VARIANT* pvarResult, 
						EXCEPINFO* pExcepInfo,
                        UINT* puArgErr);
	STDMETHOD(Load) ( LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog );
	STDMETHOD(Load) ( LPSTREAM pStm );
    STDMETHOD(InitNew) ();
	HRESULT OnDrawAdvanced(ATL_DRAWINFO& di);
	HRESULT OnDraw(ATL_DRAWINFO& di) { return S_OK; }

	HRESULT CreateFrameOldWay( HWND hwnd, int width, int height );
	HRESULT GetUnoStruct( OLECHAR* sStructName, CComPtr<IDispatch>& pdispResult );
	HRESULT LoadURLToFrame();
	HRESULT ShowSomeBars();
	HRESULT HideAllBars();
	HRESULT CallDispatch1PBool( OLECHAR* sUrl, OLECHAR* sArgName, BOOL sArgVal );
	HRESULT GetUrlStruct( OLECHAR* sUrl, CComPtr<IDispatch>& pdispUrl );
	HRESULT	Cleanup();
};

#endif //__SOACTIVEX_H_

