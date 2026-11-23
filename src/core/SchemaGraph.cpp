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

#include "SchemaGraph.h"

#include <QSet>

#include <algorithm>

namespace protoexplorer {

namespace {

void collect(const Decl &decl, std::vector<const Decl *> &out)
{
    out.push_back(&decl);
    for (const auto &inner : decl.nested)
        collect(*inner, out);
}

// Depth-first, on a stack of its own: chains of messages can be long.
void walk(const Decl *start, QSet<const Decl *> &visited, std::vector<const Decl *> *finished)
{
    struct Frame
    {
        const Decl *decl;
        std::vector<const Decl *> used;
        size_t next = 0;
    };
    if (visited.contains(start))
        return;
    visited.insert(start);
    std::vector<Frame> stack;
    stack.push_back({start, usedTypes(*start)});
    while (!stack.empty()) {
        Frame &top = stack.back();
        if (top.next == top.used.size()) {
            if (finished)
                finished->push_back(top.decl);
            stack.pop_back();
            continue;
        }
        const Decl *type = top.used[top.next++];
        if (visited.contains(type))
            continue;
        visited.insert(type);
        stack.push_back({type, usedTypes(*type)});
    }
}

} // namespace

std::vector<const Decl *> usedTypes(const Decl &decl)
{
    std::vector<const Decl *> result;
    if (decl.kind != DeclKind::Message && decl.kind != DeclKind::Extend
        && decl.kind != DeclKind::Oneof) {
        return result;
    }
    for (const auto &member : decl.members) {
        if (member->kind == DeclKind::Oneof) {
            const auto inner = usedTypes(*member);
            result.insert(result.end(), inner.begin(), inner.end());
        } else if (member->type) {
            result.push_back(member->type);
        }
    }
    return result;
}

std::vector<const Decl *> findRoots(const Schema &schema)
{
    std::vector<const Decl *> all;
    for (const auto &file : schema.files) {
        if (!file->opened)
            continue;
        for (const auto &decl : file->decls)
            collect(*decl, all);
    }

    // Two passes, as for strongly connected components. A walk started from the type that
    // finished last covers everything it leads to; the next one not yet covered is then
    // led to by nothing outside its own circle, and so on down.
    std::vector<const Decl *> finished;
    QSet<const Decl *> visited;
    for (const Decl *decl : all) {
        if (decl->isType())
            walk(decl, visited, &finished);
    }

    QSet<const Decl *> heads;
    visited.clear();
    for (const Decl *decl : all) {
        if (decl->kind == DeclKind::Service) {
            heads.insert(decl);
        } else if (decl->kind == DeclKind::Extend) {
            heads.insert(decl);
            walk(decl, visited, nullptr);
        }
    }
    for (auto it = finished.rbegin(); it != finished.rend(); ++it) {
        if (visited.contains(*it))
            continue;
        heads.insert(*it);
        walk(*it, visited, nullptr);
    }

    std::vector<const Decl *> roots;
    std::copy_if(all.begin(), all.end(), std::back_inserter(roots),
                 [&](const Decl *decl) { return heads.contains(decl); });
    return roots;
}

} // namespace protoexplorer
