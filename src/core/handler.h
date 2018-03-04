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

#ifndef CORE_HANDLER_H
#define CORE_HANDLER_H

#include <chrono>
#include <boost/logic/tribool.hpp>
#include <boost/variant.hpp>
#include "tools/containers.h"
#include "tools/trace.h"
#include "tools/concurrency.h"
#include "event.h"
#include "sequence.h"

class Receiver;
class Holder;
class Handler;

//=========
// Message
//=========

/**
 * A message is the main component used by handlers to manage events.
 * It basically adds meta data such as:
 * @li the absolute time of generation to provide fine resolution (depending on system)
 * @li the source of the event (the handler the first generated the event)
 *
 * @todo delete track and handle track filtering in the SequenceReader
 * or keep it and take profit of this additional parameter to implement
 * guitar strings recognition ?
 *
 */

struct Message final {

    static const track_t no_track = 0;

    Message(Event event = {}, Handler* source = nullptr, track_t track = no_track);

    Event event; /*!< actual event to be handled */
    Handler* source; /*!< first producer of the event */
    track_t track; /*!< the track within the source containing the event */

};

//========
// Filter
//========

/**
 * A filter contains rules that helps the categorization of messages.
 * It can be used to discriminate
 *  @li the message's source
 *  @li the message's track
 *  @li the event's family
 *  @li the event's channels
 *  @li any combination of the rules above
 *
 */

class Filter;

struct filter_traits {
    struct HandlerFilter { const Handler* handler; bool reversed; };
    struct TrackFilter { track_t track; bool reversed; };
    struct ChannelFilter { channels_t channels; };
    struct FamilyFilter { families_t families; };
    struct AnyFilter { std::vector<Filter> filters; };
    struct AllFilter { std::vector<Filter> filters; };

    using data_type = boost::variant<HandlerFilter, TrackFilter, ChannelFilter, FamilyFilter, AnyFilter, AllFilter>;

    using match_type = boost::logic::tribool;

};

class Filter final : public filter_traits {

public:

    //-----------------
    // data definition
    //-----------------

    template<typename T>
    struct visitor_type : public boost::static_visitor<T>, public filter_traits {};

    //-------------------
    // creation features
    //-------------------

    static Filter handler(const Handler* handler);
    static Filter track(track_t track);
    static Filter channels(channels_t channels);
    static Filter families(families_t families);

    friend Filter operator|(const Filter& lhs, const Filter& rhs); /*!< match any */
    friend Filter operator&(const Filter& lhs, const Filter& rhs); /*!< match all */
    friend Filter operator~(const Filter& lhs); /*!< unmatch filter */

    //---------------------
    // conversion features
    //---------------------

    Filter(data_type data = AllFilter{}); /*!< always match by default */

    operator const data_type&() const;
    operator data_type&();

    //---------------------
    // comparison features
    //---------------------

    friend bool operator==(const Filter& lhs, const Filter& rhs); /*!< two filters are equal if they cover each other */
    friend bool operator!=(const Filter& lhs, const Filter& rhs); /*!< @see operator== */

    bool covers(const Filter& rhs) const; /*!< true if lhs is more permissible than rhs */

    //-------------------
    // matching features
    //-------------------

    match_type match_nothing() const; /*!< true if always true, false if always false, else undetermined */
    match_type match_handler(const Handler* handler) const;
    // match_type match_track(track_t track) const;
    // match_type match_channels(channels_t channels) const;
    // match_type match_families(family_t families) const;

    bool match_message(const Message& message) const;

    //-----------------
    // string features
    //-----------------

    std::string string() const;

    friend std::ostream& operator<<(std::ostream& stream, const Filter& filter);

    // ------------------
    // mutation features
    // ------------------

    bool remove_usage(const Handler* handler); /*!< remove all occurences of handler */

private:
    data_type m_data;

};

//==========
// Listener
//==========

/**
 * A listener represents a handler that will receive messages forwarded from another one.
 * It is associated to a filter that will allow selecting the messages truly received
 *
 */

