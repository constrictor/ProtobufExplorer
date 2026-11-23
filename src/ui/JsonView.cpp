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

#include "JsonView.h"

#include "FindBar.h"
#include "JsonEdit.h"
#include "JsonRows.h"
#include "JsonValues.h"
#include "MessagePicker.h"
#include "SchemaTreeView.h"
#include "SourceView.h"
#include "UiColors.h"
#include "WhereLabel.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace protoexplorer {

using State = JsonTree::State;
using Form = JsonTree::Form;
using Node = JsonTree::Node;

namespace {

constexpr int kRemapDelay = 250; // ms
constexpr size_t kMaxOpenItems = 10;
constexpr int kMaxColumn = 480;

// What its JSON has for keys are its fields.
bool hasKeys(const Decl *message)
{
    return message && specialOf(*message) == Special::None;
}

QStringList pathOf(const Node &node)
{
    QStringList path;
    for (const Node *at = &node; at; at = at->parent)
        path.prepend(at->name);
    return path;
}

} // namespace

JsonView::JsonView(std::shared_ptr<const SchemaIndex> index, const Decl *message,
                   const QString &name, const QString &path, const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_index(std::move(index))
    , m_message(message)
    , m_chosen(message)
    , m_name(name)
    , m_path(path)
    , m_model(new JsonModel(this))
    , m_filter(new MissingFilter(this))
{
    m_filter->setSourceModel(m_model);

    auto *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(makeLeft());
    splitter->addWidget(makeRight());
    splitter->setSizes({550, 450});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(splitter, 1);
    layout->addWidget(makeFooter());

    m_remapTimer.setSingleShot(true);
    m_remapTimer.setInterval(kRemapDelay);
    connect(&m_remapTimer, &QTimer::timeout, this, &JsonView::remap);
    setText(text);
}

QWidget *JsonView::makeLeft()
{
    m_messageLabel = new QLabel;
    m_messageLabel->setObjectName(QStringLiteral("jsonMessage"));
    m_messageLabel->setTextFormat(Qt::PlainText);
    QFont bold = m_messageLabel->font();
    bold.setBold(true);
    m_messageLabel->setFont(bold);
    m_change = new QToolButton;
    m_change->setObjectName(QStringLiteral("jsonChange"));
    m_change->setText(tr("change ▾"));
    m_change->setToolTip(tr("Read the JSON as another message"));
    m_change->setAutoRaise(true);
    m_change->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_fit = new QLabel;
    m_fit->setObjectName(QStringLiteral("jsonFit"));
    m_fit->setForegroundRole(QPalette::PlaceholderText);
    auto *header = new QHBoxLayout;
    header->setContentsMargins(6, 4, 6, 0);
    header->addWidget(m_messageLabel);
    header->addWidget(m_change);
    header->addWidget(m_fit);
    header->addStretch(1);

    m_hint = new QWidget;
    m_hint->setObjectName(QStringLiteral("jsonHint"));
    m_hintText = new QLabel;
    m_hintText->setObjectName(QStringLiteral("jsonHintText"));
    m_hintText->setTextFormat(Qt::PlainText);
    m_hintText->setWordWrap(true);
    m_switch = new QPushButton(tr("Switch"));
    m_switch->setObjectName(QStringLiteral("jsonSwitch"));
    auto *hintLayout = new QHBoxLayout(m_hint);
    hintLayout->setContentsMargins(6, 4, 6, 4);
    hintLayout->addWidget(m_hintText, 1);
    hintLayout->addWidget(m_switch);

    m_findBar = new FindBar;
    m_findBar->setPrompt(tr("Find field or value…"));
    m_view = new SchemaTreeView;
    m_view->setObjectName(QStringLiteral("jsonTree"));
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setModel(m_filter);
    m_view->setItemDelegate(new JsonDelegate(m_view));
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->header()->setMaximumSectionSize(kMaxColumn);

    auto *copyValue = new QAction(tr("Copy Value"), m_view);
    copyValue->setShortcut(QKeySequence::Copy);
    copyValue->setShortcutContext(Qt::WidgetShortcut);
    m_view->addAction(copyValue);

    m_left = new QWidget;
    auto *layout = new QVBoxLayout(m_left);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(m_hint);
    layout->addWidget(m_findBar);
    layout->addWidget(m_view, 1);

    connect(m_change, &QToolButton::clicked, this, &JsonView::pickMessage);
    connect(m_switch, &QPushButton::clicked, this, [this] { setMessage(m_better); });
    connect(m_findBar, &FindBar::findRequested, this, &JsonView::runFind);
    connect(m_findBar, &FindBar::escapePressed, m_view, qOverload<>(&QWidget::setFocus));
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { showNode(currentNode()); });
    connect(m_view, &QWidget::customContextMenuRequested, this, &JsonView::showMenu);
    connect(copyValue, &QAction::triggered, this, [this] {
        if (const Node *node = currentNode())
            copy(m_tree->copyText(*node));
    });
    return m_left;
}

