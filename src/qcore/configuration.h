/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2018 Julien Berthault

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

#ifndef QCORE_CONFIGURATION_H
#define QCORE_CONFIGURATION_H

#include <QString>
#include <vector>
#include <QSize>
#include <QPoint>
#include <QColor>
#include <QIODevice>

/**
 * A configuration is an abstraction of a session.
 * It contains all data that can be saved or loaded from xml files.
 */

struct Configuration {

    // ----------
    // properties
    // ----------

    struct Property {
        QString key;
        QString value;
    };

    using Properties = std::vector<Property>;

    // --------
    // handlers
    // --------

    struct Handler {
        QString type;
        QString id;
        QString name;
        Properties properties;
    };

    using Handlers = std::vector<Handler>;

    // -----------
    // connections
    // -----------

    struct Connection {
        QString tail;
        QString head;
        QString source;
    };

    using Connections = std::vector<Connection>;

    // ------
    // frames
    // ------

    struct View {
        QString ref;
    };

    struct Widget;

    using Widgets = std::vector<Widget>;

    struct Frame {
        QString name;
        QSize size;
        QPoint pos;
        Qt::Orientation layout;
        Widgets widgets;
        bool visible;
    };

    using Frames = std::vector<Frame>;

    struct Widget {
        bool isFrame;
        Frame frame;
        View view;
    };

    // ------
    // colors
    // ------

    using Colors = std::vector<QColor>;

    // ------
    // config
    // ------

    Handlers handlers;
    Connections connections;
    Frames frames;
    Colors colors;

    // --
    // IO
    // --

    static Configuration read(const QByteArray& content);
    static Configuration read(QIODevice* device);
    static void write(QByteArray* content, const Configuration& config);
    static void write(QIODevice* device, const Configuration& config);

};

#endif // QCORE_CONFIGURATION_H
