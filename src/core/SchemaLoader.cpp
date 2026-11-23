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

#include "SchemaLoader.h"

#include "ProtoParser.h"
#include "SchemaGraph.h"
#include "TypeResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace protoexplorer {

namespace {

constexpr qint64 kSniffSize = 8 * 1024;
constexpr QLatin1String kBundled(":/protobuf/");

QString tr(const char *text)
{
    return QCoreApplication::translate("SchemaLoader", text);
}

// The .pb extension belongs to schema text for some and to serialized data for most. Text
// has no control characters in it but those that lay it out.
bool looksBinary(QByteArrayView head)
{
    return std::any_of(head.begin(), head.end(), [](char c) {
        const auto u = uchar(c);
        return u < 0x20 && (u < '\t' || u > '\r');
    });
}

class Loader
{
public:
    Loader(QString path, const std::function<bool()> &canceled)
        : m_canceled(canceled)
    {
        m_schema->path = std::move(path);
    }

    std::shared_ptr<const Schema> load();

private:
    bool canceled() const { return m_canceled && m_canceled(); }
    void note(const QString &path, const QString &text) { m_schema->notes.push_back({path, 0, text}); }

    // Paths to open, relative to the base.
    void scan(QStringList &names) const;
    // From the base, unless that is the long way round.
    QString displayPath(const QString &path) const;
    SourceFile *open(const QString &path, const QString &displayPath, bool opened);
    void loadImports(SourceFile &file);
    SourceFile *findImport(const SourceFile &from, const QString &import);

    std::shared_ptr<Schema> m_schema = std::make_shared<Schema>();
    QDir m_base; // the folder opened, or the one the opened file is in
    const std::function<bool()> &m_canceled;
    QHash<QString, SourceFile *> m_byPath; // null for one that could not be read
};

void Loader::scan(QStringList &names) const
{
    // Neither follows links to folders nor descends into hidden ones.
    QDirIterator it(m_base.path(), {QStringLiteral("*.proto"), QStringLiteral("*.pb")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && !canceled())
        names << m_base.relativeFilePath(it.next());
    names.sort();
}

QString Loader::displayPath(const QString &path) const
{
    const QString relative = m_base.relativeFilePath(path);
    return relative.size() < path.size() ? relative : path;
}

SourceFile *Loader::open(const QString &path, const QString &displayPath, bool opened)
{
    const QString canonical = path.startsWith(u':') ? path : QFileInfo(path).canonicalFilePath();
    const QString key = canonical.isEmpty() ? path : canonical;
    if (const auto it = m_byPath.constFind(key); it != m_byPath.cend())
        return *it;
    m_byPath.insert(key, nullptr);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        note(displayPath, tr("Cannot be read: %1").arg(file.errorString()));
        return nullptr;
    }
    QByteArray bytes = file.read(kSniffSize);
    if (!path.endsWith(QLatin1String(".proto"), Qt::CaseInsensitive) && looksBinary(bytes)) {
        note(displayPath, tr("Skipped: this is binary data, not schema text."));
        return nullptr;
    }
    bytes += file.readAll();

    auto parsed = parseProto(key, std::move(bytes));
    parsed->displayPath = displayPath;
    parsed->opened = opened;
    for (Note &n : parsed->notes)
        n.path = displayPath;
    if (!parsed->notes.empty() && parsed->decls.empty() && parsed->imports.empty()
        && path.endsWith(QLatin1String(".pb"), Qt::CaseInsensitive)) {
        // Text, but not a schema: most likely a message in text format.
        note(displayPath, tr("Skipped: this does not look like a schema."));
        return nullptr;
    }

    SourceFile *result = parsed.get();
    m_schema->files.push_back(std::move(parsed));
    m_byPath.insert(key, result);
    return result;
}

SourceFile *Loader::findImport(const SourceFile &from, const QString &import)
{
    const QStringList parts = import.split(u'/');
    if (import.isEmpty() || QDir::isAbsolutePath(import) || import.startsWith(u':')
        || parts.contains(QStringLiteral(".."))) {
        return nullptr;
    }

    // Imports are written from a root that nothing in the file names, but that is one of
    // the folders the file is in.
    if (!from.path.startsWith(u':')) {
        QDir dir = QFileInfo(from.path).dir();
        while (true) {
            const QString path = dir.filePath(import);
            if (QFileInfo(path).isFile()) {
                if (SourceFile *file = open(path, displayPath(path), false))
                    return file;
            }
            if (dir.isRoot() || !dir.cdUp())
                break;
        }

        // Or it is a folder off to the side.
        const QString tail = u'/' + import;
        SourceFile *best = nullptr;
        for (const auto &file : m_schema->files) {
            if (file->opened && file->path.endsWith(tail)
                && (!best || file->path.size() < best->path.size())) {
                best = file.get();
            }
        }
        if (best)
            return best;
    }

    const QString bundled = kBundled + import;
    if (QFile::exists(bundled))
        return open(bundled, tr("%1 (bundled)").arg(import), false);
    return nullptr;
}

void Loader::loadImports(SourceFile &file)
{
    for (Import &import : file.imports) {
        import.file = findImport(file, import.path);
        if (!import.file)
            file.notes.push_back({file.displayPath, 0, tr("Import not found: %1").arg(import.path)});
    }
}

std::shared_ptr<const Schema> Loader::load()
{
    const QFileInfo info(m_schema->path);
    if (!info.exists()) {
        note(m_schema->path, tr("There is no such file or folder."));
        return m_schema;
    }

    m_base = QDir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    QStringList names;
    if (info.isDir())
        scan(names);
    else
        names << info.fileName();
    for (const QString &name : std::as_const(names)) {
        if (canceled())
            return nullptr;
        open(m_base.filePath(name), name, true);
    }
    if (info.isDir() && names.isEmpty() && !canceled())
        note(m_schema->path, tr("There are no .proto or .pb files in this folder."));

    // Grows as it goes: imports have imports.
    for (size_t i = 0; i < m_schema->files.size(); ++i) {
        if (canceled())
            return nullptr;
        loadImports(*m_schema->files[i]);
    }
    if (canceled())
        return nullptr;

    resolveTypes(*m_schema);
    m_schema->roots = findRoots(*m_schema);
    for (const auto &file : m_schema->files) {
        // Somebody else's file is not the reader's to fix.
        if (file->opened)
            m_schema->notes.insert(m_schema->notes.end(), file->notes.begin(), file->notes.end());
    }
    return m_schema;
}

} // namespace

std::shared_ptr<const Schema> loadSchema(const QString &path, const std::function<bool()> &canceled)
{
    return Loader(path, canceled).load();
}

} // namespace protoexplorer
