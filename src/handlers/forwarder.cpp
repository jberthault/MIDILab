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

#include "forwarder.h"

using namespace handler_ns;

//================
// ForwardHandler
//================

ForwardHandler::ForwardHandler() : Handler(thru_mode) {

}

Handler::result_type ForwardHandler::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;
    forward_message(message);
    return success_result;
}

//===============
// MetaForwarder
//===============

MetaForwarder::MetaForwarder(QObject* parent) : MetaHandler(parent) {
    setIdentifier("Forwarder");
    setDescription("Connection Tool");
}

MetaHandler::instance_type MetaForwarder::instantiate(const QString& name, QWidget* /*parent*/) {
    ForwardHandler* handler = new ForwardHandler;
    handler->set_name(qstring2name(name));
    return instance_type(handler, nullptr);
}
