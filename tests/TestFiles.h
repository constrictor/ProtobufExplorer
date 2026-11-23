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

#pragma once

#include "Schema.h"
#include "SchemaLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

// A folder of files for a test to open.
class TestFiles
{
public:
    TestFiles() { Q_ASSERT(m_dir.isValid()); }

    QString path(const QString &name = QString()) const { return m_dir.filePath(name); }

    TestFiles &add(const QString &name, const QByteArray &bytes)
    {
        const QString path = m_dir.filePath(name);
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
            qFatal("cannot write %s", qPrintable(path));
        return *this;
    }

    // The same, with the syntax line supplied.
    TestFiles &proto(const QString &name, const QByteArray &body)
    {
        return add(name, "syntax = \"proto3\";\n" + body);
    }

    std::shared_ptr<const protoexplorer::Schema> load(const QString &name = QString()) const
    {
        return protoexplorer::loadSchema(path(name));
    }

private:
    QTemporaryDir m_dir;
};

inline const protoexplorer::Decl *findIn(
    const std::vector<std::unique_ptr<protoexplorer::Decl>> &decls, const QString &fullName)
{
    for (const auto &decl : decls) {
        if (decl->fullName == fullName)
            return decl.get();
        for (const auto *inner : {&decl->members, &decl->nested}) {
            if (const auto *found = findIn(*inner, fullName))
                return found;
        }
    }
    return nullptr;
}

// By full name, in the file whose shown path ends like so.
inline const protoexplorer::Decl *findDecl(const protoexplorer::Schema &schema,
                                           const QString &fullName,
                                           const QString &file = QString())
{
    for (const auto &f : schema.files) {
        if (!f->displayPath.endsWith(file))
            continue;
        if (const auto *found = findIn(f->decls, fullName))
            return found;
    }
    return nullptr;
}

inline QStringList rootNames(const protoexplorer::Schema &schema)
{
    QStringList result;
    for (const auto *decl : schema.roots)
        result << decl->fullName;
    return result;
}

inline QString allNotes(const protoexplorer::Schema &schema)
{
    QStringList result;
    for (const auto &note : schema.notes)
        result << note.path + u':' + QString::number(note.line) + u' ' + note.text;
    return result.join(u'\n');
}
