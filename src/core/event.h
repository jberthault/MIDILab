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

#ifndef CORE_EVENT_H
#define CORE_EVENT_H

#include <array>
#include <map>
#include <unordered_map>
#include <memory>
#include "note.h"
#include "tools/bytes.h"
#include "tools/flags.h"

//======
// View
//======

using byte_cview = range_t<const byte_t*>;
using byte_view = range_t<byte_t*>;

byte_cview make_view(const char* string);
byte_cview make_view(const char* string, size_t size);
byte_cview make_view(const std::string& string);
byte_cview make_view(std::string&&) = delete;

template<size_t N>
byte_cview make_view(const byte_t (& array)[N]) {
    return {array, array + N};
}

//=======
// Track
//=======

using track_t = uint16_t;

constexpr track_t default_track {0};

//=======
// Short
//=======

/// @todo find a better name

namespace short_ns {

static constexpr byte_t coarse(uint16_t value) {
    return (value >> 7) & 0x7f;
}

static constexpr byte_t fine(uint16_t value) {
    return value & 0x7f;
}

static constexpr uint16_t alter_coarse(uint16_t value, byte_t coarse) {
    return ((coarse & 0x7f) << 7) | (value & 0x007f);
}

static constexpr uint16_t alter_fine(uint16_t value, byte_t fine) {
    return (value & 0x3f80) | (fine & 0x7f);
}

struct uint14_t {
    byte_t coarse;
    byte_t fine;
};

static constexpr uint14_t cut(uint16_t value) {
    return {coarse(value), fine(value)};
}

static constexpr uint16_t glue(uint14_t value) {
    return (value.coarse << 7) | value.fine;
}

}

//=========
// Channel
//=========

using channel_t = uint8_t; /*!< channel type differs from byte type due to its limited range */

struct channels_t : flags_t<channels_t, channel_t, 16> {
    using flags_t::flags_t;

    static constexpr auto full() { return from_integral(0xffff); }
    static constexpr auto drum() { return channel_t{9}; }
    static constexpr auto drums() { return wrap(drum()); }
    static constexpr auto melodic() { return ~drums(); }

};

template<> inline auto marshall<channels_t>(const channels_t& channels) { return marshall(channels.to_integral()); }
template<> inline auto unmarshall<channels_t>(const std::string& string) { return channels_t::from_integral(unmarshall<channels_t::storage_type>(string)); }

namespace channel_ns {

std::string channel_string(channel_t channel);
std::string channels_string(channels_t channels);

// dense map<channel_t, T>
template<typename T>
using map_type = std::array<T, channels_t::capacity()>;

// channels_t[]
template<size_t N>
using array_type = std::array<channels_t, N>;

// sparse map<T, channels>
template<typename T>
using rmap_type = std::unordered_map<T, channels_t>;

template<size_t N>
void clear(array_type<N>& array, channels_t channels = channels_t::full()) {
    if (channels == channels_t::full())
        array.fill({});
    else
        for (channels_t& cs : array)
            cs &= ~channels;
}

template<size_t N>
auto aggregate(const array_type<N>& array) {
    channels_t channels;
    for (channels_t cs : array)
        channels |= cs;
    return channels;
}

template<size_t N>
auto contains(const array_type<N>& array, channels_t channels) {
    for (channels_t cs : array)
        if (cs & channels)
            return true;
    return false;
}

template<typename T, typename U>
auto find(const map_type<T>& map, U value) {
    channels_t channels;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++)
        if (map[c] == value)
            channels.set(c);
    return channels;
}

template<typename T, typename U>
void store(map_type<T>& map, channels_t channels, U value) {
    for (channel_t c : channels)
        map[c] = value;
}

template<typename T>
auto reverse(const map_type<T>& map, channels_t channels) {
    rmap_type<T> rmap;
    for (channel_t c : channels)
        rmap[map[c]].set(c);
    return rmap;
}

}

template<typename T>
using channel_map_t = channel_ns::map_type<T>;

//======
// Drum
//======

