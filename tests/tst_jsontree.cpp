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

#include "JsonModel.h"
#include "JsonTree.h"
#include "TestFiles.h"

#include <QAbstractItemModelTester>

using namespace protoexplorer;
using State = JsonTree::State;
using Node = JsonTree::Node;

namespace {

// A schema, and JSON laid over a message of it.
struct Laid
{
    Laid(const QByteArray &proto, const QByteArray &json, const QString &message = QStringLiteral("A"))
    {
        files.add(QStringLiteral("a.proto"), proto.startsWith("syntax") ? proto : "syntax = \"proto3\";\n" + proto);
        index = std::make_shared<SchemaIndex>(files.load());
        JsonDocument document = parseJson(QString::fromUtf8(json));
        if (document.error)
            qFatal("%s", qPrintable(document.error->text));
        tree = std::make_shared<JsonTree>(index, *index->messagesNamed(message).at(0), std::move(document));
    }

    const Node *at(const QString &path) const
    {
        const Node *found = nullptr;
        for (const QString &name : path.split(u'/', Qt::SkipEmptyParts)) {
            const auto &nodes = found ? found->children : tree->roots();
            const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto &n) { return n->name == name; });
            if (it == nodes.end())
                qFatal("no %s", qPrintable(path));
            found = it->get();
        }
        return found;
    }
    State state(const char *path) const { return at(QString::fromLatin1(path))->state; }
    QString value(const char *path) const { return tree->valueText(*at(QString::fromUtf8(path))); }
    QString type(const char *path) const { return tree->typeText(*at(QString::fromLatin1(path))); }
    QString problem(const char *path) const { return at(QString::fromUtf8(path))->problem; }
    QString pathTo(const char *path) const { return tree->pathTo(*at(QString::fromUtf8(path))); }
    QString copied(const char *path) const { return tree->copyText(*at(QString::fromLatin1(path))); }
    QStringList names() const
    {
        QStringList result;
        for (const Node *node : tree->nodes())
            result << node->name;
        return result;
    }

    TestFiles files;
    std::shared_ptr<const SchemaIndex> index;
    std::shared_ptr<const JsonTree> tree;
};

} // namespace

class TestJsonTree : public QObject
{
    Q_OBJECT

private slots:
    void everyFieldInTheOrderOfTheSchema()
    {
        const Laid l("message A { int32 a = 1; oneof o { int32 b = 2; B c = 3; } repeated B d = 4; string e = 5; }\n"
                     "message B { int32 x = 1; B again = 2; }",
                     "{\"extra\": 1, \"d\": [{\"x\": 1}], \"c\": {}, \"a\": 5}");
        QCOMPARE(l.names(), QStringList({"A", "a", "o", "b", "c", "x", "again", "d", "[0]", "x", "again", "e", "extra"}));
        QCOMPARE(l.state("A/a"), State::Set);
        QCOMPARE(l.state("A/o"), State::Set);
        QCOMPARE(l.state("A/o/b"), State::NotSet);
        QCOMPARE(l.state("A/o/c"), State::Set);
        QCOMPARE(l.state("A/o/c/x"), State::Default);
        QCOMPARE(l.state("A/o/c/again"), State::NotSet);
        QCOMPARE(l.state("A/d"), State::Set);
        QCOMPARE(l.state("A/e"), State::Default);
        QCOMPARE(l.state("A/extra"), State::Problem);
        // What is not there ends there, or it would not end.
        QVERIFY(l.at(QStringLiteral("A/o/c/again"))->children.empty());
        // Order is that of the rows.
        for (size_t i = 0; i < l.tree->nodes().size(); ++i)
            QCOMPARE(l.tree->nodes()[i]->order, int(i));
    }

    void counted()
    {
        const Laid l("message A { int32 a = 1; oneof o { int32 b = 2; B c = 3; } repeated B d = 4; string e = 5; }\n"
                     "message B { int32 x = 1; }",
                     "{\"extra\": 1, \"d\": [{\"x\": 1}, {}, 7], \"c\": {}, \"a\": 5}");
        const auto names = [&](State state) {
            QStringList result;
            for (const Node *node : l.tree->nodes(state))
                result << node->name;
            return result.join(u' ');
        };
        // Fields, not the parts of a value, nor the oneof.
        QCOMPARE(names(State::Set), QStringLiteral("a c d x"));
        QCOMPARE(names(State::Default), QStringLiteral("x x e"));
        QCOMPARE(names(State::NotSet), QStringLiteral("b"));
        QCOMPARE(names(State::Problem), QStringLiteral("[2] extra"));
    }

