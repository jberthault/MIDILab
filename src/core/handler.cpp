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
#include <boost/optional.hpp>
#include "handler.h"
#include "note.h"

namespace  {

template<typename It, typename P>
auto tribool_all_of(It first, It last, P predicate) {
    using namespace boost::logic;
    tribool result(true);
    for ( ; first != last ; ++first) {
        switch (predicate(*first).value) {
            case tribool::true_value: break;
            case tribool::false_value: return tribool(false);
            case tribool::indeterminate_value: result = indeterminate; break;
        }
    }
    return result;
}

template<typename It, typename P>
auto tribool_any_of(It first, It last, P predicate) {
    using namespace boost::logic;
    tribool result(false);
    for ( ; first != last ; ++first) {
        switch (predicate(*first).value) {
            case tribool::true_value: return tribool(true);
            case tribool::false_value: break;
            case tribool::indeterminate_value: result = indeterminate; break;
        }
    }
    return result;
}

template<typename T, typename P>
size_t erase_if(T& collection, P predicate) {
    auto first = std::begin(collection);
    auto last = std::end(collection);
    auto it = std::remove_if(first, last, predicate);
    auto count = std::distance(it, last);
    collection.erase(it, last);
    return count;
}

}

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

auto negate_all(const std::vector<Filter>& filters) {
    std::vector<Filter> result(filters.size());
    std::transform(filters.begin(), filters.end(), result.begin(), [](const auto& filter) { return ~filter; });
    return result;
}

struct Negate : public Filter::visitor_type<Filter::data_type> {

    auto operator()(const HandlerFilter& f) const { return HandlerFilter{f.handler, !f.reversed}; }
    auto operator()(const TrackFilter& f) const { return TrackFilter{f.track, !f.reversed}; }
    auto operator()(const ChannelFilter& f) const { return ChannelFilter{all_channels & ~f.channels}; }
    auto operator()(const FamilyFilter& f) const { return FamilyFilter{family_ns::all_families & ~f.families}; }
    auto operator()(const AnyFilter& f) const { return AllFilter{negate_all(f.filters)}; }
    auto operator()(const AllFilter& f) const { return AnyFilter{negate_all(f.filters)}; }

};

struct Match : public Filter::visitor_type<bool> {

    const Message& message;

    Match(const Message& message) : message(message) { }

    bool visit(const data_type& f) const { return apply_visitor(*this, f); }

    bool operator()(const HandlerFilter& f) const { return (message.source == f.handler) ^ f.reversed; }
    bool operator()(const TrackFilter& f) const { return (message.track == f.track) ^ f.reversed; }
    bool operator()(const ChannelFilter& f) const { return f.channels.all(message.event.channels()); }
    bool operator()(const FamilyFilter& f) const { return f.families.contains(message.event.family()); }
    bool operator()(const AnyFilter& f) const { return std::any_of(f.filters.begin(), f.filters.end(), [this](const auto& filter) { return visit(filter); }); }
    bool operator()(const AllFilter& f) const { return std::all_of(f.filters.begin(), f.filters.end(), [this](const auto& filter) { return visit(filter); }); }

};

struct MatchNothing : public Filter::visitor_type<Filter::match_type> {

    match_type operator()(const AnyFilter& f) const { return !f.filters.empty() && match_type(boost::logic::indeterminate); }
    match_type operator()(const AllFilter& f) const { return f.filters.empty() || match_type(boost::logic::indeterminate); }

    template<typename T> match_type operator()(const T&) const { return boost::logic::indeterminate; }

};

struct MatchHandler : public Filter::visitor_type<Filter::match_type> {

    const Handler* handler;

    MatchHandler(const Handler* handler) : handler(handler) { }

    match_type visit(const data_type& f) const { return apply_visitor(*this, f); }

    match_type operator()(const HandlerFilter& f) const { return (handler == f.handler) ^ f.reversed; }
    match_type operator()(const AnyFilter& f) const { return tribool_any_of(f.filters.begin(), f.filters.end(), [this](const auto& filter) { return visit(filter); }); }
    match_type operator()(const AllFilter& f) const { return tribool_all_of(f.filters.begin(), f.filters.end(), [this](const auto& filter) { return visit(filter); }); }

    template<typename T> match_type operator()(const T&) const { return boost::logic::indeterminate; }

};

