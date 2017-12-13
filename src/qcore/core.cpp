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

#include <QPluginLoader>
#include <QLabel>
#include <QMetaEnum>
#include <QPushButton>
#include <QLineEdit>
#include <sstream>
#include "core.h"
#include "qtools/misc.h"

//=================
// Name Conversion
//=================

std::string qstring2name(const QString& string) {
    return {string.toUtf8().constData()};
}

QString name2qstring(const std::string& name) {
    return QString::fromUtf8(name.data());
}

QString handlerName(const Handler* handler) {
    return handler ? name2qstring(handler->name()) : "null";
}

QString eventName(const Event& event) {
    return QString::fromStdString(event.name());
}

QString metaHandlerName(const MetaHandler* meta) {
    return meta ? meta->identifier() : "Unknown MetaHandler";
}

//=============
// Persistence
//=============

namespace serial {

QString serializeNote(const Note& note) {
    return QString::fromStdString(note.string());
}

bool parseNote(const QString& data, Note& note) {
    note = Note::from_string(data.toStdString());
    return (bool)note;
}

QString serializeByte(byte_t byte) {
    return QString::fromStdString(byte_string(byte));
}

bool parseByte(const QString& data, byte_t& byte) {
    bool ok;
    uint value = data.toUInt(&ok, 0);
    byte  = value & 0xff;
    return ok && value < 0x100;
}

QString serializeRange(const QPair<Note, Note>& range) {
    return QString("%1:%2").arg(serializeNote(range.first), serializeNote(range.second));
}

bool parseRange(const QString& data, QPair<Note, Note>& range) {
    char separator = '\0';
    std::istringstream stream(data.toStdString());
    try {
        stream >> range.first >> separator >> range.second;
        return separator == ':';
    } catch (const std::exception&) {
        return false;
    }
}

QString serializeTrack(track_t track) {
    return QString::number(track);
}

bool parseTrack(const QString& data, track_t& track) {
    bool ok;
    track = data.toULong(&ok, 0);
    return ok;
}

QString serializeOrientation(Qt::Orientation orientation) {
    return QMetaEnum::fromType<Qt::Orientation>().valueToKey(orientation);
}

bool parseOrientation(const QString& data, Qt::Orientation& orientation) {
    bool ok;
    orientation = static_cast<Qt::Orientation>(QMetaEnum::fromType<Qt::Orientation>().keyToValue(data.toLocal8Bit().constData(), &ok));
    return ok;
}

QString serializeBool(bool value) {
    return value ? "true" : "false";
}

bool parseBool(const QString& data, bool& value) {
    if (data == "true") {
        value = true;
        return true;
    }
    if (data == "false") {
        value = false;
        return true;
    }
    return false;
}

QString serializeDouble(double value) {
    return QString::number(value);
}

bool parseDouble(const QString& data, double& value) {
    bool ok;
    value = data.toDouble(&ok);
    return ok;
}

}

//==============
// StorageEvent
//==============

const QEvent::Type StorageEvent::family = (QEvent::Type)QEvent::registerEventType();

StorageEvent::StorageEvent(Handler* target, Message message) :
    QEvent(family), target(target), message(std::move(message)) {

}

//=================
// GraphicalHolder
//=================

GraphicalHolder::GraphicalHolder(QObject* parent) : QObject(parent), Holder() {

}

bool GraphicalHolder::hold_message(Handler* target, const Message& message) {
    QApplication::postEvent(this, new StorageEvent(target, message), Qt::HighEventPriority);
    return true;
}

bool GraphicalHolder::event(QEvent* e) {
    if (e->type() == StorageEvent::family) {
        auto storageEvent = static_cast<StorageEvent*>(e);
        storageEvent->target->receive_message(storageEvent->message);
        return true;
    }
    return QObject::event(e);
}

//==========
// Observer
//==========

Handler::result_type Observer::handleMessage(Handler* target, const Message& message) {
    auto result = target->handle_message(message);
    if (result == Handler::result_type::success && message.event.is(~family_ns::note_families))
        QApplication::postEvent(this, new StorageEvent(target, message));
    return result;
}

