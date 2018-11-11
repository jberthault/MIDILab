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

#include "soundfont.h"

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <sstream>
#include <fluidsynth.h>

namespace {

constexpr auto to_result(int rc) {
    return rc == FLUID_FAILED ? Handler::Result::fail : Handler::Result::success;
}

void settings_get_range(fluid_settings_t* settings, const char* name, range_t<double>& range) {
    fluid_settings_getnum_range(settings, name, &range.min, &range.max);
}

void settings_get_range(fluid_settings_t* settings, const char* name, range_t<int>& range) {
    fluid_settings_getint_range(settings, name, &range.min, &range.max);
}

void settings_get_default(fluid_settings_t* settings, const char* name, double& value) {
    fluid_settings_getnum_default(settings, name, &value);
}

void settings_get_default(fluid_settings_t* settings, const char* name, int& value) {
    fluid_settings_getint_default(settings, name, &value);
}

void settings_get_default(fluid_settings_t* settings, const char* name, bool& value) {
    int int_value;
    fluid_settings_getint_default(settings, name, &int_value);
    value = static_cast<bool>(int_value);
}

template<typename T>
auto settings_default(fluid_settings_t* settings, const char* name) {
    T value;
    settings_get_default(settings, name, value);
    return value;
}

template<typename T>
auto settings_range(fluid_settings_t* settings, const char* name) {
    range_t<T> value;
    settings_get_range(settings, name, value);
    return value;
}

}

struct FluidSettings {

    FluidSettings() = default;

    ~FluidSettings() { delete_fluid_settings(m_settings); }

    operator fluid_settings_t*() { return m_settings; }

    fluid_settings_t* m_settings {new_fluid_settings()};

};

//======
// Impl
//======

struct SoundFontHandler::Impl {

