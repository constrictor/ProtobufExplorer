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

#include "JsonView.h"
#include "MainWindow.h"
#include "SchemaTab.h"
#include "TestFiles.h"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenuBar>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>

using namespace protoexplorer;

class TestMainWindow : public QObject
{
    Q_OBJECT

    static QAction *action(MainWindow &window, const QKeySequence &key)
    {
        for (QAction *a : window.findChildren<QAction *>()) {
            if (a->shortcut() == key)
                return a;
        }
        return nullptr;
    }

private slots:
    void eachOpeningIsATab()
    {
        TestFiles files;
        files.proto("one/a.proto", "message A {}").proto("two/b.proto", "message B {}");
        MainWindow window;
        window.show();
        QVERIFY(!window.tabs()->isVisible());
        window.openPath(files.path("one/a.proto"));
        window.openPath(files.path("two"));
        QCOMPARE(window.tabs()->count(), 2);
        QVERIFY(window.tabs()->isVisible());
        QCOMPARE(window.tabs()->tabText(0), QStringLiteral("a.proto"));
        QCOMPARE(window.tabs()->tabText(1), QStringLiteral("two"));
        QCOMPARE(window.tabs()->currentIndex(), 1);
        QVERIFY(window.windowTitle().contains(QStringLiteral("two")));
    }

    void samePathGoesToItsTab()
    {
        TestFiles files;
        files.proto("in/a.proto", "message A {}");
        MainWindow window;
        window.openPath(files.path("in"));
        window.openPath(files.path("in/a.proto"));
        window.openPath(files.path("in/"));
        window.openPath(files.path("in/../in"));
        QCOMPARE(window.tabs()->count(), 2);
        QCOMPARE(window.tabs()->currentIndex(), 0);
        QVERIFY(QFile::link(files.path("in/a.proto"), files.path("link.proto")));
        window.openPath(files.path("link.proto"));
        QCOMPARE(window.tabs()->count(), 2);
        QCOMPARE(window.tabs()->currentIndex(), 1);
    }