QWidget *JsonView::makeRight()
{
    m_source = new SourceView;
    m_where = new WhereLabel;
    m_where->setObjectName(QStringLiteral("jsonWhere"));
    m_edit = new JsonEdit;
    m_edit->setObjectName(QStringLiteral("jsonEdit"));

    auto *bottom = new QWidget;
    auto *layout = new QVBoxLayout(bottom);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_where);
    layout->addWidget(m_edit, 1);

    auto *splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_source);
    splitter->addWidget(bottom);
    splitter->setSizes({300, 400});

    // The editor's own signal would come after the one that the cursor has moved, and until
    // then the places in the tree would be taken for places in the new text.
    connect(m_edit->document(), &QTextDocument::contentsChange, this, [this] {
        m_stale = true;
        m_remapTimer.start();
    });
    connect(m_edit, &QPlainTextEdit::cursorPositionChanged, this, &JsonView::followCursor);
    return splitter;
}

QWidget *JsonView::makeFooter()
{
    m_footer = new QWidget;
    auto *layout = new QHBoxLayout(m_footer);
    layout->setContentsMargins(4, 2, 6, 2);
    const std::pair<State, const char *> counts[] = {
        {State::Set, "countSet"},
        {State::Default, "countDefault"},
        {State::NotSet, "countNotSet"},
        {State::Problem, "countProblem"},
    };
    for (const auto &[state, name] : counts) {
        auto *button = new QToolButton;
        button->setObjectName(QLatin1StringView(name));
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setToolTip(tr("Go to the next one"));
        connect(button, &QToolButton::clicked, this, [this, state = state] { step(state); });
        button->hide(); // until there is something to count
        layout->addWidget(button);
        m_counts[size_t(state)] = button;
    }
    layout->addStretch(1);
    m_showMissing = new QCheckBox(tr("Show missing"));
    m_showMissing->setObjectName(QStringLiteral("showMissing"));
    m_showMissing->setToolTip(tr("Fields that the JSON leaves out"));
    m_showMissing->setChecked(true);
    connect(m_showMissing, &QCheckBox::toggled, this, &JsonView::setShowMissing);
    layout->addWidget(m_showMissing);
    return m_footer;
}

QString JsonView::title() const
{
    return m_message ? QStringLiteral("%1 ← %2").arg(m_message->name, m_name) : m_name;
}

bool JsonView::isModified() const
{
    return m_edit->document()->isModified();
}

void JsonView::setText(const QString &text)
{
    m_edit->setPlainText(text);
    remap();
    if (text.isEmpty())
        setFocusProxy(m_edit);
    else
        setFocusProxy(m_view);
}

void JsonView::setNote(const QString &text)
{
    m_note = text;
    updateHint();
}

void JsonView::setMessage(const Decl *message)
{
    if (!message)
        return;
    m_chosen = true;
    m_note.clear();
    if (message == m_message)
        return;
    m_message = message;
    remap();
    emit titleChanged();
}

