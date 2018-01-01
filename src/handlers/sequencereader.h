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

#ifndef HANDLERS_SEQUENCE_READER_H
#define HANDLERS_SEQUENCE_READER_H

#include <future>     // std::future std::promise
#include "core/handler.h"
#include "core/sequence.h"

//================
// SequenceReader
//================

class SequenceReader : public Handler {

public:
    using duration_type = Clock::duration_type;
    using clock_type = Clock::clock_type;
    using time_type = Clock::time_type;

    using position_type = std::pair<Sequence::const_iterator, timestamp_t>;

    static Event toggle_event(); /*!< pause handler if playing else start */
    static Event pause_event(); /*!< like stop_event but don't generate a reset_event */
    static Event distorsion_event(double distorsion);

    explicit SequenceReader();
    ~SequenceReader();

    const Sequence& sequence() const; /*!< returns current sequence */
    void set_sequence(const Sequence& sequence); /*!< set sequence to play */

    const std::map<byte_t, Sequence>& sequences() const; /*!< all loaded sequences */
    void load_sequence(byte_t id, const Sequence& sequence); /*!< set sequence available, for song_select events */
    bool select_sequence(byte_t id); /*!< set the current sequence by its id, return False if the id is unknown */

    double distorsion() const;
    void set_distorsion(double distorsion); /*!< negative values are silently ignored */

    bool is_playing() const;
    bool is_completed() const; /*!< returns true if current position has reached the last one */

    timestamp_t position() const; /*!< current timestamp of the current sequence */
    void set_position(timestamp_t timestamp);

    timestamp_t lower() const;
    void set_lower(timestamp_t timestamp);

    timestamp_t upper() const;
    void set_upper(timestamp_t timestamp);

    bool start_playing(bool rewind); /*!< return false if already started */
    bool stop_playing(const Event& final_event); /*!< return false if already stopped */
    bool stop_playing(); /*!< return false if already stopped */

    families_t handled_families() const override;
    result_type handle_message(const Message& message) override;
    result_type on_close(state_type state) override;

private:
    // handle callbacks
    result_type handle_beat(double beat);
    result_type handle_sequence(byte_t id);
    result_type handle_start(bool rewind);
    result_type handle_stop(const Event& final_event);
    result_type handle_distorsion(const std::string& distorsion);

    // event loop running through the sequence
    void run();

    // unsafe helpers
    void init_positions();
    void jump_position(position_type position);
    position_type make_lower(timestamp_t timestamp) const;
    position_type make_upper(timestamp_t timestamp) const;

    std::map<byte_t, Sequence> m_sequences; /*!< all loaded sequences */
    Sequence m_sequence; /*!< current sequence */
    position_type m_position; /*!< current position */
    position_type m_first_position; /*!< position of the first event to be played */
    position_type m_last_position; /*!< position of the last event to be played (not included) */
    double m_distorsion; /*!< distorsion factor: slower (<1) faster (>1) freezed (0) (default 1) */

    task_t<std::promise<void>> m_task; /*!< task based implementation to avoid creating a new thread each time we stop */
    std::future<void> m_status; /*!< status of the current run */
    bool m_playing; /*!< boolean controlling play/stop */
    mutable std::mutex m_mutex;  /*!< mutex protecting positions & distorsion */

};

#endif // HANDLERS_SEQUENCE_READER_H
