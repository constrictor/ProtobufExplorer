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

#include "JsonMatch.h"
#include "JsonModel.h"

#include <QSet>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QMenu;
class QPushButton;
class QToolButton;
QT_END_NAMESPACE

namespace protoexplorer {

class FindBar;
class JsonEdit;
class MissingFilter;
class SchemaTreeView;
class SourceView;
class WhereLabel;

// A JSON text laid over a message of a schema: the values in a tree, and on the right the
// source of whatever is selected and where in the JSON it is.
class JsonView : public QWidget
{
    Q_OBJECT

public:
    // `name` is what to call the JSON, and `path` the file it is from, if it is from one.
    // Without a message it is taken for whichever is likeliest.
    JsonView(std::shared_ptr<const SchemaIndex> index, const Decl *message, const QString &name,
             const QString &path, const QString &text, QWidget *parent = nullptr);

    const Decl *message() const { return m_message; }
    QString path() const { return m_path; }
    QString title() const;
    bool isModified() const;

    void setMessage(const Decl *message);
    void setText(const QString &text);
    // Something to tell the reader about how it was opened.
    void setNote(const QString &text);

    void activateFind();
    void find(bool forward);

signals:
    void titleChanged();

protected:
    void changeEvent(QEvent *event) override;

private:
    QWidget *makeLeft();
    QWidget *makeRight();
    QWidget *makeFooter();

    // Lay the JSON over the message anew.
    void remap();
    void updateHeader();
    void updateHint();
    void updateCounts();

    QModelIndex viewIndex(const JsonTree::Node *node) const;
    const JsonTree::Node *currentNode() const;
    void reveal(const JsonTree::Node *node);
    void expandByDefault();
    void expandFrom(const JsonTree::Node &node);
    QSet<QStringList> expandedPaths() const;
    void addExpandedPaths(const std::vector<std::unique_ptr<JsonTree::Node>> &nodes,
                          const QStringList &above, QSet<QStringList> &paths) const;
    const JsonTree::Node *nodeByPath(const QStringList &path) const;

    void showNode(const JsonTree::Node *node);
    void showWhere(const QString &where, bool isError);
    void applyPalette();
    // The row for the place in the JSON the cursor is at.
    void followCursor();

    void runFind(bool forward, bool fromStart);
    void updateMatches();
    bool matches(const JsonTree::Node &node) const;
    void step(JsonTree::State state);
    void setShowMissing(bool show);

    void pickMessage();
    void showMenu(const QPoint &at);
    void copy(const QString &text);

    std::shared_ptr<const SchemaIndex> m_index;
    const Decl *m_message;
    bool m_chosen; // the message was asked for rather than guessed
    QString m_name;
    QString m_path;
    QString m_note;

    std::shared_ptr<const JsonTree> m_tree;
    std::vector<JsonKey> m_keys;
    const Decl *m_better = nullptr; // a message that fits more of them
    bool m_stale = true;            // the JSON has changed since it was last laid out
    bool m_syncing = false;         // the rows are being set, or the cursor, to match
    bool m_following = false;       // the row is being set to match the cursor
    bool m_whereIsError = false;
    QTimer m_remapTimer;

    QString m_pattern;
    bool m_matchesMissing = true;
    bool m_matchesHold = false; // for the tree there is
    std::vector<const JsonTree::Node *> m_matches;

    JsonModel *m_model;
    MissingFilter *m_filter;

    QWidget *m_left = nullptr;
    QLabel *m_messageLabel = nullptr;
    QToolButton *m_change = nullptr;
    QLabel *m_fit = nullptr;
    QWidget *m_hint = nullptr;
    QLabel *m_hintText = nullptr;
    QPushButton *m_switch = nullptr;
    FindBar *m_findBar = nullptr;
    SchemaTreeView *m_view = nullptr;
    SourceView *m_source = nullptr;
    WhereLabel *m_where = nullptr;
    JsonEdit *m_edit = nullptr;
    QWidget *m_footer = nullptr;
    QToolButton *m_counts[4] = {}; // by JsonTree::State
    QCheckBox *m_showMissing = nullptr;
};

} // namespace protoexplorer
