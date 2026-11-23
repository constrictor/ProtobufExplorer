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

#pragma once

#include <QColor>
#include <QPalette>

namespace protoexplorer {

// Colours worked out from the palette in force, so that they read on any theme.

bool isDarkPalette(const QPalette &palette);

// The WCAG contrast ratio, from 1 to 21.
qreal contrastRatio(const QColor &a, const QColor &b);

QColor errorColor(const QPalette &palette);
QColor mutedColor(const QPalette &palette);
// For rows that are there for completeness, while still to be read.
QColor dimColor(const QPalette &palette);
// Behind text that something is pointing at.
QColor markColor(const QPalette &palette);

enum class SyntaxRole : quint8 { Comment, String, Number, Keyword, Type };

QColor syntaxColor(const QPalette &palette, SyntaxRole role);

} // namespace protoexplorer
