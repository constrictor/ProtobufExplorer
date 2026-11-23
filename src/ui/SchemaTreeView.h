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

#include <QTreeView>

namespace protoexplorer {

class SchemaTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit SchemaTreeView(QWidget *parent = nullptr);

    // Bring an item into view, opening whatever it is inside, and make it the current one.
    void reveal(const QModelIndex &index);

    // Take files that have one of these endings when they are dropped on a row.
    void acceptFiles(const QStringList &endings);

signals:
    void filesDropped(const QStringList &paths, const QModelIndex &on);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QStringList filesIn(const QMimeData &data) const;

    QStringList m_endings;
};

} // namespace protoexplorer
