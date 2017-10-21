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
 * A Filter contains the list of all sources allowed to forward a message to a sink
 *
 * @todo ...
 * * avoid inserting duplicates
 * * implement operator== to help simplification
 */

class Filter {

public:
    struct HandlerFilter { const Handler* handler; };
    struct TrackFilter { track_t track; };
    struct ChannelFilter { channels_t channels; };
    struct FamilyFilter { families_t families; };
    struct AnyFilter { std::vector<Filter> filters; };
    struct AllFilter { std::vector<Filter> filters; };

    using match_type = boost::logic::tribool;

    using data_type = boost::variant<HandlerFilter, TrackFilter, ChannelFilter, FamilyFilter, AnyFilter, AllFilter>;

    // -----------------
    // creation features
    // -----------------

    static Filter handler(const Handler* handler);
    static Filter track(track_t track);
    static Filter raw_channels(channels_t channels);
    static Filter channels(channels_t channels);
    static Filter families(families_t families);

    friend Filter operator |(const Filter& lhs, const Filter& rhs); /*!< match any */
    friend Filter operator &(const Filter& lhs, const Filter& rhs); /*!< match all */
    friend Filter operator ~(Filter lhs); /*!< unmatch filter */

    // --------------------
    // combination features
    // --------------------

    Filter(data_type data = AllFilter{}, bool reversed = false); /*!< always match by default */

    // --------------------
    // combination features
    // --------------------

    bool remove_usage(const Handler* handler); /*!< remove all occurences of handler */

    // --------
    // matching
    // --------

    match_type match_nothing() const; /*!< true if always true, false if always false, else undetermined */
    match_type match_handler(const Handler* handler) const;
    // match_type match_track(track_t track) const;
    // match_type match_channels(channels_t channels) const;
    // match_type match_families(family_t families) const;
    // match_type match_source(const Source& source) const;

    bool match_message(const Message& message) const;

    // -----------------
    // string conversion
    // -----------------

    void write(std::ostream& stream, bool surround) const;

    std::string string() const;

    friend std::ostream& operator <<(std::ostream& stream, const Filter& filter);

private:
    data_type m_data;
    bool m_reversed; /*!< if true, match if data does not match */

};

//=========
// Handler
//=========

#define MIDI_HANDLE_OPEN do { if (handle_open(message) == ::Handler::result_type::success) return ::Handler::result_type::success; } while (false)
#define MIDI_CHECK_OPEN_RECEIVE do { if (state().none(::handler_ns::receive_state)) return ::Handler::result_type::closed; } while (false)
#define MIDI_CHECK_OPEN_FORWARD_RECEIVE do { if (!state().all(::handler_ns::endpoints_state)) return ::Handler::result_type::closed; } while (false)

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
 * A handler is an object capable to process data asynchronously (via send)
 * Input messages are buffered in a holder object
 * The result of the operation is encapsulated in a future
 *
 * As a serializable object, it also defines a type that describes
 * which dynamic parameters are supported ...
 *
 * @warning changing the receiver while processing messages can be hazardous
 *
 * @todo implement a notification system instead of the receiver ?
 *
 */

namespace handler_ns {

// mode enable to check type of handler
// mode is basically used to give extra information about the handler, not to check !
// mode is guaranteed to be constant for each handler
// in_mode: handler can generate events
// thru_mode: handler can forward messages (on reception)
// out_mode: handler consume received messages (no forwarding)
// no mode checking is used to prevent usage
// mode is a read-only

using mode_t = flags_t<unsigned>;
static constexpr mode_t in_mode = mode_t::from_integral(0x1); /*!< can generate events */
static constexpr mode_t out_mode = mode_t::from_integral(0x2); /*!< can handle events */
static constexpr mode_t thru_mode = mode_t::from_integral(0x4); /*!< can forward on handling */
static constexpr mode_t io_mode = in_mode | out_mode;
static constexpr mode_t forward_mode = in_mode | thru_mode;
static constexpr mode_t receive_mode = out_mode | thru_mode;

// the handler can store different boolean states
// handlers use two different states to enable forwarding and receiving messages
// @note to forward/receive an event, the respective states must be open
// @warning messages will be handled even if the handler is closed, you must check it.

using state_t = flags_t<unsigned>;
static constexpr state_t forward_state = state_t::from_integral(0x1);
static constexpr state_t receive_state = state_t::from_integral(0x2);
static constexpr state_t endpoints_state = forward_state | receive_state; /*!< @note junction may be a better name */

}

