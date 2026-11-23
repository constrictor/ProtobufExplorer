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

#include "Schema.h"

#include <vector>

namespace protoexplorer {

// The declarations that head a tree, in the order they are found in the files: every
// message and enum that no other message uses, and every service and `extend` block.
// Messages that use one another in a circle are headed by whichever comes first.
//
// Every opened message and enum can be reached from these by following fields.
std::vector<const Decl *> findRoots(const Schema &schema);

// The types a message's fields, or an `extend` block's, refer to.
std::vector<const Decl *> usedTypes(const Decl &decl);

} // namespace protoexplorer
