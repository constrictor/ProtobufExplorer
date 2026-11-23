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

#include "JsonTree.h"

#include "JsonValues.h"
#include "SchemaTree.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace protoexplorer {

using Kind = JsonValue::Kind;
using Node = JsonTree::Node;

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("JsonTree", text);
}

bool isEnum(const Decl *type)
{
    return type && type->kind == DeclKind::Enum;
}

// To most types null is as good as nothing, but to some it is a value.
bool takesNull(const Decl *type)
{
    const Special special = type ? specialOf(*type) : Special::None;
    return special == Special::Value || special == Special::NullValue;
}

// Given as null, a field is left out.
bool isLeftOut(const Decl &field, const JsonValue &json)
{
    const FieldInfo &info = *field.field;
    return json.kind == Kind::Null
        && (info.repeated || !info.mapKey.isEmpty() || !takesNull(field.type));
}

QString counted(qsizetype count, const char *one, const char *many)
{
    return count == 1 ? tr(one) : tr(many).arg(count);
}

JsonTree::Form formOf(const Decl &field)
{
    using Form = JsonTree::Form;
    if (!field.type)
        return field.typeName.isEmpty() ? Form::Scalar : Form::Free;
    if (isEnum(field.type))
        return Form::Enum;
    switch (specialOf(*field.type)) {
    case Special::None:      return Form::Message;
    case Special::Any:       return Form::Any;
    case Special::Wrapper:   return Form::Wrapper;
    case Special::Value:
    case Special::Struct:
    case Special::ListValue: return Form::Free;
    default:                 return Form::Special;
    }
}

} // namespace

// Lays the JSON over the message.
class JsonTree::Builder
{
public:
    explicit Builder(JsonTree &tree) : m_tree(tree), m_index(*tree.m_index) {}

    void build()
    {
        const Decl &message = *m_tree.m_message;
        const std::vector<JsonValue> &values = m_tree.m_document.values;
        // An array is a message for each of its items, unless the message is one that is
        // written as an array.
        const Special special = specialOf(message);
        const bool list = values.size() == 1 && values.front().kind == Kind::Array
            && special != Special::Value && special != Special::ListValue;
        const std::vector<JsonValue> &roots = list ? values.front().members : values;
        for (size_t i = 0; i < roots.size(); ++i) {
            auto node = std::make_unique<Node>();
            node->decl = &message;
            node->type = &message;
            node->row = int(i);
            node->name = message.name;
            if (list || roots.size() > 1)
                node->name += QStringLiteral(" [%1]").arg(i);
            readMessageValue(*node, roots[i]);
            m_tree.m_roots.push_back(std::move(node));
        }
    }

private:
    static Node &addChild(Node &parent, const QString &name)
    {
        auto node = std::make_unique<Node>();
        node->parent = &parent;
        node->row = int(parent.children.size());
        node->name = name;
        parent.children.push_back(std::move(node));
        return *parent.children.back();
    }

    static void fail(Node &node, const QString &problem)
    {
        node.state = State::Problem;
        node.problem = problem;
    }

    static void check(Node &node, const Reading &reading)
    {
        if (!reading.problem.isEmpty())
            fail(node, reading.problem);
    }

    // A field with the value it is given, if any.
    void addField(Node &parent, const Decl &field, const JsonValue *json)
    {
        Node &node = addChild(parent, field.parent->kind == DeclKind::Extend ? field.field->jsonName
                                                                             : field.name);
        const FieldInfo &info = *field.field;
        node.decl = &field;
        node.type = field.type;
        node.json = json;
        node.form = info.repeated ? Form::List : info.mapKey.isEmpty() ? formOf(field) : Form::Map;
        if (!json || isLeftOut(field, *json)) {
            if (info.required)
                fail(node, tr("required, but not set"));
            else
                node.state = info.tracksPresence ? State::NotSet : State::Default;
            return;
        }
        if (node.form == Form::List)
            readList(node, *json);
        else if (node.form == Form::Map)
            readMap(node, *json);
        else
            readValue(node, *json);
    }