    void whatIsShown()
    {
        const Laid l("message A { int32 i = 1; int64 l = 2; double d = 3; bool t = 4; string s = 5; bytes b = 6;\n"
                     " E e = 7; E n = 8; E u = 9; repeated int32 r = 10; map<string, int32> m = 11; B msg = 12; C none = 13;\n"
                     " bytes text = 14; }\n"
                     "enum E { ZERO = 0; ONE = 1; } message B { int32 x = 1; } message C {}",
                     "{\"i\": 1e3, \"l\": \"9007199254740993\", \"d\": \"Infinity\", \"t\": true, \"s\": \"a\\\"\\n\","
                     "\"b\": \"AP8=\", \"e\": \"ONE\", \"n\": 1, \"u\": 7, \"r\": [1], \"m\": {\"k\": 1, \"l\": 2}, \"msg\": {}, \"none\": {},"
                     "\"text\": \"aGVsbG8=\"}");
        QCOMPARE(l.value("A/i"), QStringLiteral("1000"));
        QCOMPARE(l.value("A/l"), QStringLiteral("9007199254740993"));
        QCOMPARE(l.value("A/d"), QStringLiteral("Infinity"));
        QCOMPARE(l.value("A/t"), QStringLiteral("true"));
        QCOMPARE(l.value("A/s"), QStringLiteral("\"a\\\"\\n\""));
        QCOMPARE(l.value("A/b"), QStringLiteral("2 bytes 00 ff"));
        QCOMPARE(l.value("A/text"), QStringLiteral("5 bytes \"hello\""));
        QCOMPARE(l.value("A/e"), QStringLiteral("ONE"));
        QCOMPARE(l.value("A/n"), QStringLiteral("ONE (1)"));
        QCOMPARE(l.value("A/u"), QStringLiteral("7 (no name)"));
        QCOMPARE(l.value("A/r"), QStringLiteral("1 item"));
        QCOMPARE(l.value("A/m"), QStringLiteral("2 entries"));
        QCOMPARE(l.value("A/m/k"), QStringLiteral("1"));
        QCOMPARE(l.value("A/msg"), QString());
        QCOMPARE(l.value("A/none"), QStringLiteral("{}"));
        QCOMPARE(l.type("A"), QStringLiteral("message"));
        QCOMPARE(l.type("A/r"), QStringLiteral("repeated int32"));
        QCOMPARE(l.type("A/r/[0]"), QStringLiteral("int32"));
        QCOMPARE(l.type("A/m"), QStringLiteral("map<string, int32>"));
        QCOMPARE(l.type("A/m/k"), QStringLiteral("int32"));
        QCOMPARE(l.type("A/e"), QStringLiteral("E"));
    }

    void whatIsShownForWhatIsNot()
    {
        const Laid l("message A { int32 i = 1; string s = 2; bytes b = 3; E e = 4; repeated int32 r = 5; map<string, int32> m = 6;\n"
                     " B msg = 7; optional int32 o = 8; optional string p = 9; optional E q = 10; oneof one { int32 x = 11; B y = 12; } bool t = 13; }\n"
                     "enum E { ZERO = 0; } message B {}", "{}");
        QCOMPARE(l.value("A/i"), QStringLiteral("0"));
        QCOMPARE(l.value("A/s"), QStringLiteral("\"\""));
        QCOMPARE(l.value("A/b"), QStringLiteral("0 bytes"));
        QCOMPARE(l.value("A/e"), QStringLiteral("ZERO"));
        QCOMPARE(l.value("A/r"), QStringLiteral("0 items"));
        QCOMPARE(l.value("A/m"), QStringLiteral("0 entries"));
        QCOMPARE(l.value("A/t"), QStringLiteral("false"));
        QCOMPARE(l.value("A/msg"), QStringLiteral("not set"));
        QCOMPARE(l.value("A/o"), QStringLiteral("not set (0)"));
        QCOMPARE(l.value("A/p"), QStringLiteral("not set (\"\")"));
        QCOMPARE(l.value("A/q"), QStringLiteral("not set (ZERO)"));
        QCOMPARE(l.value("A/one"), QStringLiteral("not set"));
        QCOMPARE(l.value("A/one/x"), QStringLiteral("not set (0)"));
        QCOMPARE(l.value("A/one/y"), QStringLiteral("not set"));
    }

    void writtenDefaults()
    {
        const Laid l("syntax = \"proto2\"; message A { optional int32 i = 1 [default = -5]; optional string s = 2 [default = \"a\\n\"];\n"
                     " optional bytes b = 3 [default = \"\\xff\"]; optional E e = 4 [default = TWO]; optional bool t = 5 [default = true];\n"
                     " optional double d = 6 [default = inf]; required int32 r = 7; }\nenum E { ONE = 1; TWO = 2; }", "{\"i\": null}");
        QCOMPARE(l.value("A/i"), QStringLiteral("null → not set (-5)"));
        QCOMPARE(l.value("A/s"), QStringLiteral("not set (\"a\\n\")"));
        QCOMPARE(l.value("A/b"), QStringLiteral("not set (1 byte ff)"));
        QCOMPARE(l.value("A/e"), QStringLiteral("not set (TWO)"));
        QCOMPARE(l.value("A/t"), QStringLiteral("not set (true)"));
        QCOMPARE(l.value("A/d"), QStringLiteral("not set (inf)"));
        QCOMPARE(l.state("A/r"), State::Problem);
        QCOMPARE(l.value("A/r"), QStringLiteral("not set"));
        QCOMPARE(l.type("A/r"), QStringLiteral("required int32 — required, but not set"));
    }