struct Listener final {
    Handler* handler;
    Filter filter;
};

//==========
// Listener
//==========

/**
 * The Listeners class contains a collection of all listeners attached to a handler.
 * It corresponds roughly to a multimap {Handler => Filter}
 *
 * @note Handlers referenced multiple times will receive as many messages.
 *
 */

class Listeners {

public:
    using listeners_type = std::vector<Listener>;
    using const_iterator = listeners_type::const_iterator;
    using iterator = listeners_type::iterator;

    bool empty() const; /*!< check if the list of listeners is empty */
    size_t size() const; /*!< count the number of existing listeners */
    size_t count(const Handler* handler) const; /*!< count the number of times that the handler is referenced */
    const_iterator find(const Handler* handler) const; /*!< find the first listener mapped to the given handler */
    iterator find(const Handler* handler); /*!< @see find */

    size_t insert(Handler* handler, const Filter& filter); /*!< insert a new listener or merge filters if already inserted */
    size_t erase(const Handler* handler); /*!< remove all listeners mapped to the given handler */
    size_t clear(); /*!< remove all listeners */

    size_t sanitize(); /*!< remove all listeners for which the filter never matches */
    size_t remove_usage(const Handler* handler); /*!< remove occurences of handler from listeners and all filters */
    size_t remove_usage(const Handler* handler, const Handler* source); /*!< remove occurences of source in the filters of listeners mapped to handler */

    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

private:
    listeners_type m_listeners;

};

//=========
// Handler
//=========

#define MIDI_HANDLE_OPEN do { if (handle_open(message) == ::Handler::Result::success) return ::Handler::Result::success; } while (false)
#define MIDI_CHECK_OPEN_RECEIVE do { if (state().none(::Handler::State::receive())) return ::Handler::Result::closed; } while (false)
#define MIDI_CHECK_OPEN_FORWARD_RECEIVE do { if (!state().all(::Handler::State::endpoints())) return ::Handler::Result::closed; } while (false)

/**
 * The Handler class is the minimum interface required to deal with midi events.
 * A handler has a few attributes such as :
 * * Mode : What role(s) can play the handler
 * * State : Runtime information about constraints
 *
 * Some behaviors are delegated:
 * * How to synchronize incoming messages ? -> Holder
 * * Inject behavior when a message is processed ? -> Receiver
 *
 * @warning changing the receiver while processing messages can be hazardous
 *
 * @todo implement a notification system instead of the receiver ?
 *
 */

class Handler {

public:

    // -----
    // types
    // -----

    // each handler has a mode associated which must be one of:
    // - in: handler can generate messages
    // - out: handler can receive messages
    // - thru: handler can forward messages (on reception)
    // - io: handler can both generate and receive messages
    // it is guaranteed to be constant for each handler

    struct Mode : flags_t<Mode, size_t, 8> {
        using flags_t::flags_t;

        static constexpr auto in() { return from_integral(0x1); } /*!< can generate events */
        static constexpr auto out() { return from_integral(0x2); } /*!< can handle events */
        static constexpr auto thru() { return from_integral(0x4); } /*!< can forward on handling */
        static constexpr auto io() { return in() | out(); }

        static constexpr auto forward() { return in() | thru(); }
        static constexpr auto receive() { return out() | thru(); }

    };

    // the handler can store different boolean states
    // handlers use two different states to enable forwarding and receiving messages
    // @note to forward/receive an event, the respective states must be open
    // @warning messages will be handled even if the handler is closed, you must check it.

    struct State : flags_t<State, size_t, 8> {
        using flags_t::flags_t;

        static constexpr auto forward() { return from_integral(0x1); }
        static constexpr auto receive() { return from_integral(0x2); }
        static constexpr auto endpoints() { return forward() | receive(); } /*!< @note junction may be a better name */

    };

    enum class Result {
        success, /*!< handling was successful */
        fail, /*!< handling failed (general purpose) */
        unhandled, /*!< handling failed (event is not supposed to be handled) */
        closed,/*!< handling failed because handler was closed */
        error  /*!< an exception has been thrown while handling */
    };

