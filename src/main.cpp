/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017 Julien Berthault

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <QApplication>
#include <QSplashScreen>
#include "mainwindow.h"
#include "qcore/core.h"

int main(int argc, char *argv[]) {
    // application
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MIDILab");
    QApplication::setApplicationName("MIDILab");
    QApplication::setApplicationVersion(MIDILAB_VERSION_STRING);
    app.setWindowIcon(QIcon(":/data/logo.png"));
    // register meta types
    qRegisterMetaType<Message>();
    // load default stylesheet if not specified
    if (app.styleSheet().isNull())
        app.setStyleSheet("file:///:/data/theme.stylesheet");
    // command line
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Paths to midi files", "[files...]");
    parser.process(app);
    // load a splash screen
    QPixmap pixmap(":/data/logo.png");
    QSplashScreen splash(pixmap);
    splash.show();
    app.processEvents();
    // create main window
    MainWindow window(nullptr);
    window.readLastConfig();
    window.addFiles(parser.positionalArguments());
    window.show();
    // close the splash when window is built
    splash.finish(&window);
    return app.exec();
}
