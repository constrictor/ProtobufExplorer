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

#include "JsonRows.h"

#include "JsonModel.h"
#include "UiColors.h"

namespace protoexplorer {

namespace {

JsonTree::State stateOf(const QModelIndex &index)
{
    return JsonTree::State(index.data(JsonModel::StateRole).toInt());
}

bool isMissing(JsonTree::State state)
{
    return state == JsonTree::State::Default || state == JsonTree::State::NotSet;
}

} // namespace

void MissingFilter::setShowMissing(bool show)
{
    if (show == m_show)
        return;
    m_show = show;
    invalidate(); // the narrower ways to say so differ from one Qt to the next
}

bool MissingFilter::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    return m_show || !isMissing(stateOf(sourceModel()->index(row, 0, parent)));
}

void JsonDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    // Grayed, they are all alike.
    const auto colour = [option](const QColor &color) {
        for (const auto group : {QPalette::Active, QPalette::Inactive})
            option->palette.setColor(group, QPalette::Text, color);
    };
    const JsonTree::State state = stateOf(index);
    if (state == JsonTree::State::Problem) {
        colour(errorColor(option->palette));
    } else if (isMissing(state)) {
        // Not by colour alone: a default looks the same as a value.
        colour(dimColor(option->palette));
        option->font.setItalic(true);
    }
}

} // namespace protoexplorer