    void null()
    {
        const Laid l("import \"google/protobuf/struct.proto\";\n"
                     "message A { int32 i = 1; optional int32 o = 2; repeated int32 r = 3; map<string, int32> m = 4; A a = 5;\n"
                     " google.protobuf.Value v = 6; google.protobuf.NullValue n = 7; repeated google.protobuf.Value vs = 8; google.protobuf.Struct s = 9; }",
                     "{\"i\": null, \"o\": null, \"r\": null, \"m\": null, \"a\": null, \"v\": null, \"n\": null, \"vs\": [null], \"s\": null}");
        QCOMPARE(l.value("A/i"), QStringLiteral("null → 0"));
        QCOMPARE(l.state("A/i"), State::Default);
        QCOMPARE(l.value("A/o"), QStringLiteral("null → not set (0)"));
        QCOMPARE(l.value("A/r"), QStringLiteral("null → 0 items"));
        QCOMPARE(l.value("A/m"), QStringLiteral("null → 0 entries"));
        QCOMPARE(l.value("A/a"), QStringLiteral("null → not set"));
        QCOMPARE(l.value("A/s"), QStringLiteral("null → not set"));
        QCOMPARE(l.state("A/v"), State::Set);
        QCOMPARE(l.value("A/v"), QStringLiteral("null"));
        QCOMPARE(l.value("A/n"), QStringLiteral("NULL_VALUE"));
        QCOMPARE(l.value("A/vs/[0]"), QStringLiteral("null"));
        QVERIFY(l.tree->nodes(State::Problem).empty());
    }

    void problems()
    {
        const Laid l("message A { int32 i = 1; B b = 2; repeated int32 r = 3; map<int32, string> m = 4; E e = 5; oneof o { int32 x = 6; int32 y = 7; } bool t = 8; int32 foo_bar = 9; repeated B bs = 10; }\n"
                     "message B {} enum E { ZERO = 0; }",
                     "{\"i\": \"x\", \"b\": [], \"r\": {}, \"m\": {\"k\": \"v\", \"1\": null, \"2\": 5, \"2\": \"again\"}, \"e\": \"NOPE\", \"x\": 1, \"y\": 2, \"t\": 1, \"fooBar\": 1, \"foo_bar\": 2,"
                     "\"bs\": [null, 1], \"what\": {\"deep\": [1, 2]}}");
        QCOMPARE(l.type("A/i"), QStringLiteral("int32 — not a number"));
        QCOMPARE(l.value("A/i"), QStringLiteral("\"x\""));
        QCOMPARE(l.problem("A/b"), QStringLiteral("expected an object"));
        QCOMPARE(l.value("A/b"), QStringLiteral("[]"));
        QCOMPARE(l.problem("A/r"), QStringLiteral("expected a list"));
        QCOMPARE(l.problem("A/m/k"), QStringLiteral("the key is not a number"));
        QCOMPARE(l.problem("A/m/1"), QStringLiteral("null cannot be in a map"));
        QCOMPARE(l.value("A/m/1"), QStringLiteral("null"));
        QCOMPARE(l.problem("A/m/2"), QStringLiteral("expected a string"));
        QCOMPARE(l.at(QStringLiteral("A/m"))->children.back()->problem, QStringLiteral("this key has been given before"));
        QCOMPARE(l.problem("A/e"), QStringLiteral("no such value in E"));
        QCOMPARE(l.problem("A/o/x"), QString());
        QCOMPARE(l.value("A/o"), QStringLiteral("x"));
        QCOMPARE(l.problem("A/o/y"), QStringLiteral("o already has x"));
        QCOMPARE(l.value("A/o/y"), QStringLiteral("2"));
        QCOMPARE(l.problem("A/t"), QStringLiteral("expected true or false"));
        QCOMPARE(l.value("A/foo_bar"), QStringLiteral("1"));
        QCOMPARE(l.problem("A/bs/[0]"), QStringLiteral("null cannot be in a list"));
        QCOMPARE(l.problem("A/bs/[1]"), QStringLiteral("expected an object"));
        QCOMPARE(l.value("A/bs/[1]"), QStringLiteral("1"));
        QCOMPARE(l.type("A/bs/[1]"), QStringLiteral("B — expected an object"));
    }

