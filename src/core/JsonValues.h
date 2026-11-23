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
#include "Schema.h"

namespace protoexplorer {

// The types of google.protobuf that JSON has a form of its own for.
enum class Special : quint8 {
    None,
    Any,
    Value,     // any JSON at all
    Struct,    // any object
    ListValue, // any array
    NullValue,
    Timestamp,
    Duration,
    FieldMask,
    Wrapper, // Int32Value and the like: the bare value
};

Special specialOf(const Decl &type);
// The scalar a Wrapper wraps.
QString wrappedScalar(const Decl &wrapper);

// What protobuf makes of a JSON value.
struct Reading
{
    QString text;    // the value, to show
    QString problem; // why it is none, if it is not
};

Reading readScalar(QStringView scalar, const JsonValue &json);
Reading readEnum(const Decl &type, const JsonValue &json);
Reading readSpecial(Special special, const JsonValue &json); // the ones that are strings
// Why a key cannot be one of a map with such keys; nothing if it can.
QString mapKeyProblem(QStringView scalar, const QString &key);

// What a field is when it is left out, to show.
QString defaultText(const Decl &field);
// The same as JSON writes it, for a field that does not track presence: such a one cannot
// have a default that is written.
QString defaultJson(const Decl &field);

// On one line, and not too long for it.
QString elided(QStringView text);

} // namespace protoexplorer
