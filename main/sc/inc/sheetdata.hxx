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



#ifndef SC_SHEETDATA_HXX
#define SC_SHEETDATA_HXX

#include <xmloff/maptype.hxx>
#include <editeng/editdata.hxx>
#include <vector>
#include <hash_set>

#include "address.hxx"

class ScAddress;
class SvXMLNamespaceMap;


struct ScStreamEntry
{
    sal_Int32   mnStartOffset;
    sal_Int32   mnEndOffset;

                ScStreamEntry() :
                    mnStartOffset(-1),
                    mnEndOffset(-1)
                {
                }

                ScStreamEntry( sal_Int32 nStart, sal_Int32 nEnd ) :
                    mnStartOffset(nStart),
                    mnEndOffset(nEnd)
                {
                }
};

struct ScCellStyleEntry
{
    rtl::OUString   maName;
    ScAddress       maCellPos;

                ScCellStyleEntry( const rtl::OUString& rName, const ScAddress& rPos ) :
                    maName(rName),
                    maCellPos(rPos)
                {
                }
};

struct ScNoteStyleEntry
{
    rtl::OUString   maStyleName;
    rtl::OUString   maTextStyle;
    ScAddress       maCellPos;

                ScNoteStyleEntry( const rtl::OUString& rStyle, const rtl::OUString& rText, const ScAddress& rPos ) :
                    maStyleName(rStyle),
                    maTextStyle(rText),
                    maCellPos(rPos)
                {
                }
};

struct ScTextStyleEntry
{
    rtl::OUString   maName;
    ScAddress       maCellPos;
    ESelection      maSelection;

                ScTextStyleEntry( const rtl::OUString& rName, const ScAddress& rPos, const ESelection& rSel ) :
                    maName(rName),
                    maCellPos(rPos),
                    maSelection(rSel)
                {
                }
};

struct ScLoadedNamespaceEntry
{
    rtl::OUString   maPrefix;
    rtl::OUString   maName;
    sal_uInt16      mnKey;

                ScLoadedNamespaceEntry( const rtl::OUString& rPrefix, const rtl::OUString& rName, sal_uInt16 nKey ) :
                    maPrefix(rPrefix),
                    maName(rName),
                    mnKey(nKey)
                {
                }
};

class ScSheetSaveData
{
    std::hash_set<rtl::OUString, rtl::OUStringHash>  maInitialPrefixes;
    std::vector<ScLoadedNamespaceEntry>              maLoadedNamespaces;

    std::vector<ScCellStyleEntry> maCellStyles;
    std::vector<ScCellStyleEntry> maColumnStyles;
    std::vector<ScCellStyleEntry> maRowStyles;
    std::vector<ScCellStyleEntry> maTableStyles;
    std::vector<ScNoteStyleEntry> maNoteStyles;
    std::vector<ScTextStyleEntry> maNoteParaStyles;
    std::vector<ScTextStyleEntry> maNoteTextStyles;
    std::vector<ScTextStyleEntry> maTextStyles;
    std::vector<bool>          maBlocked;
    std::vector<ScStreamEntry> maStreamEntries;
    std::vector<ScStreamEntry> maSaveEntries;
    sal_Int32   mnStartTab;
    sal_Int32   mnStartOffset;

    ScNoteStyleEntry    maPreviousNote;

    bool                mbInSupportedSave;

public:
                ScSheetSaveData();
                ~ScSheetSaveData();

    void        AddCellStyle( const rtl::OUString& rName, const ScAddress& rCellPos );
    void        AddColumnStyle( const rtl::OUString& rName, const ScAddress& rCellPos );
    void        AddRowStyle( const rtl::OUString& rName, const ScAddress& rCellPos );
    void        AddTableStyle( const rtl::OUString& rName, const ScAddress& rCellPos );

    void        HandleNoteStyles( const rtl::OUString& rStyleName, const rtl::OUString& rTextName, const ScAddress& rCellPos );
    void        AddNoteContentStyle( sal_uInt16 nFamily, const rtl::OUString& rName, const ScAddress& rCellPos, const ESelection& rSelection );

    void        AddTextStyle( const rtl::OUString& rName, const ScAddress& rCellPos, const ESelection& rSelection );

    void        BlockSheet( sal_Int32 nTab );
    bool        IsSheetBlocked( sal_Int32 nTab ) const;

    void        AddStreamPos( sal_Int32 nTab, sal_Int32 nStartOffset, sal_Int32 nEndOffset );
    void        GetStreamPos( sal_Int32 nTab, sal_Int32& rStartOffset, sal_Int32& rEndOffset ) const;
    bool        HasStreamPos( sal_Int32 nTab ) const;

    void        StartStreamPos( sal_Int32 nTab, sal_Int32 nStartOffset );
    void        EndStreamPos( sal_Int32 nEndOffset );

    bool        HasStartPos() const { return mnStartTab >= 0; }

    void        ResetSaveEntries();
    void        AddSavePos( sal_Int32 nTab, sal_Int32 nStartOffset, sal_Int32 nEndOffset );
    void        UseSaveEntries();

    void        StoreInitialNamespaces( const SvXMLNamespaceMap& rNamespaces );
    void        StoreLoadedNamespaces( const SvXMLNamespaceMap& rNamespaces );
    bool        AddLoadedNamespaces( SvXMLNamespaceMap& rNamespaces ) const;

    const std::vector<ScCellStyleEntry>& GetCellStyles() const   { return maCellStyles; }
    const std::vector<ScCellStyleEntry>& GetColumnStyles() const { return maColumnStyles; }
    const std::vector<ScCellStyleEntry>& GetRowStyles() const    { return maRowStyles; }
    const std::vector<ScCellStyleEntry>& GetTableStyles() const  { return maTableStyles; }
    const std::vector<ScNoteStyleEntry>& GetNoteStyles() const   { return maNoteStyles; }
    const std::vector<ScTextStyleEntry>& GetNoteParaStyles() const { return maNoteParaStyles; }
    const std::vector<ScTextStyleEntry>& GetNoteTextStyles() const { return maNoteTextStyles; }
    const std::vector<ScTextStyleEntry>& GetTextStyles() const   { return maTextStyles; }

    bool        IsInSupportedSave() const;
    void        SetInSupportedSave( bool bSet );
};

#endif

