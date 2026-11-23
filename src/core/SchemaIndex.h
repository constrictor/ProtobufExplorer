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

#include <QHash>

#include <memory>
#include <vector>

namespace protoexplorer {

// A schema by name.
class SchemaIndex
{
public:
    explicit SchemaIndex(std::shared_ptr<const Schema> schema);

    const Schema &schema() const { return *m_schema; }

    // Every message there is, those that were opened before those that were imported.
    const std::vector<const Decl *> &messages() const { return m_messages; }

    // By full name.
    const Decl *message(const QString &fullName) const;
    // By full name, or if there is none of it, by a part that a full name ends with.
    std::vector<const Decl *> messagesNamed(const QString &name) const;

    // The field a JSON object for this message means by a key, be that the name JSON has
    // for it or its own. Extensions go by "[full.name]".
    const Decl *field(const Decl &message, const QString &key) const;

private:
    void add(const Decl &decl);

    std::shared_ptr<const Schema> m_schema;
    std::vector<const Decl *> m_messages;
    QHash<QString, const Decl *> m_byName; // the first of that full name
    QHash<QString, const Decl *> m_extensions;
    mutable QHash<const Decl *, QHash<QString, const Decl *>> m_fields;
};

} // namespace protoexplorer
