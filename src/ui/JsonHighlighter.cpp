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

#include "JsonHighlighter.h"

namespace protoexplorer {

namespace {

// A whole document on one line would take longer to colour than anyone will read of it.
constexpr qsizetype kMaxLine = 20'000;

bool isWord(QChar c)
{
    return c.isLetterOrNumber() || c == u'_' || c == u'.' || c == u'+' || c == u'-';
}

} // namespace

std::vector<SyntaxToken> tokenizeJson(QStringView line)
{
    std::vector<SyntaxToken> tokens;
    const qsizetype size = line.size();
    qsizetype at = 0;
    while (at < size) {
        const QChar c = line[at];
        qsizetype end = at + 1;
        if (c == u'"') {
            while (end < size && line[end] != u'"')
                end += line[end] == u'\\' ? 2 : 1;
            end = std::min(end + 1, size);
            qsizetype after = end;
            while (after < size && line[after].isSpace())
                ++after;
            const bool key = after < size && line[after] == u':';
            tokens.push_back({at, end - at, key ? SyntaxRole::Keyword : SyntaxRole::String});
        } else if (isWord(c)) {
            while (end < size && isWord(line[end]))
                ++end;
            const QStringView word = line.sliced(at, end - at);
            if (word == u"true" || word == u"false" || word == u"null")
                tokens.push_back({at, end - at, SyntaxRole::Type});
            else if (c == u'-' || (c >= u'0' && c <= u'9'))
                tokens.push_back({at, end - at, SyntaxRole::Number});
        }
        at = end;
    }
    return tokens;
}

JsonHighlighter::JsonHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

void JsonHighlighter::setPalette(const QPalette &palette)
{
    m_palette = palette;
    rehighlight();
}

void JsonHighlighter::highlightBlock(const QString &text)
{
    if (text.size() > kMaxLine)
        return;
    for (const SyntaxToken &token : tokenizeJson(text))
        setFormat(int(token.start), int(token.length), syntaxColor(m_palette, token.role));
}

} // namespace protoexplorer
