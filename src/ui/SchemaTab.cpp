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
#include "SchemaLoader.h"
#include "SchemaTreeView.h"
#include "SourceView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPromise>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

namespace protoexplorer {

using SchemaPtr = std::shared_ptr<const Schema>;

SchemaTab::SchemaTab(const QString &path, QWidget *parent)
    : QWidget(parent)
    , m_path(path)
{
    m_loading = new QLabel(tr("Loading…"), this);
    m_loading->setAlignment(Qt::AlignCenter);
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(m_loading);
    m_pages->addWidget(makeContents());

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
    m_tree = new SchemaTreeView;
    m_tree->setObjectName(QStringLiteral("schemaTree"));
    m_tree->setFrameShape(QFrame::NoFrame);

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
    return splitter;
}

void SchemaTab::finishLoading()
{
    if (m_watcher.isCanceled() || !m_watcher.future().isResultReadyAt(0))
        return;
    const SchemaPtr schema = m_watcher.result();

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
        m_source->display(SchemaTree::sourceParts(*node));
    else
        m_source->clear();
}

void SchemaTab::activateFind()
{
    m_findBar->activate();
}

void SchemaTab::find(bool forward)
{
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
