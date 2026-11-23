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

#include "FindBar.h"
#include "SchemaModel.h"
#include "SchemaTab.h"
#include "SchemaTreeView.h"
#include "TestFiles.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QToolButton>

using namespace protoexplorer;

class TestSchemaTab : public QObject
{
    Q_OBJECT

    struct Opened
    {
        explicit Opened(const QString &path)
            : tab(std::make_unique<SchemaTab>(path))
        {
            tab->resize(900, 600);
            tab->show();
            QSignalSpy spy(tab.get(), &SchemaTab::loaded);
            if (tab->isLoading() && !spy.wait())
                qFatal("not loaded");
        }

        template<typename T> T *child(const char *name) const
        {
            T *found = tab->findChild<T *>(QString::fromLatin1(name));
            if (!found)
                qFatal("no %s", name);
            return found;
        }
        SchemaTreeView *tree() const { return child<SchemaTreeView>("schemaTree"); }
        QString current() const
        {
            QStringList labels;
            for (QModelIndex i = tree()->currentIndex(); i.isValid(); i = i.parent())
                labels.prepend(i.siblingAtColumn(0).data().toString());
            return labels.join(u'/');
        }
        void type(const QString &text) const { child<QLineEdit>("findEdit")->setText(text); }
        QString status() const { return child<QLabel>("findStatus")->text(); }
        FindBar *bar() const { return tab->findChild<FindBar *>(); }
        QString source(int n) const { return child<QPlainTextEdit>(n == 1 ? "sourceText1" : "sourceText2")->toPlainText(); }
        QString header(int n) const { return child<QLabel>(n == 1 ? "sourceHeader1" : "sourceHeader2")->text(); }
        bool second() const { return child<QPlainTextEdit>("sourceText2")->isVisible(); }

        std::unique_ptr<SchemaTab> tab;
    };

