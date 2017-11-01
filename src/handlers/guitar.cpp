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

#include "guitar.h"

using namespace handler_ns;

//============
// MetaGuitar
//============

MetaGuitar::MetaGuitar(QObject* parent) : MetaInstrument(parent) {
    setIdentifier("Guitar");
}

MetaHandler::Instance MetaGuitar::instantiate() {
    return Instance(new Guitar, nullptr);
}

//========
// Guitar
//========

const QList<Note> Guitar::guitarTuning = (QList<Note>() <<
                                                 Note::from_string("E3") << Note::from_string("A4") << Note::from_string("D4") <<
                                                 Note::from_string("G4") << Note::from_string("B5") << Note::from_string("E5") );

const QList<Note> Guitar::bassTuning = (QList<Note>() <<
                                               Note::from_string("E3") << Note::from_string("A4") << Note::from_string("D4") << Note::from_string("G4") );

Guitar::Guitar() : Instrument(io_mode), mSize(0) {

}

bool Guitar::setTuning(const QList<Note>& tuning) {
    mTuning = tuning;
    return true;
}

size_t Guitar::size() const {
    return mSize;
}

void Guitar::setSize(size_t size) {
    mSize = size;
}

void Guitar::onNotesOff(channels_t channels) {
    for (channel_t channel : channels)
        mAffectations[channel].clear();
}

void Guitar::setNote(channels_t channels, const Note& note, bool on) {
    for (channel_t channel : channels)
        setSingle(channel, note, on);
}

void Guitar::setSingle(channel_t channel, const Note& note, bool on) {
    ChannelAffectation& aff = mAffectations[channel];
    if (on) {
        if (aff.count(note.code()) == 1)
            return;
        // affect
        // update
    } else {
        if (aff.erase(note.code()) == 1) {
            // update
        }
    }
}
