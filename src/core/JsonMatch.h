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

#include "Json.h"
#include "SchemaIndex.h"

#include <vector>

namespace protoexplorer {

// A key of the message or messages a JSON text holds, and what the first of them that has
// it has for it.
struct JsonKey
{
    QString name;
    JsonValue::Kind kind = JsonValue::Kind::Null;
};

// Each of them once.
std::vector<JsonKey> topLevelKeys(const JsonDocument &document);

// How many of them are for fields of the message.
int knownKeys(const SchemaIndex &index, const Decl &message, const std::vector<JsonKey> &keys);

struct MessageMatch
{
    const Decl *message = nullptr;
    int known = 0;
};

// Every message there is, the likeliest first to be what the keys are from.
std::vector<MessageMatch> rankMessages(const SchemaIndex &index, const std::vector<JsonKey> &keys);

} // namespace protoexplorer
