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

#include "TestFiles.h"

using namespace protoexplorer;

class TestTypeResolver : public QObject
{
    Q_OBJECT

    static QString typeOf(const Schema &schema, const char *field, const char *file = "")
    {
        const Decl *decl = findDecl(schema, QString::fromLatin1(field), QString::fromLatin1(file));
        if (!decl)
            return QStringLiteral("(no such field)");
        return decl->type ? decl->type->fullName : QStringLiteral("(unresolved)");
    }

private slots:
    void innermostScopeFirst()
    {
        TestFiles files;
        files.proto("a.proto", "package p; message T {} message M { message T {} T inner = 1; .p.T outer = 2; N.T far = 3; }"
                               " message N { message T {} }");
        const auto schema = files.load();
        QCOMPARE(typeOf(*schema, "p.M.inner"), QStringLiteral("p.M.T"));
        QCOMPARE(typeOf(*schema, "p.M.outer"), QStringLiteral("p.T"));
        QCOMPARE(typeOf(*schema, "p.M.far"), QStringLiteral("p.N.T"));
    }

    void importedBeatsAStrayInACloserScope()
    {
        TestFiles files;
        files.proto("acme/v1/order.proto", "package acme.v1; import \"common/money.proto\"; message Order { common.Money m = 1; }")
            .proto("common/money.proto", "package common; message Money {}")
            .proto("vendor/money.proto", "package acme.common; message Money {}");
        QCOMPARE(typeOf(*files.load(), "acme.v1.Order.m"), QStringLiteral("common.Money"));
    }

    void relativePackageName()
    {
        TestFiles files;
        files.proto("a.proto", "package x.y; import \"b.proto\"; message M { z.T t = 1; y.z.T u = 2; }")
            .proto("b.proto", "package x.y.z; message T {}");
        const auto schema = files.load();
        QCOMPARE(typeOf(*schema, "x.y.M.t"), QStringLiteral("x.y.z.T"));
        QCOMPARE(typeOf(*schema, "x.y.M.u"), QStringLiteral("x.y.z.T"));
    }

    void publicImportIsSeenThrough()
    {
        TestFiles files;
        files.proto("a.proto", "import \"b.proto\"; message M { T t = 1; }")
            .proto("b.proto", "import public \"c.proto\";")
            .proto("c.proto", "message T { int32 right = 1; }")
            .proto("0/d.proto", "message T { int32 wrong = 1; }");
        const auto schema = files.load();
        QCOMPARE(findDecl(*schema, "M.t")->type->file->displayPath, QStringLiteral("c.proto"));
    }

    void vendoredCopiesStayApart()
    {
        TestFiles files;
        files.proto("one/a.proto", "message A { T t = 1; } message T {}")
            .proto("two/a.proto", "message A { T t = 1; } message T {}");
        const auto schema = files.load();
        QCOMPARE(findDecl(*schema, "A.t", "one/a.proto")->type->file->displayPath, QStringLiteral("one/a.proto"));
        QCOMPARE(findDecl(*schema, "A.t", "two/a.proto")->type->file->displayPath, QStringLiteral("two/a.proto"));
    }

    void nearestWinsWhenNothingIsImported()
    {
        TestFiles files;
        files.proto("one/t.proto", "message T {}").proto("one/a.proto", "message A { T t = 1; }")
            .proto("two/t.proto", "message T {}").proto("two/b.proto", "message B { T t = 1; }");
        const auto schema = files.load();
        QCOMPARE(findDecl(*schema, "A.t")->type->file->displayPath, QStringLiteral("one/t.proto"));
        QCOMPARE(findDecl(*schema, "B.t")->type->file->displayPath, QStringLiteral("two/t.proto"));
    }

    void mapValueAndOneofAndGroup()
    {
        TestFiles files;
        files.add("a.proto", "syntax = \"proto2\"; package p;\n"
                             "message M { message T {} map<string, T> m = 1; oneof o { T a = 2; E e = 3; }\n"
                             "optional group G = 4 { optional T t = 5; } enum E { Z = 0; } }");
        const auto schema = files.load();
        QCOMPARE(allNotes(*schema), QString());
        QCOMPARE(typeOf(*schema, "p.M.m"), QStringLiteral("p.M.T"));
        QCOMPARE(typeOf(*schema, "p.M.a"), QStringLiteral("p.M.T"));
        QCOMPARE(typeOf(*schema, "p.M.e"), QStringLiteral("p.M.E"));
        QCOMPARE(typeOf(*schema, "p.M.g"), QStringLiteral("p.M.G"));
        QCOMPARE(typeOf(*schema, "p.M.G.t"), QStringLiteral("p.M.T"));
    }

    void extensionFieldsResolveWhereTheyAreWritten()
    {
        TestFiles files;
        files.add("a.proto", "syntax = \"proto2\"; package p;\n"
                             "message Base { extensions 100 to 200; message T {} }\n"
                             "message M { message T {} extend Base { optional T t = 100; } }\n"
                             "message T {}\nextend Base { optional T u = 101; }\n");
        const auto schema = files.load();
        QCOMPARE(typeOf(*schema, "p.M.t"), QStringLiteral("p.M.T"));
        QCOMPARE(typeOf(*schema, "p.u"), QStringLiteral("p.T"));
    }

    void unknownTypeIsLeftUnresolved()
    {
        TestFiles files;
        files.proto("a.proto", "message M { No.Such t = 1; .M self = 2; }");
        const auto schema = files.load();
        QCOMPARE(typeOf(*schema, "M.t"), QStringLiteral("(unresolved)"));
        QCOMPARE(typeOf(*schema, "M.self"), QStringLiteral("M"));
    }

    void methodsFindTheirMessages()
    {
        TestFiles files;
        files.proto("a.proto", "package p; import \"b.proto\"; enum Kind { Z = 0; }\n"
                               "message In { message Deep {} }\n"
                               "service S { rpc A(In) returns (q.Out); rpc B(.p.In.Deep) returns (Kind); rpc C(No) returns (In); }")
            .proto("b.proto", "package q; message Out {}");
        const auto schema = files.load();
        const auto types = [&](const char *name) {
            const Decl *method = findDecl(*schema, QString::fromLatin1(name));
            const auto text = [](const Decl *type) { return type ? type->fullName : QStringLiteral("-"); };
            return text(method->method->requestType) + u' ' + text(method->method->responseType);
        };
        QCOMPARE(types("p.S.A"), QStringLiteral("p.In q.Out"));
        QCOMPARE(types("p.S.B"), QStringLiteral("p.In.Deep -")); // an enum is no message
        QCOMPARE(types("p.S.C"), QStringLiteral("- p.In"));
        QVERIFY(!findDecl(*schema, QStringLiteral("p.S.A"))->type); // nor does it count as use
        QVERIFY(rootNames(*schema).contains(QStringLiteral("p.In")));
    }

    void messageFieldsTrackPresence()
    {
        TestFiles files;
        files.proto("a.proto", "message M { In a = 1; E b = 2; repeated In c = 3; map<string, In> d = 4; No e = 5; }\n"
                               "message In {} enum E { Z = 0; }");
        const auto schema = files.load();
        const auto tracks = [&](const char *name) {
            return findDecl(*schema, QString::fromLatin1(name))->field->tracksPresence;
        };
        QVERIFY(tracks("M.a"));
        QVERIFY(!tracks("M.b"));
        QVERIFY(!tracks("M.c"));
        QVERIFY(!tracks("M.d"));
        QVERIFY(!tracks("M.e"));
    }
};

QTEST_GUILESS_MAIN(TestTypeResolver)
#include "tst_typeresolver.moc"
