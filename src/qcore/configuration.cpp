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

#include <QRegExp>
#include <QDomElement>
#include <QXmlStreamWriter>
#include <QXmlInputSource>
#include "qcore/configuration.h"
#include "tools/trace.h"

/**
 * @todo ...
 * - validate id / ref
 * - introduce an editor id to handlers
 * - write a xsd
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

// ----------
// properties
// ----------

Configuration::Property parseProperty(const QDomElement& element) {
    checkNodeName(element, "property");
    auto key = parseAttribute(element, "type");
    auto node = element.firstChild();
    if (!node.isNull() && node.nodeType() != QDomNode::TextNode)
        throw QString("no data provided for property %1").arg(key);
    return {key, node.nodeValue()};
}

void writeProperty(QXmlStreamWriter& stream, const Configuration::Property& property) {
    stream.writeStartElement("property");
    stream.writeAttribute("type", property.key);
    stream.writeCharacters(property.value);
    stream.writeEndElement(); // property
}

// --------
// handlers
// --------

Configuration::Handler parseHandler(const QDomElement& element) {
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

void writeHandler(QXmlStreamWriter& stream, const Configuration::Handler& handler) {
    stream.writeStartElement("handler");
    stream.writeAttribute("type", handler.type);
    stream.writeAttribute("id", handler.id);
    stream.writeAttribute("name", handler.name);
    if (!handler.group.isEmpty() && handler.group != "default")
        stream.writeAttribute("group", handler.group);
    for (const auto& property : handler.properties)
        writeProperty(stream, property);
    stream.writeEndElement(); // handler
}

auto parseHandlers(const QDomElement& element) {
    return parseMultiple(element, parseHandler);
}

void writeHandlers(QXmlStreamWriter& stream, const Configuration::Handlers& handlers) {
    stream.writeStartElement("handlers");
    for (const auto& handler: handlers)
        writeHandler(stream, handler);
    stream.writeEndElement(); // handlers
}

// -----------
// connections
// -----------

Configuration::Connection parseConnection(const QDomElement& element) {
    checkNodeName(element, "connection");
    return {
        parseAttribute(element, "tail"),
        parseAttribute(element, "head"),
        element.attribute("source")
    };
}

void writeConnection(QXmlStreamWriter& stream, const Configuration::Connection& connection) {
    stream.writeStartElement("connection");
    stream.writeAttribute("tail", connection.tail);
    stream.writeAttribute("head", connection.head);
    if (!connection.source.isEmpty())
        stream.writeAttribute("source", connection.source);
    stream.writeEndElement(); // connection
}

auto parseConnections(const QDomElement& element) {
    return parseMultiple(element, parseConnection);
}

void writeConnections(QXmlStreamWriter& stream, const Configuration::Connections& connections) {
    stream.writeStartElement("connections");
    for (const auto& connection : connections)
        writeConnection(stream, connection);
    stream.writeEndElement(); // connections
}

// ----------------
// frame attributes
// ----------------

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

void writePos(QXmlStreamWriter& stream, const QPoint& pos) {
    if (!pos.isNull())
        stream.writeAttribute("pos", QString("%1,%2").arg(pos.x()).arg(pos.y()));
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

void writeSize(QXmlStreamWriter& stream, const QSize& size) {
    if (size.isValid())
        stream.writeAttribute("size", QString("%1x%2").arg(size.width()).arg(size.height()));
}

bool parseVisible(const QString& value) {
    if (value == "true")
        return true;
    if (value == "false")
        return false;
    throw QString("wrong visibility provided, must be 'true' or 'false'");
}

void writeVisible(QXmlStreamWriter& stream, bool visible) {
    stream.writeAttribute("visible", visible ? "true" : "false");
}

Qt::Orientation parseLayout(const QString& value) {
    if (value == "h")
        return Qt::Horizontal;
    if (value == "v")
        return Qt::Vertical;
    throw QString("layout should be 'h' or 'v'");
}

void writeLayout(QXmlStreamWriter& stream, Qt::Orientation layout) {
    stream.writeAttribute("layout", layout == Qt::Horizontal ? "h" : "v");
}

// ------
// frames
// ------

Configuration::Frame parseFrame(const QDomElement& element);
void writeFrame(QXmlStreamWriter& stream, const Configuration::Frame& frame, bool isTopLevel);

Configuration::View parseView(const QDomElement& element) {
    return { parseAttribute(element, "ref") };
}

void writeView(QXmlStreamWriter& stream, const Configuration::View& view) {
    stream.writeStartElement("view");
    stream.writeAttribute("ref", view.ref);
    stream.writeEndElement(); // view
}

Configuration::Widget parseWidget(const QDomElement& element) {
    auto nodeName = element.nodeName();
    if (nodeName == "frame")
        return {true, parseFrame(element), {}};
    if (nodeName == "view")
        return {false, {}, parseView(element)};
    throw QString("unknown tag %1").arg(nodeName);
}

void writeWidget(QXmlStreamWriter& stream, const Configuration::Widget& widget) {
    if (widget.isFrame)
        writeFrame(stream, widget.frame, false);
    else
        writeView(stream, widget.view);
}

Configuration::Frame parseFrame(const QDomElement& element) {
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

void writeFrame(QXmlStreamWriter& stream, const Configuration::Frame& frame, bool isTopLevel) {
    stream.writeStartElement("frame");
    writeLayout(stream, frame.layout);
    if (isTopLevel) {
        if (!frame.name.isEmpty())
            stream.writeAttribute("name", frame.name);
        writePos(stream, frame.pos);
        writeSize(stream, frame.size);
        writeVisible(stream, frame.visible);
    }
    for (const auto& widget : frame.widgets)
        writeWidget(stream, widget);
    stream.writeEndElement(); // frame
}

auto parseFrames(const QDomElement& element) {
    return parseMultiple(element, parseFrame);
}

void writeFrames(QXmlStreamWriter& stream, const Configuration::Frames& frames) {
    stream.writeStartElement("frames");
    for (const auto& frame : frames)
        writeFrame(stream, frame, true);
    stream.writeEndElement(); // frames
}

// ------
// colors
// ------

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

void writeColor(QXmlStreamWriter& stream, const QColor& color) {
    stream.writeStartElement("color");
    stream.writeCharacters(color.name());
    stream.writeEndElement(); // color
}

auto parseColors(const QDomElement& element) {
    auto colors = parseMultiple(element, parseColor);
    if (colors.size() != 16)
        throw QString("wrong number of colors provided, 16 expected, got %1").arg(colors.size());
    return colors;
}

void writeColors(QXmlStreamWriter& stream, const Configuration::Colors& colors) {
    Q_ASSERT(colors.size() == 16);
    stream.writeStartElement("colors");
    for (const auto& color : colors)
        writeColor(stream, color);
    stream.writeEndElement(); // colors
}

// ------
// config
// ------

Configuration parseConfiguration(const QDomElement& element) {
    checkNodeName(element, "configuration");
    return {
        parseAtMostOne(element, "handlers", parseHandlers),
        parseAtMostOne(element, "connections", parseConnections),
        parseAtMostOne(element, "frames", parseFrames),
        parseAtMostOne(element, "colors", parseColors)
    };
}

void writeConfiguration(QXmlStreamWriter& stream, const Configuration& config) {
    stream.writeStartElement("configuration");
    writeHandlers(stream, config.handlers);
    writeConnections(stream, config.connections);
    writeFrames(stream, config.frames);
    writeColors(stream, config.colors);
    stream.writeEndElement(); // configuration
}

// ---------
// document
// ---------

Configuration parseDocument(QXmlInputSource& stream) {
    TRACE_MEASURE("reading configuration");
    QString errorMsg;
    int errorLine, errorColumn;
    QDomDocument document; //("Config DOM");
    if (!document.setContent(&stream, false, &errorMsg, &errorLine, &errorColumn))
        throw QString("%1 (line %2, column %3)").arg(errorMsg).arg(errorLine).arg(errorColumn);
    return parseConfiguration(document.documentElement());
}

void writeDocument(QXmlStreamWriter& stream, const Configuration& config) {
    TRACE_MEASURE("writing configuration");
    stream.setAutoFormatting(true);
    stream.writeStartDocument();
    writeConfiguration(stream, config);
    stream.writeEndDocument();
}

}

Configuration Configuration::read(const QByteArray& content) {
    QXmlInputSource stream;
    stream.setData(content);
    return parseDocument(stream);
}

Configuration Configuration::read(QIODevice* device) {
    QXmlInputSource stream(device);
    return parseDocument(stream);
}

void Configuration::write(QByteArray* content, const Configuration& config) {
    QXmlStreamWriter stream(content);
    writeDocument(stream, config);
}

void Configuration::write(QIODevice* device, const Configuration& config) {
    QXmlStreamWriter stream(device);
    writeDocument(stream, config);
}
