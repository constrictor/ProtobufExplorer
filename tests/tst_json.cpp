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

#include "Json.h"

#include <QTest>

using namespace protoexplorer;
using Kind = JsonValue::Kind;

class TestJson : public QObject
{
    Q_OBJECT

private slots:
    void everyKind()
    {
        const JsonDocument doc = parseJson(QStringLiteral(
            "{\"n\": null, \"t\": true, \"f\": false, \"d\": -1.50e+3, \"s\": \"x\", \"a\": [1, []], \"o\": {}}"));
        QVERIFY(!doc.error);
        QCOMPARE(doc.values.size(), size_t(1));
        const JsonValue &root = doc.values.front();
        QCOMPARE(root.kind, Kind::Object);
        const std::pair<const char *, Kind> kinds[] = {
            {"n", Kind::Null}, {"t", Kind::Bool}, {"f", Kind::Bool}, {"d", Kind::Number},
            {"s", Kind::String}, {"a", Kind::Array}, {"o", Kind::Object}};
        QCOMPARE(root.members.size(), std::size(kinds));
        for (size_t i = 0; i < std::size(kinds); ++i) {
            QCOMPARE(root.members[i].key, QString::fromLatin1(kinds[i].first));
            QCOMPARE(root.members[i].kind, kinds[i].second);
        }
        QCOMPARE(root.find(u"d")->text, QStringLiteral("-1.50e+3")); // as written
        QCOMPARE(root.find(u"t")->text, QStringLiteral("true"));
        QCOMPARE(root.find(u"a")->members.size(), size_t(2));
        QVERIFY(!root.find(u"z"));
        QVERIFY(!root.find(u"a")->find(u""));
    }

    void places()
    {
        const QString text = QStringLiteral(" {\n  \"ключ\" : [ 12 , \"🙂\" ]\n} ");
        const JsonDocument doc = parseJson(text);
        QVERIFY(!doc.error);
        const JsonValue &root = doc.values.front();
        QCOMPARE(root.begin, 1);
        QCOMPARE(doc.slice(root), text.trimmed());
        const JsonValue &list = root.members.front();
        QCOMPARE(list.keyBegin, text.indexOf(u'"'));
        QCOMPARE(doc.slice(list), QStringLiteral("[ 12 , \"🙂\" ]"));
        QCOMPARE(doc.slice(list.members[0]), QStringLiteral("12"));
        QCOMPARE(list.members[0].keyBegin, list.members[0].begin);
        QCOMPARE(doc.slice(list.members[1]), QStringLiteral("\"🙂\""));
    }

    void keepsWhatQtWouldNot()
    {
        const JsonDocument doc = parseJson(QStringLiteral("{\"b\":1,\"a\":18446744073709551615,\"b\":2}"));
        const JsonValue &root = doc.values.front();
        QCOMPARE(root.members.size(), size_t(3));
        QCOMPARE(root.members[0].key, QStringLiteral("b"));
        QCOMPARE(root.members[1].text, QStringLiteral("18446744073709551615"));
        QCOMPARE(root.members[2].key, QStringLiteral("b"));
        QCOMPARE(root.find(u"b"), &root.members[0]);
    }

    void escapes()
    {
        const JsonDocument doc = parseJson(QString::fromUtf8(
            "[\"\\\"\\\\\\/\\b\\f\\n\\r\\t\", \"\\u0041\\u00e9\\ud83d\\uDE42\", \"a\\u0000b\"]"));
        QVERIFY(!doc.error);
        const auto &items = doc.values.front().members;
        QCOMPARE(items[0].text, QStringLiteral("\"\\/\b\f\n\r\t"));
        QCOMPARE(items[1].text, QStringLiteral("Aé🙂"));
        QCOMPARE(items[2].text.size(), 3);
        QCOMPARE(items[2].text[1], QChar(0));
    }

    void oneAfterAnother()
    {
        const JsonDocument doc = parseJson(QStringLiteral("{\"a\":1}\n{\"a\":2}\r\n\n [3] \"x\"\n"));
        QVERIFY(!doc.error);
        QCOMPARE(doc.values.size(), size_t(4));
        QCOMPARE(doc.values[1].find(u"a")->text, QStringLiteral("2"));
        QVERIFY(parseJson(QStringLiteral("{}{}")).values.size() == 2);
    }

    void nothing()
    {
        const QString mark{QChar(QChar::ByteOrderMark)};
        for (const QString &text : {QString(), QStringLiteral("  \n\t\r "), mark}) {
            const JsonDocument doc = parseJson(text);
            QVERIFY(!doc.error);
            QVERIFY(doc.values.empty());
        }
        QCOMPARE(parseJson(mark + QStringLiteral("{}")).values.front().begin, 1);
    }

