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

#include "channelmapper.h"

//===============
// ChannelMapper
//===============

const VoiceExtension<channels_t> ChannelMapper::remap_ext {"ChannelMapping.remap"};
const VoiceExtension<> ChannelMapper::unmap_ext {"ChannelMapping.unmap"};

ChannelMapper::ChannelMapper() : Handler{Mode::thru()} {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        m_mapping[c] = channels_t::wrap(c);
}

channel_map_t<channels_t> ChannelMapper::mapping() const {
    std::lock_guard<std::mutex> guard{m_mutex};
    return m_mapping;
}

void ChannelMapper::set_mapping(const channel_map_t<channels_t>& mapping) {
    std::lock_guard<std::mutex> guard{m_mutex};
    m_mapping = mapping;
    m_corruption.tick();
}

void ChannelMapper::reset_mapping(channels_t channels) {
    std::lock_guard<std::mutex> guard{m_mutex};
    for (channel_t channel : channels)
        m_mapping[channel] = channels_t::wrap(channel);
    m_corruption.tick();
}

Handler::Result ChannelMapper::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;

    std::lock_guard<std::mutex> guard{m_mutex};

    if (message.event.family() == family_t::extended_voice) {
        if (remap_ext.affects(message.event)) {
            channel_ns::store(m_mapping, message.event.channels(), remap_ext.decode(message.event));
            m_corruption.tick();
            return Result::success;
        } else if (unmap_ext.affects(message.event)) {
            for (channel_t channel : message.event.channels())
                m_mapping[channel] = channels_t::wrap(channel);
            m_corruption.tick();
            return Result::success;
        }
    }

    /// clean if another note comes in
    if (message.event.is(families_t::standard_note()))
        clean_corrupted(message.source, message.track);

    if (message.event.is(families_t::voice())) {
        if (const auto channels = remap(message.event.channels())) {
            Message copy{message};
            copy.event.set_channels(channels);
            feed_forward(copy);
        }
    } else {
        forward_message(message);
    }

    return Result::success;
}

void ChannelMapper::feed_forward(const Message& message) {
    m_corruption.feed(message.event);
    forward_message(message);
}

void ChannelMapper::clean_corrupted(Handler* source, track_t track) {
    if (const auto channels = m_corruption.reset())
        feed_forward({Event::controller(channels, controller_ns::all_notes_off_controller), source, track});
}

channels_t ChannelMapper::remap(channels_t channels) const {
    channels_t result;
    for (channel_t old_channel : channels)
        result |= m_mapping[old_channel];
    return result;
}
