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

#include "sequencereader.h"
#include <algorithm>

namespace {

const auto stop_sounds = Event::controller(channels_t::full(), controller_ns::all_sound_off_controller);
const auto stop_notes = Event::controller(channels_t::full(), controller_ns::all_notes_off_controller);
const auto stop_all = Event::reset();

auto make_lower(const Sequence& sequence) { return SequenceReader::position_type{sequence.begin(), sequence.first_timestamp()}; }
auto make_lower(const Sequence& sequence, timestamp_t timestamp) { return SequenceReader::position_type{std::lower_bound(sequence.begin(), sequence.end(), timestamp), timestamp}; }
auto make_upper(const Sequence& sequence, timestamp_t timestamp) { return SequenceReader::position_type{std::upper_bound(sequence.begin(), sequence.end(), timestamp), timestamp}; }
auto make_upper(const Sequence& sequence) { return SequenceReader::position_type{sequence.end(), sequence.last_timestamp()}; }

constexpr auto playing_state = Handler::State::from_integral(0x4);

}

//================
// SequenceReader
//================

const SystemExtension<void> SequenceReader::toggle_ext {"SequenceReader.toggle"};
const SystemExtension<void> SequenceReader::pause_ext {"SequenceReader.pause"};
const SystemExtension<double> SequenceReader::distorsion_ext {"SequenceReader.distorsion"};

SequenceReader::SequenceReader() : Handler{Mode::io()} {
    m_position = m_limits.min = make_lower(m_sequence);
    m_limits.max = make_upper(m_sequence);
}

SequenceReader::~SequenceReader() {
    stop_playing(stop_all, false, false);
}

const Sequence& SequenceReader::sequence() const {
    return m_sequence;
}

void SequenceReader::set_sequence(Sequence sequence) {
    stop_playing(stop_all, false, false);
    std::lock_guard<std::mutex> guard{m_mutex};
    m_sequence = std::move(sequence);
    m_position = m_limits.min = make_lower(m_sequence);
    m_limits.max = make_upper(m_sequence);
}

void SequenceReader::replace_sequence(Sequence sequence) {
    std::lock_guard<std::mutex> guard{m_mutex};
    m_sequence = std::move(sequence);
    m_position = make_lower(m_sequence, m_position.second);
    m_limits.min = make_lower(m_sequence, m_limits.min.second);
    m_limits.max = make_upper(m_sequence, m_limits.max.second);
}

const std::map<byte_t, Sequence>& SequenceReader::sequences() const {
    return m_sequences;
}

void SequenceReader::load_sequence(byte_t id, Sequence sequence) {
    m_sequences[id] = std::move(sequence);
}

bool SequenceReader::select_sequence(byte_t id) {
    auto pos = m_sequences.find(id);
    if (pos == m_sequences.end())
        return false;
    set_sequence(pos->second);
    return true;
}

double SequenceReader::distorsion() const {
    std::lock_guard<std::mutex> guard{m_mutex};
    return m_distorsion;
}

Handler::Result SequenceReader::set_distorsion(double distorsion) {
    if (distorsion < 0)
        return Result::fail;
    std::lock_guard<std::mutex> guard{m_mutex};
    m_distorsion = distorsion;
    return Result::success;
}

bool SequenceReader::is_playing() const {
    return state().any(playing_state);
}

bool SequenceReader::is_completed() const {
    std::lock_guard<std::mutex> guard{m_mutex};
    return m_position.first >= m_limits.max.first;
}

timestamp_t SequenceReader::position() const {
    std::lock_guard<std::mutex> guard{m_mutex};
    return m_position.second;
}

void SequenceReader::set_position(timestamp_t timestamp) {
    jump_position(make_lower(m_sequence, timestamp));
}

range_t<timestamp_t> SequenceReader::limits() const {
    std::lock_guard<std::mutex> guard{m_mutex};
    return {m_limits.min.second, m_limits.max.second};
}

void SequenceReader::set_lower(timestamp_t timestamp) {
    bool needs_jump = false; // if begin has been set after the current position
    {
    std::lock_guard<std::mutex> guard{m_mutex};
    m_limits.min = make_lower(m_sequence, timestamp);
    needs_jump = m_position.first < m_limits.min.first;
    }
    if (needs_jump)
        jump_position(m_limits.min);
}

