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
#include "JsonEdit.h"
#include "JsonHighlighter.h"
#include "JsonModel.h"
#include "JsonRows.h"
#include "JsonView.h"
#include "MessagePicker.h"
#include "SchemaTreeView.h"
#include "TestFiles.h"

#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QToolButton>

using namespace protoexplorer;

class TestJsonView : public QObject
{
    Q_OBJECT

    struct Opened
    {
        explicit Opened(const QString &text, const QString &message = QStringLiteral("Order"),
                        const QByteArray &proto = shop())
        {
            files.proto(QStringLiteral("shop.proto"), proto);
            index = std::make_shared<SchemaIndex>(files.load());
            const auto named = index->messagesNamed(message);
            view = std::make_unique<JsonView>(index, named.empty() ? nullptr : named.front(),
                                              QStringLiteral("order.json"), QString(), text);
            view->resize(1000, 600);
            view->show();
        }

        static QByteArray shop()
        {
            return "package shop;\n"
                   "// An order.\n"
                   "message Order {\n  string id = 1;\n  string note = 2;\n  Money total = 3;\n  repeated Item items = 4;\n"
                   "  oneof pay { Card card = 5; Cash cash = 6; }\n  optional int32 priority = 7;\n}\n"
                   "message Money { string currency = 1; int64 units = 2; }\n"
                   "message Item { string sku = 1; Money price = 2; }\n"
                   "message Card { string digits = 1; }\nmessage Cash {}\n"
                   "message Invoice { string number = 1; Money due = 2; string to = 3; string note = 4; }\n";
        }

        template<typename T> T *child(const char *name) const
        {
            T *found = view->findChild<T *>(QString::fromLatin1(name));
            if (!found)
                qFatal("no %s", name);
            return found;
        }
        SchemaTreeView *tree() const { return child<SchemaTreeView>("jsonTree"); }
        JsonEdit *edit() const { return child<JsonEdit>("jsonEdit"); }
        QString current() const { return pathOf(tree()->currentIndex()); }
        static QString pathOf(QModelIndex index)
        {
            QStringList labels;
            for (; index.isValid(); index = index.parent())
                labels.prepend(index.siblingAtColumn(0).data().toString());
            return labels.join(u'/');
        }
        QModelIndex row(const QString &path) const
        {
            QModelIndex index;
            for (const QString &name : path.split(u'/')) {
                const auto *model = tree()->model();
                QModelIndex found;
                for (int i = 0; i < model->rowCount(index); ++i) {
                    if (model->index(i, 0, index).data().toString() == name)
                        found = model->index(i, 0, index);
                }
                if (!found.isValid())
                    return {};
                index = found;
            }
            return index;
        }
        void select(const char *path) const
        {
            const QModelIndex index = row(QString::fromUtf8(path));
            if (!index.isValid())
                qFatal("no %s", path);
            tree()->setCurrentIndex(index);
        }
        QString value(const char *path) const
        {
            return row(QString::fromUtf8(path)).siblingAtColumn(JsonModel::ValueColumn).data().toString();
        }
        QString source(int n) const { return child<QPlainTextEdit>(n == 1 ? "sourceText1" : "sourceText2")->toPlainText(); }
        bool second() const { return child<QPlainTextEdit>("sourceText2")->isVisible(); }
        QString where() const { return child<QLabel>("jsonWhere")->text(); }
        QString fit() const { return child<QLabel>("jsonFit")->text(); }
        QString status() const { return child<QLabel>("findStatus")->text(); }
        void find(const QString &text) const { child<QLineEdit>("findEdit")->setText(text); }
        bool isUsable() const { return tree()->isEnabled(); }
        // What is marked in the JSON, and what is underlined.
        QString marked() const
        {
            for (const auto &selection : edit()->extraSelections()) {
                if (selection.format.underlineStyle() == QTextCharFormat::NoUnderline)
                    return selection.cursor.selectedText().replace(QChar(QChar::ParagraphSeparator), u'\n');
            }
            return {};
        }
        int underlinedAt() const
        {
            for (const auto &selection : edit()->extraSelections()) {
                if (selection.format.underlineStyle() != QTextCharFormat::NoUnderline)
                    return selection.cursor.selectionStart();
            }
            return -1;
        }
        // As if typed, all of it.
        void write(const QString &text) const
        {
            edit()->selectAll();
            edit()->insertPlainText(text);
        }
        void putCursorAt(const QString &text, int plus = 0) const
        {
            QTextCursor cursor(edit()->document());
            cursor.setPosition(int(edit()->toPlainText().indexOf(text)) + plus);
            edit()->setTextCursor(cursor);
        }
        QToolButton *count(const char *name) const { return child<QToolButton>(name); }
        QMenu *menuAt(const char *path) const
        {
            const QPoint at = path ? tree()->visualRect(row(QString::fromUtf8(path))).center() : QPoint(5, tree()->viewport()->height() - 3);
            emit tree()->customContextMenuRequested(at);
            return qobject_cast<QMenu *>(QApplication::activePopupWidget());
        }
        static QString clipboard() { return QGuiApplication::clipboard()->text(); }

