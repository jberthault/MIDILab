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

#ifndef CORE_MISC_H
#define CORE_MISC_H

#include "event.h"
#include "handler.h"

//==========
// misc ...
//==========

bool asMode(Handler* handler, Handler::mode_type mode);

bool asTail(Handler* handler);

bool asHead(Handler* handler);

//============
// NoteMemory
//============

/**
 * Helper class that keeps track of every note set on in order
 * to let user known if any channel must be reset
 *
 * @todo merge into Corruption and rename
 */

class NoteMemory {

public:
    NoteMemory();

    void feed(const Event& event);
    void set_on(channels_t channels, byte_t note); /*!< activate a note on the given channels */
    void set_off(channels_t channels, byte_t note); /*!< clear activated channels for the given note */
    void clear(channels_t channels); /*!< clear all notes on the given channels */
    void clear(); /*!< clear all notes on all channels */

    channels_t active() const; /*!< return channels that have any note-on running */
    channels_t active(byte_t note) const; /*!< return channels for which the note is on */

private:
    std::array<channels_t, 0x80> m_data; /*!< map notes (index) to channels for which note is on */

};

//============
// Corruption
//============

class Corruption {

public:
    void feed(const Event& event);
    void tick(); /*!< mark active channels as corrupted */
    void tick(channels_t channels); /*!< mark active channels as corrupted if they appear in the given mask */
    channels_t reset(); /*!< mark all channels as not-corrupted returning the old corrupted channels */

private:
    NoteMemory m_memory; /*!< active notes by channel */
    channels_t m_corrupted;

};

#endif // CORE_MISC_H
