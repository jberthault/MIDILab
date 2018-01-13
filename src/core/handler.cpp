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

#include <algorithm>
#include <sstream>
#include "handler.h"
#include "note.h"

using namespace family_ns;
using namespace handler_ns;

//=========
// Message
//=========

Message::Message(Event event, Handler* source, track_t track) :
    event(std::move(event)), source(source), track(track) {

}

//========
// Filter
//========

namespace {

namespace visitors {

struct Match : public boost::static_visitor<bool> {

    const Message& message;

    Match(const Message& message) : message(message) {

    }

    bool operator()(const Filter::HandlerFilter& filter) const {
        return message.source == filter.handler;
    }

    bool operator()(const Filter::TrackFilter& filter) const {
        return message.track == filter.track;
    }

    bool operator()(const Filter::ChannelFilter& filter) const {
        return filter.channels.all(message.event.channels());
    }

    bool operator()(const Filter::FamilyFilter& filter) const {
        return filter.families.contains(message.event.family());
    }

    bool operator()(const Filter::AllFilter& filter) const {
        return std::all_of(filter.filters.begin(), filter.filters.end(), [this](const Filter& f) {
            return f.match_message(message);
        });
    }

    bool operator()(const Filter::AnyFilter& filter) const {
        return std::any_of(filter.filters.begin(), filter.filters.end(), [this](const Filter& f) {
            return f.match_message(message);
        });
    }

};

struct String : public boost::static_visitor<void> {

    std::ostream& stream;
    bool surround;

    String(std::ostream& stream, bool surround) : stream(stream), surround(surround) {

    }

    void operator()(const Filter::HandlerFilter& filter) const {
        stream << '"' << filter.handler->name() << '"';
    }

    void operator()(const std::vector<Filter>& filters, const std::string& sep) const {
        if (surround)
            stream << '(';
        auto it = filters.begin(), last = filters.end();
        if (it != last) {
            it->write(stream, true);
            for (++it ; it != last ; ++it) {
                stream << sep;
                it->write(stream, true);
            }
        }
        if (surround)
            stream << ")";
    }

    void operator()(const Filter::AllFilter& filter) const {
        operator ()(filter.filters, " & ");
    }

    void operator()(const Filter::AnyFilter& filter) const {
        operator ()(filter.filters, " | ");
    }

    template<typename T>
    void operator()(const T&) const {
        stream << "<unimplemented>";
    }

};

struct MatchHandler : public boost::static_visitor<Filter::match_type> {

    const Handler* handler;

    MatchHandler(const Handler* handler) : handler(handler) {

    }

    Filter::match_type operator()(const Filter::HandlerFilter& filter) const {
        return handler == filter.handler;
    }

    Filter::match_type operator()(const Filter::AllFilter& filter) const {
        Filter::match_type all(true);
        for (const Filter& f : filter.filters)
            all = all && f.match_handler(handler);
        return all;
    }

    Filter::match_type operator()(const Filter::AnyFilter& filter) const {
        Filter::match_type all(false);
        for (const Filter& f : filter.filters)
            all = all || f.match_handler(handler);
        return all;
    }

    template<typename T>
    Filter::match_type operator()(const T&) const {
        return boost::logic::indeterminate;
    }

};

struct MatchNothing : public boost::static_visitor<Filter::match_type> {

    Filter::match_type operator()(const Filter::AllFilter& filter) const {
        return filter.filters.empty() || Filter::match_type(boost::logic::indeterminate);
    }

    Filter::match_type operator()(const Filter::AnyFilter& filter) const {
        return !filter.filters.empty() && Filter::match_type(boost::logic::indeterminate);
    }

    template<typename T>
    Filter::match_type operator()(const T&) const {
        return boost::logic::indeterminate;
    }

};

struct RemoveUsage : public boost::static_visitor<bool> {

    Filter* m_filter;
    bool m_reversed;
    const Handler* m_handler;

    RemoveUsage(Filter* filter, bool reversed, const Handler* handler) :
        m_filter(filter), m_reversed(reversed), m_handler(handler) {

    }

    bool set_filter(const Filter& filter) const {
        *m_filter = m_reversed ? ~filter : filter;
        return true;
    }

    bool set_bool_filter(bool match) const {
        return set_filter(match ? Filter() : ~Filter());
    }

    bool remove_usages(std::vector<Filter>& filters) const {
        bool removed = false;
        for (Filter& filter : filters)
            if (filter.remove_usage(m_handler))
                removed = true;
        return removed;
    }

