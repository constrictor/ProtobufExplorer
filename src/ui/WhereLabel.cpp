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

#include "WhereLabel.h"

namespace protoexplorer {

WhereLabel::WhereLabel(QWidget *parent)
    : QLabel(parent)
{
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setTextFormat(Qt::PlainText);
    setContentsMargins(6, 4, 6, 4);
    // Not to let a long path set the width of the window.
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

void WhereLabel::setWhere(const QString &where)
{
    m_where = where;
    setToolTip(where);
    elide();
}

void WhereLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    elide();
}

void WhereLabel::elide()
{
    setText(fontMetrics().elidedText(m_where, Qt::ElideLeft, contentsRect().width()));
}

} // namespace protoexplorer
