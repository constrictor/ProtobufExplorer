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

#include "ProtoHighlighter.h"

#include <QSet>

namespace protoexplorer {

namespace {

const QSet<QStringView> &keywords()
{
    static const QSet<QStringView> words{
        u"edition", u"enum", u"export", u"extend", u"extensions", u"group", u"import",
        u"local", u"map", u"message", u"oneof", u"option", u"optional", u"package",
        u"public", u"repeated", u"required", u"reserved", u"returns", u"rpc", u"service",
        u"stream", u"syntax", u"weak"};
    return words;
}

const QSet<QStringView> &scalars()
{
    static const QSet<QStringView> words{
        u"bool", u"bytes", u"double", u"fixed32", u"fixed64", u"float", u"int32", u"int64",
        u"sfixed32", u"sfixed64", u"sint32", u"sint64", u"string", u"uint32", u"uint64"};
    return words;
}

const QSet<QStringView> &constants()
{
    static const QSet<QStringView> words{u"true", u"false", u"inf", u"nan"};
    return words;
}

bool isWordStart(QChar c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c == u'_';
}

bool isDigit(QChar c)
{
    return c >= u'0' && c <= u'9';
}

bool isWord(QChar c)
{
    return isWordStart(c) || isDigit(c);
}

} // namespace

std::vector<SyntaxToken> tokenizeProto(QStringView line, bool &inComment)
{
    std::vector<SyntaxToken> tokens;
    const qsizetype size = line.size();
    qsizetype at = 0;
    QStringView previousWord;
    bool afterNumber = false;
    bool first = true; // no word yet on this line

    const auto add = [&](qsizetype end, SyntaxRole role) {
        if (end > at)
            tokens.push_back({at, end - at, role});
        at = end;
    };
    const auto closeComment = [&](qsizetype from) {
        const qsizetype close = line.indexOf(u"*/", from);
        inComment = close < 0;
        add(inComment ? size : close + 2, SyntaxRole::Comment);
    };

    if (inComment)
        closeComment(0);

    while (at < size) {
        const QChar c = line[at];
        const QChar next = at + 1 < size ? line[at + 1] : QChar();
        if (c == u'/' && next == u'/') {
            add(size, SyntaxRole::Comment);
        } else if (c == u'/' && next == u'*') {
            closeComment(at + 2);
        } else if (c == u'"' || c == u'\'') {
            qsizetype end = at + 1;
            while (end < size && line[end] != c)
                end += line[end] == u'\\' ? 2 : 1;
            add(std::min(end + 1, size), SyntaxRole::String);
            previousWord = {};
            afterNumber = false;
        } else if (isDigit(c) || (c == u'.' && isDigit(next))) {
            qsizetype end = at + 1;
            while (end < size
                   && (isWord(line[end]) || line[end] == u'.'
                       || ((line[end] == u'-' || line[end] == u'+')
                           && (line[end - 1] == u'e' || line[end - 1] == u'E')))) {
                ++end;
            }
            add(end, SyntaxRole::Number);
            previousWord = {};
            afterNumber = true;
        } else if (isWordStart(c)) {
            qsizetype end = at + 1;
            while (end < size && isWord(line[end]))
                ++end;
            const QStringView word = line.sliced(at, end - at);
            qsizetype after = end;
            while (after < size && line[after].isSpace())
                ++after;
            const QChar follows = after < size ? line[after] : QChar();
            const bool qualified = follows == u'.' || (at > 0 && line[at - 1] == u'.');
            // Most of these words are free to use as names, and a name is told by what
            // comes after it.
            const bool named = QStringView(u"=;)]").contains(follows) && !follows.isNull();
            const bool heading = first && (word == u"syntax" || word == u"edition");
            first = false;

            if (qualified) {
                at = end;
            } else if (keywords().contains(word) && (heading || !named) && follows != u','
                       && follows != u':') {
                add(end, SyntaxRole::Keyword);
            } else if (scalars().contains(word) && !named) {
                add(end, SyntaxRole::Type);
            } else if (constants().contains(word) && follows != u'=') {
                add(end, SyntaxRole::Number);
            } else if ((word == u"to" && afterNumber) || (word == u"max" && previousWord == u"to")) {
                add(end, SyntaxRole::Keyword);
            } else {
                at = end;
            }
            previousWord = word;
            afterNumber = false;
        } else {
            if (!c.isSpace()) {
                previousWord = {};
                afterNumber = false;
                first = false;
            }
            ++at;
        }
    }
    return tokens;
}

ProtoHighlighter::ProtoHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

void ProtoHighlighter::setPalette(const QPalette &palette)
{
    m_palette = palette;
    rehighlight();
}

void ProtoHighlighter::highlightBlock(const QString &text)
{
    bool inComment = previousBlockState() == 1;
    for (const SyntaxToken &token : tokenizeProto(text, inComment)) {
        QTextCharFormat format;
        format.setForeground(syntaxColor(m_palette, token.role));
        if (token.role == SyntaxRole::Keyword)
            format.setFontWeight(QFont::Bold);
        else if (token.role == SyntaxRole::Comment)
            format.setFontItalic(true);
        setFormat(int(token.start), int(token.length), format);
    }
    setCurrentBlockState(inComment ? 1 : 0);
}

} // namespace protoexplorer
