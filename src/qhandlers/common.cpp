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

#include <sstream>
#include <QMetaEnum>
#include "qhandlers/common.h"

//=============
// Persistence
//=============

namespace serial {

// numeric types

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

QString serializeByte(byte_t byte) {
    return QString::fromStdString(byte_string(byte));
}

bool parseByte(const QString& data, byte_t& byte) {
    bool ok;
    uint value = data.toUInt(&ok, 0);
    byte = value & 0xff;
    return ok && value < 0x100;
}

#define PARSE_NUMBER(method, data, value) do { \
        bool ok;                               \
        value = data.method(&ok);              \
        return ok;                             \
    } while (false)

bool parseShort(const QString& data, short& value) { PARSE_NUMBER(toShort, data, value); }
bool parseUShort(const QString& data, ushort& value) { PARSE_NUMBER(toUShort, data, value); }
bool parseInt(const QString& data, int& value) { PARSE_NUMBER(toInt, data, value); }
bool parseUInt(const QString& data, uint& value) { PARSE_NUMBER(toUInt, data, value); }
bool parseLong(const QString& data, long& value) { PARSE_NUMBER(toLong, data, value); }
bool parseULong(const QString& data, ulong& value) { PARSE_NUMBER(toULong, data, value); }
bool parseLongLong(const QString& data, qlonglong& value) { PARSE_NUMBER(toLongLong, data, value); }
bool parseULongLong(const QString& data, qulonglong& value) { PARSE_NUMBER(toULongLong, data, value); }
bool parseFloat(const QString& data, float& value) { PARSE_NUMBER(toFloat, data, value); }
bool parseDouble(const QString& data, double& value) { PARSE_NUMBER(toDouble, data, value); }

// note types

QString serializeNote(const Note& note) {
    return QString::fromStdString(note.string());
}

bool parseNote(const QString& data, Note& note) {
    note = Note::from_string(data.toStdString());
    return (bool)note;
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

QString serializeNotes(const std::vector<Note>& notes) {
    QStringList stringList;
    for (const Note& note : notes)
        stringList.append(QString::fromStdString(note.string()));
    return stringList.join(";");
}

bool parseNotes(const QString& data, std::vector<Note>& notes) {
    for (const QString string : data.split(';')) {
        auto note = Note::from_string(string.toStdString());
        if (!note)
            return false;
        notes.push_back(note);
    }
    return true;
}

// other types

QString serializeOrientation(Qt::Orientation orientation) {
    return QMetaEnum::fromType<Qt::Orientation>().valueToKey(orientation);
}

bool parseOrientation(const QString& data, Qt::Orientation& orientation) {
    bool ok;
    orientation = static_cast<Qt::Orientation>(QMetaEnum::fromType<Qt::Orientation>().keyToValue(data.toLocal8Bit().constData(), &ok));
    return ok;
}

}

//======================
// MetaGraphicalHandler
//======================

MetaGraphicalHandler::MetaGraphicalHandler(QObject* parent) : OpenMetaHandler(parent) {
    addParameter("track", ":uint", "message's track of generated events", "0");
}

//==================
// GraphicalHandler
//==================

GraphicalHandler::GraphicalHandler(Mode mode) : EditableHandler(mode), mTrack(Message::no_track) {

}

HandlerView::Parameters GraphicalHandler::getParameters() const {
    auto result = EditableHandler::getParameters();
    SERIALIZE("track", serial::serializeNumber, mTrack, result);
    return result;
}

size_t GraphicalHandler::setParameter(const Parameter& parameter) {
    UNSERIALIZE("track", serial::parseULong, setTrack, parameter);
    return EditableHandler::setParameter(parameter);
}

track_t GraphicalHandler::track() const {
    return mTrack;
}

void GraphicalHandler::setTrack(track_t track) {
    mTrack = track;
}

bool GraphicalHandler::canGenerate() const {
    return state().any(State::forward()) && isEnabled();
}

void GraphicalHandler::generate(Event event) {
    forward_message({std::move(event), this, mTrack});
}

//================
// MetaInstrument
//================

MetaInstrument::MetaInstrument(QObject* parent) : MetaGraphicalHandler(parent) {
    addParameter("velocity", ":byte", "velocity of note event generated while pressing keys in range [0, 0x80[, values out of range are coerced", "0x7f");
}

//============
// Instrument
//============

Instrument::Instrument(Mode mode) : GraphicalHandler(mode), mVelocity(0x7f) {

}

families_t Instrument::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::note_on, family_t::note_off, family_t::controller, family_t::reset);
}

Handler::Result Instrument::on_close(State state) {
    if (state & State::receive())
        receiveClose();
    return GraphicalHandler::on_close(state);
}

Handler::Result Instrument::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    switch (message.event.family()) {
    case family_t::note_on:
        receiveNoteOn(message.event.channels(), message.event.get_note());
        return Result::success;
    case family_t::note_off:
        receiveNoteOff(message.event.channels(), message.event.get_note());
        return Result::success;
    case family_t::reset:
        receiveReset();
        return Result::success;
    case family_t::controller:
        if (message.event.at(1) == controller_ns::all_notes_off_controller) {
            receiveNotesOff(message.event.channels());
            return Result::success;
        }
    }
    return Result::unhandled;
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
    receiveNotesOff(channels_t::full());
}

void Instrument::receiveReset() {
    receiveNotesOff(channels_t::full());
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

channels_t Instrument::channelsFromButtons(Qt::MouseButtons buttons) {
    auto* editor = channelEditor();
    return editor ? editor->channelsFromButtons(buttons) : channels_t::wrap(0);
}
