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

#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "sequence.h"
#include "tools/trace.h"

using namespace family_ns;

//=========
// details
//=========

namespace {

size_t copy_sequence(std::istream& from, std::ostream& to, size_t count) {
    for (size_t i=0 ; i < count ; i++)
        to.put(from.get());
    return count;
}

void find_pattern(const char* pattern, std::istream& stream) {
    // void same as read_pattern except that it accepts noise before finding
    // the pattern
    const char* p = pattern;
    while (*p != 0) {
        if (*p == stream.get()) { // advance if match
            ++p;
        } else if (p != pattern) { // reset if unmatch
            p = pattern;
            stream.unget();
        }
    }
}

}

//==================
// StandardMidiFile
//==================

StandardMidiFile::StandardMidiFile() :
    format(simultaneous_format), ppqn(192) {

}

//============
// variable_t
//============

bool variable_t::set_true_value(uint32_t value) {
    true_value = value;
    encoded_value = to_data_byte(value);
    while (value >>= 7)
        encoded_value = (encoded_value << 8) | 0x80 | to_data_byte(value);
    return is_msb_cleared(encoded_value >> 24);
}

bool variable_t::set_encoded_value(uint32_t value) {
    encoded_value = value;
    true_value = to_data_byte(value);
    while (is_msb_set(value)) {
        value >>= 8;
        true_value = (true_value << 7) | to_data_byte(value);
    }
    return is_msb_cleared(value);
}