namespace drum_ns {

std::ostream& print_drum(std::ostream& stream, byte_t byte);

static constexpr byte_t high_q_drum{27};
static constexpr byte_t slap_drum{28};
static constexpr byte_t scratch_push_drum{29};
static constexpr byte_t scratch_pull_drum{30};
static constexpr byte_t sticks_drum{31};
static constexpr byte_t square_click_drum{32};
static constexpr byte_t metronome_click_drum{33};
static constexpr byte_t metronome_bell_drum{34};
static constexpr byte_t bass_2_drum{35};
static constexpr byte_t bass_1_drum{36};
static constexpr byte_t sidestick_drum{37};
static constexpr byte_t snare_1_drum{38};
static constexpr byte_t handclap_drum{39};
static constexpr byte_t snare_2_drum{40};
static constexpr byte_t low_tom_2_drum{41};
static constexpr byte_t closed_hihat_drum{42};
static constexpr byte_t low_tom_1_drum{43};
static constexpr byte_t pedal_hihat_drum{44};
static constexpr byte_t mid_tom_2_drum{45};
static constexpr byte_t open_hihat_drum{46};
static constexpr byte_t mid_tom_1_drum{47};
static constexpr byte_t high_tom_2_drum{48};
static constexpr byte_t crash_cymbal_1_drum{49};
static constexpr byte_t high_tom_1_drum{50};
static constexpr byte_t ride_cymbal_1_drum{51};
static constexpr byte_t chinese_cymbal_drum{52};
static constexpr byte_t ride_bell_drum{53};
static constexpr byte_t tambourine_drum{54};
static constexpr byte_t splash_cymbal_drum{55};
static constexpr byte_t cowbell_drum{56};
static constexpr byte_t crash_cymbal_2_drum{57};
static constexpr byte_t vibra_slap_drum{58};
static constexpr byte_t ride_cymbal_2_drum{59};
static constexpr byte_t high_bongo_drum{60};
static constexpr byte_t low_bongo_drum{61};
static constexpr byte_t mute_high_conga_drum{62};
static constexpr byte_t open_high_conga_drum{63};
static constexpr byte_t low_conga_drum{64};
static constexpr byte_t high_timbale_drum{65};
static constexpr byte_t low_timbale_drum{66};
static constexpr byte_t high_agogo_drum{67};
static constexpr byte_t low_agogo_drum{68};
static constexpr byte_t cabasa_drum{69};
static constexpr byte_t maracas_drum{70};
static constexpr byte_t short_whistle_drum{71};
static constexpr byte_t long_whistle_drum{72};
static constexpr byte_t short_guiro_drum{73};
static constexpr byte_t long_guiro_drum{74};
static constexpr byte_t claves_drum{75};
static constexpr byte_t high_wood_drum{76};
static constexpr byte_t low_wood_drum{77};
static constexpr byte_t mute_cuica_drum{78};
static constexpr byte_t open_cuica_drum{79};
static constexpr byte_t mute_triangle_drum{80};
static constexpr byte_t open_triangle_drum{81};
static constexpr byte_t shaker_drum{82};
static constexpr byte_t jingle_bell_drum{83};
static constexpr byte_t bell_tree_drum{84};
static constexpr byte_t castinets_drum{85};
static constexpr byte_t mute_surdo_drum{86};
static constexpr byte_t open_surdo_drum{87};

}

//============
// Controller
//============
// undefined controllers: (0x03 0x23) (0x09 0x29) (0x0e 0x2e) (0x0f 0x2f) [0x14 0x1f] [0x54 0x5a] [0x66 0x77]

