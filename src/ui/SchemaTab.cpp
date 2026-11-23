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

#include "SchemaTab.h"

#include "FindBar.h"
#include "JsonView.h"
#include "SchemaLoader.h"
#include "SchemaTreeView.h"
#include "SourceView.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPromise>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace protoexplorer {

using SchemaPtr = std::shared_ptr<const Schema>;

namespace {

// Past this it would only be in the way: all of it goes into an editor.
constexpr qint64 kMaxJsonSize = 64 * 1024 * 1024;

// UTF-8, unless it begins by saying otherwise.
QString decodeText(const QByteArray &bytes)
{
    QStringDecoder decoder(QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8));
    return decoder.decode(bytes);
}

} // namespace

SchemaTab::SchemaTab(const QString &path, QWidget *parent)
    : QWidget(parent)
    , m_path(path)
{
    m_loading = new QLabel(tr("Loading…"), this);
    m_loading->setAlignment(Qt::AlignCenter);
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(m_loading);
    m_views = new QTabWidget;
    m_views->setObjectName(QStringLiteral("views"));
    m_views->setDocumentMode(true);
    m_views->setTabsClosable(true);
    m_views->setTabBarAutoHide(true);
    m_views->addTab(makeContents(), tr("Schema"));
    for (const auto side : {QTabBar::LeftSide, QTabBar::RightSide})
        m_views->tabBar()->setTabButton(0, side, nullptr);
    connect(m_views, &QTabWidget::tabCloseRequested, this, &SchemaTab::closeView);
    m_pages->addWidget(m_views);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_pages);

    connect(&m_watcher, &QFutureWatcher<SchemaPtr>::finished, this, &SchemaTab::finishLoading);
    // Takes nothing of this tab along, so closing the tab need not wait for it.
    m_watcher.setFuture(QtConcurrent::run([path](QPromise<SchemaPtr> &promise) {
        if (SchemaPtr schema = loadSchema(path, [&promise] { return promise.isCanceled(); }))
            promise.addResult(std::move(schema));
    }));
}

SchemaTab::~SchemaTab()
{
    m_watcher.disconnect(this);
    m_watcher.cancel();
}

QWidget *SchemaTab::makeContents()
{
    m_findBar = new FindBar;
    m_findBar->setPrompt(tr("Find message…"));
    m_tree = new SchemaTreeView;
    m_tree->setObjectName(QStringLiteral("schemaTree"));
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->acceptFiles({QStringLiteral(".json"), QStringLiteral(".jsonl"), QStringLiteral(".ndjson")});

    m_summary = new QLabel;
    m_summary->setObjectName(QStringLiteral("summary"));
    m_summary->setContentsMargins(6, 4, 6, 4);
    m_problems = new QToolButton;
    m_problems->setObjectName(QStringLiteral("problems"));
    m_problems->setCheckable(true);
    m_problems->setAutoRaise(true);
    m_problems->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_problems->hide();
    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 4, 0);
    footer->addWidget(m_summary, 1);
    footer->addWidget(m_problems);

    m_problemList = new QListWidget;
    m_problemList->setObjectName(QStringLiteral("problemList"));
    m_problemList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_problemList->setMaximumHeight(160);
    m_problemList->hide();

    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftLayout->addWidget(m_findBar);
    leftLayout->addWidget(m_tree, 1);
    leftLayout->addWidget(m_problemList);
    leftLayout->addLayout(footer);

    m_source = new SourceView;

    auto *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(left);
    splitter->addWidget(m_source);
    splitter->setSizes({450, 550});

    connect(m_findBar, &FindBar::findRequested, this, &SchemaTab::runFind);
    connect(m_findBar, &FindBar::escapePressed, m_tree, qOverload<>(&QWidget::setFocus));
    connect(m_problems, &QToolButton::toggled, m_problemList, &QWidget::setVisible);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &SchemaTab::showMenu);
    connect(m_tree, &SchemaTreeView::filesDropped, this,
            [this](const QStringList &paths, const QModelIndex &on) {
                const SchemaTree::Node *node = m_model ? m_model->nodeAt(on) : nullptr;
                const Decl *message = node ? SchemaTree::messageAt(*node) : nullptr;
                for (const QString &path : paths)
                    openJson(path, message);
            });
    return splitter;
}

void SchemaTab::finishLoading()
{
    if (m_watcher.isCanceled() || !m_watcher.future().isResultReadyAt(0))
        return;
    const SchemaPtr schema = m_watcher.result();
    m_schema = schema;

    m_model = new SchemaModel(schema, this);
    m_tree->setModel(m_model);
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) { showSource(current); });

    const auto opened = std::count_if(schema->files.begin(), schema->files.end(),
                                      [](const auto &file) { return file->opened; });
    const auto roots = schema->roots.size();
    m_summary->setText((opened == 1 ? tr("1 file") : tr("%1 files").arg(opened))
                       + QStringLiteral(", ")
                       + (roots == 1 ? tr("1 root") : tr("%1 roots").arg(roots)));
    showNotes();

    m_pages->setCurrentIndex(1);
    if (m_model->rowCount() > 0) {
        m_tree->setCurrentIndex(m_model->index(0, 0));
        // A lone message is what the reader came for.
        if (m_model->rowCount() == 1)
            m_tree->expand(m_model->index(0, 0));
    }
    if (isVisible() && !m_findBar->hasFocus())
        m_tree->setFocus();
    for (const PendingJson &json : std::exchange(m_pendingJson, {}))
        openJson(json.path, json.message);
    emit loaded();
}