namespace dumping {

byte_t read_data_byte(std::istream& stream) {
    byte_t byte = stream.get();
    if (is_msb_set(byte)) {
        TRACE_WARNING("expected byte with bit #7 cleared before " << stream.tellg());
        return 0x00;
    }
    return byte;
}

uint16_t read_uint16(std::istream& stream) {
    return byte_traits<uint16_t>::read_little_endian(stream, 2);
}

size_t write_uint16(uint16_t value, std::ostream& stream) {
    byte_traits<uint16_t>::write_little_endian(value, stream, 2);
    return 2;
}

uint32_t read_uint32(std::istream& stream) {
    return byte_traits<uint32_t>::read_little_endian(stream, 4);
}

size_t write_uint32(uint32_t value, std::ostream& stream) {
    byte_traits<uint32_t>::write_little_endian(value, stream, 4);
    return 4;
}

variable_t read_variable(std::istream& stream) {
    variable_t result;
    uint32_t buffer = 0;
    size_t count = 0;
    byte_t byte = 0xff; // initial value unused
    while (is_msb_set(byte) && count < 4) {
        byte = stream.get();
        buffer |= byte << (8*count);
        ++count;
    }
    if (!result.set_encoded_value(buffer))
        throw std::logic_error("can't read variable length data");
    return result;
}

size_t write_variable(const variable_t& variable, std::ostream& stream) {
    size_t bytes = 0;
    uint32_t buffer = variable.encoded_value;
    while (true) {
        bytes++;
        byte_t byte = to_byte(buffer);
        stream << byte;
        if (is_msb_set(byte)) {
            buffer >>= 8;
        } else
            break;
    }
    return bytes;
}

Event read_event(std::istream& stream, bool is_realtime, byte_t* running_status) {

    // read status byte
    byte_t status = stream.get();
    if (is_msb_set(status)) {
        if (running_status != nullptr)
            *running_status = status;
    } else {
        if (running_status != nullptr)
            status = *running_status;
        if (is_msb_cleared(status))
            throw std::logic_error("unknown event status");
        stream.unget();
    }

    std::stringstream bytes;
    bytes << status; // add status to data

    if (status == 0xf0) {
        size_t i=0;
        variable_t v = read_variable(stream);
        write_variable(v, bytes);
        while (true) {
            byte_t byte = stream.get();
            bytes << byte;
            if (byte == 0xf7 || ++i == v.true_value)
                break;
            if (is_msb_set(byte))
                throw std::logic_error("wrong data byte in sysex event");
        }
    } else if (status == 0xff) { // META EVENT (type as data_byte) (size as variable) (fixed size)
        bytes << read_data_byte(stream); // read meta type
        variable_t size = read_variable(stream); // meta size
        write_variable(size, bytes);
        copy_sequence(stream, bytes, size.true_value);
    } else if (0x80 <= status && status < 0xf0) { // VOICE EVENT (1 or 2 data bytes)
        bytes << read_data_byte(stream);
        if (!(0xc0 <= status && status < 0xe0))
            bytes << read_data_byte(stream);
    } else if (status == 0xf1 || status == 0xf3) {
        bytes << read_data_byte(stream); // read single byte
    } else if (status == 0xf2) {
        bytes << read_data_byte(stream); // read 2 bytes
        bytes << read_data_byte(stream);
    }

    std::string data = bytes.str();
    return Event::raw(is_realtime, {data.begin(), data.end()});
}

void read_track(std::istream& stream, StandardMidiFile::track_type& track) {
    find_pattern("MTrk", stream); // throw if header is not found
    uint32_t size = read_uint32(stream); // number of bytes in the chunk
    std::streampos end = stream.tellg() + (std::streampos)size; // position of end of track
    byte_t running_status = 0; // initialize running status
    uint32_t deltatime; // temporary deltatime
    uint64_t timestamp = 0; // cumulated deltatimes
    Event event; // temporary event
    bool eot = false; // true if end-of-track event is found
    while (!eot && stream.tellg() < end) {
        // read deltatime and add it to timestamp
        deltatime = read_variable(stream).true_value;
        timestamp += deltatime;
        // read event and check its validity
        event = read_event(stream, false, &running_status);
        if (!event)
            throw std::logic_error("none event found");
        eot = event.family() == family_t::end_of_track;
        if (deltatime == 0 && !track.empty() && Event::equivalent(track.back().second, event)) {
            // check if current event can be merged with the last one
            // it aims to remove duplicated events while combining voice events on different channels
            track.back().second.set_channels(track.back().second.channels() | event.channels());
        } else {
            // add event in the track, ignores deltatime for EOT when a particular timestamp is reached
            // it may be an annoying feature/bug of some editor
            track.emplace_back(eot && timestamp == 0x03ffff ? 0 : deltatime, event);
        }
    }
    if (!eot || stream.tellg() != end)
        throw std::logic_error("unexpected end of track");
}

StandardMidiFile read_file(std::istream& stream) {
    StandardMidiFile file;
    find_pattern("MThd", stream);
    uint32_t header_size = read_uint32(stream);
    if (header_size != 6)
        throw std::logic_error("unexpected header size");
    file.format = read_uint16(stream);
    if (file.format > 2)
        throw std::logic_error("unexpected midi file format");
    file.tracks.resize(read_uint16(stream));
    file.ppqn = read_uint16(stream); /// @todo check values of ppqn
    for (auto& track : file.tracks) {
        try {
            read_track(stream, track);
        } catch (const std::exception& err) {
            TRACE_ERROR("failed parsing track: " << err.what());
            break;
        }
    }
    return file;
}

StandardMidiFile read_file(const std::string& filename) {
    TRACE_MEASURE("read file");
    try  {
        std::fstream filestream(filename, std::ios_base::in | std::ios_base::binary);
        if (!filestream)
            throw std::logic_error("can't open file");
        filestream.exceptions(std::ios_base::failbit);
        return read_file(filestream);
    } catch (const std::exception& err) {
        TRACE_WARNING(filename << ": " << err.what());
        return StandardMidiFile();
    }
}

size_t write_deltatime(uint32_t delatime, std::ostream& stream) {
    variable_t variable;
    if (!variable.set_true_value(delatime))
        throw std::logic_error("variable error");
    return write_variable(variable, stream);
}

size_t write_raw_event(byte_t status, const Event& event, std::ostream& stream, byte_t* running_status) {
    size_t bytes = 0;
    // update running status
    bool write_status = true;
    if (running_status != nullptr) {
        write_status = status != *running_status;
        *running_status = status;
    }
    // write status
    if (write_status) {
        bytes++; stream << status;
    }
    // get the number of bytes to write
    size_t count = 0;
    if (status == 0xf0 || status == 0xff) {
        count = event.size() - 1; // all bytes
    } else if (0x80 <= status && status < 0xf0) {
        count = (0xc0 <= status && status < 0xe0) ? 1 : 2;
    } else if (status == 0xf1 || status == 0xf3) {
        count = 1;
    } else if (status == 0xf2) {
        count = 2;
    }
    // get event data as a stream (without the status)
    std::string event_string(event.begin() + 1, event.end());
    std::stringstream event_data(event_string);
    bytes += copy_sequence(event_data, stream, count);
    return bytes;
}

size_t write_event(uint32_t deltatime, const Event& event, std::ostream& stream, byte_t* running_status) {
    size_t bytes = 0;
    byte_t status = event.at(0);
    // check status
    if (!is_msb_set(status))
        throw std::invalid_argument("wrong status byte");
    // check event type
    if (!event)
        throw std::invalid_argument("can't write null event");
    if (event.is(~midi_families | system_realtime_families))
        throw std::invalid_argument("can't write custom or realtime events");
    if (event.is(voice_families)) {
        // transform note off to note on
        if (event.family() == family_t::note_off && event.at(2) == 0)
            status = 0x90;
        // check voice event's channel
        if (!event.channels())
            throw std::invalid_argument("voice event is not bound to any channel");
        // insert one event per channel
        for (channel_t channel : event.channels()) {
            bytes += write_deltatime(deltatime, stream);
            bytes += write_raw_event((0xf0 & status) | channel, event, stream, running_status);
            deltatime = 0;
        }
    } else {
        bytes += write_deltatime(deltatime, stream);
        bytes += write_raw_event(status, event, stream, running_status);
    }

    return bytes;
}

size_t write_track(const StandardMidiFile::track_type& track, std::ostream& stream, bool use_running_status) {
    size_t size = 0; // number of bytes in the chunk
    byte_t running_status = 0; // initialize running status
    byte_t* running_status_ptr = use_running_status ? &running_status : nullptr;
    std::stringstream oss;
    // check track integrity
    auto rfirst = track.rbegin();
    if (rfirst == track.rend())
        throw std::invalid_argument("empty track");
    // write data
    for (const StandardMidiFile::value_type& value: track)
        size += write_event(value.first, value.second, oss, running_status_ptr);
    if (rfirst->second.family() != family_t::end_of_track)
        size += write_event(0, Event::end_of_track(), oss, running_status_ptr);
    // prepend a header
    size_t bytes = 0;
    bytes += 4; stream << "MTrk";
    bytes += write_uint32((uint32_t)size, stream);
    bytes += copy_sequence(oss, stream, size);
    return bytes;
}

size_t write_file(const StandardMidiFile& file, std::ostream& stream, bool use_running_status) {
    /// @todo check format & tracks & ppqn
    size_t bytes = 0;
    bytes += 4; stream << "MThd";
    bytes += write_uint32(6, stream);
    bytes += write_uint16(file.format, stream);
    bytes += write_uint16((uint16_t)file.tracks.size(), stream);
    bytes += write_uint16(file.ppqn, stream);
    for (const StandardMidiFile::track_type& track: file.tracks)
        bytes += write_track(track, stream, use_running_status);
    return bytes;
}

size_t write_file(const StandardMidiFile& file, const std::string& filename, bool use_running_status) {
    try  {
        std::fstream filestream(filename, std::ios_base::out | std::ios_base::binary);
        if (!filestream)
            throw std::logic_error("can't open file");
        filestream.exceptions(std::ios_base::failbit);
        return write_file(file, filestream, use_running_status);
    } catch (const std::exception& err) {
        TRACE_WARNING(filename << ": " << err.what());
        return 0;
    }
}

}

