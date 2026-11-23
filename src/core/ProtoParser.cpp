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

// The one file that sees protobuf's headers, and it sees them before Qt's: the two
// disagree about words like `signals` and `emit`.
#include <absl/log/globals.h>
#include <absl/strings/escaping.h>
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "ProtoParser.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>
#include <map>

namespace protoexplorer {

namespace {

namespace pb = google::protobuf;

using Path = std::vector<int>;

// One mistake tends to set off a cascade, and past a point the list helps nobody.
constexpr size_t kMaxNotes = 20;

QString tr(const char *text)
{
    return QCoreApplication::translate("ProtoParser", text);
}

QString toQString(std::string_view s)
{
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}

class NoteCollector : public pb::io::ErrorCollector
{
public:
    NoteCollector(QString path, std::vector<Note> &notes)
        : m_path(std::move(path)), m_notes(notes)
    {
    }

    void RecordError(int line, pb::io::ColumnNumber, absl::string_view message) override
    {
        if (m_notes.size() < kMaxNotes)
            m_notes.push_back({m_path, line < 0 ? 0 : line + 1, toQString(message)});
    }

private:
    QString m_path;
    std::vector<Note> &m_notes;
};

class NoErrors : public pb::io::ErrorCollector
{
public:
    void RecordError(int, pb::io::ColumnNumber, absl::string_view) override {}
};

bool parseInto(const QByteArray &bytes, pb::io::ErrorCollector *errors,
               pb::FileDescriptorProto &out)
{
    pb::io::ArrayInputStream stream(bytes.constData(), int(bytes.size()));
    pb::io::Tokenizer tokenizer(&stream, errors);
    // Fresh each time: a parser carries comments over from the file before.
    pb::compiler::Parser parser;
    parser.RecordErrorsTo(errors);
    return parser.Parse(&tokenizer, &out);
}

// The parser gives up on the whole file when it cannot make sense of the `syntax` or
// `edition` line. Blanked out in place, so that every position after it still holds, the
// rest of the file parses as usual.
QByteArray withoutSyntaxLine(const SourceText &text)
{
    QByteArray bytes = text.bytes();
    qsizetype at = bytes.startsWith("\xEF\xBB\xBF") ? 3 : 0;
    auto comment = text.comments().begin();
    while (at < bytes.size()) {
        if (comment != text.comments().end() && comment->begin == at) {
            at = comment->end;
            ++comment;
        } else if (bytes[at] == ' ' || (bytes[at] >= '\t' && bytes[at] <= '\r')) {
            ++at;
        } else {
            break;
        }
    }
    while (at < bytes.size() && bytes[at] != '\n') {
        const bool last = bytes[at] == ';';
        bytes[at++] = ' ';
        if (last)
            break;
    }
    return bytes;
}

// Nothing for a field of a message or an enum, which go by name.
QString scalarName(const pb::FieldDescriptorProto &field)
{
    using F = pb::FieldDescriptorProto;
    if (!field.type_name().empty())
        return {}; // and the type can be unset, which reads as a double
    switch (field.type()) {
    case F::TYPE_DOUBLE:   return QStringLiteral("double");
    case F::TYPE_FLOAT:    return QStringLiteral("float");
    case F::TYPE_INT64:    return QStringLiteral("int64");
    case F::TYPE_UINT64:   return QStringLiteral("uint64");
    case F::TYPE_INT32:    return QStringLiteral("int32");
    case F::TYPE_FIXED64:  return QStringLiteral("fixed64");
    case F::TYPE_FIXED32:  return QStringLiteral("fixed32");
    case F::TYPE_BOOL:     return QStringLiteral("bool");
    case F::TYPE_STRING:   return QStringLiteral("string");
    case F::TYPE_BYTES:    return QStringLiteral("bytes");
    case F::TYPE_UINT32:   return QStringLiteral("uint32");
    case F::TYPE_SFIXED32: return QStringLiteral("sfixed32");
    case F::TYPE_SFIXED64: return QStringLiteral("sfixed64");
    case F::TYPE_SINT32:   return QStringLiteral("sint32");
    case F::TYPE_SINT64:   return QStringLiteral("sint64");
    default:               return QString();
    }
}

// What protobuf makes of a field's name for JSON.
QString toJsonName(const std::string &name)
{
    QString result;
    bool capital = false;
    for (const QChar c : toQString(name)) {
        if (c == u'_') {
            capital = true;
            continue;
        }
        result += capital && c >= u'a' && c <= u'z' ? QChar(c.unicode() - u'a' + u'A') : c;
        capital = false;
    }
    return result;
}

// What these options set a feature to, if they do: in a statement of its own, or among
// others in braces.
QString feature(const char *name, const pb::RepeatedPtrField<pb::UninterpretedOption> &options)
{
    for (const auto &option : options) {
        if (option.name_size() == 0 || option.name(0).is_extension()
            || option.name(0).name_part() != "features") {
            continue;
        }
        if (option.name_size() == 2 && option.name(1).name_part() == name)
            return toQString(option.identifier_value());
        if (option.name_size() == 1) {
            const QRegularExpression inside(
                QStringLiteral("\\b%1\\s*:\\s*(\\w+)").arg(QLatin1StringView(name)));
            const auto match = inside.match(toQString(option.aggregate_value()));
            if (match.hasMatch())
                return match.captured(1);
        }
    }
    return {};
}

// Turns one parsed file into declarations.
class Builder
{
public:
    Builder(SourceFile &file, const pb::FileDescriptorProto &proto)
        : m_file(file), m_proto(proto)
    {
    }