void SchemaTab::showNotes()
{
    const std::vector<Note> &notes = m_model->tree().schema().notes;
    for (const Note &note : notes) {
        const QString where = note.line > 0 ? QStringLiteral("%1:%2").arg(note.path).arg(note.line)
                                            : note.path;
        m_problemList->addItem(QStringLiteral("%1: %2").arg(where, note.text));
    }
    m_problems->setText(notes.size() == 1 ? tr("1 problem") : tr("%1 problems").arg(notes.size()));
    m_problems->setVisible(!notes.empty());
    // With nothing else to look at, they are what there is to say.
    m_problems->setChecked(!notes.empty() && m_model->rowCount() == 0);
}

void SchemaTab::showSource(const QModelIndex &index)
{
    if (const SchemaTree::Node *node = m_model->nodeAt(index))
        m_source->display(SchemaTree::sourceParts(*node->decl));
    else
        m_source->clear();
}

bool SchemaTab::takesJson() const
{
    return m_model
        && std::any_of(m_schema->files.begin(), m_schema->files.end(), [](const auto &file) {
               return std::any_of(file->decls.begin(), file->decls.end(), [](const auto &decl) {
                   return decl->kind == DeclKind::Message;
               });
           });
}

const Decl *SchemaTab::jsonTarget() const
{
    if (const JsonView *json = currentJson())
        return json->message();
    const SchemaTree::Node *node = m_model ? m_model->nodeAt(m_tree->currentIndex()) : nullptr;
    return node ? SchemaTree::messageAt(*node) : nullptr;
}

std::shared_ptr<const SchemaIndex> SchemaTab::schemaIndex()
{
    if (!m_index)
        m_index = std::make_shared<SchemaIndex>(m_schema);
    return m_index;
}

const Decl *SchemaTab::likeliest(const QString &json, const std::vector<const Decl *> &among)
{
    if (among.size() == 1)
        return among.front();
    for (const MessageMatch &match : rankMessages(*schemaIndex(), topLevelKeys(parseJson(json)))) {
        if (among.empty() || std::find(among.begin(), among.end(), match.message) != among.end())
            return match.message;
    }
    return nullptr;
}

void SchemaTab::openJson(const QString &path, const Decl *message)
{
    openJson(path, message ? std::vector{message} : std::vector<const Decl *>(), QString());
}

void SchemaTab::openJson(const QString &path, const QString &message)
{
    if (!m_model) {
        m_pendingJson.push_back({path, message});
        return;
    }
    if (!takesJson()) {
        m_problemList->addItem(tr("%1: there is no message here to read it as").arg(path));
        m_problemList->show();
        return;
    }
    if (message.isEmpty())
        return openJson(path, {}, QString());
    const std::vector<const Decl *> named = schemaIndex()->messagesNamed(message);
    QString note;
    if (named.empty())
        note = tr("There is no message %1 here, so this is the likeliest.").arg(message);
    else if (named.size() > 1)
        note = tr("%1 messages are called %2, and this is the likeliest.").arg(named.size()).arg(message);
    openJson(path, named, note);
}

void SchemaTab::openJson(const QString &path, const std::vector<const Decl *> &among, QString note)
{
    if (!takesJson())
        return;
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                                                 : info.canonicalFilePath();
    QFile file(canonical);
    QString problem;
    if (info.isDir())
        problem = tr("it is a folder");
    else if (info.size() > kMaxJsonSize)
        problem = tr("it is larger than %1 MB").arg(kMaxJsonSize / 1024 / 1024);
    else if (!file.open(QIODevice::ReadOnly))
        problem = file.errorString();
    const bool readable = problem.isEmpty();
    const QString text = readable ? decodeText(file.readAll()) : QString();
    if (!readable)
        note = tr("Cannot read %1: %2").arg(QDir::toNativeSeparators(path), problem);
    const Decl *message = likeliest(text, among);

    // The same again is to see it anew, unless it has been written on since.
    JsonView *view = nullptr;
    for (int i = 1; i < m_views->count() && !view; ++i) {
        auto *open = static_cast<JsonView *>(m_views->widget(i));
        if (readable && open->path() == canonical && !open->isModified() && open->message() == message)
            view = open;
    }
    if (view) {
        view->setText(text);
        m_views->setCurrentWidget(view);
    } else {
        // One that is only the likeliest is not to be held to.
        view = addJson(among.empty() ? nullptr : message, info.fileName(), canonical, text);
    }
    view->setNote(note);
}