/**
 * @brief The const_track_iterator struct
 * iterates and adapt Event to Sequence::Item
 * and relative deltatime to absolute timestamp
 *
 * issues:
 * * successive ++ at end will throw
 *
 */

namespace {

struct const_track_iterator : public std::iterator<std::forward_iterator_tag, Sequence::Item> {

    using inner_type = StandardMidiFile::track_type::const_iterator;

    const_track_iterator(const inner_type& it = {}, track_t track = 0, int64_t offset = 0) : m_it(it), m_track(track), m_offset(offset) {}

    Sequence::Item operator*() const { return {timestamp_t(m_offset + m_it->first), m_it->second, m_track}; }

    const_track_iterator& operator++() { m_offset += m_it->first; ++m_it; return *this;  }
    const_track_iterator operator++(int) { const_track_iterator it(*this); operator++(); return it; }

    bool operator==(const const_track_iterator& rhs) const { return m_it == rhs.m_it; }
    bool operator!=(const const_track_iterator& rhs) const { return m_it != rhs.m_it; }

private:
    inner_type m_it; /*!< track iterator */
    track_t m_track; /*!< track identifier */
    int64_t m_offset; /*!< offset is an integral type so that it avoids double approximations */

};

}

//=======
// Clock
//=======

Clock::Clock(ppqn_t ppqn) : m_ppqn(ppqn) {
    reset();
}

