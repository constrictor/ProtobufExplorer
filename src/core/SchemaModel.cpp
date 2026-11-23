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

#include "SchemaModel.h"

namespace protoexplorer {

SchemaModel::SchemaModel(std::shared_ptr<const Schema> schema, QObject *parent)
    : QAbstractItemModel(parent)
    , m_tree(std::make_unique<SchemaTree>(std::move(schema)))
{
}

SchemaTree::Node *SchemaModel::nodeAt(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<SchemaTree::Node *>(index.internalPointer()) : nullptr;
}

QModelIndex SchemaModel::indexFor(const SchemaTree::Node *node, int column) const
{
    return node ? createIndex(node->row, column, node) : QModelIndex();
}

const std::vector<std::unique_ptr<SchemaTree::Node>> &
SchemaModel::childrenOf(const QModelIndex &parent) const
{
    SchemaTree::Node *node = nodeAt(parent);
    return node ? m_tree->children(*node) : m_tree->roots();
}

QModelIndex SchemaModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    return createIndex(row, column, childrenOf(parent)[size_t(row)].get());
}

QModelIndex SchemaModel::parent(const QModelIndex &index) const
{
    const SchemaTree::Node *node = nodeAt(index);
    return node ? indexFor(node->parent) : QModelIndex();
}

int SchemaModel::rowCount(const QModelIndex &parent) const
{
    return parent.column() > 0 ? 0 : int(childrenOf(parent).size());
}

int SchemaModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

bool SchemaModel::hasChildren(const QModelIndex &parent) const
{
    return rowCount(parent) > 0;
}

QVariant SchemaModel::data(const QModelIndex &index, int role) const
{
    const SchemaTree::Node *node = nodeAt(index);
    if (!node)
        return {};
    const Decl &decl = *node->decl;

    if (role == Qt::ToolTipRole) {
        return QStringLiteral("%1\n%2:%3").arg(decl.fullName, decl.file->displayPath)
            .arg(decl.file->text.lineAt(decl.span.begin) + 1);
    }
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case NameColumn:
        return node->label;
    case TypeColumn:
        return node->recursive ? decl.signature + QStringLiteral(" ↻") : decl.signature;
    case NumberColumn:
        return decl.hasNumber ? QVariant(QString::number(decl.number)) : QVariant();
    default:
        return {};
    }
}

QVariant SchemaModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case NameColumn:   return tr("Name");
    case TypeColumn:   return tr("Type");
    case NumberColumn: return tr("#");
    default:           return {};
    }
}

} // namespace protoexplorer
