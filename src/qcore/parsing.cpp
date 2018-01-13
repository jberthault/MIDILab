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

void checkNodeName(const QDomElement& element, const QString& name) {
    if (element.nodeName() != name)
        throw QString("expected element named '%1'").arg(name);
}

template<typename F>
auto parseMultiple(const QDomElement& element, F parser) {
    using result_type = decltype(parser(element));
    QVector<result_type> values;
    auto nodes = element.childNodes();
    for (int i=0 ; i < nodes.size() ; i++) {
        auto node = nodes.at(i);
        if (node.nodeType() != QDomNode::CommentNode)
            values.append(parser(node.toElement()));
    }
    return values;
}

template<typename F>
auto parseAtMostOne(const QDomElement& element, const QString& tagName, F parser) {
    using result_type = decltype(parser(element));
    auto nodes = element.elementsByTagName(tagName);
    if (nodes.size() > 1)
        throw QString("too many tags named '%1'").arg(tagName);
    if (nodes.size() == 1)
        return parser(nodes.at(0).toElement());
    return result_type{};
}

auto parseAttribute(const QDomElement& element, const QString& attributeName) {
    auto value = element.attribute(attributeName);
    if (value.isEmpty())
        throw QString("attribute '%1' of tag '%2' is mandatory").arg(attributeName, element.nodeName());
    return value;
}

// handlers

Property parseProperty(const QDomElement& element) {
    checkNodeName(element, "property");
    auto key = parseAttribute(element, "type");
    auto node = element.firstChild();
    if (node.nodeType() != QDomNode::TextNode)
        throw QString("no data provided for property %1").arg(key);
    return {key, node.nodeValue()};
}

Handler parseHandler(const QDomElement& element) {
    checkNodeName(element, "handler");
    auto type = parseAttribute(element, "type");
    return {
        type,
        element.attribute("id"),
        element.attribute("name", type),
        element.attribute("group", "default"),
        parseMultiple(element, parseProperty)
    };
}

auto parseHandlers(const QDomElement& element) {
    return parseMultiple(element, parseHandler);
}

// connections

Connection parseConnection(const QDomElement& element) {
    checkNodeName(element, "connection");
    return {
        parseAttribute(element, "tail"),
        parseAttribute(element, "head"),
        element.attribute("source")
    };
}

auto parseConnections(const QDomElement& element) {
    return parseMultiple(element, parseConnection);
}

// frames

QPoint parsePos(const QString& value) {
    // empty value is fine
    if (value.isEmpty())
        return {};
    // or it must match exactly
    QRegExp re("(\\d+),(\\d+)");
    if (re.exactMatch(value)) {
        // get fields as integers
        QStringList captures = re.capturedTexts();
        bool xOK = false, yOK = false;
        int x = captures.at(1).toInt(&xOK);
        int y = captures.at(2).toInt(&yOK);
        // if conversion succeeds, it's fine
        if (xOK && yOK)
            return {x, y};
    }
    // if match fails or conversion fails, it's an error
    throw QString("wrong pos provided, must be <x>,<y>");
}

QSize parseSize(const QString& value) {
    // empty value is fine
    if (value.isEmpty())
        return {};
    // or it must match exactly
    QRegExp re("(\\d+)x(\\d+)");
    if (re.exactMatch(value)) {
        // get fields as integers
        QStringList captures = re.capturedTexts();
        bool widthOK = false, heightOK = false;
        int width = captures.at(1).toInt(&widthOK);
        int height = captures.at(2).toInt(&heightOK);
        // if conversion succeeds, it's fine
        if (widthOK && heightOK)
            return {width, height};
    }
    // if match fails or conversion fails, it's an error
    throw QString("wrong size provided, must be <width>x<height>");
}

bool parseVisible(const QString& value) {
    if (value == "true")
        return true;
    if (value == "false")
        return false;
    throw QString("wrong visibility provided, must be 'true' or 'false'");
}

Qt::Orientation parseLayout(const QString& value) {
    if (value == "h")
        return Qt::Horizontal;
    if (value == "v")
        return Qt::Vertical;
    throw QString("layout should be 'h' or 'v'");
}

View parseView(const QDomElement& element) {
    return { parseAttribute(element, "ref") };
}

Frame parseFrame(const QDomElement& element);

Widget parseWidget(const QDomElement& element) {
    auto nodeName = element.nodeName();
    if (nodeName == "frame")
        return {true, parseFrame(element), {}};
    if (nodeName == "view")
        return {false, {}, parseView(element)};
    throw QString("unknown tag %1").arg(nodeName);
}

Frame parseFrame(const QDomElement& element) {
    checkNodeName(element, "frame");
    return {
        element.attribute("name"),
        parseSize(element.attribute("size")),
        parsePos(element.attribute("pos")),
        parseLayout(parseAttribute(element, "layout")),
        parseMultiple(element, parseWidget),
        parseVisible(element.attribute("visible", "true"))
    };
}

auto parseFrames(const QDomElement& element) {
    return parseMultiple(element, parseFrame);
}

// colors

QColor parseColor(const QDomElement& element) {
    checkNodeName(element, "color");
    QDomNode node = element.firstChild();
    if (node.nodeType() != QDomNode::TextNode)
        throw QString("no data provided for color");
    QString colorString = node.nodeValue();
    QColor color(colorString);
    if (!color.isValid())
        throw QString("unknown color %1").arg(colorString);
    return color;
}

auto parseColors(const QDomElement& element) {
    auto colors = parseMultiple(element, parseColor);
    if (colors.size() != 16)
        throw QString("wrong number of colors provided, 16 expected, got %1").arg(colors.size());
    return colors;
}

}

Configuration readConfiguration(const QByteArray& content) {
    QString errorMsg;
    int errorLine, errorColumn;
    QDomDocument document("Config DOM");
    if (!document.setContent(content, false, &errorMsg, &errorLine, &errorColumn))
        throw QString("%1 (line %2, column %3)").arg(errorMsg).arg(errorLine).arg(errorColumn);
    return readConfiguration(document.documentElement());
}

Configuration readConfiguration(const QDomElement& element) {
    checkNodeName(element, "configuration");
    return {
        parseAtMostOne(element, "handlers", parseHandlers),
        parseAtMostOne(element, "connections", parseConnections),
        parseAtMostOne(element, "frames", parseFrames),
        parseAtMostOne(element, "colors", parseColors)
    };
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
    if (!connection.source.isEmpty())
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
