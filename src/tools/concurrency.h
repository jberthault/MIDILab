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
#include <boost/circular_buffer.hpp>

/**
 *
 * Portable way to change the priority of a thread
 *
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

std::ostream& operator << (std::ostream& stream, priority_t priority);

priority_t get_thread_priority();

priority_t get_thread_priority(std::thread& thread);

void set_thread_priority(priority_t priority);

void set_thread_priority(std::thread& thread, priority_t priority);

/**
 *
 * A stateless task is a simpler version of the statefull task
 * It can save only one parameter instead of multiple, and results of operations are discarded
 *
 * T requirements:
 * @li DefaultConstructible
 * @li MoveConstructible
 *
 * F requirements:
 * @li Callable (taking T&& as the only parameter, result not specified, constness not specified)
 *
 * @warning it's up to the user to explicitly start and stop the task, there is a risk of hanging at destruction otherwise
 * @note thread-safe is guaranteed but exception are not
 *
 */

template <typename T>
class task_t final {

public:
    using queue_type = boost::circular_buffer<T>;
    using value_type = typename queue_type::value_type;

    explicit task_t(size_t capacity) : m_queue(capacity), m_running(false) {

    }

    ~task_t() {
        assert(!m_running);
    }

    bool is_running() const {
        return m_running;
    }

    std::thread::id get_id() const {
        return m_thread.get_id();
    }

    template <typename F>
    void start(F consumer) {
        if (!m_running) {
            m_running = true;
            m_thread = std::thread([this, consumer] {
                run(consumer);
            });
        }
    }

    template <typename F>
    void start(priority_t priority, F consumer) {
        if (!m_running) {
            m_running = true;
            m_thread = std::thread([this, priority, consumer] {
                set_thread_priority(priority);
                run(consumer);
            });
        }
    }

    template <typename F>
    void run(F consumer) {
        std::unique_lock<std::mutex> guard(m_mutex);
        queue_type queue(m_queue.capacity());
        while (true) {
            while (!m_queue.empty()) {
                m_queue.swap(queue);
                guard.unlock();
                for(auto& value : queue)
                    consumer(std::move(value));
                queue.clear();
                guard.lock();
            }
            if (!m_running)
                return;
            m_condition.wait(guard);
        }
    }

    void stop(bool clear = false) {
        std::unique_lock<std::mutex> guard(m_mutex);
        if (m_running) {
            m_running = false;
            if (clear)
                m_queue.clear();
            guard.unlock();
            m_condition.notify_all();
            m_thread.join();
        }
    }

    template<typename U>
    bool push(U&& value) {
        std::unique_lock<std::mutex> guard(m_mutex);
        if (m_running) {
            m_queue.push_back(std::forward<U>(value));
            m_condition.notify_all();
        }
        return m_running;
    }

    template <typename It>
    bool push(It first, It last) {
        std::unique_lock<std::mutex> guard(m_mutex);
        if (m_running) {
            m_queue.insert(m_queue.end(), first, last);
            m_condition.notify_all();
        }
        return m_running;
    }

private:
    queue_type m_queue; /*!< queue containing data to process */
    bool m_running; /*!< boolean controlling the thread execution */
    std::thread m_thread; /*!< thread consuming messages */
    mutable std::condition_variable m_condition; /*!< thread waking condition */
    mutable std::mutex m_mutex; /*!< mutex protecting queue */

};

#endif // TOOLS_CONCURRENCY_H

