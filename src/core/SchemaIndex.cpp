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

#include "SchemaIndex.h"

namespace protoexplorer {

SchemaIndex::SchemaIndex(std::shared_ptr<const Schema> schema)
    : m_schema(std::move(schema))
{
    for (const auto &file : m_schema->files) {
        for (const auto &decl : file->decls)
            add(*decl);
    }
}

void SchemaIndex::add(const Decl &decl)
{
    if (decl.kind == DeclKind::Message) {
        m_messages.push_back(&decl);
        if (!m_byName.contains(decl.fullName))
            m_byName.insert(decl.fullName, &decl);
    } else if (decl.kind == DeclKind::Extend) {
        for (const auto &field : decl.members) {
            if (!m_extensions.contains(field->fullName))
                m_extensions.insert(field->fullName, field.get());
        }
    }
    for (const auto &inner : decl.nested)
        add(*inner);
}

const Decl *SchemaIndex::message(const QString &fullName) const
{
    return m_byName.value(fullName);
}

std::vector<const Decl *> SchemaIndex::messagesNamed(const QString &name) const
{
    const QString full = name.startsWith(u'.') ? name.sliced(1) : name;
    if (const Decl *found = message(full))
        return {found};
    std::vector<const Decl *> result;
    const QString ending = u'.' + full;
    for (const Decl *message : m_messages) {
        // Of those that share a full name, the first.
        if (message->fullName.endsWith(ending) && m_byName.value(message->fullName) == message)
            result.push_back(message);
    }
    return result;
}

const Decl *SchemaIndex::field(const Decl &message, const QString &key) const
{
    if (key.startsWith(u'[') && key.endsWith(u']')) {
        const Decl *extension = m_extensions.value(key.sliced(1, key.size() - 2));
        if (!extension)
            return nullptr;
        const Decl *extended = extension->parent->type;
        // Two copies of a file make two of every message in it.
        return extended && extended->fullName == message.fullName ? extension : nullptr;
    }

    auto it = m_fields.constFind(&message);
    if (it == m_fields.cend()) {
        QHash<QString, const Decl *> fields;
        const auto each = [&message](auto &&visit) {
            for (const auto &member : message.members) {
                if (member->kind == DeclKind::Field)
                    visit(*member);
                for (const auto &inner : member->members)
                    visit(*inner);
            }
        };
        // One field's name can be what JSON calls another, which then has it.
        each([&fields](const Decl &field) {
            if (!fields.contains(field.field->jsonName))
                fields.insert(field.field->jsonName, &field);
        });
        each([&fields](const Decl &field) {
            if (!fields.contains(field.name))
                fields.insert(field.name, &field);
        });
        it = m_fields.insert(&message, std::move(fields));
    }
    return it->value(key);
}

} // namespace protoexplorer
