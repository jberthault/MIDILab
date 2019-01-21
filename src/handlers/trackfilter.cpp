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

#include "trackfilter.h"

//=============
// TrackFilter
//=============

const SystemExtension<void> TrackFilter::enable_all_ext {"TrackFilter.enable_all"};
const SystemExtension<track_t> TrackFilter::enable_ext {"TrackFilter.enable"};
const SystemExtension<track_t> TrackFilter::disable_ext {"TrackFilter.disable"};

TrackFilter::TrackFilter() : Handler{Mode::thru()} {

}

Handler::Result TrackFilter::handle_message(const Message& message) {
    if (message.event.is(family_t::extended_system)) {
        if (disable_ext.affects(message.event)) {
            const auto track = disable_ext.decode(message.event);
            m_corruption[track].tick();
            m_filter.elements.insert(track);
            return Result::success;
        } else if (enable_ext.affects(message.event)) {
            m_filter.elements.erase(enable_ext.decode(message.event));
            return Result::success;
        } else if (enable_all_ext.affects(message.event)) {
            m_filter.elements.clear();
            return Result::success;
        }
    }
    clean_corrupted(message.source, message.event.track());
    if (m_filter.match(message.event.track()))
        feed_forward(message);
    return Result::success;
}

void TrackFilter::feed_forward(const Message& message) {
    m_corruption[message.event.track()].feed(message.event);
    forward_message(message);
}

void TrackFilter::clean_corrupted(Handler *source, track_t track) {
    if (const auto channels = m_corruption[track].reset())
        feed_forward({Event::controller(channels, controller_ns::all_notes_off_controller).with_track(track), source});
}
