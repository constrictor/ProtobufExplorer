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

// Protobuf's own schemas, which lean on every feature there is.

#include "SchemaTree.h"
#include "TestFiles.h"

#include <QElapsedTimer>
#include <QSet>

using namespace protoexplorer;

class TestCorpus : public QObject
{
    Q_OBJECT

    static void check(const Decl &decl)
    {
        const QByteArray &bytes = decl.file->text.bytes();
        const QString where = decl.file->displayPath + u' ' + decl.fullName;
        QVERIFY2(!decl.name.isEmpty(), qPrintable(where));
        QVERIFY2(!decl.span.isEmpty(), qPrintable(where));
        QVERIFY2(decl.span.begin >= 0 && decl.span.end <= bytes.size(), qPrintable(where));

        const QByteArray source = bytes.sliced(decl.span.begin, decl.span.end - decl.span.begin);
        QVERIFY2(source.endsWith(';') || source.endsWith('}'), qPrintable(where + u'\n' + QString::fromUtf8(source)));
        static const QHash<DeclKind, QByteArray> keyword{
            {DeclKind::Message, "message"}, {DeclKind::Enum, "enum"}, {DeclKind::Service, "service"},
            {DeclKind::Extend, "extend"}, {DeclKind::Oneof, "oneof"}, {DeclKind::Method, "rpc"}};
        if (const auto it = keyword.constFind(decl.kind); it != keyword.cend()) {
            const bool group = decl.kind == DeclKind::Message && source.contains("group");
            QVERIFY2(source.startsWith(*it) || group || source.startsWith("export") || source.startsWith("local"),
                     qPrintable(where + u'\n' + QString::fromUtf8(source.left(60))));
        }
        QVERIFY2(source.contains(decl.name.toUtf8()) || source.toLower().contains(decl.name.toUtf8()),
                 qPrintable(where + u'\n' + QString::fromUtf8(source)));

        const QString snippet = decl.file->text.snippet(decl.span).text;
        QVERIFY2(!snippet.isEmpty() && !snippet.front().isSpace(), qPrintable(where + u'\n' + snippet));
        for (const auto &member : decl.members)
            check(*member);
        for (const auto &inner : decl.nested)
            check(*inner);
    }

    static void collect(const Decl &decl, QSet<const Decl *> &out)
    {
        if (decl.kind == DeclKind::Message)
            out.insert(&decl);
        for (const auto &inner : decl.nested)
            collect(*inner, out);
    }

private slots:
    void protobufItself()
    {
        QElapsedTimer timer;
        timer.start();
        const auto schema = loadSchema(QStringLiteral(CORPUS_DIR));
        QVERIFY(schema->files.size() > 50);
        qInfo("%d files, %d roots, %d notes in %d ms", int(schema->files.size()), int(schema->roots.size()),
              int(schema->notes.size()), int(timer.elapsed()));

        QSet<const Decl *> messages;
        for (const auto &file : schema->files) {
            // Those with mistakes in them, on purpose, promise nothing.
            const bool broken = std::any_of(file->notes.begin(), file->notes.end(), [](const Note &n) {
                return !n.text.startsWith(QStringLiteral("Import not found"));
            });
            for (const auto &decl : file->decls) {
                if (!broken)
                    check(*decl);
                if (file->opened)
                    collect(*decl, messages);
            }
        }
        if (QTest::currentTestFailed())
            return;

        SchemaTree tree(schema);
        QSet<const Decl *> found;
        for (const SchemaTree::Node *node : tree.messages()) {
            QVERIFY(!node->recursive);
            found.insert(SchemaTree::typeOf(*node->decl));
        }
        messages.subtract(found);
        QVERIFY2(messages.isEmpty(), qPrintable(messages.isEmpty() ? QString() : (*messages.begin())->fullName));
    }
};

QTEST_GUILESS_MAIN(TestCorpus)
#include "tst_corpus.moc"
