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

#ifndef TOOLS_CONCURRENCY_H
#define TOOLS_CONCURRENCY_H

#include <cassert>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

//==========
// Priority
//==========

/**
 * Portable way to change the priority of a thread
 */

enum class priority_t {
    idle,
    lowest,
    low,
    normal,
    high,
    highest,
    realtime,
};

std::ostream& operator<<(std::ostream& stream, priority_t priority);

priority_t get_thread_priority();

priority_t get_thread_priority(std::thread& thread);

void set_thread_priority(priority_t priority);

void set_thread_priority(std::thread& thread, priority_t priority);

//==========
// Executor
//==========

/**
 * An executor is a thread executing a single task each time it is awaken
 *
 * @warning spurious wake-up may happen and are left to the caller
 * @warning a started executor *must* be stopped explicitly
 */

class Executor {

public:
    //------------
    // properties
    //------------

    inline bool is_running() const {
        return m_running;
    }

    inline const std::thread& thread() const {
        return m_thread;
    }

    //----------
    // features
    //----------

    template<typename CallableT>
    inline void start(CallableT&& callable) {
        if (!m_running.exchange(true)) {
            m_thread = std::thread{[this, callable] {
                std::unique_lock<std::mutex> guard{m_mutex};
                while (true) {
                    callable();
                    if (!m_running)
                        return;
                    m_condition_variable.wait(guard);
                }
            }};
        }
    }

    inline void halt() {
        stop();
        join();
    }

    inline void stop() {
        if (m_running.exchange(false))
            awake();
    }

    inline void join() {
        if (m_thread.joinable())
            m_thread.join();
    }

    inline void awake() {
        m_condition_variable.notify_one();
    }

    //---------------------
    // collection features
    //---------------------

    template<typename ExecutorsT, typename CallableT>
    static void start_all(ExecutorsT& executors, CallableT&& callable) {
        for (auto& executor : executors)
            executor.start(std::forward<CallableT>(callable));
    }

    template<typename ExecutorsT>
    static void halt_all(ExecutorsT& executors) {
        stop_all(executors);
        join_all(executors);
    }

    template<typename ExecutorsT>
    static void stop_all(ExecutorsT& executors) {
        for (auto& executor : executors)
            executor.stop();
    }

    template<typename ExecutorsT>
    static void join_all(ExecutorsT& executors) {
        for (auto& executor : executors)
            executor.join();
    }

    template<typename ExecutorsT>
    static void awake_all(ExecutorsT& executors) {
        for (auto& executor : executors)
            executor.awake();
    }

private:
    //------------
    // attributes
    //------------

    std::thread m_thread;
    std::atomic_bool m_running {false};
    std::condition_variable m_condition_variable;
    std::mutex m_mutex;

};

//=======
// Queue
//=======

/**
 * A queue wraps a container so that accessing it is safe in multi-threaded environments.
 *
 * This queue is designed for multiple producers and single consumer.
 * values may be produced individually, but consumption must be done on all values available
 *
 * produce will return true if all values produced previously have been consumed.
 *
 * ContainerT may be any objects providing methods 'push_back', 'empty' and 'clear'
 * It should also be swappable and default constructible
 *
 */

template<typename ContainerT>
class Queue {

public:

    bool is_consumed() const {
        std::lock_guard<std::mutex> guard{m_mutex};
        return m_consumed;
    }

    template<typename U>
    bool produce(U&& value) {
        std::lock_guard<std::mutex> guard{m_mutex};
        m_frontend.push_back(std::forward<U>(value));
        return std::exchange(m_consumed, false);
    }

    template<typename CallableT>
    void consume(CallableT&& callable) {
        while (!stash()) {
            callable(m_backend);
            m_backend.clear();
        }
    }

    bool stash() {
        using std::swap;
        std::lock_guard<std::mutex> guard{m_mutex};
        swap(m_frontend, m_backend);
        return ( m_consumed = m_backend.empty() );
    }

private:
    ContainerT m_frontend;
    ContainerT m_backend;
    bool m_consumed {true};
    mutable std::mutex m_mutex;

};

#endif // TOOLS_CONCURRENCY_H
