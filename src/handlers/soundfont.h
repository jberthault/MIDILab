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

#ifndef HANDLERS_SOUNDFONT_H
#define HANDLERS_SOUNDFONT_H

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <boost/optional.hpp>
#include "core/handler.h"

//==================
// SoundFontHandler
//==================

class SoundFontHandler : public Handler {

public:

    struct reverb_type {
        double roomsize;
        double damp;
        double level;
        double width;
    };

    using optional_reverb_type = boost::optional<reverb_type>;

    static reverb_type default_reverb();

    static Event gain_event(double gain); /*!< must be in range [0, 10] */
    static Event file_event(const std::string& file);
    static Event reverb_event(const optional_reverb_type& reverb);

    explicit SoundFontHandler();

    double gain() const;
    std::string file() const;
    optional_reverb_type reverb() const;

    families_t handled_families() const override;
    result_type handle_message(const Message& message) override;
    result_type on_open(state_type state) override;
    result_type on_close(state_type state) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // HANDLERS_SOUNDFONT_H