bool Observer::event(QEvent* e) {
    if (e->type() == StorageEvent::family) {
        auto storageEvent = static_cast<StorageEvent*>(e);
        emit messageHandled(storageEvent->target, storageEvent->message);
        return true;
    }
    return QObject::event(e);
}

//=================
// DefaultReceiver
//=================

DefaultReceiver::DefaultReceiver(QObject* parent) : Observer(parent), Receiver() {

}

Handler::result_type DefaultReceiver::receive_message(Handler* target, const Message& message) {
    return handleMessage(target, message);
}

//================
// CustomObserver
//================

CustomReceiver::CustomReceiver(QObject* parent) : QObject(parent), Receiver(), mObserver(nullptr) {

}

Observer* CustomReceiver::observer() {
    return mObserver;
}

void CustomReceiver::setObserver(Observer* observer) {
    mObserver = observer;
}

//=============
// MetaHandler
//=============

const QString& MetaHandler::identifier() const {
    return mIdentifier;
}

const QString& MetaHandler::description() const {
    return mDescription;
}

void MetaHandler::setIdentifier(const QString& identifier) {
    mIdentifier = identifier;
}

void MetaHandler::setDescription(const QString& description) {
    mDescription = description;
}

const MetaHandler::MetaParameters& MetaHandler::parameters() const {
    return mParameters;
}

void MetaHandler::addParameters(const MetaParameters& parameters) {
    mParameters.insert(mParameters.end(), parameters.begin(), parameters.end());
}

void MetaHandler::addParameter(const MetaParameter& parameter) {
    mParameters.push_back(parameter);
}

void MetaHandler::addParameter(const QString& name, const QString& type, const QString& description, const QString& defaultValue) {
    addParameter(MetaParameter{name, type, description, defaultValue});
}

//======================
// MetaHandlerCollector
//======================

const QList<MetaHandler*>& MetaHandlerCollector::metaHandlers() const {
    return mMetaHandlers;
}

MetaHandler* MetaHandlerCollector::metaHandler(const QString& type) {
    for (MetaHandler* meta : mMetaHandlers)
        if (meta->identifier() == type)
            return meta;
    qWarning() << "unknown metahandler named" << type;
    return nullptr;
}

size_t MetaHandlerCollector::addMetaHandler(MetaHandler* meta) {
    if (meta) {
        meta->setParent(this);
        mMetaHandlers << meta;
        return 1;
    }
    return 0;
}

size_t MetaHandlerCollector::addFactory(MetaHandlerFactory* factory) {
    QList<MetaHandler*> newMetaHandlers;
    if (factory)
        newMetaHandlers = factory->spawn();
    mMetaHandlers << newMetaHandlers;
    return newMetaHandlers.size();
}

size_t MetaHandlerCollector::addPlugin(const QString& filename) {
    size_t count = 0;
    QPluginLoader loader(filename, this);
    QObject* plugin = loader.instance();
    if (plugin) { /// @note lifetime is automatically managed
        count += addFactory(dynamic_cast<MetaHandlerFactory*>(plugin));
    } else {
        qWarning() << "file" << filename << "is not a plugin : " << loader.errorString() << loader.metaData();
    }
    return count;
}

size_t MetaHandlerCollector::addPlugins(const QDir& dir) {
    size_t count = 0;
    for (const QString& filename : dir.entryList(QDir::Files))
        if (QLibrary::isLibrary(filename))
            count += addPlugin(dir.absoluteFilePath(filename));
    if (count == 0)
        qWarning() << "can't find any plugin in" << dir.absolutePath();
    return count;
}

//=============
// HandlerView
//=============

HandlerView::HandlerView() : QWidget(nullptr), mContext(nullptr) {

}

ChannelEditor* HandlerView::channelEditor() {
    return context() ? context()->channelEditor() : nullptr;
}

Context* HandlerView::context() {
    return mContext;
}

void HandlerView::setContext(Context* context) {
    updateContext(context);
    mContext = context;
}

HandlerView::Parameters HandlerView::getParameters() const {
    return {};
}

size_t HandlerView::setParameter(const Parameter& /*parameter*/) {
    return 0;
}