namespace controller_ns {

static constexpr short_ns::uint14_t bank_select_controller = {0x00, 0x20};
static constexpr short_ns::uint14_t modulation_wheel_controller = {0x01, 0x21};
static constexpr short_ns::uint14_t breath_controller = {0x02, 0x22};
static constexpr short_ns::uint14_t foot_pedal_controller = {0x04, 0x24};
static constexpr short_ns::uint14_t portamento_time_controller = {0x05, 0x25};
static constexpr short_ns::uint14_t data_entry_controller = {0x06, 0x26};
static constexpr short_ns::uint14_t volume_controller = {0x07, 0x27};
static constexpr short_ns::uint14_t balance_controller = {0x08, 0x28};
static constexpr short_ns::uint14_t pan_position_controller = {0x0a, 0x2a};
static constexpr short_ns::uint14_t expression_controller = {0x0b, 0x2b};
static constexpr short_ns::uint14_t effect_control_controllers[] = {{0x0c, 0x2c}, {0x0d, 0x2d}};
static constexpr byte_t general_purpose_slider_controllers[] = {0x10, 0x11, 0x12, 0x13};
static constexpr byte_t hold_pedal_controller{0x40};
static constexpr byte_t portamento_controller{0x41};
static constexpr byte_t sustenuto_pedal_controller{0x42};
static constexpr byte_t soft_pedal_controller{0x43};
static constexpr byte_t legato_pedal_controller{0x44};
static constexpr byte_t hold_2_pedal_controller{0x45};
static constexpr byte_t sound_controllers[] = {0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
static constexpr byte_t general_purpose_button_controllers[] = {0x50, 0x51, 0x52, 0x53};
static constexpr byte_t effects_depth_controllers[] = {0x5b, 0x5c, 0x5d, 0x5e, 0x5f};
static constexpr byte_t data_button_increment_controller{0x60};
static constexpr byte_t data_button_decrement_controller{0x61};
static constexpr short_ns::uint14_t non_registered_parameter_controller = {0x63, 0x62};
static constexpr short_ns::uint14_t registered_parameter_controller = {0x65, 0x64};
static constexpr byte_t all_sound_off_controller{0x78};
static constexpr byte_t all_controllers_off_controller{0x79};
static constexpr byte_t local_keyboard_controller{0x7a};
static constexpr byte_t all_notes_off_controller{0x7b};
static constexpr byte_t omni_mode_off_controller{0x7c};
static constexpr byte_t omni_mode_on_controller{0x7d};
static constexpr byte_t mono_operation_controller{0x7e};
static constexpr byte_t poly_operation_controller{0x7f};

constexpr bool is_channel_mode_message(byte_t controller) {
    return controller >= 0x78;
}

constexpr byte_t default_value(byte_t controller) {
    switch (controller) {
    case volume_controller.coarse:
        return 0x64;
    case balance_controller.coarse:
    case pan_position_controller.coarse:
    case sound_controllers[0]:
    case sound_controllers[1]:
    case sound_controllers[2]:
    case sound_controllers[3]:
    case sound_controllers[4]:
    case sound_controllers[5]:
    case sound_controllers[6]:
    case sound_controllers[7]:
    case sound_controllers[8]:
    case sound_controllers[9]:
        return 0x40;
    case expression_controller.coarse:
    case non_registered_parameter_controller.coarse:
    case non_registered_parameter_controller.fine:
    case registered_parameter_controller.coarse:
    case registered_parameter_controller.fine:
        return 0x7f;
    default:
        return 0x00;
    };
}

const std::map<byte_t, std::string>& controller_names();

/**
 * The list of controllers that should be reset when
 * all_controllers_off_controller is received (according to RP-015).
 *
 * @warning pitch_wheel, channel_pressure and aftertouch should also be reset
 *
 */

static constexpr byte_t off_controllers[] = {
    modulation_wheel_controller.coarse,
    modulation_wheel_controller.fine,
    expression_controller.coarse,
    expression_controller.fine,
    hold_pedal_controller,
    portamento_controller,
    sustenuto_pedal_controller,
    soft_pedal_controller,
    legato_pedal_controller,
    hold_2_pedal_controller,
    registered_parameter_controller.coarse,
    registered_parameter_controller.fine,
    non_registered_parameter_controller.coarse,
    non_registered_parameter_controller.fine
};

/**
 * The list of controllers that should be reset (or sent) when
 * a reset event is received.
 *
 * @note sending all_controllers_off_controller will reset a part
 *
 * @note the following controllers won't be reset:
 * - bank_select_controller
 * - breath_controller
 * - foot_pedal_controller
 * - portamento_time_controller
 * - data_entry_controller
 * - general_purpose_slider_controllers
 * - general_purpose_button_controllers
 * - data_button_increment_controller
 * - data_button_decrement_controller
 *
 * @warning registered and non-registered parameters should also be reset
 *
 */

static constexpr byte_t reset_controllers[] = {
    all_sound_off_controller,
    all_controllers_off_controller,
    volume_controller.coarse,
    volume_controller.fine,
    balance_controller.coarse,
    balance_controller.fine,
    pan_position_controller.coarse,
    pan_position_controller.fine,
    effect_control_controllers[0].coarse,
    effect_control_controllers[0].fine,
    effect_control_controllers[1].coarse,
    effect_control_controllers[1].fine,
    sound_controllers[0],
    sound_controllers[1],
    sound_controllers[2],
    sound_controllers[3],
    sound_controllers[4],
    sound_controllers[5],
    sound_controllers[6],
    sound_controllers[7],
    sound_controllers[8],
    sound_controllers[9],
    effects_depth_controllers[0],
    effects_depth_controllers[1],
    effects_depth_controllers[2],
    effects_depth_controllers[3],
    effects_depth_controllers[4]
};

}

//========
// Family
//========

/*

  +---------+--------------------------------------------------------------------------------------------------------------------------+----------+
  |         |                                                                  full                                                    |          |
  |         +--------------------------------------------------------------------------------------------------+-----------------------+          |
  |         |                                                        standard                                  |                       |          |
  |         +-------------------------------+------------------------------+-----------------------------------+                       |          |
  |         |              voice            |            system            |                                   |        extended       |          |
  |         +------------+------------------+---------------+--------------+              meta                 |                       |          |
  |         |    note    |                  |    common     |   realtime   |                                   |                       |          |
  |         +------------+------------------+---------------+--------------+-----------------------------------+-------+--------+------+          |
  | invalid | note_off   | controller       | sysex         | clock        | sequence_number, text, copyright  |       |        |      | reserved |
  |         | note_off   | program_change   | mtc_frame     | tick         | track_name, instrument_name       |       |        |      |   ...    |
  |         | aftertouch | channel_pressure | song_position | start        | lyrics, marker, cue_point         |       |        |      |          |
  |         |            | pitch_wheel      | song_select   | continue     | program_name,device_name          | voice | system | meta |          |
  |         |            |                  | xf4           | stop         | channel_prefix, port              |       |        |      |          |
  |         |            |                  | xf5           | xfd          | end_of_track, tempo, smpte_offset |       |        |      |          |
  |         |            |                  | tune_request  | active_sense | time_signature, key_signature     |       |        |      |          |
  |         |            |                  | end_of_sysex  | reset        | proprietary, (default_meta)       |       |        |      |          |
  +---------+------------+------------------+---------------+--------------+-----------------------------------+-------+--------+------+----------+
  |    1    |     3      |        4         |       8       |      8       |               19                  |   1   |   1    |  1   |    18    |
  +---------+------------+------------------+---------------+--------------+-----------------------------------+-------+--------+------+----------+

*/

enum class family_t : uint8_t {
    invalid          , // @note special value for undefined events
    note_off         , // 8x note velocity
    note_on          , // 9x note velocity
    aftertouch       , // ax note pressure
    controller       , // bx controller value
    program_change   , // cx program
    channel_pressure , // dx pressure
    pitch_wheel      , // ex fine coarse
    sysex            , // f0 ...
    mtc_frame        , // f1 time_code_value
    song_position    , // f2 fine coarse
    song_select      , // f3 number
    xf4              , // f4 -
    xf5              , // f5 -
    tune_request     , // f6 -
    end_of_sysex     , // f7 -
    clock            , // f8 -
    tick             , // f9 -
    start            , // fa -
    continue_        , // fb -
    stop             , // fc -
    xfd              , // fd -
    active_sense     , // fe -
    reset            , // ff - @warning status is shared with meta events
    sequence_number  , // ff 00 variable (uint16)
    text             , // ff 01 variable (string)
    copyright        , // ff 02 variable (string)
    track_name       , // ff 03 variable (string)
    instrument_name  , // ff 04 variable (string)
    lyrics           , // ff 05 variable (string)
    marker           , // ff 06 variable (string)
    cue_point        , // ff 07 variable (string)
    program_name     , // ff 08 variable (string)
    device_name      , // ff 09 variable (string)
    channel_prefix   , // ff 20 variable (int)
    port             , // ff 21 variable (int)
    end_of_track     , // ff 2f variable -
    tempo            , // ff 51 variable ...
    smpte_offset     , // ff 54 variable ...
    time_signature   , // ff 58 variable ...
    key_signature    , // ff 59 variable ...
    proprietary      , // ff 7f variable ...
    default_meta     , // ff xx variable ...
    extended_voice   , //
    extended_system  , //
    extended_meta    , //
    reserved_01      , //
    reserved_02      , //
    reserved_03      , //
    reserved_04      , //
    reserved_05      , //
    reserved_06      , //
    reserved_07      , //
    reserved_08      , //
    reserved_09      , //
    reserved_10      , //
    reserved_11      , //
    reserved_12      , //
    reserved_13      , //
    reserved_14      , //
    reserved_15      , //
    reserved_16      , //
    reserved_17      , //
    reserved_18      , //
};

const char* family_name(family_t family);

std::ostream& operator<<(std::ostream& stream, family_t family);

struct families_t : flags_t<families_t, family_t, 64> {
    using flags_t::flags_t;

