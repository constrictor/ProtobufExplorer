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

#include "SourceText.h"

#include <QString>

#include <memory>
#include <vector>

namespace protoexplorer {

struct SourceFile;

enum class DeclKind : quint8 {
    Message,
    Enum,
    Service,
    Extend, // one `extend X { ... }` block
    Field,
    Oneof,
    EnumValue,
    Method,
};

struct Decl;

// What a field's values are, as far as its type alone does not say.
struct FieldInfo
{
    QString scalar;   // the scalar type of its values, or of a map's; empty for another type
    QString mapKey;   // the type of a map's keys; empty for anything but a map
    QString jsonName; // what it goes by in JSON
    QByteArray defaultValue; // text, or for `bytes` the bytes themselves
    bool hasDefault = false; // one that is written
    bool repeated = false;   // a list; a map is not one
    bool required = false;
    bool tracksPresence = false; // leaving it out is not the same as giving the default
};

struct MethodInfo
{
    QString request; // as written
    QString response;
    const Decl *requestType = nullptr; // the messages they turned out to mean, if any
    const Decl *responseType = nullptr;
};

// One declaration in a schema file, as it is written there.
struct Decl
{
    DeclKind kind = DeclKind::Message;
    QString name;
    QString fullName;  // "package.Outer.Inner", no leading dot
    QString signature; // what a reader wants beside the name: "repeated Item", "message"
    QString typeName;  // the message or enum referred to, as written; empty for scalars
    int number = -1;   // field number or enum value
    bool hasNumber = false;
    bool closed = false; // an enum that can hold no value but those it names

    const SourceFile *file = nullptr;
    Span span;
    const Decl *parent = nullptr; // the declaration this one is written inside
    const Decl *type = nullptr;   // what typeName turned out to mean, if anything

    std::unique_ptr<FieldInfo> field;   // for a Field, and only for one
    std::unique_ptr<MethodInfo> method; // for a Method, and only for one

    std::vector<std::unique_ptr<Decl>> members; // fields, oneofs, values, methods
    std::vector<std::unique_ptr<Decl>> nested;  // types and extends declared inside

    bool isType() const { return kind == DeclKind::Message || kind == DeclKind::Enum; }
};

// Something the reader should know about what they opened.
struct Note
{
    QString path;
    int line = 0; // 1-based; 0 for the file as a whole
    QString text;
};

struct Import
{
    QString path; // as written
    bool isPublic = false;
    const SourceFile *file = nullptr;
};

struct SourceFile
{
    QString path;        // canonical, or a resource path for a bundled file
    QString displayPath; // what to show for it
    bool opened = false; // asked for by the reader, rather than pulled in by an import
    SourceText text;
    QString package;
    std::vector<Import> imports;
    std::vector<std::unique_ptr<Decl>> decls; // top level, in the order written
    std::vector<Note> notes;
};

// Everything found at one opened path. Immutable once loaded.
struct Schema
{
    QString path;
    std::vector<std::unique_ptr<SourceFile>> files; // opened ones first, sorted by path
    std::vector<const Decl *> roots;
    std::vector<Note> notes;
};

} // namespace protoexplorer
