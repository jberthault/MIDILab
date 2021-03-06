/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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

// ----------------------
// generic settings range
// ----------------------

void get_settings_range_impl(fluid_settings_t* settings, const char* name, range_t<double>& range) { fluid_settings_getnum_range(settings, name, &range.min, &range.max); }
void get_settings_range_impl(fluid_settings_t* settings, const char* name, range_t<int>& range) { fluid_settings_getint_range(settings, name, &range.min, &range.max); }

template<typename T>
auto get_settings_range(fluid_settings_t* settings, const char* name) {
    range_t<T> value;
    get_settings_range_impl(settings, name, value);
    return value;
}

#if FLUIDSYNTH_VERSION_MAJOR < 2

// -----
// sfont
// -----

auto get_sfont_name(fluid_sfont_t* sfont) { return sfont->get_name(sfont); }

// ----------------
// settings default
// ----------------

template<typename T> auto get_settings_default(fluid_settings_t* settings, const char* name);
template<> auto get_settings_default<double>(fluid_settings_t* settings, const char* name) { return fluid_settings_getnum_default(settings, name); }
template<> auto get_settings_default<int>(fluid_settings_t* settings, const char* name) { return fluid_settings_getint_default(settings, name); }
template<> auto get_settings_default<bool>(fluid_settings_t* settings, const char* name) { return static_cast<bool>(get_settings_default<int>(settings, name)); }

constexpr auto get_default_reverb_roomsize(fluid_settings_t*) { return FLUID_REVERB_DEFAULT_ROOMSIZE; }
constexpr auto get_default_reverb_damp(fluid_settings_t*) { return FLUID_REVERB_DEFAULT_DAMP; }
constexpr auto get_default_reverb_width(fluid_settings_t*) { return FLUID_REVERB_DEFAULT_WIDTH; }
constexpr auto get_default_reverb_level(fluid_settings_t*) { return FLUID_REVERB_DEFAULT_LEVEL; }

constexpr auto get_default_chorus_nr(fluid_settings_t*) { return FLUID_CHORUS_DEFAULT_N; }
constexpr auto get_default_chorus_level(fluid_settings_t*) { return FLUID_CHORUS_DEFAULT_LEVEL; }
constexpr auto get_default_chorus_speed(fluid_settings_t*) { return FLUID_CHORUS_DEFAULT_SPEED; }
constexpr auto get_default_chorus_depth(fluid_settings_t*) { return FLUID_CHORUS_DEFAULT_DEPTH; }
constexpr auto get_default_chorus_type(fluid_settings_t*) { return FLUID_CHORUS_DEFAULT_TYPE; }

// --------------
// settings range
// --------------

constexpr auto get_range_reverb_roomsize(fluid_settings_t*) { return range_t<double>{0., 1.}; }
constexpr auto get_range_reverb_damp(fluid_settings_t*) { return range_t<double>{0., 1.}; }
constexpr auto get_range_reverb_level(fluid_settings_t*) { return range_t<double>{0., 1.}; }
constexpr auto get_range_reverb_width(fluid_settings_t*) { return range_t<double>{0., 100.}; }

constexpr auto get_range_chorus_nr(fluid_settings_t*) { return range_t<int>{0, 99}; }
constexpr auto get_range_chorus_level(fluid_settings_t*) { return range_t<double>{0., 10.}; }
constexpr auto get_range_chorus_speed(fluid_settings_t*) { return range_t<double>{0.29, 5.}; }
constexpr auto get_range_chorus_depth(fluid_settings_t*) { return range_t<double>{0., 21.}; }

// ---------
// accessors
// ---------

auto get_chorus_speed(fluid_synth_t* synth) { return fluid_synth_get_chorus_speed_Hz(synth); }
auto get_chorus_depth(fluid_synth_t* synth) { return fluid_synth_get_chorus_depth_ms(synth); }

auto set_reverb_roomsize(fluid_synth_t* synth, double value) {
    fluid_synth_set_reverb(synth,
        value,
        fluid_synth_get_reverb_damp(synth),
        fluid_synth_get_reverb_width(synth),
        fluid_synth_get_reverb_level(synth)
    );
    return FLUID_OK;
}

auto set_reverb_damp(fluid_synth_t* synth, double value) {
    fluid_synth_set_reverb(synth,
        fluid_synth_get_reverb_roomsize(synth),
        value,
        fluid_synth_get_reverb_width(synth),
        fluid_synth_get_reverb_level(synth)
    );
    return FLUID_OK;
}

auto set_reverb_level(fluid_synth_t* synth, double value) {
    fluid_synth_set_reverb(synth,
        fluid_synth_get_reverb_roomsize(synth),
        fluid_synth_get_reverb_damp(synth),
        fluid_synth_get_reverb_width(synth),
        value
    );
    return FLUID_OK;
}

