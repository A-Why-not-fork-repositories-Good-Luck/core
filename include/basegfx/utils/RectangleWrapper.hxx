/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <basegfx/units/Length.hxx>
#include <tools/gen.hxx>

namespace gfx
{
/**
 * Wrapps tools::Rectangle and Range2DL, to make it easier to incrementally
 * transition to use Range2DL.
 */
class RectangleWrapper
{
private:
    gfx::Range2DL m_aRange;
    mutable tools::Rectangle m_aRectangle;

public:
    RectangleWrapper() = default;

    RectangleWrapper(gfx::Length x1, gfx::Length y1, gfx::Length x2, gfx::Length y2)
        : m_aRange(x1, y1, x2, y2)
    {
    }

    gfx::Range2DL const& getRange() const { return m_aRange; }

    void setRange(gfx::Range2DL const& rRange) { m_aRange = rRange; }

    tools::Rectangle const& getRectangle() const
    {
        m_aRectangle = gfx::length::toRectangleHmm(m_aRange);
        return m_aRectangle;
    }

    void setRectangle(tools::Rectangle const& rRectangle)
    {
        m_aRange = gfx::length::fromRectangleHmm(rRectangle);
    }

    void setSize(sal_Int32 nWidth, sal_Int32 nHeight)
    {
        auto width = gfx::Length::hmm(nWidth - 1);
        auto height = gfx::Length::hmm(nHeight - 1);

        m_aRange.setSize(width, height);
    }

    void move(sal_Int32 nXDelta, sal_Int32 nYDelta)
    {
        auto deltaX = gfx::Length::hmm(nXDelta);
        auto deltaY = gfx::Length::hmm(nYDelta);

        m_aRange.shift(deltaX, deltaY);
    }

    void setPosition(sal_Int32 nX, sal_Int32 nY)
    {
        auto x = gfx::Length::hmm(nX);
        auto y = gfx::Length::hmm(nY);

        m_aRange.setPosition(x, y);
    }
};

} // end namespace gfx

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
