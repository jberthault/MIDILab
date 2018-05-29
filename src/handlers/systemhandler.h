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

#ifndef HANDLERS_SYSTEM_HANDLER_H
#define HANDLERS_SYSTEM_HANDLER_H

#include "core/handler.h"

/**
  * create the list of all available handlers available on this platform
  * @note ownership is given to the caller
  * @todo order handlers to get the default one at first
  */

Event volume_event(uint16_t left, uint16_t right);

//======================
// SystemHandlerFactory
//======================

class SystemHandlerFactory {

public:
    SystemHandlerFactory();
    ~SystemHandlerFactory();

    std::vector<std::string> available() const; /*!< list available system handlers */

    void update(); /*!< update the list of available handlers */

    std::unique_ptr<Handler> instantiate(const std::string& name); /*!< get a new handler by its name */

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

};

#endif // HANDLERS_SYSTEM_HANDLER_H
