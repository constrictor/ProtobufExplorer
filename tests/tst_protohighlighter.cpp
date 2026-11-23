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

#include "ProtoHighlighter.h"
#include "UiColors.h"

#include <QTest>

using namespace protoexplorer;

class TestProtoHighlighter : public QObject
{
    Q_OBJECT

    // "K:message T:int32 N:1" for a line, with the comment state carried in and out.
    static QString describe(const QString &line, bool &inComment)
    {
        static const char letters[] = {'C', 'S', 'N', 'K', 'T'};
        QStringList result;
        for (const SyntaxToken &t : tokenizeProto(line, inComment))
            result << QChar::fromLatin1(letters[int(t.role)]) + u':' + line.sliced(t.start, t.length);
        return result.join(u' ');
    }

private slots:
    void lines_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("expected");
        const auto row = [](const char *line, const char *expected) {
            QTest::newRow(line) << QString::fromUtf8(line) << QString::fromUtf8(expected);
        };
        row("", "");
        row("syntax = \"proto3\";", "K:syntax S:\"proto3\"");
        row("edition = \"2023\";", "K:edition S:\"2023\"");
        row("package shop.v1;", "K:package");
        row("import public \"a/b.proto\";", "K:import K:public S:\"a/b.proto\"");
        row("message Order {", "K:message");
        row("export message Order {", "K:export K:message");
        row("  repeated Item items = 2;", "K:repeated N:2");
        row("  map<string, int32> m = 4;", "K:map T:string T:int32 N:4");
        row("  optional int64 id = 1 [deprecated = true];", "K:optional T:int64 N:1 N:true");
        row("  rpc Get(stream Req) returns (Res);", "K:rpc K:stream K:returns");
        row("  reserved 1 to 5, 9 to max;", "K:reserved N:1 K:to N:5 N:9 K:to K:max");
        row("  double d = 1 [default = -1.5e+10];", "T:double N:1 N:1.5e+10");
        row("  int32 a = 0x1F;", "T:int32 N:0x1F");
        // Words that are names here.
        row("  string message = 1;", "T:string N:1");
        row("  Message string = 1;", "N:1");
        row("  int32 to = 1;", "T:int32 N:1");
        row("  bool package = 2;", "T:bool N:2");
        row("  foo.message.Bar b = 1;", "N:1");
        row("  option (my.option) = { message: 1 };", "K:option N:1");
        row("  string syntax = 3;", "T:string N:3");
        // Comments and strings inside one another.
        row("string u = \"http://x\"; // real", "T:string S:\"http://x\" C:// real");
        row("// message \"x\"", "C:// message \"x\"");
        row("a = '\\'//'; // c", "S:'\\'//' C:// c");
        row("a = \"unclosed // not a comment", "S:\"unclosed // not a comment");
        row("a = \"ends in \\", "S:\"ends in \\");
        row("int32 /* why */ a = 1;", "T:int32 C:/* why */ N:1");
        row("/* // */ message M", "C:/* // */ K:message");
        row("\xD0\x94\xD0\xBE\xD0\xBA message M", "K:message");
    }

    void lines()
    {
        QFETCH(QString, line);
        bool inComment = false;
        QTEST(describe(line, inComment), "expected");
        QVERIFY(!inComment);
    }

    void blockCommentsSpanLines()
    {
        bool inComment = false;
        QCOMPARE(describe(QStringLiteral("int32 a; /* one"), inComment), QStringLiteral("T:int32 C:/* one"));
        QVERIFY(inComment);
        QCOMPARE(describe(QStringLiteral("message \"two\""), inComment), QStringLiteral("C:message \"two\""));
        QVERIFY(inComment);
        QCOMPARE(describe(QString(), inComment), QString());
        QVERIFY(inComment);
        QCOMPARE(describe(QStringLiteral("three */ message M"), inComment), QStringLiteral("C:three */ K:message"));
        QVERIFY(!inComment);
    }

    void coloursReadOnBothThemes_data()
    {
        QTest::addColumn<QColor>("base");
        QTest::addColumn<QColor>("text");
        QTest::newRow("light") << QColor(Qt::white) << QColor(Qt::black);
        QTest::newRow("dark") << QColor(0x1e1e1e) << QColor(0xdcdcdc);
        QTest::newRow("lighter dark") << QColor(0x2b2b2b) << QColor(0xdcdcdc);
    }

    void coloursReadOnBothThemes()
    {
        QFETCH(QColor, base);
        QFETCH(QColor, text);
        QPalette palette;
        for (const auto role : {QPalette::Base, QPalette::Window})
            palette.setColor(role, base);
        for (const auto role : {QPalette::Text, QPalette::WindowText})
            palette.setColor(role, text);
        for (const auto role : {SyntaxRole::Comment, SyntaxRole::String, SyntaxRole::Number, SyntaxRole::Keyword, SyntaxRole::Type}) {
            const qreal ratio = contrastRatio(syntaxColor(palette, role), base);
            QVERIFY2(ratio >= 4.5, qPrintable(QStringLiteral("role %1: %2").arg(int(role)).arg(ratio)));
        }
        QVERIFY(contrastRatio(errorColor(palette), base) >= 4.5);
        QVERIFY(contrastRatio(mutedColor(palette), base) >= 4.5);
    }
};

QTEST_MAIN(TestProtoHighlighter)
#include "tst_protohighlighter.moc"
