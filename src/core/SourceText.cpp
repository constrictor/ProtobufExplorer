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

#include "SourceText.h"

#include <QStringList>

#include <algorithm>

namespace protoexplorer {

namespace {

constexpr int kTabWidth = 8; // the tokenizer's, not the reader's
constexpr qsizetype kBomSize = 3;

bool isSpace(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

// The whitespace `line` starts with, in full.
QStringView indentOf(const QString &line)
{
    qsizetype n = 0;
    while (n < line.size() && (line[n] == u' ' || line[n] == u'\t'))
        ++n;
    return QStringView(line).left(n);
}

QStringView commonPrefix(QStringView a, QStringView b)
{
    qsizetype n = 0;
    while (n < a.size() && n < b.size() && a[n] == b[n])
        ++n;
    return a.left(n);
}

} // namespace

SourceText::SourceText(QByteArray bytes)
    : m_bytes(std::move(bytes))
{
    indexLines();
    indexComments();
}

void SourceText::indexLines()
{
    m_lineStarts.push_back(0);
    for (qsizetype i = 0; i < m_bytes.size(); ++i) {
        if (m_bytes[i] == '\n')
            m_lineStarts.push_back(i + 1);
    }
}

void SourceText::indexComments()
{
    const qsizetype size = m_bytes.size();
    qsizetype i = 0;
    while (i < size) {
        const char c = m_bytes[i];
        const char next = i + 1 < size ? m_bytes[i + 1] : '\0';
        if (c == '"' || c == '\'') {
            // A literal cannot span lines, so an unterminated one ends with its line
            // rather than swallowing the rest of the file.
            ++i;
            while (i < size && m_bytes[i] != c && m_bytes[i] != '\n')
                i += m_bytes[i] == '\\' ? 2 : 1;
            ++i;
        } else if (c == '/' && next == '/') {
            const qsizetype end = lineEnd(i);
            m_comments.push_back({i, end});
            i = end;
        } else if (c == '/' && next == '*') {
            const qsizetype close = m_bytes.indexOf("*/", i + 2);
            const qsizetype end = close < 0 ? size : close + 2;
            m_comments.push_back({i, end});
            i = end;
        } else {
            ++i;
        }
    }
}

qsizetype SourceText::offsetAt(int line, int column) const
{
    qsizetype offset = lineStart(line);
    const qsizetype end = lineEnd(offset);
    int at = 0;
    while (offset < end && at < column) {
        at += m_bytes[offset] == '\t' ? kTabWidth - at % kTabWidth : 1;
        ++offset;
    }
    return offset;
}

int SourceText::columnAt(qsizetype offset) const
{
    int column = 0;
    for (qsizetype i = lineStart(lineAt(offset)); i < offset; ++i)
        column += m_bytes[i] == '\t' ? kTabWidth - column % kTabWidth : 1;
    return column;
}

bool SourceText::continuesTrailing(size_t comment) const
{
    for (size_t i = comment; i > 0; --i) {
        const Span &below = m_comments[i];
        const Span &above = m_comments[i - 1];
        if (!startsItsLine(below.begin) || !isBlank(above.end, below.begin)
            || newlines(above.end, below.begin) != 1
            || columnAt(above.begin) != columnAt(below.begin)) {
            return false;
        }
        if (!startsItsLine(above.begin))
            return true;
    }
    return false;
}

int SourceText::lineAt(qsizetype offset) const
{
    const auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    return int(it - m_lineStarts.begin()) - 1;
}

qsizetype SourceText::lineStart(int line) const
{
    if (line < 0)
        return 0;
    if (size_t(line) >= m_lineStarts.size())
        return m_bytes.size();
    return m_lineStarts[size_t(line)];
}

qsizetype SourceText::lineEnd(qsizetype offset) const
{
    const qsizetype newline = m_bytes.indexOf('\n', offset);
    return newline < 0 ? m_bytes.size() : newline;
}

bool SourceText::isBlank(qsizetype begin, qsizetype end) const
{
    if (begin == 0 && m_bytes.startsWith("\xEF\xBB\xBF"))
        begin = std::min(end, kBomSize);
    for (qsizetype i = begin; i < end; ++i) {
        if (!isSpace(m_bytes[i]))
            return false;
    }
    return true;
}

int SourceText::newlines(qsizetype begin, qsizetype end) const
{
    return int(std::count(m_bytes.begin() + begin, m_bytes.begin() + end, '\n'));
}

bool SourceText::startsItsLine(qsizetype offset) const
{
    return isBlank(lineStart(lineAt(offset)), offset);
}

qsizetype SourceText::withLeadingComments(qsizetype begin) const
{
    auto it = std::upper_bound(m_comments.begin(), m_comments.end(), begin,
                               [](qsizetype at, const Span &c) { return at < c.end; });
    while (it != m_comments.begin()) {
        const Span comment = *--it;
        if (!isBlank(comment.end, begin) || newlines(comment.end, begin) > 1
            || !startsItsLine(comment.begin)
            || continuesTrailing(size_t(it - m_comments.begin()))) {
            break;
        }
        begin = comment.begin;
    }
    return begin;
}

qsizetype SourceText::withTrailingComments(Span span) const
{
    auto it = std::lower_bound(m_comments.begin(), m_comments.end(), span.end,
                               [](const Span &c, qsizetype at) { return c.begin < at; });
    if (it == m_comments.end() || !isBlank(span.end, it->begin))
        return span.end;

    const int gap = newlines(span.end, it->begin);
    if (gap == 0) {
        // On the same line. A block comment with more code after it may belong to that.
        if (!isBlank(it->end, lineEnd(it->end)))
            return span.end;
        qsizetype end = it->end;
        for (++it; it != m_comments.end()
                   && continuesTrailing(size_t(it - m_comments.begin())); ++it) {
            end = it->end;
        }
        return end;
    }

    // Below. Protobuf gives these to a statement when nothing else can claim them, that
    // is when a blank line or the end of the block comes next.
    if (gap > 1 || span.isEmpty() || m_bytes[span.end - 1] != ';')
        return span.end;
    qsizetype end = it->end;
    for (++it; it != m_comments.end(); ++it) {
        if (!isBlank(end, it->begin) || newlines(end, it->begin) > 1)
            break;
        end = it->end;
    }
    qsizetype next = end;
    while (next < m_bytes.size() && isSpace(m_bytes[next]))
        ++next;
    const bool unclaimed = next == m_bytes.size() || m_bytes[next] == '}'
        || newlines(end, next) > 1;
    return unclaimed ? end : span.end;
}

Snippet SourceText::snippet(Span span) const
{
    span.begin = std::clamp<qsizetype>(span.begin, 0, m_bytes.size());
    span.end = std::clamp<qsizetype>(span.end, span.begin, m_bytes.size());
    // A declaration that failed to parse can have no extent. Its line is better than
    // nothing.
    if (span.isEmpty())
        span.end = lineEnd(span.begin);

    qsizetype begin = withLeadingComments(span.begin);
    const qsizetype end = withTrailingComments(span);
    if (startsItsLine(begin)) {
        begin = lineStart(lineAt(begin));
        if (begin == 0 && m_bytes.startsWith("\xEF\xBB\xBF"))
            begin = kBomSize;
    }

    Snippet result;
    result.firstLine = lineAt(begin) + 1;
    result.lastLine = lineAt(std::max(begin, end - 1)) + 1;

    QStringList lines = QString::fromUtf8(m_bytes.sliced(begin, end - begin)).split(u'\n');
    QString indent;
    bool first = true;
    for (QString &line : lines) {
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (QStringView(line).trimmed().isEmpty())
            continue;
        indent = first ? indentOf(line).toString()
                       : commonPrefix(indent, indentOf(line)).toString();
        first = false;
    }
    for (QString &line : lines)
        line = line.startsWith(indent) ? line.sliced(indent.size()) : QString();
    result.text = lines.join(u'\n');
    return result;
}

} // namespace protoexplorer