    // A single value: a field's, or one of its list or its map.
    void readValue(Node &node, const JsonValue &json)
    {
        node.json = &json;
        node.form = formOf(*node.decl);
        if (node.form == Form::Scalar)
            check(node, readScalar(node.decl->field->scalar, json));
        else if (node.form == Form::Enum)
            check(node, readEnum(*node.type, json));
        else if (node.type)
            readMessageValue(node, json);
        else
            readFree(node, json);
    }

    void readMessageValue(Node &node, const JsonValue &json)
    {
        node.json = &json;
        const Special special = specialOf(*node.type);
        switch (special) {
        case Special::None:
            node.form = Form::Message;
            if (json.kind == Kind::Object)
                readMembers(node, *node.type, json);
            else
                fail(node, tr("expected an object"));
            break;
        case Special::Any:
            node.form = Form::Any;
            if (json.kind == Kind::Object)
                readAny(node, json);
            else
                fail(node, tr("expected an object"));
            break;
        case Special::Wrapper:
            node.form = Form::Wrapper;
            check(node, readScalar(wrappedScalar(*node.type), json));
            break;
        case Special::Struct:
        case Special::ListValue:
        case Special::Value:
            node.form = Form::Free;
            if (special == Special::Struct && json.kind != Kind::Object)
                fail(node, tr("expected an object"));
            else if (special == Special::ListValue && json.kind != Kind::Array)
                fail(node, tr("expected a list"));
            else
                readFree(node, json);
            break;
        default:
            node.form = Form::Special;
            check(node, readSpecial(special, json));
            break;
        }
    }

    void readList(Node &node, const JsonValue &json)
    {
        if (json.kind != Kind::Array)
            return fail(node, tr("expected a list"));
        for (size_t i = 0; i < json.members.size(); ++i) {
            addItem(node, QStringLiteral("[%1]").arg(i), json.members[i],
                    tr("null cannot be in a list"));
        }
    }

    void readMap(Node &node, const JsonValue &json)
    {
        if (json.kind != Kind::Object)
            return fail(node, tr("expected an object"));
        QSet<QString> seen;
        for (const JsonValue &member : json.members) {
            Node &item = addItem(node, member.key, member, tr("null cannot be in a map"));
            const QString problem = seen.contains(member.key)
                ? tr("this key has been given before")
                : mapKeyProblem(node.decl->field->mapKey, member.key);
            if (!problem.isEmpty())
                fail(item, problem);
            seen.insert(member.key);
        }
    }

    Node &addItem(Node &parent, const QString &name, const JsonValue &json, const QString &noNull)
    {
        Node &item = addChild(parent, name);
        item.decl = parent.decl;
        item.type = parent.type;
        item.item = true;
        if (json.kind == Kind::Null && !takesNull(item.type)) {
            item.json = &json;
            item.form = formOf(*item.decl);
            fail(item, noNull);
        } else {
            readValue(item, json);
        }
        return item;
    }

    void readFree(Node &node, const JsonValue &json)
    {
        const bool object = json.kind == Kind::Object;
        QSet<QString> seen;
        for (size_t i = 0; i < json.members.size(); ++i) {
            const JsonValue &member = json.members[i];
            Node &child = addChild(node, object ? member.key : QStringLiteral("[%1]").arg(i));
            child.json = &member;
            child.form = Form::Free;
            readFree(child, member);
            if (object && seen.contains(member.key))
                fail(child, tr("this key has been given before"));
            seen.insert(member.key);
        }
    }

