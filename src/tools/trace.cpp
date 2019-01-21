/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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

#include "trace.h"

std::recursive_mutex logging_tools::mutex;

bool logging_tools::enable = true;

std::ostream& operator<<(std::ostream& os, logging_tools::level_type level) {
    switch (level) {
    case logging_tools::level_type::debug: os << "debug"; break;
    case logging_tools::level_type::info: os << "info"; break;
    case logging_tools::level_type::warning: os << "warning"; break;
    case logging_tools::level_type::error: os << "error"; break;
    }
    return os;
}
