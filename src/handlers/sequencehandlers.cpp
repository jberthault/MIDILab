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

#include "sequencehandlers.h"
#include <algorithm>

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

//================
// SequenceReader
//================

Event SequenceReader::toggle_event() {
    return Event::custom({}, "SequenceReader.toggle");
}

Event SequenceReader::pause_event() {
    return Event::custom({}, "SequenceReader.pause");
}

Event SequenceReader::distorsion_event(double distorsion) {
    return Event::custom({}, "SequenceReader.distorsion", marshall(distorsion));
}

SequenceReader::SequenceReader() : Handler(io_mode), m_distorsion(1.), m_task(1), m_playing(false) {
    init_positions();
    m_task.start([this](std::promise<void>&& promise){
        run();
        promise.set_value();
    });
}

SequenceReader::~SequenceReader() {
    stop_playing();
    m_task.stop(true);
}

const Sequence& SequenceReader::sequence() const {
    return m_sequence;
}

void SequenceReader::set_sequence(const Sequence& sequence) {
    stop_playing();
    m_sequence = sequence;
    init_positions();
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

void SequenceReader::set_distorsion(double distorsion) {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (distorsion >= 0)
        m_distorsion = distorsion;
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
    jump_position(make_lower(timestamp));
}

timestamp_t SequenceReader::lower() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_first_position.second;
}

void SequenceReader::set_lower(timestamp_t timestamp) {
    bool needs_jump = false; // if begin has been set after the current position
    {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_first_position = make_lower(timestamp);
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
    m_last_position = make_upper(timestamp);
    if (m_position.first > m_last_position.first)
        m_position = m_last_position;
}

void SequenceReader::jump_position(position_type position) {
    bool playing = m_playing;
    stop_playing();
    m_position = position;
    if (playing)
        start_playing(false);
}

bool SequenceReader::start_playing(bool rewind) {
    // handler must be stopped
    if (m_playing)
        return false;
    // ensure previous run is terminated
    stop_playing();
    // can't start if unable to generate events
    if (state().none(forward_state))
        return false;
    // reset position
    if (rewind || m_position.first < m_first_position.first)
        m_position = m_first_position;
    // check upper bound
    if (is_completed())
        return false;
    // @note it may be a good idea to clean current notes on: forward_message(Message(Event::controller(all_channels, all_notes_off_controller), this));
    // push the new request and update status
    std::promise<void> promise;
    m_status = promise.get_future();
    m_task.push(std::move(promise));
    return true;
}

void SequenceReader::stop_playing(bool reset) {
    m_playing = false; // notify the playing thread to stop
    if (m_status.valid()) { // previously started ?
        m_status.get(); // wait until the end of run and invalid status
        // we don't know (upper limits) if some events are missing
        forward_message(Message(reset ? Event::reset() : Event::controller(all_channels, all_sound_off_controller), this));
    }
}

families_t SequenceReader::handled_families() const {
    return families_t::merge(family_t::custom, family_t::song_position, family_t::song_select, family_t::start, family_t::continue_, family_t::stop);
}

Handler::result_type SequenceReader::on_close(state_type state) {
    if (state & forward_state)
        stop_playing();
    return Handler::on_close(state);
}

Handler::result_type SequenceReader::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    switch (message.event.family()) {
    case family_t::song_position: return handle_beat((double)message.event.get_14bits());
    case family_t::song_select: return handle_sequence(message.event.at(1));
    case family_t::start: return handle_start(true);
    case family_t::continue_: return handle_start(false);
    case family_t::stop: return handle_stop(true);
    case family_t::custom: {
        auto k = message.event.get_custom_key();
        if (k == "SequenceReader.pause") return handle_stop(false);
        if (k == "SequenceReader.distorsion") return handle_distorsion(message.event.get_custom_value());
        if (k == "SequenceReader.toggle") return m_playing ? handle_stop(false) : handle_start(false);
        break;
    }
    }
    return result_type::unhandled;
}

Handler::result_type SequenceReader::handle_beat(double beat) {
    set_position(m_sequence.clock().beat2timestamp(beat));
    return result_type::success;
}

Handler::result_type SequenceReader::handle_sequence(byte_t id) {
    if (select_sequence(id))
        return result_type::success;
    TRACE_WARNING("no song loaded for id: " << (int)id);
    return result_type::fail;
}

Handler::result_type SequenceReader::handle_start(bool rewind) {
    return start_playing(rewind) ? result_type::success : result_type::fail;
}

Handler::result_type SequenceReader::handle_stop(bool reset) {
    stop_playing(reset);
    return result_type::success;
}

Handler::result_type SequenceReader::handle_distorsion(const std::string& distorsion) {
    set_distorsion(unmarshall<double>(distorsion));
    return result_type::success;
}

/// @todo adopt a strategy to forward settings at 0, when no note is available

void SequenceReader::run() {
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
        // asleep this thread
        std::this_thread::yield();
    }
}

void SequenceReader::init_positions() {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_position.first = m_first_position.first = m_sequence.begin();
    m_position.second = m_first_position.second = m_sequence.first_timestamp();
    m_last_position.first = m_sequence.end();
    m_last_position.second = m_sequence.last_timestamp();
}

SequenceReader::position_type SequenceReader::make_lower(timestamp_t timestamp) const {
    return {std::lower_bound(m_sequence.begin(), m_sequence.end(), timestamp), timestamp};
}

SequenceReader::position_type SequenceReader::make_upper(timestamp_t timestamp) const {
    return {std::upper_bound(m_sequence.begin(), m_sequence.end(), timestamp), timestamp};
}

//================
// SequenceWriter
//================

/// @todo remove the lock guard

SequenceWriter::SequenceWriter() :
    Handler(out_mode), m_recording(false), m_families(voice_families | meta_families) {

    m_storage.reserve(8192);
}

void SequenceWriter::set_families(families_t families) {
    m_families = families;
}

Sequence SequenceWriter::load_sequence() const {
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    Sequence sequence = Sequence::from_realtime(m_storage);
    for (track_t track : sequence.tracks())
        sequence.push_event(Event::end_of_track(), track, sequence.last_timestamp(track));
    return sequence;
}

Handler::result_type SequenceWriter::handle_message(const Message& message) {
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    if (!message.event.is(m_families) || !m_recording)
        return result_type::unhandled;
    m_storage.push_back(Sequence::realtime_type::value_type{message.event, message.track, clock_type::now()});
    return result_type::success;
}

void SequenceWriter::start_recording() {
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    if (!m_recording) {
        m_storage.clear();
        m_recording = true;
    }
}

void SequenceWriter::stop_recording() {
    std::lock_guard<std::mutex> guard(m_storage_mutex);
    m_recording = false;
}