    void numbersOutOfReach()
    {
        // Some of which protobuf's own parser takes, or never gets to see.
        const std::pair<const char *, const char *> rows[] = {
            {"\"i\": 1e99999999999", "out of range for int32"},
            {"\"i\": 1e-99999999999", "not a whole number"},
            {"\"i\": 1e-400", "not a whole number"},
            {"\"i\": 1e30", "out of range for int32"},
            {"\"l\": 9223372036854775808", "out of range for int64"},
            {"\"u\": -1", "out of range for uint32"},
            {"\"d\": 1e400", "out of range for double"},
            {"\"d\": \"-1e400\"", "out of range for double"},
            {"\"f\": 3.4028236e38", "out of range for float"},
            {"\"d\": \" 1\"", "not a number"},
        };
        for (const auto &[json, problem] : rows) {
            const Laid l("message A { int32 i = 1; int64 l = 2; uint32 u = 3; double d = 4; float f = 5; }",
                         QByteArray("{") + json + "}");
            QCOMPARE(l.tree->nodes(State::Problem).size(), size_t(1));
            QCOMPARE(l.tree->nodes(State::Problem).front()->problem, QString::fromLatin1(problem));
        }
        QCOMPARE(Laid("message A { double d = 1; }", "{\"d\": 1e-400}").value("A/d"), QStringLiteral("1e-400"));
    }

    void timesThatAreNone()
    {
        // Which protobuf's own parser does not look into.
        for (const char *time : {"2023-02-29T00:00:00Z", "2024-13-01T00:00:00Z", "2024-01-01T24:00:00Z",
                                 "2024-01-01T00:60:00Z", "2024-01-01T00:00:60Z", "2024-01-01T00:00:00+24:00",
                                 "2024-01-01T00:00:00+01:60"}) {
            const Laid l("import \"google/protobuf/timestamp.proto\"; message A { google.protobuf.Timestamp t = 1; }",
                         QByteArray("{\"t\": \"") + time + "\"}");
            QCOMPARE(l.problem("A/t"), QStringLiteral("not a timestamp"));
            QCOMPARE(l.value("A/t"), u'"' + QString::fromLatin1(time) + u'"');
        }
    }

    void givenTwice()
    {
        const Laid l("message A { int32 foo_bar = 1; }", "{\"fooBar\": 1, \"foo_bar\": 2}");
        QCOMPARE(l.tree->roots().front()->children.size(), size_t(2));
        const Node &second = *l.tree->roots().front()->children[1];
        QCOMPARE(second.name, QStringLiteral("foo_bar"));
        QCOMPARE(second.problem, QStringLiteral("foo_bar has been given before"));
        QCOMPARE(l.tree->valueText(second), QStringLiteral("2"));
    }

    void unknownIsOnOneLine()
    {
        const Laid l("message A {}", "{\"what\": {\n  \"deep\": [1,\n 2]}, \"long\": \"" + QByteArray(1000, 'x') + "\"}");
        QCOMPARE(l.value("A/what"), QStringLiteral("{ \"deep\": [1, 2]}"));
        QCOMPARE(l.type("A/what"), QStringLiteral("not in A"));
        QCOMPARE(l.value("A/long").size(), 121);
        QVERIFY(l.value("A/long").endsWith(QChar(0x2026)));
        QVERIFY(l.at(QStringLiteral("A/what"))->children.empty());
    }

    void longStringsAreCut()
    {
        const Laid l("message A { string s = 1; }", "{\"s\": \"" + QByteArray(1000, 'x') + "\"}");
        QCOMPARE(l.value("A/s").size(), 123);
        QCOMPARE(l.copied("A/s").size(), 1000);
    }

    void severalMessages()
    {
        const QByteArray proto = "message A { int32 i = 1; }";
        const Laid list(proto, "[{\"i\": 1}, {\"i\": 2}]");
        QCOMPARE(list.names(), QStringList({"A [0]", "i", "A [1]", "i"}));
        QCOMPARE(list.pathTo("A [1]/i"), QStringLiteral("$[1].i"));
        QCOMPARE(list.pathTo("A [1]"), QStringLiteral("$[1]"));
        const Laid lines(proto, "{\"i\": 1}\n{\"i\": 2}\n");
        QCOMPARE(lines.names(), list.names());
        QCOMPARE(lines.pathTo("A [1]/i"), QStringLiteral("$.i")); // each is a document of its own
        QCOMPARE(Laid(proto, "[{}]").names(), QStringList({"A [0]", "i"}));
        QCOMPARE(Laid(proto, "{}").names(), QStringList({"A", "i"}));
        QVERIFY(Laid(proto, "[]").names().isEmpty());
        QVERIFY(Laid(proto, "").names().isEmpty());
        const Laid bad(proto, "[{}, 7, null]");
        QCOMPARE(bad.problem("A [1]"), QStringLiteral("expected an object"));
        QCOMPARE(bad.value("A [1]"), QStringLiteral("7"));
        QCOMPARE(bad.value("A [2]"), QStringLiteral("null"));
    }

