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

#ifndef HANDLERS_SYSTEM_HANDLER_H
#define HANDLERS_SYSTEM_HANDLER_H

#include <list>
#include "core/handler.h"

/**
  * create the list of all available handlers available on this platform
  * @note ownership is given to the caller
  * @todo order handlers to get the default one at first
  */

Event volume_event(uint16_t left, uint16_t right);

std::list<Handler*> create_system();

#endif // HANDLERS_SYSTEM_HANDLER_H
