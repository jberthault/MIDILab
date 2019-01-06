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

#ifndef CORE_SEQUENCE_H
#define CORE_SEQUENCE_H

#include <iostream>
#include <chrono>     // std::chrono::duration
#include <vector>     // std::vector
#include <set>        // std::set
#include "event.h"    // Event
#include "tools/containers.h"

/**
 * ppqn is a midi specific value standing for pulse per quarter note
 * it is the main interface to real timing conversion
 *
 * @todo merge with sequence
 *
 */

using ppqn_t = uint16_t;

constexpr ppqn_t default_ppqn {192};

/**
 * @brief timestamp_t is a type abstracting a quantity
 * representing the time markers of events.
 * double precision number is assumed to get subprecision
 * and use nan or infinity (unused for now)
 *
 */

using timestamp_t = double;

//==================
// StandardMidiFile
//==================

struct StandardMidiFile {

    using value_type = std::pair<uint32_t, Event>;
    using track_type = std::vector<value_type>;
    using container_type = std::vector<track_type>;

    using format_type = uint16_t;
    static const format_type single_track_format = 0;
    static const format_type simultaneous_format = 1;
    static const format_type sequencing_format = 2;

    format_type format {simultaneous_format};
    ppqn_t ppqn {default_ppqn};
    container_type tracks;

};

//=========
// dumping
//=========

/**
 *
 * This module is used to manage midi persistence
 * @todo on fail, throw additional info such as track corrupted & timestamp
 *
 */

namespace dumping {

Event read_event(byte_cview& buf, bool is_realtime, byte_t* running_status = nullptr);

StandardMidiFile read_file(const std::string& filename); /*!< return an empty file on error */
size_t write_file(const StandardMidiFile& file, const std::string& filename, bool use_running_status = true); /*!< return 0 on error */

}

//============
// TimedEvent
//============

struct TimedEvent {

    TimedEvent() noexcept = default;

    template<typename EventT>
    TimedEvent(timestamp_t timestamp, EventT&& event) : timestamp{timestamp}, event{std::forward<EventT>(event)} {}

    inline friend bool operator<(const TimedEvent& lhs, const TimedEvent& rhs) { return lhs.timestamp < rhs.timestamp; }
    inline friend bool operator<(timestamp_t lhs, const TimedEvent& rhs) { return lhs < rhs.timestamp; }
    inline friend bool operator<(const TimedEvent& lhs, timestamp_t rhs) { return lhs.timestamp < rhs; }

    timestamp_t timestamp;
    Event event;

};


//=======
// Clock
//=======

/**
 *
 * The clock is the main object responsible for dealing with time.
 * It is able to convert timestamp from/to real durations
 *
 */

class Clock {

public:

    using duration_type = std::chrono::duration<double, std::micro>;
    using clock_type = std::conditional<std::chrono::high_resolution_clock::is_steady,
                                        std::chrono::high_resolution_clock,
                                        std::chrono::steady_clock>::type;
    using time_type = clock_type::time_point;

    static_assert(clock_type::is_steady, "System does not provide a steady clock");

    struct TempoItem {
        timestamp_t timestamp;
        duration_type duration;
        Event event;
    };

    Clock(ppqn_t ppqn = default_ppqn);

    ppqn_t ppqn() const;

    duration_type base_time(const Event& tempo_event) const; /*! time corresponding to 1 timestamp at the given tempo */

    const Event& last_tempo(timestamp_t timestamp) const; /*!< returns the last tempo before or upon timestamp */
    const Event& last_time_signature(timestamp_t timestamp) const; /*!< returns the last time signature before or upon timestamp */

    // --------------
    // cache mutators
    // --------------

    void reset();

    void push_timestamp(const Event& event, timestamp_t timestamp);
    void push_duration(const Event& event, const duration_type& duration);

    // ----------------
    // time conversions
    // ----------------

    duration_type timestamp2time(timestamp_t timestamp) const;
    timestamp_t time2timestamp(const duration_type& time) const;
    timestamp_t beat2timestamp(double beat) const;
    double timestamp2beat(timestamp_t timestamp) const;

private:
    duration_type get_duration(const TempoItem& item, timestamp_t timestamp) const;
    timestamp_t get_timestamp(const TempoItem& item, const duration_type& duration) const;

private:
    ppqn_t m_ppqn; /*!< number of pulses per quarter note */
    std::vector<TempoItem> m_tempo;
    std::vector<TimedEvent> m_time_signature;

};

//==========
// Sequence
//==========

/**
 * The main differences with a standard midifile are:
 * @li timestamp has a floating point precision (better while iterating)
 * @li events are stored with their absolute timestamp
 * @li tracks are merged in a single container in a more flexible way
 *
 * This version uses a sorted vector as the internal implementation
 * It offers more flexibility on reading access (random access iterators)
 * compared to a multimap implementation for example
 *
 */

class Sequence {

public:

    struct RealtimeItem {
        Clock::time_type timepoint;
        Event event;
    };

    using realtime_type = std::vector<RealtimeItem>;
    using container_type = std::vector<TimedEvent>;
    using value_type = typename container_type::value_type;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using const_reverse_iterator = typename container_type::const_reverse_iterator;
    using blacklist_type = blacklist_t<track_t>;

    // builders
    static Sequence from_file(StandardMidiFile data);
    static Sequence from_realtime(const realtime_type& data, ppqn_t ppqn = default_ppqn);

    explicit Sequence(ppqn_t ppqn = default_ppqn);

    // clock
    const Clock& clock() const;
    void update_clock();

    // observers
    const container_type& events() const; /*!< event accessor read only */
    bool empty() const;
    std::set<track_t> tracks() const; /*!< compute the tracks assigned to events */

    timestamp_t first_timestamp() const; /*!< timestamp of the first event (expected to be 0) */
    timestamp_t last_timestamp() const; /*!< maximum event's timestamp in all the tracks */
    timestamp_t last_timestamp(track_t track) const; /*!< maximum event's timestamp in the given track */

    // mutators
    void clear();
    void push_item(TimedEvent item); /*!< invalidate clock */
    void insert_item(TimedEvent item); /*!< invalidate clock */

    // converters
    StandardMidiFile to_file(const blacklist_type& list = blacklist_type{true}) const; /*!< convert given tracks to midi file */

    // iterators
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;

private:
    std::vector<TimedEvent> m_events; /*!< event container */
    Clock m_clock;

};

#endif // CORE_SEQUENCE_H
