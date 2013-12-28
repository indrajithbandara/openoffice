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
#include "precompiled_svtools.hxx"

#include <limits.h>
#include <tools/debug.hxx>
#include <vcl/wall.hxx>
#include <vcl/help.hxx>
#include <vcl/decoview.hxx>
#include <vcl/svapp.hxx>
#include <tools/poly.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/i18nhelp.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/controllayout.hxx>

#include <svtools/ivctrl.hxx>
#include "imivctl.hxx"
#include <svtools/svmedit.hxx>

#include <algorithm>
#include <memory>

#define DD_SCROLL_PIXEL 24
#define IMPICNVIEW_ACC_RETURN 1
#define IMPICNVIEW_ACC_ESCAPE 2

#define DRAWTEXT_FLAGS_ICON \
	( TEXT_DRAW_CENTER | TEXT_DRAW_TOP | TEXT_DRAW_ENDELLIPSIS | \
	  TEXT_DRAW_CLIP | TEXT_DRAW_MULTILINE | TEXT_DRAW_WORDBREAK | TEXT_DRAW_MNEMONIC )

#define DRAWTEXT_FLAGS_SMALLICON (TEXT_DRAW_LEFT|TEXT_DRAW_ENDELLIPSIS|TEXT_DRAW_CLIP)

#define EVENTID_SHOW_CURSOR				((void*)1)
#define EVENTID_ADJUST_SCROLLBARS		((void*)2)

struct SvxIconChoiceCtrlEntry_Impl
{
	SvxIconChoiceCtrlEntry*	_pEntry;
	Point			_aPos;
					SvxIconChoiceCtrlEntry_Impl( SvxIconChoiceCtrlEntry* pEntry, const Rectangle& rBoundRect )
					: _pEntry( pEntry), _aPos( rBoundRect.TopLeft()) {}
};

static sal_Bool bEndScrollInvalidate = sal_True;

// ----------------------------------------------------------------------------------------------

class IcnViewEdit_Impl : public MultiLineEdit
{
	Link 			aCallBackHdl;
	Accelerator 	aAccReturn;
	Accelerator 	aAccEscape;
	Timer 			aTimer;
	sal_Bool 			bCanceled;
	sal_Bool 			bAlreadyInCallback;
	sal_Bool			bGrabFocus;

	void 			CallCallBackHdl_Impl();
					DECL_LINK( Timeout_Impl, Timer * );
					DECL_LINK( ReturnHdl_Impl, Accelerator * );
					DECL_LINK( EscapeHdl_Impl, Accelerator * );

public:

					IcnViewEdit_Impl(
						SvtIconChoiceCtrl* pParent,
						const Point& rPos,
						const Size& rSize,
						const XubString& rData,
						const Link& rNotifyEditEnd );

					~IcnViewEdit_Impl();
	virtual void 	KeyInput( const KeyEvent& rKEvt );
	virtual long 	PreNotify( NotifyEvent& rNEvt );
	sal_Bool			EditingCanceled() const { return bCanceled; }
	void			StopEditing( sal_Bool bCancel = sal_False );
	sal_Bool			IsGrabFocus() const { return bGrabFocus; }
};

// ----------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------

SvxIconChoiceCtrl_Impl::SvxIconChoiceCtrl_Impl( SvtIconChoiceCtrl* pCurView,
	WinBits nWinStyle ) :
	aEntries( this ),
	aVerSBar( pCurView, WB_DRAG | WB_VSCROLL ),
	aHorSBar( pCurView, WB_DRAG | WB_HSCROLL ),
	aScrBarBox( pCurView ),
	aImageSize( 32, 32 ),
	pColumns( 0 )
{
	bChooseWithCursor=sal_False;
	pEntryPaintDev = 0;
	pCurEditedEntry = 0;
	pCurHighlightFrame = 0;
	pEdit = 0;
	pAnchor = 0;
	pDraggedSelection = 0;
	pPrevDropTarget = 0;
	pHdlEntry = 0;
	pHead = NULL;
	pCursor = NULL;
	bUpdateMode = sal_True;
    bEntryEditingEnabled = sal_False;
	bInDragDrop = sal_False;
	bHighlightFramePressed = sal_False;
	eSelectionMode = MULTIPLE_SELECTION;
	pView = pCurView;
	pZOrderList = new List; //SvPtrarr;
	ePositionMode = IcnViewPositionModeFree;
	SetStyle( nWinStyle );
	nFlags = 0;
	nUserEventAdjustScrBars = 0;
	nUserEventShowCursor = 0;
	nMaxVirtWidth = DEFAULT_MAX_VIRT_WIDTH;
	nMaxVirtHeight = DEFAULT_MAX_VIRT_HEIGHT;
	pDDRefEntry = 0;
	pDDDev = 0;
	pDDBufDev = 0;
	pDDTempDev = 0;
	eTextMode = IcnShowTextShort;
	pImpCursor = new IcnCursor_Impl( this );
	pGridMap = new IcnGridMap_Impl( this );

	aVerSBar.SetScrollHdl( LINK( this, SvxIconChoiceCtrl_Impl, ScrollUpDownHdl ) );
	aHorSBar.SetScrollHdl( LINK( this, SvxIconChoiceCtrl_Impl, ScrollLeftRightHdl ) );
	Link aEndScrollHdl( LINK( this, SvxIconChoiceCtrl_Impl, EndScrollHdl ) );
	aVerSBar.SetEndScrollHdl( aEndScrollHdl );
	aHorSBar.SetEndScrollHdl( aEndScrollHdl );

	nHorSBarHeight = aHorSBar.GetSizePixel().Height();
	nVerSBarWidth = aVerSBar.GetSizePixel().Width();

	aEditTimer.SetTimeout( 800 );
	aEditTimer.SetTimeoutHdl(LINK(this,SvxIconChoiceCtrl_Impl,EditTimeoutHdl));
	aAutoArrangeTimer.SetTimeout( 100 );
	aAutoArrangeTimer.SetTimeoutHdl(LINK(this,SvxIconChoiceCtrl_Impl,AutoArrangeHdl));
	aCallSelectHdlTimer.SetTimeout( 500 );
	aCallSelectHdlTimer.SetTimeoutHdl( LINK(this,SvxIconChoiceCtrl_Impl,CallSelectHdlHdl));

	aDocRectChangedTimer.SetTimeout( 50 );
	aDocRectChangedTimer.SetTimeoutHdl(LINK(this,SvxIconChoiceCtrl_Impl,DocRectChangedHdl));
	aVisRectChangedTimer.SetTimeout( 50 );
	aVisRectChangedTimer.SetTimeoutHdl(LINK(this,SvxIconChoiceCtrl_Impl,VisRectChangedHdl));

	Clear( sal_True );

    SetGrid( Size(100, 70) );
}

SvxIconChoiceCtrl_Impl::~SvxIconChoiceCtrl_Impl()
{
	pCurEditedEntry = 0;
	DELETEZ(pEdit);
	Clear();
	StopEditTimer();
	CancelUserEvents();
	delete pZOrderList;
	delete pImpCursor;
	delete pGridMap;
	delete pDDDev;
	delete pDDBufDev;
	delete pDDTempDev;
	delete pDraggedSelection;
	delete pEntryPaintDev;
	ClearSelectedRectList();
	ClearColumnList();
}

void SvxIconChoiceCtrl_Impl::Clear( sal_Bool bInCtor )
{
	StopEntryEditing( sal_True );
	nSelectionCount = 0;
	DELETEZ(pDraggedSelection);
	bInDragDrop = sal_False;
	pCurHighlightFrame = 0;
	StopEditTimer();
	CancelUserEvents();
	ShowCursor( sal_False );
	bBoundRectsDirty = sal_False;
	nMaxBoundHeight = 0;

	nFlags &= ~(F_PAINTED | F_MOVED_ENTRIES);
	pCursor = 0;
	if( !bInCtor )
	{
		pImpCursor->Clear();
		pGridMap->Clear();
		aVirtOutputSize.Width() = 0;
		aVirtOutputSize.Height() = 0;
		Size aSize( pView->GetOutputSizePixel() );
		nMaxVirtWidth = aSize.Width() - nVerSBarWidth;
		if( nMaxVirtWidth <= 0 )
			nMaxVirtWidth = DEFAULT_MAX_VIRT_WIDTH;
		nMaxVirtHeight = aSize.Height() - nHorSBarHeight;
		if( nMaxVirtHeight <= 0 )
			nMaxVirtHeight = DEFAULT_MAX_VIRT_HEIGHT;
		pZOrderList->Clear(); //Remove(0,pZOrderList->Count());
		SetOrigin( Point() );
		if( bUpdateMode )
			pView->Invalidate(INVALIDATE_NOCHILDREN);
	}
	AdjustScrollBars();
	sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pCur = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		delete pCur;
	}
	aEntries.Clear();
	DocRectChanged();
	VisRectChanged();
}

void SvxIconChoiceCtrl_Impl::SetStyle( WinBits nWinStyle )
{
	nWinBits = nWinStyle;
	nCurTextDrawFlags = DRAWTEXT_FLAGS_ICON;
	if( nWinBits & (WB_SMALLICON | WB_DETAILS) )
		nCurTextDrawFlags = DRAWTEXT_FLAGS_SMALLICON;
	if( nWinBits & WB_NOSELECTION )
		eSelectionMode = NO_SELECTION;
	if( !(nWinStyle & (WB_ALIGN_TOP | WB_ALIGN_LEFT)))
		nWinBits |= WB_ALIGN_LEFT;
	if( (nWinStyle & WB_DETAILS))
	{
		if( !pColumns  )
			SetColumn( 0, SvxIconChoiceCtrlColumnInfo( 0, 100, IcnViewAlignLeft ));
	}
}

IMPL_LINK( SvxIconChoiceCtrl_Impl, ScrollUpDownHdl, ScrollBar*, pScrollBar )
{
	StopEntryEditing( sal_True );
	// Pfeil hoch: delta=-1; Pfeil runter: delta=+1
	Scroll( 0, pScrollBar->GetDelta(), sal_True );
	bEndScrollInvalidate = sal_True;
	return 0;
}

IMPL_LINK( SvxIconChoiceCtrl_Impl, ScrollLeftRightHdl, ScrollBar*, pScrollBar )
{
	StopEntryEditing( sal_True );
	// Pfeil links: delta=-1; Pfeil rechts: delta=+1
	Scroll( pScrollBar->GetDelta(), 0, sal_True );
	bEndScrollInvalidate = sal_True;
	return 0;
}

IMPL_LINK( SvxIconChoiceCtrl_Impl, EndScrollHdl, void*, EMPTYARG )
{
	if( pView->HasBackground() && !pView->GetBackground().IsScrollable() &&
		bEndScrollInvalidate )
	{
		pView->Invalidate(INVALIDATE_NOCHILDREN);
	}
	return 0;
}

void SvxIconChoiceCtrl_Impl::FontModified()
{
	StopEditTimer();
	DELETEZ(pDDDev);
	DELETEZ(pDDBufDev);
	DELETEZ(pDDTempDev);
	DELETEZ(pEntryPaintDev);
	SetDefaultTextSize();
	ShowCursor( sal_False );
	ShowCursor( sal_True );
}

void SvxIconChoiceCtrl_Impl::InsertEntry( SvxIconChoiceCtrlEntry* pEntry, sal_uLong nPos,
	const Point* pPos )
{
	StopEditTimer();
	aEntries.Insert( pEntry, nPos );
	if( (nFlags & F_ENTRYLISTPOS_VALID) && nPos >= aEntries.Count() - 1 )
		pEntry->nPos = aEntries.Count() - 1;
	else
		nFlags &= ~F_ENTRYLISTPOS_VALID;

	pZOrderList->Insert( (void*)pEntry, LIST_APPEND ); //pZOrderList->Count() );
	pImpCursor->Clear();
//	pGridMap->Clear();
	if( pPos )
	{
		Size aSize( CalcBoundingSize( pEntry ) );
		SetBoundingRect_Impl( pEntry, *pPos, aSize );
		SetEntryPos( pEntry, *pPos, sal_False, sal_True, sal_True /*keep grid map*/ );
		pEntry->nFlags |= ICNVIEW_FLAG_POS_MOVED;
		SetEntriesMoved( sal_True );
	}
	else
	{
		// wenn der UpdateMode sal_True ist, wollen wir nicht pauschal alle
		// BoundRects auf 'zu ueberpruefen' setzen, sondern nur das des
		// neuen Eintrags. Deshalb kein InvalidateBoundingRect aufrufen!
		pEntry->aRect.Right() = LONG_MAX;
		if( bUpdateMode )
		{
			FindBoundingRect( pEntry );
			Rectangle aOutputArea( GetOutputRect() );
			pGridMap->OccupyGrids( pEntry );
			if( !aOutputArea.IsOver( pEntry->aRect ) )
				return;	// ist nicht sichtbar
			pView->Invalidate( pEntry->aRect );
		}
		else
			InvalidateBoundingRect( pEntry->aRect );
	}
}

void SvxIconChoiceCtrl_Impl::CreateAutoMnemonics( MnemonicGenerator* _pGenerator )
{
    ::std::auto_ptr< MnemonicGenerator > pAutoDeleteOwnGenerator;
    if ( !_pGenerator )
    {
        _pGenerator = new MnemonicGenerator;
        pAutoDeleteOwnGenerator.reset( _pGenerator );
    }

    sal_uLong   nEntryCount = GetEntryCount();
	sal_uLong   i;

	// insert texts in generator
	for( i = 0; i < nEntryCount; ++i )
	{
		DBG_ASSERT( GetEntry( i ), "-SvxIconChoiceCtrl_Impl::CreateAutoMnemonics(): more expected than provided!" );

		_pGenerator->RegisterMnemonic( GetEntry( i )->GetText() );
	}

	// exchange texts with generated mnemonics
	for( i = 0; i < nEntryCount; ++i )
	{
		SvxIconChoiceCtrlEntry*	pEntry = GetEntry( i );
		String					aTxt = pEntry->GetText();

		if( _pGenerator->CreateMnemonic( aTxt ) )
			pEntry->SetText( aTxt );
	}
}

Rectangle SvxIconChoiceCtrl_Impl::GetOutputRect() const
{
	Point aOrigin( pView->GetMapMode().GetOrigin() );
	aOrigin *= -1;
	return Rectangle( aOrigin, aOutputSize );
}

void SvxIconChoiceCtrl_Impl::SetListPositions()
{
	if( nFlags & F_ENTRYLISTPOS_VALID )
		return;

	sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		pEntry->nPos = nCur;
	}
	nFlags |= F_ENTRYLISTPOS_VALID;
}

void SvxIconChoiceCtrl_Impl::RemoveEntry( SvxIconChoiceCtrlEntry* pEntry )
{
	sal_Bool bSyncSingleSelection;
	// bei Single-Selection wird die Selektion beim Umsetzen des Cursors
	// mitgefuehrt. Das soll aber nur erfolgen, wenn ueberhaupt ein
	// Eintrag selektiert ist.
	if( GetSelectionCount() )
		bSyncSingleSelection = sal_True;
	else
		bSyncSingleSelection = sal_False;

	if( pEntry == pCurHighlightFrame )
		pCurHighlightFrame = 0;

	if( bInDragDrop )
	{
		DELETEZ(pDraggedSelection);
		bInDragDrop = sal_False;
	}

	if( pEntry->IsSelected() )
		CallSelectHandler( 0 );

	if( aEntries.Count() == 1 && aEntries.GetObject(0) == pEntry )
	{
		Clear();
		return;
	}

	StopEditTimer();
	if( pEntry == pAnchor )
		pAnchor = 0;
	if( pEntry->IsSelected() )
		nSelectionCount--;
	sal_Bool bEntryBoundValid = IsBoundingRectValid( pEntry->aRect );
	if( bEntryBoundValid )
		pView->Invalidate( pEntry->aRect );

	sal_Bool bSetNewCursor = sal_False;
	SvxIconChoiceCtrlEntry* pNewCursor = NULL;

	if( pEntry == pCursor )
	{
		bSetNewCursor = sal_True;
		pNewCursor = FindNewCursor();
		ShowCursor( sal_False );
		pCursor = 0;
	}

	sal_Bool bCurEntryPosValid = (nFlags & F_ENTRYLISTPOS_VALID) ? sal_True : sal_False;
	if( bCurEntryPosValid && aEntries.GetObject(aEntries.Count()-1) != pEntry )
		nFlags &= ~F_ENTRYLISTPOS_VALID;
	sal_uLong nPos = pZOrderList->GetPos( (void*)pEntry );
	pZOrderList->Remove( nPos );
	if( bCurEntryPosValid )
	{
		DBG_ASSERT(aEntries.GetObject(pEntry->nPos)==pEntry,"RemoveEntry: Wrong nPos in entry");
		aEntries.Remove( pEntry->nPos );
	}
	else
		aEntries.Remove( pEntry );
	pImpCursor->Clear();
	pGridMap->Clear();
	delete pEntry;
	if( IsAutoArrange() && aEntries.Count() )
		aAutoArrangeTimer.Start();
	if( bSetNewCursor )
	{
		// Fokusrechteck asynchron einblenden, um das Loeschen einer
		// Multiselektion zu beschleunigen.
		SetCursor( pNewCursor, bSyncSingleSelection, sal_True );
	}
}

void SvxIconChoiceCtrl_Impl::SelectEntry( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bSelect,
	sal_Bool bCallHdl, sal_Bool bAdd, sal_Bool bSyncPaint )
{
	if( eSelectionMode == NO_SELECTION )
		return;

	if( !bAdd )
	{
		if ( 0 == ( nFlags & F_CLEARING_SELECTION ) )
		{
			nFlags |= F_CLEARING_SELECTION;
			DeselectAllBut( pEntry, sal_True );
			nFlags &= ~F_CLEARING_SELECTION;
		}
	}
	if( pEntry->IsSelected() != bSelect )
	{
		pHdlEntry = pEntry;
		sal_uInt16 nEntryFlags = pEntry->GetFlags();
		if( bSelect )
		{
			nEntryFlags |= ICNVIEW_FLAG_SELECTED;
			pEntry->AssignFlags( nEntryFlags );
			nSelectionCount++;
			if( bCallHdl )
				CallSelectHandler( pEntry );
		}
		else
		{
			nEntryFlags &= ~( ICNVIEW_FLAG_SELECTED);
			pEntry->AssignFlags( nEntryFlags );
			nSelectionCount--;
			if( bCallHdl )
				CallSelectHandler( 0 );
		}
		EntrySelected( pEntry, bSelect, bSyncPaint );
	}
}

void SvxIconChoiceCtrl_Impl::EntrySelected( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bSelect,
	sal_Bool bSyncPaint )
{
	// bei SingleSelection dafuer sorgen, dass der Cursor immer
	// auf dem (einzigen) selektierten Eintrag steht. Aber nur,
	// wenn es bereits einen Cursor gibt
	if( bSelect && pCursor &&
		eSelectionMode == SINGLE_SELECTION &&
		pEntry != pCursor )
	{
		SetCursor( pEntry );
		//DBG_ASSERT(pView->GetSelectionCount()==1,"selection count?")
	}

	// beim Aufziehen nicht, da sonst die Schleife in SelectRect
	// nicht richtig funktioniert!
	if( !(nFlags & F_SELECTING_RECT) )
		ToTop( pEntry );
	if( bUpdateMode )
	{
		if( pEntry == pCursor )
			ShowCursor( sal_False );
		if( pView->IsTracking() && (bSelect || !pView->HasBackground()) ) // beim Tracken immer synchron
			PaintEntry( pEntry );
		else if( bSyncPaint ) // synchron & mit virtuellem OutDev!
			PaintEntryVirtOutDev( pEntry );
		else
		{
			pView->Invalidate( CalcFocusRect( pEntry ) );
		}
		if( pEntry == pCursor )
			ShowCursor( sal_True );
	} // if( bUpdateMode )

    // --> OD 2009-05-27 #i101012#
    // emit vcl event LISTBOX_SELECT only in case that the given entry is selected.
    if ( bSelect )
    {
        CallEventListeners( VCLEVENT_LISTBOX_SELECT, pEntry );
    }
    // <--
}

void SvxIconChoiceCtrl_Impl::ResetVirtSize()
{
	StopEditTimer();
	aVirtOutputSize.Width() = 0;
	aVirtOutputSize.Height() = 0;
	sal_Bool bLockedEntryFound = sal_False;
	const sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pCur = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		pCur->ClearFlags( ICNVIEW_FLAG_POS_MOVED );
		if( pCur->IsPosLocked() )
		{
			// VirtSize u.a. anpassen
			if( !IsBoundingRectValid( pCur->aRect ) )
				FindBoundingRect( pCur );
			else
				AdjustVirtSize( pCur->aRect );
			bLockedEntryFound = sal_True;
		}
		else
			InvalidateBoundingRect( pCur->aRect );
	}

	if( !(nWinBits & (WB_NOVSCROLL | WB_NOHSCROLL)) )
	{
		Size aRealOutputSize( pView->GetOutputSizePixel() );
		if( aVirtOutputSize.Width() < aRealOutputSize.Width() ||
			aVirtOutputSize.Height() < aRealOutputSize.Height() )
		{
			sal_uLong nGridCount = IcnGridMap_Impl::GetGridCount(
				aRealOutputSize, (sal_uInt16)nGridDX, (sal_uInt16)nGridDY );
			if( nGridCount < nCount )
			{
				if( nWinBits & WB_ALIGN_TOP )
					nMaxVirtWidth = aRealOutputSize.Width() - nVerSBarWidth;
				else // WB_ALIGN_LEFT
					nMaxVirtHeight = aRealOutputSize.Height() - nHorSBarHeight;
			}
		}
	}

	pImpCursor->Clear();
	pGridMap->Clear();
	VisRectChanged();
}

