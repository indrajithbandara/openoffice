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



#include "iipaobj.hxx"
#include "embeddoc.hxx"



CIIAObj::CIIAObj(DocumentHolder* pDocHolder)
	: m_refCount( 0L ),
      m_rDocHolder( pDocHolder )
{
}


CIIAObj::~CIIAObj()
{
    return;
}

/* IUnknown methods */

STDMETHODIMP CIIAObj::QueryInterface(REFIID riid, LPVOID FAR *ppv)
{
    *ppv=NULL;
	
    if(IID_IUnknown==riid || 
	   IID_IOleWindow==riid ||
	   IID_IOleInPlaceActiveObject==riid)
        *ppv=this;
	
    //AddRef any interface we'll return.
    if (NULL!=*ppv)
	{
        ((LPUNKNOWN)*ppv)->AddRef();
        return NOERROR;
	}

    return ResultFromScode(E_NOINTERFACE);
}


STDMETHODIMP_(ULONG) CIIAObj::AddRef(void)
{
	return osl_incrementInterlockedCount( &m_refCount);
}

STDMETHODIMP_(ULONG) CIIAObj::Release(void)
{
	sal_Int32 nCount = osl_decrementInterlockedCount( &m_refCount);
	if ( nCount == 0 )
		delete this;

	return nCount;
}

/* IOleInPlaceActiveObject methods*/

STDMETHODIMP CIIAObj::GetWindow(HWND *)
{
	return NOERROR;
}

STDMETHODIMP CIIAObj::ContextSensitiveHelp(BOOL)
{
	return NOERROR;
}

STDMETHODIMP CIIAObj::TranslateAccelerator(LPMSG)
{
	return NOERROR;
}

STDMETHODIMP CIIAObj::OnFrameWindowActivate(BOOL)
{
	return NOERROR;
}

STDMETHODIMP CIIAObj::OnDocWindowActivate(BOOL)
{
	return NOERROR;
}

STDMETHODIMP CIIAObj::ResizeBorder(
	LPCRECT pRect,LPOLEINPLACEUIWINDOW,BOOL bFrame)
{
    if(!bFrame) return NOERROR;
    
	if ( !m_rDocHolder.is() )
		return E_FAIL;

    return m_rDocHolder->SetContRects(pRect);
}


STDMETHODIMP CIIAObj::EnableModeless(BOOL)
{
	return NOERROR;
}

// Fix strange warnings about some 
// ATL::CAxHostWindow::QueryInterface|AddRef|Releae functions.
// warning C4505: 'xxx' : unreferenced local function has been removed
#if defined(_MSC_VER)
#pragma warning(disable: 4505)
#endif
