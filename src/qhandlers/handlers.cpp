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

#include "qhandlers/handlers.h"
#include "qhandlers/piano.h"
#include "qhandlers/monitor.h"
#include "qhandlers/wheel.h"
#include "qhandlers/transposer.h"
#include "qhandlers/recorder.h"
#include "qhandlers/harmonica.h"
#include "qhandlers/forwarder.h"
#include "qhandlers/channelmapper.h"
#include "qhandlers/soundfont.h"
#include "qhandlers/player.h"
#include "qhandlers/trackfilter.h"
#include "qhandlers/guitar.h"
#include "qhandlers/system.h"

//=================
// StandardFactory
//=================

StandardFactory::StandardFactory(QObject* parent) : QObject(parent), MetaHandlerFactory() {
    mMetaHandlers = {
        // instruments
        new MetaPiano(this),
        new MetaHarmonica(this),
        new MetaGuitar(this),
        // wheels
        new MetaControllerWheel(this),
        new MetaPitchWheel(this),
        new MetaProgramWheel(this),
        new MetaVolume1Wheel(this),
        new MetaVolume2Wheel(this),
        // editors for basic handlers
        new MetaTransposer(this),
        new MetaRecorder(this),
        new MetaSystem(this),
    #ifdef MIDILAB_FLUIDSYNTH_VERSION
        new MetaSoundFont(this),
    #endif // MIDILAB_FLUIDSYNTH_VERSION
        new MetaPlayer(this),
        // other graphical handlers
        new MetaMonitor(this),
        // basic handlers
        new MetaForwarder(this),
        new MetaChannelMapper(this),
        new MetaTrackFilter(this)
    };
}

const MetaHandlers& StandardFactory::spawn() const {
    return mMetaHandlers;
}

//=================
// Pattern Handler
//=================

//PatternHandler::PatternHandler(Event target) :
//    Handler(io_mode), current_state(0), target_state(3), target_event(std::move(target)) {

//}

//Handler::Result PatternHandler::on_open(State state) {
//    current_state = 0;
//    return Handler::on_open(state);
//}

//Handler::Result PatternHandler::handle_message(const Message& message) {
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
//    if (event.is(family_t::note_on))
//        return event.get_note() == A0 ? good : bad;
//    return ignored;
//}