    // Every field of the message, in the order they are declared, then what else the JSON
    // has. `skip` is a key that is not for the message.
    void readMembers(Node &node, const Decl &message, const JsonValue &json,
                     const QString &skip = QString())
    {
        struct Extra
        {
            const JsonValue *json;
            QString problem;
        };
        QHash<const Decl *, const JsonValue *> given;
        QHash<const Decl *, const Decl *> chosen; // of each oneof
        QHash<const Decl *, QString> refused;
        std::vector<const Decl *> extensions;
        std::vector<Extra> extras;
        for (const JsonValue &member : json.members) {
            if (!skip.isNull() && member.key == skip)
                continue;
            const Decl *field = m_index.field(message, member.key);
            if (!field) {
                extras.push_back({&member, tr("not in %1").arg(message.name)});
                continue;
            }
            if (member.key == field->name && field->name != field->field->jsonName)
                m_tree.m_usesOwnNames = true;
            const bool leftOut = isLeftOut(*field, member);
            const JsonValue *before = given.value(field);
            if (before && !isLeftOut(*field, *before)) {
                if (!leftOut)
                    extras.push_back({&member, tr("%1 has been given before").arg(field->name)});
                continue;
            }
            if (field->parent->kind == DeclKind::Extend && !given.contains(field))
                extensions.push_back(field);
            given.insert(field, &member);
            if (field->parent->kind == DeclKind::Oneof && !leftOut) {
                const Decl *first = chosen.value(field->parent);
                if (first) {
                    refused.insert(field, tr("%1 already has %2").arg(field->parent->name, first->name));
                } else {
                    chosen.insert(field->parent, field);
                }
            }
        }

        const auto add = [&](Node &parent, const Decl &field) {
            addField(parent, field, given.value(&field));
            const auto refusal = refused.constFind(&field);
            if (refusal != refused.cend())
                fail(*parent.children.back(), *refusal);
        };
        for (const auto &member : message.members) {
            if (member->kind != DeclKind::Oneof) {
                add(node, *member);
                continue;
            }
            Node &oneof = addChild(node, member->name);
            oneof.decl = member.get();
            oneof.form = Form::Oneof;
            oneof.state = chosen.contains(member.get()) ? State::Set : State::NotSet;
            for (const auto &field : member->members)
                add(oneof, *field);
        }
        for (const Decl *extension : extensions)
            add(node, *extension);
        for (const Extra &extra : extras) {
            Node &child = addChild(node, extra.json->key);
            child.json = extra.json;
            child.form = Form::Unknown;
            fail(child, extra.problem);
        }
    }

    // The message inside says in the JSON itself what it is.
    void readAny(Node &node, const JsonValue &json)
    {
        static const QString typeKey = QStringLiteral("@type");
        const JsonValue *url = json.find(typeKey);
        if (!url) {
            // With nothing in it there is nothing to say.
            if (!json.members.empty())
                fail(node, tr("no @type to say what it is"));
            readFree(node, json);
            return;
        }
        const Decl *inside = nullptr;
        if (url->kind == Kind::String)
            inside = m_index.message(url->text.sliced(url->text.lastIndexOf(u'/') + 1));
        if (!inside) {
            readFree(node, json);
            for (const auto &child : node.children) {
                if (child->json == url) {
                    fail(*child, url->kind == Kind::String ? tr("not in this schema")
                                                           : tr("expected a string"));
                }
            }
            return;
        }

        Node &type = addChild(node, typeKey);
        type.json = url;
        type.form = Form::Free;
        type.type = inside;
        if (specialOf(*inside) == Special::None) {
            readMembers(node, *inside, json, typeKey);
            return;
        }
        // One with a JSON form of its own is in there under a name.
        for (const JsonValue &member : json.members) {
            if (&member == url)
                continue;
            Node &child = addChild(node, member.key);
            child.json = &member;
            if (member.key == u"value") {
                child.type = inside;
                readMessageValue(child, member);
            } else {
                child.form = Form::Unknown;
                fail(child, tr("not in %1").arg(inside->name));
            }
        }
    }

    JsonTree &m_tree;
    const SchemaIndex &m_index;
};

