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

#include "SourceText.h"

#include <QTest>

using namespace protoexplorer;

class TestSourceText : public QObject
{
    Q_OBJECT

private slots:
    void offsetCountsTabsToMultiplesOfEight()
    {
        const SourceText text("\tint32 a = 1;\n  \tb\n");
        QCOMPARE(text.offsetAt(0, 8), 1);
        QCOMPARE(text.offsetAt(0, 14), 7);
        QCOMPARE(text.offsetAt(1, 8), 17); // two spaces, then a tab that only adds six
    }

    void offsetCountsTheByteOrderMark()
    {
        const SourceText text("\xEF\xBB\xBFmessage A {}\n");
        QCOMPARE(text.offsetAt(0, 3), 3);
    }

    void offsetCountsBytesNotCharacters()
    {
        const SourceText text("/* \xC3\xA9 */ message A {}");
        QCOMPARE(text.offsetAt(0, 9), 9);
    }

    void offsetPastTheEndIsClamped()
    {
        const SourceText text("abc\nde");
        QCOMPARE(text.offsetAt(0, 99), 3);
        QCOMPARE(text.offsetAt(7, 0), 6);
    }

    void commentsIgnoreStrings()
    {
        const SourceText text("a = \"http://x\"; // real\nb = '/*'; c = \"\\\"//\";\n");
        QCOMPARE(text.comments().size(), size_t(1));
        QCOMPARE(text.bytes().sliced(text.comments()[0].begin, 7), QByteArray("// real"));
    }

    void unterminatedStringEndsWithItsLine()
    {
        const SourceText text("a = \"oops\n// seen\n");
        QCOMPARE(text.comments().size(), size_t(1));
    }

