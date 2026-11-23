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

#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

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

void SchemaTreeView::acceptFiles(const QStringList &endings)
{
    m_endings = endings;
    setAcceptDrops(!endings.isEmpty());
}

QStringList SchemaTreeView::filesIn(const QMimeData &data) const
{
    QStringList paths;
    for (const QUrl &url : data.urls()) {
        const QString path = url.toLocalFile();
        if (std::any_of(m_endings.begin(), m_endings.end(), [&path](const QString &ending) {
                return path.endsWith(ending, Qt::CaseInsensitive);
            })) {
            paths << path;
        }
    }
    return paths;
}

// The base class has ways of its own for all this, which ask the model about every step.

void SchemaTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (filesIn(*event->mimeData()).isEmpty())
        event->ignore();
    else
        event->acceptProposedAction();
}

void SchemaTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (filesIn(*event->mimeData()).isEmpty()) {
        event->ignore();
        return;
    }
    // What it would be dropped on is there to see.
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid())
        setCurrentIndex(index);
    event->acceptProposedAction();
}

void SchemaTreeView::dropEvent(QDropEvent *event)
{
    const QStringList paths = filesIn(*event->mimeData());
    if (paths.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    emit filesDropped(paths, indexAt(event->position().toPoint()));
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
