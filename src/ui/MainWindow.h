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

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QAction;
class QStackedWidget;
class QTabWidget;
QT_END_NAMESPACE

namespace protoexplorer {

struct Decl;
class SchemaTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Open a file or a folder in a tab of its own, or go to the tab it is already in.
    void openPath(const QString &path);
    // Open schemas, and over each the JSON files that come after it: over the message of that
    // name, or over the likeliest. Returns what is wrong with the list, if anything is.
    QString openAll(const QStringList &paths, const QString &message = QString());

    QTabWidget *tabs() const { return m_tabs; }

private:
    void makeActions();
    QWidget *makeWelcome();
    void chooseFiles();
    void chooseFolder();
    void chooseJson(SchemaTab &tab, const Decl *message);
    void closeTab(int index);
    SchemaTab *currentTab() const;
    void updateState();
    // Where the last thing opened was.
    QString startFolder() const;

    QTabWidget *m_tabs = nullptr;
    QStackedWidget *m_pages = nullptr;
    QAction *m_openFile = nullptr;
    QAction *m_openFolder = nullptr;
    QAction *m_openJson = nullptr;
    QAction *m_pasteJson = nullptr;
    QAction *m_close = nullptr;
    QAction *m_find = nullptr;
    QAction *m_findNext = nullptr;
    QAction *m_findPrevious = nullptr;
    QString m_lastFolder;
};

} // namespace protoexplorer
