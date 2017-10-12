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

#ifndef QCORE_PARSING_H
#define QCORE_PARSING_H

#include <QString>
#include <QDomElement>
#include <QXmlStreamWriter>
#include "core.h"

namespace parsing {

struct Property {
    QString key;
    QString value;
};

struct Handler {
    QString type;
    QString id;
    QString name;
    QString group;
    QVector<Property> properties;
};

struct Connection {
    QString tail;
    QString head;
    QString source;
};

struct View {
    QString ref;
};

struct Widget;

struct Frame {
    QString name;
    QSize size;
    QPoint pos;
    Qt::Orientation layout;
    QVector<Widget> widgets;
    bool visible;
};

struct Widget {
    bool isFrame;
    Frame frame;
    View view;
};

struct Configuration {
    QVector<Handler> handlers;
    QVector<Connection> connections;
    QVector<Frame> frames;
    QVector<QColor> colors;
};

class Reader {

public:
    bool parse(const QByteArray& content, Configuration& configuration);
    bool parseConfiguration(const QDomElement& element, Configuration& configuration);

    bool parseWidgets(const QDomElement& element, QVector<Widget>& widgets);
    bool parseFrames(const QDomElement& element, QVector<Frame>& frames);
    bool parseWidget(const QDomElement& element, Widget& widget);
    bool parseFrame(const QDomElement& element, Frame& frame);
    bool parseView(const QDomElement& element, View& view);

    bool parseConnections(const QDomElement& element, QVector<Connection>& connections);
    bool parseConnection(const QDomElement& element, Connection& connection);

    bool parseHandlers(const QDomElement& element, QVector<Handler>& handlers);
    bool parseHandler(const QDomElement& element, Handler& handler);
    bool parseProperty(const QDomElement& element, Property& property);

    bool parseColors(const QDomElement& element, QVector<QColor>& color);
    bool parseColor(const QDomElement& element, QColor& color);

};

class Writer {

public:
    Writer(QXmlStreamWriter& stream);

    void writeConfiguration(const Configuration& configuration);

    void writeFrames(const QVector<Frame>& frames);
    void writeWidget(const Widget& widget);
    void writeView(const View& view);
    void writeFrame(const Frame& frame, bool isTopLevel);

    void writeConnections(const QVector<Connection>& connections);
    void writeConnection(const Connection& connection);

    void writeHandlers(const QVector<Handler>& handlers);
    void writeHandler(const Handler& handler);
    void writeProperties(const QVector<Property>& properties);
    void writeProperty(const Property& property);

    void writeColors(const QVector<QColor>& colors);
    void writeColor(const QColor& color);

private:
    QXmlStreamWriter& stream;

};

} // namespace parsing

#endif // QCORE_PARSING_H