    template<typename T>
    bool simplify(T filter, bool is_any) const {
        if (!remove_usages(filter.filters))
            return false;
        for (auto it = filter.filters.begin() ; it != filter.filters.end() ; ) {
            auto match = it->match_nothing();
            if (boost::logic::indeterminate(match))
                ++it;
            else if (match == is_any) // true | ... (or) false & ...
                return set_bool_filter(match);
            else // true & ... (or) false | ...
                it = filter.filters.erase(it);
        }
        if (filter.filters.size() == 1)
            return set_filter(std::move(filter.filters.front()));
        return set_filter({std::move(filter)});
    }

    bool operator()(const Filter::HandlerFilter& filter) const {
        if (m_handler != filter.handler)
            return false;
        return set_bool_filter(false);
    }

    bool operator()(const Filter::AllFilter& filter) const {
        return simplify(filter, false);
    }

    bool operator()(const Filter::AnyFilter& filter) const {
        return simplify(filter, true);
    }

    template <typename T>
    bool operator()(const T&) const {
        return false;
    }

};

template<typename T>
struct Merge : public boost::static_visitor<Filter> {

    Filter operator()(T f1, const T& f2) const {
        f1.filters.insert(f1.filters.end(), f2.filters.begin(), f2.filters.end());
        return {f1};
    }

    template <typename T1>
    Filter operator()(const T1& f1, T f2) const {
        f2.filters.emplace(f2.filters.begin(), f1);
        return {f2};
    }

    template <typename T2>
    Filter operator()(T f1, const T2& f2) const {
        f1.filters.emplace_back(f2);
        return {f1};
    }

    template <typename T1, typename T2>
    Filter operator()(const T1& f1, const T2& f2) const {
        return { T{{{f1}, {f2}}} };
    }

};

}

}

Filter Filter::handler(const Handler* handler) {
    return {HandlerFilter{handler}};
}

Filter Filter::track(track_t track) {
    return {TrackFilter{track}};
}

Filter Filter::raw_channels(channels_t channels) {
    return {ChannelFilter{channels}};
}

Filter Filter::families(families_t families) {
    return {FamilyFilter{families}};
}

Filter Filter::channels(channels_t channels) {
    if (channels == all_channels)
        return {};
    return ~families(voice_families) | raw_channels(channels);
}

Filter operator|(const Filter& lhs, const Filter& rhs) {
    if (lhs.m_reversed || rhs.m_reversed)
        return {Filter::AnyFilter{{lhs, rhs}}};
    return boost::apply_visitor(visitors::Merge<Filter::AnyFilter>(), lhs.m_data, rhs.m_data);
}

Filter operator&(const Filter& lhs, const Filter& rhs) {
    if (lhs.m_reversed || rhs.m_reversed)
        return {Filter::AllFilter{{lhs, rhs}}};
    return boost::apply_visitor(visitors::Merge<Filter::AllFilter>(), lhs.m_data, rhs.m_data);
}

Filter operator~(Filter lhs) {
    return {std::move(lhs.m_data), !lhs.m_reversed};
}

Filter::Filter(data_type data, bool reversed) : m_data(std::move(data)), m_reversed(reversed) {

}

bool Filter::remove_usage(const Handler* handler) {
    return boost::apply_visitor(visitors::RemoveUsage(this, m_reversed, handler), m_data);
}

Filter::match_type Filter::match_nothing() const {
    return m_reversed != boost::apply_visitor(visitors::MatchNothing(), m_data);
}

Filter::match_type Filter::match_handler(const Handler* handler) const {
    return m_reversed != boost::apply_visitor(visitors::MatchHandler(handler), m_data);
}

bool Filter::match_message(const Message& message) const {
    return m_reversed != boost::apply_visitor(visitors::Match(message), m_data);
}

std::string Filter::string() const {
    std::stringstream os;
    os << *this;
    return os.str();
}

void Filter::write(std::ostream& stream, bool surround) const {
    switch (match_nothing().value) {
    case Filter::match_type::true_value:
        stream << "true";
        break;
    case Filter::match_type::false_value:
        stream << "false";
        break;
    case Filter::match_type::indeterminate_value:
        if (m_reversed)
            stream << '~';
        boost::apply_visitor(visitors::String(stream, m_reversed || surround), m_data);
        break;
    }
}

std::ostream& operator <<(std::ostream& stream, const Filter& filter) {
    filter.write(stream, false);
    return stream;
}

//=========
// Handler
//=========

Event Handler::open_event(state_type state) {
    return Event::custom({}, "Open", marshall(state));
}

Event Handler::close_event(state_type state) {
    return Event::custom({}, "Close", marshall(state));
}

Handler::Handler(mode_type mode) :
    m_name(), m_mode(mode), m_state(), m_holder(nullptr), m_receiver(nullptr), m_sinks_mutex(), m_sinks() {

}

Handler::~Handler() {
    TRACE_DEBUG("deleting handler " << m_name << " ...");
}

const std::string& Handler::name() const {
    return m_name;
}

void Handler::set_name(std::string name) {
    m_name = std::move(name);
}