void SvxIconChoiceCtrl_Impl::AdjustVirtSize( const Rectangle& rRect )
{
	long nHeightOffs = 0;
	long nWidthOffs = 0;

	if( aVirtOutputSize.Width() < (rRect.Right()+LROFFS_WINBORDER) )
		nWidthOffs = (rRect.Right()+LROFFS_WINBORDER) - aVirtOutputSize.Width();

	if( aVirtOutputSize.Height() < (rRect.Bottom()+TBOFFS_WINBORDER) )
		nHeightOffs = (rRect.Bottom()+TBOFFS_WINBORDER) - aVirtOutputSize.Height();

	if( nWidthOffs || nHeightOffs )
	{
		Range aRange;
		aVirtOutputSize.Width() += nWidthOffs;
		aRange.Max() = aVirtOutputSize.Width();
		aHorSBar.SetRange( aRange );

		aVirtOutputSize.Height() += nHeightOffs;
		aRange.Max() = aVirtOutputSize.Height();
		aVerSBar.SetRange( aRange );

		pImpCursor->Clear();
		pGridMap->OutputSizeChanged();
		AdjustScrollBars();
		DocRectChanged();
	}
}

void SvxIconChoiceCtrl_Impl::InitPredecessors()
{
	DBG_ASSERT(!pHead,"SvxIconChoiceCtrl_Impl::InitPredecessors() >> Already initialized");
	sal_uLong nCount = aEntries.Count();
	if( nCount )
	{
		SvxIconChoiceCtrlEntry* pPrev = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( 0 );
		for( sal_uLong nCur = 1; nCur <= nCount; nCur++ )
		{
			pPrev->ClearFlags( ICNVIEW_FLAG_POS_LOCKED | ICNVIEW_FLAG_POS_MOVED |
								ICNVIEW_FLAG_PRED_SET);

			SvxIconChoiceCtrlEntry* pNext;
			if( nCur == nCount )
				pNext = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( 0 );
			else
				pNext = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			pPrev->pflink = pNext;
			pNext->pblink = pPrev;
			pPrev = pNext;
		}
		pHead = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( 0 );
	}
	else
		pHead = 0;
	nFlags &= ~F_MOVED_ENTRIES;
}

void SvxIconChoiceCtrl_Impl::ClearPredecessors()
{
	if( pHead )
	{
		sal_uLong nCount = aEntries.Count();
		for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pCur = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			pCur->pflink = 0;
			pCur->pblink = 0;
			pCur->ClearFlags( ICNVIEW_FLAG_PRED_SET );
		}
		pHead = 0;
	}
}

void SvxIconChoiceCtrl_Impl::Arrange( sal_Bool bKeepPredecessors, long nSetMaxVirtWidth, long nSetMaxVirtHeight )
{
	if ( nSetMaxVirtWidth != 0 )
		nMaxVirtWidth = nSetMaxVirtWidth;
	else
		nMaxVirtWidth = aOutputSize.Width();

	if ( nSetMaxVirtHeight != 0 )
		nMaxVirtHeight = nSetMaxVirtHeight;
	else
		nMaxVirtHeight = aOutputSize.Height();

	ImpArrange( bKeepPredecessors );
}

void SvxIconChoiceCtrl_Impl::ImpArrange( sal_Bool bKeepPredecessors )
{
	static Point aEmptyPoint;

	sal_Bool bOldUpdate = bUpdateMode;
	Rectangle aCurOutputArea( GetOutputRect() );
	if( (nWinBits & WB_SMART_ARRANGE) && aCurOutputArea.TopLeft() != aEmptyPoint )
		bUpdateMode = sal_False;
	aAutoArrangeTimer.Stop();
	nFlags &= ~F_MOVED_ENTRIES;
	nFlags |= F_ARRANGING;
	StopEditTimer();
	ShowCursor( sal_False );
	ResetVirtSize();
	if( !bKeepPredecessors )
		ClearPredecessors();
	bBoundRectsDirty = sal_False;
	SetOrigin( Point() );
	VisRectChanged();
	RecalcAllBoundingRectsSmart();
	// in der Detailsview muss das Invalidieren intelligenter erfolgen
	//if( !(nWinBits & WB_DETAILS ))
		pView->Invalidate( INVALIDATE_NOCHILDREN );
	nFlags &= ~F_ARRANGING;
	if( (nWinBits & WB_SMART_ARRANGE) && aCurOutputArea.TopLeft() != aEmptyPoint )
	{
		MakeVisible( aCurOutputArea );
		SetUpdateMode( bOldUpdate );
	}
	ShowCursor( sal_True );
}

void SvxIconChoiceCtrl_Impl::Paint( const Rectangle& rRect )
{
	bEndScrollInvalidate = sal_False;

#if defined(OV_DRAWGRID)
	Color aOldColor ( pView->GetLineColor() );
	Color aColor( COL_BLACK );
	pView->SetLineColor( aColor );
	Point aOffs( pView->GetMapMode().GetOrigin());
	Size aXSize( pView->GetOutputSizePixel() );

	{
	Point aStart( LROFFS_WINBORDER, 0 );
	Point aEnd( LROFFS_WINBORDER, aXSize.Height());
	aStart -= aOffs;
	aEnd -= aOffs;
	pView->DrawLine( aStart, aEnd );
	}
	{
	Point aStart( 0, TBOFFS_WINBORDER );
	Point aEnd( aXSize.Width(), TBOFFS_WINBORDER );
	aStart -= aOffs;
	aEnd -= aOffs;
	pView->DrawLine( aStart, aEnd );
	}

	for( long nDX = nGridDX; nDX <= aXSize.Width(); nDX += nGridDX )
	{
		Point aStart( nDX+LROFFS_WINBORDER, 0 );
		Point aEnd( nDX+LROFFS_WINBORDER, aXSize.Height());
		aStart -= aOffs;
		aEnd -= aOffs;
		pView->DrawLine( aStart, aEnd );
	}
	for( long nDY = nGridDY; nDY <= aXSize.Height(); nDY += nGridDY )
	{
		Point aStart( 0, nDY+TBOFFS_WINBORDER );
		Point aEnd( aXSize.Width(), nDY+TBOFFS_WINBORDER );
		aStart -= aOffs;
		aEnd -= aOffs;
		pView->DrawLine( aStart, aEnd );
	}
	pView->SetLineColor( aOldColor );
#endif
	nFlags |= F_PAINTED;

	if( !aEntries.Count() )
		return;
	if( !pCursor )
	{
		// set cursor to item with focus-flag
		sal_Bool bfound = sal_False;
		for ( sal_uLong i = 0; i < pView->GetEntryCount() && !bfound; i++)
		{
			SvxIconChoiceCtrlEntry* pEntry = pView->GetEntry ( i );
			if( pEntry->IsFocused() )
			{
				pCursor = pEntry;
				bfound=sal_True;
			}
		}

		if( !bfound )
			pCursor = (SvxIconChoiceCtrlEntry*)aEntries.First();
	}

	// Show Focus at Init-Time
	if ( pView->HasFocus() )
		GetFocus();

	sal_uLong nCount = pZOrderList->Count();
	if( !nCount )
		return;

	sal_Bool bResetClipRegion = sal_False;
	if( !pView->IsClipRegion() )
	{
		Rectangle aOutputArea( GetOutputRect() );
		bResetClipRegion = sal_True;
		pView->SetClipRegion( aOutputArea );
	}

	const sal_uInt16 nListInitSize = aEntries.Count() > USHRT_MAX ?
		USHRT_MAX : (sal_uInt16)aEntries.Count();
	List* pNewZOrderList = new List( nListInitSize );
	List* pPaintedEntries = new List( nListInitSize );

	sal_uLong nPos = 0;
	while( nCount )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nPos ));
		const Rectangle& rBoundRect = GetEntryBoundRect( pEntry );
		if( rRect.IsOver( rBoundRect ) )
		{
			PaintEntry( pEntry, rBoundRect.TopLeft(), pView, sal_True );
			// Eintraege, die neu gezeichnet werden, auf Top setzen
			pPaintedEntries->Insert( pEntry, LIST_APPEND );
		}
		else
			pNewZOrderList->Insert( pEntry, LIST_APPEND );

		nCount--;
		nPos++;
	}
	delete pZOrderList;
	pZOrderList = pNewZOrderList;
	nCount = pPaintedEntries->Count();
	if( nCount )
	{
		for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
			pZOrderList->Insert( pPaintedEntries->GetObject(nCur), LIST_APPEND);
	}
	delete pPaintedEntries;

	if( bResetClipRegion )
		pView->SetClipRegion();
}

void SvxIconChoiceCtrl_Impl::RepaintEntries( sal_uInt16 nEntryFlagsMask )
{
	const sal_uLong nCount = pZOrderList->Count();
	if( !nCount )
		return;

	sal_Bool bResetClipRegion = sal_False;
	Rectangle aOutRect( GetOutputRect() );
	if( !pView->IsClipRegion() )
	{
		bResetClipRegion = sal_True;
		pView->SetClipRegion( aOutRect );
	}
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nCur));
		if( pEntry->GetFlags() & nEntryFlagsMask )
		{
			const Rectangle& rBoundRect = GetEntryBoundRect( pEntry );
			if( aOutRect.IsOver( rBoundRect ) )
				PaintEntry( pEntry, rBoundRect.TopLeft() );
		}
	}
	if( bResetClipRegion )
		pView->SetClipRegion();
}


void SvxIconChoiceCtrl_Impl::InitScrollBarBox()
{
	aScrBarBox.SetSizePixel( Size(nVerSBarWidth-1, nHorSBarHeight-1) );
	Size aSize( pView->GetOutputSizePixel() );
	aScrBarBox.SetPosPixel( Point(aSize.Width()-nVerSBarWidth+1, aSize.Height()-nHorSBarHeight+1));
}

IcnViewFieldType SvxIconChoiceCtrl_Impl::GetItem( SvxIconChoiceCtrlEntry* pEntry,
	const Point& rAbsPos )
{
	Rectangle aRect( CalcTextRect( pEntry ) );
	if( aRect.IsInside( rAbsPos ) )
		return IcnViewFieldTypeText;

	aRect = CalcBmpRect( pEntry );
	if( aRect.IsInside( rAbsPos ) )
		return IcnViewFieldTypeImage;

	return IcnViewFieldTypeDontknow;
}

sal_Bool SvxIconChoiceCtrl_Impl::MouseButtonDown( const MouseEvent& rMEvt)
{
	sal_Bool bHandled = sal_True;
	bHighlightFramePressed = sal_False;
	StopEditTimer();
	sal_Bool bGotFocus = (sal_Bool)(!pView->HasFocus() && !(nWinBits & WB_NOPOINTERFOCUS));
	if( !(nWinBits & WB_NOPOINTERFOCUS) )
		pView->GrabFocus();

	Point aDocPos( rMEvt.GetPosPixel() );
	if(aDocPos.X()>=aOutputSize.Width() || aDocPos.Y()>=aOutputSize.Height())
		return sal_False;
	ToDocPos( aDocPos );
	SvxIconChoiceCtrlEntry* pEntry = GetEntry( aDocPos, sal_True );
	if( pEntry )
		MakeEntryVisible( pEntry, sal_False );

	if( rMEvt.IsShift() && eSelectionMode != SINGLE_SELECTION )
	{
		if( pEntry )
			SetCursor_Impl( pCursor, pEntry, rMEvt.IsMod1(), rMEvt.IsShift(), sal_True);
		return sal_True;
	}

	if( pAnchor && (rMEvt.IsShift() || rMEvt.IsMod1())) // Tastaturselektion?
	{
		DBG_ASSERT(eSelectionMode != SINGLE_SELECTION,"Invalid selection mode");
		if( rMEvt.IsMod1() )
			nFlags |= F_ADD_MODE;

		if( rMEvt.IsShift() )
		{
			Rectangle aRect( GetEntryBoundRect( pAnchor ));
			if( pEntry )
				aRect.Union( GetEntryBoundRect( pEntry ) );
			else
			{
				Rectangle aTempRect( aDocPos, Size(1,1));
				aRect.Union( aTempRect );
			}
			aCurSelectionRect = aRect;
			SelectRect( aRect, (nFlags & F_ADD_MODE)!=0, &aSelectedRectList );
		}
		else if( rMEvt.IsMod1() )
		{
			AddSelectedRect( aCurSelectionRect );
			pAnchor = 0;
			aCurSelectionRect.SetPos( aDocPos );
		}

		if( !pEntry && !(nWinBits & WB_NODRAGSELECTION))
			pView->StartTracking( STARTTRACK_SCROLLREPEAT );
		return sal_True;
	}
	else
	{
		if( !pEntry )
		{
			if( eSelectionMode == MULTIPLE_SELECTION )
			{
				if( !rMEvt.IsMod1() )  // Ctrl
				{
					if( !bGotFocus )
					{
						SetNoSelection();
						ClearSelectedRectList();
					}
				}
				else
					nFlags |= F_ADD_MODE;
				aCurSelectionRect.SetPos( aDocPos );
				pView->StartTracking( STARTTRACK_SCROLLREPEAT );
			}
			else
				bHandled = sal_False;
			return bHandled;
		}
	}
	sal_Bool bSelected = pEntry->IsSelected();
	sal_Bool bEditingEnabled = IsEntryEditingEnabled();

	if( rMEvt.GetClicks() == 2 )
	{
		DeselectAllBut( pEntry );
		SelectEntry( pEntry, sal_True, sal_True, sal_False, sal_True );
		pHdlEntry = pEntry;
		pView->ClickIcon();
	}
	else
	{
		// Inplace-Editing ?
		if( rMEvt.IsMod2() )  // Alt?
		{
			if( bEntryEditingEnabled && pEntry &&
				pEntry->IsSelected())
			{
				if( pView->EditingEntry( pEntry ))
					EditEntry( pEntry );
			}
		}
		else if( eSelectionMode == SINGLE_SELECTION )
		{
			DeselectAllBut( pEntry );
			SetCursor( pEntry );
			if( bEditingEnabled && bSelected && !rMEvt.GetModifier() &&
				rMEvt.IsLeft() && IsTextHit( pEntry, aDocPos ) )
			{
				nFlags |= F_START_EDITTIMER_IN_MOUSEUP;
			}
		}
		else if( eSelectionMode == NO_SELECTION )
		{
			if( rMEvt.IsLeft() && (nWinBits & WB_HIGHLIGHTFRAME) )
			{
				pCurHighlightFrame = 0; // Neues painten des Frames erzwingen
				bHighlightFramePressed = sal_True;
				SetEntryHighlightFrame( pEntry, sal_True );
			}
		}
		else
		{
			if( !rMEvt.GetModifier() && rMEvt.IsLeft() )
			{
				if( !bSelected )
				{
					DeselectAllBut( pEntry, sal_True /* Synchron painten */ );
					SetCursor( pEntry );
					SelectEntry( pEntry, sal_True, sal_True, sal_False, sal_True );
				}
				else
				{
					// erst im Up deselektieren, falls Move per D&D!
					nFlags |= F_DOWN_DESELECT;
					if( bEditingEnabled && IsTextHit( pEntry, aDocPos ) &&
						rMEvt.IsLeft())
					{
						nFlags |= F_START_EDITTIMER_IN_MOUSEUP;
					}
				}
			}
			else if( rMEvt.IsMod1() )
				nFlags |= F_DOWN_CTRL;
		}
	}
	return bHandled;
}

sal_Bool SvxIconChoiceCtrl_Impl::MouseButtonUp( const MouseEvent& rMEvt )
{
	sal_Bool bHandled = sal_False;
	if( rMEvt.IsRight() && (nFlags & (F_DOWN_CTRL | F_DOWN_DESELECT) ))
	{
		nFlags &= ~(F_DOWN_CTRL | F_DOWN_DESELECT);
		bHandled = sal_True;
	}

	Point aDocPos( rMEvt.GetPosPixel() );
	ToDocPos( aDocPos );
	SvxIconChoiceCtrlEntry* pDocEntry = GetEntry( aDocPos );
	if( pDocEntry )
	{
		if( nFlags & F_DOWN_CTRL )
		{
			// Ctrl & MultiSelection
			ToggleSelection( pDocEntry );
			SetCursor( pDocEntry );
			bHandled = sal_True;
		}
		else if( nFlags & F_DOWN_DESELECT )
		{
			DeselectAllBut( pDocEntry );
			SetCursor( pDocEntry );
			SelectEntry( pDocEntry, sal_True, sal_True, sal_False, sal_True );
			bHandled = sal_True;
		}
	}

	nFlags &= ~(F_DOWN_CTRL | F_DOWN_DESELECT);
	if( nFlags & F_START_EDITTIMER_IN_MOUSEUP )
	{
		bHandled = sal_True;
		StartEditTimer();
		nFlags &= ~F_START_EDITTIMER_IN_MOUSEUP;
	}

	if((nWinBits & WB_HIGHLIGHTFRAME) && bHighlightFramePressed && pCurHighlightFrame)
	{
		bHandled = sal_True;
		SvxIconChoiceCtrlEntry* pEntry = pCurHighlightFrame;
		pCurHighlightFrame = 0; // Neues painten des Frames erzwingen
		bHighlightFramePressed = sal_False;
		SetEntryHighlightFrame( pEntry, sal_True );
#if 0
		CallSelectHandler( pCurHighlightFrame );
#else
		pHdlEntry = pCurHighlightFrame;
		pView->ClickIcon();

		// set focus on Icon
		SvxIconChoiceCtrlEntry* pOldCursor = pCursor;
		SetCursor_Impl( pOldCursor, pHdlEntry, sal_False, sal_False, sal_True );
#endif
		pHdlEntry = 0;
	}
	return bHandled;
}

sal_Bool SvxIconChoiceCtrl_Impl::MouseMove( const MouseEvent& rMEvt )
{
	const Point aDocPos( pView->PixelToLogic(rMEvt.GetPosPixel()) );

	if( pView->IsTracking() )
		return sal_False;
	else if( nWinBits & WB_HIGHLIGHTFRAME )
	{
		SvxIconChoiceCtrlEntry* pEntry = GetEntry( aDocPos, sal_True );
		SetEntryHighlightFrame( pEntry );
	}
	else
		return sal_False;
	return sal_True;
}

void SvxIconChoiceCtrl_Impl::Tracking( const TrackingEvent& rTEvt )
{
	if ( rTEvt.IsTrackingEnded() )
	{
		// Das Rechteck darf nicht "justified" sein, da seine
		// TopLeft-Position u.U. zur Berechnung eines Ankers
		// benutzt wird.
		AddSelectedRect( aCurSelectionRect );
		pView->HideTracking();
		nFlags &= ~(F_ADD_MODE);
		if( rTEvt.IsTrackingCanceled() )
			SetNoSelection();
	}
	else
	{
		Point aPosPixel = rTEvt.GetMouseEvent().GetPosPixel();
		Point aDocPos( aPosPixel );
		ToDocPos( aDocPos );

		long nScrollDX, nScrollDY;

		CalcScrollOffsets( aPosPixel, nScrollDX, nScrollDY, sal_False );
		if( nScrollDX || nScrollDY )
		{
			pView->HideTracking();
			pView->Scroll( nScrollDX, nScrollDY );
		}
		Rectangle aRect( aCurSelectionRect.TopLeft(), aDocPos );
		if( aRect != aCurSelectionRect )
		{
			pView->HideTracking();
			sal_Bool bAdd = (nFlags & F_ADD_MODE) ? sal_True : sal_False;
			SelectRect( aRect, bAdd, &aSelectedRectList );
		}
		pView->ShowTracking( aRect, SHOWTRACK_SMALL | SHOWTRACK_CLIP );
	}
}

void SvxIconChoiceCtrl_Impl::SetCursor_Impl( SvxIconChoiceCtrlEntry* pOldCursor,
	SvxIconChoiceCtrlEntry* pNewCursor, sal_Bool bMod1, sal_Bool bShift, sal_Bool bPaintSync )
{
	if( pNewCursor )
	{
		SvxIconChoiceCtrlEntry* pFilterEntry = 0;
		sal_Bool bDeselectAll = sal_False;
		if( eSelectionMode != SINGLE_SELECTION )
		{
			if( !bMod1 && !bShift )
				bDeselectAll = sal_True;
			else if( bShift && !bMod1 && !pAnchor )
			{
				bDeselectAll = sal_True;
				pFilterEntry = pOldCursor;
			}
		}
		if( bDeselectAll )
			DeselectAllBut( pFilterEntry, bPaintSync );
		ShowCursor( sal_False );
		MakeEntryVisible( pNewCursor );
		SetCursor( pNewCursor );
		if( bMod1 && !bShift )
		{
			if( pAnchor )
			{
				AddSelectedRect( pAnchor, pOldCursor );
				pAnchor = 0;
			}
		}
		else if( bShift )
		{
			if( !pAnchor )
				pAnchor = pOldCursor;
			if ( nWinBits & WB_ALIGN_LEFT )
				SelectRange( pAnchor, pNewCursor, (nFlags & F_ADD_MODE)!=0 );
			else
				SelectRect(pAnchor,pNewCursor,(nFlags & F_ADD_MODE)!=0,&aSelectedRectList);
		}
		else
		{
			SelectEntry( pCursor, sal_True, sal_True,  sal_False, bPaintSync );
			aCurSelectionRect = GetEntryBoundRect( pCursor );
			CallEventListeners( VCLEVENT_LISTBOX_SELECT, pCursor );
		}
	}
}