    Impl() {
        fluid_settings_setint(settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
        fluid_settings_setstr(settings, "audio.jack.id", "MIDILab");
        synth = new_fluid_synth(settings);
        adriver = new_fluid_audio_driver(settings, synth);
        if (!adriver)
            TRACE_ERROR("unable to build audio driver");
    }

    ~Impl() {
        if (adriver)
            delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
    }

    template<typename CallableT, typename ... Args>
    auto handle_voice(CallableT callable, channels_t channels, Args ... args) {
        for (channel_t channel : channels)
            callable(synth, static_cast<int>(channel), args...);
        return Result::success;
    }

    auto handle_note_off(channels_t channels, int note) {
        return handle_voice(fluid_synth_noteoff, channels, note);
    }

    auto handle_note_on(channels_t channels, int note, int velocity) {
        return handle_voice(fluid_synth_noteon, channels, note, velocity);
    }

    auto handle_program_change(channels_t channels, int program) {
        return handle_voice(fluid_synth_program_change, channels, program);
    }

    auto handle_controller(channels_t channels, byte_t controller, int value) {
        return handle_voice(fluid_synth_cc, channels, controller, value);
    }

    auto handle_raw_channel_type(channels_t channels, int type) {
        for (channel_t channel : channels)
            TRACE_INFO("SoundFont: changed channel " << static_cast<int>(channel) << " type to " << (type == CHANNEL_TYPE_DRUM ? "drum" : "melodic"));
        handle_voice(fluid_synth_set_channel_type, channels, type);
        return handle_program_change(channels, 0);
    }

    auto handle_channel_type(channels_t channels, int type) {
        const auto previous_drums = drums;
        drums.commute(channels, type == CHANNEL_TYPE_DRUM);
        return handle_raw_channel_type(drums ^ previous_drums, type);
    }

    auto handle_channel_pressure(channels_t channels, int pressure) {
        return handle_voice(fluid_synth_channel_pressure, channels, pressure);
    }

    auto handle_pitch_wheel(channels_t channels, int pitch) {
        return handle_voice(fluid_synth_pitch_bend, channels, pitch);
    }

    auto handle_reset() {
        handle_channel_type(channels_t::melodic(), CHANNEL_TYPE_MELODIC);
        handle_channel_type(channels_t::drums(), CHANNEL_TYPE_DRUM);
        for (byte_t controller : controller_ns::reset_controllers)
            handle_controller(channels_t::full(), controller, controller_ns::default_value(controller));
        return handle_voice(fluid_synth_pitch_wheel_sens, channels_t::full(), 2);
    }

    auto handle_sysex(const Event& event) {
        /// @note master volume does not seem to be handled correctly
        // roland handling
        if (const auto channels = extraction_ns::use_for_rhythm_part(event))
            return handle_channel_type(channels, CHANNEL_TYPE_DRUM);
        // default handling
        auto view = extraction_ns::dynamic_view(event);
        ++view.min; // skip status
        --view.max; // skip end-of-sysex
        return to_result(fluid_synth_sysex(synth, reinterpret_cast<const char*>(view.min), static_cast<int>(span(view)), nullptr, nullptr, nullptr, 0));
    }

    auto handle_gain(double gain) {
        fluid_synth_set_gain(synth, static_cast<float>(gain));
        return Result::success;
    }

    auto handle_file(std::string string) {
        TRACE_MEASURE("SoundFont handle_file");
        return to_result(fluid_synth_sfload(synth, string.c_str(), 1));
    }

    auto handle_reverb_activated(bool value) {
        has_reverb = value;
        fluid_synth_set_reverb_on(synth, static_cast<int>(value));
        return Result::success;
    }

    auto handle_reverb_roomsize(double value) {
        return to_result(fluid_synth_set_reverb_roomsize(synth, value));
    }

    auto handle_reverb_damp(double value) {
        return to_result(fluid_synth_set_reverb_damp(synth, value));
    }

    auto handle_reverb_level(double value) {
        return to_result(fluid_synth_set_reverb_level(synth, value));
    }

    auto handle_reverb_width(double value) {
        return to_result(fluid_synth_set_reverb_width(synth, value));
    }

    auto handle_chorus_activated(bool value) {
        has_chorus = value;
        fluid_synth_set_chorus_on(synth, static_cast<int>(value));
        return Result::success;
    }

    auto handle_chorus_type(int value) {
        return to_result(fluid_synth_set_chorus_type(synth, value));
    }

    auto handle_chorus_nr(int value) {
        return to_result(fluid_synth_set_chorus_nr(synth, value));
    }

    auto handle_chorus_level(double value) {
        return to_result(fluid_synth_set_chorus_level(synth, value));
    }

    auto handle_chorus_speed(double value) {
        return to_result(fluid_synth_set_chorus_speed(synth, value));
    }

    auto handle_chorus_depth(double value) {
        return to_result(fluid_synth_set_chorus_depth(synth, value));
    }

    void handle_close() {
        fluid_synth_system_reset(synth);
        drums = channels_t::drums();
    }

    FluidSettings settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    channels_t drums {channels_t::drums()};
    bool has_reverb {SoundFontHandler::ext.reverb.activated.default_value};
    bool has_chorus {SoundFontHandler::ext.chorus.activated.default_value};

};

//==================
// SoundFontHandler
//==================

const SoundFontExtensions SoundFontHandler::ext = [] {
    FluidSettings settings;
    return SoundFontExtensions {
        {"SoundFont.gain", settings_default<double>(settings, "synth.gain"), settings_range<double>(settings, "synth.gain")},
        {"SoundFont.file"},
        {
            {"SoundFont.reverb_activated", settings_default<bool>(settings, "synth.reverb.active")},
            {"SoundFont.reverb_roomsize", settings_default<double>(settings, "synth.reverb.room-size"), settings_range<double>(settings, "synth.reverb.room-size")},
            {"SoundFont.reverb_damp", settings_default<double>(settings, "synth.reverb.damp"), settings_range<double>(settings, "synth.reverb.damp")},
            {"SoundFont.reverb_level", settings_default<double>(settings, "synth.reverb.level"), settings_range<double>(settings, "synth.reverb.level")},
            {"SoundFont.reverb_width", settings_default<double>(settings, "synth.reverb.width"), settings_range<double>(settings, "synth.reverb.width")}
        },
        {
            {"SoundFont.chorus_activated", settings_default<bool>(settings, "synth.chorus.active")},
            {"SoundFont.chorus_type", FLUID_CHORUS_MOD_SINE},
            {"SoundFont.chorus_nr", settings_default<int>(settings, "synth.chorus.nr"), settings_range<int>(settings, "synth.chorus.nr")},
            {"SoundFont.chorus_level", settings_default<double>(settings, "synth.chorus.level"), settings_range<double>(settings, "synth.chorus.level")},
            {"SoundFont.chorus_speed", settings_default<double>(settings, "synth.chorus.speed"), settings_range<double>(settings, "synth.chorus.speed")},
            {"SoundFont.chorus_depth", settings_default<double>(settings, "synth.chorus.depth"), settings_range<double>(settings, "synth.chorus.depth")}
        }
    };
}();

SoundFontHandler::SoundFontHandler() : Handler{Mode::out()}, m_pimpl{std::make_unique<Impl>()} {

}

SoundFontHandler::~SoundFontHandler() {

}

double SoundFontHandler::gain() const {
    return static_cast<double>(fluid_synth_get_gain(m_pimpl->synth));
}

std::string SoundFontHandler::file() const {
    std::string result;
    if (auto* sfont = fluid_synth_get_sfont(m_pimpl->synth, 0))
        result.assign(fluid_sfont_get_name(sfont));
    return result;
}

bool SoundFontHandler::reverb_activated() const {
    return m_pimpl->has_reverb;
}

double SoundFontHandler::reverb_roomsize() const {
    return fluid_synth_get_reverb_roomsize(m_pimpl->synth);
}

double SoundFontHandler::reverb_damp() const {
    return fluid_synth_get_reverb_damp(m_pimpl->synth);
}

double SoundFontHandler::reverb_level() const {
    return fluid_synth_get_reverb_level(m_pimpl->synth);
}

double SoundFontHandler::reverb_width() const {
    return fluid_synth_get_reverb_width(m_pimpl->synth);
}

bool SoundFontHandler::chorus_activated() const {
    return m_pimpl->has_chorus;
}

int SoundFontHandler::chorus_type() const {
    return fluid_synth_get_chorus_type(m_pimpl->synth);
}

int SoundFontHandler::chorus_nr() const {
    return fluid_synth_get_chorus_nr(m_pimpl->synth);
}

double SoundFontHandler::chorus_level() const {
    return fluid_synth_get_chorus_level(m_pimpl->synth);
}

double SoundFontHandler::chorus_speed() const {
    return fluid_synth_get_chorus_speed(m_pimpl->synth);
}

double SoundFontHandler::chorus_depth() const {
    return fluid_synth_get_chorus_depth(m_pimpl->synth);
}

families_t SoundFontHandler::handled_families() const {
    return families_t::fuse(
        family_t::note_off,
        family_t::note_on,
        family_t::program_change,
        family_t::controller,
        family_t::channel_pressure,
        family_t::pitch_wheel,
        family_t::reset,
        family_t::sysex,
        family_t::extended_system
    );
}

SoundFontHandler::Result SoundFontHandler::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::note_off: return m_pimpl->handle_note_off(message.event.channels(), extraction_ns::note(message.event));
    case family_t::note_on: return m_pimpl->handle_note_on(message.event.channels(), extraction_ns::note(message.event), extraction_ns::velocity(message.event));
    case family_t::program_change: return m_pimpl->handle_program_change(message.event.channels(), extraction_ns::program(message.event));
    case family_t::controller: return m_pimpl->handle_controller(message.event.channels(), extraction_ns::controller(message.event), extraction_ns::controller_value(message.event));
    case family_t::channel_pressure: return m_pimpl->handle_channel_pressure(message.event.channels(), extraction_ns::channel_pressure(message.event));
    case family_t::pitch_wheel: return m_pimpl->handle_pitch_wheel(message.event.channels(), extraction_ns::get_14bits(message.event));
    case family_t::reset: return m_pimpl->handle_reset();
    case family_t::sysex: return m_pimpl->handle_sysex(message.event);
    case family_t::extended_system:
        if (ext.gain.affects(message.event)) return m_pimpl->handle_gain(ext.gain.decode(message.event));
        if (ext.file.affects(message.event)) return m_pimpl->handle_file(ext.file.decode(message.event));
        if (ext.reverb.activated.affects(message.event)) return m_pimpl->handle_reverb_activated(ext.reverb.activated.decode(message.event));
        if (ext.reverb.roomsize.affects(message.event)) return m_pimpl->handle_reverb_roomsize(ext.reverb.roomsize.decode(message.event));
        if (ext.reverb.damp.affects(message.event)) return m_pimpl->handle_reverb_damp(ext.reverb.damp.decode(message.event));
        if (ext.reverb.level.affects(message.event)) return m_pimpl->handle_reverb_level(ext.reverb.level.decode(message.event));
        if (ext.reverb.width.affects(message.event)) return m_pimpl->handle_reverb_width(ext.reverb.width.decode(message.event));
        if (ext.chorus.activated.affects(message.event)) return m_pimpl->handle_chorus_activated(ext.chorus.activated.decode(message.event));
        if (ext.chorus.type.affects(message.event)) return m_pimpl->handle_chorus_type(ext.chorus.type.decode(message.event));
        if (ext.chorus.nr.affects(message.event)) return m_pimpl->handle_chorus_nr(ext.chorus.nr.decode(message.event));
        if (ext.chorus.level.affects(message.event)) return m_pimpl->handle_chorus_level(ext.chorus.level.decode(message.event));
        if (ext.chorus.speed.affects(message.event)) return m_pimpl->handle_chorus_speed(ext.chorus.speed.decode(message.event));
        if (ext.chorus.depth.affects(message.event)) return m_pimpl->handle_chorus_depth(ext.chorus.depth.decode(message.event));
        break;
    default:
        break;
    }
    return Result::unhandled;
}

SoundFontHandler::Result SoundFontHandler::handle_close(State state) {
    if (state & State::receive())
        m_pimpl->handle_close();
    return Handler::handle_close(state);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
