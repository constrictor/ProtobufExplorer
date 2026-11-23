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

#pragma once

#include "UiColors.h"

#include <QPalette>
#include <QStringView>
#include <QSyntaxHighlighter>

#include <vector>

namespace protoexplorer {

struct SyntaxToken
{
    qsizetype start = 0;
    qsizetype length = 0;
    SyntaxRole role = SyntaxRole::Comment;
};

// The parts of one line worth colouring, in order. `inComment` says whether the line
// begins inside a block comment and, on return, whether the next one does.
std::vector<SyntaxToken> tokenizeProto(QStringView line, bool &inComment);

class ProtoHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit ProtoHighlighter(QTextDocument *document);

    void setPalette(const QPalette &palette);

protected:
    void highlightBlock(const QString &text) override;

private:
    QPalette m_palette;
};

} // namespace protoexplorer