    static void shop(TestFiles &files)
    {
        files.proto("shop/order.proto", "package shop;\nimport \"shop/item.proto\";\n"
                                        "// An order.\nmessage Order {\n  int64 id = 1;\n  repeated Item items = 2; // what\n  oneof who { Customer customer = 3; }\n}\n"
                                        "message Customer { Order last = 1; }\nmessage OrderLog { Order order = 1; }\n")
            .proto("shop/item.proto", "package shop;\n// One line.\nmessage Item {\n  string sku = 1;\n}\n");
    }

private slots:
    void opensOnTheFirstRoot()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
        QCOMPARE(o.source(1), QStringLiteral("message OrderLog { Order order = 1; }"));
        QVERIFY(!o.second());
        QVERIFY(o.tree()->isExpanded(o.tree()->model()->index(0, 0)));
        QCOMPARE(o.child<QLabel>("summary")->text(), QStringLiteral("2 files, 1 root"));
        QVERIFY(!o.child<QToolButton>("problems")->isVisible());
    }

    void messageFieldShowsBothDeclarations()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        o.type("shop.Item");
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order/items"));
        QCOMPARE(o.source(1), QStringLiteral("repeated Item items = 2; // what"));
        QCOMPARE(o.header(1), QStringLiteral("shop/order.proto : 7"));
        QVERIFY(o.second());
        QCOMPARE(o.source(2), QStringLiteral("// One line.\nmessage Item {\n  string sku = 1;\n}"));
        QCOMPARE(o.header(2), QStringLiteral("shop/item.proto : 3\u20136"));

        // And a scalar's goes back to one.
        o.tree()->setCurrentIndex(o.tree()->model()->index(0, 0, o.tree()->currentIndex()));
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order/items/sku"));
        QCOMPARE(o.source(1), QStringLiteral("string sku = 1;"));
        QVERIFY(!o.second());
    }

    void longHeaderKeepsItsEnd()
    {
        TestFiles files;
        files.proto(QStringLiteral("long/").repeated(60) + QStringLiteral("name.proto"), "message A {}");
        Opened o(files.path());
        QTRY_VERIFY(o.header(1).endsWith(QStringLiteral("/name.proto : 2")));
        QVERIFY(o.header(1).startsWith(QChar(0x2026)));
        o.tab->resize(400, 400);
        QCoreApplication::processEvents();
        QVERIFY(o.header(1).endsWith(QStringLiteral("name.proto : 2")));
        QVERIFY(o.tab->width() < 700);
    }

    void anyColumnShowsItsRow()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        const QModelIndex order = o.tree()->model()->index(0, 0, o.tree()->model()->index(0, 0));
        o.tree()->setCurrentIndex(order.siblingAtColumn(1));
        QCOMPARE(o.source(1), QStringLiteral("Order order = 1;"));
        QVERIFY(o.second());
    }

    void typingPrefersAnExactName()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        o.type("ord");
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
        QCOMPARE(o.status(), QStringLiteral("1 of 2"));
        o.type("order");
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order"));
        QCOMPARE(o.status(), QStringLiteral("2 of 2"));
    }

    void nextAndPreviousWrapInTreeOrder()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        o.type("shop.");
        QStringList seen{o.current()};
        for (int i = 0; i < 4; ++i) {
            QTest::keyClick(o.child<QLineEdit>("findEdit"), Qt::Key_Return);
            seen << o.current();
        }
        const QStringList order{"OrderLog", "OrderLog/order", "OrderLog/order/items",
                                "OrderLog/order/who/customer", "OrderLog"};
        QCOMPARE(seen, order);
        QCOMPARE(o.status(), QStringLiteral("1 of 4"));
        QTest::keyClick(o.child<QLineEdit>("findEdit"), Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(o.current(), order[3]);
        QTest::mouseClick(o.child<QToolButton>("findPrevious"), Qt::LeftButton);
        QCOMPARE(o.current(), order[2]);
        QTest::mouseClick(o.child<QToolButton>("findNext"), Qt::LeftButton);
        QCOMPARE(o.current(), order[3]);
        QVERIFY(o.tree()->viewport()->rect().contains(
            o.tree()->visualRect(o.tree()->currentIndex()).center()));
    }

    void nextGoesOnFromWhereTheReaderIs()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        o.type("shop.");
        const auto *model = static_cast<SchemaModel *>(o.tree()->model());
        // On a row that is not a match, between two that are.
        const QModelIndex order = model->index(0, 0, model->index(0, 0));
        o.tree()->setCurrentIndex(model->index(0, 0, order));
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order/id"));
        o.tab->find(true);
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order/items"));
        o.tree()->setCurrentIndex(model->index(0, 0, order));
        o.tab->find(false);
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order"));
    }

    void noMatchLeavesTheSelection()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        o.type("nothing like it");
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
        QCOMPARE(o.status(), QStringLiteral("No matches"));
        QVERIFY(o.bar()->isFailed());
        o.tab->find(true);
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
        o.type(QString());
        QVERIFY(!o.bar()->isFailed());
        QCOMPARE(o.status(), QString());
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
    }

    void escapeGoesToTheTree()
    {
        TestFiles files;
        shop(files);
        Opened o(files.path());
        QVERIFY(QTest::qWaitForWindowActive(o.tab.get()));
        o.tab->activateFind();
        QTRY_VERIFY(o.child<QLineEdit>("findEdit")->hasFocus());
        QTest::keyClick(o.child<QLineEdit>("findEdit"), Qt::Key_Escape);
        QTRY_VERIFY(o.tree()->hasFocus());
    }

    void asteriskDoesNotExpandEverything()
    {
        // Were it to, this would not come back.
        TestFiles files;
        QByteArray body;
        for (int i = 0; i < 60; ++i) {
            const QByteArray next = QByteArray::number(i + 1);
            body += "message M" + QByteArray::number(i) + " { M" + next + " a = 1; M" + next + " b = 2; }\n";
        }
        files.proto("a.proto", body + "message M60 {}");
        Opened o(files.path());
        QTest::keyClick(o.tree(), Qt::Key_Asterisk);
        o.type("M60");
        QCOMPARE(o.current().count(u'/'), 60);
    }

    void problemsAreListed()
    {
        TestFiles files;
        files.proto("a.proto", "import \"gone.proto\"; message A {}\nmessage B { int32 = 1; }");
        Opened o(files.path());
        auto *button = o.child<QToolButton>("problems");
        auto *list = o.child<QListWidget>("problemList");
        QVERIFY(button->isVisible());
        QVERIFY(!list->isVisible());
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(list->isVisible());
        QCOMPARE(list->count(), 3);
        QVERIFY(list->item(0)->text().startsWith(QStringLiteral("a.proto:3: ")));
        // The rest of the file is there all the same.
        QCOMPARE(o.current(), QStringLiteral("A"));
    }

    void problemsShowThemselvesWhenThereIsNothingElse()
    {
        TestFiles files;
        Opened o(files.path("gone.proto"));
        QVERIFY(o.child<QListWidget>("problemList")->isVisible());
        QCOMPARE(o.child<QToolButton>("problems")->text(), QStringLiteral("1 problem"));
        QCOMPARE(o.source(1), QString());
        o.type("x");
        QCOMPARE(o.status(), QStringLiteral("No matches"));
    }

    void closingWhileLoadingIsSafe()
    {
        TestFiles files;
        for (int i = 0; i < 200; ++i)
            files.proto(QStringLiteral("f%1.proto").arg(i), "message M" + QByteArray::number(i) + " { M1 a = 1; }");
        for (int i = 0; i < 20; ++i) {
            SchemaTab tab(files.path());
            if (i % 2)
                QTest::qWait(i / 4);
        }
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
    }

    void findBeforeLoadingIsHarmless()
    {
        TestFiles files;
        shop(files);
        SchemaTab tab(files.path());
        QVERIFY(tab.isLoading());
        tab.activateFind();
        tab.find(true);
        tab.findChild<QLineEdit *>(QStringLiteral("findEdit"))->setText(QStringLiteral("Item"));
        QSignalSpy spy(&tab, &SchemaTab::loaded);
        QVERIFY(spy.wait());
        tab.find(true);
        QCOMPARE(tab.findChild<QLabel *>(QStringLiteral("findStatus"))->text(), QStringLiteral("1 of 1"));
    }
};

QTEST_MAIN(TestSchemaTab)
#include "tst_schematab.moc"