sal_Bool SvxIconChoiceCtrl_Impl::KeyInput( const KeyEvent& rKEvt )
{
	StopEditTimer();

	sal_Bool bMod2 = rKEvt.GetKeyCode().IsMod2();
	sal_Unicode cChar = rKEvt.GetCharCode();
	sal_uLong nPos = (sal_uLong)-1;
	if ( bMod2 && cChar && IsMnemonicChar( cChar, nPos ) )
	{
		// shortcut is clicked
		SvxIconChoiceCtrlEntry* pNewCursor = GetEntry( nPos );
		SvxIconChoiceCtrlEntry* pOldCursor = pCursor;
		if ( pNewCursor != pOldCursor )
			SetCursor_Impl( pOldCursor, pNewCursor, sal_False, sal_False, sal_False );
		return sal_True;
	}

	if ( bMod2 )
		// no actions with <ALT>
		return sal_False;

	sal_Bool bKeyUsed = sal_True;
	sal_Bool bMod1 = rKEvt.GetKeyCode().IsMod1();
	sal_Bool bShift = rKEvt.GetKeyCode().IsShift();

	if( eSelectionMode == SINGLE_SELECTION || eSelectionMode == NO_SELECTION)
	{
		bShift = sal_False;
		bMod1 = sal_False;
	}

	if( bMod1 )
		nFlags |= F_ADD_MODE;
	sal_Bool bDeselectAll = sal_False;
	if( eSelectionMode != SINGLE_SELECTION )
	{
		if( !bMod1 && !bShift )
			bDeselectAll = sal_True;
		if( bShift && !bMod1 && !pAnchor )
			bDeselectAll = sal_True;
	}

	SvxIconChoiceCtrlEntry* pNewCursor;
	SvxIconChoiceCtrlEntry* pOldCursor = pCursor;

	sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
	switch( nCode )
	{
		case KEY_UP:
		case KEY_PAGEUP:
			if( pCursor )
			{
				MakeEntryVisible( pCursor );
				if( nCode == KEY_UP )
					pNewCursor = pImpCursor->GoUpDown(pCursor,sal_False);
				else
					pNewCursor = pImpCursor->GoPageUpDown(pCursor,sal_False);
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );
				if( !pNewCursor )
				{
					Rectangle aRect( GetEntryBoundRect( pCursor ) );
					if( aRect.Top())
					{
						aRect.Bottom() -= aRect.Top();
						aRect.Top() = 0;
						MakeVisible( aRect );
					}
				}

				if ( bChooseWithCursor && pNewCursor != NULL )
				{
					pHdlEntry = pNewCursor;//GetCurEntry();
					pCurHighlightFrame = pHdlEntry;
					pView->ClickIcon();
					pCurHighlightFrame = NULL;
				}
			}
			break;

		case KEY_DOWN:
		case KEY_PAGEDOWN:
			if( pCursor )
			{
				if( nCode == KEY_DOWN )
					pNewCursor=pImpCursor->GoUpDown( pCursor,sal_True );
				else
					pNewCursor=pImpCursor->GoPageUpDown( pCursor,sal_True );
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );

				if ( bChooseWithCursor && pNewCursor != NULL)
				{
					pHdlEntry = pNewCursor;//GetCurEntry();
					pCurHighlightFrame = pHdlEntry;
					pView->ClickIcon();
					pCurHighlightFrame = NULL;
				}
			}
			break;

		case KEY_RIGHT:
			if( pCursor )
			{
				pNewCursor=pImpCursor->GoLeftRight(pCursor,sal_True );
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );
			}
			break;

		case KEY_LEFT:
			if( pCursor )
			{
				MakeEntryVisible( pCursor );
				pNewCursor = pImpCursor->GoLeftRight(pCursor,sal_False );
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );
				if( !pNewCursor )
				{
					Rectangle aRect( GetEntryBoundRect(pCursor));
					if( aRect.Left() )
					{
						aRect.Right() -= aRect.Left();
						aRect.Left() = 0;
						MakeVisible( aRect );
					}
				}
			}
			break;

// wird vom VCL-Tracking gesteuert
#if 0
		case KEY_ESCAPE:
			if( pView->IsTracking() )
			{
				HideSelectionRect();
				//SelectAll( sal_False );
				SetNoSelection();
				ClearSelectedRectList();
				nFlags &= ~F_TRACKING;
			}
			else
				bKeyUsed = sal_False;
			break;
#endif


		case KEY_F2:
			if( !bMod1 && !bShift )
				EditTimeoutHdl( 0 );
			else
				bKeyUsed = sal_False;
			break;

		case KEY_F8:
			if( rKEvt.GetKeyCode().IsShift() )
			{
				if( nFlags & F_ADD_MODE )
					nFlags &= (~F_ADD_MODE);
				else
					nFlags |= F_ADD_MODE;
			}
			else
				bKeyUsed = sal_False;
			break;

		case KEY_SPACE:
			if( pCursor && eSelectionMode != SINGLE_SELECTION )
			{
				if( !bMod1 )
				{
					//SelectAll( sal_False );
					SetNoSelection();
					ClearSelectedRectList();

					// click Icon with spacebar
					SetEntryHighlightFrame( GetCurEntry(), sal_True );
					pView->ClickIcon();
					pHdlEntry = pCurHighlightFrame;
					pCurHighlightFrame=0;
				}
				else
					ToggleSelection( pCursor );
			}
			break;

#ifdef DBG_UTIL
		case KEY_F10:
			if( rKEvt.GetKeyCode().IsShift() )
			{
				if( pCursor )
					pView->SetEntryTextMode( IcnShowTextFull, pCursor );
			}
			if( rKEvt.GetKeyCode().IsMod1() )
			{
				if( pCursor )
					pView->SetEntryTextMode( IcnShowTextShort, pCursor );
			}
			break;
#endif

		case KEY_ADD:
		case KEY_DIVIDE :
		case KEY_A:
			if( bMod1 && (eSelectionMode != SINGLE_SELECTION))
				SelectAll( sal_True );
			else
				bKeyUsed = sal_False;
			break;

		case KEY_SUBTRACT:
		case KEY_COMMA :
			if( bMod1 )
				SetNoSelection();
			else
				bKeyUsed = sal_False;
			break;

		case KEY_RETURN:
			if( bMod1 )
			{
				if( pCursor && bEntryEditingEnabled )
					/*pView->*/EditEntry( pCursor );
			}
			else
				bKeyUsed = sal_False;
			break;

		case KEY_END:
			if( pCursor )
			{
				pNewCursor = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( aEntries.Count() - 1 );
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );
			}
			break;

		case KEY_HOME:
			if( pCursor )
			{
				pNewCursor = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( 0 );
				SetCursor_Impl( pOldCursor, pNewCursor, bMod1, bShift, sal_True );
			}
			break;

		default:
			bKeyUsed = sal_False;

	}
	return bKeyUsed;
}

// Berechnet TopLeft der Scrollbars (nicht ihre Groessen!)
void SvxIconChoiceCtrl_Impl::PositionScrollBars( long nRealWidth, long nRealHeight )
{
	// hor scrollbar
	Point aPos( 0, nRealHeight );
	aPos.Y() -= nHorSBarHeight;

	if( aHorSBar.GetPosPixel() != aPos )
		aHorSBar.SetPosPixel( aPos );

	// ver scrollbar
	aPos.X() = nRealWidth; aPos.Y() = 0;
	aPos.X() -= nVerSBarWidth;
	aPos.X()++;
	aPos.Y()--;

	if( aVerSBar.GetPosPixel() != aPos )
		aVerSBar.SetPosPixel( aPos );
}

void SvxIconChoiceCtrl_Impl::AdjustScrollBars( sal_Bool )
{
	Rectangle aOldOutRect( GetOutputRect() );
	long nVirtHeight = aVirtOutputSize.Height();
	long nVirtWidth = aVirtOutputSize.Width();

	Size aOSize( pView->Control::GetOutputSizePixel() );
	long nRealHeight = aOSize.Height();
	long nRealWidth = aOSize.Width();

	PositionScrollBars( nRealWidth, nRealHeight );

	const MapMode& rMapMode = pView->GetMapMode();
	Point aOrigin( rMapMode.GetOrigin() );

	long nVisibleWidth;
	if( nRealWidth > nVirtWidth )
		nVisibleWidth = nVirtWidth + aOrigin.X();
	else
		nVisibleWidth = nRealWidth;

	long nVisibleHeight;
	if( nRealHeight > nVirtHeight )
		nVisibleHeight = nVirtHeight + aOrigin.Y();
	else
		nVisibleHeight = nRealHeight;

	sal_Bool bVerSBar = ( nWinBits & WB_VSCROLL ) != 0;
	sal_Bool bHorSBar = ( nWinBits & WB_HSCROLL ) != 0;
	sal_Bool bNoVerSBar = ( nWinBits & WB_NOVSCROLL ) != 0;
	sal_Bool bNoHorSBar = ( nWinBits & WB_NOHSCROLL ) != 0;

	sal_uInt16 nResult = 0;
	if( nVirtHeight )
	{
		// activate ver scrollbar ?
		if( !bNoVerSBar && (bVerSBar || ( nVirtHeight > nVisibleHeight)) )
		{
			nResult = 0x0001;
			nRealWidth -= nVerSBarWidth;

			if( nRealWidth > nVirtWidth )
				nVisibleWidth = nVirtWidth + aOrigin.X();
			else
				nVisibleWidth = nRealWidth;

			nFlags |= F_HOR_SBARSIZE_WITH_VBAR;
		}
		// activate hor scrollbar ?
		if( !bNoHorSBar && (bHorSBar || (nVirtWidth > nVisibleWidth)) )
		{
			nResult |= 0x0002;
			nRealHeight -= nHorSBarHeight;

			if( nRealHeight > nVirtHeight )
				nVisibleHeight = nVirtHeight + aOrigin.Y();
			else
				nVisibleHeight = nRealHeight;

			// brauchen wir jetzt doch eine senkrechte Scrollbar ?
			if( !(nResult & 0x0001) &&  // nur wenn nicht schon da
				( !bNoVerSBar && ((nVirtHeight > nVisibleHeight) || bVerSBar)) )
			{
				nResult = 3; // beide sind an
				nRealWidth -= nVerSBarWidth;

				if( nRealWidth > nVirtWidth )
					nVisibleWidth = nVirtWidth + aOrigin.X();
				else
					nVisibleWidth = nRealWidth;

				nFlags |= F_VER_SBARSIZE_WITH_HBAR;
			}
		}
	}

	// size ver scrollbar
	long nThumb = aVerSBar.GetThumbPos();
	Size aSize( nVerSBarWidth, nRealHeight );
	aSize.Height() += 2;
	if( aSize != aVerSBar.GetSizePixel() )
		aVerSBar.SetSizePixel( aSize );
	aVerSBar.SetVisibleSize( nVisibleHeight );
	aVerSBar.SetPageSize( GetScrollBarPageSize( nVisibleHeight ));

	if( nResult & 0x0001 )
	{
		aVerSBar.SetThumbPos( nThumb );
		aVerSBar.Show();
	}
	else
	{
		aVerSBar.SetThumbPos( 0 );
		aVerSBar.Hide();
	}

	// size hor scrollbar
	nThumb = aHorSBar.GetThumbPos();
	aSize.Width() = nRealWidth;
	aSize.Height() = nHorSBarHeight;
	aSize.Width()++;
	if( nResult & 0x0001 ) // vertikale Scrollbar ?
	{
		aSize.Width()++;
		nRealWidth++;
	}
	if( aSize != aHorSBar.GetSizePixel() )
		aHorSBar.SetSizePixel( aSize );
	aHorSBar.SetVisibleSize( nVisibleWidth );
	aHorSBar.SetPageSize( GetScrollBarPageSize(nVisibleWidth ));
	if( nResult & 0x0002 )
	{
		aHorSBar.SetThumbPos( nThumb );
		aHorSBar.Show();
	}
	else
	{
		aHorSBar.SetThumbPos( 0 );
		aHorSBar.Hide();
	}

	aOutputSize.Width() = nRealWidth;
	if( nResult & 0x0002 ) // hor scrollbar ?
		nRealHeight++; // weil unterer Rand geclippt wird
	aOutputSize.Height() = nRealHeight;

	Rectangle aNewOutRect( GetOutputRect() );
	if( aNewOutRect != aOldOutRect && pView->HasBackground() )
	{
		Wallpaper aPaper( pView->GetBackground() );
		aPaper.SetRect( aNewOutRect );
		pView->SetBackground( aPaper );
	}

	if( (nResult & (0x0001|0x0002)) == (0x0001|0x0002) )
		aScrBarBox.Show();
	else
		aScrBarBox.Hide();
}

void SvxIconChoiceCtrl_Impl::Resize()
{
	StopEditTimer();
	InitScrollBarBox();
	aOutputSize = pView->GetOutputSizePixel();
	pImpCursor->Clear();
	pGridMap->OutputSizeChanged();

	const Size& rSize = pView->Control::GetOutputSizePixel();
	PositionScrollBars( rSize.Width(), rSize.Height() );
	// Die ScrollBars werden asynchron ein/ausgeblendet, damit abgeleitete
	// Klassen im Resize ein Arrange durchfuehren koennen, ohne dass
	// die ScrollBars aufblitzen
	// Wenn schon ein Event unterwegs ist, dann braucht kein neues verschickt werden,
	// zumindest, solange es nur einen EventTypen gibt
	if ( ! nUserEventAdjustScrBars )
		nUserEventAdjustScrBars =
			Application::PostUserEvent( LINK( this, SvxIconChoiceCtrl_Impl, UserEventHdl),
				EVENTID_ADJUST_SCROLLBARS);

	if( pView->HasBackground() && !pView->GetBackground().IsScrollable() )
	{
		Rectangle aRect( GetOutputRect());
		Wallpaper aPaper( pView->GetBackground() );
		aPaper.SetRect( aRect );
		pView->SetBackground( aPaper );
	}
	VisRectChanged();
}

sal_Bool SvxIconChoiceCtrl_Impl::CheckHorScrollBar()
{
	if( !pZOrderList || !aHorSBar.IsVisible() )
		return sal_False;
	const MapMode& rMapMode = pView->GetMapMode();
	Point aOrigin( rMapMode.GetOrigin() );
	if(!( nWinBits & WB_HSCROLL) && !aOrigin.X() )
	{
		long nWidth = aOutputSize.Width();
		const sal_uLong nCount = pZOrderList->Count();
		long nMostRight = 0;
		for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)pZOrderList->GetObject(nCur);
			long nRight = GetEntryBoundRect(pEntry).Right();
			if( nRight > nWidth )
				return sal_False;
			if( nRight > nMostRight )
				nMostRight = nRight;
		}
		aHorSBar.Hide();
		aOutputSize.Height() += nHorSBarHeight;
		aVirtOutputSize.Width() = nMostRight;
		aHorSBar.SetThumbPos( 0 );
		Range aRange;
		aRange.Max() = nMostRight - 1;
		aHorSBar.SetRange( aRange  );
		if( aVerSBar.IsVisible() )
		{
			Size aSize( aVerSBar.GetSizePixel());
			aSize.Height() += nHorSBarHeight;
			aVerSBar.SetSizePixel( aSize );
		}
		return sal_True;
	}
	return sal_False;
}

sal_Bool SvxIconChoiceCtrl_Impl::CheckVerScrollBar()
{
	if( !pZOrderList || !aVerSBar.IsVisible() )
		return sal_False;
	const MapMode& rMapMode = pView->GetMapMode();
	Point aOrigin( rMapMode.GetOrigin() );
	if(!( nWinBits & WB_VSCROLL) && !aOrigin.Y() )
	{
		long nDeepest = 0;
		long nHeight = aOutputSize.Height();
		const sal_uLong nCount = pZOrderList->Count();
		for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)pZOrderList->GetObject(nCur);
			long nBottom = GetEntryBoundRect(pEntry).Bottom();
			if( nBottom > nHeight )
				return sal_False;
			if( nBottom > nDeepest )
				nDeepest = nBottom;
		}
		aVerSBar.Hide();
		aOutputSize.Width() += nVerSBarWidth;
		aVirtOutputSize.Height() = nDeepest;
		aVerSBar.SetThumbPos( 0 );
		Range aRange;
		aRange.Max() = nDeepest - 1;
		aVerSBar.SetRange( aRange  );
		if( aHorSBar.IsVisible() )
		{
			Size aSize( aHorSBar.GetSizePixel());
			aSize.Width() += nVerSBarWidth;
			aHorSBar.SetSizePixel( aSize );
		}
		return sal_True;
	}
	return sal_False;
}


// blendet Scrollbars aus, wenn sie nicht mehr benoetigt werden
void SvxIconChoiceCtrl_Impl::CheckScrollBars()
{
	CheckVerScrollBar();
	if( CheckHorScrollBar() )
		CheckVerScrollBar();
	if( aVerSBar.IsVisible() && aHorSBar.IsVisible() )
		aScrBarBox.Show();
	else
		aScrBarBox.Hide();
}


void SvxIconChoiceCtrl_Impl::GetFocus()
{
	RepaintEntries( ICNVIEW_FLAG_SELECTED );
	if( pCursor )
	{
		pCursor->SetFlags( ICNVIEW_FLAG_FOCUSED );
		ShowCursor( sal_True );
	}
}

void SvxIconChoiceCtrl_Impl::LoseFocus()
{
	StopEditTimer();
	if( pCursor )
		pCursor->ClearFlags( ICNVIEW_FLAG_FOCUSED );
	ShowCursor( sal_False );

//	HideFocus ();
//	pView->Invalidate ( aFocus.aRect );

	RepaintEntries( ICNVIEW_FLAG_SELECTED );
}

void SvxIconChoiceCtrl_Impl::SetUpdateMode( sal_Bool bUpdate )
{
	if( bUpdate != bUpdateMode )
	{
		bUpdateMode = bUpdate;
		if( bUpdate )
		{
			AdjustScrollBars();
			pImpCursor->Clear();
			pGridMap->Clear();
			pView->Invalidate(INVALIDATE_NOCHILDREN);
		}
	}
}

void SvxIconChoiceCtrl_Impl::PaintEntry( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bIsBackgroundPainted )
{
	Point aPos( GetEntryPos( pEntry ) );
	PaintEntry( pEntry, aPos, 0, bIsBackgroundPainted );
}

// Prios der Emphasis:  bDropTarget => bCursored => bSelected
void SvxIconChoiceCtrl_Impl::PaintEmphasis(
	const Rectangle& rTextRect, const Rectangle& rImageRect,
	sal_Bool bSelected, sal_Bool bDropTarget, sal_Bool bCursored, OutputDevice* pOut,
	sal_Bool bIsBackgroundPainted )
{
	static Color aTransparent( COL_TRANSPARENT );

	if( !pOut )
		pOut = pView;

#ifdef OV_CHECK_EMPH_RECTS
	{
		Color aXOld( pOut->GetFillColor() );
		pOut->SetFillColor( Color( COL_GREEN ));
		pOut->DrawRect( rTextRect );
		pOut->DrawRect( rImageRect );
		pOut->SetFillColor( aXOld );
	}
#endif

	const StyleSettings& rSettings = pOut->GetSettings().GetStyleSettings();
	Color aOldFillColor( pOut->GetFillColor() );

	sal_Bool bSolidTextRect = sal_False;
	sal_Bool bSolidImageRect = sal_False;

	if( bDropTarget && ( eSelectionMode != NO_SELECTION ) )
	{
		pOut->SetFillColor( rSettings.GetHighlightColor() );
		bSolidTextRect = sal_True;
		bSolidImageRect = sal_True;
	}
	else
	{
		if ( !bSelected || bCursored )
		{
			if( !pView->HasFontFillColor() )
				pOut->SetFillColor( pOut->GetBackground().GetColor() );
			else
			{
				const Color& rFillColor = pView->GetFont().GetFillColor();
				pOut->SetFillColor( rFillColor );
				if( rFillColor != aTransparent )
					bSolidTextRect = sal_True;
			}
		}
	}

	// Textrechteck zeichnen
	if( !bSolidTextRect )
	{
		if( !bIsBackgroundPainted )
			pOut->Erase( rTextRect );
	}
	else
	{
		Color aOldLineColor;
		if( bCursored )
		{
			aOldLineColor = pOut->GetLineColor();
			pOut->SetLineColor( Color( COL_GRAY ) );
		}
		pOut->DrawRect( rTextRect );
		if( bCursored )
			pOut->SetLineColor( aOldLineColor );
	}

	// Bildrechteck zeichnen
	if( !bSolidImageRect )
	{
		if( !bIsBackgroundPainted )
			pOut->Erase( rImageRect );
	}
// die Emphasis des Images muss von der abgeleiteten Klasse gezeichnet werden
// (in der virtuellen Funktion DrawEntryImage)
//	else
//		pOut->DrawRect( rImageRect );

	pOut->SetFillColor( aOldFillColor );
}


