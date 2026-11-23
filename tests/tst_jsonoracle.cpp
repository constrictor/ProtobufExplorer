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

// Holds the overlay up against protobuf itself: what its JSON parser accepts, what it then
// has in the message, and what it prints when told to print the defaults too.

// Before Qt's: the two disagree about words like `signals` and `emit`.
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/util/message_differencer.h>

#include <google/protobuf/any.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/field_mask.pb.h>
#include <google/protobuf/struct.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/wrappers.pb.h>

#include "JsonTree.h"
#include "TestFiles.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <random>

using namespace protoexplorer;
namespace gpb = google::protobuf;

namespace {

const QByteArray kImports =
    "import \"google/protobuf/any.proto\";\n"
    "import \"google/protobuf/duration.proto\";\n"
    "import \"google/protobuf/empty.proto\";\n"
    "import \"google/protobuf/field_mask.proto\";\n"
    "import \"google/protobuf/struct.proto\";\n"
    "import \"google/protobuf/timestamp.proto\";\n"
    "import \"google/protobuf/wrappers.proto\";\n";

const QByteArray kFields =
    "  optional double f_double = 1;\n"
    "  optional float f_float = 2;\n"
    "  optional int32 f_int32 = 3;\n"
    "  optional int64 f_int64 = 4;\n"
    "  optional uint32 f_uint32 = 5;\n"
    "  optional uint64 f_uint64 = 6;\n"
    "  optional sint32 f_sint32 = 7;\n"
    "  optional sint64 f_sint64 = 8;\n"
    "  optional fixed32 f_fixed32 = 9;\n"
    "  optional fixed64 f_fixed64 = 10;\n"
    "  optional sfixed32 f_sfixed32 = 11;\n"
    "  optional sfixed64 f_sfixed64 = 12;\n"
    "  optional bool f_bool = 13;\n"
    "  optional string f_string = 14;\n"
    "  optional bytes f_bytes = 15;\n"
    "  optional Status f_enum = 16;\n"
    "  optional Inner f_message = 17;\n"
    "  repeated int32 r_int32 = 18;\n"
    "  repeated string r_string = 19;\n"
    "  repeated Status r_enum = 20;\n"
    "  repeated Inner r_message = 21;\n"
    "  map<int32, string> m_int32 = 22;\n"
    "  map<bool, int64> m_bool = 23;\n"
    "  map<string, Inner> m_message = 24;\n"
    "  map<uint64, Status> m_enum = 25;\n"
    "  oneof pick { int32 o_int32 = 26; string o_string = 27; Inner o_message = 28; }\n"
    "  optional google.protobuf.Timestamp w_time = 30;\n"
    "  optional google.protobuf.Duration w_duration = 31;\n"
    "  optional google.protobuf.FieldMask w_mask = 32;\n"
    "  optional google.protobuf.Struct w_struct = 33;\n"
    "  optional google.protobuf.Value w_value = 34;\n"
    "  optional google.protobuf.ListValue w_list = 35;\n"
    "  optional google.protobuf.NullValue w_null = 36;\n"
    "  optional google.protobuf.Int64Value w_int64 = 37;\n"
    "  optional google.protobuf.StringValue w_string = 38;\n"
    "  optional google.protobuf.BytesValue w_bytes = 39;\n"
    "  optional google.protobuf.BoolValue w_bool = 40;\n"
    "  optional google.protobuf.Empty w_empty = 41;\n"
    "  optional google.protobuf.Any w_any = 42;\n"
    "  repeated google.protobuf.Value rw_value = 43;\n"
    "  repeated google.protobuf.Timestamp rw_time = 44;\n"
    "  map<string, google.protobuf.Value> mw_value = 45;\n"
    "  optional int32 f_named = 46 [json_name = \"chosenName\"];\n"
    "  optional google.protobuf.DoubleValue w_double = 47;\n"
    "  repeated google.protobuf.NullValue rw_null = 48;\n"
    "  optional All f_again = 49;\n"
    "  repeated double r_double = 50;\n"
    "  oneof other { google.protobuf.Value ow_value = 51; google.protobuf.NullValue ow_null = 52; }\n";

// The same fields every way there is to say whether they track presence.
QByteArray schemaText(const QString &name)
{
    const QByteArray body = kImports + "message All {\n" + kFields + "}\n"
        "message Inner { optional int32 id = 1; optional string long_name = 2; }\n"
        "enum Status { UNKNOWN = 0; PAID = 2; GONE = -1; }\n";
    QByteArray implicit = body;
    implicit.replace("  optional ", "  ").replace("{ optional ", "{ ").replace("; optional ", "; ");
    QByteArray editions = implicit;
    if (name == u"proto3")
        return "syntax = \"proto3\";\npackage t;\n" + implicit;
    if (name == u"proto3 optional")
        return "syntax = \"proto3\";\npackage t;\n" + body;
    if (name == u"proto2") {
        return "syntax = \"proto2\";\npackage t;\n" + body
            + "message Two { optional int32 a = 1 [default = 5]; optional string b = 2 [default = \"x\"];\n"
              "  optional Status c = 3 [default = PAID]; optional group Grp = 4 { optional int32 x = 1; }\n"
              "  extensions 100 to 200; }\n"
              "message Needs { required int32 a = 1; optional int32 b = 2; }\n"
              "extend Two { optional int32 ext = 100; repeated Inner exts = 101; }\n"
              "message Holder { extend Two { optional string held = 102; } }\n";
    }
    if (name == u"edition")
        return "edition = \"2023\";\npackage t;\n" + editions.replace("int32 f_int32 = 3;",
            "int32 f_int32 = 3 [features.field_presence = IMPLICIT];");
    if (name == u"edition implicit") {
        return "edition = \"2023\";\npackage t;\noption features.field_presence = IMPLICIT;\n"
            + editions.replace("int64 f_int64 = 4;", "int64 f_int64 = 4 [features.field_presence = EXPLICIT];")
                  .replace("string f_string = 14;", "string f_string = 14 [features = { field_presence: EXPLICIT }];");
    }
    qFatal("no such schema");
}

// The same schema twice: protobuf's, and ours.
struct Both
{
    explicit Both(const QString &name)
    {
        // All in the one pool, or an Any would not find what is in it.
        for (const gpb::Descriptor *type :
             {gpb::Any::descriptor(), gpb::Duration::descriptor(), gpb::Empty::descriptor(),
              gpb::FieldMask::descriptor(), gpb::Struct::descriptor(), gpb::Timestamp::descriptor(),
              gpb::Int32Value::descriptor()}) {
            gpb::FileDescriptorProto file;
            type->file()->CopyTo(&file);
            if (!pool.BuildFile(file))
                qFatal("cannot build %s", file.name().c_str());
        }

        const QByteArray text = schemaText(name);
        gpb::io::ArrayInputStream stream(text.constData(), int(text.size()));
        gpb::io::Tokenizer tokenizer(&stream, nullptr);
        gpb::compiler::Parser parser;
        gpb::FileDescriptorProto proto;
        proto.set_name("a.proto");
        if (!parser.Parse(&tokenizer, &proto) || !pool.BuildFile(proto))
            qFatal("bad schema: %s", qPrintable(name));
        files.add(QStringLiteral("a.proto"), text);
        index = std::make_shared<SchemaIndex>(files.load(QStringLiteral("a.proto")));
        if (!index->schema().notes.empty())
            qFatal("%s", qPrintable(allNotes(index->schema())));
    }