// Writes the JSON out again, with what it leaves out.
class JsonTree::Writer
{
public:
    Writer(const JsonTree &tree, bool compact) : m_tree(tree), m_compact(compact) {}

    QString take() { return std::move(m_out); }

    void write(const Node &node, int depth)
    {
        const bool readable = node.state != State::Problem;
        switch (node.form) {
        case Form::Message:
        case Form::Any:
            if (node.json->kind == Kind::Object)
                return writeMembers(node, depth);
            break;
        case Form::List:
            if (readable)
                return writeItems(node, depth, u'[', u']');
            break;
        case Form::Map:
            if (readable)
                return writeItems(node, depth, u'{', u'}');
            break;
        default:
            break;
        }
        write(*node.json, depth);
    }

    void write(const JsonValue &json, int depth)
    {
        if (json.members.empty()) {
            const QString text = m_tree.m_document.slice(json);
            m_out += json.kind == Kind::Array ? QStringLiteral("[]")
                : json.kind == Kind::Object   ? QStringLiteral("{}")
                                              : text;
            return;
        }
        const bool object = json.kind == Kind::Object;
        open(object ? u'{' : u'[');
        for (const JsonValue &member : json.members) {
            next(depth);
            if (object)
                key(member.key);
            write(member, depth + 1);
        }
        close(object ? u'}' : u']', depth);
    }

    void open(QChar bracket)
    {
        m_out += bracket;
        m_first = true;
    }

    void next(int depth)
    {
        if (!m_first)
            m_out += u',';
        m_first = false;
        newLine(depth + 1);
    }

    void close(QChar bracket, int depth)
    {
        if (!m_first)
            newLine(depth);
        m_out += bracket;
        m_first = false;
    }

private:
    void newLine(int depth)
    {
        if (!m_compact)
            m_out += u'\n' + QString(2 * depth, u' ');
    }

    void key(const QString &name)
    {
        m_out += quoteJson(name) + (m_compact ? QStringLiteral(":") : QStringLiteral(": "));
    }

    void writeMembers(const Node &node, int depth)
    {
        open(u'{');
        writeFields(node, depth);
        close(u'}', depth);
    }

    void writeFields(const Node &node, int depth)
    {
        for (const auto &child : node.children) {
            if (child->form == Form::Oneof) {
                writeFields(*child, depth);
            } else if (child->state == State::Default) {
                next(depth);
                const FieldInfo &info = *child->decl->field;
                key(child->json ? child->json->key
                    : m_tree.m_usesOwnNames ? child->decl->name : info.jsonName);
                m_out += defaultJson(*child->decl);
            } else if (child->json) {
                next(depth);
                key(child->json->key);
                write(*child, depth + 1);
            }
        }
    }

    void writeItems(const Node &node, int depth, QChar opening, QChar closing)
    {
        open(opening);
        for (const auto &child : node.children) {
            next(depth);
            if (node.form == Form::Map)
                key(child->name);
            write(*child, depth + 1);
        }
        close(closing, depth);
    }

    const JsonTree &m_tree;
    bool m_compact;
    bool m_first = true;
    QString m_out;
};

JsonTree::JsonTree(std::shared_ptr<const SchemaIndex> index, const Decl &message,
                   JsonDocument document)
    : m_index(std::move(index))
    , m_message(&message)
    , m_document(std::move(document))
{
    Builder(*this).build();
    for (const auto &root : m_roots)
        number(*root);
}

void JsonTree::number(Node &node)
{
    node.order = int(m_nodes.size());
    m_nodes.push_back(&node);
    if (node.state == State::Problem || node.isField())
        m_byState[size_t(node.state)].push_back(&node);
    for (const auto &child : node.children)
        number(*child);
}