    void build();

private:
    using Decls = std::vector<std::unique_ptr<Decl>>;

    static Path child(Path path, int field, int index)
    {
        path.push_back(field);
        path.push_back(index);
        return path;
    }

    static QString qualified(const QString &scope, const QString &name)
    {
        return scope.isEmpty() ? name : scope + u'.' + name;
    }

    static void sortByPosition(Decls &decls)
    {
        std::stable_sort(decls.begin(), decls.end(), [](const auto &a, const auto &b) {
            return a->span.begin < b->span.begin;
        });
    }

    Span toSpan(const pb::SourceCodeInfo::Location &location) const;
    Span spanAt(const Path &path) const;
    // A visibility keyword is written before the declaration and left out of its span.
    Span withVisibility(Span span) const;

    std::unique_ptr<Decl> make(DeclKind kind, const std::string &name, const QString &scope,
                               Span span, const Decl *parent) const;
    void addMessage(const pb::DescriptorProto &proto, const Path &path, const QString &scope,
                    const Decl *parent, Decls &into);
    void addEnum(const pb::EnumDescriptorProto &proto, const Path &path, const QString &scope,
                 const Decl *parent, Decls &into);
    void addService(const pb::ServiceDescriptorProto &proto, const Path &path, Decls &into);
    void addExtends(const pb::RepeatedPtrField<pb::FieldDescriptorProto> &fields,
                    const pb::RepeatedPtrField<pb::DescriptorProto> &siblings,
                    const Path &path, const QString &scope, const Decl *parent, Decls &into);
    std::unique_ptr<Decl> makeField(const pb::FieldDescriptorProto &proto,
                                    const pb::RepeatedPtrField<pb::DescriptorProto> &siblings,
                                    const Path &path, const QString &scope,
                                    const Decl *parent) const;
    std::unique_ptr<FieldInfo> makeFieldInfo(const pb::FieldDescriptorProto &proto,
                                             const Decl *parent) const;