void SvxIconChoiceCtrl_Impl::PaintItem( const Rectangle& rRect,
	IcnViewFieldType eItem, SvxIconChoiceCtrlEntry* pEntry, sal_uInt16 nPaintFlags,
	OutputDevice* pOut, const String* pStr, ::vcl::ControlLayoutData* _pLayoutData )
{
	if( eItem == IcnViewFieldTypeText )
	{
		String aText;
		if( !pStr )
			aText = pView->GetEntryText( pEntry, sal_False );
		else
			aText = *pStr;

		if ( _pLayoutData )
		{
			pOut->DrawText( rRect, aText, nCurTextDrawFlags,
				&_pLayoutData->m_aUnicodeBoundRects, &_pLayoutData->m_aDisplayText );
		}
		else
		{
			Color aOldFontColor = pOut->GetTextColor();
			if ( pView->AutoFontColor() )
			{
				Color aBkgColor( pOut->GetBackground().GetColor() );
				Color aFontColor;
				sal_uInt16 nColor = ( aBkgColor.GetRed() + aBkgColor.GetGreen() + aBkgColor.GetBlue() ) / 3;
				if ( nColor > 127 )
					aFontColor.SetColor ( COL_BLACK );
				else
					aFontColor.SetColor( COL_WHITE );
				pOut->SetTextColor( aFontColor );
			}

			pOut->DrawText( rRect, aText, nCurTextDrawFlags );

			if ( pView->AutoFontColor() )
				pOut->SetTextColor( aOldFontColor );

			if( pEntry->IsFocused() )
			{
				Rectangle aRect ( CalcFocusRect( (SvxIconChoiceCtrlEntry*)pEntry ) );
				/*pView->*/ShowFocus( aRect );
				DrawFocusRect( pOut );
			}
		}
	}
	else
	{
		Point aPos( rRect.TopLeft() );
		if( nPaintFlags & PAINTFLAG_HOR_CENTERED )
			aPos.X() += (rRect.GetWidth() - aImageSize.Width() ) / 2;
		if( nPaintFlags & PAINTFLAG_VER_CENTERED )
			aPos.Y() += (rRect.GetHeight() - aImageSize.Height() ) / 2;
		pView->DrawEntryImage( pEntry, aPos, *pOut );
	}
}

void SvxIconChoiceCtrl_Impl::PaintEntryVirtOutDev( SvxIconChoiceCtrlEntry* pEntry )
{
#ifdef OV_NO_VIRT_OUTDEV
	PaintEntry( pEntry );
#else
	if( !pEntryPaintDev )
	{
		pEntryPaintDev = new VirtualDevice( *pView );
		pEntryPaintDev->SetFont( pView->GetFont() );
		pEntryPaintDev->SetLineColor();
		//pEntryPaintDev->SetBackground( pView->GetBackground() );
	}
	const Rectangle& rRect = GetEntryBoundRect( pEntry );
	Rectangle aOutRect( GetOutputRect() );
	if( !rRect.IsOver( aOutRect ) )
		return;
	Wallpaper aPaper( pView->GetBackground() );
	Rectangle aRect( aPaper.GetRect() );

	// Rechteck verschieben, so dass das Boundrect des Entries im
	// VirtOut-Dev bei 0,0 liegt.
	aRect.Move( -rRect.Left(), -rRect.Top() );
	aPaper.SetRect( aRect );
	pEntryPaintDev->SetBackground( aPaper );
	pEntryPaintDev->SetFont( pView->GetFont() );
	Rectangle aPix ( pEntryPaintDev->LogicToPixel(aRect) );


	Size aSize( rRect.GetSize() );
	pEntryPaintDev->SetOutputSizePixel( aSize );
	pEntryPaintDev->DrawOutDev(
		Point(), aSize, rRect.TopLeft(), aSize, *pView );

	PaintEntry( pEntry, Point(), pEntryPaintDev );

	pView->DrawOutDev(
		rRect.TopLeft(),
		aSize,
		Point(),
		aSize,
		*pEntryPaintDev );
#endif
}


void SvxIconChoiceCtrl_Impl::PaintEntry( SvxIconChoiceCtrlEntry* pEntry, const Point& rPos,
	OutputDevice* pOut, sal_Bool bIsBackgroundPainted )
{
	if( !pOut )
		pOut = pView;

	sal_Bool bSelected = sal_False;

	if( eSelectionMode != NO_SELECTION )
		bSelected = pEntry->IsSelected();

	sal_Bool bCursored = pEntry->IsCursored();
	sal_Bool bDropTarget = pEntry->IsDropTarget();
	sal_Bool bNoEmphasis = pEntry->IsBlockingEmphasis();

	Font aTempFont( pOut->GetFont() );

	// AutoFontColor
	/*
	if ( pView->AutoFontColor() )
	{
		aTempFont.SetColor ( aFontColor );
	}
	*/

	String aEntryText( pView->GetEntryText( pEntry, sal_False ) );
	Rectangle aTextRect( CalcTextRect(pEntry,&rPos,sal_False,&aEntryText));
	Rectangle aBmpRect( CalcBmpRect(pEntry, &rPos ) );

	sal_Bool	bShowSelection =
		(	(	( bSelected && !bCursored )
			||	bDropTarget
			)
		&&	!bNoEmphasis
		&&	( eSelectionMode != NO_SELECTION )
		);
	sal_Bool bActiveSelection = ( 0 != ( nWinBits & WB_NOHIDESELECTION ) ) || pView->HasFocus();

	if ( bShowSelection )
	{
		const StyleSettings& rSettings = pOut->GetSettings().GetStyleSettings();
		Font aNewFont( aTempFont );

		// bei hart attributierter Font-Fuellcolor muessen wir diese
		// hart auf die Highlight-Color setzen
		if( pView->HasFontFillColor() )
		{
			if( (nWinBits & WB_NOHIDESELECTION) || pView->HasFocus() )
				aNewFont.SetFillColor( rSettings.GetHighlightColor() );
			else
				aNewFont.SetFillColor( rSettings.GetDeactiveColor() );
		}

		Color aWinCol = rSettings.GetWindowTextColor();
		if ( !bActiveSelection && rSettings.GetFaceColor().IsBright() == aWinCol.IsBright() )
			aNewFont.SetColor( rSettings.GetWindowTextColor() );
		else
			aNewFont.SetColor( rSettings.GetHighlightTextColor() );

		pOut->SetFont( aNewFont );

		pOut->SetFillColor( pOut->GetBackground().GetColor() );
		pOut->DrawRect( CalcFocusRect( pEntry ) );
		pOut->SetFillColor( );
	}

	sal_Bool bResetClipRegion = sal_False;
	if( !pView->IsClipRegion() && (aVerSBar.IsVisible() || aHorSBar.IsVisible()) )
	{
		Rectangle aOutputArea( GetOutputRect() );
		if( aOutputArea.IsOver(aTextRect) || aOutputArea.IsOver(aBmpRect) )
		{
			pView->SetClipRegion( aOutputArea );
			bResetClipRegion = sal_True;
		}
	}

#ifdef OV_DRAWBOUNDRECT
	{
		Color aXOldColor = pOut->GetLineColor();
		pOut->SetLineColor( Color( COL_LIGHTRED ) );
		Rectangle aXRect( pEntry->aRect );
		aXRect.SetPos( rPos );
		pOut->DrawRect( aXRect );
		pOut->SetLineColor( aXOldColor );
	}
#endif

	sal_Bool bLargeIconMode = WB_ICON == ( nWinBits & (VIEWMODE_MASK) );
	sal_uInt16 nBmpPaintFlags = PAINTFLAG_VER_CENTERED;
	if ( bLargeIconMode )
		nBmpPaintFlags |= PAINTFLAG_HOR_CENTERED;
	sal_uInt16 nTextPaintFlags = bLargeIconMode ? PAINTFLAG_HOR_CENTERED : PAINTFLAG_VER_CENTERED;

	if( !bNoEmphasis )
		PaintEmphasis(aTextRect,aBmpRect,bSelected,bDropTarget,bCursored,pOut,bIsBackgroundPainted);

	if ( bShowSelection )
		pView->DrawSelectionBackground( CalcFocusRect( pEntry ),
		bActiveSelection ? 1 : 2 /* highlight */, sal_False /* check */, sal_True /* border */, sal_False /* ext border only */ );

	PaintItem( aBmpRect, IcnViewFieldTypeImage, pEntry, nBmpPaintFlags, pOut );

	PaintItem( aTextRect, IcnViewFieldTypeText, pEntry,
		nTextPaintFlags, pOut );

	// Highlight-Frame zeichnen
	if( pEntry == pCurHighlightFrame && !bNoEmphasis )
		DrawHighlightFrame( pOut, CalcFocusRect( pEntry ), sal_False );

	pOut->SetFont( aTempFont );
	if( bResetClipRegion )
		pView->SetClipRegion();
}

void SvxIconChoiceCtrl_Impl::SetEntryPos( SvxIconChoiceCtrlEntry* pEntry, const Point& rPos,
	sal_Bool bAdjustAtGrid, sal_Bool bCheckScrollBars, sal_Bool bKeepGridMap )
{
	ShowCursor( sal_False );
	Rectangle aBoundRect( GetEntryBoundRect( pEntry ));
	pView->Invalidate( aBoundRect );
	ToTop( pEntry );
	if( !IsAutoArrange() )
	{
		sal_Bool bAdjustVirtSize = sal_False;
		if( rPos != aBoundRect.TopLeft() )
		{
			Point aGridOffs(
				pEntry->aGridRect.TopLeft() - pEntry->aRect.TopLeft() );
			pImpCursor->Clear();
			if( !bKeepGridMap )
				pGridMap->Clear();
			aBoundRect.SetPos( rPos );
			pEntry->aRect = aBoundRect;
			pEntry->aGridRect.SetPos( rPos + aGridOffs );
			bAdjustVirtSize = sal_True;
		}
		if( bAdjustAtGrid )
		{
			if( bAdjustVirtSize )
			{
				// Durch das Ausrichten des (ggf. gerade neu positionierten) Eintrags,
				// kann er wieder komplett
				// in den sichtbaren Bereich rutschen, so dass u.U. doch keine Scrollbar
				// eingeblendet werden muss. Um deshalb ein 'Aufblitzen' der
				// Scrollbar(s) zu vermeiden, wird zum Aufplustern der virtuellen
				// Ausgabegroesse bereits das ausgerichtete Boundrect des
				// Eintrags genommen. Die virtuelle Groesse muss angepasst werden,
				// da AdjustEntryAtGrid von ihr abhaengt.
				const Rectangle& rBoundRect = GetEntryBoundRect( pEntry );
				Rectangle aCenterRect( CalcBmpRect( pEntry, 0 ));
				Point aNewPos( AdjustAtGrid( aCenterRect, rBoundRect ) );
				Rectangle aNewBoundRect( aNewPos, pEntry->aRect.GetSize());
				AdjustVirtSize( aNewBoundRect );
				bAdjustVirtSize = sal_False;
			}
			AdjustEntryAtGrid( pEntry );
			ToTop( pEntry );
		}
		if( bAdjustVirtSize )
			AdjustVirtSize( pEntry->aRect );

		if( bCheckScrollBars && bUpdateMode )
			CheckScrollBars();

		pView->Invalidate( pEntry->aRect );
		pGridMap->OccupyGrids( pEntry );
	}
	else
	{
		SvxIconChoiceCtrlEntry*	pPrev = FindEntryPredecessor( pEntry, rPos );
		SetEntryPredecessor( pEntry, pPrev );
		aAutoArrangeTimer.Start();
	}
	ShowCursor( sal_True );
}

void SvxIconChoiceCtrl_Impl::SetNoSelection()
{
	// rekursive Aufrufe ueber SelectEntry abblocken
	if( !(nFlags & F_CLEARING_SELECTION ))
	{
		nFlags |= F_CLEARING_SELECTION;
		DeselectAllBut( 0, sal_True );
		nFlags &= ~F_CLEARING_SELECTION;
	}
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GetEntry( const Point& rDocPos, sal_Bool bHit )
{
	CheckBoundingRects();
	// Z-Order-Liste vom Ende her absuchen
	sal_uLong nCount = pZOrderList->Count();
	while( nCount )
	{
		nCount--;
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nCount));
		if( pEntry->aRect.IsInside( rDocPos ) )
		{
			if( bHit )
			{
				Rectangle aRect = CalcBmpRect( pEntry );
				aRect.Top() -= 3;
				aRect.Bottom() += 3;
				aRect.Left() -= 3;
				aRect.Right() += 3;
				if( aRect.IsInside( rDocPos ) )
					return pEntry;
				aRect = CalcTextRect( pEntry );
				if( aRect.IsInside( rDocPos ) )
					return pEntry;
			}
			else
				return pEntry;
		}
	}
	return 0;
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GetNextEntry( const Point& rDocPos, SvxIconChoiceCtrlEntry* pCurEntry )
{
	CheckBoundingRects();
	SvxIconChoiceCtrlEntry* pTarget = 0;
	const sal_uLong nStartPos = pZOrderList->GetPos( (void*)pCurEntry );
	if( nStartPos != LIST_ENTRY_NOTFOUND )
	{
		const sal_uLong nCount = pZOrderList->Count();
		for( sal_uLong nCur = nStartPos+1; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nCur));
			if( pEntry->aRect.IsInside( rDocPos ) )
			{
				pTarget = pEntry;
				break;
			}
		}
	}
	return pTarget;
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GetPrevEntry( const Point& rDocPos, SvxIconChoiceCtrlEntry* pCurEntry )
{
	CheckBoundingRects();
	SvxIconChoiceCtrlEntry* pTarget = 0;
	sal_uLong nStartPos = pZOrderList->GetPos( (void*)pCurEntry );
	if( nStartPos != LIST_ENTRY_NOTFOUND && nStartPos != 0 )
	{
		nStartPos--;
		do
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nStartPos));
			if( pEntry->aRect.IsInside( rDocPos ) )
			{
				pTarget = pEntry;
				break;
			}
		} while( nStartPos > 0 );
	}
	return pTarget;
}

Point SvxIconChoiceCtrl_Impl::GetEntryPos( SvxIconChoiceCtrlEntry* pEntry )
{
	return pEntry->aRect.TopLeft();
}

void SvxIconChoiceCtrl_Impl::MakeEntryVisible( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bBound )
{
	if ( bBound )
	{
		const Rectangle& rRect = GetEntryBoundRect( pEntry );
		MakeVisible( rRect );
	}
	else
	{
		Rectangle aRect = CalcBmpRect( pEntry );
		aRect.Union( CalcTextRect( pEntry ) );
		aRect.Top() += TBOFFS_BOUND;
		aRect.Bottom() += TBOFFS_BOUND;
		aRect.Left() += LROFFS_BOUND;
		aRect.Right() += LROFFS_BOUND;
		MakeVisible( aRect );
	}
}

const Rectangle& SvxIconChoiceCtrl_Impl::GetEntryBoundRect( SvxIconChoiceCtrlEntry* pEntry )
{
	if( !IsBoundingRectValid( pEntry->aRect ))
		FindBoundingRect( pEntry );
	return pEntry->aRect;
}

Rectangle SvxIconChoiceCtrl_Impl::CalcBmpRect( SvxIconChoiceCtrlEntry* pEntry, const Point* pPos )
{
	Rectangle aBound = GetEntryBoundRect( pEntry );
	if( pPos )
		aBound.SetPos( *pPos );
	Point aPos( aBound.TopLeft() );

	switch( nWinBits & (VIEWMODE_MASK) )
	{
		case WB_ICON:
		{
			aPos.X() += ( aBound.GetWidth() - aImageSize.Width() ) / 2;
			return Rectangle( aPos, aImageSize );
		}

		case WB_SMALLICON:
		case WB_DETAILS:
			aPos.Y() += ( aBound.GetHeight() - aImageSize.Height() ) / 2;
			//todo: hor. Abstand zum BoundRect?
			return Rectangle( aPos, aImageSize );

		default:
			DBG_ERROR("IconView: Viewmode not set");
			return aBound;
	}
}

Rectangle SvxIconChoiceCtrl_Impl::CalcTextRect( SvxIconChoiceCtrlEntry* pEntry,
	const Point* pEntryPos, sal_Bool bEdit, const String* pStr )
{
	String aEntryText;
	if( !pStr )
		aEntryText = pView->GetEntryText( pEntry, bEdit );
	else
		aEntryText = *pStr;

	const Rectangle aMaxTextRect( CalcMaxTextRect( pEntry ) );
	Rectangle aBound( GetEntryBoundRect( pEntry ) );
	if( pEntryPos )
		aBound.SetPos( *pEntryPos );

	Rectangle aTextRect( aMaxTextRect );
	if( !bEdit )
		aTextRect = pView->GetTextRect( aTextRect, aEntryText, nCurTextDrawFlags );

	Size aTextSize( aTextRect.GetSize() );

	Point aPos( aBound.TopLeft() );
	long nBoundWidth = aBound.GetWidth();
	long nBoundHeight = aBound.GetHeight();

	switch( nWinBits & (VIEWMODE_MASK) )
	{
		case WB_ICON:
			aPos.Y() += aImageSize.Height();
			aPos.Y() += VER_DIST_BMP_STRING;
			// beim Editieren etwas mehr Platz
			if( bEdit )
			{
				// 20% rauf
				long nMinWidth = (( (aImageSize.Width()*10) / 100 ) * 2 ) +
								 aImageSize.Width();
				if( nMinWidth > nBoundWidth )
					nMinWidth = nBoundWidth;

				if( aTextSize.Width() < nMinWidth )
					aTextSize.Width() = nMinWidth;

				// beim Editieren ist Ueberlappung nach unten erlaubt
				Size aOptSize = aMaxTextRect.GetSize();
				if( aOptSize.Height() > aTextSize.Height() )
					aTextSize.Height() = aOptSize.Height();
			}
			aPos.X() += (nBoundWidth - aTextSize.Width()) / 2;
			break;

		case WB_SMALLICON:
		case WB_DETAILS:
			aPos.X() += aImageSize.Width();
			aPos.X() += HOR_DIST_BMP_STRING;
			aPos.Y() += (nBoundHeight - aTextSize.Height()) / 2;
			break;
	}
	return Rectangle( aPos, aTextSize );
}


long SvxIconChoiceCtrl_Impl::CalcBoundingWidth( SvxIconChoiceCtrlEntry* pEntry ) const
{
	long nStringWidth = GetItemSize( pEntry, IcnViewFieldTypeText ).Width();
//	nStringWidth += 2*LROFFS_TEXT;
	long nWidth = 0;

	switch( nWinBits & (VIEWMODE_MASK) )
	{
		case WB_ICON:
			nWidth = Max( nStringWidth, aImageSize.Width() );
			break;

		case WB_SMALLICON:
		case WB_DETAILS:
			nWidth = aImageSize.Width();
			nWidth += HOR_DIST_BMP_STRING;
			nWidth += nStringWidth;
			break;
	}
	return nWidth;
}

long SvxIconChoiceCtrl_Impl::CalcBoundingHeight( SvxIconChoiceCtrlEntry* pEntry ) const
{
	long nStringHeight = GetItemSize( pEntry, IcnViewFieldTypeText).Height();
	long nHeight = 0;

	switch( nWinBits & (VIEWMODE_MASK) )
	{
		case WB_ICON:
			nHeight = aImageSize.Height();
			nHeight += VER_DIST_BMP_STRING;
			nHeight += nStringHeight;
			break;

		case WB_SMALLICON:
		case WB_DETAILS:
			nHeight = Max( aImageSize.Height(), nStringHeight );
			break;
	}
	if( nHeight > nMaxBoundHeight )
	{
		((SvxIconChoiceCtrl_Impl*)this)->nMaxBoundHeight = nHeight;
		((SvxIconChoiceCtrl_Impl*)this)->aHorSBar.SetLineSize( GetScrollBarLineSize() );
		((SvxIconChoiceCtrl_Impl*)this)->aVerSBar.SetLineSize( GetScrollBarLineSize() );
	}
	return nHeight;
}

Size SvxIconChoiceCtrl_Impl::CalcBoundingSize( SvxIconChoiceCtrlEntry* pEntry ) const
{
	return Size( CalcBoundingWidth( pEntry ),
				 CalcBoundingHeight( pEntry ) );
}

