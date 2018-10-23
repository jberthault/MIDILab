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

#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "sequence.h"
#include "tools/trace.h"

//=========
// dumping
//=========

namespace dumping {

size_t copy_sequence(std::istream& from, std::ostream& to, size_t count) {
    for (size_t i=0 ; i < count ; i++)
        to.put(from.get());
    return count;
}

size_t expected_size(byte_t status) {
    if (0x80 <= status && status < 0xf0)
        return (0xc0 <= status && status < 0xe0) ? 1 : 2;
    if (status == 0xf1 || status == 0xf3)
        return 1;
    if (status == 0xf2)
        return 2;
    return 0;
}

// -----
// read
// -----

auto make_buffer(const char* string) {
    return range_ns::from_span(reinterpret_cast<const byte_t*>(string), ::strlen(string));
}

auto read_n(input_buffer_t& buf, ptrdiff_t count) {
    if (span(buf) < count)
        throw std::logic_error{"not enough bytes available"};
    const auto previous = std::exchange(buf.min, buf.min + count);
    return input_buffer_t{previous, buf.min};
}

auto read_at_most_n(input_buffer_t& buf, ptrdiff_t count) {
    const auto pos = std::min(buf.min + count, buf.max);
    const auto previous = std::exchange(buf.min, pos);
    return input_buffer_t{previous, pos};
}

template<typename T, ptrdiff_t count = sizeof(T)>
auto read_le(input_buffer_t& buf) {
    const auto data = read_n(buf, count);
    return byte_traits<T>::read_little_endian(data.min, data.max);
}

auto read_byte(input_buffer_t& buf) {
    return *read_n(buf, 1).min;
}

auto read_uint14(input_buffer_t& buf) {
    const auto data = read_n(buf, 2).min;
    return short_ns::uint14_t{data[1], data[0]};
}

void read_prefix(input_buffer_t& buf, const input_buffer_t& prefix) {
    const auto data = read_n(buf, span(prefix));
    if (!std::equal(data.min, data.max, prefix.min))
        throw std::logic_error{"wrong prefix"};
}

auto read_status(input_buffer_t& buf, byte_t* running_status = nullptr) {
    auto status = read_byte(buf);
    if (is_msb_set(status)) {
        if (running_status != nullptr)
            *running_status = status;
    } else {
        --buf.min; // undo read
        if (running_status != nullptr)
            status = *running_status;
        if (is_msb_cleared(status))
            throw std::logic_error{"unknown event status"};
    }
    return status;
}

auto read_variable(input_buffer_t& buf) {
    const auto last = std::min(buf.max, buf.min + 3); // do not go beyond 4 bytes
    const auto pos = std::find_if(buf.min, last, is_msb_cleared<byte_t>);
    uint32_t value = 0;
    for (const byte_t byte : read_n(buf, pos + 1 - buf.min))
        value = (value << 7) | to_data_byte(byte);
    return value;
}

auto read_sysex_size(input_buffer_t& buf, bool is_realtime) {
    if (!is_realtime)
        return static_cast<ptrdiff_t>(read_variable(buf));
    const auto pos = std::find_if(buf.min, buf.max, [](byte_t byte) { return byte == 0xf7; });
    return pos + 1 - buf.min;
}

auto read_sysex(input_buffer_t& buf, bool is_realtime) {
    const auto sysex_size = read_sysex_size(buf, is_realtime);
    const auto sysex_buf = read_n(buf, sysex_size);
    return Event::sys_ex({sysex_buf.min, sysex_buf.max});
}

auto read_meta(input_buffer_t& buf) {
    const auto meta_type_buf = read_n(buf, 1);
    const auto meta_size = read_variable(buf);
    const auto meta_data_buf = read_n(buf, meta_size);
    return Event::meta({meta_type_buf.min, meta_data_buf.max});
}

auto read_note_off(input_buffer_t& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::note_off(channels, data[0], data[1]);
}

auto read_note_on(input_buffer_t& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return data[1] == 0 ? Event::note_off(channels, data[0]) : Event::note_on(channels, data[0], data[1]);
}

auto read_aftertouch(input_buffer_t& buf,channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::aftertouch(channels, data[0], data[1]);
}

auto read_controller(input_buffer_t& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::controller(channels, data[0], data[1]);
}

Event read_event(input_buffer_t& buf, bool is_realtime, byte_t* running_status) {
    const auto status = read_status(buf, running_status);
    // ignoring 0xf4, 0xf5, 0xfd, 0xf7
    switch(status) {
    case 0xf0: return read_sysex(buf, is_realtime);
    case 0xf1: return Event::mtc_frame(read_byte(buf));
    case 0xf2: return Event::song_position(read_uint14(buf));
    case 0xf3: return Event::song_select(read_byte(buf));
    case 0xf6: return Event::tune_request();
    case 0xf8: return Event::clock();
    case 0xf9: return Event::tick();
    case 0xfa: return Event::start();
    case 0xfb: return Event::continue_();
    case 0xfc: return Event::stop();
    case 0xfe: return Event::active_sense();
    case 0xff: return is_realtime ? Event::reset() : read_meta(buf);
    default: switch (status & 0xf0) {
        case 0x80: return read_note_off(buf, channels_t::wrap(status & 0xf));
        case 0x90: return read_note_on(buf, channels_t::wrap(status & 0xf));
        case 0xa0: return read_aftertouch(buf, channels_t::wrap(status & 0xf));
        case 0xb0: return read_controller(buf, channels_t::wrap(status & 0xf));
        case 0xc0: return Event::program_change(channels_t::wrap(status & 0xf), read_byte(buf));
        case 0xd0: return Event::channel_pressure(channels_t::wrap(status & 0xf), read_byte(buf));
        case 0xe0: return Event::pitch_wheel(channels_t::wrap(status & 0xf), read_uint14(buf));
        default: return Event{};
        }
    }
}

void read_track_events(input_buffer_t& buf, StandardMidiFile::track_type& track) {
    byte_t running_status = 0; // initialize running status
    uint64_t timestamp = 0; // cumulated deltatimes
    bool eot = false; // true if end-of-track event is found
    while (!eot) {
        // read deltatime and add it to timestamp
        const auto deltatime = read_variable(buf);
        timestamp += deltatime;
        // read event and check its validity
        if (const auto event = read_event(buf, false, &running_status)) {
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
        } else {
            TRACE_WARNING("ignoring illformed event");
        }
    }
}

auto read_file(input_buffer_t& buf) {
    StandardMidiFile file;
    if (read_le<uint32_t>(buf) != 6)
        throw std::logic_error{"unexpected header size"};
    file.format = read_le<uint16_t>(buf);
    if (file.format > 2)
        throw std::logic_error{"unexpected midi file format"};
    file.tracks.resize(read_le<uint16_t>(buf));
    file.ppqn = read_le<uint16_t>(buf); /// @todo check values of ppqn
    for (auto& track : file.tracks) {
        input_buffer_t track_buf;
        try {
            read_prefix(buf, make_buffer("MTrk"));
            const auto size = read_le<uint32_t>(buf);
            track_buf = read_at_most_n(buf, size);
            track.reserve(size / 3); // rough estimation of the number of events
        } catch (const std::exception& err) {
            TRACE_ERROR("failed parsing track header: " << err.what());
            break;
        }
        try {
            read_track_events(track_buf, track);
            if (track_buf)
                throw std::logic_error{"premature end-of-track"};
        } catch (const std::exception& err) {
            TRACE_ERROR("failed parsing track events: " << err.what());
        }
    }
    return file;
}

auto remaining_size(std::istream& is) {
    const auto pos = is.tellg();
    is.seekg(0, is.end);
    const auto length = is.tellg();
    is.seekg(pos);
    return length - pos;
}

auto fill_range(std::istream& is, const range_t<byte_t*>& range) {
    if (!is.read(reinterpret_cast<char*>(range.min), span(range)))
        throw std::runtime_error{"can't read data"};
    return input_buffer_t{range.min, range.max};
}

StandardMidiFile read_file(const std::string& filename) {
    TRACE_MEASURE("read file");
    try  {
        std::ifstream ifs{filename, std::ios_base::binary};
        if (!ifs)
            throw std::runtime_error{"can't open file"};
        // check header before loading the whole file
        std::array<byte_t, 4> header_storage;
        auto header_buf = fill_range(ifs, range_ns::from_span(header_storage.data(), header_storage.size()));
        read_prefix(header_buf, make_buffer("MThd"));
        // compute the file size and read it
        std::vector<byte_t> file_storage(static_cast<size_t>(remaining_size(ifs)));
        auto file_buf = fill_range(ifs, range_ns::from_span(file_storage.data(), file_storage.size()));
        return read_file(file_buf);
    } catch (const std::exception& err) {
        TRACE_WARNING(filename << ": " << err.what());
        return StandardMidiFile{};
    }
}

// ------
// write
// ------

class variable_t {

public:

