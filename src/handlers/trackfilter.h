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

#ifndef HANDLERS_TRACKFILTER_H
#define HANDLERS_TRACKFILTER_H

#include "core/handler.h"
#include "core/misc.h"

//=============
// TrackFilter
//=============

/**
 * @brief The TrackFilter class
 *
 * @todo find a way to delete this handler and embed it
 * directly in the SequenceReader.
 * It will save a connection but mainly it will avoid keeping the track in the message
 *
 */

class TrackFilter : public Handler {

public:
    using filter_type = blacklist_t<track_t>;

    static const SystemExtension<void> enable_all_ext;
    static const SystemExtension<track_t> enable_ext;
    static const SystemExtension<track_t> disable_ext;

    explicit TrackFilter();

protected:
    Result handle_message(const Message& message) override;

private:
    void feed_forward(const Message& message); /*!< forward a message after feeding memory */
    void clean_corrupted(Handler* source, track_t track); /*!< forward a message cleaning corrupted channels */

    filter_type m_filter {true};
    std::unordered_map<track_t, Corruption> m_corruption;

};

#endif // HANDLERS_TRACKFILTER_H
