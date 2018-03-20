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

#include "concurrency.h"
#include <iostream>
#include "trace.h"

//==========
// Priority
//==========

std::ostream& operator<<(std::ostream& stream, priority_t priority) {
    switch (priority) {
    case priority_t::idle: stream << "idle"; break;
    case priority_t::lowest: stream << "lowest"; break;
    case priority_t::low: stream << "low"; break;
    case priority_t::normal: stream << "normal"; break;
    case priority_t::high: stream << "high"; break;
    case priority_t::highest: stream << "highest"; break;
    case priority_t::realtime: stream << "realtime"; break;
    }
    return stream;
}

#if defined(_WIN32)

#include <windows.h>

namespace {

void set_handle_priority(HANDLE handle, priority_t priority) {
    int p = THREAD_PRIORITY_NORMAL;
    switch (priority) {
    case priority_t::idle: p = THREAD_PRIORITY_IDLE; break;
    case priority_t::lowest: p = THREAD_PRIORITY_LOWEST; break;
    case priority_t::low: p = THREAD_PRIORITY_BELOW_NORMAL; break;
    case priority_t::normal: p = THREAD_PRIORITY_NORMAL; break;
    case priority_t::high: p = THREAD_PRIORITY_ABOVE_NORMAL; break;
    case priority_t::highest: p = THREAD_PRIORITY_HIGHEST; break;
    case priority_t::realtime: p = THREAD_PRIORITY_TIME_CRITICAL; break;
    }
    if (::SetThreadPriority(handle, p) == FALSE) {
        TRACE_ERROR("failed setting thread priority (errno " << ::GetLastError() << ")");
    }
}

priority_t get_handle_priority(HANDLE handle) {
    switch (::GetThreadPriority(handle)) {
    case THREAD_PRIORITY_IDLE: return priority_t::idle;
    case THREAD_PRIORITY_LOWEST: return priority_t::lowest;
    case THREAD_PRIORITY_BELOW_NORMAL: return priority_t::low;
    case THREAD_PRIORITY_NORMAL: return priority_t::normal;
    case THREAD_PRIORITY_ABOVE_NORMAL: return priority_t::high;
    case THREAD_PRIORITY_HIGHEST: return priority_t::highest;
    case THREAD_PRIORITY_TIME_CRITICAL: return priority_t::realtime;
    }
    return priority_t::normal;
}

}

priority_t get_thread_priority() {
    return get_handle_priority(::GetCurrentThread());
}

void set_thread_priority(priority_t priority) {
    set_handle_priority(::GetCurrentThread(), priority);
}

#if !defined(__MINGW32__) /// @warning using MinGW, the native handle does not match the Windows HANDLE

priority_t get_thread_priority(std::thread& thread) {
    return get_handle_priority((HANDLE)thread.native_handle());
}

void set_thread_priority(std::thread& thread, priority_t priority) {
    set_handle_priority((HANDLE)thread.native_handle(), priority);
}

#endif

#else

priority_t get_thread_priority() {
    TRACE_WARNING("get_thread_priority not implemented");
    return priority_t::normal;
}

void set_thread_priority(priority_t priority) {
    TRACE_WARNING("set_thread_priority not implemented");
}

priority_t get_thread_priority(std::thread& thread) {
    TRACE_WARNING("get_thread_priority not implemented");
    return priority_t::normal;
}

void set_thread_priority(std::thread& thread, priority_t priority) {
    TRACE_WARNING("set_thread_priority not implemented");
}

#endif
