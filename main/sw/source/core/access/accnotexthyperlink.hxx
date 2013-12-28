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

#ifndef _ACCNOTEXTHYPERLINK_HXX
#define _ACCNOTEXTHYPERLINK_HXX


#include <com/sun/star/accessibility/XAccessibleHyperlink.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <vos/ref.hxx>
#include <cppuhelper/implbase1.hxx>
#include <fmtinfmt.hxx>
#include <frame.hxx>
#include <layfrm.hxx>

#include "accnotextframe.hxx"
/*
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::accessibility;
using namespace ::rtl;
*/
class SwAccessibleNoTextHyperlink : 
		public ::cppu::WeakImplHelper1<
		::com::sun::star::accessibility::XAccessibleHyperlink >
{
	friend class SwAccessibleNoTextFrame;

	::vos::ORef< SwAccessibleNoTextFrame > xFrame;
	const SwFrm *mpFrm;
	sal_uInt16 mnIndex;

	SwFrmFmt *GetFmt()
	{
		return ((SwLayoutFrm*)mpFrm)->GetFmt();
	}
public:

	SwAccessibleNoTextHyperlink( SwAccessibleNoTextFrame *p, const SwFrm* aFrm, sal_uInt16 nIndex = 0xFFFF );

	// XAccessibleAction
    virtual sal_Int32 SAL_CALL getAccessibleActionCount() 
		throw (::com::sun::star::uno::RuntimeException);
    virtual sal_Bool SAL_CALL doAccessibleAction( sal_Int32 nIndex ) 
		throw (::com::sun::star::lang::IndexOutOfBoundsException, 
				::com::sun::star::uno::RuntimeException);
    virtual ::rtl::OUString SAL_CALL getAccessibleActionDescription( 
				sal_Int32 nIndex ) 
		throw (::com::sun::star::lang::IndexOutOfBoundsException,
				::com::sun::star::uno::RuntimeException);
    virtual ::com::sun::star::uno::Reference< 
			::com::sun::star::accessibility::XAccessibleKeyBinding > SAL_CALL
		   	getAccessibleActionKeyBinding( sal_Int32 nIndex ) 
		throw (::com::sun::star::lang::IndexOutOfBoundsException, 
				::com::sun::star::uno::RuntimeException);

	// XAccessibleHyperlink
    virtual ::com::sun::star::uno::Any SAL_CALL getAccessibleActionAnchor( 
				sal_Int32 nIndex ) 
		throw (::com::sun::star::lang::IndexOutOfBoundsException, 
				::com::sun::star::uno::RuntimeException);
    virtual ::com::sun::star::uno::Any SAL_CALL getAccessibleActionObject( 
			sal_Int32 nIndex ) 
		throw (::com::sun::star::lang::IndexOutOfBoundsException, 
				::com::sun::star::uno::RuntimeException);
    virtual sal_Int32 SAL_CALL getStartIndex() 
		throw (::com::sun::star::uno::RuntimeException);
    virtual sal_Int32 SAL_CALL getEndIndex() 
		throw (::com::sun::star::uno::RuntimeException);
    virtual sal_Bool SAL_CALL isValid(  ) 
		throw (::com::sun::star::uno::RuntimeException);
};

#endif