struct Write : public Filter::visitor_type<void> {

    std::ostream& stream;
    bool surround;

    Write(std::ostream& stream, bool surround = false) : stream(stream), surround(surround) { }

    void visit(const data_type& f) const { apply_visitor(*this, f); }

    void write_filters(const std::vector<Filter>& filters, const char* separator, const char* default_text) const {
        if (filters.empty()) {
            stream << default_text;
        } else {
            const Write visitor(stream, true);
            if (surround)
                stream << '(';
            auto it = filters.begin(), last = filters.end();
            visitor.visit(*it);
            for (++it ; it != last ; ++it) {
                stream << separator;
                visitor.visit(*it);
            }
            if (surround)
                stream << ")";
        }
    }

    void operator()(const HandlerFilter& f) const {
        if (f.reversed)
            stream << '~';
        stream << '"' << f.handler->name() << '"';
    }

    void operator()(const AnyFilter& f) const { write_filters(f.filters, " | ", "false"); }
    void operator()(const AllFilter& f) const { write_filters(f.filters, " & ", "true"); }

    template<typename T> void operator()(const T&) const { stream << "<unimplemented>"; }

};

struct RemoveUsage : public Filter::visitor_type<boost::optional<Filter::data_type>> {

    const Handler* m_handler;

    RemoveUsage(const Handler* handler) : m_handler(handler) { }

    size_t apply(data_type& data) const {
        auto new_data = apply_visitor(*this, data);
        if (new_data) {
            data = std::move(*new_data);
            return 1;
        }
        return 0;
    }

    template<typename It>
    size_t apply_many(It first, It last) const {
        size_t n = 0;
        for ( ; first != last ; ++first)
            n += apply(*first);
        return n;
    }

    static data_type always(bool match) {
        if (match)
            return {AllFilter{}};
        return {AnyFilter{}};
    }

    template<typename T>
    static data_type simplify_singleton(const T& f) {
        if (f.filters.size() == 1)
            return f.filters.front();
        return {f};
    }

    template<typename T>
    static boost::optional<data_type> simplify(T f, bool is_any) {
        for (auto it = f.filters.begin() ; it != f.filters.end() ; ) {
            auto match = it->match_nothing();
            if (boost::logic::indeterminate(match))
                ++it;
            else if (match == is_any) // true | ... (or) false & ...
                return {always(match)};
            else // true & ... (or) false | ...
                it = f.filters.erase(it);
        }
        return {simplify_singleton(f)};
    }

    template<typename T>
    boost::optional<data_type> remove_usage(T f, bool is_any) const {
        if (apply_many(f.filters.begin(), f.filters.end()) == 0)
            return {};
        return simplify(std::move(f), is_any);
    }

    boost::optional<data_type> operator()(const HandlerFilter& f) const {
        if (m_handler != f.handler)
            return {};
        return {always(f.reversed)};
    }

    boost::optional<data_type> operator()(const AnyFilter& f) const { return remove_usage(f, true); }
    boost::optional<data_type> operator()(const AllFilter& f) const { return remove_usage(f, false); }

    template <typename T> boost::optional<data_type> operator()(const T&) const { return {}; }

};

template<typename T>
struct Merge : public Filter::visitor_type<Filter> {

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

struct Cover : public Filter::visitor_type<bool> {

    bool apply(const data_type& f1, const data_type& f2) const {
        return apply_visitor(*this, f1, f2);
    }

    //-----------------
    // covers if equal
    // ----------------

    bool operator()(const HandlerFilter& f1, const HandlerFilter& f2) const {
        return f1.handler == f2.handler ? f1.reversed == f2.reversed : f1.reversed && !f2.reversed;
    }

    bool operator()(const TrackFilter& f1, const TrackFilter& f2) const {
        return f1.track == f2.track ? f1.reversed == f2.reversed : f1.reversed && !f2.reversed;
    }

    //--------------------
    // covers if contains
    // -------------------

    bool operator()(const ChannelFilter& f1, const ChannelFilter& f2) const {
        return f1.channels.all(f2.channels);
    }

    bool operator()(const FamilyFilter& f1, const FamilyFilter& f2) const {
        return f1.families.all(f2.families);
    }

    //--------------------
    // covers if contains
    // -------------------

