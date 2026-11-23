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

#include "SchemaTree.h"

#include <QAbstractItemModel>

#include <memory>

namespace protoexplorer {

// A SchemaTree as a Qt model.
//
// The schema never changes under it, so a branch that is made as it is looked into needs
// no announcing: to a view it was there all along.
class SchemaModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column { NameColumn, TypeColumn, NumberColumn, ColumnCount };

    explicit SchemaModel(std::shared_ptr<const Schema> schema, QObject *parent = nullptr);

    SchemaTree &tree() const { return *m_tree; }

    SchemaTree::Node *nodeAt(const QModelIndex &index) const;
    QModelIndex indexFor(const SchemaTree::Node *node, int column = NameColumn) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    const std::vector<std::unique_ptr<SchemaTree::Node>> &childrenOf(const QModelIndex &parent) const;

    std::unique_ptr<SchemaTree> m_tree;
};

} // namespace protoexplorer
