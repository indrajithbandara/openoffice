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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_cui.hxx"
#include <com/sun/star/embed/Aspects.hpp>

#include <pastedlg.hxx>
#include <svtools/svmedit.hxx>
#include <svtools/insdlg.hxx>
#include <vcl/dialog.hxx>
#include <vcl/button.hxx>
#include <vcl/fixed.hxx>
#include <vcl/group.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/msgbox.hxx>
#include "svuidlg.hrc"
#include <sot/formats.hxx>
#include <sot/stg.hxx>
#include <svtools/sores.hxx>
#include <vcl/svapp.hxx>

#include <dialmgr.hxx>

SvPasteObjectDialog::SvPasteObjectDialog( Window* pParent )

    : ModalDialog( pParent, CUI_RES( MD_PASTE_OBJECT ) ),
    aFtSource( this, CUI_RES( FT_SOURCE ) ),
    aFtObjectSource( this, CUI_RES( FT_OBJECT_SOURCE ) ),
    aRbPaste( this, CUI_RES( RB_PASTE ) ),
    aRbPasteLink( this, CUI_RES( RB_PASTE_LINK ) ),
    aCbDisplayAsIcon( this, CUI_RES( CB_DISPLAY_AS_ICON ) ),
    aPbChangeIcon( this, CUI_RES( PB_CHANGE_ICON ) ),
    aFlChoice( this, CUI_RES( FL_CHOICE ) ),
    aLbInsertList( this, CUI_RES( LB_INSERT_LIST ) ),
    aOKButton1( this, CUI_RES( 1 ) ),
    aCancelButton1( this, CUI_RES( 1 ) ),
    aHelpButton1( this, CUI_RES( 1 ) ),
    aSObject( CUI_RES( S_OBJECT ) )
{
    FreeResource();
	SetHelpId( HID_PASTE_DLG );
	SetUniqueId( HID_PASTE_DLG );

	Font aFont = aFtObjectSource.GetFont();
	aFont.SetWeight( WEIGHT_LIGHT );
	aFtObjectSource.SetFont( aFont );
	aOKButton1.Disable();

    ObjectLB().SetSelectHdl( LINK( this, SvPasteObjectDialog, SelectHdl ) );
    ObjectLB().SetDoubleClickHdl( LINK( this, SvPasteObjectDialog, DoubleClickHdl ) );
	SetDefault();

	aLbInsertList.SetAccessibleName(aFlChoice.GetText());
}

void SvPasteObjectDialog::SelectObject()
{
	if ( aLbInsertList.GetEntryCount() &&
		 !aRbPaste.IsVisible() && !aRbPasteLink.IsVisible() )
	{
		aLbInsertList.SelectEntryPos(0);
		SelectHdl( &aLbInsertList );
	}
}

IMPL_LINK( SvPasteObjectDialog, SelectHdl, ListBox *, pListBox )
{
	(void)pListBox;

	if ( !aOKButton1.IsEnabled() )
		aOKButton1.Enable();
	return 0;
}

IMPL_LINK_INLINE_START( SvPasteObjectDialog, DoubleClickHdl, ListBox *, pListBox )
{
	(void)pListBox;

	EndDialog( RET_OK );
	return 0;
}
IMPL_LINK_INLINE_END( SvPasteObjectDialog, DoubleClickHdl, ListBox *, pListBox )

void SvPasteObjectDialog::SetDefault()
{
	bLink   = sal_False;
    nAspect = (sal_uInt16)::com::sun::star::embed::Aspects::MSOLE_CONTENT;
}

SvPasteObjectDialog::~SvPasteObjectDialog()
{
	void * pStr = aSupplementTable.First();
	while( pStr )
	{
		delete (String *)pStr;
		pStr = aSupplementTable.Next();
	}
}

/*************************************************************************
|*    SvPasteObjectDialog::Insert()
|*
|*    Beschreibung
|*    Ersterstellung    MM 14.06.94
|*    Letzte Aenderung  KA 16.03.2001
*************************************************************************/
void SvPasteObjectDialog::Insert( SotFormatStringId nFormat, const String& rFormatName )
{
	String * pStr = new String( rFormatName );
	if( !aSupplementTable.Insert( nFormat, pStr ) )
		delete pStr;
}