    void specialRoots()
    {
        const QByteArray proto = "import \"google/protobuf/struct.proto\"; import \"google/protobuf/timestamp.proto\"; import \"google/protobuf/wrappers.proto\";\n"
                                 "message A { google.protobuf.Struct s = 1; google.protobuf.Timestamp t = 2; google.protobuf.Int32Value w = 3; }";
        const Laid list(proto, "[1, \"x\"]", QStringLiteral("google.protobuf.ListValue"));
        QCOMPARE(list.names(), QStringList({"ListValue", "[0]", "[1]"}));
        QCOMPARE(list.value("ListValue"), QStringLiteral("2 items"));
        const Laid any(proto, "[1]", QStringLiteral("google.protobuf.Value"));
        QCOMPARE(any.names(), QStringList({"Value", "[0]"}));
        const Laid wrapper(proto, "7", QStringLiteral("google.protobuf.Int32Value"));
        QCOMPARE(wrapper.value("Int32Value"), QStringLiteral("7"));
        const Laid time(proto, "\"nope\"", QStringLiteral("google.protobuf.Timestamp"));
        QCOMPARE(time.problem("Timestamp"), QStringLiteral("not a timestamp"));
        const Laid times(proto, "[\"2024-01-01T00:00:00Z\"]", QStringLiteral("google.protobuf.Timestamp"));
        QCOMPARE(times.value("Timestamp [0]"), QStringLiteral("2024-01-01T00:00:00Z"));
    }

    void freeJson()
    {
        const Laid l("import \"google/protobuf/struct.proto\"; message A { google.protobuf.Struct s = 1; }",
                     "{\"s\": {\"a\": [1, {\"b\": null}], \"c\": \"x\", \"d\": true, \"c\": 1}}");
        QCOMPARE(l.value("A/s"), QStringLiteral("4 entries"));
        QCOMPARE(l.value("A/s/a"), QStringLiteral("2 items"));
        QCOMPARE(l.type("A/s/a"), QStringLiteral("list"));
        QCOMPARE(l.value("A/s/a/[1]/b"), QStringLiteral("null"));
        QCOMPARE(l.type("A/s/a/[0]"), QStringLiteral("number"));
        QCOMPARE(l.type("A/s/c"), QStringLiteral("string"));
        QCOMPARE(l.type("A/s/d"), QStringLiteral("bool"));
        QCOMPARE(l.pathTo("A/s/a/[1]/b"), QStringLiteral("$.s.a[1].b"));
        QCOMPARE(l.tree->nodes(State::Problem).size(), size_t(1)); // c, again
        QCOMPARE(l.tree->nodes(State::Set).size(), size_t(1)); // the parts of it are no fields
    }

    void any()
    {
        const QByteArray proto = "import \"google/protobuf/any.proto\"; import \"google/protobuf/duration.proto\";\n"
                                 "message A { google.protobuf.Any a = 1; google.protobuf.Duration d = 2; } message In { int32 x = 1; string y = 2; }";
        const Laid l(proto, "{\"a\": {\"x\": 1, \"@type\": \"t/In\", \"z\": 3}}");
        QCOMPARE(l.value("A/a"), QStringLiteral("In"));
        QCOMPARE(l.names(), QStringList({"A", "a", "@type", "x", "y", "z", "d"}));
        QCOMPARE(l.state("A/a/y"), State::Default);
        QCOMPARE(l.type("A/a/z"), QStringLiteral("not in In"));
        QCOMPARE(JsonTree::sourceParts(*l.at(QStringLiteral("A/a/@type"))).front()->name, QStringLiteral("In"));
        QCOMPARE(JsonTree::sourceParts(*l.at(QStringLiteral("A/a/z"))).front()->name, QStringLiteral("In"));

        const Laid special(proto, "{\"a\": {\"@type\": \"t/google.protobuf.Duration\", \"value\": \"1.5s\"}}");
        QCOMPARE(special.value("A/a/value"), QStringLiteral("1.5s"));
        QCOMPARE(special.type("A/a/value"), QStringLiteral("Duration"));
        const Laid elsewhere(proto, "{\"a\": {\"@type\": \"t/google.protobuf.Duration\", \"other\": \"1s\"}}");
        QCOMPARE(elsewhere.type("A/a/other"), QStringLiteral("not in Duration"));

        const Laid gone(proto, "{\"a\": {\"@type\": \"t/Gone\", \"x\": {\"y\": 1}}}");
        QCOMPARE(gone.type("A/a/@type"), QStringLiteral("string — not in this schema"));
        QCOMPARE(gone.value("A/a/x/y"), QStringLiteral("1"));
        QCOMPARE(Laid(proto, "{\"a\": {\"x\": 1}}").problem("A/a"), QStringLiteral("no @type to say what it is"));
        QCOMPARE(Laid(proto, "{\"a\": 5}").problem("A/a"), QStringLiteral("expected an object"));
        const Laid empty(proto, "{\"a\": {}}"); // as protobuf prints one
        QCOMPARE(empty.state("A/a"), State::Set);
        QCOMPARE(empty.value("A/a"), QStringLiteral("{}"));
        QCOMPARE(empty.tree->withDefaults(), QStringLiteral("{\n  \"a\": {}\n}"));
    }

