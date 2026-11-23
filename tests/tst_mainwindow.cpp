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

#include "MainWindow.h"
#include "SchemaTab.h"
#include "TestFiles.h"

#include <QAction>
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
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
