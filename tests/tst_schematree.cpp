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

#include "SchemaModel.h"
#include "TestFiles.h"

#include <QAbstractItemModelTester>
#include <QElapsedTimer>

using namespace protoexplorer;
using Node = SchemaTree::Node;

class TestSchemaTree : public QObject
{
    Q_OBJECT

    static std::shared_ptr<const Schema> schemaOf(const QByteArray &body)
    {
        TestFiles files;
        files.proto("a.proto", body);
        return files.load();
    }

    // By labels, from the roots: "Order/items/sku".
    static Node *at(SchemaTree &tree, const QString &path)
    {
        Node *node = nullptr;
        for (const QString &label : path.split(u'/')) {
            const auto &among = node ? tree.children(*node) : tree.roots();
            const auto it = std::find_if(among.begin(), among.end(),
                                         [&](const auto &n) { return n->label == label; });
            if (it == among.end())
                return nullptr;
            node = it->get();
        }
        return node;
    }

    static QString pathOf(const Node *node)
    {
        QStringList labels;
        for (; node; node = node->parent)
            labels.prepend(node->label);
        return labels.join(u'/');
    }

    static QStringList labels(const std::vector<std::unique_ptr<Node>> &nodes)
    {
        QStringList result;
        for (const auto &n : nodes)
            result << n->label;
        return result;
    }

    static QStringList found(SchemaTree &tree)
    {
        QStringList result;
        for (const Node *node : tree.messages())
            result << pathOf(node);
        return result;
    }

