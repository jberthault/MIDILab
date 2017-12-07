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

#ifndef CORE_SEQUENCE_H
#define CORE_SEQUENCE_H

#include <iostream>
#include <chrono>     // std::chrono::duration
#include <map>        // std::map
#include <vector>     // std::vector
#include <list>       // std::list
#include <set>        // std::set
#include "event.h"    // Event
#include "tools/containers.h"

using track_t = size_t;

/**
 * ppqn is a midi specific value standing for pulse per quarter note
 * it is the main interface to real timing conversion
 *
 * @todo merge with sequence
 *
 */

using ppqn_t = uint16_t;

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

class StandardMidiFile {

public:

    using value_type = std::pair<uint32_t, Event>;
    using track_type = std::list<value_type>;
    using container_type = std::vector<track_type>;

    using format_type = uint16_t;
    static const format_type single_track_format = 0;
    static const format_type simultaneous_format = 1;
    static const format_type sequencing_format = 2;

    StandardMidiFile();

    format_type format; /*!< default simultaneous_format */
    ppqn_t ppqn; /*!< default is 192 */
    container_type tracks; /*!< default is empty */

};

//============
// variable_t
//============

/**
  A variable value can be holden by a 32 bits value but is encoded differently.
  It belongs to the range [0, 0x10000000[
  */

class variable_t {

public:

    bool set_true_value(uint32_t value); /*!< will return false if value is not in the accepted range */
    bool set_encoded_value(uint32_t value); /*!< first byte of encoded value is considered to be the LSB (e.g. 0x00 0x00 0x68 0x89) */

    uint32_t encoded_value;
    uint32_t true_value;

};

/**
 *
 * This module is used to manage midi persistence
 * @todo on fail, throw additional info such as track corrupted & timestamp
 *
 */

namespace dumping {

Event read_event(std::istream& stream, bool is_realtime, byte_t* running_status = nullptr);

StandardMidiFile read_file(std::istream& stream);
StandardMidiFile read_file(const std::string& filename); /*!< return an empty file on error */

size_t write_file(const StandardMidiFile& file, std::ostream& stream, bool use_running_status);
size_t write_file(const StandardMidiFile& file, const std::string& filename, bool use_running_status); /*!< return 0 on error */

}

//=======
// Clock
//=======

/**
 *
 * The clock is the main object responsible for dealing with time.
 * It is able to convert timestamp from/to real durations
 *
 * @todo INCLUDE SMTPE & TIME SIGNATURE FOR CONVERSIONS !
 */

class Clock {

public:

    using duration_type = std::chrono::duration<double, std::micro>;
    using clock_type = std::conditional<std::chrono::high_resolution_clock::is_steady,
                                        std::chrono::high_resolution_clock,
                                        std::chrono::steady_clock>::type;
    using time_type = clock_type::time_point;

    static_assert(clock_type::is_steady, "System does not provide a steady clock");

    struct Item {
        Event tempo_event;
        timestamp_t timestamp;
        duration_type duration;
    };

    Clock(ppqn_t ppqn = 192);

    ppqn_t ppqn() const;

    duration_type base_time(const Event& tempo_event) const; /*! time corresponding to 1 timestamp at the given tempo */

    Event last_tempo(timestamp_t timestamp) const; /*!< returns the last tempo before or upon timestamp */

    const Item& before_timestamp(timestamp_t timestamp) const;
    const Item& before_duration(const duration_type& duration) const;

    duration_type get_duration(const Item& item, timestamp_t timestamp) const;
    timestamp_t get_timestamp(const Item& item, const duration_type& duration) const;

    // --------------
    // cache mutators
    // --------------

    void reset();
    void reset(ppqn_t ppqn);

    void push_timestamp(const Event& tempo_event, timestamp_t timestamp);
    void push_duration(const Event& tempo_event, const duration_type& duration);

    // ----------------
    // time conversions
    // ----------------

    duration_type timestamp2time(timestamp_t timestamp) const;
    timestamp_t time2timestamp(const duration_type& time) const;
    timestamp_t beat2timestamp(double beat) const;
    double timestamp2beat(timestamp_t timestamp) const;

private:
    ppqn_t m_ppqn; /*!< number of pulses per quarter note */
    std::vector<Item> m_cache;

};

//==========
// Sequence
//==========

/**
 * The main differences with a standard midifile are:
 * @li timestamp has a floating point precision (better while iterating)
 * @li events are stored with their absolute timestamp
 * @li tracks are merged in a single container in a more flexible way
 * @li additional title can be set
 *
 * This version uses a sorted vector as the internal implementation
 * It offers more flexibility on reading access (random access iterators)
 * compared to a multimap implementation for example
 *
 */

class Sequence {

public:

    struct Item {
        Event event;
        track_t track;
        timestamp_t timestamp;
    };

    inline friend bool operator <(const Item& lhs, const Item& rhs) { return lhs.timestamp < rhs.timestamp; }
    inline friend bool operator <(timestamp_t lhs, const Item& rhs) { return lhs < rhs.timestamp; }
    inline friend bool operator <(const Item& lhs, timestamp_t rhs) { return lhs.timestamp < rhs; }

    struct RealtimeItem {
        Event event;
        track_t track;
        Clock::time_type time;
    };


    using blacklist_type = blacklist_t<track_t>;

    using realtime_type = std::vector<RealtimeItem>;
    using container_type = std::vector<Item>;
    using value_type = typename container_type::value_type; // Item
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using const_reverse_iterator = typename container_type::const_reverse_iterator;

    // builders
    static Sequence from_file(const StandardMidiFile& data);
    static Sequence from_realtime(const realtime_type& data, ppqn_t ppqn = 192);

    // clock
    const Clock& clock() const;
    void update_clock();
    void update_clock(ppqn_t ppqn);

    // observers
    const container_type& events() const; /*!< event accessor read only */
    bool empty() const;
    const std::set<track_t> tracks() const; /*!< compute the tracks assigned to events */

    timestamp_t first_timestamp() const; /*!< timestamp of the first event (expected to be 0) */
    timestamp_t last_timestamp() const; /*!< maximum event's timestamp in all the tracks */
    timestamp_t last_timestamp(track_t track) const; /*!< maximum event's timestamp in the given track */

    // mutators
    void clear();
    void push_item(Item item); /*!< invalidate clock */
    void insert_item(Item item); /*!< invalidate clock */
    void insert_track(const StandardMidiFile::track_type& track_data, track_t track, int64_t offset = 0); /*!< invalidate clock */

    // converters
    StandardMidiFile to_file(const blacklist_type& list = blacklist_type(true)) const; /*!< convert given tracks to midi file */

    // iterators
    const_iterator begin() const;
    const_iterator end() const;

    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;

private:
    std::string m_title; /*!< simple string that helps identifying the sequence */
    std::vector<Item> m_events; /*!< event container */
    Clock m_clock;

};

#endif // CORE_SEQUENCE_H
