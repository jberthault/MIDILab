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

#include <sstream>
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

    struct chorus_type {
        int type;
        int nr;
        double level;
        double speed; // Hz
        double depth; // ms
    };

    using optional_reverb_type = boost::optional<reverb_type>;
    using optional_chorus_type = boost::optional<chorus_type>;

    static reverb_type default_reverb();
    static chorus_type default_chorus();

    static const SystemExtension<double> gain_ext; /*!< must be in range [0, 10] */
    static const SystemExtension<std::string> file_ext;
    static const SystemExtension<optional_reverb_type> reverb_ext;
    static const SystemExtension<optional_chorus_type> chorus_ext;

    explicit SoundFontHandler();
    ~SoundFontHandler();

    double gain() const;
    std::string file() const;
    optional_reverb_type reverb() const;
    optional_chorus_type chorus() const;

    families_t handled_families() const override;
    Result handle_message(const Message& message) override;
    Result on_close(State state) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;

};

// ===========
// marshalling
// ===========

template<typename T>
struct marshalling_traits<boost::optional<T>> {
    std::string operator()(const boost::optional<T>& value) {
        if (!value)
            return {};
        return marshall(*value);
    }
};

template<typename T>
struct unmarshalling_traits<boost::optional<T>> {
    boost::optional<T> operator()(const std::string& string) {
        if (string.empty())
            return {};
        return {unmarshall<T>(string)};
    }
};

template<> inline auto marshall<SoundFontHandler::reverb_type>(const SoundFontHandler::reverb_type& reverb) {
    std::stringstream ss;
    ss << reverb.roomsize << ' ' << reverb.damp << ' ' << reverb.level << ' ' << reverb.width;
    return ss.str();
}

template<> inline auto unmarshall<SoundFontHandler::reverb_type>(const std::string& string) {
    SoundFontHandler::reverb_type reverb;
    std::stringstream ss(string);
    ss >> reverb.roomsize >> reverb.damp >> reverb.level >> reverb.width;
    return reverb;
}

template<> inline auto marshall<SoundFontHandler::chorus_type>(const SoundFontHandler::chorus_type& chorus) {
    std::stringstream ss;
    ss << chorus.type << ' ' << chorus.nr << ' ' << chorus.level << ' ' << chorus.speed << ' ' << chorus.depth;
    return ss.str();
}

template<> inline auto unmarshall<SoundFontHandler::chorus_type>(const std::string& string) {
    SoundFontHandler::chorus_type chorus;
    std::stringstream ss{string};
    ss >> chorus.type >> chorus.nr >> chorus.level >> chorus.speed >> chorus.depth;
    return chorus;
}

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // HANDLERS_SOUNDFONT_H
