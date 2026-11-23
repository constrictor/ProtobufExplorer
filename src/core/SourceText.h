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

#include <QByteArray>
#include <QString>

#include <vector>

namespace protoexplorer {

// A half-open range of byte offsets into a file.
struct Span
{
    qsizetype begin = 0;
    qsizetype end = 0;

    bool isEmpty() const { return end <= begin; }
    friend bool operator==(const Span &, const Span &) = default;
};

// A piece of source as it is written, ready to show.
struct Snippet
{
    QString text;
    int firstLine = 0; // 1-based, both ends inclusive
    int lastLine = 0;
};

// The raw bytes of a schema file, and where things are in them.
class SourceText
{
public:
    SourceText() : SourceText(QByteArray()) {}
    explicit SourceText(QByteArray bytes);

    const QByteArray &bytes() const { return m_bytes; }

    // From the coordinates protobuf reports: zero-based, columns counted in bytes except
    // that a tab advances to the next multiple of 8.
    qsizetype offsetAt(int line, int column) const;
    // Zero-based.
    int lineAt(qsizetype offset) const;

    // Every comment in the file, in order. A line comment's range excludes its newline.
    const std::vector<Span> &comments() const { return m_comments; }

    // The declaration at `span` together with the comments attached to it, with the
    // indentation its lines have in common removed.
    Snippet snippet(Span span) const;

private:
    void indexLines();
    void indexComments();

    qsizetype lineStart(int line) const;
    qsizetype lineEnd(qsizetype offset) const;
    // The range holds nothing but whitespace.
    bool isBlank(qsizetype begin, qsizetype end) const;
    int newlines(qsizetype begin, qsizetype end) const;
    bool startsItsLine(qsizetype offset) const;
    int columnAt(qsizetype offset) const;
    // The comment carries on the one that trails the code on the line above, lined up
    // beneath it. Protobuf itself would give it to the declaration below.
    bool continuesTrailing(size_t comment) const;
    qsizetype withLeadingComments(qsizetype begin) const;
    qsizetype withTrailingComments(Span span) const;

    QByteArray m_bytes;
    std::vector<qsizetype> m_lineStarts;
    std::vector<Span> m_comments;
};

} // namespace protoexplorer
