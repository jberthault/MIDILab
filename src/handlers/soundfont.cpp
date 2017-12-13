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

#include "soundfont.h"

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <sstream>
#include <fluidsynth.h>

template<>
struct marshalling_traits<SoundFontHandler::reverb_type> {
    auto operator()(const SoundFontHandler::reverb_type& reverb) {
        std::stringstream ss;
        ss << reverb.roomsize << ' ' << reverb.damp << ' ' << reverb.level << ' ' << reverb.width;
        return ss.str();
    }
};

template<>
struct unmarshalling_traits<SoundFontHandler::reverb_type> {
    auto operator()(const std::string& string) {
        SoundFontHandler::reverb_type reverb;
        std::stringstream ss(string);
        ss >> reverb.roomsize >> reverb.damp  >> reverb.level >> reverb.width;
        return reverb;
    }
};


//======
// Impl
//======

struct SoundFontHandler::Impl {

    Impl() {
        settings = new_fluid_settings();
        fluid_settings_setint(settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
        fluid_settings_setstr(settings, "audio.jack.id", "MIDILab");
        synth = new_fluid_synth(settings);
        adriver = new_fluid_audio_driver(settings, synth);
        has_reverb = true;
    }

    ~Impl() {
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    double gain() const {
        return (double)fluid_synth_get_gain(synth);
    }

    SoundFontHandler::reverb_type reverb() const {
        SoundFontHandler::reverb_type reverb;
        reverb.roomsize = fluid_synth_get_reverb_roomsize(synth);
        reverb.damp = fluid_synth_get_reverb_damp(synth);
        reverb.level = fluid_synth_get_reverb_level(synth);
        reverb.width = fluid_synth_get_reverb_width(synth);
        return reverb;
    }

    void close() {
        fluid_synth_system_reset(synth);
    }

    Handler::result_type reset_system() {
        push(Event::controller(all_channels, controller_ns::all_controllers_off_controller));
        push(Event::controller(all_channels, controller_ns::all_sound_off_controller));
        // push(Event::controller(all_channels, controller_ns::volume_coarse_controller, 100));
        return result_type::success;
    }

    Handler::result_type push(const Event& event) {
        switch (event.family()) {
        case family_t::note_off: for (channel_t channel : event.channels()) fluid_synth_noteoff(synth, channel, event.at(1)); return result_type::success;
        case family_t::note_on: for (channel_t channel : event.channels()) fluid_synth_noteon(synth, channel, event.at(1), event.at(2)); return result_type::success;
        case family_t::program_change: for (channel_t channel : event.channels()) fluid_synth_program_change(synth, channel, event.at(1)); return result_type::success;
        case family_t::controller: for (channel_t channel : event.channels()) fluid_synth_cc(synth, channel, event.at(1), event.at(2)); return result_type::success;
        case family_t::channel_pressure: for (channel_t channel : event.channels()) fluid_synth_channel_pressure(synth, channel, event.at(1)); return result_type::success;
        case family_t::pitch_wheel: for (channel_t channel : event.channels()) fluid_synth_pitch_bend(synth, channel, event.get_14bits()); return result_type::success;
        case family_t::reset: return reset_system();
        case family_t::sysex: {
            /// @note master volume does not seem to be handled correctly
            std::vector<char> sys_ex_data(event.begin()+1, event.end()-1);
            fluid_synth_sysex(synth, sys_ex_data.data(), sys_ex_data.size(), nullptr, nullptr, nullptr, 0);
            return result_type::success;
        }
        case family_t::custom: {
            auto k = event.get_custom_key();
            if (k == "SoundFont.gain") return process_gain(event.get_custom_value());
            if (k == "SoundFont.reverb") return process_reverb(event.get_custom_value());
            if (k == "SoundFont.file") return process_file(event.get_custom_value());
            break;
        }
        }
        return result_type::unhandled;
    }

    Handler::result_type process_gain(std::string string) {
        fluid_synth_set_gain(synth, (float)unmarshall<double>(string));
        return result_type::success;
    }

    Handler::result_type process_reverb(std::string string) {
        has_reverb = !string.empty();
        if (has_reverb) {
            auto reverb = unmarshall<SoundFontHandler::reverb_type>(string);
            fluid_synth_set_reverb(synth, reverb.roomsize, reverb.damp, reverb.width, reverb.level);
        }
        fluid_synth_set_reverb_on(synth, (int)has_reverb);
        return result_type::success;
    }

    Handler::result_type process_file(std::string string) {
        if (fluid_synth_sfload(synth, string.c_str(), 1) == -1)
            return result_type::fail;
        file = std::move(string);
        return result_type::success;
    }

    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    std::string file;
    bool has_reverb;

};

//==================
// SoundFontHandler
//==================

SoundFontHandler::reverb_type SoundFontHandler::default_reverb() {
    return {FLUID_REVERB_DEFAULT_ROOMSIZE, FLUID_REVERB_DEFAULT_DAMP, FLUID_REVERB_DEFAULT_LEVEL, FLUID_REVERB_DEFAULT_WIDTH};
}

Event SoundFontHandler::gain_event(double gain) {
    return Event::custom({}, "SoundFont.gain", marshall(gain));
}

Event SoundFontHandler::file_event(const std::string& file) {
    return Event::custom({}, "SoundFont.file", file);
}

Event SoundFontHandler::reverb_event(const optional_reverb_type &reverb) {
    if (reverb)
        return Event::custom({}, "SoundFont.reverb", marshall(*reverb));
    else
        return Event::custom({}, "SoundFont.reverb");
}

SoundFontHandler::SoundFontHandler() : Handler(handler_ns::out_mode), m_pimpl(std::make_unique<Impl>()) {

}

double SoundFontHandler::gain() const {
    return m_pimpl->gain();
}

std::string SoundFontHandler::file() const {
    return m_pimpl->file;
}

SoundFontHandler::optional_reverb_type SoundFontHandler::reverb() const {
    SoundFontHandler::optional_reverb_type result;
    if (m_pimpl->has_reverb)
        result = m_pimpl->reverb();
    return result;
}

families_t SoundFontHandler::handled_families() const {
    return families_t::merge(family_t::custom, family_t::sysex, family_t::reset, family_t::note_off, family_t::note_on, family_t::controller, family_t::program_change, family_t::channel_pressure, family_t::pitch_wheel);
}

SoundFontHandler::result_type SoundFontHandler::on_open(state_type state) {
    TRACE_INFO("Sound Font thread priority: " << get_thread_priority());
    return Handler::on_open(state);
}

SoundFontHandler::result_type SoundFontHandler::on_close(state_type state) {
    if (state & handler_ns::receive_state)
        m_pimpl->close();
    return Handler::on_close(state);
}

SoundFontHandler::result_type SoundFontHandler::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    return m_pimpl->push(message.event);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
