/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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
    if (QString::compare(data, "true", Qt::CaseInsensitive) == 0 || data == "1") {
        value = true;
        return true;
    }
    if (QString::compare(data, "false", Qt::CaseInsensitive) == 0 || data == "0") {
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
    const auto value = data.toUInt(&ok, 0);
    byte = to_byte(value);
    return ok && value < 0x100;
}

#define PARSE_INTEGRAL(method, data, value) do { \
        bool ok;                                 \
        value = data.method(&ok, 0);             \
        return ok;                               \
    } while (false)

#define PARSE_FLOATING(method, data, value) do { \
    bool ok;                                     \
    value = data.method(&ok);                    \
    return ok;                                   \
} while (false)

bool parseShort(const QString& data, short& value) { PARSE_INTEGRAL(toShort, data, value); }
bool parseUShort(const QString& data, ushort& value) { PARSE_INTEGRAL(toUShort, data, value); }
bool parseInt(const QString& data, int& value) { PARSE_INTEGRAL(toInt, data, value); }
bool parseUInt(const QString& data, uint& value) { PARSE_INTEGRAL(toUInt, data, value); }
bool parseLong(const QString& data, long& value) { PARSE_INTEGRAL(toLong, data, value); }
bool parseULong(const QString& data, ulong& value) { PARSE_INTEGRAL(toULong, data, value); }
bool parseLongLong(const QString& data, qlonglong& value) { PARSE_INTEGRAL(toLongLong, data, value); }
bool parseULongLong(const QString& data, qulonglong& value) { PARSE_INTEGRAL(toULongLong, data, value); }
bool parseFloat(const QString& data, float& value) { PARSE_FLOATING(toFloat, data, value); }
bool parseDouble(const QString& data, double& value) { PARSE_FLOATING(toDouble, data, value); }

// note types

QString serializeNote(const Note& note) {
    return QString::fromStdString(note.string());
}

bool parseNote(const QString& data, Note& note) {
    note = Note::from_string(data.toStdString());
    return static_cast<bool>(note);
}

QString serializeRange(const range_t<Note>& range) {
    return QString{"%1:%2"}.arg(serializeNote(range.min), serializeNote(range.max));
}

bool parseRange(const QString& data, range_t<Note>& range) {
    char separator = '\0';
    std::istringstream stream{data.toStdString()};
    try {
        stream >> range.min >> separator >> range.max;
        return separator == ':';
    } catch (const std::exception&) {
        return false;
    }
}

QString serializeNotes(const std::vector<Note>& notes) {
    QStringList stringList;
    for (const auto& note : notes)
        stringList.append(serializeNote(note));
    return stringList.join(";");
}

bool parseNotes(const QString& data, std::vector<Note>& notes) {
    Note note;
    for (const auto& string : data.split(';')) {
        if (!parseNote(string, note))
            return false;
        notes.push_back(note);
    }
    return true;
}

// other types

template<typename T>
QString serializeHex(T value) {
    return QString{"0x%1"}.arg(value, 2*sizeof(T), 16, QChar{'0'});
}

QString serializeChannels(channels_t channels) {
    return serializeHex(channels.to_integral());
}

bool parseChannels(const QString& data, channels_t& channels) {
    ushort value;
    if (parseUShort(data, value)) {
        channels = channels_t::from_integral(value);
        return true;
    }
    return false;
}

QString serializeFamilies(families_t families) {
    return serializeHex(families.to_integral());
}

bool parseFamilies(const QString& data, families_t& families) {
    qulonglong value;
    if (parseULongLong(data, value)) {
        families = families_t::from_integral(value);
        return true;
    }
    return false;
}

QString serializeOrientation(Qt::Orientation orientation) {
    return QMetaEnum::fromType<Qt::Orientation>().valueToKey(orientation);
}

bool parseOrientation(const QString& data, Qt::Orientation& orientation) {
    const auto isHorizontal = serializeOrientation(Qt::Horizontal).startsWith(data, Qt::CaseInsensitive);
    const auto isVertical = serializeOrientation(Qt::Vertical).startsWith(data, Qt::CaseInsensitive);
    if (isHorizontal == isVertical)
        return false;
    orientation = isHorizontal ? Qt::Horizontal : Qt::Vertical;
    return true;
}

}

//==================
// GraphicalHandler
//==================

MetaHandler* makeMetaGraphicalHandler(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->addParameter({"track", "message's track of generated events", "0", MetaHandler::MetaParameter::Visibility::advanced});
    return meta;
}

HandlerView::Parameters GraphicalHandler::getParameters() const {
    auto result = EditableHandler::getParameters();
    SERIALIZE("track", serial::serializeNumber, mTrack, result);
    return result;
}

size_t GraphicalHandler::setParameter(const Parameter& parameter) {
    UNSERIALIZE("track", serial::parseUShort, setTrack, parameter);
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
    produce_message(std::move(event).with_track(mTrack));
}

//============
// Instrument
//============

MetaHandler* makeMetaInstrument(QObject* parent) {
    auto* meta = makeMetaGraphicalHandler(parent);
    meta->addParameter({"velocity", "velocity of note event generated while pressing keys in range [0, 0x80[, values out of range are coerced", "0x7f", MetaHandler::MetaParameter::Visibility::basic});
    return meta;
}

families_t Instrument::handled_families() const {
    return families_t::fuse(family_t::note_on, family_t::note_off, family_t::controller, family_t::reset);
}

Handler::Result Instrument::handle_close(State state) {
    if (state & State::receive())
        receiveClose();
    return GraphicalHandler::handle_close(state);
}

Handler::Result Instrument::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::note_on:
        receiveNoteOn(message.event.channels(), extraction_ns::get_note(message.event));
        return Result::success;
    case family_t::note_off:
        receiveNoteOff(message.event.channels(), extraction_ns::get_note(message.event));
        return Result::success;
    case family_t::reset:
        receiveReset();
        return Result::success;
    case family_t::controller:
        if (extraction_ns::controller(message.event) == controller_ns::all_notes_off_controller) {
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

void Instrument::receiveNotesOff(channels_t) {

}

void Instrument::receiveNoteOn(channels_t, const Note&) {

}

void Instrument::receiveNoteOff(channels_t, const Note&) {

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