QString JsonTree::valueText(const Node &node) const
{
    const QString null = node.json && node.json->kind == Kind::Null ? QStringLiteral("null → ")
                                                                    : QString();
    switch (node.state) {
    case State::Default:
        return null + defaultText(*node.decl);
    case State::NotSet: {
        const QString value = node.form == Form::Oneof ? QString() : defaultText(*node.decl);
        return null + (value.isEmpty() ? tr("not set") : tr("not set (%1)").arg(value));
    }
    case State::Problem:
        if (!node.json || (node.isField() && node.json->kind == Kind::Null))
            return null + tr("not set");
        break; // and what is wrong can be something other than the value
    case State::Set:
        break;
    }

    const auto or_raw = [&](const Reading &reading) {
        return reading.problem.isEmpty() ? reading.text : elided(m_document.slice(*node.json));
    };
    switch (node.form) {
    case Form::Message:
        if (node.json->kind == Kind::Object)
            return node.children.empty() ? QStringLiteral("{}") : QString();
        break;
    case Form::Any: {
        const JsonValue *url = node.json->find(u"@type");
        if (url && url->kind == Kind::String)
            return url->text.sliced(url->text.lastIndexOf(u'/') + 1);
        break;
    }
    case Form::Free:
        switch (node.json->kind) {
        case Kind::Array:
            return counted(qsizetype(node.json->members.size()), "1 item", "%1 items");
        case Kind::Object:
            return counted(qsizetype(node.json->members.size()), "1 entry", "%1 entries");
        case Kind::String:
            return readScalar(u"string", *node.json).text;
        default:
            return node.json->text;
        }
    case Form::Scalar:
        return or_raw(readScalar(node.decl->field->scalar, *node.json));
    case Form::Enum:
        return or_raw(readEnum(*node.type, *node.json));
    case Form::Special:
        return or_raw(readSpecial(specialOf(*node.type), *node.json));
    case Form::Wrapper:
        return or_raw(readScalar(wrappedScalar(*node.type), *node.json));
    case Form::List:
        if (node.json->kind == Kind::Array)
            return counted(qsizetype(node.json->members.size()), "1 item", "%1 items");
        break;
    case Form::Map:
        if (node.json->kind == Kind::Object)
            return counted(qsizetype(node.json->members.size()), "1 entry", "%1 entries");
        break;
    case Form::Oneof:
        for (const auto &child : node.children) {
            if (child->state == State::Set)
                return child->name;
        }
        // The one that is chosen has something wrong with it.
        for (const auto &child : node.children) {
            if (child->json && child->json->kind != Kind::Null)
                return child->name;
        }
        return {};
    case Form::Unknown:
        break;
    }
    return elided(m_document.slice(*node.json));
}

QString JsonTree::typeText(const Node &node) const
{
    QString type;
    if (node.item) {
        type = node.decl->typeName.isEmpty() ? node.decl->field->scalar : node.decl->typeName;
    } else if (node.decl) {
        type = node.decl->signature;
    } else if (node.type && node.form != Form::Free) {
        type = node.type->name; // inside an Any
    } else if (node.form == Form::Free && node.json) {
        switch (node.json->kind) {
        case Kind::Null:   break;
        case Kind::Bool:   type = tr("bool"); break;
        case Kind::Number: type = tr("number"); break;
        case Kind::String: type = tr("string"); break;
        case Kind::Array:  type = tr("list"); break;
        case Kind::Object: type = tr("object"); break;
        }
    }
    if (node.decl && node.decl->kind == DeclKind::Field && !node.decl->type
        && !node.decl->typeName.isEmpty()) {
        type += QStringLiteral(" ") + tr("(not found)");
    }
    if (node.problem.isEmpty())
        return type;
    return type.isEmpty() ? node.problem : QStringLiteral("%1 — %2").arg(type, node.problem);
}