Handler::mode_type Handler::mode() const {
    return m_mode;
}

Handler::state_type Handler::state() const {
    return m_state;
}

void Handler::set_state(state_type state) {
    m_state = state;
}

void Handler::alter_state(state_type state, bool on) {
    m_state.commute(state, on);
}

families_t Handler::handled_families() const {
    return all_families;
}

families_t Handler::input_families() const {
    return all_families;
}

Handler::result_type Handler::handle_open(const Message& message) {
    if (message.event.family() == family_t::custom) {
        auto key = message.event.get_custom_key();
        if (key == "Open") {
            on_open(unmarshall<state_type>(message.event.get_custom_value()));
            return result_type::success;
        } else if (key == "Close") {
            on_close(unmarshall<state_type>(message.event.get_custom_value()));
            return result_type::success;
        }
    }
    return result_type::unhandled;
}

Handler::result_type Handler::on_open(state_type state) {
    alter_state(state, true);
    return result_type::success;
}

Handler::result_type Handler::on_close(state_type state) {
    alter_state(state, false);
    return result_type::success;
}

Holder* Handler::holder() const {
    return m_holder;
}

void Handler::set_holder(Holder* holder) {
    m_holder = holder;
}

Receiver* Handler::receiver() const {
    return m_receiver;
}

void Handler::set_receiver(Receiver* receiver) {
    m_receiver = receiver;
}

bool Handler::send_message(const Message& message) {
    return m_holder && m_holder->hold_message(this, message);
}

Handler::result_type Handler::receive_message(const Message& message) {
    try {
        return m_receiver ? m_receiver->receive_message(this, message) : handle_message(message);
    } catch (const std::exception& error) {
        TRACE_ERROR(m_name << " handling exception: " << error.what());
        return result_type::error;
    }
}

Handler::result_type Handler::handle_message(const Message& message) {
    return handle_open(message);
}

Handler::sinks_type Handler::sinks() const {
    std::lock_guard<std::mutex> guard(m_sinks_mutex);
    return m_sinks;
}

void Handler::set_sinks(sinks_type sinks) {
    std::lock_guard<std::mutex> guard(m_sinks_mutex);
    m_sinks = std::move(sinks);
}

void Handler::forward_message(const Message& message) {
    std::lock_guard<std::mutex> guard(m_sinks_mutex);
    for (const auto& pair : m_sinks)
        if (pair.second.match_message(message))
            pair.first->send_message(message);
}

//================
// StandardHolder
//================

#ifndef MIDILAB_MEASUREMENTS

StandardHolder::StandardHolder(priority_t priority, std::string name) :
    Holder(), m_task(512), m_name(std::move(name)) {

    m_task.start(priority, [this](data_type&& data) {
        data.first->receive_message(data.second);
    });
}

StandardHolder::~StandardHolder() {
    m_task.stop();
    TRACE_DEBUG("deleting holder " << m_name << " ...");
}

std::thread::id StandardHolder::get_id() const {
    return m_task.get_id();
}

const std::string& StandardHolder::name() const {
    return m_name;
}

void StandardHolder::set_name(std::string name) {
    m_name = std::move(name);
}

bool StandardHolder::hold_message(Handler* target, const Message& message) {
    return m_task.push(data_type{target, message});
}

#else

StandardHolder::StandardHolder(priority_t priority, std::string name) :
    Holder(), m_task(512), m_name(std::move(name)) {

    reset(clock_type::now());

    m_task.start(priority, [this](data_type&& data) {
        feed(std::move(std::get<2>(data)));
        std::get<0>(data)->receive_message(std::get<1>(data));
    });
}

StandardHolder::~StandardHolder() {
    m_task.stop();
    TRACE_DEBUG("deleting holder " << m_name << " ...");
}

std::thread::id StandardHolder::get_id() const {
    return m_task.get_id();
}

const std::string& StandardHolder::name() const {
    return m_name;
}

void StandardHolder::set_name(std::string name) {
    m_name = std::move(name);
}

void StandardHolder::feed(time_type time) {
    auto now = clock_type::now();
    m_delta += std::chrono::duration_cast<decltype(m_delta)>(now - time);
    m_count++;
    if (now > m_reference + std::chrono::seconds(3)) {
        auto mean_delta = m_delta / m_count;
        if (mean_delta > std::chrono::microseconds(75))
            TRACE_INFO(m_name << " " << mean_delta.count() << " us");
        reset(now);
    }
}

void StandardHolder::reset(time_type time) {
    m_count = 0;
    m_delta = std::chrono::microseconds::zero();
    m_reference = std::move(time);
}

bool StandardHolder::hold_message(Handler* target, const Message& message) {
    return m_task.push(data_type{target, message, clock_type::now()});
}

#endif
