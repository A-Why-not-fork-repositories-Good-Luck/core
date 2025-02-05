/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include "XMLIndexTemplateContext.hxx"
#include "XMLIndexSimpleEntryContext.hxx"
#include "XMLIndexSpanEntryContext.hxx"
#include "XMLIndexTabStopEntryContext.hxx"
#include "XMLIndexBibliographyEntryContext.hxx"
#include "XMLIndexChapterInfoEntryContext.hxx"
#include <xmloff/xmlictxt.hxx>
#include <xmloff/xmlimp.hxx>
#include <xmloff/txtimp.hxx>
#include <xmloff/namespacemap.hxx>
#include <xmloff/xmlnamespace.hxx>
#include <xmloff/xmltoken.hxx>
#include <xmloff/xmluconv.hxx>
#include <xmloff/xmlement.hxx>
#include <tools/debug.hxx>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>
#include <com/sun/star/container/XIndexReplace.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>

#include <algorithm>

using namespace ::xmloff::token;

using ::com::sun::star::beans::XPropertySet;
using ::com::sun::star::beans::PropertyValues;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;
using ::com::sun::star::uno::Any;
using ::com::sun::star::container::XIndexReplace;

XMLIndexTemplateContext::XMLIndexTemplateContext(
    SvXMLImport& rImport,
    Reference<XPropertySet> & rPropSet,
    const SvXMLEnumMapEntry<sal_uInt16>* pLevelNameMap,
    enum XMLTokenEnum eLevelAttrName,
    const char** pLevelStylePropMap,
    const bool* pAllowedTokenTypes,
    bool bT )
:   SvXMLImportContext(rImport)
,   pOutlineLevelNameMap(pLevelNameMap)
,   eOutlineLevelAttrName(eLevelAttrName)
,   pOutlineLevelStylePropMap(pLevelStylePropMap)
,   pAllowedTokenTypesMap(pAllowedTokenTypes)
,   nOutlineLevel(1)    // all indices have level 1 (0 is for header)
,   bStyleNameOK(false)
,   bOutlineLevelOK(false)
,   bTOC( bT )
,   rPropertySet(rPropSet)
{
    DBG_ASSERT( ((XML_TOKEN_INVALID != eLevelAttrName) &&  (nullptr != pLevelNameMap))
                || ((XML_TOKEN_INVALID == eLevelAttrName) &&  (nullptr == pLevelNameMap)),
                "need both, attribute name and value map, or neither" );
    SAL_WARN_IF( nullptr == pOutlineLevelStylePropMap, "xmloff", "need property name map" );
    SAL_WARN_IF( nullptr == pAllowedTokenTypes, "xmloff", "need allowed tokens map" );

    // no map for outline-level? then use 1
    if (nullptr == pLevelNameMap)
    {
        nOutlineLevel = 1;
        bOutlineLevelOK = true;
    }
}

XMLIndexTemplateContext::~XMLIndexTemplateContext()
{
}


void XMLIndexTemplateContext::addTemplateEntry(
    const PropertyValues& aValues)
{
    aValueVector.push_back(aValues);
}


void XMLIndexTemplateContext::startFastElement(
    sal_Int32 /*nElement*/,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList )
{
    // process two attributes: style-name, outline-level
    for( auto& aIter : sax_fastparser::castToFastAttributeList(xAttrList) )
    {
        if(aIter.getToken() == XML_ELEMENT(TEXT, XML_STYLE_NAME))
        {
            // style name
            sStyleName = aIter.toString();
            bStyleNameOK = true;
        }
        else if (aIter.getToken() == XML_ELEMENT(TEXT, eOutlineLevelAttrName))
        {
            // we have an attr name! Then see if we have the attr, too.
            // outline level
            sal_uInt16 nTmp;
            if (SvXMLUnitConverter::convertEnum(nTmp, aIter.toView(), pOutlineLevelNameMap))
            {
                nOutlineLevel = nTmp;
                bOutlineLevelOK = true;
            }
            // else: illegal value -> ignore
        }
        // else: attribute not in text namespace -> ignore
    }
}

