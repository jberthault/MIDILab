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
        ss >> reverb.roomsize >> reverb.damp >> reverb.level >> reverb.width;
        return reverb;
    }
};

template<>
struct marshalling_traits<SoundFontHandler::chorus_type> {
    auto operator()(const SoundFontHandler::chorus_type& chorus) {
        std::stringstream ss;
        ss << chorus.type << ' ' << chorus.nr << ' ' << chorus.level << ' ' << chorus.speed << ' ' << chorus.depth;
        return ss.str();
    }
};

template<>
struct unmarshalling_traits<SoundFontHandler::chorus_type> {
    auto operator()(const std::string& string) {
        SoundFontHandler::chorus_type chorus;
        std::stringstream ss(string);
        ss >> chorus.type >> chorus.nr >> chorus.level >> chorus.speed >> chorus.depth;
        return chorus;
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
        return channels_t::wrap(event.at(7) & 0x0f);
    return {};
}

}

//======
// Impl
//======

struct SoundFontHandler::Impl {

    using Result = Handler::Result;
    using reverb_type = SoundFontHandler::reverb_type;
    using chorus_type = SoundFontHandler::chorus_type;

    // ---------
    // structors
    // ---------

    Impl() : drums(channels_t::drums()), has_reverb(true), has_chorus(true) {
        settings = new_fluid_settings();
        fluid_settings_setint(settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
        fluid_settings_setstr(settings, "audio.jack.id", "MIDILab");
        synth = new_fluid_synth(settings);
        adriver = new_fluid_audio_driver(settings, synth);
        // chorus seems to be zero until a value is set, so we force the default settings
        fluid_synth_set_chorus(synth, FLUID_CHORUS_DEFAULT_N, FLUID_CHORUS_DEFAULT_LEVEL, FLUID_CHORUS_DEFAULT_SPEED, FLUID_CHORUS_DEFAULT_DEPTH, FLUID_CHORUS_DEFAULT_TYPE);
    }

    ~Impl() {
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    // ---------
    // accessors
    // ---------

    auto gain() const {
        return (double)fluid_synth_get_gain(synth);
    }

    static auto default_reverb() {
        return reverb_type{FLUID_REVERB_DEFAULT_ROOMSIZE, FLUID_REVERB_DEFAULT_DAMP, FLUID_REVERB_DEFAULT_LEVEL, FLUID_REVERB_DEFAULT_WIDTH};
    }

    auto reverb() const {
        return reverb_type{
            fluid_synth_get_reverb_roomsize(synth),
            fluid_synth_get_reverb_damp(synth),
            fluid_synth_get_reverb_level(synth),
            fluid_synth_get_reverb_width(synth)
        };
    }

    static auto default_chorus() {
        return chorus_type{FLUID_CHORUS_DEFAULT_TYPE, FLUID_CHORUS_DEFAULT_N, FLUID_CHORUS_DEFAULT_LEVEL, FLUID_CHORUS_DEFAULT_SPEED, FLUID_CHORUS_DEFAULT_DEPTH};
    }

    auto chorus() const {
        return chorus_type{
            fluid_synth_get_chorus_type(synth),
            fluid_synth_get_chorus_nr(synth),
            fluid_synth_get_chorus_level(synth),
            fluid_synth_get_chorus_speed_Hz(synth),
            fluid_synth_get_chorus_depth_ms(synth)
        };
    }

    // --------------
    // handle methods
    // --------------

    static constexpr auto handled_families() {
        return families_t::fuse(
            family_t::note_off,
            family_t::note_on,
            family_t::program_change,
            family_t::controller,
            family_t::channel_pressure,
            family_t::pitch_wheel,
            family_t::reset,
            family_t::sysex,
            family_t::custom
        );
    }

    Result handle(const Event& event) {
        switch (event.family()) {
        case family_t::note_off: return handle_note_off(event.channels(), event.at(1));
        case family_t::note_on: return handle_note_on(event.channels(), event.at(1), event.at(2));
        case family_t::program_change: return handle_program_change(event.channels(), event.at(1));
        case family_t::controller: return handle_controller(event.channels(), event.at(1), event.at(2));
        case family_t::channel_pressure: return handle_channel_pressure(event.channels(), event.at(1));
        case family_t::pitch_wheel: return handle_pitch_wheel(event.channels(), event.get_14bits());
        case family_t::reset: return handle_reset();
        case family_t::sysex: return handle_sysex(event);
        case family_t::custom:
            if (event.has_custom_key("SoundFont.gain")) return handle_gain(event.get_custom_value());
            if (event.has_custom_key("SoundFont.reverb")) return handle_reverb(event.get_custom_value());
            if (event.has_custom_key("SoundFont.chorus")) return handle_chorus(event.get_custom_value());
            if (event.has_custom_key("SoundFont.file")) return handle_file(event.get_custom_value());
            break;
        }
        return Result::unhandled;
    }

    Result handle_note_off(channels_t channels, int note) {
        for (channel_t channel : channels)
            fluid_synth_noteoff(synth, channel, note);
        return Result::success;
    }

    Result handle_note_on(channels_t channels, int note, int velocity) {
        for (channel_t channel : channels)
            fluid_synth_noteon(synth, channel, note, velocity);
        return Result::success;
    }

    Result handle_program_change(channels_t channels, int program) {
        for (channel_t channel : channels)
            fluid_synth_program_change(synth, channel, program);
        return Result::success;
    }

    Result handle_controller(channels_t channels, byte_t controller, int value) {
        for (channel_t channel : channels)
            fluid_synth_cc(synth, channel, controller, value);
        return Result::success;
    }

    Result handle_channel_type(channels_t channels, int type) {
        channels_t old_drum_channels = drums;
        drums.commute(channels, type == CHANNEL_TYPE_DRUM);
        for (channel_t channel : drums ^ old_drum_channels) {
            TRACE_INFO("SoundFont: changed channel " << (int)channel << " type to " << (type == CHANNEL_TYPE_DRUM ? "drum" : "melodic"));
            fluid_synth_set_channel_type(synth, channel, type);
            fluid_synth_program_change(synth, channel, 0);
        }
        return Result::success;
    }

    Result handle_channel_pressure(channels_t channels, int pressure) {
        for (channel_t channel : channels)
            fluid_synth_channel_pressure(synth, channel, pressure);
        return Result::success;
    }

    Result handle_pitch_wheel(channels_t channels, int pitch) {
        for (channel_t channel : channels)
            fluid_synth_pitch_bend(synth, channel, pitch);
        return Result::success;
    }

    Result handle_reset() {
        handle_channel_type(channels_t::melodic(), CHANNEL_TYPE_MELODIC);
        handle_channel_type(channels_t::drums(), CHANNEL_TYPE_DRUM);
        for (byte_t controller : controller_ns::reset_controllers)
            handle_controller(channels_t::full(), controller, controller_ns::default_value(controller));
        for (channel_t channel : channels_t::full())
            fluid_synth_pitch_wheel_sens(synth, channel, 2);
        return Result::success;
    }

    Result handle_sysex(const Event& event) {
        /// @note master volume does not seem to be handled correctly
        // roland handling
        if (auto channels = use_for_rhythm_part(event))
            return handle_channel_type(channels, CHANNEL_TYPE_DRUM);
        // default handling
        std::vector<char> sys_ex_data(event.begin()+1, event.end()-1);
        fluid_synth_sysex(synth, sys_ex_data.data(), sys_ex_data.size(), nullptr, nullptr, nullptr, 0);
        return Result::success;
    }

    Result handle_gain(const std::string& string) {
        fluid_synth_set_gain(synth, (float)unmarshall<double>(string));
        return Result::success;
    }

    Result handle_reverb(const std::string& string) {
        has_reverb = !string.empty();
        if (has_reverb) {
            auto reverb = unmarshall<reverb_type>(string);
            fluid_synth_set_reverb(synth, reverb.roomsize, reverb.damp, reverb.width, reverb.level);
        }
        fluid_synth_set_reverb_on(synth, (int)has_reverb);
        return Result::success;
    }

    Result handle_chorus(const std::string& string) {
        has_chorus = !string.empty();
        if (has_chorus) {
            auto chorus = unmarshall<chorus_type>(string);
            fluid_synth_set_chorus(synth, chorus.nr, chorus.level, chorus.speed, chorus.depth, chorus.type);
        }
        fluid_synth_set_chorus_on(synth, (int)has_chorus);
        return Result::success;
    }

    Result handle_file(std::string string) {
        TRACE_MEASURE("SoundFont handle_file");
        if (fluid_synth_sfload(synth, string.c_str(), 1) == -1)
            return Result::fail;
        file = std::move(string);
        return Result::success;
    }

    void handle_close() {
        fluid_synth_system_reset(synth);
        drums = channels_t::drums();
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
    bool has_chorus;

};

//==================
// SoundFontHandler
//==================

SoundFontHandler::reverb_type SoundFontHandler::default_reverb() {
    return Impl::default_reverb();
}

SoundFontHandler::chorus_type SoundFontHandler::default_chorus() {
    return Impl::default_chorus();
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
    return Event::custom({}, "SoundFont.reverb");
}

Event SoundFontHandler::chorus_event(const optional_chorus_type& chorus) {
    if (chorus)
        return Event::custom({}, "SoundFont.chorus", marshall(*chorus));
    return Event::custom({}, "SoundFont.chorus");
}

SoundFontHandler::SoundFontHandler() : Handler(Mode::out()), m_pimpl(std::make_unique<Impl>()) {

}

SoundFontHandler::~SoundFontHandler() {

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

SoundFontHandler::optional_chorus_type SoundFontHandler::chorus() const {
    SoundFontHandler::optional_chorus_type result;
    if (m_pimpl->has_chorus)
        result = m_pimpl->chorus();
    return result;
}

families_t SoundFontHandler::handled_families() const {
    return Impl::handled_families();
}

SoundFontHandler::Result SoundFontHandler::on_close(State state) {
    if (state & State::receive())
        m_pimpl->handle_close();
    return Handler::on_close(state);
}

SoundFontHandler::Result SoundFontHandler::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    return m_pimpl->handle(message.event);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