    /**
     *
     * For "A covers B" written as "A > B" :
     *
     * A | B > X  if  A > X   or  B > X
     * A & B > X  if  A > X  and  B > X
     * X > A | B  if  X > A  and  X > B
     * X > A & B  if  X > A   or  X > B
     *
     */

    bool any_covers(const AnyFilter& lhs, const data_type& rhs) const { return std::any_of(lhs.filters.begin(), lhs.filters.end(), [&](const auto& filter) { return apply(filter, rhs); }); }
    bool all_covers(const AllFilter& lhs, const data_type& rhs) const { return std::all_of(lhs.filters.begin(), lhs.filters.end(), [&](const auto& filter) { return apply(filter, rhs); }); }
    bool covers_all(const data_type& lhs, const AnyFilter& rhs) const { return std::all_of(rhs.filters.begin(), rhs.filters.end(), [&](const auto& filter) { return apply(lhs, filter); }); }
    bool covers_any(const data_type& lhs, const AllFilter& rhs) const { return std::any_of(rhs.filters.begin(), rhs.filters.end(), [&](const auto& filter) { return apply(lhs, filter); }); }

    template<typename T2> bool operator()(const AnyFilter& f1, const T2& f2) const { return any_covers(f1, f2); }
    template<typename T2> bool operator()(const AllFilter& f1, const T2& f2) const { return all_covers(f1, f2); }
    template<typename T1> bool operator()(const T1& f1, const AnyFilter& f2) const { return covers_all(f1, f2); }
    template<typename T1> bool operator()(const T1& f1, const AllFilter& f2) const { return covers_any(f1, f2); }

    // for disambiguation
    bool operator()(const AnyFilter& f1, const AnyFilter& f2) const { return any_covers(f1, f2); }
    bool operator()(const AnyFilter& f1, const AllFilter& f2) const { return any_covers(f1, f2); }
    bool operator()(const AllFilter& f1, const AnyFilter& f2) const { return all_covers(f1, f2); }
    bool operator()(const AllFilter& f1, const AllFilter& f2) const { return all_covers(f1, f2); }

    template <typename T1, typename T2> bool operator()(const T1&, const T2&) const { return false; }

};

}

}

Filter Filter::handler(const Handler* handler) {
    return {HandlerFilter{handler, false}};
}

Filter Filter::track(track_t track) {
    return {TrackFilter{track, false}};
}

Filter Filter::channels(channels_t channels) {
    return {ChannelFilter{channels}};
}

Filter Filter::families(families_t families) {
    return {FamilyFilter{families}};
}

Filter operator|(const Filter& lhs, const Filter& rhs) {
    return boost::apply_visitor(visitors::Merge<Filter::AnyFilter>(), lhs.m_data, rhs.m_data);
}

Filter operator&(const Filter& lhs, const Filter& rhs) {
    return boost::apply_visitor(visitors::Merge<Filter::AllFilter>(), lhs.m_data, rhs.m_data);
}

Filter operator~(const Filter& lhs) {
    return boost::apply_visitor(visitors::Negate(), lhs.m_data);
}

Filter::Filter(data_type data) : m_data(std::move(data)) {

}

Filter::operator const data_type&() const {
    return m_data;
}

Filter::operator data_type&() {
    return m_data;
}

bool operator==(const Filter& lhs, const Filter& rhs) {
    return lhs.covers(rhs) && rhs.covers(lhs);
}

bool operator!=(const Filter& lhs, const Filter& rhs) {
    return !(lhs == rhs);
}

bool Filter::covers(const Filter& rhs) const {
    return boost::apply_visitor(visitors::Cover(), m_data, rhs.m_data);
}

bool Filter::remove_usage(const Handler* handler) {
    return visitors::RemoveUsage(handler).apply(m_data) != 0;
}

Filter::match_type Filter::match_nothing() const {
    return boost::apply_visitor(visitors::MatchNothing(), m_data);
}

Filter::match_type Filter::match_handler(const Handler* handler) const {
    return boost::apply_visitor(visitors::MatchHandler(handler), m_data);
}

bool Filter::match_message(const Message& message) const {
    return boost::apply_visitor(visitors::Match(message), m_data);
}

std::string Filter::string() const {
    std::stringstream os;
    boost::apply_visitor(visitors::Write(os), m_data);
    return os.str();
}