void SvxIconChoiceCtrl_Impl::RecalcAllBoundingRects()
{
	nMaxBoundHeight	= 0;
	pZOrderList->Clear();
	sal_uLong nCount = aEntries.Count();
	sal_uLong nCur;
	SvxIconChoiceCtrlEntry* pEntry;

	if( !IsAutoArrange() || !pHead )
	{
		for( nCur = 0; nCur < nCount; nCur++ )
		{
			pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			FindBoundingRect( pEntry );
			pZOrderList->Insert( pEntry, LIST_APPEND );
		}
	}
	else
	{
		nCur = 0;
		pEntry = pHead;
		while( nCur != nCount )
		{
			DBG_ASSERT(pEntry->pflink&&pEntry->pblink,"SvxIconChoiceCtrl_Impl::RecalcAllBoundingRect > Bad link(s)");
			FindBoundingRect( pEntry );
			pZOrderList->Insert( pEntry, pZOrderList->Count() );
			pEntry = pEntry->pflink;
			nCur++;
		}
	}
	bBoundRectsDirty = sal_False;
	AdjustScrollBars();
}

void SvxIconChoiceCtrl_Impl::RecalcAllBoundingRectsSmart()
{
	nMaxBoundHeight	= 0;
	pZOrderList->Clear();
	sal_uLong nCur;
	SvxIconChoiceCtrlEntry* pEntry;
	const sal_uLong nCount = aEntries.Count();

	if( !IsAutoArrange() || !pHead )
	{
		for( nCur = 0; nCur < nCount; nCur++ )
		{
			pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			if( IsBoundingRectValid( pEntry->aRect ))
			{
				Size aBoundSize( pEntry->aRect.GetSize() );
				if( aBoundSize.Height() > nMaxBoundHeight )
					nMaxBoundHeight = aBoundSize.Height();
			}
			else
				FindBoundingRect( pEntry );
			pZOrderList->Insert( pEntry, LIST_APPEND );
		}
	}
	else
	{
		nCur = 0;
		pEntry = pHead;
		while( nCur != nCount )
		{
			DBG_ASSERT(pEntry->pflink&&pEntry->pblink,"SvxIconChoiceCtrl_Impl::RecalcAllBoundingRect > Bad link(s)");
			if( IsBoundingRectValid( pEntry->aRect ))
			{
				Size aBoundSize( pEntry->aRect.GetSize() );
				if( aBoundSize.Height() > nMaxBoundHeight )
					nMaxBoundHeight = aBoundSize.Height();
			}
			else
				FindBoundingRect( pEntry );
			pZOrderList->Insert( pEntry, LIST_APPEND );
			pEntry = pEntry->pflink;
			nCur++;
		}
	}
	AdjustScrollBars();
}

void SvxIconChoiceCtrl_Impl::UpdateBoundingRects()
{
	const sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		GetEntryBoundRect( pEntry );
	}
}

void SvxIconChoiceCtrl_Impl::FindBoundingRect( SvxIconChoiceCtrlEntry* pEntry )
{
	DBG_ASSERT(!pEntry->IsPosLocked(),"Locked entry pos in FindBoundingRect");
	if( pEntry->IsPosLocked() && IsBoundingRectValid( pEntry->aRect) )
	{
		AdjustVirtSize( pEntry->aRect );
		return;
	}
	Size aSize( CalcBoundingSize( pEntry ) );
	Point aPos(pGridMap->GetGridRect(pGridMap->GetUnoccupiedGrid(sal_True)).TopLeft());
	SetBoundingRect_Impl( pEntry, aPos, aSize );
}

void SvxIconChoiceCtrl_Impl::SetBoundingRect_Impl( SvxIconChoiceCtrlEntry* pEntry, const Point& rPos,
	const Size& /*rBoundingSize*/ )
{
	Rectangle aGridRect( rPos, Size(nGridDX, nGridDY) );
	pEntry->aGridRect = aGridRect;
	Center( pEntry );
	AdjustVirtSize( pEntry->aRect );
	pGridMap->OccupyGrids( pEntry );
}


void SvxIconChoiceCtrl_Impl::SetCursor( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bSyncSingleSelection,
	sal_Bool bShowFocusAsync )
{
	if( pEntry == pCursor )
	{
		if( pCursor && eSelectionMode == SINGLE_SELECTION && bSyncSingleSelection &&
				!pCursor->IsSelected() )
			SelectEntry( pCursor, sal_True, sal_True );
		return;
	}
	ShowCursor( sal_False );
	SvxIconChoiceCtrlEntry* pOldCursor = pCursor;
	pCursor = pEntry;
	if( pOldCursor )
	{
		pOldCursor->ClearFlags( ICNVIEW_FLAG_FOCUSED );
		if( eSelectionMode == SINGLE_SELECTION && bSyncSingleSelection )
			SelectEntry( pOldCursor, sal_False, sal_True ); // alten Cursor deselektieren
	}
	if( pCursor )
	{
		ToTop( pCursor );
		pCursor->SetFlags( ICNVIEW_FLAG_FOCUSED );
		if( eSelectionMode == SINGLE_SELECTION && bSyncSingleSelection )
			SelectEntry( pCursor, sal_True, sal_True );
		if( !bShowFocusAsync )
			ShowCursor( sal_True );
		else
		{
			if( !nUserEventShowCursor )
				nUserEventShowCursor =
					Application::PostUserEvent( LINK( this, SvxIconChoiceCtrl_Impl, UserEventHdl),
						EVENTID_SHOW_CURSOR );
		}
	}
}


void SvxIconChoiceCtrl_Impl::ShowCursor( sal_Bool bShow )
{
	if( !pCursor || !bShow || !pView->HasFocus() )
	{
		pView->HideFocus();
		return;
	}
	Rectangle aRect ( CalcFocusRect( pCursor ) );
	/*pView->*/ShowFocus( aRect );
}


void SvxIconChoiceCtrl_Impl::HideDDIcon()
{
	pView->Update();
	ImpHideDDIcon();
	pDDBufDev = pDDDev;
	pDDDev = 0;
}

void SvxIconChoiceCtrl_Impl::ImpHideDDIcon()
{
	if( pDDDev )
	{
		Size aSize( pDDDev->GetOutputSizePixel() );
		// pView restaurieren
		pView->DrawOutDev( aDDLastRectPos, aSize, Point(), aSize, *pDDDev );
	}
}


void SvxIconChoiceCtrl_Impl::ShowDDIcon( SvxIconChoiceCtrlEntry* pRefEntry, const Point& rPosPix )
{
	pView->Update();
	if( pRefEntry != pDDRefEntry )
	{
		DELETEZ(pDDDev);
		DELETEZ(pDDBufDev);
	}
	sal_Bool bSelected = pRefEntry->IsSelected();
	pRefEntry->ClearFlags( ICNVIEW_FLAG_SELECTED );
	if( !pDDDev )
	{
		if( pDDBufDev )
		{
			// nicht bei jedem Move ein Device anlegen, da dies besonders
			// auf Remote-Clients zu langsam ist
			pDDDev = pDDBufDev;
			pDDBufDev = 0;
		}
		else
		{
			pDDDev = new VirtualDevice( *pView );
			pDDDev->SetFont( pView->GetFont() );
		}
	}
	else
	{
		ImpHideDDIcon();
	}
	const Rectangle& rRect = GetEntryBoundRect( pRefEntry );
	pDDDev->SetOutputSizePixel( rRect.GetSize() );

	Point aPos( rPosPix );
	ToDocPos( aPos );

	Size aSize( pDDDev->GetOutputSizePixel() );
	pDDRefEntry = pRefEntry;
	aDDLastEntryPos = aPos;
	aDDLastRectPos = aPos;

	// Hintergrund sichern
	pDDDev->DrawOutDev( Point(), aSize, aPos, aSize, *pView );
	// Icon in pView malen
	pRefEntry->SetFlags( ICNVIEW_FLAG_BLOCK_EMPHASIS );
	PaintEntry( pRefEntry, aPos );
	pRefEntry->ClearFlags( ICNVIEW_FLAG_BLOCK_EMPHASIS );
	if( bSelected )
		pRefEntry->SetFlags( ICNVIEW_FLAG_SELECTED );
}

void SvxIconChoiceCtrl_Impl::HideShowDDIcon( SvxIconChoiceCtrlEntry* pRefEntry, const Point& rPosPix )
{
/*  In Notfaellen folgenden flackernden Code aktivieren:

		HideDDIcon();
		ShowDDIcon( pRefEntry, rPosPix );
		return;
*/
	if( !pDDDev )
	{
		ShowDDIcon( pRefEntry, rPosPix );
		return;
	}

	if( pRefEntry != pDDRefEntry )
	{
		HideDDIcon();
		ShowDDIcon( pRefEntry, rPosPix );
		return;
	}

	Point aEmptyPoint;

	Point aCurEntryPos( rPosPix );
	ToDocPos( aCurEntryPos );

	const Rectangle& rRect = GetEntryBoundRect( pRefEntry );
	Size aEntrySize( rRect.GetSize() );
	Rectangle aPrevEntryRect( aDDLastEntryPos, aEntrySize );
	Rectangle aCurEntryRect( aCurEntryPos, aEntrySize );

	if( !aPrevEntryRect.IsOver( aCurEntryRect ) )
	{
		HideDDIcon();
		ShowDDIcon( pRefEntry, rPosPix );
		return;
	}

	// Ueberlappung des neuen und alten D&D-Pointers!

	Rectangle aFullRect( aPrevEntryRect.Union( aCurEntryRect ) );
	if( !pDDTempDev )
	{
		pDDTempDev = new VirtualDevice( *pView );
		pDDTempDev->SetFont( pView->GetFont() );
	}

	Size aFullSize( aFullRect.GetSize() );
	Point aFullPos( aFullRect.TopLeft() );

	pDDTempDev->SetOutputSizePixel( aFullSize );

	// Hintergrund (mit dem alten D&D-Pointer!) sichern
	pDDTempDev->DrawOutDev( aEmptyPoint, aFullSize, aFullPos, aFullSize, *pView );
	// den alten Buffer in den neuen Buffer pasten
	aDDLastRectPos = aDDLastRectPos - aFullPos;

	pDDTempDev->DrawOutDev(
		aDDLastRectPos,
		pDDDev->GetOutputSizePixel(),
		aEmptyPoint,
		pDDDev->GetOutputSizePixel(),
		*pDDDev );

	// Swap
	VirtualDevice* pTemp = pDDDev;
	pDDDev = pDDTempDev;
	pDDTempDev = pTemp;

	// in den restaurierten Hintergrund den neuen D&D-Pointer zeichnen
	pDDTempDev->SetOutputSizePixel( pDDDev->GetOutputSizePixel() );
	pDDTempDev->DrawOutDev(
		aEmptyPoint, aFullSize, aEmptyPoint, aFullSize, *pDDDev );
	Point aRelPos = aCurEntryPos - aFullPos;
	pRefEntry->SetFlags( ICNVIEW_FLAG_BLOCK_EMPHASIS );
	PaintEntry( pRefEntry, aRelPos, pDDTempDev );
	pRefEntry->ClearFlags( ICNVIEW_FLAG_BLOCK_EMPHASIS );

	aDDLastRectPos = aFullPos;
	aDDLastEntryPos = aCurEntryPos;

	pView->DrawOutDev(
		aDDLastRectPos,
		pDDDev->GetOutputSizePixel(),
		aEmptyPoint,
		pDDDev->GetOutputSizePixel(),
		*pDDTempDev );
}

void SvxIconChoiceCtrl_Impl::InvalidateBoundingRect( SvxIconChoiceCtrlEntry* pEntry )
{
	InvalidateBoundingRect( pEntry->aRect );
}


sal_Bool SvxIconChoiceCtrl_Impl::HandleScrollCommand( const CommandEvent& rCmd )
{
	Rectangle aDocRect(	GetDocumentRect() );
	Rectangle aVisRect( GetVisibleRect() );
	if( aVisRect.IsInside( aDocRect ))
		return sal_False;
	Size aDocSize( aDocRect.GetSize() );
	Size aVisSize( aVisRect.GetSize() );
	sal_Bool bHor = aDocSize.Width() > aVisSize.Width();
	sal_Bool bVer = aDocSize.Height() > aVisSize.Height();

	long nScrollDX = 0, nScrollDY = 0;

	switch( rCmd.GetCommand() )
	{
		case COMMAND_STARTAUTOSCROLL:
		{
			pView->EndTracking();
			sal_uInt16 nScrollFlags = 0;
			if( bHor )
				nScrollFlags |= AUTOSCROLL_HORZ;
			if( bVer )
				nScrollFlags |= AUTOSCROLL_VERT;
			if( nScrollFlags )
			{
				pView->StartAutoScroll( nScrollFlags );
				return sal_True;
			}
		}
		break;

		case COMMAND_WHEEL:
		{
			const CommandWheelData* pData = rCmd.GetWheelData();
			if( pData && (COMMAND_WHEEL_SCROLL == pData->GetMode()) && !pData->IsHorz() )
			{
				sal_uLong nScrollLines = pData->GetScrollLines();
				if( nScrollLines == COMMAND_WHEEL_PAGESCROLL )
				{
					nScrollDY = GetScrollBarPageSize( aVisSize.Width() );
					if( pData->GetDelta() < 0 )
						nScrollDY *= -1;
				}
				else
				{
					nScrollDY = pData->GetNotchDelta() * (long)nScrollLines;
					nScrollDY *= GetScrollBarLineSize();
				}
			}
		}
		break;

		case COMMAND_AUTOSCROLL:
		{
			const CommandScrollData* pData = rCmd.GetAutoScrollData();
			if( pData )
			{
				nScrollDX = pData->GetDeltaX() * GetScrollBarLineSize();
				nScrollDY = pData->GetDeltaY() * GetScrollBarLineSize();
			}
		}
		break;
	}

	if( nScrollDX || nScrollDY )
	{
		aVisRect.Top() -= nScrollDY;
		aVisRect.Bottom() -= nScrollDY;
		aVisRect.Left() -= nScrollDX;
		aVisRect.Right() -= nScrollDX;
		MakeVisible( aVisRect );
		return sal_True;
	}
	return sal_False;
}


void SvxIconChoiceCtrl_Impl::Command( const CommandEvent& rCEvt )
{
	// Rollmaus-Event?
	if( (rCEvt.GetCommand() == COMMAND_WHEEL) ||
		(rCEvt.GetCommand() == COMMAND_STARTAUTOSCROLL) ||
		(rCEvt.GetCommand() == COMMAND_AUTOSCROLL) )
	{
#if 1
		if( HandleScrollCommand( rCEvt ) )
			return;
#else
		ScrollBar* pHor = aHorSBar.IsVisible() ? &aHorSBar : 0;
		ScrollBar* pVer = aVerSBar.IsVisible() ? &aVerSBar : 0;
		if( pView->HandleScrollCommand( rCEvt, pHor, pVer ) )
			return;
#endif
	}
}

void SvxIconChoiceCtrl_Impl::ToTop( SvxIconChoiceCtrlEntry* pEntry )
{
	if( pZOrderList->GetObject( pZOrderList->Count() - 1 ) != pEntry )
	{
		sal_uLong nPos = pZOrderList->GetPos( (void*)pEntry );
		pZOrderList->Remove( nPos );
		pZOrderList->Insert( pEntry, LIST_APPEND );
	}
}

void SvxIconChoiceCtrl_Impl::ClipAtVirtOutRect( Rectangle& rRect ) const
{
	if( rRect.Bottom() >= aVirtOutputSize.Height() )
		rRect.Bottom() = aVirtOutputSize.Height() - 1;
	if( rRect.Right() >= aVirtOutputSize.Width() )
		rRect.Right() = aVirtOutputSize.Width() - 1;
	if( rRect.Top() < 0 )
		rRect.Top() = 0;
	if( rRect.Left() < 0 )
		rRect.Left() = 0;
}

// rRect: Bereich des Dokumentes (in Dokumentkoordinaten), der
// sichtbar gemacht werden soll.
// bScrBar == sal_True: Das Rect wurde aufgrund eines ScrollBar-Events berechnet

void SvxIconChoiceCtrl_Impl::MakeVisible( const Rectangle& rRect, sal_Bool bScrBar,
	sal_Bool bCallRectChangedHdl )
{
	Rectangle aVirtRect( rRect );
	ClipAtVirtOutRect( aVirtRect );
	Point aOrigin( pView->GetMapMode().GetOrigin() );
	// in Dokumentkoordinate umwandeln
	aOrigin *= -1;
	Rectangle aOutputArea( GetOutputRect() );
	if( aOutputArea.IsInside( aVirtRect ) )
		return;	// ist schon sichtbar

	long nDy;
	if( aVirtRect.Top() < aOutputArea.Top() )
	{
		// nach oben scrollen (nDy < 0)
		nDy = aVirtRect.Top() - aOutputArea.Top();
	}
	else if( aVirtRect.Bottom() > aOutputArea.Bottom() )
	{
		// nach unten scrollen (nDy > 0)
		nDy = aVirtRect.Bottom() - aOutputArea.Bottom();
	}
	else
		nDy = 0;

	long nDx;
	if( aVirtRect.Left() < aOutputArea.Left() )
	{
		// nach links scrollen (nDx < 0)
		nDx = aVirtRect.Left() - aOutputArea.Left();
	}
	else if( aVirtRect.Right() > aOutputArea.Right() )
	{
		// nach rechts scrollen (nDx > 0)
		nDx = aVirtRect.Right() - aOutputArea.Right();
	}
	else
		nDx = 0;

	aOrigin.X() += nDx;
	aOrigin.Y() += nDy;
	aOutputArea.SetPos( aOrigin );
	if( GetUpdateMode() )
	{
		HideDDIcon();
		pView->Update();
		ShowCursor( sal_False );
	}

	// Origin fuer SV invertieren (damit wir in
	// Dokumentkoordinaten scrollen/painten koennen)
	aOrigin *= -1;
	SetOrigin( aOrigin );

	sal_Bool bScrollable = pView->GetBackground().IsScrollable();
	if( pView->HasBackground() && !bScrollable )
	{
		Rectangle aRect( GetOutputRect());
		Wallpaper aPaper( pView->GetBackground() );
		aPaper.SetRect( aRect );
		pView->SetBackground( aPaper );
	}

	if( bScrollable && GetUpdateMode() )
	{
		// in umgekehrte Richtung scrollen!
		pView->Control::Scroll( -nDx, -nDy, aOutputArea,
			SCROLL_NOCHILDREN | SCROLL_USECLIPREGION | SCROLL_CLIP );
	}
	else
		pView->Invalidate(INVALIDATE_NOCHILDREN);

	if( aHorSBar.IsVisible() || aVerSBar.IsVisible() )
	{
		if( !bScrBar )
		{
			aOrigin *= -1;
			// Thumbs korrigieren
			if(aHorSBar.IsVisible() && aHorSBar.GetThumbPos() != aOrigin.X())
				aHorSBar.SetThumbPos( aOrigin.X() );
			if(aVerSBar.IsVisible() && aVerSBar.GetThumbPos() != aOrigin.Y())
				aVerSBar.SetThumbPos( aOrigin.Y() );
		}
	}

	if( GetUpdateMode() )
		ShowCursor( sal_True );

	// pruefen, ob ScrollBars noch benoetigt werden
	CheckScrollBars();
	if( bScrollable && GetUpdateMode() )
		pView->Update();

	// kann der angeforderte Bereich nicht komplett sichtbar gemacht werden,
	// wird auf jeden Fall der Vis-Rect-Changed-Handler gerufen. Eintreten kann der
	// Fall z.B. wenn nur wenige Pixel des unteren Randes nicht sichtbar sind,
	// eine ScrollBar aber eine groessere Line-Size eingestellt hat.
	if( bCallRectChangedHdl || GetOutputRect() != rRect )
		VisRectChanged();
}


SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::FindNewCursor()
{
	SvxIconChoiceCtrlEntry* pNewCursor;
	if( pCursor )
	{
		pNewCursor = pImpCursor->GoLeftRight( pCursor, sal_False );
		if( !pNewCursor )
		{
			pNewCursor = pImpCursor->GoLeftRight( pCursor, sal_True );
			if( !pNewCursor )
			{
				pNewCursor = pImpCursor->GoUpDown( pCursor, sal_False );
				if( !pNewCursor )
					pNewCursor = pImpCursor->GoUpDown( pCursor, sal_True );
			}
		}
	}
	else
		pNewCursor = (SvxIconChoiceCtrlEntry*)aEntries.First();
	DBG_ASSERT(!pNewCursor|| (pCursor&&pCursor!=pNewCursor),"FindNewCursor failed");
	return pNewCursor;
}

sal_uLong SvxIconChoiceCtrl_Impl::GetSelectionCount() const
{
	if( (nWinBits & WB_HIGHLIGHTFRAME) && pCurHighlightFrame )
		return 1;
	return nSelectionCount;
}

void SvxIconChoiceCtrl_Impl::ToggleSelection( SvxIconChoiceCtrlEntry* pEntry )
{
	sal_Bool bSel;
	if( pEntry->IsSelected() )
		bSel = sal_False;
	else
		bSel = sal_True;
	SelectEntry( pEntry, bSel, sal_True, sal_True );
}

