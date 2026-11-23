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
#include "JsonView.h"
#include "SchemaModel.h"
#include "SchemaTab.h"
#include "SchemaTreeView.h"
#include "TestFiles.h"

#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
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

        QTabWidget *views() const { return child<QTabWidget>("views"); }
        QStringList titles() const
        {
            QStringList result;
            for (int i = 0; i < views()->count(); ++i)
                result << views()->tabText(i);
            return result;
        }
        JsonView *json() const { return tab->currentJson(); }
        QString jsonText() const { return json()->findChild<QPlainTextEdit *>(QStringLiteral("jsonEdit"))->toPlainText(); }
        QString hint() const
        {
            auto *label = json()->findChild<QLabel *>(QStringLiteral("jsonHintText"));
            return label->isVisible() ? label->text() : QString();
        }
        QString target() const
        {
            const Decl *message = tab->jsonTarget();
            return message ? message->fullName : QString();
        }
        const Decl *message(const char *name) const
        {
            const auto &schema = static_cast<SchemaModel *>(tree()->model())->tree().schema();
            return findDecl(schema, QString::fromLatin1(name));
        }
        void select(const char *path) const
        {
            QModelIndex index;
            for (const QString &name : QString::fromLatin1(path).split(u'/')) {
                QModelIndex found;
                for (int i = 0; i < tree()->model()->rowCount(index); ++i) {
                    if (tree()->model()->index(i, 0, index).data().toString() == name)
                        found = tree()->model()->index(i, 0, index);
                }
                if (!found.isValid())
                    qFatal("no %s", path);
                index = found;
            }
            tree()->setCurrentIndex(index);
            tree()->scrollTo(index);
        }
        QStringList menuHere() const
        {
            emit tree()->customContextMenuRequested(tree()->visualRect(tree()->currentIndex()).center());
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QStringList result;
            if (!menu)
                return result;
            for (const QAction *action : menu->actions())
                result << (action->isSeparator() ? QStringLiteral("-") : action->text());
            menu->close();
            return result;
        }

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

    // JSON

    static void withService(TestFiles &files)
    {
        shop(files);
        files.add("shop/api.proto", "syntax = \"proto2\"; package shop; import \"shop/order.proto\";\n"
                                    "message GetOrder { optional string id = 1; extensions 100 to 200; }\n"
                                    "service Shop { rpc Get(GetOrder) returns (Order); rpc Same(Order) returns (Order); rpc Odd(No) returns (Such); }\n"
                                    "enum Alone { Z = 0; }\nextend GetOrder { optional int32 extra = 100; optional Order order = 101; }\n");
        files.add("order.json", "{\"id\": \"7\", \"items\": [{\"sku\": \"a\"}]}");
    }

    void jsonIsOverWhatIsSelected()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        QVERIFY(o.tab->takesJson());
        const auto target = [&](const char *path) {
            o.select(path);
            return o.target();
        };
        QCOMPARE(target("OrderLog"), QStringLiteral("shop.OrderLog"));
        QCOMPARE(target("OrderLog/order"), QStringLiteral("shop.Order"));
        QCOMPARE(target("OrderLog/order/id"), QStringLiteral("shop.Order"));
        QCOMPARE(target("OrderLog/order/items"), QStringLiteral("shop.Item"));
        QCOMPARE(target("OrderLog/order/who"), QStringLiteral("shop.Order"));
        QCOMPARE(target("OrderLog/order/who/customer"), QStringLiteral("shop.Customer"));
        QCOMPARE(target("Shop"), QString());
        QCOMPARE(target("Shop/Get"), QString());
        QCOMPARE(target("Alone"), QString());
        QCOMPARE(target("Alone/Z"), QString());
        QCOMPARE(target("GetOrder/extra"), QStringLiteral("shop.GetOrder")); // the extend block
        QCOMPARE(target("GetOrder/order"), QStringLiteral("shop.Order"));
    }

    void openingJson()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        QVERIFY(!o.views()->tabBar()->isVisible());
        QVERIFY(!o.json());
        o.select("OrderLog/order");
        o.tab->openJson(files.path("order.json"), o.tab->jsonTarget());
        QVERIFY(o.json());
        QCOMPARE(o.titles(), QStringList({"Schema", "Order ← order.json"}));
        QVERIFY(o.views()->tabBar()->isVisible());
        QCOMPARE(o.views()->currentIndex(), 1);
        QCOMPARE(o.jsonText(), QStringLiteral("{\"id\": \"7\", \"items\": [{\"sku\": \"a\"}]}"));
        QVERIFY(o.hint().isEmpty());
        // From there it is over the same one.
        QCOMPARE(o.target(), QStringLiteral("shop.Order"));
        o.views()->setCurrentIndex(0);
        o.select("OrderLog");
        QCOMPARE(o.target(), QStringLiteral("shop.OrderLog"));
        // The schema is not for closing, but the JSON is.
        QVERIFY(!o.views()->tabBar()->tabButton(0, QTabBar::RightSide) && !o.views()->tabBar()->tabButton(0, QTabBar::LeftSide));
        emit o.views()->tabCloseRequested(0);
        QVERIFY(!o.tab->closeCurrentJson());
        QCOMPARE(o.views()->count(), 2);
        emit o.views()->tabCloseRequested(1);
        QCOMPARE(o.titles(), QStringList({"Schema"}));
        QVERIFY(!o.views()->tabBar()->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void withNothingSelectedItIsOverTheLikeliest()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.select("Alone");
        o.tab->openJson(files.path("order.json"), o.tab->jsonTarget());
        QCOMPARE(o.titles().last(), QStringLiteral("Order ← order.json"));
        // That is a guess, and with nothing to go by it waits.
        files.add("empty.json", "");
        o.tab->openJson(files.path("empty.json"), static_cast<const Decl *>(nullptr));
        o.json()->findChild<QPlainTextEdit *>(QStringLiteral("jsonEdit"))->insertPlainText(QStringLiteral("{\"sku\": \"a\"}"));
        QTRY_COMPARE(o.titles().last(), QStringLiteral("Item ← empty.json"));
        o.tab->closeCurrentJson();
        // Once more, it is the one that is open.
        o.views()->setCurrentIndex(0);
        o.tab->openJson(files.path("order.json"), o.tab->jsonTarget());
        QCOMPARE(o.titles(), QStringList({"Schema", "Order ← order.json"}));
        QCOMPARE(o.views()->currentIndex(), 1);
    }

    void sameJsonAgainIsReadAnew()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        o.views()->setCurrentIndex(0);
        files.add("order.json", "{\"id\": \"8\"}");
        o.tab->openJson(files.path("shop/../order.json"), o.message("shop.Order"));
        QCOMPARE(o.views()->count(), 2);
        QCOMPARE(o.views()->currentIndex(), 1);
        QCOMPARE(o.jsonText(), QStringLiteral("{\"id\": \"8\"}"));
        // Over another message it is something else.
        o.tab->openJson(files.path("order.json"), o.message("shop.Item"));
        QCOMPARE(o.titles(), QStringList({"Schema", "Order ← order.json", "Item ← order.json"}));
        // And what has been written there is not for throwing away.
        o.views()->setCurrentIndex(1);
        o.json()->findChild<QPlainTextEdit *>(QStringLiteral("jsonEdit"))->appendPlainText(QStringLiteral(" "));
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        QCOMPARE(o.views()->count(), 4);
        QCOMPARE(o.views()->currentIndex(), 3);
        // Nor is what cannot be read any more.
        QVERIFY(QFile::remove(files.path("order.json")));
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        QCOMPARE(o.views()->count(), 5);
        o.views()->setCurrentIndex(3);
        QCOMPARE(o.jsonText(), QStringLiteral("{\"id\": \"8\"}"));
    }

    void pastingJson()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        QGuiApplication::clipboard()->setText(QStringLiteral("{\"sku\": \"z\"}"));
        o.tab->pasteJson(o.message("shop.Item"));
        QCOMPARE(o.titles().last(), QStringLiteral("Item ← pasted"));
        QCOMPARE(o.jsonText(), QStringLiteral("{\"sku\": \"z\"}"));
        QGuiApplication::clipboard()->clear();
        o.tab->pasteJson(o.tab->jsonTarget());
        QCOMPARE(o.titles().last(), QStringLiteral("Item ← pasted 2"));
        QCOMPARE(o.jsonText(), QString());
        o.tab->closeCurrentJson();
        o.tab->closeCurrentJson();
        QCOMPARE(o.titles(), QStringList({"Schema"}));
    }

    void titleFollowsTheMessage()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        o.json()->setMessage(o.message("shop.Customer"));
        QCOMPARE(o.titles().last(), QStringLiteral("Customer ← order.json"));
    }

    void jsonThatCannotBeRead()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.tab->openJson(files.path("gone.json"), o.message("shop.Order"));
        QVERIFY2(o.hint().startsWith(QStringLiteral("Cannot read ")) && o.hint().contains(QStringLiteral("gone.json: ")), qPrintable(o.hint()));
        QCOMPARE(o.jsonText(), QString());
        o.tab->openJson(files.path("shop"), o.message("shop.Order"));
        QVERIFY(o.hint().endsWith(QStringLiteral("it is a folder")));
        QFile big(files.path("big.json"));
        QVERIFY(big.open(QIODevice::WriteOnly) && big.resize(65LL * 1024 * 1024));
        o.tab->openJson(files.path("big.json"), o.message("shop.Order"));
        QVERIFY(o.hint().endsWith(QStringLiteral("it is larger than 64 MB")));
    }

    void encodings()
    {
        TestFiles files;
        withService(files);
        files.add("bom.json", "\xEF\xBB\xBF{\"sku\": \"\xD0\xB6\"}");
        files.add("wide.json", QByteArray::fromHex("fffe7b007d00"));
        Opened o(files.path());
        o.tab->openJson(files.path("bom.json"), o.message("shop.Item"));
        QCOMPARE(o.jsonText(), QStringLiteral("{\"sku\": \"ж\"}"));
        o.tab->openJson(files.path("wide.json"), o.message("shop.Item"));
        QCOMPARE(o.jsonText(), QStringLiteral("{}"));
    }

    void jsonByTheNameOfItsMessage()
    {
        TestFiles files;
        withService(files);
        // Which is not the first of them.
        files.proto("z/item.proto", "package other; message Item { string sku = 1; int32 weight = 2; }");
        files.add("item.json", "{\"sku\": \"a\", \"weight\": 1}");
        SchemaTab tab(files.path());
        tab.resize(900, 600);
        tab.show();
        // Before there is a schema, and after.
        tab.openJson(files.path("order.json"), QStringLiteral("shop.Customer"));
        tab.openJson(files.path("item.json"), QStringLiteral("Item"));
        QVERIFY(!tab.currentJson());
        QSignalSpy spy(&tab, &SchemaTab::loaded);
        QVERIFY(spy.wait());
        auto *views = tab.findChild<QTabWidget *>(QStringLiteral("views"));
        const auto hint = [&] {
            auto *label = tab.currentJson()->findChild<QLabel *>(QStringLiteral("jsonHintText"));
            return label->isVisible() ? label->text() : QString();
        };
        QCOMPARE(views->count(), 3);
        QCOMPARE(views->tabText(1), QStringLiteral("Customer ← order.json"));
        QCOMPARE(tab.currentJson()->message()->fullName, QStringLiteral("other.Item"));
        QCOMPARE(hint(), QStringLiteral("2 messages are called Item, and this is the likeliest."));
        tab.openJson(files.path("order.json"), QStringLiteral("Order"));
        QCOMPARE(tab.currentJson()->message()->fullName, QStringLiteral("shop.Order"));
        QCOMPARE(hint(), QString());
        tab.openJson(files.path("item.json"), QStringLiteral("shop.Nope"));
        QCOMPARE(tab.currentJson()->message()->fullName, QStringLiteral("other.Item"));
        QCOMPARE(hint(), QStringLiteral("There is no message shop.Nope here, so this is the likeliest."));
        tab.openJson(files.path("order.json"), QString());
        QCOMPARE(tab.currentJson()->message()->fullName, QStringLiteral("shop.Order"));
        // Likelier ones that are not called that are not asked for.
        tab.openJson(files.path("order.json"), QStringLiteral("Item"));
        QCOMPARE(tab.currentJson()->message()->name, QStringLiteral("Item"));
        tab.closeCurrentJson();
        // Again, it is the same one, with no more to be said of it than there is.
        const int count = views->count();
        tab.openJson(files.path("item.json"), QStringLiteral("Item"));
        QCOMPARE(hint(), QStringLiteral("2 messages are called Item, and this is the likeliest."));
        tab.openJson(files.path("item.json"), QStringLiteral("other.Item"));
        QCOMPARE(hint(), QString());
        QCOMPARE(views->count(), count);
        // That it cannot be read is what matters most.
        tab.openJson(files.path("gone.json"), QStringLiteral("shop.Nope"));
        QVERIFY(hint().startsWith(QStringLiteral("Cannot read")));
        tab.openJson(files.path("gone.json"), QStringLiteral("Item"));
        QVERIFY(hint().startsWith(QStringLiteral("Cannot read")));
    }

    void jsonWhereThereIsNoMessage()
    {
        TestFiles files;
        files.proto("a.proto", "enum E { Z = 0; }").add("a.json", "{}");
        SchemaTab tab(files.path());
        tab.show();
        QVERIFY(!tab.takesJson());
        tab.openJson(files.path("a.json"), QString());
        QSignalSpy spy(&tab, &SchemaTab::loaded);
        QVERIFY(spy.wait());
        QVERIFY(!tab.takesJson());
        QVERIFY(!tab.currentJson());
        auto *list = tab.findChild<QListWidget *>(QStringLiteral("problemList"));
        QVERIFY(list->isVisible());
        QVERIFY(list->item(0)->text().contains(QStringLiteral("no message here")));
        tab.pasteJson(nullptr);
        tab.openJson(files.path("a.json"), static_cast<const Decl *>(nullptr));
        QVERIFY(!tab.currentJson());
    }

    void menus()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        const auto menu = [&](const char *path) {
            o.select(path);
            return o.menuHere();
        };
        QCOMPARE(menu("OrderLog/order"), QStringList({"Map JSON onto Order…", "Paste JSON as Order"}));
        QCOMPARE(menu("OrderLog/order/id"), QStringList({"Map JSON onto Order…", "Paste JSON as Order"}));
        QCOMPARE(menu("Shop/Get"), QStringList({"Map JSON onto GetOrder…", "Paste JSON as GetOrder", "-",
                                                "Map JSON onto Order…", "Paste JSON as Order"}));
        QCOMPARE(menu("Shop/Same"), QStringList({"Map JSON onto Order…", "Paste JSON as Order"}));
        QCOMPARE(menu("Shop/Odd"), QStringList());
        QCOMPARE(menu("Alone"), QStringList());
        QCOMPARE(menu("Shop"), QStringList());
    }

    void menuActions()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.select("Shop/Get");
        const auto trigger = [&](int i) {
            emit o.tree()->customContextMenuRequested(o.tree()->visualRect(o.tree()->currentIndex()).center());
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            menu->actions().at(i)->trigger();
            menu->close();
        };
        QSignalSpy wanted(o.tab.get(), &SchemaTab::jsonWanted);
        trigger(3);
        QCOMPARE(wanted.count(), 1);
        QCOMPARE(wanted.at(0).at(0).value<const Decl *>(), o.message("shop.Order"));
        QGuiApplication::clipboard()->setText(QStringLiteral("{}"));
        trigger(1);
        QCOMPARE(o.titles().last(), QStringLiteral("GetOrder ← pasted"));
    }

    void dropping()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.select("OrderLog/order/items");
        const QPoint at = o.tree()->visualRect(o.tree()->currentIndex()).center();
        o.select("OrderLog");
        const auto drop = [&](const QList<QUrl> &urls, const QPoint &where) {
            QMimeData data;
            data.setUrls(urls);
            QDragEnterEvent enter(where, Qt::CopyAction, &data, Qt::LeftButton, {});
            QCoreApplication::sendEvent(o.tree()->viewport(), &enter);
            QDragMoveEvent move(where, Qt::CopyAction, &data, Qt::LeftButton, {});
            QCoreApplication::sendEvent(o.tree()->viewport(), &move);
            QDropEvent dropped(where, Qt::CopyAction, &data, Qt::LeftButton, {});
            QCoreApplication::sendEvent(o.tree()->viewport(), &dropped);
            return enter.isAccepted() && move.isAccepted() && dropped.isAccepted();
        };
        const QUrl json = QUrl::fromLocalFile(files.path("order.json"));
        QVERIFY(!drop({QUrl::fromLocalFile(files.path("shop/order.proto"))}, at));
        QVERIFY(!drop({QUrl(QStringLiteral("https://example.com/order.json"))}, at));
        QCOMPARE(o.current(), QStringLiteral("OrderLog"));
        QVERIFY(!o.json());
        QVERIFY(drop({json}, at));
        QCOMPARE(o.current(), QStringLiteral("OrderLog/order/items")); // shown where it would go
        QCOMPARE(o.titles(), QStringList({"Schema", "Item ← order.json"}));
        // Onto nothing in particular, it is for the likeliest.
        o.views()->setCurrentIndex(0);
        QVERIFY(drop({json, QUrl::fromLocalFile(files.path("shop/order.proto"))}, QPoint(20, o.tree()->viewport()->height() - 2)));
        QCOMPARE(o.titles().last(), QStringLiteral("Order ← order.json"));
    }

    void findGoesToWhatIsShowing()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.type("Customer");
        const QString before = o.current();
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        QVERIFY(QTest::qWaitForWindowActive(o.tab.get()));
        o.tab->activateFind();
        auto *edit = o.json()->findChild<QLineEdit *>(QStringLiteral("findEdit"));
        QTRY_VERIFY(edit->hasFocus());
        edit->setText(QStringLiteral("sku"));
        o.tab->find(true);
        QCOMPARE(o.json()->findChild<QLabel *>(QStringLiteral("findStatus"))->text(), QStringLiteral("1 of 1"));
        QCOMPARE(o.current(), before);
        o.views()->setCurrentIndex(0);
        o.tab->activateFind();
        QTRY_VERIFY(o.views()->widget(0)->findChild<QLineEdit *>(QStringLiteral("findEdit"))->hasFocus());
    }

    void closingTheTabClosesItsJson()
    {
        TestFiles files;
        withService(files);
        Opened o(files.path());
        o.tab->openJson(files.path("order.json"), o.message("shop.Order"));
        o.json()->findChild<QPlainTextEdit *>(QStringLiteral("jsonEdit"))->appendPlainText(QStringLiteral(" "));
        o.tab.reset(); // with a change still to be laid out
        QCoreApplication::processEvents();
    }
};

QTEST_MAIN(TestSchemaTab)
#include "tst_schematab.moc"
