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
#include "TestFiles.h"

using namespace protoexplorer;

class TestJsonMatch : public QObject
{
    Q_OBJECT

    static QStringList ranked(const SchemaIndex &index, const QString &json)
    {
        QStringList result;
        for (const MessageMatch &match : rankMessages(index, topLevelKeys(parseJson(json))))
            result << QStringLiteral("%1 %2").arg(match.message->name).arg(match.known);
        return result;
    }

    static SchemaIndex indexOf(const TestFiles &files) { return SchemaIndex(files.load()); }

private slots:
    void keys()
    {
        const auto names = [](const char *json) {
            QStringList result;
            for (const JsonKey &key : topLevelKeys(parseJson(QString::fromUtf8(json))))
                result << key.name;
            return result.join(u' ');
        };
        QCOMPARE(names("{\"b\":1,\"a\":{\"c\":1},\"b\":2}"), QStringLiteral("b a"));
        QCOMPARE(names("[{\"a\":1},{\"b\":1,\"a\":2},7,[]]"), QStringLiteral("a b"));
        QCOMPARE(names("{\"a\":1}\n{\"b\":1}"), QStringLiteral("a b"));
        QCOMPARE(names("7"), QString());
        QCOMPARE(names(""), QString());
    }

    void mostKeysWins()
    {
        TestFiles files;
        files.proto("a.proto", "message A { int32 id = 1; }\n"
                               "message B { int32 id = 1; string long_name = 2; int32 x = 3; }\n"
                               "message C { int32 other = 1; }\nmessage D { oneof o { int32 x = 1; int32 y = 2; } }");
        const SchemaIndex index = indexOf(files);
        QCOMPARE(ranked(index, QStringLiteral("{\"id\":1,\"longName\":\"x\",\"what\":1}")),
                 QStringList({"B 2", "A 1", "C 0", "D 0"}));
        // By its own name as well.
        QCOMPARE(ranked(index, QStringLiteral("{\"long_name\":\"x\"}")).first(), QStringLiteral("B 1"));
        // Alike in that, the one that has the least left over.
        QCOMPARE(ranked(index, QStringLiteral("{\"id\":1}")).first(), QStringLiteral("A 1"));
        QCOMPARE(ranked(index, QStringLiteral("{\"x\":1}")).first(), QStringLiteral("D 1"));
    }

    void whatSortOfValueItIsDecides()
    {
        TestFiles files;
        files.proto("a.proto",
            "enum E { X = 0; } message M {}\n"
            "message Num { int32 v = 1; } message Str { string v = 1; } message Flag { bool v = 1; }\n"
            "message List { repeated int32 v = 1; } message Obj { M v = 1; } message Map { map<string, string> v = 1; }\n");
        const SchemaIndex index = indexOf(files);
        const auto first = [&](const char *value) {
            return ranked(index, QStringLiteral("{\"v\": %1}").arg(QString::fromUtf8(value))).first();
        };
        QCOMPARE(first("1"), QStringLiteral("Num 1"));
        QCOMPARE(first("true"), QStringLiteral("Flag 1"));
        QCOMPARE(first("[]"), QStringLiteral("List 1"));
        QCOMPARE(first("{}"), QStringLiteral("Obj 1"));
    }

    void nothingToGoBy()
    {
        TestFiles files;
        files.proto("a.proto", "message Big { int32 a = 1; int32 b = 2; }\nmessage Small {}")
            .proto("b.proto", "import \"google/protobuf/timestamp.proto\"; message Z { google.protobuf.Timestamp t = 1; }");
        const SchemaIndex index = indexOf(files);
        // Those that were opened as they come, and the special ones last.
        QCOMPARE(ranked(index, QStringLiteral("{}")), QStringList({"Big 0", "Small 0", "Z 0", "Timestamp 0"}));
        // Which nobody's JSON has the fields of.
        QCOMPARE(ranked(index, QStringLiteral("{\"seconds\": 1}")).last(), QStringLiteral("Timestamp 1"));
    }

    void counts()
    {
        TestFiles files;
        files.add("a.proto", "syntax = \"proto2\"; package p;\n"
                             "message A { oneof o { int32 in_oneof = 1; } extensions 10 to 20; }\n"
                             "extend A { optional int32 ext = 10; }");
        const SchemaIndex index = indexOf(files);
        const auto keys = topLevelKeys(parseJson(QStringLiteral("{\"inOneof\":1,\"[p.ext]\":1,\"ext\":1,\"o\":1}")));
        QCOMPARE(knownKeys(index, *index.message(QStringLiteral("p.A")), keys), 2);
    }

    void index()
    {
        TestFiles files;
        files.proto("a.proto", "package a.b; message M { message In {} } message X {}")
            .proto("c.proto", "package c; message M {} message XX {}")
            .proto("copy/c.proto", "package c; message M {}");
        const SchemaIndex index = indexOf(files);
        const auto named = [&](const char *name) {
            QStringList result;
            for (const Decl *message : index.messagesNamed(QString::fromLatin1(name)))
                result << message->fullName;
            return result.join(u' ');
        };
        QCOMPARE(named("a.b.M"), QStringLiteral("a.b.M"));
        QCOMPARE(named(".a.b.M"), QStringLiteral("a.b.M"));
        QCOMPARE(named("b.M"), QStringLiteral("a.b.M"));
        QCOMPARE(named("M"), QStringLiteral("a.b.M c.M")); // the copy is the same one
        QCOMPARE(named("M.In"), QStringLiteral("a.b.M.In"));
        QCOMPARE(named("X"), QStringLiteral("a.b.X"));
        QCOMPARE(named("nope"), QString());
        QCOMPARE(index.messages().size(), size_t(6));
    }

    void fieldNames()
    {
        TestFiles files;
        files.proto("a.proto", "message A { int32 foo_bar = 1; int32 fooBar2 = 2 [json_name = \"foo_bar\"];\n"
                               "  int32 _x__y_ = 3; int32 CAPS_name = 4; }");
        const SchemaIndex index = indexOf(files);
        const Decl &a = *index.message(QStringLiteral("A"));
        const auto name = [&](const char *key) {
            const Decl *field = index.field(a, QString::fromLatin1(key));
            return field ? field->name : QString();
        };
        QCOMPARE(name("fooBar"), QStringLiteral("foo_bar"));
        QCOMPARE(name("foo_bar"), QStringLiteral("fooBar2")); // it was chosen for it
        QCOMPARE(name("fooBar2"), QStringLiteral("fooBar2"));
        QCOMPARE(name("XY"), QStringLiteral("_x__y_"));
        QCOMPARE(name("CAPSName"), QStringLiteral("CAPS_name"));
        QCOMPARE(name("capsName"), QString());
        QCOMPARE(name(""), QString());
        QCOMPARE(name("[A.foo_bar]"), QString());
    }
};

QTEST_GUILESS_MAIN(TestJsonMatch)
#include "tst_jsonmatch.moc"
