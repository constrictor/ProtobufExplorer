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

#include "Schema.h"

#include <QWidget>

#include <vector>

QT_BEGIN_NAMESPACE
class QLabel;
class QPlainTextEdit;
QT_END_NAMESPACE

namespace protoexplorer {

class ProtoHighlighter;

// Declarations as they are written, one above the other, each under where it is from.
class SourceView : public QWidget
{
    Q_OBJECT

public:
    explicit SourceView(QWidget *parent = nullptr);

    // One or two.
    void display(const std::vector<const Decl *> &parts);
    void clear();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Pane
    {
        QWidget *box = nullptr;
        QLabel *header = nullptr;
        QString where; // the header in full
        QPlainTextEdit *text = nullptr;
        ProtoHighlighter *highlighter = nullptr;
    };

    Pane makePane(int number);
    void fill(Pane &pane, const Decl &decl);
    // Too long to show whole, a path keeps its end: the file and the lines.
    static void elideHeader(const Pane &pane);
    void applyFont();
    void applyPalette();
    // Make the first pane just tall enough, when there is a second to leave room for.
    void fitFirst(bool alone);

    Pane m_first;
    Pane m_second;
};

} // namespace protoexplorer