    static constexpr auto standard_note() { return fuse(
        family_t::note_off, family_t::note_on, family_t::aftertouch
    ); }

    static constexpr auto standard_voice() { return fuse(
        standard_note(), family_t::controller, family_t::program_change, family_t::channel_pressure, family_t::pitch_wheel
    ); }

    static constexpr auto standard_system_common() { return fuse(
        family_t::sysex, family_t::mtc_frame, family_t::song_position, family_t::song_select,
        family_t::xf4, family_t::xf5, family_t::tune_request, family_t::end_of_sysex
    ); }

    static constexpr auto standard_system_realtime() { return fuse(
        family_t::clock, family_t::tick, family_t::start, family_t::continue_,
        family_t::stop, family_t::xfd, family_t::active_sense, family_t::reset
    ); }

    static constexpr auto standard_system() { return standard_system_common() | standard_system_realtime(); }

    static constexpr auto standard_meta() { return fuse(
        family_t::sequence_number, family_t::text, family_t::copyright, family_t::track_name,
        family_t::instrument_name, family_t::lyrics, family_t::marker, family_t::cue_point,
        family_t::program_name, family_t::device_name, family_t::channel_prefix, family_t::port,
        family_t::end_of_track, family_t::tempo, family_t::smpte_offset, family_t::time_signature,
        family_t::key_signature, family_t::proprietary, family_t::default_meta
    ); }

