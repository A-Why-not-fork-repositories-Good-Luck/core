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


#include <svl/itempool.hxx>
#include <svl/itemset.hxx>
#include <svl/setitem.hxx>
#include <svl/poolcach.hxx>
#include <tools/debug.hxx>

SfxItemPoolCache::SfxItemPoolCache( SfxItemPool *pItemPool,
                                    const SfxPoolItem *pPutItem ):
    pPool(pItemPool),
    pSetToPut( nullptr ),
    pItemToPut( &pItemPool->Put(*pPutItem) )
{
    DBG_ASSERT(pItemPool, "No Pool provided");
}


SfxItemPoolCache::SfxItemPoolCache( SfxItemPool *pItemPool,
                                    const SfxItemSet *pPutSet ):
    pPool(pItemPool),
    pSetToPut( pPutSet ),
    pItemToPut( nullptr )
{
    DBG_ASSERT(pItemPool, "No Pool provided");
}


SfxItemPoolCache::~SfxItemPoolCache()
{
    for (const SfxItemModifyImpl & rImpl : m_aCache) {
        pPool->Remove( *rImpl.pPoolItem );
        pPool->Remove( *rImpl.pOrigItem );
    }

    if ( pItemToPut )
        pPool->Remove( *pItemToPut );
}


const SfxSetItem& SfxItemPoolCache::ApplyTo( const SfxSetItem &rOrigItem )
{
    DBG_ASSERT( pPool == rOrigItem.GetItemSet().GetPool(), "invalid Pool" );
    DBG_ASSERT( IsDefaultItem( &rOrigItem ) || IsPooledItem( &rOrigItem ),
                "original not in pool" );

    // Find whether this Transformations ever occurred
    for (const SfxItemModifyImpl & rMapEntry : m_aCache)
    {
        if ( rMapEntry.pOrigItem == &rOrigItem )
        {
            // Did anything change at all?
            if ( rMapEntry.pPoolItem != &rOrigItem )
            {
                rMapEntry.pPoolItem->AddRef(2); // One for the cache
                pPool->Put( rOrigItem );    //FIXME: AddRef?
            }
            return *rMapEntry.pPoolItem;
        }
    }

    // Insert the new attributes in a new Set
    std::unique_ptr<SfxSetItem> pNewItem(rOrigItem.Clone());
    if ( pItemToPut )
    {
        pNewItem->GetItemSet().Put( *pItemToPut );
        DBG_ASSERT( &pNewItem->GetItemSet().Get( pItemToPut->Which() ) == pItemToPut,
                    "wrong item in temporary set" );
    }
    else
        pNewItem->GetItemSet().Put( *pSetToPut );
    const SfxSetItem* pNewPoolItem = &pPool->Put( std::move(pNewItem) );

    // Adapt refcount; one each for the cache
    pNewPoolItem->AddRef( pNewPoolItem != &rOrigItem ? 2 : 1 );
    pPool->Put( rOrigItem );    //FIXME: AddRef?

    // Add the transformation to the cache
    SfxItemModifyImpl aModify;
    aModify.pOrigItem = &rOrigItem;
    aModify.pPoolItem = const_cast<SfxSetItem*>(pNewPoolItem);
    m_aCache.push_back( aModify );

    DBG_ASSERT( !pItemToPut ||
                &pNewPoolItem->GetItemSet().Get( pItemToPut->Which() ) == pItemToPut,
                "wrong item in resulting set" );

    return *pNewPoolItem;
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