ppqn_t Clock::ppqn() const {
    return m_ppqn;
}

Clock::duration_type Clock::base_time(const Event& tempo_event) const {
    assert(tempo_event.family() == family_t::tempo);
    /// @todo check negative values of first byte of ppqn
    return duration_type{tempo_event.get_meta_int<int64_t>() / (double)m_ppqn};
}

Event Clock::last_tempo(timestamp_t timestamp) const {
    return before_timestamp(timestamp).tempo_event;
}

const Clock::Item& Clock::before_timestamp(timestamp_t timestamp) const {
    auto it = std::upper_bound(m_cache.begin(), m_cache.end(), timestamp, [](timestamp_t t, const Item& item) { return t < item.timestamp; });
    if (it != m_cache.begin()) /// @note very likely (only true for negative timestamp)
        --it;
    return *it;
}

const Clock::Item& Clock::before_duration(const duration_type& duration) const {
    auto it = std::upper_bound(m_cache.begin(), m_cache.end(), duration, [](const duration_type& d, const Item& item) { return d < item.duration; });
    if (it != m_cache.begin()) /// @note very likely (only true for negative duration)
        --it;
    return *it;
}

Clock::duration_type Clock::get_duration(const Item& item, timestamp_t timestamp) const {
    return item.duration + base_time(item.tempo_event) * (timestamp - item.timestamp);
}

timestamp_t Clock::get_timestamp(const Item& item, const duration_type& duration) const {
    return item.timestamp + timestamp_t((duration - item.duration) / base_time(item.tempo_event));
}

void Clock::reset() {
    m_cache.clear();
    m_cache.push_back(Item{Event::tempo(120.), 0, duration_type::zero()});
}

void Clock::reset(ppqn_t ppqn) {
    m_ppqn = ppqn;
    reset();
}

void Clock::push_timestamp(const Event& tempo_event, timestamp_t timestamp) {
    auto& last = m_cache.back();
    assert(timestamp >= last.timestamp);
    if (tempo_event == last.tempo_event) {
        /// ignoring event
    } else if (timestamp == last.timestamp) {
        last.tempo_event = tempo_event;
    } else {
        m_cache.push_back(Item{tempo_event, timestamp, get_duration(last, timestamp)});
    }
}

void Clock::push_duration(const Event& tempo_event, const duration_type& duration) {
    assert(duration > m_cache.back().duration);
    m_cache.push_back(Item{tempo_event, get_timestamp(m_cache.back(), duration), duration});
}

Clock::duration_type Clock::timestamp2time(timestamp_t timestamp) const {
    return get_duration(before_timestamp(timestamp), timestamp);
}

timestamp_t Clock::time2timestamp(const duration_type& time) const {
    return get_timestamp(before_duration(time), time);
}

timestamp_t Clock::beat2timestamp(double beat) const {
    return beat * m_ppqn / 4.;
}

double Clock::timestamp2beat(timestamp_t timestamp) const {
    return 4. * timestamp / m_ppqn;
}

//==========
// Sequence
//==========

// builders

Sequence Sequence::from_file(const StandardMidiFile& data) {
    Sequence sequence;
    bool enqueue = data.format == StandardMidiFile::sequencing_format;
    track_t track = 0;
    for (const auto& track_data : data.tracks)
        sequence.insert_track(track_data, track++, enqueue ? sequence.last_timestamp() : 0);
    sequence.update_clock(data.ppqn);
    return sequence;
}