    void anyOfAListOfSpecialOnes()
    {
        const Laid l("import \"google/protobuf/any.proto\"; import \"google/protobuf/struct.proto\";\n"
                     "message A { repeated google.protobuf.Any a = 1; google.protobuf.Struct s = 2; }",
                     "{\"a\": [{\"@type\": \"t/google.protobuf.Struct\", \"value\": {\"k\": [1]}}]}");
        QCOMPARE(l.value("A/a/[0]"), QStringLiteral("google.protobuf.Struct"));
        QCOMPARE(l.value("A/a/[0]/value/k/[0]"), QStringLiteral("1"));
        QVERIFY(l.tree->nodes(State::Problem).empty());
    }

    void unresolvedTypes()
    {
        const Laid l("message A { Gone a = 1; repeated Gone b = 2; Gone c = 3; map<string, Gone> d = 4; }",
                     "{\"a\": {\"x\": [1]}, \"b\": [\"v\"], \"d\": {\"k\": 5}}");
        QVERIFY(l.tree->nodes(State::Problem).empty()); // it is the schema that lacks it
        QCOMPARE(l.type("A/a"), QStringLiteral("Gone (not found)"));
        QCOMPARE(l.value("A/a/x/[0]"), QStringLiteral("1"));
        QCOMPARE(l.value("A/b/[0]"), QStringLiteral("\"v\""));
        QCOMPARE(l.type("A/b/[0]"), QStringLiteral("Gone (not found)"));
        QCOMPARE(l.value("A/d/k"), QStringLiteral("5"));
        QVERIFY(l.at(QStringLiteral("A/c"))->isMissing());
    }

    void paths()
    {
        const Laid l("message A { A foo_bar = 1; repeated A r = 2; map<string, A> m = 3; oneof o { A x = 4; } int32 n = 5 [json_name = \"odd name\"]; }",
                     "{\"fooBar\": {\"r\": [{}, {\"m\": {\"a b\": {\"x\": {}}, \"ok\": {}}}]}, \"odd name\": 1}");
        QCOMPARE(l.pathTo("A"), QStringLiteral("$"));
        QCOMPARE(l.pathTo("A/foo_bar"), QStringLiteral("$.fooBar"));
        QCOMPARE(l.pathTo("A/foo_bar/r/[1]/m/a b/o/x"), QStringLiteral("$.fooBar.r[1].m[\"a b\"].x"));
        QCOMPARE(l.pathTo("A/foo_bar/r/[1]/m/ok"), QStringLiteral("$.fooBar.r[1].m.ok"));
        QCOMPARE(l.pathTo("A/n"), QStringLiteral("$[\"odd name\"]"));
        QCOMPARE(l.pathTo("A/o"), QStringLiteral("$"));
        QCOMPARE(l.pathTo("A/o/x"), QStringLiteral("$.x"));          // where it would be
        QCOMPARE(l.pathTo("A/foo_bar/foo_bar"), QStringLiteral("$.fooBar.fooBar"));
        const Laid own("message A { A foo_bar = 1; int32 b_c = 2; }", "{\"foo_bar\": {}}");
        QCOMPARE(own.pathTo("A/b_c"), QStringLiteral("$.b_c"));      // as this JSON calls things
        QCOMPARE(own.pathTo("A/foo_bar/b_c"), QStringLiteral("$.foo_bar.b_c"));
    }

