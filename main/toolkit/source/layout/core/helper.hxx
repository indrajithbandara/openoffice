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



#ifndef LAYOUT_CORE_HELPER_HXX
#define LAYOUT_CORE_HELPER_HXX

#include <toolkit/dllapi.h>
#include <vector>

#include <com/sun/star/awt/XLayoutConstrains.hpp>
#include <com/sun/star/awt/XLayoutContainer.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/xml/input/XRoot.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <cppuhelper/implbase1.hxx>
#include <cppuhelper/propshlp.hxx>
#include <osl/module.h>
#include <rtl/ustring.hxx>

class Window;
class VCLXWindow;
extern "C"
{
	typedef Window* (SAL_CALL *WindowCreator) (VCLXWindow** component, rtl::OUString const& name, Window* parent, long& attributes);
}

namespace layoutimpl
{

namespace css = ::com::sun::star;

/* ChildProps -- a helper to set child properties for the XLayoutContainer interface. */

class LockHelper
{
public:
    osl::Mutex               maGuard;
    cppu::OBroadcastHelper maBrdcstHelper;
    LockHelper() : maBrdcstHelper( maGuard )
    {
    }
};

class PropHelper : public LockHelper
                 , public cppu::OPropertySetHelper
                 , public cppu::OWeakObject
{
    cppu::OPropertyArrayHelper *pHelper;

    struct PropDetails
    {
        rtl::OUString  aName;
        css::uno::Type aType;
        void          *pValue;
    };
    std::vector< PropDetails > maDetails;

protected:
    void addProp( char const *pName, sal_Int32 nNameLen, rtl_TextEncoding e,
                  css::uno::Type aType, void *pPtr );

public:
    PropHelper();

    // com::sun::star::uno::XInterface
    void SAL_CALL acquire() throw() { OWeakObject::acquire(); }
    void SAL_CALL release() throw() { OWeakObject::release(); }
    ::com::sun::star::uno::Any SAL_CALL queryInterface( const ::com::sun::star::uno::Type & rType ) throw(::com::sun::star::uno::RuntimeException);

    // cppu::OPropertySetHelper
    virtual cppu::IPropertyArrayHelper & SAL_CALL getInfoHelper();
    virtual sal_Bool SAL_CALL convertFastPropertyValue( css::uno::Any &,
                                                        css::uno::Any &, sal_Int32 nHandle, const css::uno::Any & )
        throw(css::lang::IllegalArgumentException);
    virtual void SAL_CALL setFastPropertyValue_NoBroadcast( sal_Int32 nHandle,
                                                            const css::uno::Any& rValue ) throw (css::uno::Exception);
    using OPropertySetHelper::getFastPropertyValue;
    virtual void SAL_CALL getFastPropertyValue( css::uno::Any& rValue,
                                                sal_Int32 nHandle ) const;

    virtual css::uno::Reference <css::beans::XPropertySetInfo> SAL_CALL getPropertySetInfo () throw (css::uno::RuntimeException);

    struct Listener
    {
        virtual void propertiesChanged() = 0;
    };

    void setChangeListener( Listener *pListener )
    {
        mpListener = pListener;
    }

protected:
    Listener *mpListener;
};

css::uno::Any anyFromString (const rtl::OUString &value, const css::uno::Type &type);

// The native widgets wrapper hierarchy may not reflect that of the layout
// hierarchy as some containers don't have an associated native widget.
// Use this function to get the native parent of the given peer.
css::uno::Reference< css::awt::XWindowPeer >
getParent( css::uno::Reference< css::uno::XInterface > xPeer );

class TOOLKIT_DLLPUBLIC WidgetFactory
{
public:
	static oslModule mSfx2Library;
	static WindowCreator mSfx2CreateWidget;

    // Should use UNO services in due course
    static css::uno::Reference <css::awt::XLayoutConstrains> toolkitCreateWidget (css::uno::Reference <css::awt::XToolkit> xToolkit, css::uno::Reference <css::uno::XInterface> xParent, rtl::OUString const& name, long properties);
    static css::uno::Reference< css::awt::XLayoutConstrains > createWidget( css::uno::Reference <css::awt::XToolkit > xToolkit, css::uno::Reference< css::uno::XInterface > xParent, rtl::OUString const &name, long properties);
    static css::uno::Reference <css::awt::XLayoutContainer> createContainer (rtl::OUString const& name);
    static css::uno::Reference <css::awt::XLayoutConstrains> implCreateWidget (css::uno::Reference <css::uno::XInterface> xParent, rtl::OUString name, long attributes);
    static Window* sfx2CreateWindow (VCLXWindow** component, Window* parent, rtl::OUString const& name, long& attributes);
    static Window* layoutCreateWindow (VCLXWindow** component, Window *parent, rtl::OUString const& name, long& attributes);
};


css::uno::Reference< css::graphic::XGraphic > loadGraphic( const char *pName );

} // end namespace layoutimpl

#endif /* LAYOUT_CORE_HELPER_HXX */