size_t HandlerView::setParameters(const Parameters& parameters) {
    size_t results = 0;
    for (const auto& parameter : parameters) {
        size_t count = setParameter(parameter);
        if (!count)
            TRACE_WARNING("unable to set parameter " << parameter.name.toStdString());
        results += count;
    }
    return results;
}

void HandlerView::updateContext(Context* /*context*/) {

}

//======================
// MetaGraphicalHandler
//======================

MetaGraphicalHandler::MetaGraphicalHandler(QObject* parent) : MetaHandler(parent) {
    addParameter("track", ":uint", "message's track of generated events", "0");
}

//==================
// GraphicalHandler
//==================

GraphicalHandler::GraphicalHandler(mode_type mode) : HandlerView(), Handler(mode), mTrack(Message::no_track) {

}

HandlerView::Parameters GraphicalHandler::getParameters() const {
    auto result = HandlerView::getParameters();
    SERIALIZE("track", serial::serializeTrack, mTrack, result);
    return result;
}

size_t GraphicalHandler::setParameter(const Parameter& parameter) {
    UNSERIALIZE("track", serial::parseTrack, setTrack, parameter);
    return HandlerView::setParameter(parameter);
}

track_t GraphicalHandler::track() const {
    return mTrack;
}

void GraphicalHandler::setTrack(track_t track) {
    mTrack = track;
}

bool GraphicalHandler::canGenerate() const {
    return state().any(handler_ns::forward_state) && isEnabled();
}

void GraphicalHandler::generate(Event event) {
    forward_message({std::move(event), this, mTrack});
}

//================
// MetaInstrument
//================

MetaInstrument::MetaInstrument(QObject* parent) : MetaGraphicalHandler(parent) {
    addParameter({"velocity", ":byte", "velocity of note event generated while pressing keys in range [0, 0x80[, values out of range are coerced", "0x7f"});
}

//============
// Instrument
//============

Instrument::Instrument(mode_type mode) : GraphicalHandler(mode), mVelocity(0x7f) {

}

families_t Instrument::handled_families() const {
    return families_t::merge(family_t::custom, family_t::note_on, family_t::note_off, family_t::controller, family_t::reset);
}

Handler::result_type Instrument::on_close(state_type state) {
    if (state & handler_ns::receive_state)
        receiveClose();
    return GraphicalHandler::on_close(state);
}

Handler::result_type Instrument::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;

    switch (message.event.family()) {
    case family_t::note_on:
        receiveNoteOn(message.event.channels(), message.event.get_note());
        return result_type::success;
    case family_t::note_off:
        receiveNoteOff(message.event.channels(), message.event.get_note());
        return result_type::success;
    case family_t::reset:
        receiveReset();
        return result_type::success;
    case family_t::controller:
        if (message.event.at(1) == controller_ns::all_notes_off_controller) {
            receiveNotesOff(message.event.channels());
            return result_type::success;
        }
    }
    return result_type::unhandled;
}

HandlerView::Parameters Instrument::getParameters() const {
    auto result = GraphicalHandler::getParameters();
    SERIALIZE("velocity", serial::serializeByte, mVelocity, result);
    return result;
}

size_t Instrument::setParameter(const Parameter& parameter) {
    UNSERIALIZE("velocity", serial::parseByte, setVelocity, parameter);
    return GraphicalHandler::setParameter(parameter);
}

byte_t Instrument::velocity() const {
    return mVelocity;
}

void Instrument::setVelocity(byte_t velocity) {
    mVelocity = to_data_byte(velocity);
}

void Instrument::receiveClose() {
    receiveNotesOff(all_channels);
}

void Instrument::receiveReset() {
    receiveNotesOff(all_channels);
}

void Instrument::receiveNotesOff(channels_t /*channels*/) {

}

void Instrument::receiveNoteOn(channels_t /*channels*/, const Note& /*note*/) {

}

void Instrument::receiveNoteOff(channels_t /*channels*/, const Note& /*note*/) {

}

void Instrument::generateNoteOn(channels_t channels, const Note& note) {
    generate(Event::note_on(channels, note.code(), velocity()));
}

void Instrument::generateNoteOff(channels_t channels, const Note& note) {
    generate(Event::note_off(channels, note.code()));
}