    static Event open_event(State state); /*!< Specific action with key "Open" */
    static Event close_event(State state); /*!< Specific action with key "Close" */

    // ---------
    // structors
    // ---------

    explicit Handler(Mode mode);
    Handler(const Handler&) = delete;
    virtual ~Handler();

    const std::string& name() const;
    void set_name(std::string name);

    Mode mode() const; /*!< get handler's mode (it is guaranteed to be constant) */

    State state() const; /*!< get current state */
    void set_state(State state); /*!< replace all states */
    void alter_state(State state, bool on); /*! set all states specified to on or off leaving others */

    /**
     * @return the families used when handling a message (default accept any event)
     * @note this is just a hint, any event type can be received
     * @note mode & state are not checked at this point
     * @todo delegate to the meta class
     */

    virtual families_t handled_families() const;
    virtual families_t input_families() const;

    Result handle_open(const Message& message);
    virtual Result on_open(State state);
    virtual Result on_close(State state);

    // ------------------
    // reception features
    // ------------------

    Holder* holder() const;
    void set_holder(Holder* holder);

    Receiver* receiver() const;
    void set_receiver(Receiver* receiver);

    bool send_message(const Message& message);
    Result receive_message(const Message& message);
    virtual Result handle_message(const Message& message); /*!< endpoint of messages, by default handles open/close actions */

    // -------------------
    // forwarding features
    // -------------------

    Listeners listeners() const;
    void set_listeners(Listeners listeners);

    void forward_message(const Message& message); /*!< send message to all listeners matching their filters, returns number of times the message is forwarded */

private:
    std::string m_name;
    const Mode m_mode;
    State m_state;
    Holder* m_holder; /*!< delegate synchronizing messages */
    Receiver* m_receiver; /*!< delegate handling synchronized messages */
    mutable std::mutex m_listeners_mutex;
    Listeners m_listeners;

};

template<> inline auto marshall<Handler::State>(Handler::State state) { return marshall(state.to_integral()); }
template<> inline auto unmarshall<Handler::State>(const std::string& string) { return Handler::State::from_integral(unmarshall<Handler::State::storage_type>(string)); }

//==========
// Receiver
//==========

/**
 * The Receiver is the delegate that will be able to catch messages sent
 * to the handler
 *
 */

class Receiver {

public:
    using Result = Handler::Result;

    virtual ~Receiver() = default;
    virtual Result receive_message(Handler* target, const Message& message) = 0;

};

//========
// Holder
//========

/**
 * The Holder is the delegate responsible to delay the reception of incoming messages
 * of a Handler
 */

class Holder {

public:
    virtual ~Holder() = default;
    virtual bool hold_message(Handler* target, const Message& message) = 0;

};

//================
// StandardHolder
//================

/**
  * The StandardHolder is an implementation using an internal message queue
  *
  */

class StandardHolder : public Holder {

public:
    using clock_type = Clock::clock_type;
    using time_type = Clock::time_type;
#ifdef MIDILAB_ENABLE_TIMING
    using data_type = std::tuple<Handler*, Message, time_type>;
#else
    using data_type = std::pair<Handler*, Message>;
#endif
    using task_type = task_t<data_type>;

    StandardHolder(std::string name = {});

    void start(priority_t priority); /*!< (re)activate message processing */
    void stop(); /*!< deactivate message processing */

    std::thread::id get_id() const;

    const std::string& name() const;
    void set_name(std::string name);

#ifdef MIDILAB_ENABLE_TIMING
    void feed(time_type time);
    void reset(time_type time);
#endif

    bool hold_message(Handler* target, const Message& message) override;

private:
    task_type m_task;
    std::string m_name;
#ifdef MIDILAB_ENABLE_TIMING
    time_type m_reference; /*!< time reference */
    std::chrono::microseconds m_delta; /*!< deltatime accumulation */
    size_t m_count; /*!< number of events fed since the last reset */
#endif

};

#endif // CORE_HANDLER_H
