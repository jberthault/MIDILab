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

#ifndef HANDLERS_CHANNELMAPPER_H
#define HANDLERS_CHANNELMAPPER_H

#include "core/handler.h"
#include "core/misc.h"

//===============
// ChannelMapper
//===============

class ChannelMapper : public Handler {

public:
    static const VoiceExtension<channels_t> remap_ext; /*!< channels given will all map to the whole new channels */
    static const VoiceExtension<> unmap_ext; /*!< all channels specified won't be mapped */

    explicit ChannelMapper();

    channel_map_t<channels_t> mapping() const;
    void set_mapping(channel_map_t<channels_t> mapping);
    void reset_mapping(channels_t channels = channels_t::full());

    Result handle_message(const Message& message) override;

private:
    void feed_forward(const Message& message);
    void clean_corrupted(Handler* source, track_t track);
    channels_t remap(channels_t channels) const;

    channel_map_t<channels_t> m_mapping; /*!< Channel -> [Channel] */
    Corruption m_corruption;
    mutable std::mutex m_mutex; /*!< mutex protecting mapping */

};

#endif // HANDLERS_CHANNELMAPPER_H