class Handler {

public:
    using mode_type = handler_ns::mode_t;
    using state_type = handler_ns::state_t;
    using sinks_type = std::unordered_map<Handler*, Filter>;

    enum class result_type {
        success, /*!< handling was successful */
        fail, /*!< handling failed (general purpose) */
        unhandled, /*!< handling failed (event is not supposed to be handled) */
        closed,/*!< handling failed because handler was closed */
        error  /*!< an exception has been thrown while handling */
    };

    static Event open_event(state_type state); /*!< Specific action with key "Open" */
    static Event close_event(state_type state); /*!< Specific action with key "Close" */

    explicit Handler(mode_type mode);
    virtual ~Handler();

    Handler(const Handler&) = delete;

    const std::string& name() const;
    void set_name(std::string name);

    mode_type mode() const; /*!< get handler's mode (it is guaranteed to be constant) */

    state_type state() const; /*!< get current state */
    void set_state(state_type state); /*!< replace all states */
    void alter_state(state_type state, bool on); /*! set all states specified to on or off leaving others */

    /**
     * @return the families used when handling a message (default accept any event)
     * @note this is just a hint, any event type can be received
     * @note mode & state are not checked at this point
     * @todo delegate to the meta class
     */

    virtual families_t handled_families() const;
    virtual families_t input_families() const;

    result_type handle_open(const Message& message);
    virtual result_type on_open(state_type state);
    virtual result_type on_close(state_type state);

    // ------------------
    // reception features
    // ------------------

    Holder* holder() const;
    void set_holder(Holder* holder);

    Receiver* receiver() const;
    void set_receiver(Receiver* receiver);

    bool send_message(const Message& message);
    result_type receive_message(const Message& message);
    virtual result_type handle_message(const Message& message); /*!< endpoint of messages, by default handles open/close actions */

    // -------------------
    // forwarding features
    // -------------------

    sinks_type sinks() const;
    void set_sinks(sinks_type sinks);

    void forward_message(const Message& message); /*!< send message to all sinks matching their filters, returns number of times the message is forwarded */

private:
    std::string m_name;
    const mode_type m_mode;
    state_type m_state;
    Holder* m_holder; /*!< delegate synchronizing messages */
    Receiver* m_receiver; /*!< delegate handling synchronized messages */
    mutable std::mutex m_sinks_mutex;
    sinks_type m_sinks;

};

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
    using result_type = Handler::result_type;

    virtual ~Receiver() = default;
    virtual result_type receive_message(Handler* target, const Message& message) = 0;

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

#ifdef NDEBUG

class StandardHolder : public Holder {

public:
    using data_type = std::pair<Handler*, Message>;
    using task_type = task_t<data_type>;

    StandardHolder(priority_t priority, std::string name = {});
    ~StandardHolder();

    std::thread::id get_id() const;

    const std::string& name() const;
    void set_name(std::string name);

    bool hold_message(Handler* target, const Message& message) override;

private:
     task_type m_task;
     std::string m_name;

};

#else

class StandardHolder : public Holder {

public:
    using clock_type = Clock::clock_type;
    using time_type = Clock::time_type;
    using data_type = std::tuple<Handler*, Message, time_type>;
    using task_type = task_t<data_type>;

    StandardHolder(priority_t priority, std::string name = {});
    ~StandardHolder();

    std::thread::id get_id() const;

    const std::string& name() const;
    void set_name(std::string name);

    void feed(time_type time);
    void reset(time_type time);

    bool hold_message(Handler* target, const Message& message) override;

private:
     task_type m_task;
     std::string m_name;
     time_type m_reference; /*!< time reference */
     std::chrono::microseconds m_delta; /*!< deltatime accumulation */
     size_t m_count; /*!< number of events fed since the last reset */

};

#endif

#endif // CORE_HANDLER_H
