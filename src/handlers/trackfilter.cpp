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

#include "trackfilter.h"

//=============
// TrackFilter
//=============

Event TrackFilter::enable_all_event() {
    return Event::custom({}, "TrackFilter.enable_all");
}

Event TrackFilter::enable_event(track_t track) {
    return Event::custom({}, "TrackFilter.enable", marshall(track));
}

Event TrackFilter::disable_event(track_t track) {
    return Event::custom({}, "TrackFilter.disable", marshall(track));
}

TrackFilter::TrackFilter() : Handler(Mode::thru()), m_filter(true) {

}

Handler::Result TrackFilter::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;

    if (message.event.family() == family_t::custom) {
        std::string k = message.event.get_custom_key();
        if (k == "TrackFilter.disable") {
            auto track = unmarshall<track_t>(message.event.get_custom_value());
            m_corruption[track].tick();
            m_filter.elements.insert(track);
            return Result::success;
        } else if (k == "TrackFilter.enable") {
            m_filter.elements.erase(unmarshall<track_t>(message.event.get_custom_value()));
            return Result::success;
        } else if (k == "TrackFilter.enable_all") {
            m_filter.elements.clear();
            return Result::success;
        }
    }

    clean_corrupted(message.source, message.track);
    if (m_filter.match(message.track))
        feed_forward(message);
    return Result::success;
}

void TrackFilter::feed_forward(const Message& message) {
    m_corruption[message.track].feed(message.event);
    forward_message(message);
}

void TrackFilter::clean_corrupted(Handler *source, track_t track) {
    channels_t channels = m_corruption[track].reset();
    if (channels)
        return feed_forward({Event::controller(channels, controller_ns::all_notes_off_controller), source, track});
}