Sequence Sequence::from_realtime(const realtime_type& data, ppqn_t ppqn) {
    Sequence sequence;
    Clock::time_type t0 = data.empty() ? Clock::time_type() : data.begin()->timepoint;
    // compute clock
    sequence.m_clock.reset(ppqn);
    for (const RealtimeItem& item : data)
        if (item.event.family() == family_t::tempo)
            sequence.m_clock.push_duration(item.event, item.timepoint - t0);
    // fill event
    for (const RealtimeItem& item : data)
        sequence.push_item({sequence.m_clock.time2timestamp(item.timepoint - t0), item.event, item.track});
    return sequence;
}

// clock

const Clock& Sequence::clock() const {
    return m_clock;
}

void Sequence::update_clock() {
    m_clock.reset();
    for (const Item& item : m_events)
        if (item.event.family() == family_t::tempo)
            m_clock.push_timestamp(item.event, item.timestamp);
}

void Sequence::update_clock(ppqn_t ppqn) {
    m_clock.reset(ppqn);
    for (const Item& item : m_events)
        if (item.event.family() == family_t::tempo)
            m_clock.push_timestamp(item.event, item.timestamp);
}

// observers

const Sequence::container_type& Sequence::events() const {
    return m_events;
}

bool Sequence::empty() const {
    return m_events.empty();
}

std::set<track_t> Sequence::tracks() const {
    std::set<track_t> results;
    for (const Item& item : m_events)
        results.insert(item.track);
    return results;
}

timestamp_t Sequence::first_timestamp() const {
    return m_events.empty() ? 0. : m_events.front().timestamp;
}

timestamp_t Sequence::last_timestamp() const {
    return m_events.empty() ? 0. : m_events.back().timestamp;
}

timestamp_t Sequence::last_timestamp(track_t track) const {
    for (auto it = m_events.rbegin() ; it != m_events.rend() ; ++it)
        if (it->track == track)
            return it->timestamp;
    return 0.;
}

// mutators

void Sequence::clear() {
    m_events.clear();
    m_clock.reset();
}

void Sequence::push_item(Item item) {
    m_events.push_back(std::move(item));
}

void Sequence::insert_item(Item item) {
    auto it = std::upper_bound(m_events.begin(), m_events.end(), item.timestamp);
    m_events.emplace(it, std::move(item));
}

void Sequence::insert_track(const StandardMidiFile::track_type& track_data, track_t track, int64_t offset) {
    container_type new_events(m_events.size() + track_data.size());
    const_track_iterator it(track_data.begin(), track, offset), last(track_data.end());
    std::merge(m_events.begin(), m_events.end(), it, last, new_events.begin());
    std::swap(m_events, new_events);
}

// converters

StandardMidiFile Sequence::to_file(const blacklist_type& list) const {
    /// @todo check here non standard events
    StandardMidiFile smf;
    // map the tracks to a consecutive track id
    std::unordered_map<track_t, size_t> mapping;
    size_t track_counter = 0;
    for (track_t track: tracks())
        if (list.match(track))
            mapping[track] = track_counter++;
    std::vector<timestamp_t> timestamps(mapping.size(), 0); // (assert sequence starts at 0)
    // set file properties
    smf.format = (mapping.size() == 1) ? StandardMidiFile::single_track_format : StandardMidiFile::simultaneous_format;
    smf.ppqn = m_clock.ppqn();
    smf.tracks.resize(mapping.size());
    // set file events
    for (const Item& item : m_events) {
        auto track_it = mapping.find(item.track);
        if (track_it != mapping.end()) {
            size_t track_number = track_it->second;
            double deltatime = item.timestamp - timestamps[track_number];
            timestamps[track_number] = item.timestamp;
            smf.tracks[track_number].emplace_back(decay_value<uint32_t>(deltatime), item.event);
        }
    }
    return smf;
}

// iterators

Sequence::const_iterator Sequence::begin() const {
    return m_events.begin();
}

Sequence::const_iterator Sequence::end() const {
    return m_events.end();
}

Sequence::const_reverse_iterator Sequence::rbegin() const {
    return m_events.rbegin();
}

Sequence::const_reverse_iterator Sequence::rend() const {
    return m_events.rend();
}
