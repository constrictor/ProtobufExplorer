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

#include "JsonModel.h"

namespace protoexplorer {

JsonModel::JsonModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void JsonModel::setTree(std::shared_ptr<const JsonTree> tree)
{
    beginResetModel();
    m_tree = std::move(tree);
    endResetModel();
}

const JsonTree::Node *JsonModel::nodeAt(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const JsonTree::Node *>(index.constInternalPointer()) : nullptr;
}

QModelIndex JsonModel::indexFor(const JsonTree::Node *node, int column) const
{
    return node ? createIndex(node->row, column, node) : QModelIndex();
}

QModelIndex JsonModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    const JsonTree::Node *node = nodeAt(parent);
    const auto &nodes = node ? node->children : m_tree->roots();
    return indexFor(nodes[size_t(row)].get(), column);
}

QModelIndex JsonModel::parent(const QModelIndex &index) const
{
    const JsonTree::Node *node = nodeAt(index);
    return node ? indexFor(node->parent) : QModelIndex();
}

int JsonModel::rowCount(const QModelIndex &parent) const
{
    if (!m_tree || parent.column() > 0)
        return 0;
    const JsonTree::Node *node = nodeAt(parent);
    return int((node ? node->children : m_tree->roots()).size());
}

int JsonModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant JsonModel::data(const QModelIndex &index, int role) const
{
    const JsonTree::Node *node = nodeAt(index);
    if (!node)
        return {};
    if (role == StateRole)
        return QVariant::fromValue(int(node->state));
    if (role == Qt::ToolTipRole)
        return toolTip(*node);
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case NameColumn:
        return node->state == JsonTree::State::Problem ? QStringLiteral("✕ ") + node->name : node->name;
    case ValueColumn:
        return m_tree->valueText(*node);
    case TypeColumn:
        return m_tree->typeText(*node);
    case NumberColumn:
        return node->isField() && node->decl->hasNumber ? QVariant(QString::number(node->decl->number))
                                                        : QVariant();
    default:
        return {};
    }
}

QString JsonModel::toolTip(const JsonTree::Node &node) const
{
    QStringList lines{m_tree->pathTo(node)};
    if (!node.problem.isEmpty())
        lines << node.problem;
    if (node.decl) {
        lines << node.decl->fullName
              << QStringLiteral("%1:%2").arg(node.decl->file->displayPath)
                     .arg(node.decl->file->text.lineAt(node.decl->span.begin) + 1);
    }
    return lines.join(u'\n');
}

QVariant JsonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case NameColumn:   return tr("Name");
    case ValueColumn:  return tr("Value");
    case TypeColumn:   return tr("Type");
    case NumberColumn: return tr("#");
    default:           return {};
    }
}

} // namespace protoexplorer