void SvxIconChoiceCtrl_Impl::DeselectAllBut( SvxIconChoiceCtrlEntry* pThisEntryNot,
	sal_Bool bPaintSync )
{
	ClearSelectedRectList();
	//
	// !!!!!!! Todo: Evtl. Z-Orderlist abarbeiten !!!!!!!
	//
	sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		if( pEntry != pThisEntryNot && pEntry->IsSelected() )
			SelectEntry( pEntry, sal_False, sal_True, sal_True, bPaintSync );
	}
	pAnchor = 0;
	nFlags &= (~F_ADD_MODE);
}

Size SvxIconChoiceCtrl_Impl::GetMinGrid() const
{
	Size aMinSize( aImageSize );
	aMinSize.Width() += 2 * LROFFS_BOUND;
	aMinSize.Height() += TBOFFS_BOUND;	// PB: einmal Offset reicht (FileDlg)
	String aStrDummy( RTL_CONSTASCII_USTRINGPARAM( "XXX" ) );
	Size aTextSize( pView->GetTextWidth( aStrDummy ), pView->GetTextHeight() );
	if( nWinBits & WB_ICON )
	{
		aMinSize.Height() += VER_DIST_BMP_STRING;
		aMinSize.Height() += aTextSize.Height();
	}
	else
	{
		aMinSize.Width() += HOR_DIST_BMP_STRING;
		aMinSize.Width() += aTextSize.Width();
	}
	return aMinSize;
}

void SvxIconChoiceCtrl_Impl::SetGrid( const Size& rSize )
{
	Size aSize( rSize );
	Size aMinSize( GetMinGrid() );
	if( aSize.Width() < aMinSize.Width() )
		aSize.Width() = aMinSize.Width();
	if( aSize.Height() < aMinSize.Height() )
		aSize.Height() = aMinSize.Height();

	nGridDX = aSize.Width();
	// HACK(Detail-Modus ist noch nicht vollstaendig implementiert!)
	// dieses Workaround bringts mit einer Spalte zum Fliegen
	if( nWinBits & WB_DETAILS )
	{
		const SvxIconChoiceCtrlColumnInfo* pCol = GetColumn( 0 );
		if( pCol )
			((SvxIconChoiceCtrlColumnInfo*)pCol)->SetWidth( nGridDX );
	}
	nGridDY = aSize.Height();
	SetDefaultTextSize();
}

// berechnet die maximale Groesse, die das Textrechteck innerhalb des
// umschliessenden Rechtecks einnehmen kann. Im Modus WB_ICON und
// IcnShowTextFull wird Bottom auf LONG_MAX gesetzt

Rectangle SvxIconChoiceCtrl_Impl::CalcMaxTextRect( const SvxIconChoiceCtrlEntry* pEntry ) const
{
	Rectangle aBoundRect;
	// keine Endlosrekursion! deshalb das Bound-Rect hier nicht berechnen
	if( IsBoundingRectValid( pEntry->aRect ) )
		aBoundRect = pEntry->aRect;
	else
		aBoundRect = pEntry->aGridRect;

	Rectangle aBmpRect( ((SvxIconChoiceCtrl_Impl*)this)->CalcBmpRect(
		(SvxIconChoiceCtrlEntry*)pEntry ) );
	if( nWinBits & WB_ICON )
	{
		aBoundRect.Top() = aBmpRect.Bottom();
		aBoundRect.Top() += VER_DIST_BMP_STRING;
		if( aBoundRect.Top() > aBoundRect.Bottom())
			aBoundRect.Top() = aBoundRect.Bottom();
		aBoundRect.Left() += LROFFS_BOUND;
		aBoundRect.Left()++;
		aBoundRect.Right() -= LROFFS_BOUND;
		aBoundRect.Right()--;
		if( aBoundRect.Left() > aBoundRect.Right())
			aBoundRect.Left() = aBoundRect.Right();
		if( GetEntryTextModeSmart( pEntry ) == IcnShowTextFull )
			aBoundRect.Bottom() = LONG_MAX;
	}
	else
	{
		aBoundRect.Left() = aBmpRect.Right();
		aBoundRect.Left() += HOR_DIST_BMP_STRING;
		aBoundRect.Right() -= LROFFS_BOUND;
		if( aBoundRect.Left() > aBoundRect.Right() )
			aBoundRect.Left() = aBoundRect.Right();
		long nHeight = aBoundRect.GetSize().Height();
		nHeight = nHeight - aDefaultTextSize.Height();
		nHeight /= 2;
		aBoundRect.Top() += nHeight;
		aBoundRect.Bottom() -= nHeight;
	}
	return aBoundRect;
}

void SvxIconChoiceCtrl_Impl::SetDefaultTextSize()
{
	long nDY = nGridDY;
	nDY -= aImageSize.Height();
	nDY -= VER_DIST_BMP_STRING;
	nDY -= 2*TBOFFS_BOUND;
	if( nDY <= 0 )
		nDY = 2;

	long nDX = nGridDX;
	nDX -= 2*LROFFS_BOUND;
	nDX -= 2;
	if( nDX <= 0 )
		nDX = 2;

	String aStrDummy( RTL_CONSTASCII_USTRINGPARAM( "X" ) );
	long nHeight = pView->GetTextHeight();
	if( nDY < nHeight )
		nDY = nHeight;
	aDefaultTextSize = Size( nDX, nDY );
}


void SvxIconChoiceCtrl_Impl::Center( SvxIconChoiceCtrlEntry* pEntry ) const
{
	pEntry->aRect = pEntry->aGridRect;
	Size aSize( CalcBoundingSize( pEntry ) );
	if( nWinBits & WB_ICON )
	{
		// horizontal zentrieren
		long nBorder = pEntry->aGridRect.GetWidth() - aSize.Width();
		pEntry->aRect.Left() += nBorder / 2;
		pEntry->aRect.Right() -= nBorder / 2;
	}
	// vertikal zentrieren
	pEntry->aRect.Bottom() = pEntry->aRect.Top() + aSize.Height();
}


// Die Deltas entsprechen Offsets, um die die View auf dem Doc verschoben wird
// links, hoch: Offsets < 0
// rechts, runter: Offsets > 0
void SvxIconChoiceCtrl_Impl::Scroll( long nDeltaX, long nDeltaY, sal_Bool bScrollBar )
{
	const MapMode& rMapMode = pView->GetMapMode();
	Point aOrigin( rMapMode.GetOrigin() );
	// in Dokumentkoordinate umwandeln
	aOrigin *= -1;
	aOrigin.Y() += nDeltaY;
	aOrigin.X() += nDeltaX;
	Rectangle aRect( aOrigin, aOutputSize );
	MakeVisible( aRect, bScrollBar );
}


const Size& SvxIconChoiceCtrl_Impl::GetItemSize( SvxIconChoiceCtrlEntry*,
	IcnViewFieldType eItem ) const
{
	if( eItem == IcnViewFieldTypeText )
		return aDefaultTextSize;
	return aImageSize;
}

Rectangle SvxIconChoiceCtrl_Impl::CalcFocusRect( SvxIconChoiceCtrlEntry* pEntry )
{
	Rectangle aBmpRect( CalcBmpRect( pEntry ) );
	Rectangle aTextRect( CalcTextRect( pEntry ) );
	Rectangle aBoundRect( GetEntryBoundRect( pEntry ) );
	Rectangle aFocusRect( aBoundRect.Left(), aBmpRect.Top() - 1,
						  aBoundRect.Right() - 4, aTextRect.Bottom() + 1 );
	// Das Fokusrechteck soll nicht den Text beruehren
	if( aFocusRect.Left() - 1 >= pEntry->aRect.Left() )
		aFocusRect.Left()--;
	if( aFocusRect.Right() + 1 <= pEntry->aRect.Right() )
		aFocusRect.Right()++;

	return aFocusRect;
}

// Der 'Hot Spot' sind die inneren 50% der Rechteckflaeche
static Rectangle GetHotSpot( const Rectangle& rRect )
{
	Rectangle aResult( rRect );
	aResult.Justify();
	Size aSize( rRect.GetSize() );
	long nDelta = aSize.Width() / 4;
	aResult.Left() += nDelta;
	aResult.Right() -= nDelta;
	nDelta = aSize.Height() / 4;
	aResult.Top() += nDelta;
	aResult.Bottom() -= nDelta;
	return aResult;
}

void SvxIconChoiceCtrl_Impl::SelectRect( SvxIconChoiceCtrlEntry* pEntry1, SvxIconChoiceCtrlEntry* pEntry2,
	sal_Bool bAdd, SvPtrarr* pOtherRects )
{
	DBG_ASSERT(pEntry1 && pEntry2,"SelectEntry: Invalid Entry-Ptr");
	Rectangle aRect( GetEntryBoundRect( pEntry1 ) );
	aRect.Union( GetEntryBoundRect( pEntry2 ) );
	SelectRect( aRect, bAdd, pOtherRects );
}

void SvxIconChoiceCtrl_Impl::SelectRect( const Rectangle& rRect, sal_Bool bAdd,
	SvPtrarr* pOtherRects )
{
	aCurSelectionRect = rRect;
	if( !pZOrderList || !pZOrderList->Count() )
		return;

	// Flag setzen, damit im Select kein ToTop gerufen wird
	sal_Bool bAlreadySelectingRect = nFlags & F_SELECTING_RECT ? sal_True : sal_False;
	nFlags |= F_SELECTING_RECT;

	CheckBoundingRects();
	pView->Update();
	const sal_uLong nCount = pZOrderList->Count();

	Rectangle aRect( rRect );
	aRect.Justify();
	sal_Bool bCalcOverlap = (bAdd && pOtherRects && pOtherRects->Count()) ? sal_True : sal_False;

	sal_Bool bResetClipRegion = sal_False;
	if( !pView->IsClipRegion() )
	{
		bResetClipRegion = sal_True;
		pView->SetClipRegion( GetOutputRect() );
	}

	for( sal_uLong nPos = 0; nPos < nCount; nPos++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)(pZOrderList->GetObject(nPos ));

		if( !IsBoundingRectValid( pEntry->aRect ))
			FindBoundingRect( pEntry );
		Rectangle aBoundRect( GetHotSpot( pEntry->aRect ) );
		sal_Bool bSelected = pEntry->IsSelected();

		sal_Bool bOverlaps;
		if( bCalcOverlap )
			bOverlaps = IsOver( pOtherRects, aBoundRect );
		else
			bOverlaps = sal_False;
		sal_Bool bOver = aRect.IsOver( aBoundRect );

		if( bOver && !bOverlaps )
		{
			// Ist im neuen Selektionsrechteck und in keinem alten
			// => selektieren
			if( !bSelected )
				SelectEntry( pEntry, sal_True, sal_True, sal_True );
		}
		else if( !bAdd )
		{
			// ist ausserhalb des Selektionsrechtecks
			// => Selektion entfernen
			if( bSelected )
				SelectEntry( pEntry, sal_False, sal_True, sal_True );
		}
		else if( bAdd && bOverlaps )
		{
			// Der Eintrag befindet sich in einem alten (=>Aufspannen
			// mehrerer Rechtecke mit Ctrl!) Selektionsrechteck

			// Hier ist noch ein Bug! Der Selektionsstatus eines Eintrags
			// in einem vorherigen Rechteck, muss restauriert werden, wenn
			// er vom aktuellen Selektionsrechteck beruehrt wurde, jetzt aber
			// nicht mehr in ihm liegt. Ich gehe hier der Einfachheit halber
			// pauschal davon aus, dass die Eintraege in den alten Rechtecken
			// alle selektiert sind. Ebenso ist es falsch, die Schnittmenge
			// nur zu deselektieren.
			// Loesungsmoeglichkeit: Snapshot der Selektion vor dem Auf-
			// spannen des Rechtecks merken
			if( aBoundRect.IsOver( rRect))
			{
				// Schnittmenge zwischen alten Rects & aktuellem Rect desel.
				if( bSelected )
					SelectEntry( pEntry, sal_False, sal_True, sal_True );
			}
			else
			{
				// Eintrag eines alten Rects selektieren
				if( !bSelected )
					SelectEntry( pEntry, sal_True, sal_True, sal_True );
			}
		}
		else if( !bOver && bSelected )
		{
			// Der Eintrag liegt voellig ausserhalb und wird deshalb desel.
			SelectEntry( pEntry, sal_False, sal_True, sal_True );
		}
	}

	if( !bAlreadySelectingRect )
		nFlags &= ~F_SELECTING_RECT;

	pView->Update();
	if( bResetClipRegion )
		pView->SetClipRegion();
}

void SvxIconChoiceCtrl_Impl::SelectRange(
						SvxIconChoiceCtrlEntry* pStart,
						SvxIconChoiceCtrlEntry* pEnd,
						sal_Bool bAdd )
{
	sal_uLong nFront = GetEntryListPos( pStart );
	sal_uLong nBack	 = GetEntryListPos( pEnd );
	sal_uLong nFirst = std::min( nFront, nBack );
	sal_uLong nLast	 = std::max( nFront, nBack );
	sal_uLong i;
	SvxIconChoiceCtrlEntry* pEntry;

	if ( ! bAdd )
	{
		// deselect everything before the first entry if not in
		// adding mode
		for ( i=0; i<nFirst; i++ )
		{
			pEntry = GetEntry( i );
			if( pEntry->IsSelected() )
				SelectEntry( pEntry, sal_False, sal_True, sal_True, sal_True );
		}
	}

	// select everything between nFirst and nLast
	for ( i=nFirst; i<=nLast; i++ )
	{
		pEntry = GetEntry( i );
		if( ! pEntry->IsSelected() )
			SelectEntry( pEntry, sal_True, sal_True,  sal_True, sal_True );
	}

	if ( ! bAdd )
	{
		// deselect everything behind the last entry if not in
		// adding mode
		sal_uLong nEnd = GetEntryCount();
		for ( ; i<nEnd; i++ )
		{
			pEntry = GetEntry( i );
			if( pEntry->IsSelected() )
				SelectEntry( pEntry, sal_False, sal_True, sal_True, sal_True );
		}
	}
}

sal_Bool SvxIconChoiceCtrl_Impl::IsOver( SvPtrarr* pRectList, const Rectangle& rBoundRect ) const
{
	const sal_uInt16 nCount = pRectList->Count();
	for( sal_uInt16 nCur = 0; nCur < nCount; nCur++ )
	{
		Rectangle* pRect = (Rectangle*)pRectList->GetObject( nCur );
		if( rBoundRect.IsOver( *pRect ))
			return sal_True;
	}
	return sal_False;
}

void SvxIconChoiceCtrl_Impl::AddSelectedRect( SvxIconChoiceCtrlEntry* pEntry1,
	SvxIconChoiceCtrlEntry* pEntry2 )
{
	DBG_ASSERT(pEntry1 && pEntry2,"SelectEntry: Invalid Entry-Ptr");
	Rectangle aRect( GetEntryBoundRect( pEntry1 ) );
	aRect.Union( GetEntryBoundRect( pEntry2 ) );
	AddSelectedRect( aRect );
}

void SvxIconChoiceCtrl_Impl::AddSelectedRect( const Rectangle& rRect )
{
	Rectangle* pRect = new Rectangle( rRect );
	pRect->Justify();
	aSelectedRectList.Insert( (void*)pRect, aSelectedRectList.Count() );
}

void SvxIconChoiceCtrl_Impl::ClearSelectedRectList()
{
	const sal_uInt16 nCount = aSelectedRectList.Count();
	for( sal_uInt16 nCur = 0; nCur < nCount; nCur++ )
	{
		Rectangle* pRect = (Rectangle*)aSelectedRectList.GetObject( nCur );
		delete pRect;
	}
	aSelectedRectList.Remove( 0, aSelectedRectList.Count() );
}

void SvxIconChoiceCtrl_Impl::CalcScrollOffsets( const Point& rPosPixel,
	long& rX, long& rY, sal_Bool isInDragDrop, sal_uInt16 nBorderWidth)
{
	// Scrolling der View, falls sich der Mauszeiger im Grenzbereich des
	// Fensters befindet
	long nPixelToScrollX = 0;
	long nPixelToScrollY = 0;
	Size aWndSize = aOutputSize;

	nBorderWidth = (sal_uInt16)(Min( (long)(aWndSize.Height()-1), (long)nBorderWidth ));
	nBorderWidth = (sal_uInt16)(Min( (long)(aWndSize.Width()-1), (long)nBorderWidth ));

	if ( rPosPixel.X() < nBorderWidth )
	{
		if( isInDragDrop )
			nPixelToScrollX = -DD_SCROLL_PIXEL;
		else
			nPixelToScrollX = rPosPixel.X()- nBorderWidth;
	}
	else if ( rPosPixel.X() > aWndSize.Width() - nBorderWidth )
	{
		if( isInDragDrop )
			nPixelToScrollX = DD_SCROLL_PIXEL;
		else
			nPixelToScrollX = rPosPixel.X() - (aWndSize.Width() - nBorderWidth);
	}
	if ( rPosPixel.Y() < nBorderWidth )
	{
		if( isInDragDrop )
			nPixelToScrollY = -DD_SCROLL_PIXEL;
		else
			nPixelToScrollY = rPosPixel.Y() - nBorderWidth;
	}
	else if ( rPosPixel.Y() > aWndSize.Height() - nBorderWidth )
	{
		if( isInDragDrop )
			nPixelToScrollY = DD_SCROLL_PIXEL;
		else
			nPixelToScrollY = rPosPixel.Y() - (aWndSize.Height() - nBorderWidth);
	}

	rX = nPixelToScrollX;
	rY = nPixelToScrollY;
}

IMPL_LINK(SvxIconChoiceCtrl_Impl, AutoArrangeHdl, void*, EMPTYARG )
{
	aAutoArrangeTimer.Stop();
	Arrange( IsAutoArrange() );
	return 0;
}

IMPL_LINK(SvxIconChoiceCtrl_Impl, VisRectChangedHdl, void*, EMPTYARG )
{
	aVisRectChangedTimer.Stop();
	pView->VisibleRectChanged();
	return 0;
}

IMPL_LINK(SvxIconChoiceCtrl_Impl, DocRectChangedHdl, void*, EMPTYARG )
{
	aDocRectChangedTimer.Stop();
	pView->DocumentRectChanged();
	return 0;
}

void SvxIconChoiceCtrl_Impl::PrepareCommandEvent( const CommandEvent& rCEvt )
{
	StopEditTimer();
	SvxIconChoiceCtrlEntry* pEntry = pView->GetEntry( rCEvt.GetMousePosPixel() );
	if( (nFlags & F_DOWN_CTRL) && pEntry && !pEntry->IsSelected() )
		SelectEntry( pEntry, sal_True, sal_True );
	nFlags &= ~(F_DOWN_CTRL | F_DOWN_DESELECT);
}

sal_Bool SvxIconChoiceCtrl_Impl::IsTextHit( SvxIconChoiceCtrlEntry* pEntry, const Point& rDocPos )
{
	Rectangle aRect( CalcTextRect( pEntry ));
	if( aRect.IsInside( rDocPos ) )
		return sal_True;
	return sal_False;
}

IMPL_LINK(SvxIconChoiceCtrl_Impl, EditTimeoutHdl, Timer*, EMPTYARG )
{
	SvxIconChoiceCtrlEntry* pEntry = GetCurEntry();
	if( bEntryEditingEnabled && pEntry &&
		pEntry->IsSelected())
	{
		if( pView->EditingEntry( pEntry ))
			EditEntry( pEntry );
	}
	return 0;
}


//
// Funktionen zum Ausrichten der Eintraege am Grid
//

// pStart == 0: Alle Eintraege werden ausgerichtet
// sonst: Alle Eintraege der Zeile ab einschliesslich pStart werden ausgerichtet
void SvxIconChoiceCtrl_Impl::AdjustEntryAtGrid( SvxIconChoiceCtrlEntry* pStart )
{
	SvPtrarr aLists;
	pImpCursor->CreateGridAjustData( aLists, pStart );
	const sal_uInt16 nCount = aLists.Count();
	for( sal_uInt16 nCur = 0; nCur < nCount; nCur++ )
		AdjustAtGrid( *(SvPtrarr*)aLists[ nCur ], pStart );
	IcnCursor_Impl::DestroyGridAdjustData( aLists );
	CheckScrollBars();
}

// Richtet eine Zeile aus, erweitert ggf. die Breite; Bricht die Zeile nicht um
void SvxIconChoiceCtrl_Impl::AdjustAtGrid( const SvPtrarr& rRow, SvxIconChoiceCtrlEntry* pStart )
{
	if( !rRow.Count() )
		return;

	sal_Bool bGo;
	if( !pStart )
		bGo = sal_True;
	else
		bGo = sal_False;

	long nCurRight = 0;
	for( sal_uInt16 nCur = 0; nCur < rRow.Count(); nCur++ )
	{
		SvxIconChoiceCtrlEntry* pCur = (SvxIconChoiceCtrlEntry*)rRow[ nCur ];
		if( !bGo && pCur == pStart )
			bGo = sal_True;

		//SvIcnVwDataEntry* pViewData = ICNVIEWDATA(pCur);
		// Massgebend (fuer unser Auge) ist die Bitmap, da sonst
		// durch lange Texte der Eintrag stark springen kann
		const Rectangle& rBoundRect = GetEntryBoundRect( pCur );
		Rectangle aCenterRect( CalcBmpRect( pCur, 0 ));
		if( bGo && !pCur->IsPosLocked() )
		{
			long nWidth = aCenterRect.GetSize().Width();
			Point aNewPos( AdjustAtGrid( aCenterRect, rBoundRect ) );
			while( aNewPos.X() < nCurRight )
				aNewPos.X() += nGridDX;
			if( aNewPos != rBoundRect.TopLeft() )
			{
				SetEntryPos( pCur, aNewPos );
				pCur->SetFlags( ICNVIEW_FLAG_POS_MOVED );
				nFlags |= F_MOVED_ENTRIES;
			}
			nCurRight = aNewPos.X() + nWidth;
		}
		else
		{
			nCurRight = rBoundRect.Right();
		}
	}
}

