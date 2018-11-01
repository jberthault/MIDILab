/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2018 Julien Berthault

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

#include "sequencewriter.h"

namespace {

constexpr auto recording_state = Handler::State::from_integral(0x4);

}

//================
// SequenceWriter
//================

/// @todo remove the lock guard

SequenceWriter::SequenceWriter() : Handler{Mode::out()} {
    m_storage.reserve(8192);
}

void SequenceWriter::set_families(families_t families) {
    m_families = families;
}

Sequence SequenceWriter::load_sequence() const {
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    return Sequence::from_realtime(m_storage);
}

Handler::Result SequenceWriter::handle_message(const Message& message) {
    if (!message.event.is(m_families) || state().none(recording_state))
        return Result::unhandled;
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    m_storage.push_back({clock_type::now(), message.event});
    return Result::success;
}

void SequenceWriter::start_recording() {
    if (activate_state(recording_state).none(recording_state)) {
        std::lock_guard<std::mutex> guard(m_storage_mutex);
        m_storage.clear();
    }
}

void SequenceWriter::stop_recording() {
    deactivate_state(recording_state);
}
