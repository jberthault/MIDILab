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

#ifndef HANDLERS_TRANSPOSER_H
#define HANDLERS_TRANSPOSER_H

#include "core/handler.h"
#include "core/misc.h"

//============
// Transposer
//============

/**
 *
 * Concerning note bound to more than one channel, if transposition key
 * differs, note will be duplicated for each different key
 *
 * To Change a key : send a handler_event with key "Transpose" and an int value
 * representing the offset to apply to each note. the key will be set for each channel
 * specified.
 *
 */

class Transposer : public Handler {

public:
    static const VoiceExtension<int> transpose_ext;

    explicit Transposer();

protected:
    Result handle_message(const Message& message) override;

private:
    void clean_corrupted(Handler* source, track_t track); /*!< forward a message cleaning corrupted channels */
    void set_key(channels_t channels, int key); /*!< set the transposition key for the given channels */

    channel_map_t<int> m_keys; /*!< number of semi-tones shifted by channel */
    Corruption m_corruption;

};

#endif // HANDLERS_TRANSPOSER_H
