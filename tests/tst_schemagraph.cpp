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

#include "SchemaGraph.h"
#include "TestFiles.h"

#include <QSet>

using namespace protoexplorer;

class TestSchemaGraph : public QObject
{
    Q_OBJECT

    static QStringList roots(const QByteArray &body)
    {
        TestFiles files;
        files.proto("a.proto", body);
        return rootNames(*files.load());
    }

private slots:
    void usedMessagesAreNotRoots()
    {
        QCOMPARE(roots("message Item {} message Order { repeated Item items = 1; Who who = 2; } message Who { Item i = 1; }"),
                 QStringList({"Order"}));
    }

    void everyWayOfUsingCounts()
    {
        QCOMPARE(roots("message A {} message B {} message C {} message D { message E {} E e = 1; }\n"
                       "message R { map<string, A> a = 1; oneof o { B b = 2; } optional C c = 3; D d = 4; }"),
                 QStringList({"R"}));
    }

    void selfUseDoesNotCount()
    {
        QCOMPARE(roots("message Tree { repeated Tree kids = 1; }"), QStringList({"Tree"}));
    }

    void circleIsHeadedByItsFirst()
    {
        QCOMPARE(roots("message A { B b = 1; } message B { A a = 1; }"), QStringList({"A"}));
        QCOMPARE(roots("message B { A a = 1; } message A { B b = 1; }"), QStringList({"B"}));
    }

    void circleUsedFromOutsideHasNoHead()
    {
        QCOMPARE(roots("message A { B b = 1; } message B { A a = 1; } message R { B b = 1; }"),
                 QStringList({"R"}));
    }

    void circleLeadingToACircle()
    {
        // Declared back to front, so that the order alone would pick the wrong one.
        QCOMPARE(roots("message C { D d = 1; } message D { C c = 1; } message A { B b = 1; } message B { A a = 1; C c = 2; }"),
                 QStringList({"A"}));
    }

    void rpcUseDoesNotCount()
    {
        QCOMPARE(roots("message Req {} message Res {} service S { rpc Get(Req) returns (Res); }"),
                 QStringList({"Req", "Res", "S"}));
    }

    void enums()
    {
        QCOMPARE(roots("enum Used { A = 0; } enum Unused { B = 0; } message M { Used u = 1; }"),
                 QStringList({"Unused", "M"}));
    }

    void unusedNestedTypesAreRoots()
    {
        QCOMPARE(roots("message Outer { message Used {} message Unused {} Used u = 1; enum E { Z = 0; } }"),
                 QStringList({"Outer", "Outer.Unused", "Outer.E"}));
    }

    void extendBlocks()
    {
        QCOMPARE(roots("import \"google/protobuf/descriptor.proto\"; message Opt { Deep d = 1; } message Deep {}\n"
                       "extend google.protobuf.FieldOptions { Opt opt = 50000; }"),
                 QStringList({"extend google.protobuf.FieldOptions"}));
    }

    void duplicatesAreAllRoots()
    {
        TestFiles files;
        files.proto("one.proto", "message T {}").proto("two.proto", "message T {}");
        QCOMPARE(rootNames(*files.load()), QStringList({"T", "T"}));
    }

    void duplicateInOneFileIsNotLost()
    {
        QCOMPARE(roots("message T { int32 a = 1; } message T { int32 b = 1; } message M { T t = 1; }"),
                 QStringList({"T", "M"}));
    }

    void importedFilesAddNoRoots()
    {
        TestFiles files;
        files.proto("in/a.proto", "import \"b.proto\"; message A { B b = 1; }")
            .proto("b.proto", "message B {} message Other {} service S {} enum E { Z = 0; }");
        QCOMPARE(rootNames(*files.load("in")), QStringList({"A"}));
    }

    void useByAnImportedMessageCountsOnlyIfItIsReached()
    {
        // B, outside, uses C, inside. Reached through A it accounts for C; not reached, C
        // would be nowhere to be found.
        const QByteArray b = "import \"c.proto\"; message B { C c = 1; }";
        TestFiles reached;
        reached.proto("in/a.proto", "import \"b.proto\"; message A { B b = 1; }")
            .proto("in/c.proto", "message C {}").proto("b.proto", b);
        QCOMPARE(rootNames(*reached.load("in")), QStringList({"A"}));

        TestFiles unreached;
        unreached.proto("in/a.proto", "import \"b.proto\"; message A {}")
            .proto("in/c.proto", "message C {}").proto("b.proto", b);
        QCOMPARE(rootNames(*unreached.load("in")), QStringList({"A", "C"}));
    }

    void useByAnImportedExtensionDoesNotCount()
    {
        TestFiles files;
        files.proto("in/a.proto", "import \"x.proto\"; message A {}")
            .proto("x.proto", "import \"in/a.proto\"; import \"google/protobuf/descriptor.proto\";\n"
                              "extend google.protobuf.FieldOptions { A a = 50000; }");
        QCOMPARE(rootNames(*files.load("in")), QStringList({"A"}));
    }

    void orderFollowsTheFiles()
    {
        TestFiles files;
        files.proto("b.proto", "message B2 {} service S {} message B1 {}").proto("a.proto", "message Z {} message Y {}");
        QCOMPARE(rootNames(*files.load()), QStringList({"Z", "Y", "B2", "S", "B1"}));
    }

    void longChainDoesNotOverflow()
    {
        QByteArray body;
        const int length = 20000;
        for (int i = 0; i < length; ++i)
            body += "message M" + QByteArray::number(i) + " { M" + QByteArray::number(i + 1) + " n = 1; }\n";
        QCOMPARE(roots(body), QStringList({"M0"}));
    }

    // Whatever the shape, nothing opened may be out of reach, and no root may lead to
    // another, or it would be in the tree twice over.
    void randomSchemas()
    {
        for (uint seed = 1; seed <= 300; ++seed) {
            srand(seed);
            const int count = 1 + rand() % 12;
            QByteArray body;
            for (int i = 0; i < count; ++i) {
                body += "message M" + QByteArray::number(i) + " {";
                const int fields = rand() % 4;
                for (int f = 1; f <= fields; ++f) {
                    body += " M" + QByteArray::number(rand() % count) + " f" + QByteArray::number(f)
                        + " = " + QByteArray::number(f) + ";";
                }
                body += " }\n";
            }
            TestFiles files;
            files.proto("a.proto", body);
            const auto schema = files.load();

            const auto reach = [](const Decl *from) {
                QSet<const Decl *> seen{from};
                std::vector<const Decl *> todo{from};
                while (!todo.empty()) {
                    const Decl *d = todo.back();
                    todo.pop_back();
                    for (const Decl *t : usedTypes(*d)) {
                        if (!seen.contains(t)) {
                            seen.insert(t);
                            todo.push_back(t);
                        }
                    }
                }
                return seen;
            };
            QSet<const Decl *> covered;
            for (const Decl *root : schema->roots) {
                const auto reached = reach(root);
                for (const Decl *other : schema->roots)
                    QVERIFY2(other == root || !reached.contains(other), body.constData());
                covered.unite(reached);
            }
            QVERIFY2(covered.size() == count, body.constData());
        }
    }
};

QTEST_GUILESS_MAIN(TestSchemaGraph)
#include "tst_schemagraph.moc"