void XMLIndexTemplateContext::endFastElement(sal_Int32 )
{
    if (!bOutlineLevelOK)
        return;

    const sal_Int32 nCount = aValueVector.size();
    Sequence<PropertyValues> aValueSequence(nCount);
    std::copy(aValueVector.begin(), aValueVector.end(), aValueSequence.getArray());

    // get LevelFormat IndexReplace ...
    Any aAny = rPropertySet->getPropertyValue("LevelFormat");
    Reference<XIndexReplace> xIndexReplace;
    aAny >>= xIndexReplace;

    // ... and insert
    xIndexReplace->replaceByIndex(nOutlineLevel, Any(aValueSequence));

    if (!bStyleNameOK)
        return;

    const char* pStyleProperty =
        pOutlineLevelStylePropMap[nOutlineLevel];

    DBG_ASSERT(nullptr != pStyleProperty, "need property name");
    if (nullptr == pStyleProperty)
        return;

    OUString sDisplayStyleName =
            GetImport().GetStyleDisplayName(
            XmlStyleFamily::TEXT_PARAGRAPH,
            sStyleName );
    // #i50288#: Check if style exists
    const Reference < css::container::XNameContainer > & rStyles =
        GetImport().GetTextImport()->GetParaStyles();
    if( rStyles.is() &&
        rStyles->hasByName( sDisplayStyleName ) )
    {
        rPropertySet->setPropertyValue(
            OUString::createFromAscii(pStyleProperty), css::uno::Any(sDisplayStyleName));
    }
}

namespace {
/// template token types; used for aTokenTypeMap parameter
enum TemplateTokenType
{
    XML_TOK_INDEX_TYPE_ENTRY_TEXT = 0,
    XML_TOK_INDEX_TYPE_TAB_STOP,
    XML_TOK_INDEX_TYPE_TEXT,
    XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    XML_TOK_INDEX_TYPE_CHAPTER,
    XML_TOK_INDEX_TYPE_LINK_START,
    XML_TOK_INDEX_TYPE_LINK_END,
    XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};

}

SvXMLEnumMapEntry<TemplateTokenType> const aTemplateTokenTypeMap[] =
{
    { XML_INDEX_ENTRY_TEXT,         XML_TOK_INDEX_TYPE_ENTRY_TEXT },
    { XML_INDEX_ENTRY_TAB_STOP,     XML_TOK_INDEX_TYPE_TAB_STOP },
    { XML_INDEX_ENTRY_SPAN,         XML_TOK_INDEX_TYPE_TEXT },
    { XML_INDEX_ENTRY_PAGE_NUMBER,  XML_TOK_INDEX_TYPE_PAGE_NUMBER },
    { XML_INDEX_ENTRY_CHAPTER,      XML_TOK_INDEX_TYPE_CHAPTER },
    { XML_INDEX_ENTRY_LINK_START,   XML_TOK_INDEX_TYPE_LINK_START },
    { XML_INDEX_ENTRY_LINK_END,     XML_TOK_INDEX_TYPE_LINK_END },
    { XML_INDEX_ENTRY_BIBLIOGRAPHY, XML_TOK_INDEX_TYPE_BIBLIOGRAPHY },
    { XML_TOKEN_INVALID, TemplateTokenType(0) }
};

