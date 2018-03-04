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

Event ChannelMapper::remap_event(channels_t channels, channels_t mapped_channels) {
    return Event::custom(channels, "ChannelMapping.remap", marshall(mapped_channels));
}

Event ChannelMapper::unmap_event(channels_t channels) {
    return Event::custom(channels, "ChannelMapping.unmap");
}

ChannelMapper::ChannelMapper() : Handler(Mode::thru()) {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        m_mapping[c] = channels_t::wrap(c);
}

channel_map_t<channels_t> ChannelMapper::mapping() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_mapping;
}

void ChannelMapper::set_mapping(channel_map_t<channels_t> mapping) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_mapping = std::move(mapping);
    m_corruption.tick();
}

void ChannelMapper::reset_mapping(channels_t channels) {
    std::lock_guard<std::mutex> guard(m_mutex);
    for (channel_t channel : channels)
        m_mapping[channel] = channels_t::wrap(channel);
    m_corruption.tick();
}

Handler::Result ChannelMapper::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;

    std::lock_guard<std::mutex> guard(m_mutex);

    if (message.event.family() == family_t::custom) {
        auto k = message.event.get_custom_key();
        if (k == "ChannelMapping.remap") {
            auto mapped_channels = unmarshall<channels_t>(message.event.get_custom_value());
            channel_ns::store(m_mapping, message.event.channels(), mapped_channels);
            m_corruption.tick();
            return Result::success;
        } else if (k == "ChannelMapping.unmap") {
            for (channel_t channel : message.event.channels())
                m_mapping[channel] = channels_t::wrap(channel);
            m_corruption.tick();
            return Result::success;
        }
    }

    /// clean if another note comes in
    if (message.event.is(families_t::note()))
        clean_corrupted(message.source, message.track);

    Message copy(message);

     /// @todo get all relevant families and don't forward if no channels
    if (message.event.is(families_t::voice()))
        copy.event.set_channels(remap(message.event.channels()));

    feed_forward(copy);
    return Result::success;
}

void ChannelMapper::feed_forward(const Message& message) {
    m_corruption.feed(message.event);
    forward_message(message);
}

void ChannelMapper::clean_corrupted(Handler* source, track_t track) {
    channels_t channels = m_corruption.reset();
    if (channels)
        feed_forward({Event::controller(channels, controller_ns::all_notes_off_controller), source, track});
}

channels_t ChannelMapper::remap(channels_t channels) const {
    channels_t result;
    for (channel_t old_channel : channels)
        result |= m_mapping[old_channel];
    return result;
}
