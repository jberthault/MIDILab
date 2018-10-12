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

SequenceReader::position_type make_lower(const Sequence& sequence) { return {sequence.begin(), sequence.first_timestamp()}; }
SequenceReader::position_type make_lower(const Sequence& sequence, timestamp_t timestamp) { return {std::lower_bound(sequence.begin(), sequence.end(), timestamp), timestamp}; }
SequenceReader::position_type make_upper(const Sequence& sequence, timestamp_t timestamp) { return {std::upper_bound(sequence.begin(), sequence.end(), timestamp), timestamp}; }
SequenceReader::position_type make_upper(const Sequence& sequence) { return {sequence.end(), sequence.last_timestamp()}; }

}

//================
// SequenceReader
//================

const SystemExtension<> SequenceReader::toggle_ext {"SequenceReader.toggle"};
const SystemExtension<> SequenceReader::pause_ext {"SequenceReader.pause"};
const SystemExtension<double> SequenceReader::distorsion_ext {"SequenceReader.distorsion"};

SequenceReader::SequenceReader() : Handler{Mode::io()}, m_distorsion{1.}, m_playing{false} {
    m_position = m_first_position = make_lower(m_sequence);
    m_last_position = make_upper(m_sequence);
}

SequenceReader::~SequenceReader() {
    stop_playing(stop_all);
}

const Sequence& SequenceReader::sequence() const {
    return m_sequence;
}

void SequenceReader::set_sequence(Sequence sequence) {
    stop_playing(stop_all);
    std::lock_guard<std::mutex> guard(m_mutex);
    m_sequence = std::move(sequence);
    m_position = m_first_position = make_lower(m_sequence);
    m_last_position = make_upper(m_sequence);
}

const std::map<byte_t, Sequence>& SequenceReader::sequences() const {
    return m_sequences;
}

void SequenceReader::load_sequence(byte_t id, const Sequence& sequence) {
    m_sequences[id] = sequence;
}

bool SequenceReader::select_sequence(byte_t id) {
    auto pos = m_sequences.find(id);
    if (pos == m_sequences.end())
        return false;
    set_sequence(pos->second);
    return true;
}

double SequenceReader::distorsion() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_distorsion;
}

Handler::Result SequenceReader::set_distorsion(double distorsion) {
    if (distorsion < 0)
        return Result::fail;
    std::lock_guard<std::mutex> guard(m_mutex);
    m_distorsion = distorsion;
    return Result::success;
}

bool SequenceReader::is_playing() const {
    return m_playing;
}

bool SequenceReader::is_completed() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return  m_position.first >= m_last_position.first;
}

timestamp_t SequenceReader::position() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_position.second;
}

void SequenceReader::set_position(timestamp_t timestamp) {
    jump_position(make_lower(m_sequence, timestamp));
}

timestamp_t SequenceReader::lower() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_first_position.second;
}

void SequenceReader::set_lower(timestamp_t timestamp) {
    bool needs_jump = false; // if begin has been set after the current position
    {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_first_position = make_lower(m_sequence, timestamp);
    needs_jump = m_position.first < m_first_position.first;
    }
    if (needs_jump)
        jump_position(m_first_position);
}

timestamp_t SequenceReader::upper() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_last_position.second;
}

void SequenceReader::set_upper(timestamp_t timestamp) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_last_position = make_upper(m_sequence, timestamp);
    if (m_position.first > m_last_position.first)
        m_position = m_last_position;
}

void SequenceReader::jump_position(position_type position) {
    bool playing = m_playing;
    stop_playing(false);
    forward_message({stop_notes, this});
    m_position = std::move(position);
    if (playing)
        start_playing(false);
}

bool SequenceReader::start_playing(bool rewind) {
    /// @todo adopt a strategy to forward settings at 0, when no note is available
    // handler must be stopped
    if (m_playing)
        return false;
    // ensure previous run is terminated
    stop_playing(stop_sounds);
    // can't start if unable to generate events
    if (state().none(State::forward()))
        return false;
    // reset position
    if (rewind || m_position.first < m_first_position.first)
        m_position = m_first_position;
    // check upper bound
    if (is_completed())
        return false;
    // @note it may be a good idea to clean current notes on: forward_message({stop_notes, this});
    // starts worker thread
    m_worker = std::thread{ [this] {
        m_playing = true;
        duration_type base_time = m_sequence.clock().base_time(m_sequence.clock().last_tempo(position())); // current base time for 1 deltatime
        time_type t1, t0 = clock_type::now();
        Sequence::const_iterator it, last;
        while (m_playing) {
            // compute time elapsed since last loop
            t1 = clock_type::now();
            duration_type elapsed = t1 - t0;
            t0 = t1;
            { // lock position resources
                std::lock_guard<std::mutex> guard(m_mutex);
                m_position.second += m_distorsion * elapsed / base_time; // add deltatime to the current position
                it = m_position.first; // memorize starting position
                m_position.first = std::lower_bound(it, m_last_position.first, m_position.second); // get next position
                last = m_position.first; // memorize next position
                if (m_position.first == m_last_position.first) // stop when last event is reached
                    m_playing = false;
            }
            // forward events in the current range
            for ( ; it != last ; ++it) {
                const Event& event = it->event;
                if (event.family() == family_t::tempo)
                    base_time = m_sequence.clock().base_time(event);
                forward_message({event, this, it->track});
            }
            // asleep this thread for a minimal period
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    }};
    return true;
}

bool SequenceReader::stop_playing(bool rewind) {
    m_playing = false; // notify the playing thread to stop
    const bool started = m_worker.joinable();
    if (started)
        m_worker.join();
    if (rewind)
        m_position = m_first_position;
    return started;
}

bool SequenceReader::stop_playing(const Event& final_event) {
    const bool stopped = stop_playing(false);
    if (stopped)
        forward_message({final_event, this});
    return stopped;
}

families_t SequenceReader::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::song_position, family_t::song_select, family_t::start, family_t::continue_, family_t::stop);
}

Handler::Result SequenceReader::handle_close(State state) {
    if (state & State::forward())
        stop_playing(stop_all);
    return Handler::handle_close(state);
}

Handler::Result SequenceReader::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::song_position: return handle_beat((double)message.event.get_14bits());
    case family_t::song_select: return handle_sequence(message.event.at(1));
    case family_t::start: return handle_start(true);
    case family_t::continue_: return handle_start(false);
    case family_t::stop: return handle_stop(stop_all);
    case family_t::extended_system:
        if (pause_ext.affects(message.event)) return handle_stop(stop_sounds);
        if (distorsion_ext.affects(message.event)) return set_distorsion(distorsion_ext.decode(message.event));
        if (toggle_ext.affects(message.event)) return m_playing ? handle_stop(stop_sounds) : handle_start(false);
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
    TRACE_WARNING("no song loaded for id: " << (int)id);
    return Result::fail;
}

Handler::Result SequenceReader::handle_start(bool rewind) {
    return start_playing(rewind) ? Result::success : Result::fail;
}

Handler::Result SequenceReader::handle_stop(const Event& final_event) {
    stop_playing(final_event);
    return Result::success;
}