    void tabsCloseDownToTheWelcome()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}").proto("b.proto", "message B {}");
        MainWindow window;
        window.show();
        window.openPath(files.path("a.proto"));
        window.openPath(files.path("b.proto"));
        QAction *close = action(window, QKeySequence(Qt::CTRL | Qt::Key_W));
        QVERIFY(close);
        emit window.tabs()->tabCloseRequested(0);
        QCOMPARE(window.tabs()->count(), 1);
        QCOMPARE(window.tabs()->tabText(0), QStringLiteral("b.proto"));
        close->trigger();
        QCOMPARE(window.tabs()->count(), 0);
        QVERIFY(!window.tabs()->isVisible());
        QVERIFY(!close->isEnabled());
        close->trigger();
        // And can be opened again.
        window.openPath(files.path("a.proto"));
        QCOMPARE(window.tabs()->count(), 1);
    }

    void ampersandIsNotAMnemonic()
    {
        TestFiles files;
        files.proto("R&D.proto", "message A {}");
        MainWindow window;
        window.openPath(files.path("R&D.proto"));
        QCOMPARE(window.tabs()->tabText(0), QStringLiteral("R&&D.proto"));
    }

    void shortcutsReachTheCurrentTab()
    {
        TestFiles files;
        files.proto("a.proto", "message A { B b = 1; } message B {}");
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(!action(window, QKeySequence(QKeySequence::Find))->isEnabled());
        window.openPath(files.path("a.proto"));
        auto *tab = static_cast<SchemaTab *>(window.tabs()->currentWidget());
        QSignalSpy spy(tab, &SchemaTab::loaded);
        QVERIFY(spy.wait());
        action(window, QKeySequence(QKeySequence::Find))->trigger();
        auto *edit = tab->findChild<QWidget *>(QStringLiteral("findEdit"));
        QTRY_VERIFY(edit->hasFocus());
        QTest::keyClicks(edit, QStringLiteral("B"));
        QTest::keyClick(edit, Qt::Key_F3);
        auto *status = tab->findChild<QWidget *>(QStringLiteral("findStatus"));
        QCOMPARE(status->property("text").toString(), QStringLiteral("1 of 1"));
    }

    void missingPathOpensATabThatSaysSo()
    {
        TestFiles files;
        MainWindow window;
        window.openPath(files.path("gone.proto"));
        QCOMPARE(window.tabs()->count(), 1);
        auto *tab = static_cast<SchemaTab *>(window.tabs()->currentWidget());
        QSignalSpy spy(tab, &SchemaTab::loaded);
        QVERIFY(spy.wait());
    }

    static SchemaTab *loaded(MainWindow &window)
    {
        auto *tab = static_cast<SchemaTab *>(window.tabs()->currentWidget());
        QSignalSpy spy(tab, &SchemaTab::loaded);
        return !tab->isLoading() || spy.wait() ? tab : nullptr;
    }

    void jsonNeedsASchema()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}").proto("none.proto", "enum E { Z = 0; }");
        MainWindow window;
        QAction *open = action(window, QKeySequence(Qt::CTRL | Qt::Key_J));
        QAction *paste = action(window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
        QVERIFY(open && paste);
        QVERIFY(!open->isEnabled() && !paste->isEnabled());
        window.openPath(files.path("none.proto"));
        QVERIFY(loaded(window));
        QVERIFY(!open->isEnabled() && !paste->isEnabled()); // nothing to lay it over
        window.openPath(files.path("a.proto"));
        QVERIFY(!open->isEnabled()); // not yet
        QVERIFY(loaded(window));
        QVERIFY(open->isEnabled() && paste->isEnabled());
        window.tabs()->setCurrentIndex(0);
        QVERIFY(!open->isEnabled() && !paste->isEnabled());
    }

    void closeGoesFromTheInsideOut()
    {
        TestFiles files;
        files.proto("a.proto", "message A { int32 i = 1; }");
        MainWindow window;
        window.show();
        window.openPath(files.path("a.proto"));
        SchemaTab *tab = loaded(window);
        QVERIFY(tab);
        QGuiApplication::clipboard()->setText(QStringLiteral("{\"i\": 1}"));
        action(window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V))->trigger();
        action(window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V))->trigger();
        QVERIFY(tab->currentJson());
        QCOMPARE(tab->currentJson()->title(), QStringLiteral("A ← pasted 2"));
        QAction *close = action(window, QKeySequence(Qt::CTRL | Qt::Key_W));
        close->trigger();
        QCOMPARE(tab->currentJson()->title(), QStringLiteral("A ← pasted"));
        close->trigger();
        QVERIFY(!tab->currentJson());
        QCOMPARE(window.tabs()->count(), 1);
        close->trigger();
        QCOMPARE(window.tabs()->count(), 0);
    }

    void openingAListOfPaths()
    {
        TestFiles files;
        files.proto("one/a.proto", "message A { int32 i = 1; }").proto("two/b.proto", "message B { int32 j = 1; } message C { int32 i = 1; }");
        files.add("a.json", "{\"i\": 1}").add("b.JSONL", "{\"j\": 1}\n{\"j\": 2}\n").add("odd.json/c.proto", "syntax = \"proto3\"; message Odd {}");
        MainWindow window;
        window.show();
        QCOMPARE(window.openAll({files.path("one"), files.path("a.json"), files.path("two"), files.path("b.JSONL"), files.path("a.json")}), QString());
        QCOMPARE(window.tabs()->count(), 2);
        const auto titles = [&](int tab) {
            auto *views = window.tabs()->widget(tab)->findChild<QTabWidget *>(QStringLiteral("views"));
            QStringList result;
            for (int i = 0; i < views->count(); ++i)
                result << views->tabText(i);
            return result;
        };
        QTRY_COMPARE(titles(0), QStringList({"Schema", "A ← a.json"}));
        QTRY_COMPARE(titles(1), QStringList({"Schema", "B ← b.JSONL", "C ← a.json"}));
        // A folder can be called anything.
        QCOMPARE(window.openAll({files.path("odd.json")}), QString());
        QCOMPARE(window.tabs()->count(), 3);
    }

    void openingWithAMessage()
    {
        TestFiles files;
        files.proto("b.proto", "package p; message B { int32 i = 1; } message C { int32 i = 1; }").add("a.json", "{\"i\": 1}");
        MainWindow window;
        QCOMPARE(window.openAll({files.path("b.proto"), files.path("a.json")}, QStringLiteral("C")), QString());
        SchemaTab *tab = loaded(window);
        QCOMPARE(tab->currentJson()->message()->fullName, QStringLiteral("p.C"));
    }

    void jsonFirstIsTurnedDown()
    {
        TestFiles files;
        files.proto("a.proto", "message A {}").add("a.json", "{}");
        MainWindow window;
        QCOMPARE(window.openAll({QStringLiteral("a.json"), files.path("a.proto")}),
                 QStringLiteral("a.json is JSON, and needs a schema before it."));
        QCOMPARE(window.tabs()->count(), 0);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
