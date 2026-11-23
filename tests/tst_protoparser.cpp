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

#include "ProtoParser.h"

#include <QTest>

using namespace protoexplorer;

namespace {

const Decl *find(const std::vector<std::unique_ptr<Decl>> &decls, const QString &name)
{
    for (const auto &d : decls) {
        if (d->name == name)
            return d.get();
    }
    return nullptr;
}

QStringList names(const std::vector<std::unique_ptr<Decl>> &decls)
{
    QStringList result;
    for (const auto &d : decls)
        result << d->name;
    return result;
}

QString sourceOf(const Decl *decl)
{
    return decl ? decl->file->text.snippet(decl->span).text : QStringLiteral("(missing)");
}

} // namespace

class TestProtoParser : public QObject
{
    Q_OBJECT

private slots:
    void proto3()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto3\";\n"
            "package shop.v1;\n"
            "import \"x/y.proto\";\n"
            "import public \"z.proto\";\n"
            "// An order.\n"
            "message Order {\n"
            "  int64 id = 1; // the id\n"
            "  repeated Item items = 2;\n"
            "  optional string note = 3;\n"
            "  map<string, Item> by_sku = 4;\n"
            "  map<int32, bool> flags = 5;\n"
            "  .shop.v1.Status status = 6;\n"
            "}\n"
            "enum Status { UNKNOWN = 0; DONE = 1; }\n"
            "service Orders { rpc Get(stream Req) returns (stream Order); rpc Put(Order) returns (Req); }\n");
        QVERIFY2(file->notes.empty(), qPrintable(file->notes.empty() ? QString() : file->notes[0].text));
        QCOMPARE(file->package, QStringLiteral("shop.v1"));
        QCOMPARE(file->imports.size(), size_t(2));
        QCOMPARE(file->imports[0].path, QStringLiteral("x/y.proto"));
        QVERIFY(!file->imports[0].isPublic);
        QVERIFY(file->imports[1].isPublic);
        QCOMPARE(names(file->decls), QStringList({"Order", "Status", "Orders"}));

        const Decl *order = find(file->decls, QStringLiteral("Order"));
        QCOMPARE(order->fullName, QStringLiteral("shop.v1.Order"));
        // No oneof invented for `optional`, and no entry types for the maps.
        QCOMPARE(names(order->members), QStringList({"id", "items", "note", "by_sku", "flags", "status"}));
        QVERIFY(order->nested.empty());

        const auto signature = [&](const char *name) {
            return find(order->members, QString::fromLatin1(name))->signature;
        };
        QCOMPARE(signature("id"), QStringLiteral("int64"));
        QCOMPARE(signature("items"), QStringLiteral("repeated Item"));
        QCOMPARE(signature("note"), QStringLiteral("optional string"));
        QCOMPARE(signature("by_sku"), QStringLiteral("map<string, Item>"));
        QCOMPARE(signature("flags"), QStringLiteral("map<int32, bool>"));
        QCOMPARE(find(order->members, QStringLiteral("by_sku"))->typeName, QStringLiteral("Item"));
        QCOMPARE(find(order->members, QStringLiteral("flags"))->typeName, QString());
        QCOMPARE(find(order->members, QStringLiteral("items"))->number, 2);

        QCOMPARE(sourceOf(find(order->members, QStringLiteral("id"))), QStringLiteral("int64 id = 1; // the id"));
        QCOMPARE(sourceOf(find(order->members, QStringLiteral("by_sku"))), QStringLiteral("map<string, Item> by_sku = 4;"));
        QVERIFY(sourceOf(order).startsWith(QStringLiteral("// An order.\nmessage Order {")));
        QVERIFY(sourceOf(order).endsWith(u'}'));

        const Decl *service = find(file->decls, QStringLiteral("Orders"));
        QCOMPARE(service->members[0]->signature, QStringLiteral("rpc (stream Req) → stream Order"));
        QCOMPARE(service->members[1]->signature, QStringLiteral("rpc (Order) → Req"));
        const Decl *status = find(file->decls, QStringLiteral("Status"));
        QCOMPARE(names(status->members), QStringList({"UNKNOWN", "DONE"}));
        QCOMPARE(status->members[1]->number, 1);
        QCOMPARE(sourceOf(status->members[1].get()), QStringLiteral("DONE = 1;"));
    }

    void proto2GroupsAndLabels()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto2\";\n"
            "message M {\n"
            "  required int32 a = 1;\n"
            "  optional M b = 2 [deprecated = true];\n"
            "  repeated group Result = 3 {\n"
            "    optional string url = 4;\n"
            "  }\n"
            "  extensions 100 to max;\n"
            "}\n");
        QVERIFY(file->notes.empty());
        const Decl *m = find(file->decls, QStringLiteral("M"));
        QCOMPARE(m->members[0]->signature, QStringLiteral("required int32"));
        QCOMPARE(m->members[1]->signature, QStringLiteral("optional M"));
        QCOMPARE(m->members[2]->signature, QStringLiteral("repeated Result"));
        QCOMPARE(names(m->nested), QStringList({"Result"}));
        QCOMPARE(names(m->nested[0]->members), QStringList({"url"}));
        QCOMPARE(m->members[2]->span, m->nested[0]->span);
    }

    void editions_data()
    {
        QTest::addColumn<QByteArray>("edition");
        QTest::newRow("2023") << QByteArray("2023");
        QTest::newRow("2024") << QByteArray("2024");
    }

    void editions()
    {
        QFETCH(QByteArray, edition);
        const auto file = parseProto(QStringLiteral("a.proto"),
            "edition = \"" + edition + "\";\nmessage M { int32 a = 1; M b = 2; }\n");
        QVERIFY2(file->notes.empty(), qPrintable(file->notes.empty() ? QString() : file->notes[0].text));
        const Decl *m = find(file->decls, QStringLiteral("M"));
        QVERIFY(m);
        QCOMPARE(m->members[0]->signature, QStringLiteral("int32"));
    }

    void visibilityIsPartOfTheDeclaration()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "edition = \"2024\";\n// Doc.\nexport message M { int32 a = 1; }\nlocal enum E { Z = 0; }\n");
        QVERIFY2(file->notes.empty(), qPrintable(file->notes.empty() ? QString() : file->notes[0].text));
        QCOMPARE(sourceOf(find(file->decls, QStringLiteral("M"))),
                 QStringLiteral("// Doc.\nexport message M { int32 a = 1; }"));
        QCOMPARE(sourceOf(find(file->decls, QStringLiteral("E"))),
                 QStringLiteral("local enum E { Z = 0; }"));
    }

    void oneofsHoldTheirFieldsInOrder()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto3\";\n"
            "message M {\n"
            "  int32 first = 9;\n"
            "  // Pick one.\n"
            "  oneof pick {\n"
            "    string s = 1;\n"
            "    M m = 2;\n"
            "  }\n"
            "  int32 last = 3;\n"
            "}\n");
        const Decl *m = find(file->decls, QStringLiteral("M"));
        QCOMPARE(names(m->members), QStringList({"first", "pick", "last"}));
        const Decl *pick = m->members[1].get();
        QCOMPARE(pick->kind, DeclKind::Oneof);
        QCOMPARE(names(pick->members), QStringList({"s", "m"}));
        QCOMPARE(sourceOf(pick),
                 QStringLiteral("// Pick one.\noneof pick {\n  string s = 1;\n  M m = 2;\n}"));
    }

    void extendBlocksKeepTheirOwnFields()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto2\";\n"
            "extend Foo { optional int32 a = 100; optional int32 b = 101; }\n"
            "message M {\n"
            "  extend Bar { optional M c = 102; }\n"
            "  extend Baz { optional M d = 103; }\n"
            "}\n"
            "extend Foo { optional string e = 104; }\n");
        QVERIFY(file->notes.empty());
        QCOMPARE(names(file->decls), QStringList({"Foo", "M", "Foo"}));
        QCOMPARE(names(file->decls[0]->members), QStringList({"a", "b"}));
        QCOMPARE(names(file->decls[2]->members), QStringList({"e"}));
        QCOMPARE(sourceOf(file->decls[2].get()), QStringLiteral("extend Foo { optional string e = 104; }"));
        const Decl *m = find(file->decls, QStringLiteral("M"));
        QCOMPARE(names(m->nested), QStringList({"Bar", "Baz"}));
        QCOMPARE(names(m->nested[1]->members), QStringList({"d"}));
        QCOMPARE(m->nested[1]->kind, DeclKind::Extend);
    }

    void packageMayFollowMessages()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
                                     "syntax = \"proto3\";\nmessage M {}\npackage late;\n");
        QCOMPARE(file->decls[0]->fullName, QStringLiteral("late.M"));
    }

    void tabsAndByteOrderMark()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "\xEF\xBB\xBFsyntax = \"proto3\";\nmessage M {\n\tint32\ta\t=\t1;\tint32 b = 2;\n}\n");
        QVERIFY(file->notes.empty());
        QCOMPARE(sourceOf(file->decls[0]->members[0].get()), QStringLiteral("int32\ta\t=\t1;"));
        QCOMPARE(sourceOf(file->decls[0]->members[1].get()), QStringLiteral("int32 b = 2;"));
    }

    void brokenFileKeepsWhatItCan()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto3\";\n"
            "message Good { int32 a = 1; }\n"
            "message Bad { int32 = ; int32 ok = 2; }\n"
            "message Also { int32 b = 1; }\n");
        QVERIFY(file->notes.size() >= 2);
        QCOMPARE(file->notes[0].line, 3);
        QVERIFY(file->notes.back().text.contains(QStringLiteral("may be missing")));
        QVERIFY(find(file->decls, QStringLiteral("Good")));
        QVERIFY(find(file->decls, QStringLiteral("Also")));
        QCOMPARE(names(find(file->decls, QStringLiteral("Bad"))->members), QStringList({"ok"}));
    }

    void badSyntaxLineDoesNotHideTheFile_data()
    {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("unknown edition") << QByteArray("edition = \"2099\";");
        QTest::newRow("no semicolon") << QByteArray("syntax = \"proto3\"");
        QTest::newRow("unknown syntax") << QByteArray("syntax = \"proto9\";");
        QTest::newRow("after a comment") << QByteArray("// License.\n/* x */ edition = \"2099\";");
    }

    void badSyntaxLineDoesNotHideTheFile()
    {
        QFETCH(QByteArray, line);
        const auto file = parseProto(QStringLiteral("a.proto"),
                                     line + "\npackage p;\n// Doc.\nmessage M { int32 a = 1; }\n");
        QVERIFY(!file->notes.empty());
        QCOMPARE(file->package, QStringLiteral("p"));
        QCOMPARE(names(file->decls), QStringList({"M"}));
        QCOMPARE(names(file->decls[0]->members), QStringList({"a"}));
        QCOMPARE(sourceOf(file->decls[0].get()), QStringLiteral("// Doc.\nmessage M { int32 a = 1; }"));
        // One line at fault, not one complaint per unlabelled field.
        QCOMPARE(file->notes.size(), size_t(2));
    }

    void leftoversWithoutNamesAreDropped()
    {
        const auto file = parseProto(QStringLiteral("a.proto"),
            "syntax = \"proto3\";\nmessage { int32 a = 1; }\nenum { A = 0; }\nservice { }\n"
            "message M { oneof { int32 x = 1; } int32 y = 2; }\n");
        for (const auto &d : file->decls) {
            QVERIFY(!d->name.isEmpty());
            for (const auto &m : d->members)
                QVERIFY(!m->name.isEmpty());
        }
        QVERIFY(!file->notes.empty());
    }

    void errorsAreCapped()
    {
        QByteArray source("syntax = \"proto3\";\n");
        for (int i = 0; i < 500; ++i)
            source += "nonsense;\n";
        const auto file = parseProto(QStringLiteral("a.proto"), source);
        QVERIFY(file->notes.size() <= 21);
    }

    void emptyAndHostileInputs_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("mark only") << QByteArray("\xEF\xBB\xBF");
        QTest::newRow("comment only") << QByteArray("// nothing");
        QTest::newRow("unclosed comment") << QByteArray("/* nothing");
        QTest::newRow("unclosed message") << QByteArray("syntax = \"proto3\"; message M { int32 a = 1;");
        QTest::newRow("syntax alone") << QByteArray("syntax");
        QTest::newRow("bytes") << QByteArray("\x00\x01\xff\xfe{{{", 7);
        QTest::newRow("stray braces") << QByteArray("}}}} message M {} }");
        QTest::newRow("bad utf-8") << QByteArray("syntax = \"proto3\"; message M { // \xff\xfe\n int32 a = 1; }");
    }

    void emptyAndHostileInputs()
    {
        QFETCH(QByteArray, bytes);
        const auto file = parseProto(QStringLiteral("a.proto"), bytes);
        QVERIFY(file);
        for (const auto &d : file->decls)
            sourceOf(d.get());
    }
};

QTEST_GUILESS_MAIN(TestProtoParser)
#include "tst_protoparser.moc"
