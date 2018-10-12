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

#ifndef HANDLERS_SEQUENCE_WRITER_H
#define HANDLERS_SEQUENCE_WRITER_H

#include <mutex>      // std::mutex
#include "core/handler.h"
#include "core/sequence.h"

//================
// SequenceWriter
//================

class SequenceWriter : public Handler {

public:
    using duration_type = Clock::duration_type;
    using clock_type = Clock::clock_type;
    using time_type = Clock::time_type;

    explicit SequenceWriter();

    void set_families(families_t families); /*!< default is all voice events */

    Sequence load_sequence() const;

    void start_recording(); /*!< first event received will be mark as t0, no effect if handler is recording */
    void stop_recording();

protected:
    Result handle_message(const Message& message) override;

private:
    families_t m_families {families_t::standard_voice() | families_t::standard_meta()}; /*!< accepted families */
    Sequence::realtime_type m_storage;
    mutable std::mutex m_storage_mutex;

};

#endif // HANDLERS_SEQUENCE_WRITER_H