    void copying()
    {
        const Laid l("syntax = \"proto2\"; message A { optional string s = 1; optional int64 l = 2; optional bytes b = 3; optional A a = 4; repeated int32 r = 5;\n"
                     " optional string d = 6 [default = \"dé\"]; optional bytes bd = 7 [default = \"\\xff\"]; optional A none = 8; optional int32 i = 9 [default = 4]; repeated int32 e = 10; oneof o { int32 x = 11; } }",
                     "{\"s\": \"a\\n\\\"\", \"l\": \"12\", \"b\": \"AP8=\", \"a\": { \"i\" : 1 }, \"r\": [1, 2], \"what\": {\"x\": 1}}");
        QCOMPARE(l.copied("A/s"), QStringLiteral("a\n\""));
        QCOMPARE(l.copied("A/l"), QStringLiteral("12"));
        QCOMPARE(l.copied("A/b"), QStringLiteral("AP8="));
        QCOMPARE(l.copied("A/a"), QStringLiteral("{ \"i\" : 1 }"));
        QCOMPARE(l.copied("A/r"), QStringLiteral("[1, 2]"));
        QCOMPARE(l.copied("A/r/[1]"), QStringLiteral("2"));
        QCOMPARE(l.copied("A/d"), QStringLiteral("dé"));
        QCOMPARE(l.copied("A/bd"), QStringLiteral("/w=="));
        QCOMPARE(l.copied("A/i"), QStringLiteral("4"));
        QCOMPARE(l.copied("A/e"), QStringLiteral("[]"));
        QCOMPARE(l.copied("A/what"), QStringLiteral("{\"x\": 1}"));
        QVERIFY(l.copied("A/none").isNull());
        QVERIFY(l.copied("A/o").isNull());
    }

    void filledIn()
    {
        const QByteArray proto = "message A { int32 foo_bar = 1; int64 l = 2; repeated A r = 3; optional int32 o = 4; A a = 5; map<string, A> m = 6; E e = 7; string s = 8; oneof one { int32 x = 9; } }\n"
                                 "enum E { ZERO = 0; } message B { int32 i = 1; }";
        // Over again in the words of the JSON, with what is missing in its place.
        QCOMPARE(Laid(proto, "{\"s\": \"\\u0041\", \"l\": null, \"what\": [1, {\"z\": 1.50}], \"o\": null, \"a\": {}}").tree->withDefaults(),
                 QStringLiteral("{\n  \"fooBar\": 0,\n  \"l\": \"0\",\n  \"r\": [],\n  \"o\": null,\n  \"a\": {\n    \"fooBar\": 0,\n    \"l\": \"0\",\n    \"r\": [],\n"
                                "    \"m\": {},\n    \"e\": \"ZERO\",\n    \"s\": \"\"\n  },\n  \"m\": {},\n  \"e\": \"ZERO\",\n  \"s\": \"\\u0041\",\n"
                                "  \"what\": [\n    1,\n    {\n      \"z\": 1.50\n    }\n  ]\n}"));
        // Under the names it uses.
        QVERIFY(Laid(proto, "{\"r\": [{\"foo_bar\": 1}]}").tree->withDefaults().startsWith(
            QStringLiteral("{\n  \"foo_bar\": 0,\n  \"l\"")));
        // And what is in there already keeps the one it has.
        QCOMPARE(Laid("message A { int32 foo_bar = 1; int32 b_c = 2; int32 d_e = 3; }", "{\"fooBar\": null, \"b_c\": 1}").tree->withDefaults(),
                 QStringLiteral("{\n  \"fooBar\": 0,\n  \"b_c\": 1,\n  \"d_e\": 0\n}"));
        const QByteArray small = "message A { int32 i = 1; }";
        QCOMPARE(Laid(small, "[{}, {\"i\": 1}]").tree->withDefaults(),
                 QStringLiteral("[\n  {\n    \"i\": 0\n  },\n  {\n    \"i\": 1\n  }\n]"));
        QCOMPARE(Laid(small, "{}\n{\"i\" : 1, \"x\": [1, 2]}\n").tree->withDefaults(),
                 QStringLiteral("{\"i\":0}\n{\"i\":1,\"x\":[1,2]}"));
        QCOMPARE(Laid(small, "[]").tree->withDefaults(), QStringLiteral("[]"));
        QCOMPARE(Laid(small, "").tree->withDefaults(), QString());
        QCOMPARE(Laid(small, "7").tree->withDefaults(), QStringLiteral("7"));
        QCOMPARE(Laid(small, "[7, {\"i\": \"x\"}]").tree->withDefaults(),
                 QStringLiteral("[\n  7,\n  {\n    \"i\": \"x\"\n  }\n]"));
        QCOMPARE(Laid("message A {}", "{ }").tree->withDefaults(), QStringLiteral("{}"));
    }

