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

#ifndef HANDLERS_FORWARDER_H
#define HANDLERS_FORWARDER_H

#include "core/handler.h"

//================
// ForwardHandler
//================

class ForwardHandler : public Handler {

public:
    explicit ForwardHandler();

protected:
    Result handle_message(const Message& message) override;

};

#endif // HANDLERS_FORWARDER_H
