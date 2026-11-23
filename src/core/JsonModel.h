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

#include "JsonTree.h"

#include <QAbstractItemModel>

#include <memory>

namespace protoexplorer {

// A JsonTree as a Qt model.
class JsonModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column { NameColumn, ValueColumn, TypeColumn, NumberColumn, ColumnCount };
    enum Role { StateRole = Qt::UserRole }; // a JsonTree::State

    explicit JsonModel(QObject *parent = nullptr);

    // Another in place of the one there is.
    void setTree(std::shared_ptr<const JsonTree> tree);
    const JsonTree *tree() const { return m_tree.get(); }

    const JsonTree::Node *nodeAt(const QModelIndex &index) const;
    QModelIndex indexFor(const JsonTree::Node *node, int column = NameColumn) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QString toolTip(const JsonTree::Node &node) const;

    std::shared_ptr<const JsonTree> m_tree;
};

} // namespace protoexplorer