void SchemaTab::pasteJson(const Decl *message)
{
    if (!takesJson())
        return;
    ++m_pasted;
    addJson(message, m_pasted == 1 ? tr("pasted") : tr("pasted %1").arg(m_pasted), QString(),
            QGuiApplication::clipboard()->text());
}

JsonView *SchemaTab::addJson(const Decl *message, const QString &name, const QString &path,
                             const QString &text)
{
    auto *view = new JsonView(schemaIndex(), message, name, path, text);
    const auto label = [view] { return view->title().replace(u'&', QStringLiteral("&&")); };
    const int index = m_views->addTab(view, label());
    m_views->setTabToolTip(index, path.isEmpty() ? QString() : QDir::toNativeSeparators(path));
    connect(view, &JsonView::titleChanged, this, [this, view, label] {
        m_views->setTabText(m_views->indexOf(view), label());
    });
    m_views->setCurrentIndex(index);
    view->setFocus();
    return view;
}

JsonView *SchemaTab::currentJson() const
{
    return qobject_cast<JsonView *>(m_views->currentWidget());
}

bool SchemaTab::closeCurrentJson()
{
    if (!currentJson())
        return false;
    closeView(m_views->currentIndex());
    return true;
}

void SchemaTab::closeView(int index)
{
    auto *view = qobject_cast<JsonView *>(m_views->widget(index));
    if (!view)
        return;
    m_views->removeTab(index);
    view->deleteLater();
}

void SchemaTab::showMenu(const QPoint &at)
{
    const SchemaTree::Node *node = m_model ? m_model->nodeAt(m_tree->indexAt(at)) : nullptr;
    if (!node)
        return;
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto add = [this, menu](const Decl *message) {
        QString name = message->name;
        name.replace(u'&', QStringLiteral("&&"));
        connect(menu->addAction(tr("Map JSON onto %1…").arg(name)), &QAction::triggered, this,
                [this, message] { emit jsonWanted(message); });
        connect(menu->addAction(tr("Paste JSON as %1").arg(name)), &QAction::triggered, this,
                [this, message] { pasteJson(message); });
    };
    if (const MethodInfo *method = node->decl->method.get()) {
        if (method->requestType)
            add(method->requestType);
        if (method->responseType && method->responseType != method->requestType) {
            menu->addSeparator();
            add(method->responseType);
        }
    } else if (const Decl *message = SchemaTree::messageAt(*node)) {
        add(message);
    }
    if (menu->isEmpty())
        delete menu;
    else
        menu->popup(m_tree->viewport()->mapToGlobal(at));
}

void SchemaTab::activateFind()
{
    if (JsonView *json = currentJson())
        json->activateFind();
    else
        m_findBar->activate();
}

void SchemaTab::find(bool forward)
{
    if (JsonView *json = currentJson())
        json->find(forward);
    else
        runFind(forward, false);
}

void SchemaTab::updateMatches()
{
    const QString pattern = m_findBar->pattern();
    if (pattern == m_pattern)
        return;
    m_pattern = pattern;
    m_matches.clear();
    if (pattern.isEmpty())
        return;
    for (SchemaTree::Node *node : m_model->tree().messages()) {
        if (SchemaTree::typeOf(*node->decl)->fullName.contains(pattern, Qt::CaseInsensitive))
            m_matches.push_back(node);
    }
}

void SchemaTab::runFind(bool forward, bool fromStart)
{
    if (!m_model)
        return;
    updateMatches();
    m_findBar->setFailed(!m_pattern.isEmpty() && m_matches.empty());
    if (m_matches.empty()) {
        m_findBar->setStatus(m_pattern.isEmpty() ? QString() : tr("No matches"));
        return;
    }

    size_t match = 0;
    const SchemaTree::Node *current = m_model->nodeAt(m_tree->currentIndex());
    if (fromStart) {
        const auto exact = std::find_if(m_matches.begin(), m_matches.end(), [this](const auto *node) {
            const Decl *type = SchemaTree::typeOf(*node->decl);
            return type->name.compare(m_pattern, Qt::CaseInsensitive) == 0
                || type->fullName.compare(m_pattern, Qt::CaseInsensitive) == 0;
        });
        if (exact != m_matches.end())
            match = size_t(exact - m_matches.begin());
    } else if (current) {
        // From wherever the reader is, be that a match or not.
        const auto before = [](const SchemaTree::Node *a, const SchemaTree::Node *b) {
            return SchemaTree::pathTo(*a) < SchemaTree::pathTo(*b);
        };
        if (forward) {
            const auto it = std::upper_bound(m_matches.begin(), m_matches.end(), current, before);
            match = it == m_matches.end() ? 0 : size_t(it - m_matches.begin());
        } else {
            const auto it = std::lower_bound(m_matches.begin(), m_matches.end(), current, before);
            match = (it == m_matches.begin() ? m_matches.size() : size_t(it - m_matches.begin())) - 1;
        }
    }

    m_tree->reveal(m_model->indexFor(m_matches[match]));
    m_findBar->setStatus(tr("%1 of %2").arg(match + 1).arg(m_matches.size()));
}

} // namespace protoexplorer