    static constexpr auto standard() { return standard_voice() | standard_system() | standard_meta(); }
    static constexpr auto extended() { return fuse(family_t::extended_voice, family_t::extended_system, family_t::extended_meta); }
    static constexpr auto voice() { return fuse(standard_voice(), family_t::extended_voice); }
    static constexpr auto system() { return fuse(standard_system(), family_t::extended_system); }
    static constexpr auto meta() { return fuse(standard_meta(), family_t::extended_meta); }
    static constexpr auto full() { return standard() | extended(); }

    static constexpr auto string() { return fuse(
        family_t::text, family_t::copyright, family_t::track_name, family_t::instrument_name,
        family_t::lyrics, family_t::marker, family_t::cue_point, family_t::program_name, family_t::device_name
    ); }

    static constexpr auto size_0() { return wrap(family_t::invalid); }

    static constexpr auto size_1() { return fuse(
        family_t::xf4, family_t::xf5, family_t::tune_request, family_t::end_of_sysex, standard_system_realtime()
    ); }

    static constexpr auto size_2() { return fuse(
        family_t::program_change, family_t::channel_pressure, family_t::mtc_frame, family_t::song_select
    ); }

    static constexpr auto size_3() { return fuse(
        standard_note(), family_t::controller, family_t::pitch_wheel, family_t::song_position
    ); }

    static constexpr auto static_() { return size_0() | size_1() | size_2() | size_3(); }
    static constexpr auto dynamic() { return full() & ~static_(); }

};

//=======
// Event
//=======

/**
 * @brief The Event class provides different ways to handle midi events
 * It provides a family representation that can be combined easily as flags
 *
 * @note voice events can be bound to multiple channels (the low nibble of the status byte is unused)
 *
 */

class Event final {

public:
    // event builders (no 0xf4, 0xf5, 0xfd) @todo make meta events builder

    static Event note_off(channels_t channels, byte_t note, byte_t velocity = 0) noexcept;
    static Event note_on(channels_t channels, byte_t note, byte_t velocity) noexcept;
    static Event aftertouch(channels_t channels, byte_t note, byte_t pressure) noexcept;
    static Event controller(channels_t channels, byte_t controller) noexcept;
    static Event controller(channels_t channels, byte_t controller, byte_t value) noexcept;
    static Event program_change(channels_t channels, byte_t program) noexcept;
    static Event channel_pressure(channels_t channels, byte_t pressure) noexcept;
    static Event pitch_wheel(channels_t channels, short_ns::uint14_t pitch) noexcept;
    static Event sys_ex(byte_cview data); /*!< data should contain all bytes, including end-of-sysex, but not the status */
    static Event master_volume(short_ns::uint14_t volume, byte_t sysex_channel = 0x7f);
    static Event mtc_frame(byte_t value) noexcept;
    static Event song_position(short_ns::uint14_t position) noexcept;
    static Event song_select(byte_t value) noexcept;
    static Event tune_request() noexcept;
    static Event clock() noexcept;
    static Event tick() noexcept;
    static Event start() noexcept;
    static Event continue_() noexcept;
    static Event stop() noexcept;
    static Event active_sense() noexcept;
    static Event reset() noexcept;
    static Event meta(byte_cview data); /*!< data should contain meta type, meta size and the rest if data, but not the status */
    static Event tempo(double bpm);
    static Event time_signature(byte_t nn, byte_t dd, byte_t cc, byte_t bb);
    static Event end_of_track();

