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

#include "handlers.h"
#include "piano.h"
#include "monitor.h"
#include "wheel.h"
#include "transposer.h"
#include "recorder.h"
#include "harmonica.h"
#include "forwarder.h"
#include "channelmapper.h"
#include "soundfont.h"
#include "player.h"
#include "trackfilter.h"
#include "guitar.h"

//=================
// StandardFactory
//=================

StandardFactory::StandardFactory(QObject* parent) : QObject(parent), MetaHandlerFactory() {
    mMetaHandlers
             // instruments
             << new MetaPiano(this)
             << new MetaHarmonica(this)
             // << new MetaGuitar(this)
             // wheels
             << new MetaControllerWheel(this)
             << new MetaPitchWheel(this)
             << new MetaProgramWheel(this)
             << new MetaVolume1Wheel(this)
             << new MetaVolume2Wheel(this)
             // editors for basic handlers
             << new MetaTransposer(this)
             << new MetaRecorder(this)
            #ifdef MIDILAB_FLUIDSYNTH_VERSION
             << new MetaSoundFont(this)
            #endif // MIDILAB_FLUIDSYNTH_VERSION
             << new MetaPlayer(this)
             // other graphical handlers
             << new MetaMonitor(this)
             // basic handlers
             << new MetaForwarder(this)
             << new MetaChannelMapper(this)
             << new MetaTrackFilter(this);
}

const QList<MetaHandler*>& StandardFactory::spawn() const {
    return mMetaHandlers;
}

//=================
// Pattern Handler
//=================

//PatternHandler::PatternHandler(Event target) :
//    Handler(io_mode), current_state(0), target_state(3), target_event(std::move(target)) {

//}

//Handler::result_type PatternHandler::on_open(state_t state) {
//    current_state = 0;
//    return Handler::on_open(state);
//}

//Handler::result_type PatternHandler::handle_message(const Message& message) {
//    MIDI_HANDLE_OPEN;
//    MIDI_CHECK_OPEN_FORWARD_RECEIVE;
//    switch (advance(message.event)) {
//    case ignored:
//        return success_result;
//    case bad: // reset broken chain
//        current_state = 0;
//        return success_result;
//    default:
//        if (++current_state == target_state) {
//            current_state = 0;
//            return forward_message({target_event, this});
//        }
//        return success_result;
//    }
//}

//int PatternHandler::advance(const Event& event) {
//    static const Note A0(Tonality::A, 0);
//    if (event.is(family_ns::note_on_family))
//        return event.get_note() == A0 ? good : bad;
//    return ignored;
//}
