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

#include "tickhandler.h"

namespace {

constexpr auto playing_state = Handler::State::from_integral(0x4);

}

//=============
// TickHandler
//=============

TickHandler::TickHandler() : Handler{Mode::in()} {

}

TickHandler::~TickHandler() {
    stop();
}

Handler::Result TickHandler::handle_open(State state) {
    if (state & State::forward())
        start();
    return Handler::handle_open(state);
}

Handler::Result TickHandler::handle_close(State state) {
    if (state & State::forward())
        stop();
    return Handler::handle_close(state);
}

families_t TickHandler::produced_families() const {
    return families_t::wrap(family_t::tick);
}

void TickHandler::start() {
    if (m_worker.joinable())
        return;
    m_worker = std::thread{ [this] {
        activate_state(playing_state);
        while (state().any(playing_state)) {
            produce_message(Event::tick());
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }};
}

void TickHandler::stop() {
    deactivate_state(playing_state);
    if (m_worker.joinable())
        m_worker.join();
}