    std::unique_ptr<gpb::Message> theirs(const QByteArray &type, const QByteArray &json) const
    {
        const auto *descriptor = pool.FindMessageTypeByName(type.toStdString());
        std::unique_ptr<gpb::Message> message(factory.GetPrototype(descriptor)->New());
        gpb::json::ParseOptions options;
        options.allow_legacy_nonconformant_behavior = false;
        const auto status = gpb::json::JsonStringToMessage(json.toStdString(), message.get(), options);
        // It leaves this to whoever asked.
        return status.ok() && message->IsInitialized() ? std::move(message) : nullptr;
    }

    JsonTree ours(const QByteArray &type, const QByteArray &json) const
    {
        JsonDocument document = parseJson(QString::fromUtf8(json));
        if (document.error)
            qFatal("%s: %s", json.constData(), qPrintable(document.error->text));
        return JsonTree(index, *index->message(QString::fromUtf8(type)), std::move(document));
    }

    TestFiles files;
    gpb::DescriptorPool pool;
    mutable gpb::DynamicMessageFactory factory;
    std::shared_ptr<const SchemaIndex> index;
};

QString problems(const JsonTree &tree)
{
    QStringList result;
    for (const auto *node : tree.nodes(JsonTree::State::Problem))
        result << tree.pathTo(*node) + QStringLiteral(": ") + node->problem;
    return result.join(u'\n');
}

const JsonTree::Node *fieldNode(const JsonTree::Node &in, const QString &name)
{
    for (const auto &child : in.children) {
        if (child->isField() && child->name == name)
            return child.get();
        if (child->form == JsonTree::Form::Oneof) {
            if (const auto *found = fieldNode(*child, name))
                return found;
        }
    }
    return nullptr;
}

std::string print(const gpb::Message &message, bool defaults, bool ownNames = false,
                  bool numbers = false)
{
    gpb::json::PrintOptions options;
    options.always_print_fields_with_no_presence = defaults;
    options.preserve_proto_field_names = ownNames;
    options.always_print_enums_as_ints = numbers;
    std::string out;
    const auto status = gpb::json::MessageToJsonString(message, &out, options);
    if (!status.ok())
        qFatal("cannot print: %s", std::string(status.message()).c_str());
    return out;
}

// Every field of it is in the state that protobuf has it in, all the way down. Returns
// what is not.
QString disagreement(const JsonTree &tree, const JsonTree::Node &node, const gpb::Message &message)
{
    using State = JsonTree::State;
    using F = gpb::FieldDescriptor;
    const gpb::Descriptor *descriptor = message.GetDescriptor();
    const gpb::Reflection *reflection = message.GetReflection();
    for (int i = 0; i < descriptor->field_count(); ++i) {
        const F *field = descriptor->field(i);
        const QString name = QString::fromStdString(std::string(field->name()));
        const auto *at = fieldNode(node, name);
        if (!at)
            return name + QStringLiteral(" is not there");
        const auto wrong = [&](const QString &what) {
            return QStringLiteral("%1: %2, ours %3 \"%4\"").arg(tree.pathTo(*at), what)
                .arg(int(at->state)).arg(tree.valueText(*at));
        };

        if (field->is_repeated()) {
            const int size = reflection->FieldSize(message, field);
            if (size > 0 && at->state != State::Set)
                return wrong(QStringLiteral("has %1").arg(size));
            if (size != int(at->children.size()))
                return wrong(QStringLiteral("has %1 items").arg(size));
            if (!field->is_map() && at->form == JsonTree::Form::List
                && field->cpp_type() == F::CPPTYPE_MESSAGE) {
                for (int k = 0; k < size; ++k) {
                    if (at->children[size_t(k)]->form != JsonTree::Form::Message)
                        continue;
                    const QString inner = disagreement(tree, *at->children[size_t(k)],
                                                       reflection->GetRepeatedMessage(message, field, k));
                    if (!inner.isEmpty())
                        return inner;
                }
            }
            continue;
        }

        const bool has = reflection->HasField(message, field);
        if (field->has_presence()) {
            if (at->state != (has ? State::Set : State::NotSet))
                return wrong(has ? QStringLiteral("set") : QStringLiteral("not set"));
        } else if (at->state != State::Set && (at->state != State::Default || has)) {
            return wrong(has ? QStringLiteral("has a value") : QStringLiteral("has no presence"));
        }

        if (field->cpp_type() == F::CPPTYPE_MESSAGE) {
            if (has && at->form == JsonTree::Form::Message) {
                const QString inner = disagreement(tree, *at, reflection->GetMessage(message, field));
                if (!inner.isEmpty())
                    return inner;
            }
            continue;
        }

        // What reading the field gives is what there is to see, set or not.
        QString shown = tree.valueText(*at);
        shown.remove(QStringLiteral("null → "));
        if (shown.startsWith(u"not set ("))
            shown = shown.sliced(9, shown.size() - 10);
        const auto number = [&](auto value) { return QString::number(value); };
        const auto same = [&](double value, double read) {
            return value == read || (std::isnan(value) && std::isnan(read));
        };
        const auto asDouble = [&shown] {
            QString s = shown.toLower();
            return s.replace(QStringLiteral("infinity"), QStringLiteral("inf")).toDouble();
        };
        switch (field->cpp_type()) {
        case F::CPPTYPE_INT32:
            if (shown != number(reflection->GetInt32(message, field)))
                return wrong(number(reflection->GetInt32(message, field)));
            break;
        case F::CPPTYPE_INT64:
            if (shown != number(qlonglong(reflection->GetInt64(message, field))))
                return wrong(number(qlonglong(reflection->GetInt64(message, field))));
            break;
        case F::CPPTYPE_UINT32:
            if (shown != number(reflection->GetUInt32(message, field)))
                return wrong(number(reflection->GetUInt32(message, field)));
            break;
        case F::CPPTYPE_UINT64:
            if (shown != number(qulonglong(reflection->GetUInt64(message, field))))
                return wrong(number(qulonglong(reflection->GetUInt64(message, field))));
            break;
        case F::CPPTYPE_DOUBLE:
            if (!same(reflection->GetDouble(message, field), asDouble()))
                return wrong(number(reflection->GetDouble(message, field)));
            break;
        case F::CPPTYPE_FLOAT:
            if (!same(reflection->GetFloat(message, field), float(asDouble())))
                return wrong(number(double(reflection->GetFloat(message, field))));
            break;
        case F::CPPTYPE_BOOL:
            if (shown != (reflection->GetBool(message, field) ? u"true" : u"false"))
                return wrong(QStringLiteral("a bool"));
            break;
        case F::CPPTYPE_ENUM: {
            const int value = reflection->GetEnumValue(message, field);
            const auto *known = field->enum_type()->FindValueByNumber(value);
            const QString text = known ? QString::fromStdString(std::string(known->name()))
                                       : QString::number(value);
            if (!shown.startsWith(text))
                return wrong(text);
            break;
        }
        case F::CPPTYPE_STRING: {
            const QByteArray bytes = QByteArray::fromStdString(reflection->GetString(message, field));
            const QString copied = tree.copyText(*at);
            if (field->type() == F::TYPE_BYTES) {
                QByteArray base64 = copied.toLatin1();
                base64.replace('-', '+').replace('_', '/');
                if (QByteArray::fromBase64(base64) != bytes)
                    return wrong(QString::fromLatin1(bytes.toHex()));
            } else if (copied != QString::fromUtf8(bytes)) {
                return wrong(QString::fromUtf8(bytes));
            }
            break;
        }
        default:
            break;
        }
    }
    return {};
}

// A message with something or nothing in every field.
void fillAtRandom(gpb::Message &message, std::mt19937 &random, int depth)
{
    using F = gpb::FieldDescriptor;
    const gpb::Descriptor *descriptor = message.GetDescriptor();
    const gpb::Reflection *reflection = message.GetReflection();
    const std::string type(descriptor->full_name());
    const auto below = [&random](int n) { return int(random() % unsigned(n)); };

    if (type == "google.protobuf.Timestamp" || type == "google.protobuf.Duration") {
        reflection->SetInt64(&message, descriptor->FindFieldByName("seconds"), below(1'000'000));
        reflection->SetInt32(&message, descriptor->FindFieldByName("nanos"), below(3) ? 0 : 5'000'000);
        return;
    }
    if (type == "google.protobuf.FieldMask") {
        reflection->AddString(&message, descriptor->field(0), "a_b.c");
        return;
    }
    if (type == "google.protobuf.Any") {
        reflection->SetString(&message, descriptor->FindFieldByName("type_url"),
                              below(2) ? "type.googleapis.com/t.Inner"
                                       : "type.googleapis.com/google.protobuf.Duration");
        return;
    }
    if (type == "google.protobuf.Value") {
        if (below(2) || depth <= 0)
            reflection->SetString(&message, descriptor->FindFieldByName("string_value"), "v");
        else if (below(2))
            reflection->SetEnumValue(&message, descriptor->FindFieldByName("null_value"), 0);
        else
            fillAtRandom(*reflection->MutableMessage(&message, descriptor->FindFieldByName("struct_value")),
                         random, depth - 1);
        return;
    }

    const auto set = [&](const F *field, bool add) {
        const auto put = [&](auto setter, auto adder, auto value) {
            if (add)
                (reflection->*adder)(&message, field, value);
            else
                (reflection->*setter)(&message, field, value);
        };
        using R = gpb::Reflection;
        switch (field->cpp_type()) {
        case F::CPPTYPE_INT32:
            put(&R::SetInt32, &R::AddInt32, below(3) ? int32_t(random()) : 0);
            break;
        case F::CPPTYPE_INT64:
            put(&R::SetInt64, &R::AddInt64, below(3) ? int64_t(uint64_t(random()) << 32 | random()) : 0);
            break;
        case F::CPPTYPE_UINT32:
            put(&R::SetUInt32, &R::AddUInt32, below(3) ? uint32_t(random()) : 0U);
            break;
        case F::CPPTYPE_UINT64:
            put(&R::SetUInt64, &R::AddUInt64, below(3) ? uint64_t(random()) << 32 | random() : uint64_t(0));
            break;
        case F::CPPTYPE_DOUBLE: {
            const double values[] = {0, 1.5, -1e300, 5e-324, INFINITY, NAN, 1.0 / 3};
            put(&R::SetDouble, &R::AddDouble, values[below(7)]);
            break;
        }
        case F::CPPTYPE_FLOAT: {
            const float values[] = {0, 1.5F, -3.4028235e38F, -INFINITY, NAN, 1.0F / 3};
            put(&R::SetFloat, &R::AddFloat, values[below(6)]);
            break;
        }
        case F::CPPTYPE_BOOL:
            put(&R::SetBool, &R::AddBool, below(2) == 1);
            break;
        case F::CPPTYPE_ENUM: {
            // One with a name: the others are of no use to a closed enum.
            const auto *value = field->enum_type()->value(below(field->enum_type()->value_count()));
            put(&R::SetEnumValue, &R::AddEnumValue, value->number());
            break;
        }
        case F::CPPTYPE_STRING: {
            const char *texts[] = {"", "plain", "quo\"te\\ \n\t\x01", "ґудзик 🙂", "<&>"};
            std::string text = texts[below(5)];
            if (field->type() == F::TYPE_BYTES)
                text.assign("\0\xff\xfe", size_t(below(4)));
            if (add)
                reflection->AddString(&message, field, text);
            else
                reflection->SetString(&message, field, text);
            break;
        }
        case F::CPPTYPE_MESSAGE:
            fillAtRandom(add ? *reflection->AddMessage(&message, field)
                             : *reflection->MutableMessage(&message, field), random, depth - 1);
            break;
        }
    };

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const F *field = descriptor->field(i);
        const bool required = field->is_required();
        if (!required && (below(2) || (depth <= 0 && field->cpp_type() == F::CPPTYPE_MESSAGE)))
            continue;
        if (field->is_map()) {
            const F *key = field->message_type()->map_key();
            for (int k = 0, n = 1 + below(2); k < n; ++k) {
                gpb::Message *entry = reflection->AddMessage(&message, field);
                fillAtRandom(*entry, random, depth - 1);
                // All different.
                if (key->cpp_type() == F::CPPTYPE_STRING)
                    entry->GetReflection()->SetString(entry, key, "k" + std::to_string(k));
                else if (key->cpp_type() == F::CPPTYPE_BOOL)
                    entry->GetReflection()->SetBool(entry, key, k == 1);
                else if (key->cpp_type() == F::CPPTYPE_INT32)
                    entry->GetReflection()->SetInt32(entry, key, k);
                else
                    entry->GetReflection()->SetUInt64(entry, key, uint64_t(k));
                const F *value = field->message_type()->map_value();
                if (value->cpp_type() == F::CPPTYPE_MESSAGE && !entry->GetReflection()->HasField(*entry, value))
                    fillAtRandom(*entry->GetReflection()->MutableMessage(entry, value), random, 0);
            }
        } else if (field->is_repeated()) {
            for (int k = 0, n = 1 + below(3); k < n; ++k)
                set(field, true);
        } else {
            set(field, false);
        }
    }
}