    SourceFile &m_file;
    const pb::FileDescriptorProto &m_proto;
    std::map<Path, Span> m_spans;
    // What the file says of fields and enums that do not say themselves.
    bool m_tracksPresence = true;
    bool m_closedEnums = false;
};

void Builder::build()
{
    for (const auto &location : m_proto.source_code_info().location()) {
        // The first one wins: a field whose type fails to parse records its own path again.
        m_spans.try_emplace(Path(location.path().begin(), location.path().end()),
                            toSpan(location));
    }

    const auto &options = m_proto.options().uninterpreted_option();
    const bool editions = m_proto.syntax() == "editions";
    const bool proto3 = m_proto.syntax() == "proto3";
    m_tracksPresence = !proto3 && feature("field_presence", options) != u"IMPLICIT";
    m_closedEnums = editions ? feature("enum_type", options) == u"CLOSED" : !proto3;

    m_file.package = toQString(m_proto.package());
    for (int i = 0; i < m_proto.dependency_size(); ++i) {
        Import import;
        import.path = toQString(m_proto.dependency(i));
        m_file.imports.push_back(import);
    }
    for (const int i : m_proto.public_dependency()) {
        if (i >= 0 && size_t(i) < m_file.imports.size())
            m_file.imports[size_t(i)].isPublic = true;
    }

    using F = pb::FileDescriptorProto;
    const QString &package = m_file.package;
    for (int i = 0; i < m_proto.message_type_size(); ++i) {
        addMessage(m_proto.message_type(i), {F::kMessageTypeFieldNumber, i}, package, nullptr,
                   m_file.decls);
    }
    for (int i = 0; i < m_proto.enum_type_size(); ++i) {
        addEnum(m_proto.enum_type(i), {F::kEnumTypeFieldNumber, i}, package, nullptr,
                m_file.decls);
    }
    for (int i = 0; i < m_proto.service_size(); ++i)
        addService(m_proto.service(i), {F::kServiceFieldNumber, i}, m_file.decls);
    addExtends(m_proto.extension(), m_proto.message_type(), {F::kExtensionFieldNumber},
               package, nullptr, m_file.decls);
    sortByPosition(m_file.decls);
}

Span Builder::toSpan(const pb::SourceCodeInfo::Location &location) const
{
    // Three numbers when it begins and ends on one line, four otherwise.
    const auto &s = location.span();
    if (s.size() < 3)
        return {};
    const int endLine = s.size() > 3 ? s[2] : s[0];
    const int endColumn = s.size() > 3 ? s[3] : s[2];
    Span span;
    span.begin = m_file.text.offsetAt(s[0], s[1]);
    // One that failed to parse can end before it begins.
    span.end = std::max(span.begin, m_file.text.offsetAt(endLine, endColumn));
    return span;
}

Span Builder::spanAt(const Path &path) const
{
    const auto it = m_spans.find(path);
    return it == m_spans.end() ? Span() : it->second;
}

Span Builder::withVisibility(Span span) const
{
    const QByteArray &bytes = m_file.text.bytes();
    qsizetype at = span.begin;
    while (at > 0 && QByteArrayView(" \t\r\n").contains(bytes[at - 1]))
        --at;
    for (const QByteArrayView word : {QByteArrayView("export"), QByteArrayView("local")}) {
        if (at >= word.size() && QByteArrayView(bytes).sliced(at - word.size(), word.size()) == word) {
            span.begin = at - word.size();
            break;
        }
    }
    return span;
}

std::unique_ptr<Decl> Builder::make(DeclKind kind, const std::string &name,
                                    const QString &scope, Span span, const Decl *parent) const
{
    auto decl = std::make_unique<Decl>();
    decl->kind = kind;
    decl->name = toQString(name);
    decl->fullName = qualified(scope, decl->name);
    decl->file = &m_file;
    decl->span = span;
    decl->parent = parent;
    return decl;
}

void Builder::addMessage(const pb::DescriptorProto &proto, const Path &path,
                         const QString &scope, const Decl *parent, Decls &into)
{
    using D = pb::DescriptorProto;
    // A map's entry type is the parser's doing. A nameless one is what it leaves behind
    // when it cannot make out a declaration.
    if (proto.options().map_entry() || proto.name().empty())
        return;

    Span span = spanAt(path);
    if (proto.visibility() != pb::SymbolVisibility::VISIBILITY_UNSET)
        span = withVisibility(span);
    auto message = make(DeclKind::Message, proto.name(), scope, span, parent);
    message->signature = QStringLiteral("message");
    const QString &inner = message->fullName;

    // Made only once something goes in, which is what leaves out the invented oneof that
    // proto3 wraps an `optional` field in.
    std::vector<Decl *> oneofs(size_t(proto.oneof_decl_size()), nullptr);
    for (int i = 0; i < proto.field_size(); ++i) {
        const auto &field = proto.field(i);
        if (field.name().empty())
            continue;
        const Path fieldPath = child(path, D::kFieldFieldNumber, i);
        const int index = field.has_oneof_index() && !field.proto3_optional()
            ? field.oneof_index() : -1;
        Decl *owner = message.get();
        if (index >= 0 && size_t(index) < oneofs.size()) {
            Decl *&oneof = oneofs[size_t(index)];
            if (!oneof) {
                auto made = make(DeclKind::Oneof, proto.oneof_decl(index).name(), inner,
                                 spanAt(child(path, D::kOneofDeclFieldNumber, index)),
                                 message.get());
                made->signature = QStringLiteral("oneof");
                oneof = made.get();
                message->members.push_back(std::move(made));
            }
            owner = oneof;
        }
        owner->members.push_back(makeField(field, proto.nested_type(), fieldPath, inner, owner));
    }
    sortByPosition(message->members);

    for (int i = 0; i < proto.nested_type_size(); ++i) {
        addMessage(proto.nested_type(i), child(path, D::kNestedTypeFieldNumber, i), inner,
                   message.get(), message->nested);
    }
    for (int i = 0; i < proto.enum_type_size(); ++i) {
        addEnum(proto.enum_type(i), child(path, D::kEnumTypeFieldNumber, i), inner,
                message.get(), message->nested);
    }
    Path extends = path;
    extends.push_back(D::kExtensionFieldNumber);
    addExtends(proto.extension(), proto.nested_type(), extends, inner, message.get(),
               message->nested);
    sortByPosition(message->nested);

    into.push_back(std::move(message));
}

void Builder::addEnum(const pb::EnumDescriptorProto &proto, const Path &path,
                      const QString &scope, const Decl *parent, Decls &into)
{
    if (proto.name().empty())
        return;
    Span span = spanAt(path);
    if (proto.visibility() != pb::SymbolVisibility::VISIBILITY_UNSET)
        span = withVisibility(span);
    auto decl = make(DeclKind::Enum, proto.name(), scope, span, parent);
    decl->signature = QStringLiteral("enum");
    const QString type = feature("enum_type", proto.options().uninterpreted_option());
    decl->closed = type.isEmpty() ? m_closedEnums : type == u"CLOSED";
    for (int i = 0; i < proto.value_size(); ++i) {
        const auto &value = proto.value(i);
        if (value.name().empty())
            continue;
        auto made = make(DeclKind::EnumValue, value.name(), decl->fullName,
                         spanAt(child(path, pb::EnumDescriptorProto::kValueFieldNumber, i)),
                         decl.get());
        made->number = value.number();
        made->hasNumber = value.has_number();
        decl->members.push_back(std::move(made));
    }
    into.push_back(std::move(decl));
}

void Builder::addService(const pb::ServiceDescriptorProto &proto, const Path &path, Decls &into)
{
    if (proto.name().empty())
        return;
    auto service = make(DeclKind::Service, proto.name(), m_file.package, spanAt(path), nullptr);
    service->signature = QStringLiteral("service");
    for (int i = 0; i < proto.method_size(); ++i) {
        const auto &method = proto.method(i);
        if (method.name().empty())
            continue;
        auto made = make(DeclKind::Method, method.name(), service->fullName,
                         spanAt(child(path, pb::ServiceDescriptorProto::kMethodFieldNumber, i)),
                         service.get());
        const auto side = [](const std::string &type, bool streaming) {
            return (streaming ? QStringLiteral("stream ") : QString()) + toQString(type);
        };
        made->signature = QStringLiteral("rpc (%1) → %2")
                              .arg(side(method.input_type(), method.client_streaming()),
                                   side(method.output_type(), method.server_streaming()));
        made->method = std::make_unique<MethodInfo>();
        made->method->request = toQString(method.input_type());
        made->method->response = toQString(method.output_type());
        service->members.push_back(std::move(made));
    }
    into.push_back(std::move(service));
}

void Builder::addExtends(const pb::RepeatedPtrField<pb::FieldDescriptorProto> &fields,
                         const pb::RepeatedPtrField<pb::DescriptorProto> &siblings,
                         const Path &path, const QString &scope, const Decl *parent,
                         Decls &into)
{
    if (fields.empty())
        return;
    // Every block in a scope goes by the same path, so which fields belong to which is
    // told by the order the locations come in: a block, then what is inside it.
    Decl *block = nullptr;
    Span blockSpan;
    std::vector<bool> taken(size_t(fields.size()), false);
    for (const auto &location : m_proto.source_code_info().location()) {
        const auto &at = location.path();
        if (size_t(at.size()) < path.size() || !std::equal(path.begin(), path.end(), at.begin()))
            continue;
        if (size_t(at.size()) == path.size()) {
            block = nullptr;
            blockSpan = toSpan(location);
            continue;
        }
        if (size_t(at.size()) != path.size() + 1)
            continue;
        const int i = at[at.size() - 1];
        if (i < 0 || i >= fields.size() || taken[size_t(i)] || fields[i].name().empty())
            continue;
        taken[size_t(i)] = true;
        if (!block) {
            auto made = make(DeclKind::Extend, fields[i].extendee(), scope, blockSpan, parent);
            made->signature = QStringLiteral("extend");
            made->typeName = toQString(fields[i].extendee());
            made->fullName = QStringLiteral("extend ") + made->typeName;
            block = made.get();
            into.push_back(std::move(made));
        }
        Path fieldPath = path;
        fieldPath.push_back(i);
        block->members.push_back(makeField(fields[i], siblings, fieldPath, scope, block));
    }
}

std::unique_ptr<Decl> Builder::makeField(
    const pb::FieldDescriptorProto &proto,
    const pb::RepeatedPtrField<pb::DescriptorProto> &siblings, const Path &path,
    const QString &scope, const Decl *parent) const
{
    using F = pb::FieldDescriptorProto;
    auto field = make(DeclKind::Field, proto.name(), scope, spanAt(path), parent);
    field->number = proto.number();
    field->hasNumber = proto.has_number();
    field->field = makeFieldInfo(proto, parent);
    if (parent && parent->kind == DeclKind::Extend)
        field->field->jsonName = u'[' + field->fullName + u']';

    const auto typeOf = [](const F &f) {
        return f.type_name().empty() ? scalarName(f) : toQString(f.type_name());
    };

    if (!proto.type_name().empty()) {
        for (const auto &entry : siblings) {
            if (!entry.options().map_entry() || entry.name() != proto.type_name()
                || entry.field_size() != 2) {
                continue;
            }
            field->signature = QStringLiteral("map<%1, %2>")
                                   .arg(typeOf(entry.field(0)), typeOf(entry.field(1)));
            field->typeName = toQString(entry.field(1).type_name());
            field->field->mapKey = scalarName(entry.field(0));
            field->field->scalar = scalarName(entry.field(1));
            field->field->repeated = false;
            return field;
        }
    }

    field->typeName = toQString(proto.type_name());
    field->signature = typeOf(proto);
    // Only a label that is written: the parser fills one in where there is none.
    Path label = path;
    label.push_back(F::kLabelFieldNumber);
    if (m_spans.contains(label)) {
        const QString word = proto.label() == F::LABEL_REPEATED ? QStringLiteral("repeated ")
            : proto.label() == F::LABEL_REQUIRED                ? QStringLiteral("required ")
                                                                : QStringLiteral("optional ");
        field->signature.prepend(word);
    }
    return field;
}

std::unique_ptr<FieldInfo> Builder::makeFieldInfo(const pb::FieldDescriptorProto &proto,
                                                  const Decl *parent) const
{
    using F = pb::FieldDescriptorProto;
    auto info = std::make_unique<FieldInfo>();
    info->scalar = scalarName(proto);
    info->repeated = proto.label() == F::LABEL_REPEATED;
    info->jsonName = proto.has_json_name() ? toQString(proto.json_name()) : toJsonName(proto.name());

    if (proto.has_default_value()) {
        info->hasDefault = true;
        std::string value = proto.default_value();
        if (proto.type() == F::TYPE_BYTES)
            absl::CUnescape(proto.default_value(), &value);
        info->defaultValue = QByteArray::fromStdString(value);
    }

    if (info->repeated)
        return info;
    const QString presence = feature("field_presence", proto.options().uninterpreted_option());
    info->required = proto.label() == F::LABEL_REQUIRED || presence == u"LEGACY_REQUIRED";
    // A field that holds a message does as well, which is not known until it is found.
    info->tracksPresence = (parent && parent->kind != DeclKind::Message) // a oneof, an extension
        || proto.proto3_optional() || info->required
        || (presence.isEmpty() ? m_tracksPresence : presence != u"IMPLICIT");
    return info;
}

} // namespace

std::unique_ptr<SourceFile> parseProto(const QString &path, QByteArray bytes)
{
    // It warns on the standard error about every file that does not state its syntax.
    static const bool quiet = [] {
        absl::SetMinLogLevel(absl::LogSeverityAtLeast::kError);
        return true;
    }();
    Q_UNUSED(quiet)

    auto file = std::make_unique<SourceFile>();
    file->path = path;
    file->displayPath = path;
    file->text = SourceText(std::move(bytes));

    pb::FileDescriptorProto proto;
    proto.set_name(path.toStdString());
    NoteCollector errors(path, file->notes);
    const bool clean = parseInto(file->text.bytes(), &errors, proto);
    if (!proto.has_source_code_info()) {
        NoErrors ignored;
        proto.Clear();
        proto.set_name(path.toStdString());
        parseInto(withoutSyntaxLine(file->text), &ignored, proto);
    }

    Builder(*file, proto).build();

    if (!clean) {
        // Recovering from a mistake, the parser can skip more than the mistake.
        file->notes.push_back({path, 0, tr("This file has errors, so some of its "
                                           "declarations may be missing.")});
    }
    return file;
}

} // namespace protoexplorer