sal_uLong SvPasteObjectDialog::GetFormat( const TransferableDataHelper& rHelper,
                                      const DataFlavorExVector* pFormats,
                                      const TransferableObjectDescriptor* )
{
    //TODO/LATER: why is the Descriptor never used?!
	TransferableObjectDescriptor aDesc;
    if( rHelper.HasFormat( SOT_FORMATSTR_ID_OBJECTDESCRIPTOR ) )
	    ((TransferableDataHelper&)rHelper).GetTransferableObjectDescriptor(
								SOT_FORMATSTR_ID_OBJECTDESCRIPTOR, aDesc );
    if ( !pFormats )
        pFormats = &rHelper.GetDataFlavorExVector();

	//Dialogbox erzeugen und fuellen
	String aSourceName, aTypeName;
	sal_uLong nSelFormat = 0;
	SvGlobalName aEmptyNm;

    ObjectLB().SetUpdateMode( sal_False );

    DataFlavorExVector::iterator aIter( ((DataFlavorExVector&)*pFormats).begin() ),
                                 aEnd( ((DataFlavorExVector&)*pFormats).end() );
	while( aIter != aEnd )
	{
		::com::sun::star::datatransfer::DataFlavor aFlavor( *aIter );
		SotFormatStringId nFormat = (*aIter++).mnSotId;

		String*	pName = (String*) aSupplementTable.Get( nFormat );
		String aName;

#ifdef WNT
/*
		if( !pName &&
			( nFormat == SOT_FORMATSTR_ID_EMBED_SOURCE_OLE || nFormat == SOT_FORMATSTR_ID_EMBEDDED_OBJ_OLE ) )
		{
			sal_Bool IsClipboardObject_Impl( SotDataObject * );
			if( IsClipboardObject_Impl( pDataObj ) )
			{
				IDataObject * pDO = NULL;
				OleGetClipboard( &pDO );
				if( pDO )
				{
					FORMATETC fe;
					STGMEDIUM stm;
					(fe).cfFormat=RegisterClipboardFormat( "Object Descriptor" );
					(fe).dwAspect=DVASPECT_CONTENT;
					(fe).ptd=NULL;
					(fe).tymed=TYMED_HGLOBAL;
					(fe).lindex=-1;

					if (SUCCEEDED(pDO->GetData(&fe, &stm)))
					{
						LPOBJECTDESCRIPTOR pOD=(LPOBJECTDESCRIPTOR)GlobalLock(stm.hGlobal);
						if( pOD->dwFullUserTypeName )
						{
							OLECHAR * pN = (OLECHAR *)(((sal_uInt8 *)pOD) + pOD->dwFullUserTypeName);
							aName.Append( pN );
							pName = &aName;
							// set format to ole object
							nFormat = SOT_FORMATSTR_ID_EMBED_SOURCE_OLE;
						}
						if( pOD->dwSrcOfCopy )
						{
							OLECHAR * pN = (OLECHAR *)(((sal_uInt8 *)pOD) + pOD->dwSrcOfCopy);
							aSourceName.Append( *pN++ );
						}
						else
							aSourceName = String( ResId( STR_UNKNOWN_SOURCE, SOAPP->GetResMgr() ) );
						GlobalUnlock(stm.hGlobal);
						ReleaseStgMedium(&stm);
					}
				}
			}
		}
*/
#endif

		// if there is an "Embed Source" or and "Embedded Object" on the
		// Clipboard we read the Description and the Source of this object
		// from an accompanied "Object Descriptor" format on the clipboard
		// Remember: these formats mostly appear together on the clipboard
		if ( !pName )
		{
            SvPasteObjectHelper::GetEmbeddedName(rHelper,aName,aSourceName,nFormat);
			if ( aName.Len() )
				pName = &aName;
		}


		if( pName )
		{
			aName = *pName;

			if( SOT_FORMATSTR_ID_EMBED_SOURCE == nFormat )
			{
                if( aDesc.maClassName != aEmptyNm )                                {
					aSourceName = aDesc.maDisplayName;

					if( aDesc.maClassName == aObjClassName )
						aName = aObjName;
					else
						aName = aTypeName = aDesc.maTypeName;
				}
			}
			else if( SOT_FORMATSTR_ID_LINK_SOURCE == nFormat )
			{
                PasteLink().Enable();
				continue;
			}
			else if( !aName.Len() )
                aName = SvPasteObjectHelper::GetSotFormatUIName( nFormat );

            if( LISTBOX_ENTRY_NOTFOUND == ObjectLB().GetEntryPos( aName ) )
                ObjectLB().SetEntryData(
                    ObjectLB().InsertEntry( aName ), (void*) nFormat );
		}
	}

	if( !aTypeName.Len() && !aSourceName.Len() )
	{
		if( aDesc.maClassName != aEmptyNm )
		{
			aSourceName = aDesc.maDisplayName;
			aTypeName = aDesc.maTypeName;
		}

		if( !aTypeName.Len() && !aSourceName.Len() )
        {
            com::sun::star::lang::Locale aLocale = Application::GetSettings().GetUILocale();
            ResMgr* pMgr = ResMgr::CreateResMgr( "svt", aLocale );
            // global resource from svtools (former so3 resource)
            if( pMgr )
                aSourceName = String( ResId( STR_UNKNOWN_SOURCE, *pMgr ) );
            delete pMgr;
        }
	}

    ObjectLB().SetUpdateMode( sal_True );
    SelectObject();

	if( aSourceName.Len() )
	{
		if( aTypeName.Len() )
			aTypeName += '\n';

		aTypeName += aSourceName;
		aTypeName.ConvertLineEnd();
	}

    ObjectSource().SetText( aTypeName );

	SetDefault();

    if( Dialog::Execute() == RET_OK )
	{
        bLink = PasteLink().IsChecked();

        if( AsIconBox().IsChecked() )
            nAspect = (sal_uInt16)com::sun::star::embed::Aspects::MSOLE_ICON;

        nSelFormat  = (sal_uLong)ObjectLB().GetEntryData( ObjectLB().GetSelectEntryPos() );
	}

	return nSelFormat;
}

void SvPasteObjectDialog::SetObjName( const SvGlobalName & rClass, const String & rObjName )
{
    aObjClassName = rClass;
    aObjName = rObjName;
}

