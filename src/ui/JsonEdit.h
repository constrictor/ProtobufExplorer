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

#include <QPlainTextEdit>

#include <optional>

namespace protoexplorer {

class JsonHighlighter;

// Where the reader sees the JSON, and can write it.
class JsonEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit JsonEdit(QWidget *parent = nullptr);

    // What is in it. Places in this are places in the document.
    QString json() const;

    // Point at a part of it, or at none with nothing in the part.
    void mark(int begin, int end);
    // Bring a place into view, with the cursor at it.
    void goTo(int position);
    // Where it stops making sense, if it does.
    void setErrorAt(std::optional<int> position);

protected:
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void changeEvent(QEvent *event) override;

private:
    void applyPalette();
    void updateMarks();

    JsonHighlighter *m_highlighter;
    int m_markBegin = 0;
    int m_markEnd = 0;
    std::optional<int> m_errorAt;
};

} // namespace protoexplorer
