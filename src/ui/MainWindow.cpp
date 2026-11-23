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

#include "MainWindow.h"

#include "SchemaTab.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace protoexplorer {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_tabs = new QTabWidget;
    m_tabs->setObjectName(QStringLiteral("tabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::updateState);

    makeActions();
    m_pages = new QStackedWidget;
    m_pages->addWidget(makeWelcome());
    m_pages->addWidget(m_tabs);
    setCentralWidget(m_pages);
    resize(1100, 700);
    updateState();
}

void MainWindow::makeActions()
{
    const auto add = [this](QMenu *menu, const QString &text, const QKeySequence &key, auto slot) {
        QAction *action = menu->addAction(text);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    QMenu *file = menuBar()->addMenu(tr("&File"));
    m_openFile = add(file, tr("&Open File…"), QKeySequence::Open, [this] { chooseFiles(); });
    m_openFolder = add(file, tr("Open &Folder…"), QKeySequence(tr("Ctrl+Shift+O")),
                       [this] { chooseFolder(); });
    file->addSeparator();
    m_openJson = add(file, tr("Open &JSON…"), QKeySequence(tr("Ctrl+J")), [this] {
        if (SchemaTab *tab = currentTab())
            chooseJson(*tab, tab->jsonTarget());
    });
    m_pasteJson = add(file, tr("&Paste JSON"), QKeySequence(tr("Ctrl+Shift+V")), [this] {
        if (SchemaTab *tab = currentTab())
            tab->pasteJson(tab->jsonTarget());
    });
    file->addSeparator();
    // What is open the innermost goes first.
    m_close = add(file, tr("&Close Tab"), QKeySequence(tr("Ctrl+W")), [this] {
        SchemaTab *tab = currentTab();
        if (tab && !tab->closeCurrentJson())
            closeTab(m_tabs->currentIndex());
    });
    file->addSeparator();
    add(file, tr("&Quit"), QKeySequence::Quit, [this] { close(); });

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    m_find = add(edit, tr("&Find…"), QKeySequence::Find, [this] {
        if (SchemaTab *tab = currentTab())
            tab->activateFind();
    });
    m_findNext = add(edit, tr("Find &Next"), QKeySequence(Qt::Key_F3), [this] {
        if (SchemaTab *tab = currentTab())
            tab->find(true);
    });
    m_findPrevious = add(edit, tr("Find &Previous"), QKeySequence(Qt::SHIFT | Qt::Key_F3), [this] {
        if (SchemaTab *tab = currentTab())
            tab->find(false);
    });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *about = add(help, tr("&About Protobuf Explorer"), {}, [this] { showAbout(); });
    about->setObjectName(QStringLiteral("about"));
    about->setMenuRole(QAction::AboutRole);
    // The AppImage carries Qt, so it has to be able to name it.
    QAction *aboutQt = add(help, tr("About &Qt"), {}, [] { QApplication::aboutQt(); });
    aboutQt->setMenuRole(QAction::AboutQtRole);
}

QWidget *MainWindow::makeWelcome()
{
    auto *label = new QLabel(tr("Open a schema file, or a folder of them."));
    label->setAlignment(Qt::AlignCenter);
    auto *openFile = new QPushButton(tr("Open File…"));
    auto *openFolder = new QPushButton(tr("Open Folder…"));
    connect(openFile, &QPushButton::clicked, m_openFile, &QAction::trigger);
    connect(openFolder, &QPushButton::clicked, m_openFolder, &QAction::trigger);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(openFile);
    buttons->addWidget(openFolder);
    buttons->addStretch();
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addStretch();
    layout->addWidget(label);
    layout->addLayout(buttons);
    layout->addStretch();
    return page;
}

QString MainWindow::startFolder() const
{
    return m_lastFolder.isEmpty() ? QDir::homePath() : m_lastFolder;
}

void MainWindow::chooseFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open File"), startFolder(),
        tr("Protocol Buffers schemas (*.proto *.pb);;All files (*)"));
    for (const QString &path : paths)
        openPath(path);
}

void MainWindow::chooseFolder()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Open Folder"), startFolder());
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::chooseJson(SchemaTab &tab, const Decl *message)
{
    const QPointer<SchemaTab> still(&tab);
    const QString path = QFileDialog::getOpenFileName(
        this, message ? tr("Open JSON as %1").arg(message->name) : tr("Open JSON"), startFolder(),
        tr("JSON (*.json *.jsonl *.ndjson);;All files (*)"));
    if (path.isEmpty() || !still)
        return;
    m_lastFolder = QFileInfo(path).absolutePath();
    tab.openJson(path, message);
}

QString MainWindow::openAll(const QStringList &paths, const QString &message)
{
    static const QRegularExpression json(QStringLiteral("\\.(json|jsonl|ndjson)\\z"),
                                         QRegularExpression::CaseInsensitiveOption);
    bool any = false;
    for (const QString &path : paths) {
        if (!json.match(path).hasMatch() || QFileInfo(path).isDir())
            openPath(path);
        else if (!any)
            return tr("%1 is JSON, and needs a schema before it.").arg(path);
        else
            currentTab()->openJson(path, message);
        any = true;
    }
    return {};
}

void MainWindow::openPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QString key = canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    m_lastFolder = QFileInfo(key).absolutePath();

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (static_cast<SchemaTab *>(m_tabs->widget(i))->path() == key) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    QString title = QFileInfo(key).fileName();
    if (title.isEmpty())
        title = key;
    title.replace(u'&', QStringLiteral("&&")); // not a mnemonic
    auto *tab = new SchemaTab(key);
    connect(tab, &SchemaTab::loaded, this, &MainWindow::updateState);
    connect(tab, &SchemaTab::jsonWanted, this,
            [this, tab](const Decl *message) { chooseJson(*tab, message); });
    const int index = m_tabs->addTab(tab, title);
    m_tabs->setTabToolTip(index, QDir::toNativeSeparators(key));
    m_tabs->setCurrentIndex(index);
    updateState();
}

void MainWindow::closeTab(int index)
{
    QWidget *tab = m_tabs->widget(index);
    if (!tab)
        return;
    m_tabs->removeTab(index);
    tab->deleteLater();
    updateState();
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About Protobuf Explorer"));
    box.setTextFormat(Qt::PlainText);
    box.setText(tr("Protobuf Explorer %1\n"
                   "A visual explorer for Protocol Buffers schemas.\n\n"
                   "License: GPL-3.0-or-later\n"
                   "Free software with NO WARRANTY. See the LICENSE file shipped with it.")
                    .arg(QCoreApplication::applicationVersion()));
    // To be copied into a bug report.
    box.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    box.exec();
}

SchemaTab *MainWindow::currentTab() const
{
    return static_cast<SchemaTab *>(m_tabs->currentWidget());
}

void MainWindow::updateState()
{
    const SchemaTab *tab = currentTab();
    for (QAction *action : {m_close, m_find, m_findNext, m_findPrevious})
        action->setEnabled(tab);
    for (QAction *action : {m_openJson, m_pasteJson})
        action->setEnabled(tab && tab->takesJson());
    m_pages->setCurrentIndex(tab ? 1 : 0);
    setWindowTitle(tab ? tr("%1 — Protobuf Explorer").arg(QDir::toNativeSeparators(tab->path()))
                       : tr("Protobuf Explorer"));
}

} // namespace protoexplorer
