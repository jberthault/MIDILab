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

#ifndef TOOLS_TRACE_H
#define TOOLS_TRACE_H

#include <iostream>
#include <mutex>
#include <chrono>

//========
// Traces
//========

struct logging_tools {

    enum class level_type {
        debug,
        info,
        warning,
        error
    };

    friend std::ostream& operator<<(std::ostream& os, level_type level);

    static bool enable;
    static std::recursive_mutex mutex;

};

#define TRACE(level, ...)\
    do {\
        if (logging_tools::enable) {\
            std::lock_guard<std::recursive_mutex> __trace_lock(logging_tools::mutex);\
            std::cout << '[' << logging_tools::level_type::level << "] " << __VA_ARGS__ << std::endl;\
        }\
    } while (false)

//=========
// Measure
//=========

struct measure_t {

    using clock_type = std::chrono::steady_clock;
    using duration_type = std::chrono::duration<double, std::milli>;

    inline measure_t(const char* text) : text(text), t0(clock_type::now()) {}

    inline ~measure_t() {
        auto t1 = clock_type::now();
        auto dt = std::chrono::duration_cast<duration_type>(t1-t0);
        TRACE(debug, text << ": " << dt.count() << " ms");
    }

    const char* text;
    clock_type::time_point t0;
};

//========
// MACROS
//========

#define TRACE_IGNORE(...) do {} while (false)

#ifdef MIDILAB_ENABLE_DEBUG
    #define TRACE_DEBUG(...) TRACE(debug, __VA_ARGS__)
#else
    #define TRACE_DEBUG TRACE_IGNORE
#endif

#ifdef MIDILAB_ENABLE_INFO
    #define TRACE_INFO(...) TRACE(info, __VA_ARGS__)
#else
    #define TRACE_INFO TRACE_IGNORE
#endif

#ifdef MIDILAB_ENABLE_WARNING
    #define TRACE_WARNING(...) TRACE(warning, __VA_ARGS__)
#else
    #define TRACE_WARNING TRACE_IGNORE
#endif

#ifdef MIDILAB_ENABLE_ERROR
    #define TRACE_ERROR(...) TRACE(error, __VA_ARGS__)
#else
    #define TRACE_ERROR TRACE_IGNORE
#endif

#ifdef MIDILAB_ENABLE_TIMING
    #define TRACE_MEASURE(...) measure_t __measure {__VA_ARGS__}
#else
    #define TRACE_MEASURE TRACE_IGNORE
#endif

#endif // TOOLS_TRACE_H
