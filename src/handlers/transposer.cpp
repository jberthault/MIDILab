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

#include "transposer.h"

namespace {

constexpr auto bypass_state = Handler::State::from_integral(0x4);

}

//============
// Transposer
//============

const VoiceExtension<int> Transposer::transpose_ext {"Transpose"};

Transposer::Transposer() : Handler{Mode::thru()} {
    m_keys.fill(0);
    activate_state(bypass_state);
}

Handler::Result Transposer::handle_message(const Message& message) {
    if (message.event.family() == family_t::extended_voice) {
        if (transpose_ext.affects(message.event)) {
            set_key(message.event.channels(), transpose_ext.decode(message.event));
            return Result::success;
        }
    } else if (message.event.is(families_t::standard_note())) {
        clean_corrupted(message.source, message.track);
        if (state().none(bypass_state)) {
            for (const auto& pair : channel_ns::reverse(m_keys, message.event.channels())) {
                Message copy(message);
                Event::iterator note_it = ++copy.event.begin(); // second byte contains the note
                *note_it = to_data_byte(*note_it + pair.first); // shift note
                copy.event.set_channels(pair.second);
                feed_forward(copy);
            }
            return Result::success;
        }
    }
    feed_forward(message); // to feed controller events
    return Result::success;
}

void Transposer::feed_forward(const Message& message) {
    m_corruption.feed(message.event);
    forward_message(message);
}

void Transposer::clean_corrupted(Handler* source, track_t track) {
    if (const auto channels = m_corruption.reset())
        feed_forward({Event::controller(channels, controller_ns::all_notes_off_controller), source, track});
}

void Transposer::set_key(channels_t channels, int key) {
    channels_t channels_changed;
    // registered key for each channel
    for (channel_t channel : channels) {
        if (m_keys[channel] != key) {
            m_keys[channel] = key;
            channels_changed.set(channel);
        }
    }
    // bypass handler if no keys is shifted
    if (channel_ns::find(m_keys, 0) == channels_t::full())
        activate_state(bypass_state);
    else
        deactivate_state(bypass_state);
    // mark channels as corrupted
    m_corruption.tick(channels_changed);

    /// @todo call clean_corrupted here instead of waiting for a note event
    /// forward with the transposer as se source
    /// but it means listeners may accept transposer as a source
    /// or we could generate using the last source
}
