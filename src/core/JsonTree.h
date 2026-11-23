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

#include <memory>
#include <vector>

namespace protoexplorer {

// A JSON text laid over the message it is of: every field the message has, with the value
// the JSON gives it or the one it has for being left out.
//
// Unlike a SchemaTree this ends wherever the JSON does, so all of it is made at once. It
// does not change after that.
class JsonTree
{
public:
    enum class State : quint8 {
        Set,     // in the JSON
        Default, // left out, which is the same as being set to the default
        NotSet,  // left out, which is not
        Problem, // in the JSON but not to be had, or left out and not to be
    };
    enum class Form : quint8 {
        Message,
        Any,
        Free, // JSON that no message describes
        Scalar,
        Enum,
        Special, // a string that stands for a message, such as a timestamp
        Wrapper,
        List,
        Map,
        Oneof,
        Unknown, // not in the message
    };

    struct Node
    {
        const Decl *decl = nullptr; // the field or the oneof; the message, for a root
        const Decl *type = nullptr; // the message or the enum its value is of
        const JsonValue *json = nullptr;
        Node *parent = nullptr;
        int row = 0;
        int order = 0; // among all of them, from the top of the tree to the bottom
        Form form = Form::Scalar;
        State state = State::Set;
        bool item = false; // of a list or of a map
        QString name;
        QString problem;
        std::vector<std::unique_ptr<Node>> children;

        bool isMissing() const { return state == State::Default || state == State::NotSet; }
        // A field of a message rather than some part of one's value.
        bool isField() const { return decl && decl->kind == DeclKind::Field && !item; }
    };

    JsonTree(std::shared_ptr<const SchemaIndex> index, const Decl &message, JsonDocument document);

    const Decl &message() const { return *m_message; }
    const JsonDocument &document() const { return m_document; }

    // One for each message in the JSON.
    const std::vector<std::unique_ptr<Node>> &roots() const { return m_roots; }
    // From top to bottom.
    const std::vector<const Node *> &nodes() const { return m_nodes; }
    // The fields in a state, or whatever there is a problem with.
    const std::vector<const Node *> &nodes(State state) const { return m_byState[size_t(state)]; }

    QString valueText(const Node &node) const;
    QString typeText(const Node &node) const;

    // The value by itself, to paste elsewhere. Nothing if there is none.
    QString copyText(const Node &node) const;
    // Where the value is or would be, from `$`.
    QString pathTo(const Node &node) const;
    // The JSON with the fields it leaves out at their defaults filled in.
    QString withDefaults() const;

    // What there is at a place in the JSON, as far down as it goes.
    const Node *nodeAt(qsizetype offset) const;
    // The part of the JSON to show for a node: its own, or what it is missing from.
    static const JsonValue *jsonFor(const Node &node);
    // The declarations to show for a node.
    static std::vector<const Decl *> sourceParts(const Node &node);

private:
    class Builder;
    class Writer;

    void number(Node &node);

    std::shared_ptr<const SchemaIndex> m_index;
    const Decl *m_message;
    JsonDocument m_document;
    bool m_usesOwnNames = false; // the JSON calls fields what the schema does
    std::vector<std::unique_ptr<Node>> m_roots;
    std::vector<const Node *> m_nodes;
    std::vector<const Node *> m_byState[4];
};

} // namespace protoexplorer