    void whereItIs()
    {
        const QByteArray json = "{\"a\": {\"r\": [10, 20]}, \"x\" : 5, \"what\": {\"k\": 1}}";
        const Laid l("message A { A a = 1; repeated int32 r = 2; oneof o { int32 x = 3; } int32 none = 4; }", json);
        const auto nameAt = [&](const char *at, int plus = 0) {
            const Node *node = l.tree->nodeAt(json.indexOf(at) + plus);
            return node ? l.tree->pathTo(*node) : QString();
        };
        QCOMPARE(nameAt("{"), QStringLiteral("$"));
        QCOMPARE(nameAt("\"a\""), QStringLiteral("$.a"));
        QCOMPARE(nameAt("\"r\"", 1), QStringLiteral("$.a.r"));
        QCOMPARE(nameAt("10", 1), QStringLiteral("$.a.r[0]"));
        QCOMPARE(nameAt("20", 2), QStringLiteral("$.a.r[1]")); // right behind it
        QCOMPARE(nameAt("\"x\"", 4), QStringLiteral("$.x"));    // in a oneof
        QCOMPARE(nameAt("\"k\""), QStringLiteral("$.what"));    // as far as it goes
        QCOMPARE(nameAt(", \"x\"", 1), QStringLiteral("$"));
        QVERIFY(!l.tree->nodeAt(json.size() + 1));

        const auto slice = [&](const char *path) {
            return l.tree->document().slice(*JsonTree::jsonFor(*l.at(QString::fromLatin1(path))));
        };
        QCOMPARE(slice("A/o/x"), QStringLiteral("5"));
        QCOMPARE(slice("A/o"), QString::fromLatin1(json)); // what it is in
        QCOMPARE(slice("A/none"), QString::fromLatin1(json));
        QCOMPARE(slice("A/a/none"), QStringLiteral("{\"r\": [10, 20]}"));
    }

    void whatDeclaresIt()
    {
        const Laid l("message A { B b = 1; repeated B r = 2; map<string, int32> m = 3; oneof o { int32 x = 4; } } message B { int32 i = 1; }",
                     "{\"b\": {\"else\": 1}, \"r\": [{}], \"m\": {\"k\": 1}, \"what\": 1}");
        const auto parts = [&](const char *path) {
            QStringList result;
            for (const Decl *decl : JsonTree::sourceParts(*l.at(QString::fromLatin1(path))))
                result << decl->fullName;
            return result.join(u' ');
        };
        QCOMPARE(parts("A"), QStringLiteral("A"));
        QCOMPARE(parts("A/b"), QStringLiteral("A.b B"));
        QCOMPARE(parts("A/b/i"), QStringLiteral("B.i"));
        QCOMPARE(parts("A/r/[0]"), QStringLiteral("A.r B"));
        QCOMPARE(parts("A/m/k"), QStringLiteral("A.m"));
        QCOMPARE(parts("A/o"), QStringLiteral("A.o"));
        QCOMPARE(parts("A/what"), QStringLiteral("A")); // what it is not in
        QCOMPARE(parts("A/b/else"), QStringLiteral("B"));
    }

    void model()
    {
        const Laid l("message A { int32 a = 1; repeated A r = 2; string gone = 7; }",
                     "{\"a\": 5, \"r\": [{}, {\"r\": []}], \"what\": 1}");
        JsonModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.rowCount(), 0);
        model.setTree(l.tree);
        QCOMPARE(model.rowCount(), 1);
        const QModelIndex root = model.index(0, 0);
        QCOMPARE(model.rowCount(root), 4);
        const auto text = [&](int row, int column) { return model.index(row, column, root).data().toString(); };
        QCOMPARE(text(0, JsonModel::NameColumn), QStringLiteral("a"));
        QCOMPARE(text(0, JsonModel::ValueColumn), QStringLiteral("5"));
        QCOMPARE(text(0, JsonModel::TypeColumn), QStringLiteral("int32"));
        QCOMPARE(text(0, JsonModel::NumberColumn), QStringLiteral("1"));
        QCOMPARE(text(2, JsonModel::NumberColumn), QStringLiteral("7"));
        QCOMPARE(text(3, JsonModel::NameColumn), QStringLiteral("✕ what"));
        QCOMPARE(text(3, JsonModel::NumberColumn), QString());
        const QModelIndex item = model.index(0, 0, model.index(1, 0, root));
        QCOMPARE(item.data().toString(), QStringLiteral("[0]"));
        QCOMPARE(item.siblingAtColumn(JsonModel::NumberColumn).data().toString(), QString());
        QCOMPARE(model.index(2, 0, root).data(JsonModel::StateRole).toInt(), int(State::Default));
        QVERIFY(model.index(0, 0, root).data(Qt::ToolTipRole).toString().startsWith(QStringLiteral("$.a\nA.a\na.proto:")));
        for (const Node *node : l.tree->nodes())
            QCOMPARE(model.nodeAt(model.indexFor(node)), node);
        model.setTree(nullptr);
        QCOMPARE(model.rowCount(), 0);
    }

    void deep()
    {
        // As deep as the reader goes.
        QByteArray json;
        for (int i = 0; i < 400; ++i)
            json += "{\"a\": ";
        json += "{}" + QByteArray(400, '}');
        const Laid l("message A { A a = 1; int32 i = 2; }", json);
        QCOMPARE(l.tree->nodes(State::Set).size(), size_t(400));
        QVERIFY(l.tree->withDefaults().count(QStringLiteral("\"i\": 0")) == 401);
    }
};

QTEST_GUILESS_MAIN(TestJsonTree)
#include "tst_jsontree.moc"
