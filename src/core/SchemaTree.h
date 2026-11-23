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

#include <QString>

#include <memory>
#include <vector>

namespace protoexplorer {

// The trees a schema is shown as: one for each root, where a field that holds a message
// or an enum has that type's members for its children.
//
// Branches are made when first looked into, since a type is repeated wherever it is used
// and all of them together can be past counting. Every branch does end: a type already on
// the way down to a field is not opened up again.
class SchemaTree
{
public:
    struct Node
    {
        const Decl *decl = nullptr;
        Node *parent = nullptr;
        int row = 0;
        QString label;
        bool recursive = false; // its type is already above it, so it has no children

        bool isRoot() const { return !parent; }

    private:
        friend class SchemaTree;
        std::vector<std::unique_ptr<Node>> children;
        bool loaded = false;
    };

    explicit SchemaTree(std::shared_ptr<const Schema> schema);

    const Schema &schema() const { return *m_schema; }

    const std::vector<std::unique_ptr<Node>> &roots() const { return m_roots; }
    const std::vector<std::unique_ptr<Node>> &children(Node &node);

    // The first place each message turns up, from the top of the tree to the bottom.
    const std::vector<Node *> &messages();

    // The rows that lead to a node from the top. Compares in the order of the tree.
    static std::vector<int> pathTo(const Node &node);

    // What to show for a declaration: itself, then the one its type refers to.
    static std::vector<const Decl *> sourceParts(const Decl &decl);

    // The message or enum a node stands for: itself, or the type of a field.
    static const Decl *typeOf(const Decl &decl);
    // The message a node stands for or is in, or null.
    static const Decl *messageAt(const Node &node);

private:
    static const std::vector<std::unique_ptr<Decl>> &membersOf(const Node &node);

    std::shared_ptr<const Schema> m_schema;
    std::vector<std::unique_ptr<Node>> m_roots;
    std::vector<Node *> m_messages;
    bool m_searched = false;
};

} // namespace protoexplorer
