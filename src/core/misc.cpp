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
#include "misc.h"

//============
// NoteMemory
//============

NoteMemory::NoteMemory() {
    clear();
}

void NoteMemory::feed(const Event& event) {
    switch (event.family()) {
    case family_t::note_on:
        set_on(event.channels(), extraction_ns::note(event));
        break;
    case family_t::note_off:
        set_off(event.channels(), extraction_ns::note(event));
        break;
    case family_t::controller:
        if ( extraction_ns::controller(event) == controller_ns::all_sound_off_controller ||  extraction_ns::controller(event) == controller_ns::all_notes_off_controller)
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
    channel_ns::clear(m_data, channels);
}

void NoteMemory::clear() {
    channel_ns::clear(m_data);
}

channels_t NoteMemory::active() const {
    return channel_ns::aggregate(m_data);
}

channels_t NoteMemory::active(byte_t note) const {
    return m_data[note];
}

//============
// Corruption
//============

void Corruption::feed(const Event& event) {
    memory.feed(event);
}

void Corruption::tick() {
    corrupted |= memory.active();
}

void Corruption::tick(channels_t channels) {
    corrupted |= memory.active() & channels;
}

channels_t Corruption::reset() {
    return std::exchange(corrupted, {});
}