std::ostream& operator<<(std::ostream& stream, const Filter& filter) {
    boost::apply_visitor(visitors::Write(stream), filter.m_data);
    return stream;
}

//==========
// Listener
//==========

bool Listeners::empty() const {
    return m_listeners.empty();
}

size_t Listeners::size() const {
    return m_listeners.size();
}

size_t Listeners::count(const Handler* handler) const {
    return std::count_if(begin(), end(), [=](const auto& listener) { return listener.handler == handler; });
}

Listeners::const_iterator Listeners::find(const Handler* handler) const {
    return std::find_if(begin(), end(), [=](const auto& listener) { return listener.handler == handler; });
}

Listeners::iterator Listeners::find(const Handler* handler) {
    return std::find_if(begin(), end(), [=](const auto& listener) { return listener.handler == handler; });
}

size_t Listeners::insert(Handler* handler, const Filter& filter) {
    auto it = find(handler);
    if (it == end()) { // the filter must be inserted
        m_listeners.push_back({handler, filter});
        return 1;
    }
    if (it->filter.covers(filter)) // the filter doesn't have to change
        return 0;
    if (filter.covers(it->filter)) // the filter can be fully replaced
        it->filter = filter;
    else // both filters must be merged
        it->filter = it->filter | filter;
    return 1;
}

size_t Listeners::erase(const Handler* handler) {
    return erase_if(m_listeners, [=](const auto& listener) { return listener.handler == handler; });
}

size_t Listeners::remove_usage(const Handler* handler, const Handler* source) {
    size_t n = 0;
    for (auto& listener : m_listeners)
        if (listener.handler == handler && listener.filter.remove_usage(source))
            ++n;
    n += sanitize();
    return n;
}

size_t Listeners::clear() {
    size_t n = size();
    m_listeners.clear();
    return n;
}

size_t Listeners::sanitize() {
    return erase_if(m_listeners, [](const auto& listener) { return !listener.filter.match_nothing(); });
}

size_t Listeners::remove_usage(const Handler* handler) {
    size_t n = erase(handler);
    for (auto& listener : m_listeners)
        if (listener.filter.remove_usage(handler))
            ++n;
    n += sanitize();
    return n;
}

Listeners::const_iterator Listeners::begin() const {
    return m_listeners.begin();
}

Listeners::const_iterator Listeners::end() const {
    return m_listeners.end();
}

Listeners::iterator Listeners::begin() {
    return m_listeners.begin();
}

Listeners::iterator Listeners::end() {
    return m_listeners.end();
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
    m_name(), m_mode(mode), m_state(), m_holder(nullptr), m_receiver(nullptr), m_listeners_mutex(), m_listeners() {

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
    return family_ns::all_families;
}

families_t Handler::input_families() const {
    return family_ns::all_families;
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

Listeners Handler::listeners() const {
    std::lock_guard<std::mutex> guard(m_listeners_mutex);
    return m_listeners;
}

void Handler::set_listeners(Listeners listeners) {
    std::lock_guard<std::mutex> guard(m_listeners_mutex);
    m_listeners = std::move(listeners);
}

void Handler::forward_message(const Message& message) {
    std::lock_guard<std::mutex> guard(m_listeners_mutex);
    for (const auto& listener : m_listeners)
        if (listener.filter.match_message(message))
            listener.handler->send_message(message);
}

//================
// StandardHolder
//================

StandardHolder::StandardHolder(std::string name) : Holder(), m_task(512), m_name(std::move(name)) {

}

void StandardHolder::start(priority_t priority) {
#ifdef MIDILAB_ENABLE_TIMING
    reset(clock_type::now());
    m_task.start(priority, [this](data_type&& data) {
        feed(std::move(std::get<2>(data)));
        std::get<0>(data)->receive_message(std::get<1>(data));
    });
#else
    m_task.start(priority, [](data_type&& data) {
        data.first->receive_message(data.second);
    });
#endif
}

void StandardHolder::stop() {
    m_task.stop(true);
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

#ifdef MIDILAB_ENABLE_TIMING

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

#endif

bool StandardHolder::hold_message(Handler* target, const Message& message) {
#ifdef MIDILAB_ENABLE_TIMING
    return m_task.push(data_type{target, message, clock_type::now()});
#else
    return m_task.push(data_type{target, message});
#endif
}