void SequenceReader::set_upper(timestamp_t timestamp) {
    std::lock_guard<std::mutex> guard{m_mutex};
    m_limits.max = make_upper(m_sequence, timestamp);
    if (m_position.first > m_limits.max.first)
        m_position = m_limits.max;
}

void SequenceReader::jump_position(position_type position) {
    const bool playing = is_playing();
    stop_playing(stop_notes, true, false);
    m_position = std::move(position);
    if (playing)
        start_playing(false);
}

bool SequenceReader::start_playing(bool rewind) {
    /// @todo adopt a strategy to forward settings at 0, when no note is available
    // handler must be stopped
    if (is_playing())
        return false;
    // ensure previous run is terminated
    stop_playing(stop_sounds, false, false);
    // can't start if unable to generate events
    if (state().none(State::forward()))
        return false;
    // reset position
    if (rewind || m_position.first < m_limits.min.first)
        m_position = m_limits.min;
    // check upper bound
    if (is_completed())
        return false;
    // @note it may be a good idea to clean current notes on: produce_message(stop_notes);
    // starts worker thread
    m_worker = std::thread{ [this] {
        activate_state(playing_state);
        duration_type base_time = m_sequence.clock().last_base_time(position()); // current base time for 1 deltatime
        range_t<time_type> time_loop = {{}, clock_type::now()};
        range_t<TimedEvents::const_iterator> it_loop;
        while (is_playing()) {
            // update time loop
            time_loop.min = std::exchange(time_loop.max, clock_type::now());
            { // lock position resources
                std::lock_guard<std::mutex> guard{m_mutex};
                m_position.second += m_distorsion * span(time_loop) / base_time; // add deltatime to the current position
                it_loop.min = m_position.first; // memorize starting position
                m_position.first = std::lower_bound(m_position.first, m_limits.max.first, m_position.second); // get next position
                it_loop.max = m_position.first; // memorize next position
                if (m_position.first == m_limits.max.first) // stop when last event is reached
                    deactivate_state(playing_state);
            }
            // forward events in the current range
            for (const auto& item : it_loop) {
                if (item.event.is(family_t::tempo))
                    base_time = m_sequence.clock().base_time(item.event);
                produce_message(item.event);
            }
            // asleep this thread for a minimal period
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    }};
    return true;
}

bool SequenceReader::stop_playing(const Event& final_event, bool always_send, bool rewind) {
    deactivate_state(playing_state); // notify the playing thread to stop
    const bool started = m_worker.joinable();
    if (started)
        m_worker.join();
    if (rewind)
        m_position = m_limits.min;
    if (started || always_send)
        produce_message(final_event);
    return started;
}

families_t SequenceReader::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::song_position, family_t::song_select, family_t::start, family_t::continue_, family_t::stop);
}

Handler::Result SequenceReader::handle_close(State state) {
    if (state & State::forward())
        stop_playing(stop_all, false, false);
    return Handler::handle_close(state);
}

Handler::Result SequenceReader::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::song_position: return handle_beat(static_cast<double>(extraction_ns::get_14bits(message.event)));
    case family_t::song_select: return handle_sequence(extraction_ns::song(message.event));
    case family_t::start: return handle_start(true);
    case family_t::continue_: return handle_start(false);
    case family_t::stop: return handle_stop(stop_all);
    case family_t::extended_system:
        if (pause_ext.affects(message.event)) return handle_stop(stop_sounds);
        if (distorsion_ext.affects(message.event)) return set_distorsion(distorsion_ext.decode(message.event));
        if (toggle_ext.affects(message.event)) return is_playing() ? handle_stop(stop_sounds) : handle_start(false);
        break;
    default:
        break;
    }
    return Result::unhandled;
}

Handler::Result SequenceReader::handle_beat(double beat) {
    set_position(m_sequence.clock().beat2timestamp(beat));
    return Result::success;
}

Handler::Result SequenceReader::handle_sequence(byte_t id) {
    if (select_sequence(id))
        return Result::success;
    TRACE_WARNING("no song loaded for id: " << static_cast<int>(id));
    return Result::fail;
}

Handler::Result SequenceReader::handle_start(bool rewind) {
    return start_playing(rewind) ? Result::success : Result::fail;
}

Handler::Result SequenceReader::handle_stop(const Event& final_event) {
    stop_playing(final_event, false, false);
    return Result::success;
}
