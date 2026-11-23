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
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("protobufexplorer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Protobuf Explorer"));
    QApplication::setApplicationVersion(QStringLiteral(PROTOEXPLORER_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("path"),
        QApplication::translate("main", "A schema file or a folder of them, or a JSON file to lay "
                                        "over the schema before it."),
        QStringLiteral("[path...]"));
    const QCommandLineOption message(
        QStringLiteral("message"),
        QApplication::translate("main", "The message that the JSON is of. Without it, the likeliest."),
        QStringLiteral("name"));
    parser.addOption(message);
    parser.process(app);

    protoexplorer::MainWindow window;
    const QString problem = window.openAll(parser.positionalArguments(), parser.value(message));
    if (!problem.isEmpty()) {
        qCritical().noquote() << problem;
        return 1;
    }
    window.show();
    return QApplication::exec();
}
