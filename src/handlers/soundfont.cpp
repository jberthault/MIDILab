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

namespace {

// check if event specifies to turn some channels to drum channels
channels_t use_for_rhythm_part(const Event& event) {
    if (   event.family() == family_t::sysex             // sysex event
        && event.at(2) == 0x41                           // roland manufacturer
        && event.at(5) == 0x12                           // sending data
        && event.at(6) == 0x40                           //
        && (event.at(7) & 0xf0) == 0x10                  //
        && event.at(8) == 0x15                           // 0x40 0x1X 0x15 <=> address of USE_FOR_RHYTHM_PART
        && (event.at(9) == 0x01 || event.at(9) == 0x02)) // value {0x00: OFF, 0x01: MAP1, 0x02: MAP2}
        return channels_t::merge(event.at(7) & 0x0f);
    return {};
}

}


//======
// Impl
//======

struct SoundFontHandler::Impl {

    using result_type = Handler::result_type;
    using reverb_type = SoundFontHandler::reverb_type;

    // ---------
    // structors
    // ---------

    Impl() {
        settings = new_fluid_settings();
        fluid_settings_setint(settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
        fluid_settings_setstr(settings, "audio.jack.id", "MIDILab");
        synth = new_fluid_synth(settings);
        adriver = new_fluid_audio_driver(settings, synth);
        has_reverb = true;
        drums = drum_channels;
    }

    ~Impl() {
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    // ---------
    // accessors
    // ---------

    double gain() const {
        return (double)fluid_synth_get_gain(synth);
    }

    reverb_type reverb() const {
        SoundFontHandler::reverb_type reverb;
        reverb.roomsize = fluid_synth_get_reverb_roomsize(synth);
        reverb.damp = fluid_synth_get_reverb_damp(synth);
        reverb.level = fluid_synth_get_reverb_level(synth);
        reverb.width = fluid_synth_get_reverb_width(synth);
        return reverb;
    }


    // --------------
    // handle methods
    // --------------

    result_type handle(const Event& event) {
        switch (event.family()) {
        case family_t::note_off: return handle_note_off(event.channels(), event.at(1));
        case family_t::note_on: return handle_note_on(event.channels(), event.at(1), event.at(2));
        case family_t::program_change: return handle_program_change(event.channels(), event.at(1));
        case family_t::controller: return handle_controller(event.channels(), event.at(1), event.at(2));
        case family_t::channel_pressure: return handle_channel_pressure(event.channels(), event.at(1));
        case family_t::pitch_wheel: return handle_pitch_wheel(event.channels(), event.get_14bits());
        case family_t::reset: return handle_reset();
        case family_t::sysex: return handle_sysex(event);
        case family_t::custom: {
            auto k = event.get_custom_key();
            if (k == "SoundFont.gain") return handle_gain(event.get_custom_value());
            if (k == "SoundFont.reverb") return handle_reverb(event.get_custom_value());
            if (k == "SoundFont.file") return handle_file(event.get_custom_value());
            break;
        }
        }
        return result_type::unhandled;
    }

    result_type handle_note_off(channels_t channels, int note) {
        for (channel_t channel : channels)
            fluid_synth_noteoff(synth, channel, note);
        return result_type::success;
    }

    result_type handle_note_on(channels_t channels, int note, int velocity) {
        for (channel_t channel : channels)
            fluid_synth_noteon(synth, channel, note, velocity);
        return result_type::success;
    }

    result_type handle_program_change(channels_t channels, int program) {
        for (channel_t channel : channels)
            fluid_synth_program_change(synth, channel, program);
        return result_type::success;
    }

    result_type handle_controller(channels_t channels, byte_t controller, int value = 0x00) {
        if (controller == controller_ns::all_controllers_off_controller) {
            handle_channel_type(channels & ~drum_channels, CHANNEL_TYPE_MELODIC);
            handle_channel_type(channels & drum_channels, CHANNEL_TYPE_DRUM);
            for (channel_t channel : channels) {
                fluid_synth_cc(synth, channel, controller_ns::modulation_wheel_coarse_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::modulation_wheel_fine_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::expression_coarse_controller, 0x7f);
                fluid_synth_cc(synth, channel, controller_ns::expression_fine_controller, 0x7f);
                fluid_synth_cc(synth, channel, controller_ns::hold_pedal_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::portamento_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::sustenuto_pedal_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::soft_pedal_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::legato_pedal_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::hold_2_pedal_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::volume_coarse_controller, 100);
                fluid_synth_cc(synth, channel, controller_ns::volume_fine_controller, 0);
                fluid_synth_cc(synth, channel, controller_ns::pan_position_coarse_controller, 64);
                fluid_synth_cc(synth, channel, controller_ns::pan_position_fine_controller, 0);
                fluid_synth_pitch_wheel_sens(synth, channel, 2);
                fluid_synth_pitch_bend(synth, channel, 0x2000);
                fluid_synth_channel_pressure(synth, channel, 0);
            }
        } else {
            for (channel_t channel : channels)
                fluid_synth_cc(synth, channel, controller, value);
        }
        return result_type::success;
    }

    result_type handle_channel_type(channels_t channels, int type) {
        channels_t old_drum_channels = drums;
        drums.commute(channels, type == CHANNEL_TYPE_DRUM);
        for(channel_t channel : drums ^ old_drum_channels) {
            TRACE_INFO("SoundFont: changed channel " << (int)channel << " type to " << (type == CHANNEL_TYPE_DRUM ? "drum" : "melodic"));
            fluid_synth_set_channel_type(synth, channel, type);
            fluid_synth_program_change(synth, channel, 0);
        }
        return result_type::success;
    }

    result_type handle_channel_pressure(channels_t channels, int pressure) {
        for (channel_t channel : channels)
            fluid_synth_channel_pressure(synth, channel, pressure);
        return result_type::success;
    }

    result_type handle_pitch_wheel(channels_t channels, int pitch) {
        for (channel_t channel : channels)
            fluid_synth_pitch_bend(synth, channel, pitch);
        return result_type::success;
    }

    result_type handle_reset() {
        handle_controller(all_channels, controller_ns::all_sound_off_controller);
        handle_controller(all_channels, controller_ns::all_controllers_off_controller);
        return result_type::success;
    }

    result_type handle_sysex(const Event& event) {
        /// @note master volume does not seem to be handled correctly
        // roland handling
        auto channels = use_for_rhythm_part(event);
        if (channels)
            return handle_channel_type(channels, CHANNEL_TYPE_DRUM);
        // default handling
        std::vector<char> sys_ex_data(event.begin()+1, event.end()-1);
        fluid_synth_sysex(synth, sys_ex_data.data(), sys_ex_data.size(), nullptr, nullptr, nullptr, 0);
        return result_type::success;
    }

    result_type handle_gain(std::string string) {
        fluid_synth_set_gain(synth, (float)unmarshall<double>(string));
        return result_type::success;
    }

    result_type handle_reverb(std::string string) {
        has_reverb = !string.empty();
        if (has_reverb) {
            auto reverb = unmarshall<SoundFontHandler::reverb_type>(string);
            fluid_synth_set_reverb(synth, reverb.roomsize, reverb.damp, reverb.width, reverb.level);
        }
        fluid_synth_set_reverb_on(synth, (int)has_reverb);
        return result_type::success;
    }

    result_type handle_file(std::string string) {
        if (fluid_synth_sfload(synth, string.c_str(), 1) == -1)
            return result_type::fail;
        file = std::move(string);
        return result_type::success;
    }

    void handle_close() {
        fluid_synth_system_reset(synth);
        drums = drum_channels;
    }

    // ----------
    // attributes
    // ----------

    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    std::string file;
    channels_t drums;
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

SoundFontHandler::result_type SoundFontHandler::on_close(state_type state) {
    if (state & handler_ns::receive_state)
        m_pimpl->handle_close();
    return Handler::on_close(state);
}

SoundFontHandler::result_type SoundFontHandler::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    return m_pimpl->handle(message.event);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
