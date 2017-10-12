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
#include "misc.h"

using namespace controller_ns;
using namespace family_ns;
using namespace handler_ns;

//==========
// misc ...
//==========

bool asMode(Handler* handler, Handler::mode_type mode) {
    return handler->mode().any(mode);
}

bool asTail(Handler* handler) {
    return asMode(handler, in_mode);
}

bool asHead(Handler* handler) {
    return asMode(handler, out_mode);
}

//============
// NoteMemory
//============

NoteMemory::NoteMemory() {
    clear();
}

void NoteMemory::feed(const Event& event) {
    switch (+event.family()) {
    case note_on_family:
        set_on(event.channels(), event.at(1));
        break;
    case note_off_family:
        set_off(event.channels(), event.at(1));
        break;
    case controller_family:
        if (event.at(1) == all_sound_off_controller || event.at(1) == all_notes_off_controller)
            clear(event.channels());
        break;
    default:
        break;
    }
}

void NoteMemory::set_on(channels_t channels, byte_t note) {
    m_data[note] |= channels;
}

void NoteMemory::set_off(channels_t channels, byte_t note) {
    m_data[note] &= ~channels;
}

void NoteMemory::clear(channels_t channels) {
    if (channels == all_channels)
        clear();
    else
        for (channels_t& cs : m_data)
            cs &= ~channels;
}

void NoteMemory::clear() {
    m_data.fill(channels_t{});
}

channels_t NoteMemory::active() const {
    channels_t result;
    for (channels_t cs : m_data)
        result |= cs;
    return result;
}

channels_t NoteMemory::active(byte_t note) const {
    return m_data[note];
}

//============
// Corruption
//============

void Corruption::feed(const Event& event) {
    m_memory.feed(event);
}

void Corruption::tick() {
    m_corrupted |= m_memory.active();
}

void Corruption::tick(channels_t channels) {
    m_corrupted |= m_memory.active() & channels;
}

channels_t Corruption::reset() {
    channels_t previous = m_corrupted;
    m_corrupted = channels_t();
    return previous;
}