auto set_reverb_width(fluid_synth_t* synth, double value) {
    fluid_synth_set_reverb(synth,
        fluid_synth_get_reverb_roomsize(synth),
        fluid_synth_get_reverb_damp(synth),
        value,
        fluid_synth_get_reverb_level(synth)
    );
    return FLUID_OK;
}

auto set_chorus_type(fluid_synth_t* synth, int value) {
    fluid_synth_set_chorus(synth,
        fluid_synth_get_chorus_nr(synth),
        fluid_synth_get_chorus_level(synth),
        get_chorus_speed(synth),
        get_chorus_depth(synth),
        value
    );
    return FLUID_OK;
}

auto set_chorus_nr(fluid_synth_t* synth, int value) {
    fluid_synth_set_chorus(synth,
        value,
        fluid_synth_get_chorus_level(synth),
        get_chorus_speed(synth),
        get_chorus_depth(synth),
        fluid_synth_get_chorus_type(synth)
    );
    return FLUID_OK;
}

auto set_chorus_level(fluid_synth_t* synth, double value) {
    fluid_synth_set_chorus(synth,
        fluid_synth_get_chorus_nr(synth),
        value,
        get_chorus_speed(synth),
        get_chorus_depth(synth),
        fluid_synth_get_chorus_type(synth)
    );
    return FLUID_OK;
}

auto set_chorus_speed(fluid_synth_t* synth, double value) {
    fluid_synth_set_chorus(synth,
        fluid_synth_get_chorus_nr(synth),
        fluid_synth_get_chorus_level(synth),
        value,
        get_chorus_depth(synth),
        fluid_synth_get_chorus_type(synth)
    );
    return FLUID_OK;
}

auto set_chorus_depth(fluid_synth_t* synth, double value) {
    fluid_synth_set_chorus(synth,
        fluid_synth_get_chorus_nr(synth),
        fluid_synth_get_chorus_level(synth),
        get_chorus_speed(synth),
        value,
        fluid_synth_get_chorus_type(synth)
    );
    return FLUID_OK;
}

#else

// -----
// sfont
// -----

auto get_sfont_name(fluid_sfont_t* sfont) { return fluid_sfont_get_name(sfont); }

// ------------------------
// generic settings default
// ------------------------

template<typename T> auto get_settings_default(fluid_settings_t* settings, const char* name);

template<> auto get_settings_default<double>(fluid_settings_t* settings, const char* name) {
    double value;
    fluid_settings_getnum_default(settings, name, &value);
    return value;
}

template<> auto get_settings_default<int>(fluid_settings_t* settings, const char* name) {
    int value;
    fluid_settings_getint_default(settings, name, &value);
    return value;
}

template<> auto get_settings_default<bool>(fluid_settings_t* settings, const char* name) {
    return static_cast<bool>(get_settings_default<int>(settings, name));
}