    bool set_true_value(uint32_t value) {
        true_value = value;
        encoded_value = to_data_byte(value);
        while (value >>= 7)
            encoded_value = (encoded_value << 8) | 0x80 | to_data_byte(value);
        return is_msb_cleared(encoded_value >> 24);
    }

    uint32_t encoded_value;
    uint32_t true_value;

};

size_t write_uint16(uint16_t value, std::ostream& stream) {
    byte_traits<uint16_t>::write_little_endian(value, stream, 2);
    return 2;
}

size_t write_uint32(uint32_t value, std::ostream& stream) {
    byte_traits<uint32_t>::write_little_endian(value, stream, 4);
    return 4;
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

size_t write_as_variable(uint32_t value, std::ostream& stream) {
    variable_t variable;
    if (!variable.set_true_value(value))
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
    // write status (always written for sysex events in case the size has msb set)
    if (write_status || status == 0xf0) {
        bytes++;
        stream << status;
    }
    // get the number of bytes to write
    size_t count = expected_size(status);
    if (status == 0xf0 || status == 0xff)
        count = event.size() - 1; // all bytes
    // write sysex size
    if (status == 0xf0)
        bytes += write_as_variable(static_cast<uint32_t>(count), stream);
    // get event data as a stream (without the status)
    std::stringstream event_data{std::string{event.begin() + 1, event.end()}};
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
    if (event.is(~families_t::standard() | families_t::standard_system_realtime()))
        throw std::invalid_argument("can't write custom or realtime events");
    if (event.is(families_t::standard_voice())) {
        // transform note off to note on
        if (event.family() == family_t::note_off && event.at(2) == 0)
            status = 0x90;
        // check voice event's channel
        if (!event.channels())
            throw std::invalid_argument("voice event is not bound to any channel");
        // insert one event per channel
        for (channel_t channel : event.channels()) {
            bytes += write_as_variable(deltatime, stream);
            bytes += write_raw_event((0xf0 & status) | channel, event, stream, running_status);
            deltatime = 0;
        }
    } else {
        bytes += write_as_variable(deltatime, stream);
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

Clock::Clock(ppqn_t ppqn) : m_ppqn{ppqn} {
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
    const bool enqueue = data.format == StandardMidiFile::sequencing_format;
    track_t track = 0;
    for (const auto& track_data : data.tracks)
        sequence.insert_track(track_data, track++, enqueue ? sequence.last_timestamp() : 0);
    sequence.update_clock(data.ppqn);
    return sequence;
}

Sequence Sequence::from_realtime(const realtime_type& data, ppqn_t ppqn) {
    Sequence sequence;
    const auto t0 = data.empty() ? Clock::time_type{} : data.begin()->timepoint;
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
