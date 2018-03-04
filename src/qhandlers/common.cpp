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

#include "qhandlers/common.h"

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
    addParameter({"velocity", ":byte", "velocity of note event generated while pressing keys in range [0, 0x80[, values out of range are coerced", "0x7f"});
}

//============
// Instrument
//============

Instrument::Instrument(Mode mode) : GraphicalHandler(mode), mVelocity(0x7f) {

}

families_t Instrument::handled_families() const {
    return families_t::fuse(family_t::custom, family_t::note_on, family_t::note_off, family_t::controller, family_t::reset);
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
