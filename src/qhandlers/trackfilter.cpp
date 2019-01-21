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

#include "handlers/trackfilter.h"
#include "qhandlers/trackfilter.h"

MetaHandler* makeMetaTrackFilter(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("TrackFilter");
    meta->setDescription("Filters events that do no belong to certain tracks");
    meta->setFactory(new OpenProxyFactory<TrackFilter>);
    return meta;
}