auto get_default_reverb_roomsize(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.reverb.room-size"); }
auto get_default_reverb_damp(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.reverb.damp"); }
auto get_default_reverb_width(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.reverb.width"); }
auto get_default_reverb_level(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.reverb.level"); }

auto get_default_chorus_nr(fluid_settings_t* settings) { return get_settings_default<int>(settings, "synth.chorus.nr"); }
auto get_default_chorus_level(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.chorus.level"); }
auto get_default_chorus_speed(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.chorus.speed"); }
auto get_default_chorus_depth(fluid_settings_t* settings) { return get_settings_default<double>(settings, "synth.chorus.depth"); }
auto get_default_chorus_type(fluid_settings_t*) { return FLUID_CHORUS_MOD_SINE; }

// --------------
// settings range
// --------------

auto get_range_reverb_roomsize(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.reverb.room-size"); }
auto get_range_reverb_damp(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.reverb.damp"); }
auto get_range_reverb_level(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.reverb.level"); }
auto get_range_reverb_width(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.reverb.width"); }

auto get_range_chorus_nr(fluid_settings_t* settings) { return  get_settings_range<int>(settings, "synth.chorus.nr"); }
auto get_range_chorus_level(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.chorus.level"); }
auto get_range_chorus_speed(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.chorus.speed"); }
auto get_range_chorus_depth(fluid_settings_t* settings) { return get_settings_range<double>(settings, "synth.chorus.depth"); }

// ---------
// accessors
// ---------

auto get_chorus_speed(fluid_synth_t* synth) { return fluid_synth_get_chorus_speed(synth); }
auto get_chorus_depth(fluid_synth_t* synth) { return fluid_synth_get_chorus_depth(synth); }

auto set_reverb_roomsize(fluid_synth_t* synth, double value) { return fluid_synth_set_reverb_roomsize(synth, value); }
auto set_reverb_damp(fluid_synth_t* synth, double value) { return fluid_synth_set_reverb_damp(synth, value); }
auto set_reverb_level(fluid_synth_t* synth, double value) { return fluid_synth_set_reverb_level(synth, value); }
auto set_reverb_width(fluid_synth_t* synth, double value) { return fluid_synth_set_reverb_width(synth, value); }

auto set_chorus_type(fluid_synth_t* synth, int value) { return fluid_synth_set_chorus_type(synth, value); }
auto set_chorus_nr(fluid_synth_t* synth, int value) { return fluid_synth_set_chorus_nr(synth, value); }
auto set_chorus_level(fluid_synth_t* synth, double value) { return fluid_synth_set_chorus_level(synth, value); }
auto set_chorus_speed(fluid_synth_t* synth, double value) { return fluid_synth_set_chorus_speed(synth, value); }
auto set_chorus_depth(fluid_synth_t* synth, double value) { return fluid_synth_set_chorus_depth(synth, value); }

#endif

struct FluidSettings {

    FluidSettings() = default;

    ~FluidSettings() { delete_fluid_settings(m_settings); }

    operator fluid_settings_t*() { return m_settings; }

    fluid_settings_t* m_settings {new_fluid_settings()};

};

}

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
        reverb_activated = value;
        fluid_synth_set_reverb_on(synth, static_cast<int>(value));
        return Result::success;
    }

    auto handle_reverb_roomsize(double value) { return to_result(set_reverb_roomsize(synth, value)); }
    auto handle_reverb_damp(double value) { return to_result(set_reverb_damp(synth, value)); }
    auto handle_reverb_level(double value) { return to_result(set_reverb_level(synth, value)); }
    auto handle_reverb_width(double value) { return to_result(set_reverb_width(synth, value)); }

    auto handle_chorus_activated(bool value) {
        chorus_activated = value;
        fluid_synth_set_chorus_on(synth, static_cast<int>(value));
        return Result::success;
    }

    auto handle_chorus_type(int value) { return to_result(set_chorus_type(synth, value));  }
    auto handle_chorus_nr(int value) { return to_result(set_chorus_nr(synth, value)); }
    auto handle_chorus_level(double value) { return to_result(set_chorus_level(synth, value)); }
    auto handle_chorus_speed(double value) { return to_result(set_chorus_speed(synth, value)); }
    auto handle_chorus_depth(double value) { return to_result(set_chorus_depth(synth, value)); }

    void handle_close() {
        fluid_synth_system_reset(synth);
        drums = channels_t::drums();
    }

    FluidSettings settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    channels_t drums {channels_t::drums()};
    bool reverb_activated {SoundFontHandler::ext.reverb.activated.default_value};
    bool chorus_activated {SoundFontHandler::ext.chorus.activated.default_value};

};

//==================
// SoundFontHandler
//==================

const SoundFontExtensions SoundFontHandler::ext = [] {
    FluidSettings settings;
    return SoundFontExtensions {
        {"SoundFont.gain", get_settings_default<double>(settings, "synth.gain"), get_settings_range<double>(settings, "synth.gain")},
        {"SoundFont.file"},
        {
            {"SoundFont.reverb_activated", get_settings_default<bool>(settings, "synth.reverb.active")},
            {"SoundFont.reverb_roomsize", get_default_reverb_roomsize(settings), get_range_reverb_roomsize(settings)},
            {"SoundFont.reverb_damp", get_default_reverb_damp(settings), get_range_reverb_damp(settings)},
            {"SoundFont.reverb_level", get_default_reverb_level(settings), get_range_reverb_level(settings)},
            {"SoundFont.reverb_width", get_default_reverb_width(settings), get_range_reverb_width(settings)}
        },
        {
            {"SoundFont.chorus_activated", get_settings_default<bool>(settings, "synth.chorus.active")},
            {"SoundFont.chorus_type", get_default_chorus_type(settings)},
            {"SoundFont.chorus_nr", get_default_chorus_nr(settings), get_range_chorus_nr(settings)},
            {"SoundFont.chorus_level", get_default_chorus_level(settings), get_range_chorus_level(settings)},
            {"SoundFont.chorus_speed", get_default_chorus_speed(settings), get_range_chorus_speed(settings)},
            {"SoundFont.chorus_depth", get_default_chorus_depth(settings), get_range_chorus_depth(settings)}
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
        result.assign(get_sfont_name(sfont));
    return result;
}

bool SoundFontHandler::reverb_activated() const {
    return m_pimpl->reverb_activated;
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
    return m_pimpl->chorus_activated;
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
    return get_chorus_speed(m_pimpl->synth);
}

double SoundFontHandler::chorus_depth() const {
    return get_chorus_depth(m_pimpl->synth);
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
