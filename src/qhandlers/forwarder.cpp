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

#include "qhandlers/forwarder.h"

//===============
// MetaForwarder
//===============

MetaForwarder::MetaForwarder(QObject* parent) : OpenMetaHandler(parent) {
    setIdentifier("Forwarder");
    setDescription("Connection Tool");
}

void MetaForwarder::setContent(HandlerProxy& proxy) {
    proxy.setContent(new ForwardHandler);
}
