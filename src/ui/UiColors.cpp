// protobufexplorer — a visual explorer for Protocol Buffers schemas.
// Copyright (C) 2026 Valentyn Pavliuchenko
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Adapted from loftail (Copyright (C) 2026 Valentyn Pavliuchenko, GPL-3.0-or-later).

#include "UiColors.h"

#include <cmath>

namespace protoexplorer {

namespace {

qreal relativeLuminance(const QColor &color)
{
    const auto channel = [](qreal value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF())
        + 0.0722 * channel(color.blueF());
}

QColor mix(const QColor &from, const QColor &to, float amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

} // namespace

bool isDarkPalette(const QPalette &palette)
{
    return palette.color(QPalette::Base).lightness() < palette.color(QPalette::Text).lightness();
}

qreal contrastRatio(const QColor &a, const QColor &b)
{
    const qreal la = relativeLuminance(a);
    const qreal lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor errorColor(const QPalette &palette)
{
    return QColor::fromRgb(isDarkPalette(palette) ? 0xff7b6e : 0xc0392b);
}

QColor mutedColor(const QPalette &palette)
{
    return mix(palette.color(QPalette::WindowText), palette.color(QPalette::Window), 0.35F);
}

QColor dimColor(const QPalette &palette)
{
    return mix(palette.color(QPalette::Text), palette.color(QPalette::Base), 0.45F);
}

QColor markColor(const QPalette &palette)
{
    return mix(palette.color(QPalette::Base), palette.color(QPalette::Highlight), 0.22F);
}

QColor syntaxColor(const QPalette &palette, SyntaxRole role)
{
    const bool dark = isDarkPalette(palette);
    switch (role) {
    case SyntaxRole::Comment: return QColor::fromRgb(dark ? 0x8fbf8f : 0x4a7a4a);
    case SyntaxRole::String:  return QColor::fromRgb(dark ? 0x6bd18a : 0x0f7a3d);
    case SyntaxRole::Number:  return QColor::fromRgb(dark ? 0xe0a253 : 0x8a4b00);
    case SyntaxRole::Keyword: return QColor::fromRgb(dark ? 0x6fb8e8 : 0x0b5f96);
    case SyntaxRole::Type:    return QColor::fromRgb(dark ? 0xc9a2f2 : 0x7a3e9d);
    }
    return palette.color(QPalette::Text);
}

} // namespace protoexplorer