QString JsonTree::copyText(const Node &node) const
{
    if (node.json && !node.isMissing()) {
        if (node.form == Form::Enum && node.state == State::Set && node.json->kind == Kind::Null)
            return QStringLiteral("NULL_VALUE");
        return node.json->kind == Kind::String ? node.json->text : m_document.slice(*node.json);
    }
    if (!node.isField() || node.state == State::Problem)
        return {};
    const FieldInfo &info = *node.decl->field;
    if (info.scalar == u"string" && !info.repeated && info.mapKey.isEmpty())
        return QString::fromUtf8(info.defaultValue);
    if (info.scalar == u"bytes" && !info.repeated && info.mapKey.isEmpty())
        return QString::fromLatin1(info.defaultValue.toBase64());
    if (info.repeated || !info.mapKey.isEmpty())
        return defaultJson(*node.decl);
    return defaultText(*node.decl);
}

QString JsonTree::pathTo(const Node &node) const
{
    static const QRegularExpression plain(QStringLiteral("\\A[A-Za-z_][A-Za-z0-9_]*\\z"));
    if (!node.parent) {
        const bool list = m_document.values.size() == 1 && node.json != &m_document.values.front();
        return list ? QStringLiteral("$[%1]").arg(node.row) : QStringLiteral("$");
    }
    const QString above = pathTo(*node.parent);
    if (node.form == Form::Oneof)
        return above;
    const JsonValue *in = node.json ? jsonFor(*node.parent) : nullptr;
    if (in && in->kind == Kind::Array)
        return above + QStringLiteral("[%1]").arg(node.row);
    const QString key = node.json ? node.json->key
        : m_usesOwnNames          ? node.decl->name
                                  : node.decl->field->jsonName;
    return above + (plain.match(key).hasMatch() ? u'.' + key : u'[' + quoteJson(key) + u']');
}

QString JsonTree::withDefaults() const
{
    const std::vector<JsonValue> &values = m_document.values;
    if (values.size() == 1) {
        Writer writer(*this, false);
        if (!m_roots.empty() && m_roots.front()->json == &values.front()) {
            writer.write(*m_roots.front(), 0);
            return writer.take();
        }
        writer.open(u'[');
        for (const auto &root : m_roots) {
            writer.next(0);
            writer.write(*root, 1);
        }
        writer.close(u']', 0);
        return writer.take();
    }
    // JSON Lines, where every line is a whole value.
    QStringList lines;
    for (const auto &root : m_roots) {
        Writer writer(*this, true);
        writer.write(*root, 0);
        lines << writer.take();
    }
    return lines.join(u'\n');
}

const JsonTree::Node *JsonTree::nodeAt(qsizetype offset) const
{
    const auto holds = [offset](const Node &node) {
        return node.json && node.json->keyBegin <= offset && offset <= node.json->end;
    };
    // A oneof is not in the JSON, but what it holds is.
    const auto among = [&holds](const std::vector<std::unique_ptr<Node>> &nodes) -> const Node * {
        for (const auto &node : nodes) {
            if (holds(*node))
                return node.get();
            if (node->form != Form::Oneof)
                continue;
            for (const auto &child : node->children) {
                if (holds(*child))
                    return child.get();
            }
        }
        return nullptr;
    };
    const Node *found = nullptr;
    for (const Node *next = among(m_roots); next; next = among(next->children))
        found = next;
    return found;
}

const JsonValue *JsonTree::jsonFor(const Node &node)
{
    const Node *at = &node;
    while (!at->json && at->parent)
        at = at->parent;
    return at->json;
}

std::vector<const Decl *> JsonTree::sourceParts(const Node &node)
{
    if (node.decl)
        return SchemaTree::sourceParts(*node.decl);
    if (node.type)
        return {node.type};
    // Nothing declares it: what would have.
    for (const Node *at = node.parent; at; at = at->parent) {
        if (node.form == Form::Unknown && at->form == Form::Any) {
            for (const auto &child : at->children) {
                if (!child->decl && child->type)
                    return {child->type};
            }
        }
        if (node.form == Form::Unknown && at->type)
            return {at->type};
        if (at->decl)
            return SchemaTree::sourceParts(*at->decl);
        if (at->type)
            return {at->type};
    }
    return {};
}

} // namespace protoexplorer
