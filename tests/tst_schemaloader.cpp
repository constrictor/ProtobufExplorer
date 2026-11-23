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

class TestSchemaLoader : public QObject
{
    Q_OBJECT

    static QStringList shown(const Schema &schema, bool opened)
    {
        QStringList result;
        for (const auto &file : schema.files) {
            if (file->opened == opened)
                result << file->displayPath;
        }
        return result;
    }

private slots:
    void folderIsScannedRecursively()
    {
        TestFiles files;
        files.proto("b.proto", "message B {}").proto("sub/deep/a.proto", "message A {}")
            .proto("sub/c.pb", "message C {}").add("readme.txt", "message D {}")
            .proto(".git/hidden.proto", "message E {}").proto("UP.PROTO", "message F {}");
        const auto schema = files.load();
        QCOMPARE(shown(*schema, true), QStringList({"UP.PROTO", "b.proto", "sub/c.pb", "sub/deep/a.proto"}));
        QCOMPARE(allNotes(*schema), QString());
    }

    void singleFileOpensOnlyItself()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}").proto("b.proto", "message B {}");
        const auto schema = files.load("a.proto");
        QCOMPARE(shown(*schema, true), QStringList({"a.proto"}));
        QCOMPARE(rootNames(*schema), QStringList({"A"}));
    }

    void anyExtensionOpensByName()
    {
        TestFiles files;
        files.proto("schema.txt", "message A {}");
        QCOMPARE(rootNames(*files.load("schema.txt")), QStringList({"A"}));
    }

    void binaryPbIsSkipped()
    {
        TestFiles files;
        files.add("data.pb", QByteArray("\x0a\x05hello\x10\x01", 9)).proto("a.proto", "message A {}");
        const auto schema = files.load();
        QCOMPARE(shown(*schema, true), QStringList({"a.proto"}));
        QCOMPARE(schema->notes.size(), size_t(1));
        QVERIFY(schema->notes[0].text.contains(QStringLiteral("binary")));
        QCOMPARE(schema->notes[0].path, QStringLiteral("data.pb"));
    }

    void textFormatPbGetsOneNote()
    {
        TestFiles files;
        files.add("graph.pb", "node {\n  name: \"x\"\n  op: \"y\"\n}\nnode {\n  name: \"z\"\n}\n");
        const auto schema = files.load();
        QVERIFY(schema->files.empty());
        QCOMPARE(schema->notes.size(), size_t(1));
    }

    void oddBytesDoNotHideAProto()
    {
        TestFiles files;
        files.proto("a.proto", QByteArray("// \x01\nmessage A {}"));
        QCOMPARE(rootNames(*files.load()), QStringList({"A"}));
    }

    void linkedFileIsOpenedOnce()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}");
        QVERIFY(QFile::link(files.path("a.proto"), files.path("z.proto")));
        const auto schema = files.load();
        QCOMPARE(shown(*schema, true), QStringList({"a.proto"}));
        QCOMPARE(rootNames(*schema), QStringList({"A"}));
    }

    void importIsFoundAboveTheFile()
    {
        TestFiles files;
        files.proto("proto/shop/order.proto", "package shop; import \"common/money.proto\";\nmessage Order { common.Money m = 1; }")
            .proto("proto/common/money.proto", "package common; message Money { int64 units = 1; }");
        const auto schema = files.load("proto/shop/order.proto");
        QCOMPARE(shown(*schema, true), QStringList({"order.proto"}));
        QCOMPARE(shown(*schema, false).size(), 1);
        QCOMPARE(allNotes(*schema), QString());
        const Decl *m = findDecl(*schema, "shop.Order.m");
        QVERIFY(m && m->type);
        QCOMPARE(m->type->fullName, QStringLiteral("common.Money"));
        QCOMPARE(m->type->file->displayPath, QStringLiteral("../common/money.proto"));
        QCOMPARE(rootNames(*schema), QStringList({"shop.Order"}));
    }

    void importIsFoundOffToTheSide()
    {
        TestFiles files;
        files.proto("app/x.proto", "import \"common/money.proto\"; message X { Money m = 1; }")
            .proto("lib/proto/common/money.proto", "message Money {}");
        const auto schema = files.load();
        QCOMPARE(allNotes(*schema), QString());
        QCOMPARE(rootNames(*schema), QStringList({"X"}));
    }

    void importsOfImportsAreLoaded()
    {
        TestFiles files;
        files.proto("in/a.proto", "import \"b.proto\"; message A { B b = 1; }")
            .proto("b.proto", "import \"c.proto\"; message B { C c = 1; }")
            .proto("c.proto", "message C {}");
        const auto schema = files.load("in");
        QCOMPARE(shown(*schema, true), QStringList({"a.proto"}));
        QCOMPARE(shown(*schema, false).size(), 2);
        QVERIFY(findDecl(*schema, "B.c")->type);
    }

    void importCycleEnds()
    {
        TestFiles files;
        files.proto("a.proto", "import \"b.proto\"; message A { B b = 1; }")
            .proto("b.proto", "import \"a.proto\"; message B { A a = 1; }");
        const auto schema = files.load("a.proto");
        QCOMPARE(schema->files.size(), size_t(2));
        QCOMPARE(rootNames(*schema), QStringList({"A"}));
    }

    void wellKnownTypesAreBundled()
    {
        TestFiles files;
        files.proto("a.proto", "import \"google/protobuf/timestamp.proto\";\nmessage A { google.protobuf.Timestamp t = 1; }");
        const auto schema = files.load();
        QCOMPARE(allNotes(*schema), QString());
        const Decl *t = findDecl(*schema, "A.t");
        QVERIFY(t && t->type);
        QCOMPARE(t->type->members.size(), size_t(2));
        QCOMPARE(t->type->file->displayPath, QStringLiteral("google/protobuf/timestamp.proto (bundled)"));
        QCOMPARE(rootNames(*schema), QStringList({"A"}));
    }

    void everyBundledFileLoads_data()
    {
        QTest::addColumn<QString>("name");
        for (const char *name : {"any", "api", "compiler/plugin", "cpp_features", "descriptor", "duration",
                                 "empty", "field_mask", "source_context", "struct", "timestamp", "type",
                                 "wrappers"}) {
            QTest::newRow(name) << QStringLiteral("google/protobuf/%1.proto").arg(QLatin1String(name));
        }
    }

    void everyBundledFileLoads()
    {
        QFETCH(QString, name);
        TestFiles files;
        files.proto("a.proto", "import \"" + name.toUtf8() + "\"; message A {}");
        const auto schema = files.load();
        QCOMPARE(allNotes(*schema), QString());
        for (const auto &file : schema->files)
            QVERIFY2(file->notes.empty(), qPrintable(file->displayPath));
    }

    void missingImportIsANote()
    {
        TestFiles files;
        files.proto("a.proto", "import \"nowhere.proto\"; message A { Gone g = 1; int32 x = 2; }");
        const auto schema = files.load();
        QCOMPARE(schema->notes.size(), size_t(1));
        QVERIFY(schema->notes[0].text.contains(QStringLiteral("nowhere.proto")));
        QCOMPARE(rootNames(*schema), QStringList({"A"}));
        QVERIFY(!findDecl(*schema, "A.g")->type);
    }

    void unsafeImportsAreNotFollowed_data()
    {
        QTest::addColumn<QByteArray>("import");
        QTest::newRow("absolute") << QByteArray("/etc/passwd");
        QTest::newRow("parent") << QByteArray("../up.proto");
        QTest::newRow("parent inside") << QByteArray("x/../../up.proto");
        QTest::newRow("resource") << QByteArray(":/protobuf/google/protobuf/any.proto");
        QTest::newRow("empty") << QByteArray("");
    }

    void unsafeImportsAreNotFollowed()
    {
        QFETCH(QByteArray, import);
        TestFiles files;
        files.proto("up.proto", "message Up {}").proto("in/x/y.txt", "")
            .proto("in/a.proto", "import \"" + import + "\"; message A {}");
        const auto schema = files.load("in");
        QCOMPARE(schema->files.size(), size_t(1));
        QCOMPARE(schema->notes.size(), size_t(1));
    }

    void syntaxErrorIsNotedAgainstItsFile()
    {
        TestFiles files;
        files.proto("bad.proto", "message A { int32 = 1; }\nmessage B {}");
        const auto schema = files.load();
        QVERIFY(!schema->notes.empty());
        QCOMPARE(schema->notes[0].path, QStringLiteral("bad.proto"));
        QCOMPARE(schema->notes[0].line, 2);
        QVERIFY(rootNames(*schema).contains(QStringLiteral("B")));
    }

    void nothingThere()
    {
        TestFiles files;
        QCOMPARE(files.load("nope.proto")->notes.size(), size_t(1));
        QDir().mkpath(files.path("empty"));
        QCOMPARE(files.load("empty")->notes.size(), size_t(1));
    }

    void cancelStops()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}");
        QVERIFY(!loadSchema(files.path(), [] { return true; }));
        int asked = 0;
        QVERIFY(!loadSchema(files.path(), [&] { return ++asked > 3; }));
        QVERIFY(loadSchema(files.path(), [] { return false; }));
    }
};

QTEST_GUILESS_MAIN(TestSchemaLoader)
#include "tst_schemaloader.moc"
