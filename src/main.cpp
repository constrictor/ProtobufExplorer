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

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("protobufexplorer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Protobuf Explorer"));
    QApplication::setApplicationVersion(QStringLiteral(PROTOEXPLORER_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QApplication::translate("main", "A schema file or a folder of them."),
                                 QStringLiteral("[path...]"));
    parser.process(app);

    protoexplorer::MainWindow window;
    const QStringList paths = parser.positionalArguments();
    for (const QString &path : paths)
        window.openPath(path);
    window.show();
    return QApplication::exec();
}
