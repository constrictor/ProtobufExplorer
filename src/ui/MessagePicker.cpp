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

#include "MessagePicker.h"

#include <QHash>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace protoexplorer {

namespace {

// Nobody reads further than this; they type.
constexpr size_t kMaxShown = 500;

} // namespace

MessagePicker::MessagePicker(std::vector<MessageMatch> matches, int keys, const Decl *current,
                             QWidget *parent)
    : QFrame(parent, Qt::Popup)
    , m_matches(std::move(matches))
    , m_keys(keys)
    , m_current(current)
    , m_filter(new QLineEdit(this))
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("messagePicker"));
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);
    m_filter->setObjectName(QStringLiteral("pickerFilter"));
    m_filter->setPlaceholderText(tr("Type to narrow…"));
    m_filter->installEventFilter(this);
    m_list->setObjectName(QStringLiteral("pickerList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    setFocusProxy(m_filter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_filter);
    layout->addWidget(m_list, 1);

    // Those that share a name are told apart by their files.
    QHash<QString, int> uses;
    for (const MessageMatch &match : m_matches)
        ++uses[match.message->fullName];
    for (const MessageMatch &match : m_matches) {
        QString label = match.message->fullName;
        if (uses[label] > 1)
            label += QStringLiteral(" (%1)").arg(match.message->file->displayPath);
        m_labels.push_back(label);
    }

    connect(m_filter, &QLineEdit::textChanged, this, &MessagePicker::fill);
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { pick(m_list->row(item)); });
    fill();
}

void MessagePicker::fill()
{
    const QString pattern = m_filter->text().trimmed();
    m_list->clear();
    m_shown.clear();
    int current = 0;
    for (size_t i = 0; i < m_matches.size() && m_shown.size() < kMaxShown; ++i) {
        if (!m_labels[i].contains(pattern, Qt::CaseInsensitive))
            continue;
        if (m_matches[i].message == m_current)
            current = int(m_shown.size());
        m_shown.push_back(i);
        m_list->addItem(m_keys == 0 ? m_labels[i]
                                    : tr("%1 — %2 of %3").arg(m_labels[i]).arg(m_matches[i].known).arg(m_keys));
    }
    // With something typed the first is as good a guess as any, and otherwise it is
    // easier to go on from the one there is.
    m_list->setCurrentRow(pattern.isEmpty() ? current : 0);
}

void MessagePicker::pick(int row)
{
    if (row < 0 || size_t(row) >= m_shown.size())
        return;
    const Decl *message = m_matches[m_shown[size_t(row)]].message;
    close();
    emit picked(message);
}

void MessagePicker::popUpUnder(const QWidget &widget)
{
    resize(std::max(320, m_list->sizeHintForColumn(0) + 60), 360);
    QPoint at = widget.mapToGlobal(QPoint(0, widget.height()));
    if (const QScreen *screen = widget.screen()) {
        const QRect available = screen->availableGeometry();
        at.setX(std::clamp(at.x(), available.left(), std::max(available.left(), available.right() - width())));
        at.setY(std::clamp(at.y(), available.top(), std::max(available.top(), available.bottom() - height())));
    }
    move(at);
    show();
    m_filter->setFocus();
}

bool MessagePicker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down: {
            const int step = key->key() == Qt::Key_Up ? -1 : 1;
            m_list->setCurrentRow(std::clamp(m_list->currentRow() + step, 0,
                                             std::max(0, m_list->count() - 1)));
            return true;
        }
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return QCoreApplication::sendEvent(m_list, event);
        case Qt::Key_Return:
        case Qt::Key_Enter:
            pick(m_list->currentRow());
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace protoexplorer