void JsonView::remap()
{
    m_remapTimer.stop();
    JsonDocument document = parseJson(m_edit->json());
    m_left->setEnabled(!document.error);
    m_footer->setEnabled(!document.error);
    if (document.error) {
        // What there was stays in sight, but it is no longer what there is.
        m_edit->mark(0, 0);
        m_edit->setErrorAt(int(document.error->at));
        showWhere(QStringLiteral("%1 : %2 — %3").arg(m_name).arg(document.error->line)
                      .arg(document.error->text), true);
        updateHeader();
        updateHint();
        return;
    }
    m_edit->setErrorAt({});

    const bool guessing = !m_message || (!m_chosen && m_keys.empty());
    m_keys = topLevelKeys(document);
    const Decl *before = m_message;
    if (guessing) {
        const auto ranked = rankMessages(*m_index, m_keys);
        m_message = ranked.empty() ? nullptr : ranked.front().message;
    }
    if (!m_message)
        return;

    // The same rows as before, where there are such.
    const bool same = m_tree && &m_tree->message() == m_message && !m_tree->roots().empty();
    const QSet<QStringList> expanded = same ? expandedPaths() : QSet<QStringList>();
    const QStringList current = same && currentNode() ? pathOf(*currentNode()) : QStringList();
    const int scrolled = m_view->verticalScrollBar()->value();

    auto tree = std::make_shared<const JsonTree>(m_index, *m_message, std::move(document));
    m_syncing = true;
    m_model->setTree(tree); // the one before it has to last until then
    m_tree = std::move(tree);
    m_matches.clear();
    m_matchesHold = false;
    m_stale = false;
    if (same) {
        for (const QStringList &path : expanded) {
            if (const Node *node = nodeByPath(path))
                m_view->expand(viewIndex(node));
        }
    } else {
        expandByDefault();
    }
    const Node *node = current.isEmpty() ? nullptr : nodeByPath(current);
    if (!node && !m_tree->roots().empty())
        node = m_tree->roots().front().get();
    m_view->setCurrentIndex(viewIndex(node));
    if (same)
        m_view->verticalScrollBar()->setValue(scrolled);
    m_syncing = false;

    updateHeader();
    updateHint();
    updateCounts();
    m_findBar->setStatus(QString());
    if (m_edit->hasFocus())
        followCursor();
    else
        showNode(currentNode());
    if (before != m_message)
        emit titleChanged();
}

void JsonView::updateHeader()
{
    m_messageLabel->setText(m_message ? m_message->name : QString());
    m_messageLabel->setToolTip(m_message ? m_message->fullName : QString());
    if (m_keys.empty() || !hasKeys(m_message)) {
        m_fit->clear();
        return;
    }
    const int known = knownKeys(*m_index, *m_message, m_keys);
    m_fit->setText(m_keys.size() == 1 ? tr("%1 of 1 key matches").arg(known)
                                      : tr("%1 of %2 keys match").arg(known).arg(m_keys.size()));
}

void JsonView::updateHint()
{
    m_better = nullptr;
    QStringList hints;
    if (!m_note.isEmpty())
        hints << m_note;
    // Only when it looks to be some other message altogether.
    const int known = hasKeys(m_message) ? knownKeys(*m_index, *m_message, m_keys) : 0;
    if (hasKeys(m_message) && 2 * size_t(known) < m_keys.size()) {
        const auto ranked = rankMessages(*m_index, m_keys);
        if (!ranked.empty() && ranked.front().known > known && ranked.front().message != m_message) {
            m_better = ranked.front().message;
            hints << tr("%1 of %2 keys match %3, while %4 matches %5.")
                         .arg(known).arg(m_keys.size()).arg(m_message->name, m_better->name)
                         .arg(ranked.front().known);
        }
    }
    m_hintText->setText(hints.join(u' '));
    m_switch->setVisible(m_better);
    m_hint->setVisible(!hints.isEmpty());
}

void JsonView::updateCounts()
{
    const auto show = [this](State state, const QString &text) {
        m_counts[size_t(state)]->setText(text);
        m_counts[size_t(state)]->setVisible(!m_tree->nodes(state).empty());
    };
    const auto count = [this](State state) { return m_tree->nodes(state).size(); };
    show(State::Set, tr("%1 set").arg(count(State::Set)));
    show(State::Default, tr("%1 defaulted").arg(count(State::Default)));
    show(State::NotSet, tr("%1 not set").arg(count(State::NotSet)));
    const auto problems = count(State::Problem);
    show(State::Problem, problems == 1 ? tr("1 problem") : tr("%1 problems").arg(problems));
}

QModelIndex JsonView::viewIndex(const Node *node) const
{
    return m_filter->mapFromSource(m_model->indexFor(node));
}

const Node *JsonView::currentNode() const
{
    return m_model->nodeAt(m_filter->mapToSource(m_view->currentIndex()));
}

void JsonView::reveal(const Node *node)
{
    if (node && !viewIndex(node).isValid())
        m_showMissing->setChecked(true);
    m_view->reveal(viewIndex(node));
}

void JsonView::expandByDefault()
{
    // Several messages are a list to choose from, and one is to be seen.
    if (m_tree->roots().size() == 1)
        expandFrom(*m_tree->roots().front());
}

