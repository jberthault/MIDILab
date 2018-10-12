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

#ifndef HANDLERS_SOUNDFONT_H
#define HANDLERS_SOUNDFONT_H

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <sstream>
#include <boost/optional.hpp>
#include "core/handler.h"

template<typename T>
struct SoundFontExtension : SystemExtension<T> {
    SoundFontExtension(std::string key, T default_value) : SystemExtension<T>{std::move(key)}, default_value{default_value} {}
    T default_value;
};

template<typename T>
struct SoundFontBoundedExtension : SoundFontExtension<T> {
    SoundFontBoundedExtension(std::string key, T default_value, const basic_range_t<T>& range) : SoundFontExtension<T>{std::move(key), default_value}, range{range} {}
    basic_range_t<T> range;
};

struct SoundFontExtensions {
    SoundFontBoundedExtension<double> gain;
    SystemExtension<std::string> file;
    struct {
        SoundFontExtension<bool> activated;
        SoundFontBoundedExtension<double> roomsize;
        SoundFontBoundedExtension<double> damp;
        SoundFontBoundedExtension<double> level;
        SoundFontBoundedExtension<double> width;
    } reverb;
    struct {
        SoundFontExtension<bool> activated;
        SoundFontExtension<int> type;
        SoundFontBoundedExtension<int> nr;
        SoundFontBoundedExtension<double> level;
        SoundFontBoundedExtension<double> speed;
        SoundFontBoundedExtension<double> depth;
    } chorus;
};

//==================
// SoundFontHandler
//==================

class SoundFontHandler : public Handler {

public:
    static const SoundFontExtensions ext;

    explicit SoundFontHandler();
    ~SoundFontHandler();

    double gain() const;
    std::string file() const;
    bool reverb_activated() const;
    double reverb_roomsize() const;
    double reverb_damp() const;
    double reverb_level() const;
    double reverb_width() const;
    bool chorus_activated() const;
    int chorus_type() const;
    int chorus_nr() const;
    double chorus_level() const;
    double chorus_speed() const;
    double chorus_depth() const;

protected:
    Result handle_close(State state) override;
    Result handle_message(const Message& message) override;
    families_t handled_families() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // HANDLERS_SOUNDFONT_H
