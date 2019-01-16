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
#include <numeric>
#include "sequence.h"
#include "tools/trace.h"

//=========
// dumping
//=========

namespace dumping {

// -----
// read
// -----

auto read_n(byte_cview& buf, std::ptrdiff_t count) {
    if (span(buf) < count)
        throw std::logic_error{"not enough bytes available"};
    const auto previous = std::exchange(buf.min, buf.min + count);
    return byte_cview{previous, buf.min};
}

auto read_at_most_n(byte_cview& buf, std::ptrdiff_t count) {
    const auto pos = std::min(buf.min + count, buf.max);
    const auto previous = std::exchange(buf.min, pos);
    return byte_cview{previous, pos};
}

template<typename T, std::ptrdiff_t count = sizeof(T)>
auto read_le(byte_cview& buf) {
    const auto data = read_n(buf, count);
    return byte_traits<T>::read_le(data.min, data.max);
}

auto read_byte(byte_cview& buf) {
    return *read_n(buf, 1).min;
}

auto read_uint14(byte_cview& buf) {
    const auto data = read_n(buf, 2).min;
    return short_ns::uint14_t{data[1], data[0]};
}

void read_prefix(byte_cview& buf, byte_cview prefix) {
    const auto data = read_n(buf, span(prefix));
    if (!std::equal(data.min, data.max, prefix.min))
        throw std::logic_error{"wrong prefix"};
}

auto read_status(byte_cview& buf, byte_t* running_status = nullptr) {
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

auto read_variable(byte_cview& buf) {
    const auto last = std::min(buf.max, buf.min + 3); // do not go beyond 4 bytes
    const auto pos = std::find_if(buf.min, last, is_msb_cleared<byte_t>);
    uint32_t value = 0;
    for (const auto byte : read_n(buf, pos + 1 - buf.min))
        value = (value << 7) | to_data_byte(byte);
    return value;
}

auto read_sysex_size(byte_cview& buf, bool is_realtime) {
    if (!is_realtime)
        return static_cast<std::ptrdiff_t>(read_variable(buf));
    const auto pos = std::find_if(buf.min, buf.max, [](byte_t byte) { return byte == 0xf7; });
    return pos + 1 - buf.min;
}

auto read_sysex(byte_cview& buf, bool is_realtime) {
    const auto sysex_size = read_sysex_size(buf, is_realtime);
    const auto sysex_buf = read_n(buf, sysex_size);
    return Event::sys_ex({sysex_buf.min, sysex_buf.max});
}

auto read_meta(byte_cview& buf) {
    const auto meta_type_buf = read_n(buf, 1);
    const auto meta_size = read_variable(buf);
    const auto meta_data_buf = read_n(buf, meta_size);
    return Event::meta({meta_type_buf.min, meta_data_buf.max});
}

auto read_note_off(byte_cview& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::note_off(channels, data[0], data[1]);
}

auto read_note_on(byte_cview& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return data[1] == 0 ? Event::note_off(channels, data[0]) : Event::note_on(channels, data[0], data[1]);
}

auto read_aftertouch(byte_cview& buf,channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::aftertouch(channels, data[0], data[1]);
}

auto read_controller(byte_cview& buf, channels_t channels) {
    const auto data = read_n(buf, 2).min;
    return Event::controller(channels, data[0], data[1]);
}

Event read_event(byte_cview& buf, bool is_realtime, byte_t* running_status) {
    const auto status = read_status(buf, running_status);
    // ignoring 0xf4, 0xf5, 0xfd, 0xf7
    switch (status) {
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

void read_track_events(byte_cview& buf, track_t track_number, StandardMidiFile::track_type& track) {
    byte_t running_status = 0; // initialize running status
    uint64_t timestamp = 0; // cumulated deltatimes
    bool eot = false; // true if end-of-track event is found
    while (!eot) {
        // read deltatime and add it to timestamp
        const auto deltatime = read_variable(buf);
        timestamp += deltatime;
        // read event and check its validity
        if (auto event = read_event(buf, false, &running_status)) {
            eot = event.is(family_t::end_of_track);
            if (deltatime == 0 && !track.empty() && Event::equivalent(track.back().second, event)) {
                // check if current event can be merged with the last one
                // it aims to remove duplicated events while combining voice events on different channels
                track.back().second.set_channels(track.back().second.channels() | event.channels());
            } else {
                // add event in the track, ignores deltatime for EOT when a particular timestamp is reached
                // it may be an annoying feature/bug of some editor
                event.set_track(track_number);
                track.emplace_back(eot && timestamp == 0x03ffff ? 0 : deltatime, std::move(event));
            }
        } else {
            TRACE_WARNING("ignoring illformed event");
        }
    }
}

auto read_file(byte_cview& buf) {
    StandardMidiFile file;
    if (read_le<uint32_t>(buf) != 6)
        throw std::logic_error{"unexpected header size"};
    file.format = read_le<uint16_t>(buf);
    if (file.format > 2)
        throw std::logic_error{"unexpected midi file format"};
    file.tracks.resize(read_le<uint16_t>(buf));
    file.ppqn = read_le<uint16_t>(buf); /// @todo check values of ppqn
    track_t track_number = 0;
    for (auto& track : file.tracks) {
        byte_cview track_buf;
        try {
            read_prefix(buf, make_view("MTrk"));
            const auto size = read_le<uint32_t>(buf);
            track_buf = read_at_most_n(buf, size);
            track.reserve(size / 3); // rough estimation of the number of events
        } catch (const std::exception& err) {
            TRACE_ERROR("failed parsing track header: " << err.what());
            break;
        }
        try {
            read_track_events(track_buf, track_number, track);
            if (track_buf)
                throw std::logic_error{"premature end-of-track"};
        } catch (const std::exception& err) {
            TRACE_ERROR("failed parsing track events: " << err.what());
        }
        ++track_number;
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

auto fill_range(std::istream& is, byte_view range) {
    if (!is.read(reinterpret_cast<char*>(range.min), span(range)))
        throw std::runtime_error{"can't read data"};
    return byte_cview{range.min, range.max};
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
        read_prefix(header_buf, make_view("MThd"));
        // compute the file size and read it
        std::vector<byte_t> file_storage(static_cast<size_t>(remaining_size(ifs)));
        auto file_buf = fill_range(ifs, range_ns::from_span(file_storage.data(), file_storage.size()));
        return read_file(file_buf);
    } catch (const std::exception& err) {
        TRACE_ERROR(filename << ": " << err.what());
        return StandardMidiFile{};
    }
}

// ------
// write
// ------

size_t write_byte(byte_t value, std::ostream& stream) {
    stream << value;
    return 1;
}

template<typename T, size_t count = sizeof(T)>
size_t write_le(T value, std::ostream& stream) {
    byte_traits<T>::write_le(value, stream, count);
    return count;
}

size_t write_buf(const byte_cview& buf, std::ostream& stream) {
    stream.write(reinterpret_cast<const char*>(buf.min), span(buf));
    return static_cast<size_t>(span(buf));
}

size_t write_status(byte_t status, std::ostream& stream, byte_t* running_status) {
    // update running status
    bool write_status = true;
    if (running_status != nullptr) {
        write_status = status != *running_status;
        *running_status = status;
    }
    // write status (always written for sysex events in case the size has msb set)
    size_t bytes = 0;
    if (write_status || status == 0xf0)
        bytes += write_byte(status, stream);
    return bytes;
}

size_t write_variable(uint32_t value, std::ostream& stream) {
    const auto encoded_value = encode_variable(value);
    return write_buf(make_view(encoded_value), stream);
}

size_t write_raw_event(const Event& event, std::ostream& stream) {
    size_t bytes = 0;
    auto view = extraction_ns::view(event);
    const auto status = read_byte(view); // consume status byte from view
    if (status == 0xf0) // write sysex size
        bytes += write_variable(static_cast<uint32_t>(span(view)), stream);
    bytes += write_buf(view, stream);
    return bytes;
}

size_t write_event(uint32_t deltatime, const Event& event, std::ostream& stream, byte_t* running_status) {
    // check event type
    if (!event)
        throw std::invalid_argument{"can't write null event"};
    if (event.is(~families_t::standard() | families_t::standard_system_realtime()))
        throw std::invalid_argument{"can't write custom or realtime events"};
    size_t bytes = 0;
    byte_t status = extraction_ns::status(event);
    if (event.is(families_t::standard_voice())) {
        // transform note off to note on
        if (event.is(family_t::note_off) && extraction_ns::velocity(event) == 0)
            status = 0x90;
        // check voice event's channel
        if (!event.channels())
            throw std::invalid_argument{"voice event is not bound to any channel"};
        // insert one event per channel
        for (channel_t channel : event.channels()) {
            bytes += write_variable(deltatime, stream);
            bytes += write_status(status | channel, stream, running_status);
            bytes += write_raw_event(event, stream);
            deltatime = 0;
        }
    } else {
        bytes += write_variable(deltatime, stream);
        bytes += write_status(status, stream, running_status);
        bytes += write_raw_event(event, stream);
    }
    return bytes;
}

size_t write_track(const StandardMidiFile::track_type& track, std::ostream& stream, bool use_running_status) {
    size_t size = 0; // number of bytes in the chunk
    byte_t running_status = 0; // initialize running status
    byte_t* running_status_ptr = use_running_status ? &running_status : nullptr;
    std::stringstream oss;
    // check track integrity
    if (track.empty())
        throw std::invalid_argument{"empty track"};
    // write data
    for (const auto& value : track)
        size += write_event(value.first, value.second, oss, running_status_ptr);
    if (track.back().second.family() != family_t::end_of_track)
        size += write_event(0, Event::end_of_track(), oss, running_status_ptr);
    // prepend a header
    size_t bytes = 0;
    bytes += write_buf(make_view("MTrk"), stream);
    bytes += write_le(static_cast<uint32_t>(size), stream);
    stream << oss.rdbuf();
    bytes += size;
    return bytes;
}

size_t write_file(const StandardMidiFile& file, std::ostream& stream, bool use_running_status) {
    /// @todo check format & tracks & ppqn
    size_t bytes = 0;
    bytes += write_buf(make_view("MThd"), stream);
    bytes += write_le(static_cast<uint32_t>(6), stream);
    bytes += write_le(static_cast<uint16_t>(file.format), stream);
    bytes += write_le(static_cast<uint16_t>(file.tracks.size()), stream);
    bytes += write_le(static_cast<uint16_t>(file.ppqn), stream);
    for (const StandardMidiFile::track_type& track: file.tracks)
        bytes += write_track(track, stream, use_running_status);
    return bytes;
}

size_t write_file(const StandardMidiFile& file, const std::string& filename, bool use_running_status) {
    try  {
        std::ofstream ofs{filename, std::ios_base::binary};
        if (!ofs)
            throw std::logic_error{"can't open file"};
        ofs.exceptions(std::ios_base::failbit);
        return write_file(file, ofs, use_running_status);
    } catch (const std::exception& err) {
        TRACE_WARNING(filename << ": " << err.what());
        return 0;
    }
}

}

namespace {

void copy_track(StandardMidiFile::track_type track, uint64_t& timestamp, TimedEvents::iterator& it) {
    for (auto& pair : track) {
        timestamp += pair.first;
        it->event = std::move(pair.second);
        it->timestamp = timestamp;
        ++it;
    }
}

template<typename It>
auto count_sizes(It first, It last, size_t init = 0) {
    return std::accumulate(first, last, init, [](size_t partial, const auto& collection) { return partial + collection.size(); });
}

template<typename IteratorT, typename ... Args>
auto relaxed_upper_bound(IteratorT first, IteratorT last, Args&& ... args) {
    // returns an iterator to the last element in [first, last) which value compares less than or equal to key
    auto it = std::upper_bound(first, last, std::forward<Args>(args)...);
    if (it != first)
        --it;
    return it;
}

auto before_timestamp(const Clock::TempoItems& items, timestamp_t timestamp) {
    return relaxed_upper_bound(items.begin(), items.end(), timestamp, [](timestamp_t t, const Clock::TempoItem& item) { return t < item.timestamp; });
}

template<typename DurationT>
auto before_duration(const Clock::TempoItems& items, const DurationT& duration) {
    return relaxed_upper_bound(items.begin(), items.end(), duration, [](const DurationT& d, const Clock::TempoItem& item) { return d < item.duration; });
}

template<typename ItemT>
bool merge_event(ItemT& item, const Event& event, timestamp_t timestamp) {
    assert(timestamp >= item.timestamp);
    if (Event::equivalent(event, item.event))
        return true;
    if (timestamp == item.timestamp) {
        item.event = event;
        return true;
    }
    return false;
}

}

//=======
// Clock
//=======

Clock::Clock(ppqn_t ppqn) : m_ppqn{ppqn} {
    reset();
}

void Clock::reset() {
    m_tempo.resize(1, {0., Clock::duration_type::zero(), Event::tempo(120.)});
    m_time_signature.resize(1, {0., Event::time_signature(4, 2, 24, 8)});
}

void Clock::push_timestamp(const Event& event, timestamp_t timestamp) {
    if (event.is(family_t::tempo)) {
        auto& last = m_tempo.back();
        if (!merge_event(last, event, timestamp))
            m_tempo.push_back({timestamp, get_duration(last, timestamp), event});
    } else if (event.is(family_t::time_signature)) {
        if (!merge_event(m_time_signature.back(), event, timestamp))
            m_time_signature.emplace_back(timestamp, event);
    }
}

void Clock::push_duration(const Event& event, const duration_type& duration) {
    if (event.is(family_t::tempo)) {
        assert(duration > m_tempo.back().duration);
        m_tempo.push_back({get_timestamp(m_tempo.back(), duration), duration, event});
    } else if (event.is(family_t::time_signature)) {
        assert(duration > m_time_signature.back().duration);
        m_time_signature.emplace_back(get_timestamp(m_tempo.back(), duration), event);
    }
}

Clock::duration_type Clock::base_time(const Event& tempo_event) const {
    assert(tempo_event.is(family_t::tempo));
    /// @todo check negative values of first byte of ppqn
    return duration_type{extraction_ns::get_meta_int(tempo_event) / static_cast<double>(m_ppqn)};
}

const Clock::TempoItem& Clock::last_tempo(timestamp_t timestamp) const {
    return *before_timestamp(m_tempo, timestamp);
}

const TimedEvent& Clock::last_time_signature(timestamp_t timestamp) const {
    return *relaxed_upper_bound(m_time_signature.begin(), m_time_signature.end(), timestamp);
}

Clock::duration_type Clock::last_base_time(timestamp_t timestamp) const {
    return base_time(last_tempo(timestamp).event);
}

Clock::duration_type Clock::timestamp2time(timestamp_t timestamp) const {
    return get_duration(last_tempo(timestamp), timestamp);
}

timestamp_t Clock::time2timestamp(const duration_type& time) const {
    return get_timestamp(*before_duration(m_tempo, time), time);
}

double Clock::timestamp2beat(timestamp_t timestamp) const {
    return 4. * timestamp2qn(timestamp);
}

timestamp_t Clock::beat2timestamp(double beat) const {
    return qn2timestamp(beat / 4.);
}

double Clock::timestamp2clock(timestamp_t timestamp) const {
    return 24. * timestamp2qn(timestamp);
}

timestamp_t Clock::clock2timestamp(double clock) const {
    return qn2timestamp(clock / 24.);
}

double Clock::timestamp2qn(timestamp_t timestamp) const {
    return timestamp / m_ppqn;
}

timestamp_t Clock::qn2timestamp(double qn) const {
    return qn * m_ppqn;
}

Clock::duration_type Clock::get_duration(const TempoItem& item, timestamp_t timestamp) const {
    return item.duration + base_time(item.event) * (timestamp - item.timestamp);
}

timestamp_t Clock::get_timestamp(const TempoItem& item, const duration_type& duration) const {
    return item.timestamp + static_cast<timestamp_t>((duration - item.duration) / base_time(item.event));
}

//==========
// Sequence
//==========

// builders

Sequence Sequence::from_file(StandardMidiFile data) {
    Sequence sequence{data.ppqn};
    sequence.m_events.resize(count_sizes(data.tracks.begin(), data.tracks.end()));
    const auto first = sequence.begin();
    if (data.format == StandardMidiFile::sequencing_format) {
        auto it = first;
        uint64_t timestamp = 0;
        for (auto& track : data.tracks)
            copy_track(std::move(track), timestamp, it);
    } else {
        auto it = first;
        for (auto& track : data.tracks) {
            uint64_t timestamp = 0;
            const auto pos = it;
            copy_track(std::move(track), timestamp, it);
            std::inplace_merge(first, pos, it);
        }
    }
    for (const auto& item : sequence.m_events)
        sequence.m_clock.push_timestamp(item.event, item.timestamp);
    return sequence;
}

Sequence Sequence::from_realtime(const realtime_type& data, ppqn_t ppqn) {
    Sequence sequence{ppqn};
    const auto t0 = data.empty() ? Clock::time_type{} : data.begin()->timepoint;
    // compute clock
    for (const auto& realtime_item : data)
        sequence.m_clock.push_duration(realtime_item.event, realtime_item.timepoint - t0);
    // fill event
    for (const auto& realtime_item : data)
        sequence.m_events.emplace_back(sequence.m_clock.time2timestamp(realtime_item.timepoint - t0), realtime_item.event);
    return sequence;
}

Sequence::Sequence(ppqn_t ppqn) : m_clock{ppqn} {

}

std::set<track_t> Sequence::tracks() const {
    std::set<track_t> results;
    for (const auto& item : m_events)
        results.insert(item.event.track());
    return results;
}

range_t<uint32_t> Sequence::track_range() const {
    range_t<uint32_t> result{default_track, default_track};
    if (!m_events.empty()) {
        const auto its = std::minmax_element(m_events.begin(), m_events.end(), [](const auto& lhs, const auto& rhs) { return lhs.event.track() < rhs.event.track(); });
        result.min = static_cast<uint32_t>(its.first->event.track());
        result.max = static_cast<uint32_t>(its.second->event.track()) + 1;
    }
    return result;
}

timestamp_t Sequence::first_timestamp() const {
    return m_events.empty() ? 0. : m_events.front().timestamp;
}

timestamp_t Sequence::last_timestamp() const {
    return m_events.empty() ? 0. : m_events.back().timestamp;
}

timestamp_t Sequence::last_timestamp(track_t track) const {
    for (auto it = m_events.rbegin() ; it != m_events.rend() ; ++it)
        if (it->event.track() == track)
            return it->timestamp;
    return 0.;
}

void Sequence::clear() {
    m_events.clear();
    m_clock.reset();
}

void Sequence::push_item(TimedEvent item) {
    m_events.push_back(std::move(item));
}

void Sequence::insert_item(TimedEvent item) {
    auto it = std::upper_bound(m_events.begin(), m_events.end(), item.timestamp);
    m_events.emplace(it, std::move(item));
}

void Sequence::insert_items(const TimedEvents& items) {
    const auto previous_size = static_cast<std::ptrdiff_t>(m_events.size());
    m_events.insert(m_events.end(), items.begin(), items.end());
    std::inplace_merge(m_events.begin(), m_events.begin() + previous_size, m_events.end());
}

void Sequence::update_clock() {
    m_clock.reset();
    for (const auto& item : m_events)
        m_clock.push_timestamp(item.event, item.timestamp);
}

StandardMidiFile Sequence::to_file(const blacklist_type& list) const {
    /// @todo check here non standard events
    StandardMidiFile smf;
    // map the tracks to a consecutive track id
    std::unordered_map<track_t, size_t> mapping;
    size_t track_counter = 0;
    for (const auto track : tracks())
        if (list.match(track))
            mapping[track] = track_counter++;
    std::vector<timestamp_t> timestamps(mapping.size(), 0); // (assert sequence starts at 0)
    // set file properties
    smf.format = (mapping.size() == 1) ? StandardMidiFile::single_track_format : StandardMidiFile::simultaneous_format;
    smf.ppqn = m_clock.ppqn();
    smf.tracks.resize(mapping.size());
    // set file events
    for (const auto& item : m_events) {
        const auto track_it = mapping.find(item.event.track());
        if (track_it != mapping.end()) {
            const auto track_number = track_it->second;
            const auto deltatime = item.timestamp - timestamps[track_number];
            timestamps[track_number] = item.timestamp;
            smf.tracks[track_number].emplace_back(decay_value<uint32_t>(deltatime), item.event);
        }
    }
    return smf;
}

TimedEvents Sequence::make_metronome(byte_t velocity) const {
    TimedEvents result;
    // Compute next track available
    const auto range = track_range();
    const auto track = range.max <= std::numeric_limits<track_t>::max() ? static_cast<track_t>(range.max) : default_track;
    // Prepare metronome events
    const auto click = Event::note_on(channels_t::drums(), drum_ns::metronome_click_drum, velocity).with_track(track);
    const auto bell = Event::note_on(channels_t::drums(), drum_ns::metronome_bell_drum, velocity).with_track(track);
    // There will be around 1 event per quarter note (24 MIDI clocks)
    result.reserve(decay_value<size_t>(last_timestamp() / m_clock.ppqn()));
    // Iterate over all time signature events
    const auto last = m_clock.time_signature().end();
    for (auto it = m_clock.time_signature().begin() ; it != last ; ++it) {
        // Compute the timestamp of a single tick, which is basically the ppqn.
        // We choose to ignore the given cc and bb values (view.min[2] and view.min[3])
        // so that the metronome clicks for each true quarter note.
        // Otherwise the tick base would be (cc/24)*ppqn
        const auto view = extraction_ns::get_meta_cview(it->event);
        const auto nn = view.min[0];
        const auto tick_base = static_cast<timestamp_t>(m_clock.ppqn());
        // compute the limit
        const auto next_it = std::next(it);
        const auto next_timestamp = next_it == last ? last_timestamp() : next_it->timestamp;
        // fill quarter notes until the next time signature
        for (uint32_t tick=0 ; ; ++tick) {
            const auto timestamp = it->timestamp + tick * tick_base;
            // stops if timestamp reaches next timestamp or if it is too close (half of the tick base)
            if (timestamp + tick_base / 2 >= next_timestamp)
                break;
            // Every nn quarter notes, the bell is emitted, starting at the time signature timestamp
            result.emplace_back(timestamp, (tick % nn == 0) ? bell : click);
        }
    }
    return result;
}