void JsonView::expandFrom(const Node &node)
{
    m_view->expand(viewIndex(&node));
    for (const auto &child : node.children) {
        if (child->children.empty() || child->isMissing())
            continue;
        if (child->form == Form::Message || child->form == Form::Any || child->form == Form::Oneof)
            expandFrom(*child);
        else if (child->children.size() <= kMaxOpenItems)
            m_view->expand(viewIndex(child.get()));
    }
}

QSet<QStringList> JsonView::expandedPaths() const
{
    QSet<QStringList> paths;
    addExpandedPaths(m_tree->roots(), {}, paths);
    return paths;
}

void JsonView::addExpandedPaths(const std::vector<std::unique_ptr<Node>> &nodes,
                                const QStringList &above, QSet<QStringList> &paths) const
{
    for (const auto &node : nodes) {
        if (node->children.empty() || !m_view->isExpanded(viewIndex(node.get())))
            continue;
        const QStringList path = above + QStringList(node->name);
        paths.insert(path);
        addExpandedPaths(node->children, path, paths);
    }
}

const Node *JsonView::nodeByPath(const QStringList &path) const
{
    const Node *found = nullptr;
    for (const QString &name : path) {
        const auto &nodes = found ? found->children : m_tree->roots();
        const auto it = std::find_if(nodes.begin(), nodes.end(),
                                     [&name](const auto &node) { return node->name == name; });
        if (it == nodes.end())
            return nullptr;
        found = it->get();
    }
    return found;
}

void JsonView::showNode(const Node *node)
{
    if (m_syncing || !m_tree)
        return;
    if (!node) {
        m_source->clear();
        m_edit->mark(0, 0);
        showWhere(m_name, false);
        return;
    }
    m_source->display(JsonTree::sourceParts(*node));
    const JsonValue *json = JsonTree::jsonFor(*node);
    if (!json || m_stale)
        return;
    m_edit->mark(int(json->keyBegin), int(json->end));
    const auto line = [this](qsizetype at) {
        return m_edit->document()->findBlock(int(at)).blockNumber() + 1;
    };
    const int first = line(json->keyBegin);
    const int last = line(json->end);
    showWhere(first == last ? QStringLiteral("%1 : %2").arg(m_name).arg(first)
                            : QStringLiteral("%1 : %2–%3").arg(m_name).arg(first).arg(last), false);
    if (!m_following) {
        m_syncing = true;
        m_edit->goTo(int(json->keyBegin));
        m_syncing = false;
    }
}

void JsonView::showWhere(const QString &where, bool isError)
{
    m_whereIsError = isError;
    applyPalette();
    m_where->setWhere(where);
}

void JsonView::applyPalette()
{
    QPalette colors = palette();
    colors.setColor(QPalette::WindowText, m_whereIsError ? errorColor(palette()) : mutedColor(palette()));
    m_where->setPalette(colors);
    colors = palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive})
        colors.setColor(group, QPalette::ButtonText, errorColor(palette()));
    m_counts[size_t(State::Problem)]->setPalette(colors);
}

void JsonView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange && m_showMissing) // all there
        applyPalette();
    QWidget::changeEvent(event);
}

void JsonView::followCursor()
{
    if (m_syncing || m_stale || !m_tree)
        return;
    const Node *node = m_tree->nodeAt(m_edit->textCursor().position());
    // Those that are hidden are in what is not.
    while (node && !viewIndex(node).isValid())
        node = node->parent;
    m_following = true;
    if (node && node != currentNode()) {
        m_view->setCurrentIndex(viewIndex(node));
        m_view->scrollTo(viewIndex(node)); // no further than it has to be
    } else {
        showNode(currentNode());
    }
    m_following = false;
}

void JsonView::activateFind()
{
    m_findBar->activate();
}

void JsonView::find(bool forward)
{
    runFind(forward, false);
}

bool JsonView::matches(const Node &node) const
{
    const auto has = [this](const QString &text) {
        return text.contains(m_pattern, Qt::CaseInsensitive);
    };
    if (has(node.name) || (node.json && has(node.json->key)))
        return true;
    if (node.isMissing() || !node.json)
        return false;
    // What a message or a list holds is in rows of its own.
    switch (node.form) {
    case Form::Message:
    case Form::List:
    case Form::Map:
    case Form::Oneof:
        return false;
    case Form::Free:
        if (!node.json->members.empty())
            return false;
        break;
    default:
        break;
    }
    return has(m_tree->valueText(node))
        || (node.json->kind == JsonValue::Kind::String && has(node.json->text));
}

