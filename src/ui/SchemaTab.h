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

#include "SchemaModel.h"

#include <QFutureWatcher>
#include <QWidget>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QStackedWidget;
class QToolButton;
QT_END_NAMESPACE

namespace protoexplorer {

class FindBar;
class SchemaTreeView;
class SourceView;

// One opened file or folder: its trees, and the source of whatever is selected in them.
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

signals:
    void loaded();

private:
    QWidget *makeContents();
    void finishLoading();
    void showNotes();
    void showSource(const QModelIndex &index);
    void runFind(bool forward, bool fromStart);
    void updateMatches();

    QString m_path;
    QFutureWatcher<std::shared_ptr<const Schema>> m_watcher;
    SchemaModel *m_model = nullptr;

    // What the pattern last looked for matches, in the order of the tree.
    QString m_pattern;
    std::vector<SchemaTree::Node *> m_matches;

    QStackedWidget *m_pages = nullptr;
    QLabel *m_loading = nullptr;
    FindBar *m_findBar = nullptr;
    SchemaTreeView *m_tree = nullptr;
    QLabel *m_summary = nullptr;
    QToolButton *m_problems = nullptr;
    QListWidget *m_problemList = nullptr;
    SourceView *m_source = nullptr;
};

} // namespace protoexplorer
