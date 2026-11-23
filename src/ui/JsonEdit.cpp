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

#include "JsonEdit.h"

#include "JsonHighlighter.h"

#include <QFontDatabase>
#include <QMimeData>

#include <algorithm>

namespace protoexplorer {

JsonEdit::JsonEdit(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new JsonHighlighter(document()))
{
    setPlaceholderText(tr("Paste or type JSON here."));
    // It often comes all on one line.
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(font);
    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(u' '));
    applyPalette();
}

QString JsonEdit::json() const
{
    // Not toPlainText(), which makes plain spaces of those that do not break.
    QString text = document()->toRawText();
    text.replace(QChar(QChar::ParagraphSeparator), u'\n');
    return text;
}

void JsonEdit::mark(int begin, int end)
{
    m_markBegin = begin;
    m_markEnd = end;
    updateMarks();
}

void JsonEdit::goTo(int position)
{
    QTextCursor cursor(document());
    cursor.setPosition(std::clamp(position, 0, document()->characterCount() - 1));
    setTextCursor(cursor);
    ensureCursorVisible();
}

void JsonEdit::setErrorAt(std::optional<int> position)
{
    m_errorAt = position;
    updateMarks();
}

void JsonEdit::updateMarks()
{
    const int last = document()->characterCount() - 1;
    const auto select = [&](int begin, int end) {
        QTextEdit::ExtraSelection selection;
        selection.cursor = QTextCursor(document());
        selection.cursor.setPosition(std::clamp(begin, 0, last));
        selection.cursor.setPosition(std::clamp(end, 0, last), QTextCursor::KeepAnchor);
        return selection;
    };
    QList<QTextEdit::ExtraSelection> selections;
    if (m_markEnd > m_markBegin) {
        selections << select(m_markBegin, m_markEnd);
        selections.last().format.setBackground(markColor(palette()));
    }
    if (m_errorAt) {
        // At the end there is nothing to underline but what is before it.
        const int at = std::clamp(*m_errorAt, 0, std::max(0, last - 1));
        selections << select(at, at + 1);
        selections.last().format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        selections.last().format.setUnderlineColor(errorColor(palette()));
        selections.last().format.setBackground(markColor(palette()));
    }
    setExtraSelections(selections);
}

bool JsonEdit::canInsertFromMimeData(const QMimeData *source) const
{
    // A file dropped here would leave its address in the JSON.
    return !source->hasUrls() && QPlainTextEdit::canInsertFromMimeData(source);
}

void JsonEdit::applyPalette()
{
    m_highlighter->setPalette(palette());
    updateMarks();
}

void JsonEdit::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        applyPalette();
    QPlainTextEdit::changeEvent(event);
}

} // namespace protoexplorer
