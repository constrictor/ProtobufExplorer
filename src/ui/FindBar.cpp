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

#include "FindBar.h"

#include "UiColors.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace protoexplorer {

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
    , m_edit(new QLineEdit(this))
    , m_status(new QLabel(this))
    , m_previous(new QToolButton(this))
    , m_next(new QToolButton(this))
{
    m_edit->setObjectName(QStringLiteral("findEdit"));
    m_edit->setPlaceholderText(tr("Find message…"));
    m_edit->setClearButtonEnabled(true);
    m_edit->installEventFilter(this);
    m_status->setObjectName(QStringLiteral("findStatus"));
    m_previous->setObjectName(QStringLiteral("findPrevious"));
    m_previous->setArrowType(Qt::UpArrow);
    m_previous->setToolTip(tr("Previous match (Shift+Enter)"));
    m_previous->setAutoRaise(true);
    m_next->setObjectName(QStringLiteral("findNext"));
    m_next->setArrowType(Qt::DownArrow);
    m_next->setToolTip(tr("Next match (Enter)"));
    m_next->setAutoRaise(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_status);
    layout->addWidget(m_previous);
    layout->addWidget(m_next);

    connect(m_edit, &QLineEdit::textChanged, this, [this] { emit findRequested(true, true); });
    connect(m_previous, &QToolButton::clicked, this, [this] { emit findRequested(false, false); });
    connect(m_next, &QToolButton::clicked, this, [this] { emit findRequested(true, false); });
}

QString FindBar::pattern() const
{
    return m_edit->text().trimmed();
}

QString FindBar::status() const
{
    return m_status->text();
}

void FindBar::activate()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

void FindBar::setStatus(const QString &text)
{
    m_status->setText(text);
}

void FindBar::setFailed(bool failed)
{
    m_failed = failed;
    applyFailed();
}

void FindBar::applyFailed()
{
    QPalette colors = palette();
    if (m_failed)
        colors.setColor(QPalette::Text, errorColor(colors));
    m_edit->setPalette(colors);
}

bool FindBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit findRequested(!(key->modifiers() & Qt::ShiftModifier), false);
            return true;
        case Qt::Key_Escape:
            emit escapePressed();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FindBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        applyFailed();
    QWidget::changeEvent(event);
}

} // namespace protoexplorer