    // structors

    Event() noexcept = default; /*!< default constructor makes an invalid event */
    Event(const Event& event);
    Event(Event&& event) noexcept = default;
    ~Event() noexcept = default;

    Event& operator=(const Event& event);
    Event& operator=(Event&& event) noexcept = default;

    // accessors

    inline family_t family() const noexcept { return m_family; }
    inline bool is(family_t family) const noexcept { return m_family == family; }
    inline bool is(families_t families) const noexcept { return families.test(m_family); } /*!< true if family belongs to families */
    inline explicit operator bool() const noexcept { return m_family != family_t::invalid; } /*!< true if event is valid */

    uint32_t static_size() const noexcept;
    inline const byte_t* static_data() const noexcept { return m_static_data.data(); }
    inline byte_t* static_data() noexcept { return m_static_data.data(); }

    inline track_t track() const noexcept { return m_track; }
    inline void set_track(track_t track) noexcept { m_track = track; }
    inline Event&& with_track(track_t track) && noexcept { m_track = track; return std::move(*this); }

    inline channels_t channels() const noexcept { return m_channels; }
    inline void set_channels(channels_t channels) noexcept { m_channels = channels; }

    uint32_t dynamic_size() const noexcept;
    inline const byte_t* dynamic_data() const noexcept { return m_dynamic_data.get(); }
    inline byte_t* dynamic_data() noexcept { return m_dynamic_data.get(); }

    // comparison

    static bool equivalent(const Event& lhs, const Event& rhs); /*!< test if both events are equivalent, meaning same family and same data */

    // string

    std::string description() const; /*!< get event description based on family & data */

    friend std::ostream& operator<<(std::ostream& os, const Event& event);

private:
    friend class EventPrivate;
    Event(family_t family, track_t track, channels_t channels) noexcept;

    family_t m_family {family_t::invalid}; // 1 byte
    std::array<byte_t, 3> m_static_data {0x00, 0x00, 0x00}; // 3 bytes
    track_t m_track {default_track}; // 2 bytes
    channels_t m_channels {}; // 2 bytes
    std::unique_ptr<byte_t[]> m_dynamic_data {}; // 4 or 8 bytes

};

// ==========
// Extraction
// ==========

namespace extraction_ns {

struct static_tag {};
struct dynamic_tag {};
struct any_tag {};

inline uint32_t get_size(static_tag, const Event& event) noexcept { return event.static_size(); }
inline uint32_t get_size(dynamic_tag, const Event& event) noexcept { return event.dynamic_size(); }
inline uint32_t get_size(any_tag, const Event& event) noexcept { return event.dynamic_data() ? event.dynamic_size() : event.static_size(); }

inline const byte_t* get_cdata(static_tag, const Event& event) noexcept { return event.static_data(); }
inline const byte_t* get_cdata(dynamic_tag, const Event& event) noexcept { return event.dynamic_data(); }
inline const byte_t* get_cdata(any_tag, const Event& event) noexcept { return event.dynamic_data() ? event.dynamic_data() : event.static_data(); }

inline byte_t* get_data(static_tag, Event& event) noexcept { return event.static_data(); }
inline byte_t* get_data(dynamic_tag, Event& event) noexcept { return event.dynamic_data(); }
inline byte_t* get_data(any_tag, Event& event) noexcept { return event.dynamic_data() ? event.dynamic_data() : event.static_data(); }

template<typename Tag>
struct ViewExtracter {
    byte_cview operator()(const Event& event) const { return range_ns::from_span(get_cdata(Tag{}, event), get_size(Tag{}, event)); }
    byte_view operator()(Event& event) const { return range_ns::from_span(get_data(Tag{}, event), get_size(Tag{}, event)); }
};

static constexpr auto static_view = ViewExtracter<static_tag>{};
static constexpr auto dynamic_view = ViewExtracter<dynamic_tag>{};
static constexpr auto view = ViewExtracter<any_tag>{};

template<typename Tag, size_t index>
struct ByteExtracter {
    byte_t operator()(const Event& event) const noexcept { return get_cdata(Tag{}, event)[index]; }
    byte_t& operator()(Event& event) const noexcept { return get_data(Tag{}, event)[index]; }
};

static constexpr auto status = ByteExtracter<any_tag, 0>{};
static constexpr auto controller = ByteExtracter<static_tag, 1>{};
static constexpr auto controller_value = ByteExtracter<static_tag, 2>{};
static constexpr auto note = ByteExtracter<static_tag, 1>{};
static constexpr auto program = ByteExtracter<static_tag, 1>{};
static constexpr auto velocity = ByteExtracter<static_tag, 2>{};
static constexpr auto song = ByteExtracter<static_tag, 1>{};
static constexpr auto channel_pressure = ByteExtracter<static_tag, 1>{};

byte_cview get_meta_cview(const Event& event);
byte_view get_meta_view(Event& event);

Note get_note(const Event& event);
uint16_t get_14bits(const Event& event);
double get_bpm(const Event& event);

template<class T = int64_t>
T get_meta_int(const Event& event) { /*!< interprets meta data as an integer */
    const auto view = get_meta_cview(event);
    return byte_traits<T>::read_le(view.min, view.max);
}

bool get_master_volume(const Event& event, short_ns::uint14_t& volume, byte_t& sysex_channel); /*!< precondition: event.is(family_t::sysex) */

/// check if event specifies to turn some channels to drum channels */
channels_t use_for_rhythm_part(const Event& event); /*!< precondition: event.is(family_t::sysex) */

};

// =========
// Extension
// =========

namespace extension_ns {

struct extension_t {

