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

#include "JsonMatch.h"

#include "JsonValues.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace protoexplorer {

namespace {

// Plenty to tell what they all are, however many there are.
constexpr size_t kMaxMessages = 100;

using Kind = JsonValue::Kind;

// It could be a value of this field, going by no more than what sort of JSON it is.
bool couldHold(const Decl &field, Kind kind)
{
    const FieldInfo &info = *field.field;
    if (kind == Kind::Null)
        return true;
    if (info.repeated)
        return kind == Kind::Array;
    if (!info.mapKey.isEmpty())
        return kind == Kind::Object;
    if (field.type) {
        if (field.type->kind == DeclKind::Enum)
            return kind == Kind::String || kind == Kind::Number;
        return specialOf(*field.type) != Special::None || kind == Kind::Object;
    }
    if (info.scalar.isEmpty())
        return true;
    if (info.scalar == u"bool")
        return kind == Kind::Bool;
    if (info.scalar == u"string" || info.scalar == u"bytes")
        return kind == Kind::String;
    return kind == Kind::Number || kind == Kind::String;
}

template<typename Visit> void eachField(const Decl &message, Visit visit)
{
    for (const auto &member : message.members) {
        if (member->kind == DeclKind::Field)
            visit(*member);
        for (const auto &inner : member->members)
            visit(*inner);
    }
}

} // namespace

std::vector<JsonKey> topLevelKeys(const JsonDocument &document)
{
    const bool list = document.values.size() == 1 && document.values.front().kind == Kind::Array;
    const std::vector<JsonValue> &messages = list ? document.values.front().members : document.values;
    std::vector<JsonKey> keys;
    QSet<QString> seen;
    for (size_t i = 0; i < std::min(messages.size(), kMaxMessages); ++i) {
        if (messages[i].kind != Kind::Object)
            continue;
        for (const JsonValue &member : messages[i].members) {
            if (seen.contains(member.key))
                continue;
            seen.insert(member.key);
            keys.push_back({member.key, member.kind});
        }
    }
    return keys;
}

int knownKeys(const SchemaIndex &index, const Decl &message, const std::vector<JsonKey> &keys)
{
    return int(std::count_if(keys.begin(), keys.end(), [&](const JsonKey &key) {
        return index.field(message, key.name) != nullptr;
    }));
}

std::vector<MessageMatch> rankMessages(const SchemaIndex &index, const std::vector<JsonKey> &keys)
{
    struct Candidate
    {
        MessageMatch match;
        int misfits = 0;
        int unused = 0;
        bool special = false;
    };
    QHash<QString, Kind> kinds;
    for (const JsonKey &key : keys)
        kinds.insert(key.name, key.kind);

    std::vector<Candidate> candidates;
    candidates.reserve(index.messages().size());
    for (const Decl *message : index.messages()) {
        Candidate candidate;
        candidate.match.message = message;
        // Field by field rather than by SchemaIndex::field(), which would go and remember
        // the fields of every message there is.
        const auto count = [&](const Decl &field, const QString &name) {
            const auto it = kinds.constFind(name);
            if (it == kinds.cend())
                return;
            ++candidate.match.known;
            candidate.misfits += couldHold(field, *it) ? 0 : 1;
        };
        if (!keys.empty()) {
            eachField(*message, [&](const Decl &field) {
                count(field, field.field->jsonName);
                if (field.name != field.field->jsonName)
                    count(field, field.name);
                ++candidate.unused;
            });
        }
        candidate.special = specialOf(*message) != Special::None;
        candidates.push_back(candidate);
    }
    // Alike in all this, they stay in the order of the schema.
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.special != b.special)
            return b.special;
        if (a.match.known != b.match.known)
            return a.match.known > b.match.known;
        if (a.misfits != b.misfits)
            return a.misfits < b.misfits;
        return a.unused < b.unused;
    });

    std::vector<MessageMatch> ranked;
    ranked.reserve(candidates.size());
    for (const Candidate &candidate : candidates)
        ranked.push_back(candidate.match);
    return ranked;
}

} // namespace protoexplorer