        TestFiles files;
        std::shared_ptr<const SchemaIndex> index;
        std::unique_ptr<JsonView> view;
    };

    static QString sample()
    {
        return QStringLiteral("{\n  \"id\": \"A-1\",\n  \"total\": {\n    \"currency\": \"UAH\"\n  },\n"
                              "  \"items\": [{\"sku\": \"bolt\"}, {\"sku\": \"nut\"}],\n  \"card\": {},\n  \"what\": 1\n}\n");
    }

private slots:
    void opens()
    {
        const Opened o(sample());
        QCOMPARE(o.view->title(), QStringLiteral("Order ← order.json"));
        QCOMPARE(o.child<QLabel>("jsonMessage")->text(), QStringLiteral("Order"));
        QCOMPARE(o.fit(), QStringLiteral("4 of 5 keys match"));
        QVERIFY(!o.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(o.current(), QStringLiteral("Order"));
        QVERIFY(o.source(1).startsWith(QStringLiteral("// An order.\nmessage Order {")));
        QCOMPARE(o.marked(), sample().trimmed());
        QCOMPARE(o.where(), QStringLiteral("order.json : 1–9"));
        QCOMPARE(o.value("Order/id"), QStringLiteral("\"A-1\""));
        QCOMPARE(o.value("Order/total/units"), QStringLiteral("0"));
        QCOMPARE(o.count("countSet")->text(), QStringLiteral("7 set"));
        QCOMPARE(o.count("countDefault")->text(), QStringLiteral("3 defaulted"));
        QCOMPARE(o.count("countNotSet")->text(), QStringLiteral("4 not set"));
        QCOMPARE(o.count("countProblem")->text(), QStringLiteral("1 problem"));
        QVERIFY(!o.view->isModified());
    }

    void opensOutWhatIsThere()
    {
        const Opened o(sample());
        const auto expanded = [&](const char *path) { return o.tree()->isExpanded(o.row(QString::fromLatin1(path))); };
        QVERIFY(expanded("Order"));
        QVERIFY(expanded("Order/total"));
        QVERIFY(expanded("Order/items"));
        QVERIFY(!expanded("Order/items/[0]"));
        QVERIFY(expanded("Order/pay"));
        QVERIFY(expanded("Order/pay/card"));

        QString many = QStringLiteral("{\"items\": [{}");
        for (int i = 0; i < 10; ++i)
            many += QStringLiteral(", {}");
        const Opened long_(many + QStringLiteral("]}"));
        QVERIFY(!long_.tree()->isExpanded(long_.row(QStringLiteral("Order/items"))));
        // All the way down, however many fields there are.
        QByteArray fields = "message M { M in = 100;";
        for (int i = 1; i < 20; ++i)
            fields += " int32 f" + QByteArray::number(i) + " = " + QByteArray::number(i) + ";";
        const Opened deep(QStringLiteral("{\"in\": {\"in\": {}}}"), QStringLiteral("M"), fields + " }");
        QVERIFY(deep.tree()->isExpanded(deep.row(QStringLiteral("M/in/in"))));
        const Opened several(QStringLiteral("[{\"total\": {}}, {}]"));
        QVERIFY(!several.tree()->isExpanded(several.row(QStringLiteral("Order [0]"))));
    }

    void selectingARowShowsItBothWays()
    {
        const Opened o(sample());
        o.select("Order/total");
        QCOMPARE(o.source(1), QStringLiteral("Money total = 3;"));
        QVERIFY(o.second());
        QVERIFY(o.source(2).startsWith(QStringLiteral("message Money {")));
        QCOMPARE(o.marked(), QStringLiteral("\"total\": {\n    \"currency\": \"UAH\"\n  }"));
        QCOMPARE(o.where(), QStringLiteral("order.json : 3–5"));
        QCOMPARE(o.edit()->textCursor().position(), sample().indexOf(QStringLiteral("\"total\"")));

        o.select("Order/items/[1]/sku");
        QCOMPARE(o.source(1), QStringLiteral("string sku = 1;"));
        QCOMPARE(o.marked(), QStringLiteral("\"sku\": \"nut\""));
        QCOMPARE(o.where(), QStringLiteral("order.json : 6"));

        // Missing, it shows what it is missing from.
        o.select("Order/total/units");
        QCOMPARE(o.source(1), QStringLiteral("int64 units = 2;"));
        QCOMPARE(o.marked(), QStringLiteral("\"total\": {\n    \"currency\": \"UAH\"\n  }"));
        o.select("Order/pay");
        QVERIFY(o.source(1).startsWith(QStringLiteral("oneof pay {")));
        QCOMPARE(o.marked(), sample().trimmed());
        o.select("Order/✕ what");
        QVERIFY(o.source(1).startsWith(QStringLiteral("// An order.")));
        QCOMPARE(o.marked(), QStringLiteral("\"what\": 1"));
    }

    void theCursorSelectsTheRow()
    {
        const Opened o(sample());
        o.putCursorAt(QStringLiteral("\"UAH\""), 2);
        QCOMPARE(o.current(), QStringLiteral("Order/total/currency"));
        QCOMPARE(o.marked(), QStringLiteral("\"currency\": \"UAH\""));
        QCOMPARE(o.edit()->textCursor().position(), sample().indexOf(QStringLiteral("\"UAH\"")) + 2); // left alone
        o.putCursorAt(QStringLiteral("\"nut\""));
        QCOMPARE(o.current(), QStringLiteral("Order/items/[1]/sku"));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order/items/[1]"))));
        o.putCursorAt(QStringLiteral("\"card\""), 1);
        QCOMPARE(o.current(), QStringLiteral("Order/pay/card"));
        o.putCursorAt(QStringLiteral("\"what\""));
        QCOMPARE(o.current(), QStringLiteral("Order/✕ what"));
        // Outside of everything there is nothing to go to.
        QTextCursor end(o.edit()->document());
        end.movePosition(QTextCursor::End);
        o.edit()->setTextCursor(end);
        QCOMPARE(o.current(), QStringLiteral("Order/✕ what"));
    }

    void writingLaysItOutAnew()
    {
        const Opened o(sample());
        o.select("Order/total/currency");
        o.tree()->collapse(o.row(QStringLiteral("Order/items")));
        o.tree()->expand(o.row(QStringLiteral("Order/items/[0]")));
        QString text = sample();
        o.write(text.replace(QStringLiteral("UAH"), QStringLiteral("EUR")).replace(QStringLiteral("\"id\": \"A-1\""), QStringLiteral("\"note\": \"x\"")));
        QCOMPARE(o.value("Order/total/currency"), QStringLiteral("\"UAH\"")); // not at once
        QTRY_COMPARE(o.value("Order/total/currency"), QStringLiteral("\"EUR\""));
        QCOMPARE(o.value("Order/note"), QStringLiteral("\"x\""));
        QCOMPARE(o.value("Order/id"), QStringLiteral("\"\""));
        // What was open and what was not stays so, and so does what was selected.
        QVERIFY(!o.tree()->isExpanded(o.row(QStringLiteral("Order/items"))));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order/total"))));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order/pay"))));
        QCOMPARE(o.current(), QStringLiteral("Order/total/currency"));
        QCOMPARE(o.marked(), QStringLiteral("\"currency\": \"EUR\""));
        QVERIFY(o.view->isModified());

        // Unless it is no longer there.
        o.write(QStringLiteral("{\"total\": null}"));
        QTRY_COMPARE(o.value("Order/total"), QStringLiteral("null → not set"));
        QCOMPARE(o.current(), QStringLiteral("Order"));
        QCOMPARE(o.marked(), QStringLiteral("{\"total\": null}"));
    }

    void theRowFollowsTheCursorWhileWriting()
    {
        Opened o(sample());
        QVERIFY(QTest::qWaitForWindowActive(o.view.get()));
        o.edit()->setFocus();
        QTRY_VERIFY(o.edit()->hasFocus());
        o.putCursorAt(QStringLiteral("bolt"), 4);
        QCOMPARE(o.current(), QStringLiteral("Order/items/[0]/sku"));
        QTest::keyClicks(o.edit(), QStringLiteral("XY"));
        const int at = o.edit()->textCursor().position();
        QTRY_COMPARE(o.value("Order/items/[0]/sku"), QStringLiteral("\"boltXY\""));
        QCOMPARE(o.current(), QStringLiteral("Order/items/[0]/sku"));
        QCOMPARE(o.marked(), QStringLiteral("\"sku\": \"boltXY\""));
        QCOMPARE(o.edit()->textCursor().position(), at);
        QVERIFY(o.edit()->hasFocus());
    }

    void malformedLeavesWhatThereWas()
    {
        const Opened o(sample());
        o.select("Order/total");
        o.write(QStringLiteral("{\n  \"id\": \"B-2\"\n  \"note\": \"x\"\n}"));
        QTRY_VERIFY(!o.isUsable());
        QCOMPARE(o.where(), QStringLiteral("order.json : 3 — expected ‘,’ or ‘}’"));
        const auto whereColour = [&] { return o.child<QLabel>("jsonWhere")->palette().color(QPalette::WindowText); };
        QCOMPARE(whereColour(), errorColor(o.view->palette()));
        QCOMPARE(o.underlinedAt(), o.edit()->toPlainText().indexOf(QStringLiteral("\"note\"")));
        QCOMPARE(o.marked(), QString());
        QCOMPARE(o.value("Order/id"), QStringLiteral("\"A-1\""));
        QCOMPARE(o.current(), QStringLiteral("Order/total"));
        QVERIFY(!o.count("countSet")->isEnabled());
        QVERIFY(!o.child<QToolButton>("jsonChange")->isEnabled());
        // Places in it mean nothing now.
        o.putCursorAt(QStringLiteral("B-2"));
        QCOMPARE(o.current(), QStringLiteral("Order/total"));
        o.view->find(true);

        o.write(QStringLiteral("{\"id\": \"B-2\"}"));
        QTRY_VERIFY(o.isUsable());
        QCOMPARE(o.underlinedAt(), -1);
        QCOMPARE(o.value("Order/id"), QStringLiteral("\"B-2\""));
        QCOMPARE(o.current(), QStringLiteral("Order/total"));
        QCOMPARE(o.where(), QStringLiteral("order.json : 1"));
        QCOMPARE(whereColour(), mutedColor(o.view->palette()));
    }

    void malformedFromTheStart()
    {
        const Opened o(QStringLiteral("{\"id\": "));
        QVERIFY(!o.isUsable());
        QCOMPARE(o.view->title(), QStringLiteral("Order ← order.json"));
        QVERIFY(o.where().contains(QStringLiteral("ends too soon")));
        QVERIFY(o.underlinedAt() >= 0);
        QVERIFY(!o.count("countSet")->isVisible());
        o.write(QStringLiteral("{\"id\": \"x\"}"));
        QTRY_VERIFY(o.isUsable());
        QCOMPARE(o.value("Order/id"), QStringLiteral("\"x\""));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order"))));
    }

    void countsStepThroughWhatTheyCount()
    {
        const Opened o(sample());
        const auto step = [&](const char *name) {
            QTest::mouseClick(o.count(name), Qt::LeftButton);
            return o.current();
        };
        QCOMPARE(step("countDefault"), QStringLiteral("Order/note"));
        QCOMPARE(step("countDefault"), QStringLiteral("Order/total/units"));
        QCOMPARE(step("countDefault"), QStringLiteral("Order/pay/card/digits"));
        QCOMPARE(step("countDefault"), QStringLiteral("Order/note")); // round again
        QCOMPARE(step("countProblem"), QStringLiteral("Order/✕ what"));
        QCOMPARE(step("countNotSet"), QStringLiteral("Order/items/[0]/price"));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order/items/[0]"))));
        QCOMPARE(step("countSet"), QStringLiteral("Order/items/[1]/sku"));
    }

    void countsOfNothingAreNotThere()
    {
        const Opened o(QStringLiteral("{}"), QStringLiteral("Money"));
        QVERIFY(!o.count("countSet")->isVisible());
        QVERIFY(o.count("countDefault")->isVisible());
        QVERIFY(!o.count("countNotSet")->isVisible());
        QVERIFY(!o.count("countProblem")->isVisible());
        const Opened two(QStringLiteral("{\"x\": 1, \"y\": 1}"), QStringLiteral("Money"));
        QCOMPARE(two.count("countProblem")->text(), QStringLiteral("2 problems"));
    }

    void missingCanBeHidden()
    {
        const Opened o(sample());
        auto *box = o.child<QCheckBox>("showMissing");
        QVERIFY(box->isChecked());
        o.select("Order/total/units");
        box->click();
        QVERIFY(!o.row(QStringLiteral("Order/note")).isValid());
        QVERIFY(!o.row(QStringLiteral("Order/pay/cash")).isValid());
        QVERIFY(o.row(QStringLiteral("Order/pay/card")).isValid());
        QVERIFY(o.row(QStringLiteral("Order/✕ what")).isValid());
        QVERIFY(!o.row(QStringLiteral("Order/items/[0]/price")).isValid());
        QCOMPARE(o.current(), QStringLiteral("Order/total")); // what was selected was in it
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Order/items"))));
        // To be shown them is to want them.
        QTest::mouseClick(o.count("countNotSet"), Qt::LeftButton);
        QVERIFY(box->isChecked());
        QCOMPARE(o.current(), QStringLiteral("Order/items/[0]/price"));
    }

    void theCursorFindsNoHiddenRow()
    {
        const Opened o(QStringLiteral("{\"total\": null, \"id\": \"x\"}"));
        o.child<QCheckBox>("showMissing")->click();
        o.select("Order/id");
        o.putCursorAt(QStringLiteral("null"), 1);
        QCOMPARE(o.current(), QStringLiteral("Order"));
    }

    void findsFieldsAndValues()
    {
        const Opened o(sample());
        o.find(QStringLiteral("sku"));
        QCOMPARE(o.current(), QStringLiteral("Order/items/[0]/sku"));
        QCOMPARE(o.status(), QStringLiteral("1 of 2"));
        o.view->find(true);
        QCOMPARE(o.current(), QStringLiteral("Order/items/[1]/sku"));
        o.view->find(true);
        QCOMPARE(o.current(), QStringLiteral("Order/items/[0]/sku"));
        o.view->find(false);
        QCOMPARE(o.current(), QStringLiteral("Order/items/[1]/sku"));
        o.find(QStringLiteral("NUT"));
        QCOMPARE(o.current(), QStringLiteral("Order/items/[1]/sku"));
        QCOMPARE(o.status(), QStringLiteral("1 of 1"));
        o.find(QStringLiteral("price")); // missing, but there to see
        QCOMPARE(o.status(), QStringLiteral("1 of 2"));
        o.find(QStringLiteral("not set")); // which is no value of anything
        QCOMPARE(o.status(), QStringLiteral("No matches"));
        QVERIFY(o.view->findChild<FindBar *>()->isFailed());
        o.find(QString());
        QCOMPARE(o.status(), QString());
    }

    void findPrefersAnExactName()
    {
        const Opened o(QStringLiteral("{\"note\": \"id\", \"id\": \"note\", \"priority\": 1}"));
        o.find(QStringLiteral("i"));
        QCOMPARE(o.current(), QStringLiteral("Order/id"));
        o.find(QStringLiteral("priority"));
        QCOMPARE(o.current(), QStringLiteral("Order/priority"));
        o.find(QStringLiteral("note"));
        QCOMPARE(o.current(), QStringLiteral("Order/note")); // though what id holds is before it
        QCOMPARE(o.status(), QStringLiteral("2 of 2"));
    }

    void findGoesByTheNamesOfTheJsonToo()
    {
        const Opened o(QStringLiteral("{\"longName\": 1}"), QStringLiteral("M"), "message M { int32 long_name = 1; }");
        o.find(QStringLiteral("longName"));
        QCOMPARE(o.current(), QStringLiteral("M/long_name"));
        o.find(QStringLiteral("long_n"));
        QCOMPARE(o.status(), QStringLiteral("1 of 1"));
    }

    void findSkipsWhatIsHidden()
    {
        const Opened o(sample());
        o.find(QStringLiteral("cash"));
        QCOMPARE(o.status(), QStringLiteral("1 of 1"));
        o.child<QCheckBox>("showMissing")->click();
        o.view->find(true);
        QCOMPARE(o.status(), QStringLiteral("No matches"));
        QVERIFY(!o.child<QCheckBox>("showMissing")->isChecked());
    }

    void findAfterWritingIsOfWhatIsThereNow()
    {
        const Opened o(sample());
        o.find(QStringLiteral("sku"));
        QCOMPARE(o.status(), QStringLiteral("1 of 2"));
        for (int i = 1; i <= 4; ++i) {
            // Over and over, as one tree takes the place of another.
            QString items = QStringLiteral("{\"sku\": \"x\"}");
            for (int k = 1; k < i; ++k)
                items += QStringLiteral(", {\"sku\": \"x\"}");
            o.write(QStringLiteral("{\"items\": [%1]}").arg(items));
            QTRY_COMPARE(o.value("Order/items"), i == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 items").arg(i));
            QCOMPARE(o.status(), QString());
            o.view->find(true);
            QVERIFY(o.status().endsWith(QStringLiteral(" of %1").arg(i)));
        }
    }

    void findFromTheJsonBringsItIntoView()
    {
        QString text = QStringLiteral("{\"items\": [\n");
        for (int i = 0; i < 300; ++i)
            text += QStringLiteral("  {\"sku\": \"s%1\"},\n").arg(i);
        Opened o(text + QStringLiteral("  {\"sku\": \"last\"}\n]}"));
        QVERIFY(QTest::qWaitForWindowActive(o.view.get()));
        o.edit()->setFocus();
        QTRY_VERIFY(o.edit()->hasFocus());
        o.find(QStringLiteral("last"));
        QCOMPARE(o.current(), QStringLiteral("Order/items/[300]/sku"));
        QVERIFY(o.edit()->viewport()->rect().contains(o.edit()->cursorRect().center()));
        QVERIFY(o.edit()->verticalScrollBar()->value() > 200);
    }

    void aLikelierMessageIsOffered()
    {
        const Opened o(QStringLiteral("{\"number\": \"1\", \"due\": {}, \"to\": \"x\", \"id\": \"y\"}"));
        QVERIFY(o.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(o.child<QLabel>("jsonHintText")->text(),
                 QStringLiteral("1 of 4 keys match Order, while Invoice matches 3."));
        QSignalSpy spy(o.view.get(), &JsonView::titleChanged);
        QTest::mouseClick(o.child<QPushButton>("jsonSwitch"), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(o.view->title(), QStringLiteral("Invoice ← order.json"));
        QCOMPARE(o.fit(), QStringLiteral("3 of 4 keys match"));
        QVERIFY(!o.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(o.current(), QStringLiteral("Invoice"));
        QVERIFY(o.tree()->isExpanded(o.row(QStringLiteral("Invoice/due"))));
    }

    void andNotWithoutNeed()
    {
        // Half will do.
        const Opened half(QStringLiteral("{\"id\": \"1\", \"note\": \"2\", \"number\": \"3\", \"due\": {}}"));
        QCOMPARE(half.fit(), QStringLiteral("2 of 4 keys match"));
        QVERIFY(!half.child<QWidget>("jsonHint")->isVisible());
        // Nothing fits better.
        const Opened none(QStringLiteral("{\"a\": 1, \"b\": 2}"));
        QVERIFY(!none.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(none.fit(), QStringLiteral("0 of 2 keys match"));
        QCOMPARE(Opened(QStringLiteral("{\"id\": \"1\"}")).fit(), QStringLiteral("1 of 1 key matches"));
        QCOMPARE(Opened(QStringLiteral("{}")).fit(), QString());
        // Its keys are no fields.
        const Opened any(QStringLiteral("{\"number\": \"1\", \"due\": {}}"), QStringLiteral("google.protobuf.Struct"),
                         "import \"google/protobuf/struct.proto\"; message Invoice { string number = 1; int32 due = 2; google.protobuf.Struct s = 3; }");
        QCOMPARE(any.view->title(), QStringLiteral("Struct ← order.json"));
        QVERIFY(!any.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(any.fit(), QString());
    }

    void withoutAMessageTheLikeliest()
    {
        const Opened o(QStringLiteral("{\"number\": \"1\", \"due\": {}}"), QStringLiteral("(none)"));
        QCOMPARE(o.view->title(), QStringLiteral("Invoice ← order.json"));
        // What was settled on stays, and is not guessed anew at every stroke.
        o.write(QStringLiteral("{\"id\": \"1\", \"items\": [], \"note\": \"x\"}"));
        QTRY_COMPARE(o.fit(), QStringLiteral("1 of 3 keys match"));
        QCOMPARE(o.view->title(), QStringLiteral("Invoice ← order.json"));
        QVERIFY(o.child<QPushButton>("jsonSwitch")->isVisible());
    }

    void withoutAMessageItWaitsForSomethingToGoBy()
    {
        const Opened o(QString(), QStringLiteral("(none)"));
        QVERIFY(o.isUsable());
        QVERIFY(!o.tree()->model()->hasChildren());
        QSignalSpy spy(o.view.get(), &JsonView::titleChanged);
        o.write(QStringLiteral("{\"number\": \"1\"}"));
        QTRY_COMPARE(o.view->title(), QStringLiteral("Invoice ← order.json"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(o.value("Invoice/number"), QStringLiteral("\"1\""));
        // One that was asked for is not up for guessing.
        const Opened chosen{QString()};
        chosen.write(QStringLiteral("{\"number\": \"1\"}"));
        QTRY_COMPARE(chosen.fit(), QStringLiteral("0 of 1 key matches"));
        QCOMPARE(chosen.view->title(), QStringLiteral("Order ← order.json"));
    }

    void emptyTakesTheFocusToWhereItIsWritten()
    {
        Opened o{QString()};
        QVERIFY(QTest::qWaitForWindowActive(o.view.get()));
        o.view->setFocus();
        QTRY_VERIFY(o.edit()->hasFocus());
        Opened full(sample());
        QVERIFY(QTest::qWaitForWindowActive(full.view.get()));
        full.view->setFocus();
        QTRY_VERIFY(full.tree()->hasFocus());
    }

    void picking()
    {
        const Opened o(sample());
        QTest::mouseClick(o.child<QToolButton>("jsonChange"), Qt::LeftButton);
        auto *picker = qobject_cast<MessagePicker *>(QApplication::activePopupWidget());
        QVERIFY(picker);
        auto *list = picker->findChild<QListWidget *>(QStringLiteral("pickerList"));
        auto *filter = picker->findChild<QLineEdit *>(QStringLiteral("pickerFilter"));
        QCOMPARE(list->count(), 6);
        QCOMPARE(list->item(0)->text(), QStringLiteral("shop.Order — 4 of 5"));
        QCOMPARE(list->currentRow(), 0);
        QTest::keyClicks(filter, QStringLiteral("shop.i"));
        QCOMPARE(list->count(), 2);
        QTest::keyClick(filter, Qt::Key_Down);
        const QString second = list->currentItem()->text();
        QTest::keyClick(filter, Qt::Key_Down); // no further than the end
        QCOMPARE(list->currentItem()->text(), second);
        QTest::keyClick(filter, Qt::Key_Up);
        QTest::keyClick(filter, Qt::Key_Down);
        QTest::keyClick(filter, Qt::Key_Return);
        QVERIFY(second.contains(o.view->message()->fullName));
        QVERIFY(o.view->message()->name != u"Order");
        QCOMPARE(o.row(o.view->message()->name).isValid(), true);
        QTRY_VERIFY(!QApplication::activePopupWidget());
    }

    void pickingByClickAndNotAtAll()
    {
        const Opened o(sample());
        const auto open = [&] {
            QTest::mouseClick(o.child<QToolButton>("jsonChange"), Qt::LeftButton);
            return qobject_cast<MessagePicker *>(QApplication::activePopupWidget());
        };
        MessagePicker *picker = open();
        QTest::keyClicks(picker->findChild<QLineEdit *>(QStringLiteral("pickerFilter")), QStringLiteral("zzz"));
        QCOMPARE(picker->findChild<QListWidget *>(QStringLiteral("pickerList"))->count(), 0);
        QTest::keyClick(picker->findChild<QLineEdit *>(QStringLiteral("pickerFilter")), Qt::Key_Return);
        QCOMPARE(o.view->message()->name, QStringLiteral("Order"));
        picker->close();

        picker = open();
        auto *list = picker->findChild<QListWidget *>(QStringLiteral("pickerList"));
        const auto items = list->findItems(QStringLiteral("shop.Money"), Qt::MatchStartsWith);
        QCOMPARE(items.size(), 1);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(items.first()).center());
        QCOMPARE(o.view->message()->name, QStringLiteral("Money"));
    }

    void copying()
    {
        const Opened o(sample());
        const auto trigger = [&](const char *path, int action) {
            QMenu *menu = o.menuAt(path);
            if (!menu)
                qFatal("no menu");
            menu->actions().at(action)->trigger();
            menu->close();
            return Opened::clipboard();
        };
        QCOMPARE(trigger("Order/id", 0), QStringLiteral("A-1"));
        QCOMPARE(trigger("Order/total/units", 1), QStringLiteral("$.total.units"));
        QVERIFY(trigger("Order/id", 3).startsWith(QStringLiteral("{\n  \"id\": \"A-1\",\n  \"note\": \"\",")));
        QMenu *menu = o.menuAt("Order/pay/cash");
        QVERIFY(!menu->actions().at(0)->isEnabled());
        QVERIFY(menu->actions().at(1)->isEnabled());
        menu->close();

        o.select("Order/total/currency");
        o.tree()->setFocus();
        QGuiApplication::clipboard()->clear();
        for (QAction *action : o.tree()->actions())
            action->trigger();
        QCOMPARE(Opened::clipboard(), QStringLiteral("UAH"));
    }

    void copyingInTheEmpty()
    {
        const Opened o(QStringLiteral("{}"), QStringLiteral("Cash"));
        QMenu *menu = o.menuAt(nullptr);
        QVERIFY(!menu->actions().at(0)->isEnabled());
        QVERIFY(!menu->actions().at(1)->isEnabled());
        QVERIFY(menu->actions().at(3)->isEnabled());
        menu->close();
    }

    void notesAreShown()
    {
        const Opened o(sample());
        o.view->setNote(QStringLiteral("Mind this."));
        QVERIFY(o.child<QWidget>("jsonHint")->isVisible());
        QCOMPARE(o.child<QLabel>("jsonHintText")->text(), QStringLiteral("Mind this."));
        QVERIFY(!o.child<QPushButton>("jsonSwitch")->isVisible());
        o.view->setMessage(o.index->messagesNamed(QStringLiteral("Money")).front());
        QVERIFY(!o.child<QLabel>("jsonHintText")->text().contains(QStringLiteral("Mind")));
    }

    void rowsAreToldApart()
    {
        struct Peek : JsonDelegate
        {
            using JsonDelegate::initStyleOption;
        };
        const Opened o(sample());
        const auto option = [&](const char *path) {
            QStyleOptionViewItem result;
            result.palette = o.tree()->palette();
            Peek().initStyleOption(&result, o.row(QString::fromUtf8(path)));
            return result;
        };
        const QColor plain = o.tree()->palette().color(QPalette::Text);
        QCOMPARE(option("Order/id").palette.color(QPalette::Text), plain);
        QVERIFY(!option("Order/id").font.italic());
        QCOMPARE(option("Order/note").palette.color(QPalette::Text), dimColor(o.tree()->palette()));
        QVERIFY(option("Order/note").font.italic());
        QVERIFY(option("Order/priority").font.italic());
        QCOMPARE(option("Order/✕ what").palette.color(QPalette::Text), errorColor(o.tree()->palette()));
        const QColor grayed = o.tree()->palette().color(QPalette::Disabled, QPalette::Text);
        QCOMPARE(option("Order/✕ what").palette.color(QPalette::Disabled, QPalette::Text), grayed);
        QCOMPARE(option("Order/note").palette.color(QPalette::Disabled, QPalette::Text), grayed);
    }

    void coloursKeepUpWithTheTheme()
    {
        const Opened o(sample());
        QPalette dark = o.view->palette();
        dark.setColor(QPalette::Window, QColor(45, 45, 48));
        dark.setColor(QPalette::WindowText, QColor(225, 225, 225));
        dark.setColor(QPalette::Base, QColor(30, 30, 32));
        dark.setColor(QPalette::Text, QColor(225, 225, 225));
        const auto problems = [&] { return o.count("countProblem")->palette().color(QPalette::ButtonText); };
        const auto where = [&] { return o.child<QLabel>("jsonWhere")->palette().color(QPalette::WindowText); };
        const QColor lightProblems = problems();
        const QColor lightWhere = where();
        o.view->setPalette(dark);
        QCOMPARE(problems(), errorColor(dark));
        QVERIFY(problems() != lightProblems);
        QCOMPARE(where(), mutedColor(dark));
        QVERIFY(where() != lightWhere);
    }

    void dimIsStillToBeRead()
    {
        QPalette dark;
        dark.setColor(QPalette::Text, QColor(225, 225, 225));
        dark.setColor(QPalette::Base, QColor(30, 30, 32));
        QPalette light;
        light.setColor(QPalette::Text, Qt::black);
        light.setColor(QPalette::Base, Qt::white);
        for (const QPalette &palette : {light, dark})
            QVERIFY(contrastRatio(dimColor(palette), palette.color(QPalette::Base)) >= 4.5);
    }

    void theEditorKeepsWhatItIsGiven()
    {
        JsonEdit edit;
        const QString text = QStringLiteral("a b c\nd");
        edit.setPlainText(text);
        QCOMPARE(edit.json(), text);
        // Somebody else's address is not JSON.
        struct Peek : JsonEdit
        {
            using JsonEdit::canInsertFromMimeData;
        };
        QMimeData file;
        file.setUrls({QUrl::fromLocalFile(QStringLiteral("/a.json"))});
        file.setText(QStringLiteral("file:///a.json"));
        QVERIFY(!Peek().canInsertFromMimeData(&file));
        QMimeData plain;
        plain.setText(QStringLiteral("{}"));
        QVERIFY(Peek().canInsertFromMimeData(&plain));
    }

    void colours()
    {
        const auto tokens = [](const char *line) {
            QStringList result;
            const QString text = QString::fromUtf8(line);
            const char *roles[] = {"comment", "string", "number", "key", "word"};
            for (const SyntaxToken &t : tokenizeJson(text))
                result << QString::fromLatin1(roles[size_t(t.role)]) + u':' + text.mid(t.start, t.length);
            return result.join(u' ');
        };
        QCOMPARE(tokens("  \"a\" : -1.5e+3, \"b\":\"x\\\"y\", true"),
                 QStringLiteral("key:\"a\" number:-1.5e+3 key:\"b\" string:\"x\\\"y\" word:true"));
        QCOMPARE(tokens("[null, \"a:b\", \"c\"]"), QStringLiteral("word:null string:\"a:b\" string:\"c\""));
        QCOMPARE(tokens("\"open"), QStringLiteral("string:\"open"));
        QCOMPARE(tokens("\"open\\"), QStringLiteral("string:\"open\\"));
        QCOMPARE(tokens("nope {}"), QString());
        QCOMPARE(tokens(""), QString());
    }
};

QTEST_MAIN(TestJsonView)
#include "tst_jsonview.moc"
