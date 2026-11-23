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

#include "TypeResolver.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace protoexplorer {

namespace {

class Resolver
{
public:
    explicit Resolver(Schema &schema)
        : m_schema(schema)
    {
        for (const auto &file : schema.files) {
            for (const auto &decl : file->decls)
                index(*decl);
        }
    }

    void resolveAll()
    {
        for (const auto &file : m_schema.files) {
            m_visible = visibleFrom(*file);
            for (const auto &decl : file->decls)
                resolve(*decl);
        }
    }

private:
    void index(const Decl &decl)
    {
        if (decl.isType())
            m_types[decl.fullName].push_back(&decl);
        for (const auto &inner : decl.nested)
            index(*inner);
    }

    // The file itself, what it imports, and what those import publicly, nearest first.
    static std::vector<const SourceFile *> visibleFrom(const SourceFile &file)
    {
        std::vector<const SourceFile *> result{&file};
        QSet<const SourceFile *> seen{&file};
        for (const Import &import : file.imports) {
            if (import.file && !seen.contains(import.file)) {
                seen.insert(import.file);
                result.push_back(import.file);
            }
        }
        for (size_t i = 1; i < result.size(); ++i) {
            for (const Import &import : result[i]->imports) {
                if (import.isPublic && import.file && !seen.contains(import.file)) {
                    seen.insert(import.file);
                    result.push_back(import.file);
                }
            }
        }
        return result;
    }

    void resolve(Decl &decl)
    {
        if (!decl.typeName.isEmpty())
            decl.type = lookUp(decl);
        for (const auto &member : decl.members)
            resolve(*member);
        for (const auto &inner : decl.nested)
            resolve(*inner);
    }

    static QString scopeOf(const Decl &decl)
    {
        for (const Decl *p = decl.parent; p; p = p->parent) {
            if (p->kind == DeclKind::Message)
                return p->fullName;
        }
        return decl.file->package;
    }

    // Every full name the reference could stand for, innermost scope first.
    static QStringList candidates(const Decl &decl)
    {
        if (decl.typeName.startsWith(u'.'))
            return {decl.typeName.sliced(1)};
        QStringList result;
        QString scope = scopeOf(decl);
        while (!scope.isEmpty()) {
            result << scope + u'.' + decl.typeName;
            scope.truncate(std::max<qsizetype>(0, scope.lastIndexOf(u'.')));
        }
        result << decl.typeName;
        return result;
    }

    const Decl *lookUp(const Decl &decl) const
    {
        const QStringList names = candidates(decl);

        // What the file can properly see goes first, whatever the scope. Were the scope to
        // go first, `common.Money` in package acme.v1 would mean a stray acme.common.Money
        // that the file never imported over the common.Money that it did.
        for (const QString &name : names) {
            const auto it = m_types.constFind(name);
            if (it == m_types.cend())
                continue;
            for (const SourceFile *file : m_visible) {
                for (const Decl *type : *it) {
                    if (type->file == file)
                        return type;
                }
            }
        }

        // An import is missing or was not written: a likely guess beats a dead end.
        for (const QString &name : names) {
            const auto it = m_types.constFind(name);
            if (it != m_types.cend())
                return nearest(*decl.file, *it);
        }
        return nullptr;
    }

    static qsizetype sharedPath(QStringView a, QStringView b)
    {
        qsizetype shared = 0;
        for (qsizetype i = 0; i < std::min(a.size(), b.size()) && a[i] == b[i]; ++i) {
            if (a[i] == u'/')
                shared = i;
        }
        return shared;
    }

    static const Decl *nearest(const SourceFile &from, const std::vector<const Decl *> &types)
    {
        return *std::min_element(types.begin(), types.end(), [&](const Decl *a, const Decl *b) {
            const auto sa = sharedPath(from.path, a->file->path);
            const auto sb = sharedPath(from.path, b->file->path);
            return sa != sb ? sa > sb : a->file->path < b->file->path;
        });
    }

    Schema &m_schema;
    QHash<QString, std::vector<const Decl *>> m_types;
    std::vector<const SourceFile *> m_visible;
};

} // namespace

void resolveTypes(Schema &schema)
{
    Resolver(schema).resolveAll();
}

} // namespace protoexplorer
