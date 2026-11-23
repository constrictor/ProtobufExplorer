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

#include <functional>
#include <memory>

namespace protoexplorer {

// Load a schema file, or every .proto and .pb under a folder, along with what they import.
// Null only when `canceled` says to stop; anything else that goes wrong is a note.
std::shared_ptr<const Schema> loadSchema(const QString &path,
                                         const std::function<bool()> &canceled = {});

} // namespace protoexplorer
