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

#include "SchemaTree.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace protoexplorer {

namespace {

// Outer.Inner, without the package. An `extend` block says what it extends.
QString localName(const Decl &decl)
{
    const QString &package = decl.file->package;
    if (decl.kind != DeclKind::Extend && !package.isEmpty()
        && decl.fullName.startsWith(package + u'.')) {
        return decl.fullName.sliced(package.size() + 1);
    }
    return decl.kind == DeclKind::Extend ? decl.name : decl.fullName;
}

} // namespace

SchemaTree::SchemaTree(std::shared_ptr<const Schema> schema)
    : m_schema(std::move(schema))
{
    // The short name where it will do; the full one where it is shared; and where that is
    // shared as well, the file.
    QHash<QString, QSet<QString>> fullNames;
    QHash<QString, int> uses;
    const auto key = [](const Decl &decl) { return QString::number(int(decl.kind)) + localName(decl); };
    const auto fullKey = [](const Decl &decl) { return QString::number(int(decl.kind)) + decl.fullName; };
    for (const Decl *decl : m_schema->roots) {
        fullNames[key(*decl)].insert(decl->fullName);
        ++uses[fullKey(*decl)];
    }

    for (const Decl *decl : m_schema->roots) {
        auto node = std::make_unique<Node>();
        node->decl = decl;
        node->row = int(m_roots.size());
        node->label = fullNames[key(*decl)].size() > 1 ? decl->fullName : localName(*decl);
        if (decl->kind != DeclKind::Extend && uses[fullKey(*decl)] > 1) {
            node->label += QStringLiteral(" (%1:%2)").arg(decl->file->displayPath)
                               .arg(decl->file->text.lineAt(decl->span.begin) + 1);
        }
        m_roots.push_back(std::move(node));
    }
}

const Decl *SchemaTree::typeOf(const Decl &decl)
{
    if (decl.isType())
        return &decl;
    return decl.kind == DeclKind::Field ? decl.type : nullptr;
}

const std::vector<std::unique_ptr<Decl>> &SchemaTree::membersOf(const Node &node)
{
    static const std::vector<std::unique_ptr<Decl>> none;
    if (node.recursive)
        return none;
    if (node.decl->kind == DeclKind::Field)
        return node.decl->type ? node.decl->type->members : none;
    return node.decl->members;
}

const std::vector<std::unique_ptr<SchemaTree::Node>> &SchemaTree::children(Node &node)
{
    if (node.loaded)
        return node.children;
    node.loaded = true;
    for (const auto &member : membersOf(node)) {
        auto child = std::make_unique<Node>();
        child->decl = member.get();
        child->parent = &node;
        child->row = int(node.children.size());
        child->label = member->name;
        if (member->kind == DeclKind::Field && member->type
            && member->type->kind == DeclKind::Message) {
            for (const Node *above = &node; above && !child->recursive; above = above->parent)
                child->recursive = typeOf(*above->decl) == member->type;
        }
        node.children.push_back(std::move(child));
    }
    return node.children;
}

const std::vector<SchemaTree::Node *> &SchemaTree::messages()
{
    if (m_searched)
        return m_messages;
    m_searched = true;

    // Breadth first, opening up each type only where it is first met. That meets all of
    // them without going through every repetition, and what is met first is never a
    // recursive one: whatever is above a node has been met before it.
    QSet<const Decl *> met;
    std::vector<Node *> level;
    const auto visit = [&](Node *node) {
        const Decl *type = typeOf(*node->decl);
        if (type) {
            if (met.contains(type))
                return;
            met.insert(type);
            if (type->kind == DeclKind::Message)
                m_messages.push_back(node);
        }
        level.push_back(node);
    };
    for (const auto &root : m_roots)
        visit(root.get());
    for (size_t i = 0; i < level.size(); ++i) {
        const Decl *type = typeOf(*level[i]->decl);
        if (type ? type->kind != DeclKind::Message
                 : level[i]->decl->kind != DeclKind::Extend && level[i]->decl->kind != DeclKind::Oneof) {
            continue; // nothing under it holds a message
        }
        for (const auto &child : children(*level[i]))
            visit(child.get());
    }

    std::vector<std::pair<std::vector<int>, Node *>> ordered;
    ordered.reserve(m_messages.size());
    for (Node *node : m_messages)
        ordered.emplace_back(pathTo(*node), node);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    std::transform(ordered.begin(), ordered.end(), m_messages.begin(),
                   [](const auto &entry) { return entry.second; });
    return m_messages;
}

std::vector<int> SchemaTree::pathTo(const Node &node)
{
    std::vector<int> rows;
    for (const Node *at = &node; at; at = at->parent)
        rows.push_back(at->row);
    std::reverse(rows.begin(), rows.end());
    return rows;
}

std::vector<const Decl *> SchemaTree::sourceParts(const Node &node)
{
    std::vector<const Decl *> parts{node.decl};
    if (node.decl->kind == DeclKind::Field && node.decl->type) {
        const Decl *type = node.decl->type;
        // A group is a field and a type written as one.
        if (type->file != node.decl->file || type->span != node.decl->span)
            parts.push_back(type);
    }
    return parts;
}

} // namespace protoexplorer