    static QStringList parts(const Node *node)
    {
        QStringList result;
        for (const Decl *decl : SchemaTree::sourceParts(*node))
            result << decl->file->text.snippet(decl->span).text;
        return result;
    }

private slots:
    void fieldsOpenIntoTheirTypes()
    {
        SchemaTree tree(schemaOf("message Order { int64 id = 1; repeated Item items = 2; Status s = 3; map<string, Item> m = 4;\n"
                                 "oneof o { Item a = 5; string b = 6; } }\n"
                                 "message Item { string sku = 1; } enum Status { Z = 0; ONE = 1; }\n"
                                 "service Svc { rpc Get(Order) returns (Order); }"));
        QCOMPARE(labels(tree.roots()), QStringList({"Order", "Svc"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order"))), QStringList({"id", "items", "s", "m", "o"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order/items"))), QStringList({"sku"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order/s"))), QStringList({"Z", "ONE"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order/m"))), QStringList({"sku"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order/o"))), QStringList({"a", "b"}));
        QCOMPARE(labels(tree.children(*at(tree, "Order/o/a"))), QStringList({"sku"}));
        QVERIFY(tree.children(*at(tree, "Order/id")).empty());
        QCOMPARE(labels(tree.children(*at(tree, "Svc"))), QStringList({"Get"}));
        QVERIFY(tree.children(*at(tree, "Svc/Get")).empty());
    }

    void recursionStops()
    {
        SchemaTree tree(schemaOf("message A { B b = 1; A self = 2; B again = 3; } message B { A a = 1; oneof o { B me = 2; } }"));
        QVERIFY(at(tree, "A/self")->recursive);
        QVERIFY(tree.children(*at(tree, "A/self")).empty());
        QVERIFY(!at(tree, "A/b")->recursive);
        QVERIFY(at(tree, "A/b/a")->recursive);
        QVERIFY(at(tree, "A/b/o/me")->recursive);
        // Beside is not above.
        QVERIFY(!at(tree, "A/again")->recursive);
        QCOMPARE(labels(tree.children(*at(tree, "A/again"))), QStringList({"a", "o"}));
    }

    void rootLabels()
    {
        TestFiles files;
        files.proto("a.proto", "package p; message Outer { message Inner {} } message Same {} message Twice {}")
            .proto("b.proto", "package q; message Same {}")
            .proto("c.proto", "package p; message Twice {}");
        SchemaTree tree(files.load());
        QCOMPARE(labels(tree.roots()),
                 QStringList({"Outer", "Outer.Inner", "p.Same", "Twice (a.proto:2)", "q.Same", "Twice (c.proto:2)"}));
    }

    void sourceOfAFieldIsItsLineThenItsType()
    {
        SchemaTree tree(schemaOf("message Order {\n  // What.\n  repeated Item items = 2; // yes\n  int32 n = 3;\n  S s = 4;\n}\n"
                                 "// One line.\nmessage Item {\n  string sku = 1;\n}\nenum S { Z = 0; }\n"));
        QCOMPARE(parts(at(tree, "Order/items")),
                 QStringList({"// What.\nrepeated Item items = 2; // yes",
                              "// One line.\nmessage Item {\n  string sku = 1;\n}"}));
        QCOMPARE(parts(at(tree, "Order/n")), QStringList({"int32 n = 3;"}));
        QCOMPARE(parts(at(tree, "Order/s")), QStringList({"S s = 4;", "enum S { Z = 0; }"}));
        QCOMPARE(parts(at(tree, "Order/s/Z")), QStringList({"Z = 0;"}));
        QCOMPARE(parts(at(tree, "Order")).size(), 1);
    }

    void sourceOfAMapFieldIncludesItsValueType()
    {
        SchemaTree tree(schemaOf("message M { map<string, V> m = 1; } message V {}"));
        QCOMPARE(parts(at(tree, "M/m")), QStringList({"map<string, V> m = 1;", "message V {}"}));
    }

    void groupIsShownOnce()
    {
        TestFiles files;
        files.add("a.proto", "syntax = \"proto2\"; message M { optional group G = 1 { optional int32 x = 2; } }");
        SchemaTree tree(files.load());
        QCOMPARE(parts(at(tree, "M/g")).size(), 1);
        QCOMPARE(labels(tree.children(*at(tree, "M/g"))), QStringList({"x"}));
    }

    void extendBlockHoldsItsFieldsAlone()
    {
        TestFiles files;
        files.add("a.proto", "syntax = \"proto2\"; message Base { extensions 100 to max; optional int32 own = 1; }\n"
                             "extend Base { optional Extra e = 100; }\nmessage Extra { optional int32 x = 1; }");
        SchemaTree tree(files.load());
        QCOMPARE(labels(tree.roots()), QStringList({"Base", "Base"}));
        Node *extend = tree.roots()[1].get();
        QCOMPARE(extend->decl->kind, DeclKind::Extend);
        QCOMPARE(labels(tree.children(*extend)), QStringList({"e"}));
        QCOMPARE(parts(extend), QStringList({"extend Base { optional Extra e = 100; }"}));
        QCOMPARE(found(tree), QStringList({"Base", "Base/e"}));
    }

    void messagesAreFoundOnceInTreeOrder()
    {
        SchemaTree tree(schemaOf("message A { B deep = 1; C c = 2; } message B { D d = 1; } message C {} message D { C c = 1; E e = 2; }\n"
                                 "enum E { Z = 0; } message Z { C c = 1; } service S { rpc G(A) returns (A); }"));
        // C is nearest the top as A/c, though it is met sooner going down.
        QCOMPARE(found(tree), QStringList({"A", "A/deep", "A/deep/d", "A/c", "Z"}));
    }

    void messagesUnderOneofsAndImportsAreFound()
    {
        TestFiles files;
        files.proto("a.proto", "import \"google/protobuf/struct.proto\"; message A { oneof o { google.protobuf.Struct s = 1; } }");
        SchemaTree tree(files.load());
        QCOMPARE(found(tree), QStringList({"A", "A/o/s", "A/o/s/fields", "A/o/s/fields/kind/list_value"}));
    }

    void explosiveSchemaIsCheap()
    {
        // Each level refers to the next twice: more nodes than could ever be made.
        QByteArray body;
        for (int i = 0; i < 200; ++i) {
            const QByteArray next = QByteArray::number(i + 1);
            body += "message M" + QByteArray::number(i) + " { M" + next + " a = 1; M" + next + " b = 2; }\n";
        }
        body += "message M200 {}";
        SchemaTree tree(schemaOf(body));
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(tree.messages().size(), size_t(201));
        QVERIFY(timer.elapsed() < 2000);
    }

    void everyMessageIsFoundInRandomSchemas()
    {
        for (uint seed = 1; seed <= 200; ++seed) {
            srand(seed);
            const int count = 1 + rand() % 10;
            QByteArray body;
            for (int i = 0; i < count; ++i) {
                body += "message M" + QByteArray::number(i) + " {";
                for (int f = 1, fields = rand() % 4; f <= fields; ++f)
                    body += " M" + QByteArray::number(rand() % count) + " f" + QByteArray::number(f) + " = " + QByteArray::number(f) + ";";
                body += " }\n";
            }
            SchemaTree tree(schemaOf(body));
            QVERIFY2(int(tree.messages().size()) == count, body.constData());
            for (const Node *node : tree.messages())
                QVERIFY2(!node->recursive, body.constData());
        }
    }

    void modelIsWellFormed()
    {
        SchemaModel model(schemaOf("message A { B b = 1; A self = 2; int32 n = 3; } message B { A a = 1; } message Empty {}\n"
                                   "enum E { Z = 0; NEG = -1; } service S { rpc G(A) returns (A); }"));
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        const QModelIndex a = model.index(0, 0);
        QCOMPARE(a.data().toString(), QStringLiteral("A"));
        QCOMPARE(a.siblingAtColumn(SchemaModel::TypeColumn).data().toString(), QStringLiteral("message"));
        QVERIFY(!a.siblingAtColumn(SchemaModel::NumberColumn).data().isValid());
        const QModelIndex self = model.index(1, 0, a);
        QCOMPARE(self.siblingAtColumn(SchemaModel::TypeColumn).data().toString(), QStringLiteral("A ↻"));
        QVERIFY(!model.hasChildren(self));
        QCOMPARE(model.index(2, SchemaModel::NumberColumn, a).data().toString(), QStringLiteral("3"));
        QVERIFY(!model.hasChildren(model.index(2, 0, a)));
        const QModelIndex empty = model.index(1, 0);
        QCOMPARE(empty.data().toString(), QStringLiteral("Empty"));
        QVERIFY(!model.hasChildren(empty));
        const QModelIndex e = model.index(2, 0);
        QCOMPARE(model.index(1, SchemaModel::NumberColumn, e).data().toString(), QStringLiteral("-1"));
        QVERIFY(a.data(Qt::ToolTipRole).toString().contains(QStringLiteral("a.proto:2")));
    }

    void everyFoundNodeHasItsIndex()
    {
        SchemaModel model(schemaOf("message A { B b = 1; } message B { C c = 1; } message C {}"));
        for (Node *node : model.tree().messages()) {
            QModelIndex index = model.indexFor(node);
            QCOMPARE(model.nodeAt(index), node);
            // And can be got to from the top by asking the model alone.
            QModelIndex parent = index.parent();
            QCOMPARE(model.index(index.row(), 0, parent), index);
        }
    }

    void emptySchema()
    {
        TestFiles files;
        SchemaModel model(files.load());
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.tree().messages().empty());
    }
};

QTEST_GUILESS_MAIN(TestSchemaTree)
#include "tst_schematree.moc"
