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

#include "SourceView.h"

#include "ProtoHighlighter.h"
#include "WhereLabel.h"

#include <QEvent>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

namespace protoexplorer {

namespace {

constexpr int kMaxFirstLines = 12;

} // namespace

SourceView::SourceView(QWidget *parent)
    : QWidget(parent)
    , m_first(makePane(1))
    , m_second(makePane(2))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_first.box);
    layout->addWidget(m_second.box, 1);
    applyFont();
    applyPalette();
    clear();
}

SourceView::Pane SourceView::makePane(int number)
{
    Pane pane;
    pane.box = new QWidget(this);
    pane.header = new WhereLabel(pane.box);
    pane.header->setObjectName(QStringLiteral("sourceHeader%1").arg(number));
    pane.text = new QPlainTextEdit(pane.box);
    pane.text->setObjectName(QStringLiteral("sourceText%1").arg(number));
    pane.text->setReadOnly(true);
    pane.text->setLineWrapMode(QPlainTextEdit::NoWrap);
    pane.text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    pane.highlighter = new ProtoHighlighter(pane.text->document());

    auto *layout = new QVBoxLayout(pane.box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(pane.header);
    layout->addWidget(pane.text, 1);
    return pane;
}

void SourceView::display(const std::vector<const Decl *> &parts)
{
    if (parts.empty()) {
        clear();
        return;
    }
    fill(m_first, *parts.front());
    const bool alone = parts.size() < 2;
    if (!alone)
        fill(m_second, *parts[1]);
    m_second.box->setVisible(!alone);
    fitFirst(alone);
}

void SourceView::clear()
{
    m_first.header->setWhere(QString());
    m_first.text->clear();
    m_second.text->clear();
    m_second.box->setVisible(false);
    fitFirst(true);
}

void SourceView::fill(Pane &pane, const Decl &decl)
{
    const Snippet snippet = decl.file->text.snippet(decl.span);
    const QString lines = snippet.firstLine == snippet.lastLine
        ? QString::number(snippet.firstLine)
        : QStringLiteral("%1–%2").arg(snippet.firstLine).arg(snippet.lastLine);
    pane.header->setWhere(QStringLiteral("%1 : %2").arg(decl.file->displayPath, lines));
    pane.text->setPlainText(snippet.text);
}

void SourceView::fitFirst(bool alone)
{
    auto *layout = static_cast<QVBoxLayout *>(this->layout());
    layout->setStretchFactor(m_first.box, alone ? 1 : 0);
    if (alone) {
        m_first.text->setMinimumHeight(0);
        m_first.text->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    const int lines = std::min(m_first.text->blockCount(), kMaxFirstLines);
    const QPlainTextEdit *text = m_first.text;
    m_first.text->setFixedHeight(int(lines * text->fontMetrics().lineSpacing()
                                     + 2 * text->document()->documentMargin()
                                     + 2 * text->frameWidth()
                                     + text->horizontalScrollBar()->sizeHint().height()));
}

void SourceView::applyFont()
{
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (const Pane *pane : {&m_first, &m_second}) {
        pane->text->setFont(font);
        pane->text->setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(u' '));
    }
}

void SourceView::applyPalette()
{
    QPalette header = palette();
    header.setColor(QPalette::WindowText, mutedColor(palette()));
    for (const Pane *pane : {&m_first, &m_second}) {
        pane->header->setPalette(header);
        pane->highlighter->setPalette(palette());
    }
}

void SourceView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        applyPalette();
    QWidget::changeEvent(event);
}

} // namespace protoexplorer