void JsonView::updateMatches()
{
    const QString pattern = m_findBar->pattern();
    const bool missing = m_filter->showsMissing();
    if (pattern == m_pattern && missing == m_matchesMissing && m_matchesHold)
        return;
    m_pattern = pattern;
    m_matchesMissing = missing;
    m_matchesHold = true;
    m_matches.clear();
    if (pattern.isEmpty())
        return;
    for (const Node *node : m_tree->nodes()) {
        if ((missing || !node->isMissing()) && matches(*node))
            m_matches.push_back(node);
    }
}

void JsonView::runFind(bool forward, bool fromStart)
{
    if (!m_tree || !m_left->isEnabled())
        return;
    updateMatches();
    m_findBar->setFailed(!m_pattern.isEmpty() && m_matches.empty());
    if (m_matches.empty()) {
        m_findBar->setStatus(m_pattern.isEmpty() ? QString() : tr("No matches"));
        return;
    }

    size_t match = 0;
    const Node *current = currentNode();
    if (fromStart) {
        const auto exact = std::find_if(m_matches.begin(), m_matches.end(), [this](const Node *node) {
            return node->name.compare(m_pattern, Qt::CaseInsensitive) == 0
                || (node->json && node->json->key.compare(m_pattern, Qt::CaseInsensitive) == 0);
        });
        if (exact != m_matches.end())
            match = size_t(exact - m_matches.begin());
    } else if (current) {
        // From wherever the reader is, be that a match or not.
        const auto before = [](const Node *a, const Node *b) { return a->order < b->order; };
        if (forward) {
            const auto it = std::upper_bound(m_matches.begin(), m_matches.end(), current, before);
            match = it == m_matches.end() ? 0 : size_t(it - m_matches.begin());
        } else {
            const auto it = std::lower_bound(m_matches.begin(), m_matches.end(), current, before);
            match = (it == m_matches.begin() ? m_matches.size() : size_t(it - m_matches.begin())) - 1;
        }
    }
    reveal(m_matches[match]);
    m_findBar->setStatus(tr("%1 of %2").arg(match + 1).arg(m_matches.size()));
}

void JsonView::step(State state)
{
    if (!m_tree)
        return;
    const std::vector<const Node *> &nodes = m_tree->nodes(state);
    if (nodes.empty())
        return;
    const Node *current = currentNode();
    const auto next = std::find_if(nodes.begin(), nodes.end(), [current](const Node *node) {
        return current && node->order > current->order;
    });
    reveal(next == nodes.end() ? nodes.front() : *next);
}

void JsonView::setShowMissing(bool show)
{
    const Node *node = currentNode();
    m_filter->setShowMissing(show);
    // What was selected may be gone, and then what it was missing from is not.
    while (node && !viewIndex(node).isValid())
        node = node->parent;
    if (node)
        m_view->reveal(viewIndex(node));
}

void JsonView::pickMessage()
{
    auto *picker = new MessagePicker(rankMessages(*m_index, m_keys), int(m_keys.size()), m_message, this);
    connect(picker, &MessagePicker::picked, this, &JsonView::setMessage);
    picker->popUpUnder(*m_change);
}

void JsonView::showMenu(const QPoint &at)
{
    const Node *node = m_model->nodeAt(m_filter->mapToSource(m_view->indexAt(at)));
    if (!m_tree)
        return;
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto add = [this, menu](const QString &text, const QString &what) {
        QAction *action = menu->addAction(text);
        action->setEnabled(!what.isNull());
        connect(action, &QAction::triggered, this, [this, what] { copy(what); });
    };
    add(tr("Copy Value"), node ? m_tree->copyText(*node) : QString());
    add(tr("Copy JSON Path"), node ? m_tree->pathTo(*node) : QString());
    menu->addSeparator();
    connect(menu->addAction(tr("Copy JSON with Defaults")), &QAction::triggered, this,
            [this, tree = m_tree] { copy(tree->withDefaults()); });
    menu->popup(m_view->viewport()->mapToGlobal(at));
}

void JsonView::copy(const QString &text)
{
    if (!text.isNull())
        QGuiApplication::clipboard()->setText(text);
}

} // namespace protoexplorer