    void snippet_data()
    {
        QTest::addColumn<QByteArray>("source");
        QTest::addColumn<QByteArray>("declaration"); // located by its text
        QTest::addColumn<QString>("expected");
        QTest::addColumn<int>("firstLine");
        QTest::addColumn<int>("lastLine");

        const auto row = [](const char *name, const char *source, const char *decl,
                            const char *expected, int first, int last) {
            QTest::newRow(name) << QByteArray(source) << QByteArray(decl)
                                << QString::fromUtf8(expected) << first << last;
        };

        row("plain", "\nmessage A {\n  int32 x = 1;\n}\n", "message A {\n  int32 x = 1;\n}",
            "message A {\n  int32 x = 1;\n}", 2, 4);
        row("dedented", "message A {\n  message B {\n    int32 x = 1;\n  }\n}\n",
            "message B {\n    int32 x = 1;\n  }", "message B {\n  int32 x = 1;\n}", 2, 4);
        row("leading line comments", "// One.\n// Two.\nmessage A {}\n", "message A {}",
            "// One.\n// Two.\nmessage A {}", 1, 3);
        row("leading block comment", "/*\n * Doc.\n */\nmessage A {}\n", "message A {}",
            "/*\n * Doc.\n */\nmessage A {}", 1, 4);
        row("block comment on the same line", "  /* d */ int32 a = 1;\n", "int32 a = 1;",
            "/* d */ int32 a = 1;", 1, 1);
        row("detached comment left out", "// Detached.\n\n// Mine.\nmessage A {}\n",
            "message A {}", "// Mine.\nmessage A {}", 3, 4);
        row("previous line's comment left out", "int32 a = 1; // about a\nint32 b = 2;\n",
            "int32 b = 2;", "int32 b = 2;", 2, 2);
        row("comment after a brace left out", "message A { // about A\n  int32 x = 1;\n}\n",
            "int32 x = 1;", "int32 x = 1;", 2, 2);
        row("same line trailing", "  int32 a = 1; // about a\n  int32 b = 2;\n",
            "int32 a = 1;", "int32 a = 1; // about a", 1, 1);
        row("trailing goes on below", "  A = 1;    // long,\n            // longer\n            // done\n  B = 2;    // b\n",
            "A = 1;", "A = 1;    // long,\n          // longer\n          // done", 1, 3);
        row("not taken from above", "  A = 1;    // long,\n            // longer\n  B = 2;    // b\n",
            "B = 2;", "B = 2;    // b", 3, 3);
        row("not lined up is leading", "  A = 1;    // a\n  // about b\n  B = 2;\n",
            "B = 2;", "// about b\nB = 2;", 2, 3);
        row("leading after going on", "  A = 1;    // long,\n            // longer\n  // about b\n  B = 2;\n",
            "B = 2;", "// about b\nB = 2;", 3, 4);
        row("lined up with tabs", "\tA = 1;\t// long,\n\t\t// longer\n\tB = 2;\n",
            "A = 1;", "A = 1;\t// long,\n\t// longer", 1, 2);
        row("block comment before more code", "int32 a = 1; /* ? */ int32 b = 2;\n",
            "int32 a = 1;", "int32 a = 1;", 1, 1);
        row("trailing below, then blank", "  int32 a = 1;\n  // about a\n  // still\n\n  int32 b = 2;\n",
            "int32 a = 1;", "int32 a = 1;\n// about a\n// still", 1, 3);
        row("trailing below, then brace", "{\n  int32 a = 1;\n  // about a\n}\n", "int32 a = 1;",
            "int32 a = 1;\n// about a", 2, 3);
        row("below belongs to the next", "int32 a = 1;\n// about b\nint32 b = 2;\n",
            "int32 a = 1;", "int32 a = 1;", 1, 1);
        row("two on a line", "message A {} message B {}\n", "message B {}", "message B {}", 1, 1);
        row("crlf", "// Doc.\r\nmessage A {\r\n  int32 x = 1;\r\n}\r\n",
            "message A {\r\n  int32 x = 1;\r\n}", "// Doc.\nmessage A {\n  int32 x = 1;\n}", 1, 4);
        row("crlf blank line detaches", "// Detached.\r\n\r\nmessage A {}\r\n", "message A {}",
            "message A {}", 3, 3);
        row("byte order mark", "\xEF\xBB\xBF// Doc.\nmessage A {}\n", "message A {}",
            "// Doc.\nmessage A {}", 1, 2);
        row("no newline at the end", "message A {}", "message A {}", "message A {}", 1, 1);
        row("multibyte", "// \xD0\x94\xD0\xBE\xD0\xBA.\nmessage A {}\n", "message A {}",
            "// \xD0\x94\xD0\xBE\xD0\xBA.\nmessage A {}", 1, 2);
        row("blank line inside kept", "  message A {\n\n    int32 x = 1;\n  }\n",
            "message A {\n\n    int32 x = 1;\n  }", "message A {\n\n  int32 x = 1;\n}", 1, 4);
        row("tabs dedented", "\tmessage A {\n\t\tint32 x = 1;\n\t}\n",
            "message A {\n\t\tint32 x = 1;\n\t}", "message A {\n\tint32 x = 1;\n}", 1, 3);
    }

    void snippet()
    {
        QFETCH(QByteArray, source);
        QFETCH(QByteArray, declaration);
        const qsizetype at = source.indexOf(declaration);
        QVERIFY(at >= 0);
        const Snippet s = SourceText(source).snippet({at, at + declaration.size()});
        QTEST(s.text, "expected");
        QTEST(s.firstLine, "firstLine");
        QTEST(s.lastLine, "lastLine");
    }

    void emptySpanShowsItsLine()
    {
        const SourceText text("message A {}\n  message {\n}\n");
        QCOMPARE(text.snippet({15, 15}).text, QStringLiteral("message {"));
    }

    void spanBeyondTheEndIsClamped()
    {
        QCOMPARE(SourceText("abc").snippet({90, 99}).text, QString());
        QCOMPARE(SourceText().snippet({0, 0}).text, QString());
    }
};

QTEST_GUILESS_MAIN(TestSourceText)
#include "tst_sourcetext.moc"