css::uno::Reference< css::xml::sax::XFastContextHandler > XMLIndexTemplateContext::createFastChildContext(
    sal_Int32 nElement,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& )
{
    SvXMLImportContext* pContext = nullptr;

    if (IsTokenInNamespace(nElement, XML_NAMESPACE_TEXT) || IsTokenInNamespace(nElement, XML_NAMESPACE_LO_EXT))
    {
        TemplateTokenType nToken;
        if (SvXMLUnitConverter::convertEnum(nToken, SvXMLImport::getNameFromToken(nElement),
                                            aTemplateTokenTypeMap))
        {
            // can this index accept this kind of token?
            if (pAllowedTokenTypesMap[nToken])
            {
                switch (nToken)
                {
                    case XML_TOK_INDEX_TYPE_ENTRY_TEXT:
                        pContext = new XMLIndexSimpleEntryContext(
                            GetImport(), "TokenEntryText", *this);
                        break;

                    case XML_TOK_INDEX_TYPE_PAGE_NUMBER:
                        pContext = new XMLIndexSimpleEntryContext(
                            GetImport(), "TokenPageNumber", *this);
                        break;

                    case XML_TOK_INDEX_TYPE_LINK_START:
                        pContext = new XMLIndexSimpleEntryContext(
                            GetImport(), "TokenHyperlinkStart", *this);
                        break;

                    case XML_TOK_INDEX_TYPE_LINK_END:
                        pContext = new XMLIndexSimpleEntryContext(
                            GetImport(), "TokenHyperlinkEnd", *this);
                        break;

                    case XML_TOK_INDEX_TYPE_TEXT:
                        pContext = new XMLIndexSpanEntryContext(
                            GetImport(), *this);
                        break;

                    case XML_TOK_INDEX_TYPE_TAB_STOP:
                        pContext = new XMLIndexTabStopEntryContext(
                            GetImport(), *this);
                        break;

                    case XML_TOK_INDEX_TYPE_BIBLIOGRAPHY:
                        pContext = new XMLIndexBibliographyEntryContext(
                            GetImport(), *this);
                        break;

                    case XML_TOK_INDEX_TYPE_CHAPTER:
                        pContext = new XMLIndexChapterInfoEntryContext(
                            GetImport(), *this, bTOC );
                        break;

                    default:
                        // ignore!
                        break;
                }
            }
        }
    }

    // ignore unknown
    return pContext;
}


// maps for the XMLIndexTemplateContext constructor


// table of content and user defined index:

const SvXMLEnumMapEntry<sal_uInt16> aSvLevelNameTOCMap[] =
{
    { XML_1, 1 },
    { XML_2, 2 },
    { XML_3, 3 },
    { XML_4, 4 },
    { XML_5, 5 },
    { XML_6, 6 },
    { XML_7, 7 },
    { XML_8, 8 },
    { XML_9, 9 },
    { XML_10, 10 },
    { XML_TOKEN_INVALID, 0 }
};

const char* aLevelStylePropNameTOCMap[] =
    { nullptr, "ParaStyleLevel1", "ParaStyleLevel2", "ParaStyleLevel3",
          "ParaStyleLevel4", "ParaStyleLevel5", "ParaStyleLevel6",
          "ParaStyleLevel7", "ParaStyleLevel8", "ParaStyleLevel9",
          "ParaStyleLevel10", nullptr };

