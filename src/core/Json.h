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

#include <QString>

#include <optional>
#include <vector>

namespace protoexplorer {

// One value in a JSON text, and where in it that is, in UTF-16 units.
struct JsonValue
{
    enum class Kind : quint8 { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    QString key;  // what the object it is a member of calls it
    QString text; // a string, decoded; a number, as it is written; true or false
    qsizetype keyBegin = 0; // where its key begins, or where it does if it has none
    qsizetype begin = 0;
    qsizetype end = 0;
    std::vector<JsonValue> members; // of an array or an object, in the order written

    const JsonValue *find(QStringView name) const;
};

struct JsonError
{
    qsizetype at = 0;
    int line = 1;
    QString text;
};

// A JSON text: one value, or several one after another as in JSON Lines.
//
// Unlike QJsonDocument it keeps the order of things, keys that come twice, numbers to the
// last digit, and where everything is.
struct JsonDocument
{
    QString text;
    std::vector<JsonValue> values;
    std::optional<JsonError> error; // where reading stopped; no values then

    QString slice(const JsonValue &value) const
    {
        return text.sliced(value.begin, value.end - value.begin);
    }
};

JsonDocument parseJson(const QString &text);

// As JSON writes it, quotes and all.
QString quoteJson(QStringView text);

} // namespace protoexplorer
