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

#include <QFrame>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QListWidget;
QT_END_NAMESPACE

namespace protoexplorer {

// A list that drops down to pick a message from, typing to narrow it.
class MessagePicker : public QFrame
{
    Q_OBJECT

public:
    // `keys` is how many there are, of which each message knows some.
    MessagePicker(std::vector<MessageMatch> matches, int keys, const Decl *current,
                  QWidget *parent = nullptr);

    void popUpUnder(const QWidget &widget);

signals:
    void picked(const protoexplorer::Decl *message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void fill();
    void pick(int row);

    std::vector<MessageMatch> m_matches;
    std::vector<QString> m_labels;
    std::vector<size_t> m_shown; // which of them are in the list
    int m_keys;
    const Decl *m_current;
    QLineEdit *m_filter;
    QListWidget *m_list;
};

} // namespace protoexplorer
