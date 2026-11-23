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

#include "SchemaTreeView.h"

#include <QHeaderView>
#include <QKeyEvent>

namespace protoexplorer {

SchemaTreeView::SchemaTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    // Deep rows are wide, so let them scroll rather than be squeezed.
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    header()->setSectionsMovable(false);
}

void SchemaTreeView::reveal(const QModelIndex &index)
{
    setCurrentIndex(index);
    scrollTo(index, QAbstractItemView::PositionAtCenter); // opens what it is inside
}

void SchemaTreeView::keyPressEvent(QKeyEvent *event)
{
    // Opens everything underneath, and there can be more of that than would ever finish.
    if (event->key() == Qt::Key_Asterisk) {
        event->accept();
        return;
    }
    QTreeView::keyPressEvent(event);
}

} // namespace protoexplorer