// Richtet Rect am Grid aus, garantiert jedoch nicht, dass die
// neue Pos. frei ist. Die Pos. kann fuer SetEntryPos verwendet werden.
// Das CenterRect beschreibt den Teil des BoundRects, der fuer
// die Berechnung des Ziel-Rechtecks verwendet wird.
Point SvxIconChoiceCtrl_Impl::AdjustAtGrid( const Rectangle& rCenterRect,
	const Rectangle& rBoundRect ) const
{
	Point aPos( rCenterRect.TopLeft() );
	Size aSize( rCenterRect.GetSize() );

	aPos.X() -= LROFFS_WINBORDER;
	aPos.Y() -= TBOFFS_WINBORDER;

	// align (ref ist mitte des rects)
	short nGridX = (short)((aPos.X()+(aSize.Width()/2)) / nGridDX);
	short nGridY = (short)((aPos.Y()+(aSize.Height()/2)) / nGridDY);
	aPos.X() = nGridX * nGridDX;
	aPos.Y() = nGridY * nGridDY;
	// hor. center
	aPos.X() += (nGridDX - rBoundRect.GetSize().Width() ) / 2;

	aPos.X() += LROFFS_WINBORDER;
	aPos.Y() += TBOFFS_WINBORDER;

	return aPos;
}

void SvxIconChoiceCtrl_Impl::SetEntryTextMode( SvxIconChoiceCtrlTextMode eMode, SvxIconChoiceCtrlEntry* pEntry )
{
	if( !pEntry )
	{
		if( eTextMode != eMode )
		{
			if( eTextMode == IcnShowTextDontKnow )
				eTextMode = IcnShowTextShort;
			eTextMode = eMode;
			Arrange( sal_True );
		}
	}
	else
	{
		if( pEntry->eTextMode != eMode )
		{
			pEntry->eTextMode = eMode;
			InvalidateEntry( pEntry );
			pView->Invalidate( GetEntryBoundRect( pEntry ) );
			AdjustVirtSize( pEntry->aRect );
		}
	}
}

SvxIconChoiceCtrlTextMode SvxIconChoiceCtrl_Impl::GetTextMode( const SvxIconChoiceCtrlEntry* pEntry ) const
{
	if( !pEntry )
		return eTextMode;
	return pEntry->GetTextMode();
}

SvxIconChoiceCtrlTextMode SvxIconChoiceCtrl_Impl::GetEntryTextModeSmart( const SvxIconChoiceCtrlEntry* pEntry ) const
{
	DBG_ASSERT(pEntry,"GetEntryTextModeSmart: Entry not set");
	SvxIconChoiceCtrlTextMode eMode = pEntry->GetTextMode();
	if( eMode == IcnShowTextDontKnow )
		return eTextMode;
	return eMode;
}