QJsonDocument qjson(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError)
        qFatal("%s\n%s", qPrintable(error.errorString()), json.constData());
    return document;
}

void schemas()
{
    QTest::addColumn<QString>("schema");
    for (const char *name : {"proto3", "proto3 optional", "proto2", "edition", "edition implicit"})
        QTest::newRow(name) << QString::fromLatin1(name);
}

} // namespace

class TestJsonOracle : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyValueForEveryField_data() { schemas(); }
    void everyValueForEveryField()
    {
        QFETCH(QString, schema);
        const Both both(schema);
        const QByteArrayList values{
            "null", "true", "false", "0", "-0", "1", "-1", "1.0", "1.5", "1e3", "10e-1", "1e-1",
            "15.50e1", "0.1e1", "0e99999999999", "1e-99999999999",
            "2147483647", "2147483648", "-2147483648", "-2147483649", "4294967295",
            "4294967296", "9223372036854775807", "9223372036854775808",
            "-9223372036854775808", "-9223372036854775809", "18446744073709551615",
            "18446744073709551616", "123456789012345678901234567890", "3.4028235e38",
            "3.4028236e38", "3.5e38", "-3.5e38", "1e308", "1e-400",
            "\"1\"", "\"1.0\"", "\"1e3\"", "\"-1\"", "\"\"", "\" 1\"", "\"1 \"", "\"1.5\"",
            "\"9223372036854775807\"", "\"9223372036854775808\"", "\"0x1\"", "\"1,0\"",
            "\"abc\"", "\"NaN\"", "\"Infinity\"", "\"-Infinity\"", "\"true\"", "\"1e999\"",
            "\"AAEC\"", "\"AAE=\"", "\"AAE\"", "\"AA==\"", "\"AA=\"", "\"A\"", "\"-_8=\"", "\"+/8=\"",
            "\"A===\"", "\"====\"", "\"AA=A\"", "\"A A=\"",
            "\"2024-02-29T12:30:45Z\"", "\"2024-02-29T12:30:45.123456789+05:30\"",
            "\"0001-01-01T00:00:00Z\"", "\"9999-12-31T23:59:59.999999999Z\"",
            "\"0000-01-01T00:00:00Z\"", "\"2024-02-29T12:30:45\"", "\"2024-02-29T12:30:45z\"",
            "\"2024-02-29t12:30:45Z\"", "\"2024-02-29 12:30:45Z\"", "\"2024-02-29T12:30:45.Z\"",
            "\"2024-02-29T12:30:45.1234567890Z\"", "\"2024-02-29\"", "\"24-02-29T12:30:45Z\"",
            "\"1.5s\"", "\"-0.5s\"", "\"1s\"", "\"1.s\"", "\"s\"", "\".5s\"", "\"1\"", "\"1.1234567890s\"",
            "\"315576000000s\"", "\"315576000001s\"", "\"-315576000001s\"", "\"1S\"",
            "\"fooBar,baz.qux\"", "\"foo_bar\"", "\"a b\"",
            "\"UNKNOWN\"", "\"PAID\"", "\"paid\"", "\"GONE\"", "\"2\"", "2", "7", "\"7\"", "\"NULL_VALUE\"",
            "{}", "[]", "[1]", "[\"a\"]", "[\"PAID\", 7]", "[null]", "[1.5]", "[{}]", "[[]]",
            "[\"2024-02-29T12:30:45Z\"]", "[1, \"2\", 3.0]", "[{\"id\": 1}, {\"longName\": \"x\"}]",
            "{\"a\":1}", "{\"1\":\"1\"}", "{\"1\":1}", "{\"true\":\"5\"}", "{\"true\":1,\"false\":2}",
            "{\"01\":\"x\"}", "{\"1.0\":\"x\"}", "{\"-1\":\"x\"}", "{\"4294967296\":\"x\"}",
            "{\"18446744073709551615\":\"PAID\"}", "{\"18446744073709551616\":\"PAID\"}",
            "{\"1\":\"x\",\"1\":\"y\"}", "{\"a\":{}}", "{\"a\":{\"id\":\"7\"}}", "{\"a\":[]}",
            "{\"id\":1}", "{\"id\":null}", "{\"id\":\"x\"}", "{\"longName\":\"x\"}", "{\"long_name\":\"x\"}",
            "{\"longName\":\"x\",\"long_name\":\"y\"}", "{\"id\":1,\"id\":1}", "{\"nope\":1}", "{\"nope\":null}",
            "{\"fInt32\":1,\"fAgain\":{\"rInt32\":[1,2]}}",
            "{\"@type\":\"type.googleapis.com/t.Inner\",\"id\":1}",
            "{\"id\":1,\"@type\":\"type.googleapis.com/t.Inner\"}",
            "{\"@type\":\"type.googleapis.com/t.Inner\",\"nope\":1}",
            "{\"@type\":\"type.googleapis.com/t.Gone\"}", "{\"@type\":1}",
            "{\"@type\":\"type.googleapis.com/google.protobuf.Duration\",\"value\":\"1s\"}",
            "{\"@type\":\"type.googleapis.com/google.protobuf.Duration\",\"value\":\"x\"}",
            "{\"@type\":\"type.googleapis.com/google.protobuf.Duration\",\"other\":1}",
        };
        // Where protobuf's C++ parser is laxer than the specification and its own conformance
        // tests, and we are not.
        const auto laxer = [&schema](const gpb::FieldDescriptor *field, const QByteArray &value) {
            // And where it is stricter than its own printer, which prints an empty one so.
            if (field->json_name() == "wAny" && value == "{}")
                return true;
            // What a number in quotes may look like, and how close to whole it has to be.
            if (value == "\"0x1\"" || value == "\" 1\"" || value == "\"1 \"" || value == "\"1e999\""
                || value == "1e-99999999999" || value == "1e-400") {
                return true;
            }
            // Null where there has to be a value.
            if (field->is_map() && value.contains(":null"))
                return true;
            // In C++ an open enum is closed to the fields of a proto2 file.
            return schema == u"proto2" && field->enum_type()
                && field->enum_type()->full_name() == "google.protobuf.NullValue";
        };

        const auto *descriptor = both.pool.FindMessageTypeByName("t.All");
        int accepted = 0;
        int refused = 0;
        int unnamed = 0;
        QStringList different;
        for (int i = 0; i < descriptor->field_count(); ++i) {
            const QByteArray field = QByteArray::fromStdString(std::string(descriptor->field(i)->json_name()));
            for (const QByteArray &value : values) {
                if (laxer(descriptor->field(i), value))
                    continue;
                const QByteArray json = "{\"" + field + "\": " + value + "}";
                const auto theirs = both.theirs("t.All", json);
                const JsonTree ours = both.ours("t.All", json);
                const QString said = problems(ours);
                // In C++ a closed enum takes a number it has no name for, and then has no
                // value. Other languages refuse it, and to us it is a problem either way.
                if (schema == u"proto2" && theirs && said.contains(QStringLiteral("no such value in Status"))) {
                    QVERIFY(!field.startsWith("fEnum") || !theirs->GetReflection()->HasField(*theirs, descriptor->field(i)));
                    ++unnamed;
                    continue;
                }
                if (bool(theirs) != said.isEmpty()) {
                    different << QString::fromUtf8(json)
                            + (theirs ? QStringLiteral(" taken, but we say ") + said
                                      : QStringLiteral(" refused, but not by us"));
                }
                if (!theirs) {
                    ++refused;
                    continue;
                }
                ++accepted;
                const QString wrong = disagreement(ours, *ours.roots().front(), *theirs);
                if (!wrong.isEmpty())
                    different << QString::fromUtf8(json) + QStringLiteral(" disagrees: ") + wrong;
            }
        }
        QVERIFY2(different.isEmpty(), qPrintable(u'\n' + different.join(u'\n')));
        QVERIFY(accepted > 300);
        QVERIFY(refused > 3000);
        QCOMPARE(unnamed > 10, schema == u"proto2");
    }

    void wholeMessages_data() { schemas(); }
    void wholeMessages()
    {
        QFETCH(QString, schema);
        const Both both(schema);
        const auto *descriptor = both.pool.FindMessageTypeByName("t.All");
        std::mt19937 random(12345);
        for (int round = 0; round < 400; ++round) {
            std::unique_ptr<gpb::Message> message(both.factory.GetPrototype(descriptor)->New());
            fillAtRandom(*message, random, 3);
            const bool ownNames = round % 3 == 1;
            const std::string json = print(*message, round % 4 == 3, ownNames, round % 5 == 4);
            const JsonTree tree = both.ours("t.All", QByteArray::fromStdString(json));
            QVERIFY2(problems(tree).isEmpty(), qPrintable(problems(tree) + u'\n' + QString::fromStdString(json)));
            const QString wrong = disagreement(tree, *tree.roots().front(), *message);
            QVERIFY2(wrong.isEmpty(), qPrintable(wrong + u'\n' + QString::fromStdString(json)));

            // What we fill in is what protobuf prints when told to.
            if (round % 5 == 4)
                continue; // enums as numbers, which are only the JSON's to say
            const QByteArray filled = tree.withDefaults().toUtf8();
            const QByteArray printed = QByteArray::fromStdString(print(*message, true, ownNames));
            if (qjson(filled) != qjson(printed)) {
                qWarning().noquote() << "\nours:" << qjson(filled).toJson() << "\ntheirs:" << qjson(printed).toJson();
                QFAIL("differ");
            }
            const auto again = both.theirs("t.All", filled);
            QVERIFY(again);
            QCOMPARE(print(*again, false), print(*message, false));
        }
    }

    void twoOfThem()
    {
        const Both both(QStringLiteral("proto3"));
        const QByteArrayList documents{
            "{\"oInt32\": 1, \"oString\": \"x\"}",
            "{\"oInt32\": null, \"oString\": \"x\"}",
            "{\"oInt32\": 1, \"oString\": null}",
            "{\"owValue\": null, \"owNull\": null}",
            "{\"owValue\": null}",
            "{\"owNull\": null}",
            "{\"fInt32\": 1, \"f_int32\": 1}",
            "{\"fInt32\": null, \"f_int32\": 1}",
            "{\"chosenName\": 1}",
            "{\"f_named\": 1}",
            "{\"chosenName\": 1, \"f_named\": 1}",
            "{\"oMessage\": {}, \"oInt32\": 0}",
            "{\"oMessage\": {}, \"owNull\": null}",
        };
        for (const QByteArray &json : documents) {
            const auto theirs = both.theirs("t.All", json);
            const JsonTree ours = both.ours("t.All", json);
            QVERIFY2(bool(theirs) == problems(ours).isEmpty(), json.constData());
            if (theirs)
                QCOMPARE(disagreement(ours, *ours.roots().front(), *theirs), QString());
        }
    }

    void proto2()
    {
        const Both both(QStringLiteral("proto2"));
        const std::pair<QByteArray, QByteArray> documents[] = {
            {"t.Two", "{}"},
            {"t.Two", "{\"a\": 5}"},
            {"t.Two", "{\"a\": null, \"b\": \"\"}"},
            {"t.Two", "{\"grp\": {\"x\": 1}}"},
            {"t.Two", "{\"[t.ext]\": 1}"},
            {"t.Two", "{\"[t.ext]\": \"x\"}"},
            {"t.Two", "{\"[t.exts]\": [{\"id\": 1}, {}]}"},
            {"t.Two", "{\"[t.exts]\": {}}"},
            {"t.Two", "{\"[t.Holder.held]\": \"x\"}"},
            {"t.Two", "{\"[t.held]\": \"x\"}"},
            {"t.Two", "{\"[t.gone]\": 1}"},
            {"t.Two", "{\"ext\": 1}"},
            {"t.All", "{\"[t.ext]\": 1}"},
            {"t.Two", "{\"[t.ext]\": 1, \"[t.ext]\": 1}"},
            {"t.Needs", "{}"},
            {"t.Needs", "{\"a\": null}"},
            {"t.Needs", "{\"a\": 0}"},
            {"t.Needs", "{\"b\": 0}"},
        };
        for (const auto &[type, json] : documents) {
            const auto theirs = both.theirs(type, json);
            const JsonTree ours = both.ours(type, json);
            QVERIFY2(bool(theirs) == problems(ours).isEmpty(),
                     (type + ' ' + json + ' ' + problems(ours).toUtf8()).constData());
            if (theirs)
                QCOMPARE(disagreement(ours, *ours.roots().front(), *theirs), QString());
        }
    }
};

QTEST_GUILESS_MAIN(TestJsonOracle)
#include "tst_jsonoracle.moc"
