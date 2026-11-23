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

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QToolButton;
QT_END_NAMESPACE

namespace protoexplorer {

// Where the reader types what to find. It only asks; whoever owns it does the finding and
// reports back through setStatus() and setFailed().
class FindBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent = nullptr);

    QString pattern() const;
    QString status() const;
    bool isFailed() const { return m_failed; }

    // Take the focus, with what is already there selected to be typed over.
    void activate();
    void setStatus(const QString &text);
    // Nothing matches what is in the box: say so on the box itself.
    void setFailed(bool failed);

signals:
    // `fromStart` when the pattern is new, rather than another match of it wanted.
    void findRequested(bool forward, bool fromStart);
    void escapePressed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyFailed();

    QLineEdit *m_edit = nullptr;
    QLabel *m_status = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    bool m_failed = false;
};

} // namespace protoexplorer