void SvxIconChoiceCtrl_Impl::ShowEntryFocusRect( const SvxIconChoiceCtrlEntry* pEntry )
{
	if( !pEntry )
	{
		pView->HideFocus();
	}
	else
	{
		Rectangle aRect ( CalcFocusRect( (SvxIconChoiceCtrlEntry*)pEntry ) );
		/*pView->*/ShowFocus( aRect );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Draw my own focusrect, because the focusrect of the outputdevice has got the inverted color
// of the background. But what will we see, if the the backgroundcolor is gray ? - We will see
// a gray focusrect on a gray background !!!
//
void SvxIconChoiceCtrl_Impl::ShowFocus ( Rectangle& rRect )
{
	Color aBkgColor ( pView->GetBackground().GetColor() );
	Color aPenColor;
	sal_uInt16 nColor = ( aBkgColor.GetRed() + aBkgColor.GetGreen() + aBkgColor.GetBlue() ) / 3;
	if ( nColor > 128 )
		aPenColor.SetColor ( COL_BLACK );
	else
		aPenColor.SetColor( COL_WHITE );

	aFocus.bOn = sal_True;
	aFocus.aPenColor = aPenColor;
	aFocus.aRect = rRect;
}

void SvxIconChoiceCtrl_Impl::HideFocus ()
{
	aFocus.bOn = sal_False;
}

void SvxIconChoiceCtrl_Impl::DrawFocusRect ( OutputDevice* pOut )
{
	pOut->SetLineColor( aFocus.aPenColor );
	pOut->SetFillColor();
	Polygon aPolygon ( aFocus.aRect );

	LineInfo aLineInfo ( LINE_DASH );

	aLineInfo.SetDashLen ( 1 );

	aLineInfo.SetDotLen ( 1L );
	aLineInfo.SetDistance ( 1L );
	aLineInfo.SetDotCount ( 1 );

	pOut->DrawPolyLine ( aPolygon, aLineInfo );
}

sal_Bool SvxIconChoiceCtrl_Impl::IsMnemonicChar( sal_Unicode cChar, sal_uLong& rPos ) const
{
	sal_Bool bRet = sal_False;
    const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();
	sal_uLong nEntryCount = GetEntryCount();
	for ( sal_uLong i = 0; i < nEntryCount; ++i )
	{
		if ( rI18nHelper.MatchMnemonic( GetEntry( i )->GetText(), cChar ) )
		{
			bRet = sal_True;
			rPos = i;
			break;
		}
	}

	return bRet;
}

//
////////////////////////////////////////////////////////////////////////////////////////////////

IMPL_LINK(SvxIconChoiceCtrl_Impl, UserEventHdl, void*, nId )
{
	if( nId == EVENTID_ADJUST_SCROLLBARS )
	{
		nUserEventAdjustScrBars = 0;
		AdjustScrollBars();
	}
	else if( nId == EVENTID_SHOW_CURSOR )
	{
		nUserEventShowCursor = 0;
		ShowCursor( sal_True );
	}
	return 0;
}

void SvxIconChoiceCtrl_Impl::CancelUserEvents()
{
	if( nUserEventAdjustScrBars )
	{
		Application::RemoveUserEvent( nUserEventAdjustScrBars );
		nUserEventAdjustScrBars = 0;
	}
	if( nUserEventShowCursor )
	{
		Application::RemoveUserEvent( nUserEventShowCursor );
		nUserEventShowCursor = 0;
	}
}

void SvxIconChoiceCtrl_Impl::InvalidateEntry( SvxIconChoiceCtrlEntry* pEntry )
{
	if( pEntry == pCursor )
		ShowCursor( sal_False );
	pView->Invalidate( pEntry->aRect );
	Center( pEntry );
	pView->Invalidate( pEntry->aRect );
	if( pEntry == pCursor )
		ShowCursor( sal_True );
}

void SvxIconChoiceCtrl_Impl::EditEntry( SvxIconChoiceCtrlEntry* pEntry )
{
	DBG_ASSERT(pEntry,"EditEntry: Entry not set");
	if( !pEntry )
		return;

	StopEntryEditing( sal_True );
	DELETEZ(pEdit);
	SetNoSelection();

	pCurEditedEntry = pEntry;
	String aEntryText( pView->GetEntryText( pEntry, sal_True ) );
	Rectangle aRect( CalcTextRect( pEntry, 0, sal_True, &aEntryText ) );
	MakeVisible( aRect );
	Point aPos( aRect.TopLeft() );
	aPos = pView->GetPixelPos( aPos );
	aRect.SetPos( aPos );
	pView->HideFocus();
	pEdit = new IcnViewEdit_Impl(
		pView,
		aRect.TopLeft(),
		aRect.GetSize(),
		aEntryText,
		LINK( this, SvxIconChoiceCtrl_Impl, TextEditEndedHdl ) );
}

IMPL_LINK( SvxIconChoiceCtrl_Impl, TextEditEndedHdl, IcnViewEdit_Impl*, EMPTYARG )
{
	DBG_ASSERT(pEdit,"TextEditEnded: pEdit not set");
	if( !pEdit )
	{
		pCurEditedEntry = 0;
		return 0;
	}
	DBG_ASSERT(pCurEditedEntry,"TextEditEnded: pCurEditedEntry not set");

	if( !pCurEditedEntry )
	{
		pEdit->Hide();
		if( pEdit->IsGrabFocus() )
			pView->GrabFocus();
		return 0;
	}

	String aText;
	if ( !pEdit->EditingCanceled() )
		aText = pEdit->GetText();
	else
		aText = pEdit->GetSavedValue();

	if( pView->EditedEntry( pCurEditedEntry, aText, pEdit->EditingCanceled() ) )
		InvalidateEntry( pCurEditedEntry );
	if( !GetSelectionCount() )
		SelectEntry( pCurEditedEntry, sal_True );

	pEdit->Hide();
	if( pEdit->IsGrabFocus() )
		pView->GrabFocus();
	// Das Edit kann nicht hier geloescht werden, weil es noch in einem
	// Handler steht. Es wird im Dtor oder im naechsten EditEntry geloescht.
	pCurEditedEntry = 0;
	return 0;
}

void SvxIconChoiceCtrl_Impl::StopEntryEditing( sal_Bool bCancel )
{
	if( pEdit )
		pEdit->StopEditing( bCancel );
}

void SvxIconChoiceCtrl_Impl::LockEntryPos( SvxIconChoiceCtrlEntry* pEntry, sal_Bool bLock )
{
	if( bLock )
		pEntry->SetFlags( ICNVIEW_FLAG_POS_LOCKED );
	else
		pEntry->ClearFlags( ICNVIEW_FLAG_POS_LOCKED );
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GetFirstSelectedEntry( sal_uLong& rPos ) const
{
	if( !GetSelectionCount() )
		return 0;

	if( (nWinBits & WB_HIGHLIGHTFRAME) && (eSelectionMode == NO_SELECTION) )
	{
		rPos = pView->GetEntryListPos( pCurHighlightFrame );
		return pCurHighlightFrame;
	}

	sal_uLong nCount = aEntries.Count();
	if( !pHead )
	{
		for( sal_uLong nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			if( pEntry->IsSelected() )
			{
				rPos = nCur;
				return pEntry;
			}
		}
	}
	else
	{
		SvxIconChoiceCtrlEntry* pEntry = pHead;
		while( nCount-- )
		{
			if( pEntry->IsSelected() )
			{
				rPos = GetEntryListPos( pEntry );
				return pEntry;
			}
			pEntry = pEntry->pflink;
			if( nCount && pEntry == pHead )
			{
				DBG_ERROR("SvxIconChoiceCtrl_Impl::GetFirstSelectedEntry > Endlosschleife!");
				return 0;
			}
		}
	}
	return 0;
}

// kein Round Robin!
SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GetNextSelectedEntry( sal_uLong& rStartPos ) const
{
	sal_uLong nCount = aEntries.Count();
	if( rStartPos > nCount || !GetSelectionCount() )
		return 0;
	if( !pHead )
	{
		for( sal_uLong nCur = rStartPos+1; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			if( pEntry->IsSelected() )
			{
				rStartPos = nCur;
				return pEntry;
			}
		}
	}
	else
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( rStartPos );
		pEntry = pEntry->pflink;
		while( pEntry != pHead )
		{
			if( pEntry->IsSelected() )
			{
				rStartPos = GetEntryListPos( pEntry );
				return pEntry;
			}
			pEntry = pEntry->pflink;
		}
	}

	rStartPos = 0xffffffff;
	return 0;
}

void SvxIconChoiceCtrl_Impl::SelectAll( sal_Bool bSelect, sal_Bool bPaint )
{
	bPaint = sal_True;

	sal_uLong nCount = aEntries.Count();
	for( sal_uLong nCur = 0; nCur < nCount && (bSelect || GetSelectionCount() ); nCur++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
		SelectEntry( pEntry, bSelect, sal_True, sal_True, bPaint );
	}
	nFlags &= (~F_ADD_MODE);
	pAnchor = 0;
}

void SvxIconChoiceCtrl_Impl::SaveSelection( List** ppList )
{
	if( !*ppList )
		*ppList = new List;
	sal_uLong nPos;
	SvxIconChoiceCtrlEntry* pEntry = GetFirstSelectedEntry( nPos );
	while( pEntry && GetSelectionCount() != (*ppList)->Count() )
	{
		(*ppList)->Insert( pEntry, LIST_APPEND );
		pEntry = GetNextSelectedEntry( nPos );
	}
}

IcnViewEdit_Impl::IcnViewEdit_Impl( SvtIconChoiceCtrl* pParent, const Point& rPos,
	const Size& rSize, const XubString& rData, const Link& rNotifyEditEnd ) :
	MultiLineEdit( pParent, (pParent->GetStyle() & WB_ICON) ? WB_CENTER : WB_LEFT),
	aCallBackHdl( rNotifyEditEnd ),
	bCanceled( sal_False ),
	bAlreadyInCallback( sal_False ),
	bGrabFocus( sal_False )
{
	Font aFont( pParent->GetPointFont() );
	aFont.SetTransparent( sal_False );
	SetControlFont( aFont );
	if( !pParent->HasFontFillColor() )
	{
		Color aColor( pParent->GetBackground().GetColor() );
		SetControlBackground( aColor );
	}
	else
		SetControlBackground( aFont.GetFillColor() );
	SetControlForeground( aFont.GetColor() );
	SetPosPixel( rPos );
	SetSizePixel( CalcAdjustedSize(rSize) );
	SetText( rData );
	SaveValue();

	aAccReturn.InsertItem( IMPICNVIEW_ACC_RETURN, KeyCode(KEY_RETURN) );
	aAccEscape.InsertItem( IMPICNVIEW_ACC_ESCAPE, KeyCode(KEY_ESCAPE) );

	aAccReturn.SetActivateHdl( LINK( this, IcnViewEdit_Impl, ReturnHdl_Impl) );
	aAccEscape.SetActivateHdl( LINK( this, IcnViewEdit_Impl, EscapeHdl_Impl) );
	GetpApp()->InsertAccel( &aAccReturn);//, ACCEL_ALWAYS );
	GetpApp()->InsertAccel( &aAccEscape);//, ACCEL_ALWAYS );
	Show();
	GrabFocus();
}

IcnViewEdit_Impl::~IcnViewEdit_Impl()
{
	if( !bAlreadyInCallback )
	{
		GetpApp()->RemoveAccel( &aAccReturn );
		GetpApp()->RemoveAccel( &aAccEscape );
	}
}

void IcnViewEdit_Impl::CallCallBackHdl_Impl()
{
	aTimer.Stop();
	if ( !bAlreadyInCallback )
	{
		bAlreadyInCallback = sal_True;
		GetpApp()->RemoveAccel( &aAccReturn );
		GetpApp()->RemoveAccel( &aAccEscape );
		Hide();
		aCallBackHdl.Call( this );
	}
}

IMPL_LINK( IcnViewEdit_Impl, Timeout_Impl, Timer*, EMPTYARG )
{
	CallCallBackHdl_Impl();
	return 0;
}

IMPL_LINK( IcnViewEdit_Impl, ReturnHdl_Impl, Accelerator*, EMPTYARG  )
{
	bCanceled = sal_False;
	bGrabFocus = sal_True;
	CallCallBackHdl_Impl();
	return 1;
}

IMPL_LINK( IcnViewEdit_Impl, EscapeHdl_Impl, Accelerator*, EMPTYARG  )
{
	bCanceled = sal_True;
	bGrabFocus = sal_True;
	CallCallBackHdl_Impl();
	return 1;
}

void IcnViewEdit_Impl::KeyInput( const KeyEvent& rKEvt )
{
	KeyCode aCode = rKEvt.GetKeyCode();
	sal_uInt16 nCode = aCode.GetCode();

	switch ( nCode )
	{
		case KEY_ESCAPE:
			bCanceled = sal_True;
			bGrabFocus = sal_True;
			CallCallBackHdl_Impl();
			break;

		case KEY_RETURN:
			bCanceled = sal_False;
			bGrabFocus = sal_True;
			CallCallBackHdl_Impl();
			break;

		default:
			MultiLineEdit::KeyInput( rKEvt );
	}
}

long IcnViewEdit_Impl::PreNotify( NotifyEvent& rNEvt )
{
	if( rNEvt.GetType() == EVENT_LOSEFOCUS )
	{
		if ( !bAlreadyInCallback &&
			((!Application::GetFocusWindow()) || !IsChild(Application::GetFocusWindow())))
		{
			bCanceled = sal_False;
			aTimer.SetTimeout(10);
			aTimer.SetTimeoutHdl(LINK(this,IcnViewEdit_Impl,Timeout_Impl));
			aTimer.Start();
		}
	}
	return 0;
}

void IcnViewEdit_Impl::StopEditing( sal_Bool bCancel )
{
	if ( !bAlreadyInCallback )
	{
		bCanceled = bCancel;
		CallCallBackHdl_Impl();
	}
}

sal_uLong SvxIconChoiceCtrl_Impl::GetEntryListPos( SvxIconChoiceCtrlEntry* pEntry ) const
{
	if( !(nFlags & F_ENTRYLISTPOS_VALID ))
		((SvxIconChoiceCtrl_Impl*)this)->SetListPositions();
	return pEntry->nPos;
}

void SvxIconChoiceCtrl_Impl::SetEntryListPos( SvxIconChoiceCtrlEntry* pListEntry, sal_uLong nNewPos )
{
	sal_uLong nCurPos = GetEntryListPos( pListEntry );
	if( nCurPos == nNewPos )
		return;
	aEntries.List::Remove( nCurPos );
	aEntries.List::Insert( (void*)pListEntry, nNewPos );
	// Eintragspositionen anpassen
	sal_uLong nStart, nEnd;
	if( nNewPos < nCurPos )
	{
		nStart = nNewPos;
		nEnd = nCurPos;
	}
	else
	{
		nStart = nCurPos;
		nEnd = nNewPos;
	}
	for( ; nStart <= nEnd; nStart++ )
	{
		SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nStart );
		pEntry->nPos = nStart;
	}
}

void SvxIconChoiceCtrl_Impl::SetEntryFlags( SvxIconChoiceCtrlEntry* pEntry, sal_uInt16 nEntryFlags )
{
	pEntry->nFlags = nEntryFlags;
	if( nEntryFlags & ICNVIEW_FLAG_POS_MOVED )
		nFlags |= F_MOVED_ENTRIES;
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GoLeftRight( SvxIconChoiceCtrlEntry* pStart, sal_Bool bRight )
{
	return pImpCursor->GoLeftRight( pStart, bRight );
}

SvxIconChoiceCtrlEntry* SvxIconChoiceCtrl_Impl::GoUpDown( SvxIconChoiceCtrlEntry* pStart, sal_Bool bDown )
{
	return pImpCursor->GoUpDown( pStart, bDown );
}

void SvxIconChoiceCtrl_Impl::InitSettings()
{
	const StyleSettings& rStyleSettings = pView->GetSettings().GetStyleSettings();

	if( !pView->HasFont() )
	{
		// Unit aus den Settings ist Point
		Font aFont( rStyleSettings.GetFieldFont() );
		//const Font& rFont = pView->GetFont();
		//if( pView->HasFontTextColor() )
			aFont.SetColor( rStyleSettings.GetWindowTextColor() );
		//if( pView->HasFontFillColor() )
			//aFont.SetFillColor( rFont.GetFillColor() );
		pView->SetPointFont( aFont );
		SetDefaultTextSize();
	}

	//if( !pView->HasFontTextColor() )
		pView->SetTextColor( rStyleSettings.GetFieldTextColor() );
	//if( !pView->HasFontFillColor() )
		pView->SetTextFillColor();

	//if( !pView->HasBackground() )
		pView->SetBackground( rStyleSettings.GetFieldColor());

	long nScrBarSize = rStyleSettings.GetScrollBarSize();
	if( nScrBarSize != nHorSBarHeight || nScrBarSize != nVerSBarWidth )
	{
		nHorSBarHeight = nScrBarSize;
		Size aSize( aHorSBar.GetSizePixel() );
		aSize.Height() = nScrBarSize;
		aHorSBar.Hide();
		aHorSBar.SetSizePixel( aSize );

		nVerSBarWidth = nScrBarSize;
		aSize = aVerSBar.GetSizePixel();
		aSize.Width() = nScrBarSize;
		aVerSBar.Hide();
		aVerSBar.SetSizePixel( aSize );

		Size aOSize( pView->Control::GetOutputSizePixel() );
		PositionScrollBars( aOSize.Width(), aOSize.Height() );
		AdjustScrollBars();
	}
}

EntryList_Impl::EntryList_Impl( SvxIconChoiceCtrl_Impl* pOwner, sal_uInt16 _nInitSize , sal_uInt16 _nReSize ) :
	List( _nInitSize, _nReSize ),
	_pOwner( pOwner )
{
	_pOwner->pHead = 0;
}

EntryList_Impl::EntryList_Impl( SvxIconChoiceCtrl_Impl* pOwner, sal_uInt16 _nBlockSize, sal_uInt16 _nInitSize, sal_uInt16 _nReSize ) :
	List( _nBlockSize, _nInitSize, _nReSize ),
	_pOwner( pOwner )
{
	_pOwner->pHead = 0;
}

EntryList_Impl::~EntryList_Impl()
{
	_pOwner->pHead = 0;
}

void EntryList_Impl::Clear()
{
	_pOwner->pHead = 0;
	List::Clear();
}

void EntryList_Impl::Insert( SvxIconChoiceCtrlEntry* pEntry, sal_uLong nPos )
{
	List::Insert( pEntry, nPos );
	if( _pOwner->pHead )
		pEntry->SetBacklink( _pOwner->pHead->pblink );
}

SvxIconChoiceCtrlEntry* EntryList_Impl::Remove( sal_uLong nPos )
{
	SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)List::Remove( nPos );
	DBG_ASSERT(pEntry,"EntryList_Impl::Remove > Entry not found");
	Removed_Impl( pEntry );
	return pEntry;
}

void EntryList_Impl::Remove( SvxIconChoiceCtrlEntry* pEntry )
{
	List::Remove( (void*)pEntry );
	Removed_Impl( pEntry );
}

void EntryList_Impl::Removed_Impl( SvxIconChoiceCtrlEntry* pEntry )
{
	if( _pOwner->pHead )
	{
		if( _pOwner->pHead == pEntry )
		{
			if( _pOwner->pHead != pEntry->pflink )
				_pOwner->pHead = pEntry->pflink;
			else
			{
				DBG_ASSERT(!Count(),"EntryList_Impl::Remove > Invalid predecessor" );
				_pOwner->pHead = 0;
			}
		}
		pEntry->Unlink();
	}
}

void SvxIconChoiceCtrl_Impl::SetPositionMode( SvxIconChoiceCtrlPositionMode eMode )
{
	sal_uLong nCur;

	if( eMode == ePositionMode )
		return;

	SvxIconChoiceCtrlPositionMode eOldMode = ePositionMode;
	ePositionMode = eMode;
	sal_uLong nCount = aEntries.Count();

	if( eOldMode == IcnViewPositionModeAutoArrange )
	{
		// positionieren wir verschobene Eintraege 'hart' gibts noch Probleme
		// mit ungewollten Ueberlappungen, da diese Eintrage im Arrange
		// nicht beruecksichtigt werden.
#if 1
		if( aEntries.Count() )
			aAutoArrangeTimer.Start();
#else
		if( pHead )
		{
			// verschobene Eintraege 'hart' auf ihre Position setzen
			nCur = nCount;
			SvxIconChoiceCtrlEntry* pEntry = pHead;
			while( nCur )
			{
				SvxIconChoiceCtrlEntry* pPred;
				if( GetEntryPredecessor( pEntry, &pPred ))
					SetEntryFlags( pEntry, ICNVIEW_FLAG_POS_MOVED );
				pEntry = pEntry->pflink;
				nCur--;
			}
			ClearPredecessors();
		}
#endif
		return;
	}

	if( ePositionMode == IcnViewPositionModeAutoArrange )
	{
		List aMovedEntries;
		for( nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry* pEntry = (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nCur );
			if( pEntry->GetFlags() & (ICNVIEW_FLAG_POS_LOCKED | ICNVIEW_FLAG_POS_MOVED))
			{
				SvxIconChoiceCtrlEntry_Impl* pE = new SvxIconChoiceCtrlEntry_Impl(
						pEntry, GetEntryBoundRect( pEntry ));
				aMovedEntries.Insert( pE, LIST_APPEND );
			}
		}
		nCount = aMovedEntries.Count();
		for( nCur = 0; nCur < nCount; nCur++ )
		{
			SvxIconChoiceCtrlEntry_Impl* pE = (SvxIconChoiceCtrlEntry_Impl*)aMovedEntries.GetObject(nCur);
			SetEntryPos( pE->_pEntry, pE->_aPos );
		}
		for( nCur = 0; nCur < nCount; nCur++ )
			delete (SvxIconChoiceCtrlEntry_Impl*)aMovedEntries.GetObject( nCur );
		if( aEntries.Count() )
			aAutoArrangeTimer.Start();
	}
	else if( ePositionMode == IcnViewPositionModeAutoAdjust )
	{
		AdjustEntryAtGrid( 0 );
	}
}

void SvxIconChoiceCtrl_Impl::SetEntryPredecessor( SvxIconChoiceCtrlEntry* pEntry,
	SvxIconChoiceCtrlEntry* pPredecessor )
{
	if( !IsAutoArrange() )
		return;

	if( pEntry == pPredecessor )
		return;

	sal_uLong nPos1 = GetEntryListPos( pEntry );
	if( !pHead )
	{
		if( pPredecessor )
		{
			sal_uLong nPos2 = GetEntryListPos( pPredecessor );
			if( nPos1 == (nPos2 + 1) )
				return; // ist schon Vorgaenger
		}
		else if( !nPos1 )
			return;
	}

	if( !pHead )
		InitPredecessors();

	if( !pPredecessor && pHead == pEntry )
		return; // ist schon der Erste

	sal_Bool bSetHead = sal_False;
	if( !pPredecessor )
	{
		bSetHead = sal_True;
		pPredecessor = pHead->pblink;
	}
	if( pEntry == pHead )
	{
		pHead = pHead->pflink;
		bSetHead = sal_False;
	}
	if( pEntry != pPredecessor )
	{
		pEntry->Unlink();
		pEntry->SetBacklink( pPredecessor );
	}
	if( bSetHead )
		pHead = pEntry;
	pEntry->SetFlags( ICNVIEW_FLAG_PRED_SET );
	aAutoArrangeTimer.Start();
}

sal_Bool SvxIconChoiceCtrl_Impl::GetEntryPredecessor( SvxIconChoiceCtrlEntry* pEntry,
	SvxIconChoiceCtrlEntry** ppPredecessor )
{
	*ppPredecessor = 0;
	if( !pHead )
		return sal_False;
	DBG_ASSERT(pEntry->pblink,"GetEntryPredecessor: Backward link not set");
	DBG_ASSERT(pEntry->pflink,"GetEntryPredecessor: Forward link not set");

	if( pEntry == pHead )
	{
		SvxIconChoiceCtrlEntry* pFirst = (SvxIconChoiceCtrlEntry*)aEntries.GetObject(0);
		if( pFirst != pEntry )
			return sal_True;
		return sal_False;
	}
	*ppPredecessor = pEntry->pblink;
	if( !(pEntry->nFlags & ICNVIEW_FLAG_PRED_SET) &&
		(GetEntryListPos( *ppPredecessor ) + 1) == GetEntryListPos( pEntry ))
		return sal_False;
	return sal_True;
}

SvxIconChoiceCtrlEntry*	SvxIconChoiceCtrl_Impl::FindEntryPredecessor( SvxIconChoiceCtrlEntry* pEntry,
	const Point& rPosTopLeft )
{
	Point aPos( rPosTopLeft ); //TopLeft
	Rectangle aCenterRect( CalcBmpRect( pEntry, &aPos ));
	Point aNewPos( aCenterRect.Center() );
	sal_uLong nGrid = GetPredecessorGrid( aNewPos );
	sal_uLong nCount = aEntries.Count();
	if( nGrid == ULONG_MAX )
		return 0;
	if( nGrid >= nCount )
		nGrid = nCount - 1;
	if( !pHead )
		return (SvxIconChoiceCtrlEntry*)aEntries.GetObject( nGrid );

	SvxIconChoiceCtrlEntry* pCur = pHead; // Grid 0
	// todo: Liste von hinten aufrollen wenn nGrid > nCount/2
	for( sal_uLong nCur = 0; nCur < nGrid; nCur++ )
		pCur = pCur->pflink;

	return pCur;
}

sal_uLong SvxIconChoiceCtrl_Impl::GetPredecessorGrid( const Point& rPos) const
{
	Point aPos( rPos );
	aPos.X() -= LROFFS_WINBORDER;
	aPos.Y() -= TBOFFS_WINBORDER;
	sal_uInt16 nMaxCol = (sal_uInt16)(aVirtOutputSize.Width() / nGridDX);
	if( nMaxCol )
		nMaxCol--;
	sal_uInt16 nGridX = (sal_uInt16)(aPos.X() / nGridDX);
	if( nGridX > nMaxCol )
		nGridX = nMaxCol;
	sal_uInt16 nGridY = (sal_uInt16)(aPos.Y() / nGridDY);
	sal_uInt16 nGridsX = (sal_uInt16)(aOutputSize.Width() / nGridDX);
	sal_uLong nGrid = (nGridY * nGridsX) + nGridX;
	long nMiddle = (nGridX * nGridDX) + (nGridDX / 2);
	if( rPos.X() < nMiddle )
	{
		if( !nGrid )
			nGrid = ULONG_MAX;
		else
			nGrid--;
	}
	return nGrid;
}

void SvxIconChoiceCtrl_Impl::Flush()
{
	if( aAutoArrangeTimer.IsActive() )
	{
		AutoArrangeHdl( 0 );
	}
}

sal_Bool SvxIconChoiceCtrl_Impl::RequestHelp( const HelpEvent& rHEvt )
{
	if ( !(rHEvt.GetMode() & HELPMODE_QUICK ) )
		return sal_False;

	Point aPos( pView->ScreenToOutputPixel(rHEvt.GetMousePosPixel() ) );
	aPos -= pView->GetMapMode().GetOrigin();
	SvxIconChoiceCtrlEntry* pEntry = GetEntry( aPos, sal_True );

	if ( !pEntry )
		return sal_False;

	String sQuickHelpText = pEntry->GetQuickHelpText();
	String aEntryText( pView->GetEntryText( pEntry, sal_False ) );
	Rectangle aTextRect( CalcTextRect( pEntry, 0, sal_False, &aEntryText ) );
	if ( ( !aTextRect.IsInside( aPos ) || !aEntryText.Len() ) && !sQuickHelpText.Len() )
		return sal_False;

	Rectangle aOptTextRect( aTextRect );
	aOptTextRect.Bottom() = LONG_MAX;
	sal_uInt16 nNewFlags = nCurTextDrawFlags;
	nNewFlags &= ~( TEXT_DRAW_CLIP | TEXT_DRAW_ENDELLIPSIS );
	aOptTextRect = pView->GetTextRect( aOptTextRect, aEntryText, nNewFlags );
	if ( aOptTextRect != aTextRect || sQuickHelpText.Len() > 0 )
	{
		//aTextRect.Right() = aTextRect.Left() + aRealSize.Width() + 4;
		Point aPt( aOptTextRect.TopLeft() );
		aPt += pView->GetMapMode().GetOrigin();
		aPt = pView->OutputToScreenPixel( aPt );
		// Border der Tiphilfe abziehen
		aPt.Y() -= 1;
		aPt.X() -= 3;
		aOptTextRect.SetPos( aPt );
		String sHelpText;
		if ( sQuickHelpText.Len() > 0 )
			sHelpText = sQuickHelpText;
		else
			sHelpText = aEntryText;
		Help::ShowQuickHelp( (Window*)pView, aOptTextRect, sHelpText, QUICKHELP_LEFT | QUICKHELP_VCENTER );
	}

	return sal_True;
}

void SvxIconChoiceCtrl_Impl::ClearColumnList()
{
	if( !pColumns )
		return;

	const sal_uInt16 nCount = pColumns->Count();
	for( sal_uInt16 nCur = 0; nCur < nCount; nCur++ )
	{
		SvxIconChoiceCtrlColumnInfo* pInfo = (SvxIconChoiceCtrlColumnInfo*)
			pColumns->GetObject( nCur );
		delete pInfo;
	}
	DELETEZ(pColumns);
}

void SvxIconChoiceCtrl_Impl::SetColumn( sal_uInt16 nIndex, const SvxIconChoiceCtrlColumnInfo& rInfo)
{
	if( !pColumns )
		pColumns = new SvPtrarr;
	while( pColumns->Count() < nIndex + 1 )
		pColumns->Insert( (void*)0, pColumns->Count() );

	SvxIconChoiceCtrlColumnInfo* pInfo =
		(SvxIconChoiceCtrlColumnInfo*)pColumns->GetObject(nIndex);
	if( !pInfo )
	{
		pInfo = new SvxIconChoiceCtrlColumnInfo( rInfo );
		pColumns->Insert( (void*)pInfo, nIndex );
	}
	else
	{
		delete pInfo;
		pInfo = new SvxIconChoiceCtrlColumnInfo( rInfo );
		pColumns->Replace( pInfo, nIndex );
	}

	// HACK(Detail-Modus ist noch nicht vollstaendig implementiert!)
	// dieses Workaround bringts mit einer Spalte zum Fliegen
	if( !nIndex && (nWinBits & WB_DETAILS) )
		nGridDX = pInfo->GetWidth();

	if( GetUpdateMode() )
		Arrange( IsAutoArrange() );
}

const SvxIconChoiceCtrlColumnInfo* SvxIconChoiceCtrl_Impl::GetColumn( sal_uInt16 nIndex ) const
{
	if( !pColumns || nIndex >= pColumns->Count() )
		return 0;
	return (const SvxIconChoiceCtrlColumnInfo*)pColumns->GetObject( nIndex );
}

const SvxIconChoiceCtrlColumnInfo* SvxIconChoiceCtrl_Impl::GetItemColumn( sal_uInt16 nSubItem,
	long& rLeft ) const
{
	rLeft = 0;
	if( !pColumns )
		return 0;
	const sal_uInt16 nCount = pColumns->Count();
	const SvxIconChoiceCtrlColumnInfo* pCol = 0;
	for( sal_uInt16 nCur = 0; nCur < nCount; nCur++ )
	{
		 pCol = (const SvxIconChoiceCtrlColumnInfo*)pColumns->GetObject( nCur );
		if( !pCol || pCol->GetSubItem() == nSubItem )
			return pCol;
		rLeft += pCol->GetWidth();
	}
	return pCol;
}

void SvxIconChoiceCtrl_Impl::DrawHighlightFrame(
	OutputDevice* pOut, const Rectangle& rBmpRect, sal_Bool bHide )
{
	Rectangle aBmpRect( rBmpRect );
	long nBorder = 2;
	if( aImageSize.Width() < 32 )
		nBorder = 1;
	aBmpRect.Right() += nBorder;
	aBmpRect.Left() -= nBorder;
	aBmpRect.Bottom() += nBorder;
	aBmpRect.Top() -= nBorder;

	if ( bHide )
		pView->Invalidate( aBmpRect );
	else
	{
		DecorationView aDecoView( pOut );
		sal_uInt16 nDecoFlags;
		if ( bHighlightFramePressed )
			nDecoFlags = FRAME_HIGHLIGHT_TESTBACKGROUND | FRAME_HIGHLIGHT_IN;
		else
			nDecoFlags = FRAME_HIGHLIGHT_TESTBACKGROUND | FRAME_HIGHLIGHT_OUT;
		aDecoView.DrawHighlightFrame( aBmpRect, nDecoFlags );
	}
}

void SvxIconChoiceCtrl_Impl::SetEntryHighlightFrame( SvxIconChoiceCtrlEntry* pEntry,
	sal_Bool bKeepHighlightFlags )
{
	if( pEntry == pCurHighlightFrame )
		return;

	if( !bKeepHighlightFlags )
		bHighlightFramePressed = sal_False;

	HideEntryHighlightFrame();
	pCurHighlightFrame = pEntry;
	if( pEntry )
	{
		Rectangle aBmpRect( CalcFocusRect(pEntry) );
		DrawHighlightFrame( pView, aBmpRect, sal_False );
	}
}

void SvxIconChoiceCtrl_Impl::HideEntryHighlightFrame()
{
	if( !pCurHighlightFrame )
		return;

	SvxIconChoiceCtrlEntry* pEntry = pCurHighlightFrame;
	pCurHighlightFrame = 0;
	Rectangle aBmpRect( CalcFocusRect(pEntry) );
	DrawHighlightFrame( pView, aBmpRect, sal_True );
}

void SvxIconChoiceCtrl_Impl::CallSelectHandler( SvxIconChoiceCtrlEntry* )
{
	// Bei aktiviertem Single-Click-Modus sollte der Selektionshandler
	// synchron gerufen werden, weil die Selektion automatisch
	// weggenommen wird, wenn der Mauszeiger nicht mehr das Objekt
	// beruehrt. Es kann sonst zu fehlenden Select-Aufrufen kommen,
	// wenn das Objekt aus einer Mausbewegung heraus selektiert wird,
	// weil beim Ausloesen des Timers der Mauszeiger das Objekt u.U.
	// schon verlassen hat.
	// Fuer spezielle Faelle (=>SfxFileDialog!) koennen synchrone
	// Aufrufe auch per WB_NOASYNCSELECTHDL erzwungen werden.
	if( nWinBits & (WB_NOASYNCSELECTHDL | WB_HIGHLIGHTFRAME) )
	{
		pHdlEntry = 0;
		pView->ClickIcon();
		//pView->Select();
	}
	else
		aCallSelectHdlTimer.Start();
}

IMPL_LINK( SvxIconChoiceCtrl_Impl, CallSelectHdlHdl, void*, EMPTYARG )
{
	pHdlEntry = 0;
	pView->ClickIcon();
	//pView->Select();
	return 0;
}

Point SvxIconChoiceCtrl_Impl::GetPopupMenuPosPixel() const
{
	Point aResult;
	if( !GetSelectionCount() )
		return aResult;

	SvxIconChoiceCtrlEntry* pEntry = GetCurEntry();
	if( !pEntry || !pEntry->IsSelected() )
	{
		sal_uLong nNext;
		pEntry = GetFirstSelectedEntry( nNext );
	}
	if( pEntry )
	{
		Rectangle aRect( ((SvxIconChoiceCtrl_Impl*)this)->CalcBmpRect( pEntry ) );
		aResult = aRect.Center();
		aResult = pView->GetPixelPos( aResult );
	}
	return aResult;
}

void SvxIconChoiceCtrl_Impl::SetOrigin( const Point& rPos, sal_Bool bDoNotUpdateWallpaper )
{
	MapMode aMapMode( pView->GetMapMode() );
	aMapMode.SetOrigin( rPos );
	pView->SetMapMode( aMapMode );
	if( !bDoNotUpdateWallpaper )
	{
		sal_Bool bScrollable = pView->GetBackground().IsScrollable();
		if( pView->HasBackground() && !bScrollable )
		{
			Rectangle aRect( GetOutputRect());
			Wallpaper aPaper( pView->GetBackground() );
			aPaper.SetRect( aRect );
			pView->SetBackground( aPaper );
		}
	}
}

sal_uLong SvxIconChoiceCtrl_Impl::GetGridCount( const Size& rSize, sal_Bool bCheckScrBars,
	sal_Bool bSmartScrBar ) const
{
	Size aSize( rSize );
	if( bCheckScrBars && aHorSBar.IsVisible() )
		aSize.Height() -= nHorSBarHeight;
	else if( bSmartScrBar && (nWinBits & WB_ALIGN_LEFT) )
		aSize.Height() -= nHorSBarHeight;

	if( bCheckScrBars && aVerSBar.IsVisible() )
		aSize.Width() -= nVerSBarWidth;
	else if( bSmartScrBar && (nWinBits & WB_ALIGN_TOP) )
		aSize.Width() -= nVerSBarWidth;

	if( aSize.Width() < 0 )
		aSize.Width() = 0;
	if( aSize.Height() < 0 )
		aSize.Height() = 0;

	return IcnGridMap_Impl::GetGridCount( aSize, (sal_uInt16)nGridDX, (sal_uInt16)nGridDY );
}

sal_Bool SvxIconChoiceCtrl_Impl::HandleShortCutKey( const KeyEvent& rKEvt )
{
	StopEditTimer();

	sal_Bool		bRet = sal_False;

	DBG_ASSERT( rKEvt.GetKeyCode().IsMod2(), "*SvxIconChoiceCtrl_Impl::HandleShortCutKey(): no <ALT> pressed!?" );

	sal_Unicode	cChar = rKEvt.GetCharCode();
	sal_uLong		nPos = (sal_uLong)-1;

	if( cChar && IsMnemonicChar( cChar, nPos ) )
	{
		// shortcut is clicked
		SvxIconChoiceCtrlEntry* pNewCursor = GetEntry( nPos );
		SvxIconChoiceCtrlEntry* pOldCursor = pCursor;
		if( pNewCursor != pOldCursor )
		{
			SetCursor_Impl( pOldCursor, pNewCursor, sal_False, sal_False, sal_False );

			if( pNewCursor != NULL )
			{
				pHdlEntry = pNewCursor;
				pCurHighlightFrame = pHdlEntry;
				pView->ClickIcon();
				pCurHighlightFrame = NULL;
			}
		}
		bRet = sal_True;
	}

	return bRet;
}

// -----------------------------------------------------------------------

void SvxIconChoiceCtrl_Impl::CallEventListeners( sal_uLong nEvent, void* pData )
{
    pView->CallImplEventListeners( nEvent, pData );
}