const bool aAllowedTokenTypesTOC[] =
{
    true,       // XML_TOK_INDEX_TYPE_ENTRY_TEXT =
    true,       // XML_TOK_INDEX_TYPE_TAB_STOP,
    true,       // XML_TOK_INDEX_TYPE_TEXT,
    true,       // XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    true,       // XML_TOK_INDEX_TYPE_CHAPTER,
    true,       // XML_TOK_INDEX_TYPE_LINK_START,
    true,       // XML_TOK_INDEX_TYPE_LINK_END,
    false       // XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};

const bool aAllowedTokenTypesUser[] =
{
    true,       // XML_TOK_INDEX_TYPE_ENTRY_TEXT =
    true,       // XML_TOK_INDEX_TYPE_TAB_STOP,
    true,       // XML_TOK_INDEX_TYPE_TEXT,
    true,       // XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    true,       // XML_TOK_INDEX_TYPE_CHAPTER,
    true,       // XML_TOK_INDEX_TYPE_LINK_START,
    true,       // XML_TOK_INDEX_TYPE_LINK_END,
    false       // XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};


// alphabetical index

const SvXMLEnumMapEntry<sal_uInt16> aLevelNameAlphaMap[] =
{
    { XML_SEPARATOR, 1 },
    { XML_1, 2 },
    { XML_2, 3 },
    { XML_3, 4 },
    { XML_TOKEN_INVALID, 0 }
};

const char* aLevelStylePropNameAlphaMap[] =
    { nullptr, "ParaStyleSeparator", "ParaStyleLevel1", "ParaStyleLevel2",
          "ParaStyleLevel3", nullptr };

const bool aAllowedTokenTypesAlpha[] =
{
    true,       // XML_TOK_INDEX_TYPE_ENTRY_TEXT =
    true,       // XML_TOK_INDEX_TYPE_TAB_STOP,
    true,       // XML_TOK_INDEX_TYPE_TEXT,
    true,       // XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    true,       // XML_TOK_INDEX_TYPE_CHAPTER,
    false,      // XML_TOK_INDEX_TYPE_LINK_START,
    false,      // XML_TOK_INDEX_TYPE_LINK_END,
    false       // XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};


// bibliography index:

const SvXMLEnumMapEntry<sal_uInt16> aLevelNameBibliographyMap[] =
{
    { XML_ARTICLE, 1 },
    { XML_BOOK, 2 },
    { XML_BOOKLET, 3 },
    { XML_CONFERENCE, 4 },
    { XML_CUSTOM1, 5 },
    { XML_CUSTOM2, 6 },
    { XML_CUSTOM3, 7 },
    { XML_CUSTOM4, 8 },
    { XML_CUSTOM5, 9 },
    { XML_EMAIL, 10 },
    { XML_INBOOK, 11 },
    { XML_INCOLLECTION, 12 },
    { XML_INPROCEEDINGS, 13 },
    { XML_JOURNAL, 14 },
    { XML_MANUAL, 15 },
    { XML_MASTERSTHESIS, 16 },
    { XML_MISC, 17 },
    { XML_PHDTHESIS, 18 },
    { XML_PROCEEDINGS, 19 },
    { XML_TECHREPORT, 20 },
    { XML_UNPUBLISHED, 21 },
    { XML_WWW, 22 },
    { XML_TOKEN_INVALID, 0 }
};

// TODO: replace with real property names, when available
const char* aLevelStylePropNameBibliographyMap[] =
{
    nullptr, "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", "ParaStyleLevel1", "ParaStyleLevel1",
    "ParaStyleLevel1", nullptr };

const bool aAllowedTokenTypesBibliography[] =
{
    true,       // XML_TOK_INDEX_TYPE_ENTRY_TEXT =
    true,       // XML_TOK_INDEX_TYPE_TAB_STOP,
    true,       // XML_TOK_INDEX_TYPE_TEXT,
    true,       // XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    false,      // XML_TOK_INDEX_TYPE_CHAPTER,
    false,      // XML_TOK_INDEX_TYPE_LINK_START,
    false,      // XML_TOK_INDEX_TYPE_LINK_END,
    true        // XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};


// table, illustration and object index

// no name map
const SvXMLEnumMapEntry<sal_uInt16>* aLevelNameTableMap = nullptr;

const char* aLevelStylePropNameTableMap[] =
    { nullptr, "ParaStyleLevel1", nullptr };

const bool aAllowedTokenTypesTable[] =
{
    true,       // XML_TOK_INDEX_TYPE_ENTRY_TEXT =
    true,       // XML_TOK_INDEX_TYPE_TAB_STOP,
    true,       // XML_TOK_INDEX_TYPE_TEXT,
    true,       // XML_TOK_INDEX_TYPE_PAGE_NUMBER,
    true,       // XML_TOK_INDEX_TYPE_CHAPTER,
    true,       // XML_TOK_INDEX_TYPE_LINK_START,
    true,       // XML_TOK_INDEX_TYPE_LINK_END,
    false       // XML_TOK_INDEX_TYPE_BIBLIOGRAPHY
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
