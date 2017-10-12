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

#include "parsing.h"

#include <QRegExp>

namespace parsing {

/**
 * @todo
 * validate id / ref
 * introduce an editor id to handlers
 *
 */

namespace {

template<typename T, typename F>
bool parseMultiple(const QDomElement& element, QVector<T>& values, F parser) {
    QDomNodeList nodes = element.childNodes();
    for (int i=0 ; i < nodes.size() ; i++) {
        QDomNode node = nodes.at(i);
        if (node.nodeType() == QDomNode::CommentNode)
            continue;
        T value;
        if (!parser(node.toElement(), value))
            return false;
        values.append(value);
    }
    return true;
}

bool parsePos(const QString& value, QPoint& pos) {
    // empty value is fine
    if (value.isEmpty()) {
        pos = QPoint();
        return true;
    }
    // or it must match exactly
    QRegExp re("(\\d+),(\\d+)");
    if (re.exactMatch(value)) {
        // get fields as integers
        QStringList captures = re.capturedTexts();
        bool xOK = false, yOK = false;
        int x = captures.at(1).toInt(&xOK);
        int y = captures.at(2).toInt(&yOK);
        // if conversion succeeds, it's fine
        if (xOK && yOK) {
            pos = QPoint(x, y);
            return true;
        }
    }
    // if match fails or conversion fails, it's an error
    qWarning() << "wrong pos provided, must be <x>,<y>";
    pos = QPoint();
    return false;
}

bool parseSize(const QString& value, QSize& size) {
    // empty value is fine
    if (value.isEmpty()) {
        size = QSize();
        return true;
    }
    // or it must match exactly
    QRegExp re("(\\d+)x(\\d+)");
    if (re.exactMatch(value)) {
        // get fields as integers
        QStringList captures = re.capturedTexts();
        bool widthOK = false, heightOK = false;
        int width = captures.at(1).toInt(&widthOK);
        int height = captures.at(2).toInt(&heightOK);
        // if conversion succeeds, it's fine
        if (widthOK && heightOK) {
            size = QSize(width, height);
            return true;
        }
    }
    // if match fails or conversion fails, it's an error
    qWarning() << "wrong size provided, must be <width>x<height>";
    size = QSize();
    return false;
}

bool parseVisible(const QString& value, bool& visible) {
    if (value == "true") {
        visible = true;
        return true;
    } else if (value == "false") {
        visible = false;
        return true;
    }
    qWarning() << "wrong visibility provided, must be 'true' or 'false'";
    return false;
}

}

bool Reader::parse(const QByteArray& content, Configuration& configuration) {
    QString errorMsg;
    int errorLine, errorColumn;
    QDomDocument document("Config DOM");
    if (!document.setContent(content, false, &errorMsg, &errorLine, &errorColumn)) {
        qCritical() << "config is illformed: " << errorMsg
                    << ", on line " << errorLine
                    << ", on column " << errorColumn;
        return false;
    }
    return parseConfiguration(document.documentElement(), configuration);
}

bool Reader::parseConfiguration(const QDomElement& element, Configuration& configuration) {
    if (element.nodeName() != "configuration") {
        qCritical() << "config root is not \"configuration\"";
        return false;
    }
    QDomNodeList allHandlers = element.elementsByTagName("handlers");
    QDomNodeList allConnections = element.elementsByTagName("connections");
    QDomNodeList allFrames = element.elementsByTagName("frames");
    QDomNodeList allColors = element.elementsByTagName("colors");
    bool handlersOK = allHandlers.isEmpty() || (allHandlers.size() == 1 && parseHandlers(allHandlers.at(0).toElement(), configuration.handlers));
    bool connectionsOK = allConnections.isEmpty() || (allConnections.size() == 1 && parseConnections(allConnections.at(0).toElement(), configuration.connections));
    bool framesOK = allFrames.isEmpty() || (allFrames.size() == 1 && parseFrames(allFrames.at(0).toElement(), configuration.frames));
    bool colorsOK = allColors.isEmpty() || (allColors.size() == 1 && parseColors(allColors.at(0).toElement(), configuration.colors));
    return handlersOK && connectionsOK && framesOK && colorsOK;
}

bool Reader::parseWidgets(const QDomElement& element, QVector<Widget>& widgets) {
    return parseMultiple(element, widgets, [this](const QDomElement& element, Widget& widget) {
        return parseWidget(element, widget);
    });
}

bool Reader::parseFrames(const QDomElement& element, QVector<Frame>& frames) {
    return parseMultiple(element, frames, [this](const QDomElement& element, Frame& frame) {
        return parseFrame(element, frame);
    });
}

bool Reader::parseWidget(const QDomElement& element, Widget& widget) {
    QString nodeName = element.nodeName();
    if (nodeName == "frame") {
        widget.isFrame = true;
        return parseFrame(element, widget.frame);
    } else if (nodeName == "view") {
        widget.isFrame = false;
        return parseView(element, widget.view);
    }
    qWarning() << "unknown tag" << nodeName;
    return false;
}

bool Reader::parseFrame(const QDomElement& element, Frame& frame) {
    QString nodeName = element.nodeName();
    if (nodeName != "frame") {
        qWarning() << "unknown tag" << nodeName;
        return false;
    }
    // parse name
    frame.name = element.attribute("name");
    // parse layout orientation
    QString layout = element.attribute("layout");
    if (layout.isNull()) {
        qWarning() << "attribute layout of window is mandatory";
        return false;
    }
    if (layout == "h")
        frame.layout = Qt::Horizontal;
    else if (layout == "v")
        frame.layout = Qt::Vertical;
    else {
        qWarning() << "attribute layout should be 'h' or 'v'";
        return false;
    }
    // parse pos
    if (!parsePos(element.attribute("pos"), frame.pos))
        return false;
    // parse size
    if (!parseSize(element.attribute("size"), frame.size))
        return false;
    // parse visible
    if (!parseVisible(element.attribute("visible", "true"), frame.visible))
        return false;
    // parse children
    return parseWidgets(element, frame.widgets);
}

bool Reader::parseView(const QDomElement& element, View& view) {
    view.ref = element.attribute("ref");
    if (view.ref.isNull()) {
        qWarning() << "attribute ref of view is mandatory";
        return false;
    }
    return true;
}

bool Reader::parseHandlers(const QDomElement& element, QVector<Handler>& handlers) {
    return parseMultiple(element, handlers, [this](const QDomElement& element, Handler& handler) {
        return parseHandler(element, handler);
    });
}

bool Reader::parseHandler(const QDomElement& element, Handler& handler) {
    // getting type
    handler.type = element.attribute("type");
    if (handler.type.isNull()) {
        qWarning() << "attribute type of handler is mandatory";
        return false;
    }
    // getting name
    handler.name = element.attribute("name", handler.type);
    // getting group
    handler.group = element.attribute("group", "default");
    // getting reference
    handler.id = element.attribute("id");
    // getting properties
    return parseMultiple(element, handler.properties, [this](const QDomElement& element, Property& property) {
        return parseProperty(element, property);
    });
}

bool Reader::parseProperty(const QDomElement& element, Property& property) {
    if (element.nodeName() != "property") {
        qWarning() << "not a property";
        return false;
    }
    // getting key
    property.key = element.attribute("type");
    if (property.key.isEmpty()) {
        qWarning() << "attribute type of property is mandatory";
        return false;
    }
    // getting value
    QDomNode node = element.firstChild();
    if (node.nodeType() != QDomNode::TextNode) {
        qWarning() << "no data provided for property" << property.key;
        return false;
    }
    property.value = node.nodeValue();
    return true;
}

bool Reader::parseConnections(const QDomElement& element, QVector<Connection>& connections) {
    return parseMultiple(element, connections, [this](const QDomElement& element, Connection& connection) {
        return parseConnection(element, connection);
    });
}

bool Reader::parseConnection(const QDomElement& element, Connection& connection) {
    if (element.nodeName() != "connection") {
        qWarning() << "not a connection";
        return false;
    }
    connection.tail = element.attribute("tail");
    connection.head = element.attribute("head");
    connection.source = element.attribute("source");
    if (connection.tail.isNull() || connection.head.isNull() || connection.source.isNull()) {
        qWarning() << "connection attributes 'tail', 'head' and 'source' are mandatory";
        return false;
    }
    return true;
}

bool Reader::parseColors(const QDomElement& element, QVector<QColor>& colors) {
    bool ok = parseMultiple(element, colors, [this](const QDomElement& element, QColor& color) {
        return parseColor(element, color);
    });
    if (!ok)
        return false;
    if (colors.size() != 16) {
        qWarning() << " wrong number of colors provided, 16 expected, got" << colors.size();
        return false;
    }
    return true;
}

bool Reader::parseColor(const QDomElement& element, QColor& color) {
    if (element.nodeName() != "color") {
        qWarning() << "not a color";
        return false;
    }
    // getting value
    QDomNode node = element.firstChild();
    if (node.nodeType() != QDomNode::TextNode) {
        qWarning() << "no data provided for color";
        return false;
    }
    QString colorString = node.nodeValue();
    color.setNamedColor(colorString);
    if (!color.isValid()) {
        qWarning() << "unknown color" << colorString;
        return false;
    }
    return true;
}

Writer::Writer(QXmlStreamWriter& stream) : stream(stream) {

}

void Writer::writeConfiguration(const Configuration& configuration) {
    stream.writeStartElement("configuration");
    writeHandlers(configuration.handlers);
    writeConnections(configuration.connections);
    writeFrames(configuration.frames);
    writeColors(configuration.colors);
    stream.writeEndElement(); // configuration
}

void Writer::writeFrames(const QVector<Frame>& frames) {
    stream.writeStartElement("frames");
    for (const auto& frame : frames)
        writeFrame(frame, true);
    stream.writeEndElement(); // frames
}

void Writer::writeWidget(const Widget& widget) {
    if (widget.isFrame)
        writeFrame(widget.frame, false);
    else
        writeView(widget.view);
}

void Writer::writeView(const View& view) {
    stream.writeStartElement("view");
    stream.writeAttribute("ref", view.ref);
    stream.writeEndElement(); // view
}

void Writer::writeFrame(const Frame& frame, bool isTopLevel) {
    stream.writeStartElement("frame");
    stream.writeAttribute("layout", frame.layout == Qt::Horizontal ? "h" : "v");
    if (!frame.name.isNull())
        stream.writeAttribute("name", frame.name);
    if (isTopLevel && !frame.pos.isNull())
        stream.writeAttribute("pos", QString("%1,%2").arg(frame.pos.x()).arg(frame.pos.y()));
    if (isTopLevel && frame.size.isValid())
        stream.writeAttribute("size", QString("%1x%2").arg(frame.size.width()).arg(frame.size.height()));
    if (isTopLevel)
        stream.writeAttribute("visible", frame.visible ? "true" : "false");
    for (const auto& widget : frame.widgets)
        writeWidget(widget);
    stream.writeEndElement(); // frame
}

void Writer::writeConnections(const QVector<Connection>& connections) {
    stream.writeStartElement("connections");
    for (const auto& connection : connections)
        writeConnection(connection);
    stream.writeEndElement(); // connections
}

void Writer::writeConnection(const Connection& connection) {
    stream.writeStartElement("connection");
    stream.writeAttribute("tail", connection.tail);
    stream.writeAttribute("head", connection.head);
    if (!connection.source.isNull())
        stream.writeAttribute("source", connection.source);
    stream.writeEndElement(); // connection
}

void Writer::writeHandlers(const QVector<Handler>& handlers) {
    stream.writeStartElement("handlers");
    for (const auto& handler: handlers)
        writeHandler(handler);
    stream.writeEndElement(); // handlers
}

void Writer::writeHandler(const Handler& handler) {
    stream.writeStartElement("handler");
    stream.writeAttribute("type", handler.type);
    stream.writeAttribute("id", handler.id);
    stream.writeAttribute("name", handler.name);
    writeProperties(handler.properties);
    stream.writeEndElement(); // handler
}

void Writer::writeProperties(const QVector<Property>& properties) {
    if (!properties.empty())
        for (const auto& property : properties)
            writeProperty(property);
}

void Writer::writeProperty(const Property& property) {
    stream.writeStartElement("property");
    stream.writeAttribute("type", property.key);
    stream.writeCharacters(property.value);
    stream.writeEndElement(); // property
}

void Writer::writeColors(const QVector<QColor>& colors) {
    Q_ASSERT(colors.size() == 16);
    stream.writeStartElement("colors");
    for (const QColor& color : colors)
        writeColor(color);
    stream.writeEndElement(); // colors
}

void Writer::writeColor(const QColor& color) {
    stream.writeStartElement("color");
    stream.writeCharacters(color.name());
    stream.writeEndElement(); // color
}


}
