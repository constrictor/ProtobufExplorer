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

#include "SchemaIndex.h"
#include "SchemaModel.h"

#include <QFutureWatcher>
#include <QWidget>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QStackedWidget;
class QTabWidget;
class QToolButton;
QT_END_NAMESPACE

namespace protoexplorer {

class FindBar;
class JsonView;
class SchemaTreeView;
class SourceView;

// One opened file or folder: its trees, and the source of whatever is selected in them. Any
// JSON laid over its messages goes into views of its own beside that.
class SchemaTab : public QWidget
{
    Q_OBJECT

public:
    explicit SchemaTab(const QString &path, QWidget *parent = nullptr);
    ~SchemaTab() override;

    QString path() const { return m_path; }
    bool isLoading() const { return !m_model; }

    void activateFind();
    void find(bool forward);

    // There is a message for JSON to be laid over.
    bool takesJson() const;
    // The message that is selected, or null if none is.
    const Decl *jsonTarget() const;
    // Lay a JSON file over a message, or over the likeliest if there is none. One given by
    // name may be asked for before the schema is loaded.
    void openJson(const QString &path, const Decl *message);
    void openJson(const QString &path, const QString &message);
    // The same with what is in the clipboard.
    void pasteJson(const Decl *message);
    JsonView *currentJson() const;
    // Close the one that is showing, if one is.
    bool closeCurrentJson();

signals:
    void loaded();
    // The reader is to say which file.
    void jsonWanted(const protoexplorer::Decl *message);

private:
    struct PendingJson
    {
        QString path;
        QString message;
    };

    QWidget *makeContents();
    JsonView *addJson(const Decl *message, const QString &name, const QString &path, const QString &text);
    void closeView(int index);
    void showMenu(const QPoint &at);
    std::shared_ptr<const SchemaIndex> schemaIndex();
    // Of these messages, or of all of them if there are none.
    const Decl *likeliest(const QString &json, const std::vector<const Decl *> &among);
    void openJson(const QString &path, const std::vector<const Decl *> &among, QString note);
    void finishLoading();
    void showNotes();
    void showSource(const QModelIndex &index);
    void runFind(bool forward, bool fromStart);
    void updateMatches();

    QString m_path;
    QFutureWatcher<std::shared_ptr<const Schema>> m_watcher;
    std::shared_ptr<const Schema> m_schema;
    std::shared_ptr<const SchemaIndex> m_index; // made when first needed
    SchemaModel *m_model = nullptr;
    std::vector<PendingJson> m_pendingJson;
    int m_pasted = 0;

    // What the pattern last looked for matches, in the order of the tree.
    QString m_pattern;
    std::vector<SchemaTree::Node *> m_matches;

    QStackedWidget *m_pages = nullptr;
    QTabWidget *m_views = nullptr; // the schema first, then the JSON
    QLabel *m_loading = nullptr;
    FindBar *m_findBar = nullptr;
    SchemaTreeView *m_tree = nullptr;
    QLabel *m_summary = nullptr;
    QToolButton *m_problems = nullptr;
    QListWidget *m_problemList = nullptr;
    SourceView *m_source = nullptr;
};

} // namespace protoexplorer
