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

#include "bytes.h"

namespace {

char nibble2str(byte_t nibble) {
    nibble &= 0xf;
    return nibble + ((nibble < 0xa) ? '0' : 'W');
}

}

std::string byte_string(byte_t byte) {
    std::string string = "0x";
    string += nibble2str(byte >> 4);
    string += nibble2str(byte);
    return string;
}