    void mistakes_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("at");
        QTest::addColumn<int>("line");
        QTest::addColumn<QString>("says");
        const auto row = [](const char *text, int at, int line, const char *says) {
            QTest::newRow(text) << QString::fromUtf8(text) << at << line << QString::fromUtf8(says);
        };
        row("{", 1, 1, "ends too soon");
        row("[1,", 3, 1, "ends too soon");
        row("{\"a\"", 4, 1, "ends too soon");
        row("{\"a\":", 5, 1, "ends too soon");
        row("[1\n2]", 3, 2, "expected ‘,’ or ‘]’");
        row("{\"a\":1 \"b\":2}", 7, 1, "expected ‘,’ or ‘}’");
        row("{a:1}", 1, 1, "a name in double quotes");
        row("{'a':1}", 1, 1, "a name in double quotes");
        row("{\"a\":1,}", 7, 1, "a name in double quotes");
        row("[1,]", 3, 1, "expected a value");
        row("{\"a\" 1}", 5, 1, "expected ‘:’");
        row("[NaN]", 1, 1, "expected a value");
        row("[True]", 1, 1, "expected a value");
        row("[nullx]", 1, 1, "expected a value");
        row("[// no\n1]", 1, 1, "expected a value");
        row("{} x", 3, 1, "expected a value");
        row("{},{}", 2, 1, "expected a value");
        row("[\"a\nb\"]", 1, 1, "does not end");
        row("[\"a", 1, 1, "does not end");
        row("[\"a\tb\"]", 3, 1, "control character");
        row("[\"\\x\"]", 2, 1, "not an escape");
        row("[\"\\u12\"]", 2, 1, "four hexadecimal digits");
        row("[\"\\u+123\"]", 2, 1, "four hexadecimal digits");
        row("[\"\\ud83d\"]", 2, 1, "half of a character");
        row("[\"\\ud83d\\u0041\"]", 2, 1, "half of a character");
        row("[\"\\ude42\"]", 2, 1, "half of a character");
        row("[01]", 1, 1, "cannot begin with 0");
        row("[-]", 1, 1, "not a number");
        row("[1.]", 1, 1, "not a number");
        row("[1e]", 1, 1, "not a number");
        row("[.5]", 1, 1, "expected a value");
        row("[+1]", 1, 1, "expected a value");
        row("\n\n\n  ]", 5, 4, "expected a value");
        row("[\"\\", 1, 1, "");
    }
    void mistakes()
    {
        QFETCH(QString, text);
        QFETCH(int, at);
        QFETCH(int, line);
        QFETCH(QString, says);
        const JsonDocument doc = parseJson(text);
        QVERIFY(doc.error);
        QVERIFY(doc.values.empty());
        QVERIFY2(doc.error->text.contains(says), qPrintable(doc.error->text));
        if (!says.isEmpty())
            QCOMPARE(doc.error->at, at);
        QCOMPARE(doc.error->line, line);
    }

    void everyBeginningOfItIsSafe()
    {
        const QString text = QString::fromUtf8(
            "{\"a\": [1.5e-3, -0, \"\\ud83d\\ude42 \\u00e9 \\\"\", true, null], \"b\": {\"c\": [], \"d\": {}}}");
        for (qsizetype i = 0; i < text.size(); ++i)
            QVERIFY(i == 0 || parseJson(text.first(i)).error);
        QVERIFY(!parseJson(text).error);
    }

    void tooDeep()
    {
        const auto nested = [](int depth) { return QString(depth, u'[') + QString(depth, u']'); };
        QVERIFY(!parseJson(nested(400)).error);
        const JsonDocument doc = parseJson(nested(100'000));
        QVERIFY(doc.error);
        QVERIFY(doc.error->text.contains(QStringLiteral("too deeply")));
    }

    void quoting()
    {
        QCOMPARE(quoteJson(u"a\"b\\c\n\t\r\b\f\x01é🙂"),
                 QString::fromUtf8("\"a\\\"b\\\\c\\n\\t\\r\\b\\f\\u0001é🙂\""));
        for (const QString &text : {QStringLiteral("plain"), QStringLiteral("\x1f\x7f\""), QString(QChar(0))}) {
            const JsonDocument doc = parseJson(quoteJson(text));
            QVERIFY(!doc.error);
            QCOMPARE(doc.values.front().text, text);
        }
    }
};

QTEST_GUILESS_MAIN(TestJson)
#include "tst_json.moc"