    explicit extension_t(family_t family, std::string key);

    Event make_event(channels_t channels) const;
    Event make_event(channels_t channels, byte_cview value) const;
    Event make_event(channels_t channels, const std::string& value) const;

    template<typename T>
    Event make_event(channels_t channels, const T& value) const {
        return make_event(channels, marshall(value));
    }

    bool affects(const Event& event) const; /*!< precondition: event.is(families_t::extended()) */

    static std::string extract_key(const Event& event); /*!< precondition: event.is(families_t::extended()) */
    std::string extract_value(const Event& event) const; /*!< precondition: affects(event) */

    const family_t family;
    const std::string key;

};

template<typename T>
struct extension_facade_t : extension_t {
    using extension_t::extension_t;
    auto encode(channels_t channels, const T& value) const { return make_event(channels, value); }
    auto decode(const Event& event) const { return unmarshall<T>(extract_value(event)); }
};

template<>
struct extension_facade_t<std::string> : extension_t {
    using extension_t::extension_t;
    auto encode(channels_t channels, const std::string& value) const { return make_event(channels, value); }
    auto decode(const Event& event) const { return extract_value(event); }
};

template<>
struct extension_facade_t<void> : extension_t {
    using extension_t::extension_t;
    auto encode(channels_t channels) const { return make_event(channels); }
};

template<typename T>
struct VoiceExtension : extension_facade_t<T> {
    VoiceExtension(std::string key) : extension_facade_t<T>{family_t::extended_voice, std::move(key)} {}
    template<typename ... Args>
    auto operator()(Args&& ... args) const {
        return this->encode(std::forward<Args>(args)...);
    }
};

template<typename T>
struct SystemExtension : extension_facade_t<T> {
    SystemExtension(std::string key) : extension_facade_t<T>{family_t::extended_system, std::move(key)} {}
    template<typename ... Args>
    auto operator()(Args&& ... args) const {
        return this->encode(channels_t{}, std::forward<Args>(args)...);
    }
};

template<typename T>
struct MetaExtension : extension_facade_t<T> {
    MetaExtension(std::string key) : extension_facade_t<T>{family_t::extended_meta, std::move(key)} {}
    template<typename ... Args>
    auto operator()(Args&& ... args) const {
        return this->encode(channels_t{}, std::forward<Args>(args)...);
    }
};

}

using extension_ns::VoiceExtension;
using extension_ns::SystemExtension;
using extension_ns::MetaExtension;

#endif // CORE_EVENT_H
