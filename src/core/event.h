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

#ifndef CORE_EVENT_H
#define CORE_EVENT_H

#include <array>
#include <map>
#include <unordered_map>
#include <boost/container/small_vector.hpp>
#include "note.h"
#include "tools/bytes.h"

class Event;

//=======
// Short
//=======

/// @todo find a better name

struct short_tools {

static const uint16_t maximum_value = 0x3fff;

static uint16_t glue(byte_t coarse, byte_t fine);

static byte_t coarse(uint16_t value);
static byte_t fine(uint16_t value);

static uint16_t alter_coarse(uint16_t value, byte_t coarse);
static uint16_t alter_fine(uint16_t value, byte_t fine);

};

//=========
// Channel
//=========

using channel_t = uint8_t; /*!< channel type differs from byte type due to its limited range */
using channels_t = flags_t<uint16_t, channel_t>;

static constexpr channel_t drum_channel = 9;
static constexpr channels_t drum_channels = channels_t::from_bit(drum_channel);
static constexpr channels_t all_channels = 0xffff;

namespace channel_ns {

std::string channel_string(channel_t channel);
std::string channels_string(channels_t channels);

template<typename T>
using map_type = std::array<T, 0x10>;

template<typename T>
using rmap_type = std::unordered_map<T, channels_t>;

template<typename T, typename U>
channels_t find(const map_type<T>& map, U value) {
    channels_t channels;
    for (channel_t c=0 ; c < 0x10 ; c++)
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
rmap_type<T> reverse(const map_type<T>& map, channels_t channels) {
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

static constexpr byte_t high_q_drum(27);
static constexpr byte_t slap_drum(28);
static constexpr byte_t scratch_push_drum(29);
static constexpr byte_t scratch_pull_drum(30);
static constexpr byte_t sticks_drum(31);
static constexpr byte_t square_click_drum(32);
static constexpr byte_t metronome_click_drum(33);
static constexpr byte_t metronome_bell_drum(34);
static constexpr byte_t bass_2_drum(35);
static constexpr byte_t bass_1_drum(36);
static constexpr byte_t sidestick_drum(37);
static constexpr byte_t snare_1_drum(38);
static constexpr byte_t handclap_drum(39);
static constexpr byte_t snare_2_drum(40);
static constexpr byte_t low_tom_2_drum(41);
static constexpr byte_t closed_hihat_drum(42);
static constexpr byte_t low_tom_1_drum(43);
static constexpr byte_t pedal_hihat_drum(44);
static constexpr byte_t mid_tom_2_drum(45);
static constexpr byte_t open_hihat_drum(46);
static constexpr byte_t mid_tom_1_drum(47);
static constexpr byte_t high_tom_2_drum(48);
static constexpr byte_t crash_cymbal_1_drum(49);
static constexpr byte_t high_tom_1_drum(50);
static constexpr byte_t ride_cymbal_1_drum(51);
static constexpr byte_t chinese_cymbal_drum(52);
static constexpr byte_t ride_bell_drum(53);
static constexpr byte_t tambourine_drum(54);
static constexpr byte_t splash_cymbal_drum(55);
static constexpr byte_t cowbell_drum(56);
static constexpr byte_t crash_cymbal_2_drum(57);
static constexpr byte_t vibra_slap_drum(58);
static constexpr byte_t ride_cymbal_2_drum(59);
static constexpr byte_t high_bongo_drum(60);
static constexpr byte_t low_bongo_drum(61);
static constexpr byte_t mute_high_conga_drum(62);
static constexpr byte_t open_high_conga_drum(63);
static constexpr byte_t low_conga_drum(64);
static constexpr byte_t high_timbale_drum(65);
static constexpr byte_t low_timbale_drum(66);
static constexpr byte_t high_agogo_drum(67);
static constexpr byte_t low_agogo_drum(68);
static constexpr byte_t cabasa_drum(69);
static constexpr byte_t maracas_drum(70);
static constexpr byte_t short_whistle_drum(71);
static constexpr byte_t long_whistle_drum(72);
static constexpr byte_t short_guiro_drum(73);
static constexpr byte_t long_guiro_drum(74);
static constexpr byte_t claves_drum(75);
static constexpr byte_t high_wood_drum(76);
static constexpr byte_t low_wood_drum(77);
static constexpr byte_t mute_cuica_drum(78);
static constexpr byte_t open_cuica_drum(79);
static constexpr byte_t mute_triangle_drum(80);
static constexpr byte_t open_triangle_drum(81);
static constexpr byte_t shaker_drum(82);
static constexpr byte_t jingle_bell_drum(83);
static constexpr byte_t bell_tree_drum(84);
static constexpr byte_t castinets_drum(85);
static constexpr byte_t mute_surdo_drum(86);
static constexpr byte_t open_surdo_drum(87);

}

//============
// Controller
//============
// undefined controllers: (0x03 0x23) (0x09 0x29) (0x0e 0x2e) (0x0f 0x2f) [0x14 0x1f] [0x54 0x5a] [0x66 0x77]

struct controller_tools {

    struct info_type {
        std::string name;
        byte_t default_value;
        bool is_action;
    };

    using storage_type = std::map<byte_t, info_type>;

    static const storage_type& info();

};

namespace controller_ns {

static constexpr byte_t bank_select_coarse_controller(0x00);
static constexpr byte_t bank_select_fine_controller(0x20);
static constexpr byte_t modulation_wheel_coarse_controller(0x01);
static constexpr byte_t modulation_wheel_fine_controller(0x21);
static constexpr byte_t breath_coarse_controller(0x02);
static constexpr byte_t breath_fine_controller(0x22);
static constexpr byte_t foot_pedal_coarse_controller(0x04);
static constexpr byte_t foot_pedal_fine_controller(0x24);
static constexpr byte_t portamento_time_coarse_controller(0x05);
static constexpr byte_t portamento_time_fine_controller(0x25);
static constexpr byte_t data_entry_coarse_controller(0x06);
static constexpr byte_t data_entry_fine_controller(0x26);
static constexpr byte_t volume_coarse_controller(0x07);
static constexpr byte_t volume_fine_controller(0x27);
static constexpr byte_t balance_coarse_controller(0x08);
static constexpr byte_t balance_fine_controller(0x28);
static constexpr byte_t pan_position_coarse_controller(0x0a);
static constexpr byte_t pan_position_fine_controller(0x2a);
static constexpr byte_t expression_coarse_controller(0x0b);
static constexpr byte_t expression_fine_controller(0x2b);
static constexpr byte_t effect_control_1_coarse_controllers(0x0c);
static constexpr byte_t effect_control_1_fine_controllers(0x2c);
static constexpr byte_t effect_control_2_coarse_controllers(0x0d);
static constexpr byte_t effect_control_2_fine_controllers(0x2d);
static constexpr byte_t general_purpose_slider_controllers[] = {0x10, 0x11, 0x12, 0x13};
static constexpr byte_t hold_pedal_controller(0x40);
static constexpr byte_t portamento_controller(0x41);
static constexpr byte_t sustenuto_pedal_controller(0x42);
static constexpr byte_t soft_pedal_controller(0x43);
static constexpr byte_t legato_pedal_controller(0x44);
static constexpr byte_t hold_2_pedal_controller(0x45);
static constexpr byte_t sound_controllers[] = {0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
static constexpr byte_t variation_controller = sound_controllers[0];
static constexpr byte_t timbre_controller = sound_controllers[1];
static constexpr byte_t sound_release_time = sound_controllers[2];
static constexpr byte_t sound_attack_time = sound_controllers[3];
static constexpr byte_t sound_brightness = sound_controllers[4];
static constexpr byte_t general_purpose_button_controllers[] = {0x50, 0x51, 0x52, 0x53};
static constexpr byte_t effects_level_controller(0x5b);
static constexpr byte_t tremulo_level_controller(0x5c);
static constexpr byte_t chorus_level_controller(0x5d);
static constexpr byte_t celeste_level_controller(0x5e);
static constexpr byte_t phaser_level_controller(0x5f);
static constexpr byte_t data_button_increment_controller(0x60);
static constexpr byte_t data_button_decrement_controller(0x61);
static constexpr byte_t non_registered_parameter_coarse_controller(0x63);
static constexpr byte_t non_registered_parameter_fine_controller(0x62);
static constexpr byte_t registered_parameter_coarse_controller(0x65);
static constexpr byte_t registered_parameter_fine_controller(0x64);
static constexpr byte_t all_sound_off_controller(0x78);
static constexpr byte_t all_controllers_off_controller(0x79);
static constexpr byte_t local_keyboard_controller(0x7a);
static constexpr byte_t all_notes_off_controller(0x7b);
static constexpr byte_t omni_mode_off_controller(0x7c);
static constexpr byte_t omni_mode_on_controller(0x7d);
static constexpr byte_t mono_operation_controller(0x7e);
static constexpr byte_t poly_operation_controller(0x7f);

}

//========
// Family
//========

using family_t = flags_t<uint64_t>;

struct family_info_t {

    using printer_type = std::ostream& (*)(std::ostream& stream, const Event& event);

    std::string name;
    printer_type printer;
    bool has_channels;

};

struct family_tools {

    using printer_type = family_info_t::printer_type;
    using family_storage_type = std::unordered_map<family_t, family_info_t>;

    static std::ostream& print_event(std::ostream& stream, const Event& event);

    static std::ostream& print_bytes(std::ostream& stream, const Event& event);
    static std::ostream& print_controller(std::ostream& stream, const Event& event);
    static std::ostream& print_program(std::ostream& stream, const Event& event);
    static std::ostream& print_note(std::ostream& stream, const Event& event);
    static std::ostream& print_8bits(std::ostream& stream, const Event& event);
    static std::ostream& print_14bits(std::ostream& stream, const Event& event);
    static std::ostream& print_key_signature(std::ostream& stream, const Event& event);
    static std::ostream& print_time_signature(std::ostream& stream, const Event& event);
    static std::ostream& print_mtc_frame(std::ostream& stream, const Event& event);
    static std::ostream& print_smpte_offset(std::ostream& stream, const Event& event);
    static std::ostream& print_meta_string(std::ostream& stream, const Event& event);
    static std::ostream& print_meta_int(std::ostream& stream, const Event& event);
    static std::ostream& print_meta_bytes(std::ostream& stream, const Event& event);
    static std::ostream& print_bpm(std::ostream& stream, const Event& event);
    static std::ostream& print_noop(std::ostream& stream, const Event& event);
    static std::ostream& print_custom(std::ostream& stream, const Event& event);

    static family_t register_family(family_info_t family_info); /*!< register a new family (may throw overflow_error) */

    static const family_info_t& family_info(family_t family);
    static printer_type family_printer(family_t family, printer_type default_printer);
    static const std::string& family_name(family_t family, const std::string& default_name);

    static bool has_family(family_t family); /*!< true if family is registered (must be unique) */
    static const family_storage_type& info(); /*!< name of families + description function (public access) */

private:
    static family_storage_type& m_info(); /*!< info about families */

};

namespace family_ns {

/// @todo separate family from families

static constexpr family_t no_family               = 0x0000000000000000u;
static constexpr family_t note_off_family         = 0x0000000000000001u;  // 8x note velocity
static constexpr family_t note_on_family          = 0x0000000000000002u;  // 9x note velocity
static constexpr family_t aftertouch_family       = 0x0000000000000004u;  // ax note pressure
static constexpr family_t controller_family       = 0x0000000000000008u;  // bx controller value
static constexpr family_t program_change_family   = 0x0000000000000010u;  // cx program
static constexpr family_t channel_pressure_family = 0x0000000000000020u;  // dx pressure
static constexpr family_t pitch_wheel_family      = 0x0000000000000040u;  // ex fine coarse
static constexpr family_t sysex_family            = 0x0000000000000080u;  // f0 ...
static constexpr family_t mtc_frame_family        = 0x0000000000000100u;  // f1 time_code_value
static constexpr family_t song_position_family    = 0x0000000000000200u;  // f2 fine coarse
static constexpr family_t song_select_family      = 0x0000000000000400u;  // f3 number
static constexpr family_t xf4_family              = 0x0000000000000800u;  // f4 -
static constexpr family_t xf5_family              = 0x0000000000001000u;  // f5 -
static constexpr family_t tune_request_family     = 0x0000000000002000u;  // f6 -
static constexpr family_t end_of_sysex_family     = 0x0000000000004000u;  // f7 -
static constexpr family_t clock_family            = 0x0000000000008000u;  // f8 -
static constexpr family_t tick_family             = 0x0000000000010000u;  // f9 -
static constexpr family_t start_family            = 0x0000000000020000u;  // fa -
static constexpr family_t continue_family         = 0x0000000000040000u;  // fb -
static constexpr family_t stop_family             = 0x0000000000080000u;  // fc -
static constexpr family_t xfd_family              = 0x0000000000100000u;  // fd -
static constexpr family_t active_sense_family     = 0x0000000000200000u;  // fe -
static constexpr family_t reset_family            = 0x0000000000400000u;  // ff - @warning status is shared with meta events
static constexpr family_t sequence_number_family  = 0x0000000000800000u;  // ff 00 variable (uint16)
static constexpr family_t text_family             = 0x0000000001000000u;  // ff 01 variable (string)
static constexpr family_t copyright_family        = 0x0000000002000000u;  // ff 02 variable (string)
static constexpr family_t track_name_family       = 0x0000000004000000u;  // ff 03 variable (string)
static constexpr family_t instrument_name_family  = 0x0000000008000000u;  // ff 04 variable (string)
static constexpr family_t lyrics_family           = 0x0000000010000000u;  // ff 05 variable (string)
static constexpr family_t marker_family           = 0x0000000020000000u;  // ff 06 variable (string)
static constexpr family_t cue_point_family        = 0x0000000040000000u;  // ff 07 variable (string)
static constexpr family_t program_name_family     = 0x0000000080000000u;  // ff 08 variable (string)
static constexpr family_t device_name_family      = 0x0000000100000000u;  // ff 09 variable (string)
static constexpr family_t channel_prefix_family   = 0x0000000200000000u;  // ff 20 variable (int)
static constexpr family_t port_family             = 0x0000000400000000u;  // ff 21 variable (int)
static constexpr family_t end_of_track_family     = 0x0000000800000000u;  // ff 2f variable -
static constexpr family_t tempo_family            = 0x0000001000000000u;  // ff 51 variable ...
static constexpr family_t smpte_offset_family     = 0x0000002000000000u;  // ff 54 variable ...
static constexpr family_t time_signature_family   = 0x0000004000000000u;  // ff 58 variable ...
static constexpr family_t key_signature_family    = 0x0000008000000000u;  // ff 59 variable ...
static constexpr family_t proprietary_family      = 0x0000010000000000u;  // ff 7f variable ...
static constexpr family_t default_meta_family     = 0x0000020000000000u;  // ff xx variable ...
static constexpr family_t custom_family           = 0x0000040000000000u;  // <implementation defined>

static constexpr family_t note_families =
        note_off_family | note_on_family | aftertouch_family;

static constexpr family_t voice_families =
        note_families | controller_family | program_change_family |
        channel_pressure_family | pitch_wheel_family;

static constexpr family_t system_common_families =
        sysex_family | mtc_frame_family | song_position_family | song_select_family |
        xf4_family | xf5_family | tune_request_family | end_of_sysex_family;

static constexpr family_t system_realtime_families =
        clock_family | tick_family | start_family | continue_family | stop_family |
        xfd_family | active_sense_family | reset_family;

static constexpr family_t system_families =
        system_common_families | system_realtime_families;

static constexpr family_t meta_families =
        sequence_number_family | text_family | copyright_family | track_name_family |
        instrument_name_family | lyrics_family | marker_family | cue_point_family |
        program_name_family | device_name_family | channel_prefix_family | port_family |
        end_of_track_family | tempo_family | smpte_offset_family | time_signature_family |
        key_signature_family | proprietary_family | default_meta_family;

static constexpr family_t midi_families =
        voice_families | system_families | meta_families;

static constexpr family_t string_families =
        text_family | copyright_family | track_name_family | instrument_name_family |
        lyrics_family | marker_family | cue_point_family | program_name_family |
        device_name_family;

}

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

    // types definition

    using data_type = boost::container::small_vector<byte_t, 4>; /*!< a vector would fit as well */
    using const_iterator = data_type::const_iterator;
    using iterator = data_type::iterator;

    // event builders (no 0xf4, 0xf5, 0xfd) @todo make meta events builder

    static Event note_off(channels_t channels, const Note& note, byte_t velocity = 0);
    static Event note_on(channels_t channels, const Note& note, byte_t velocity);
    static Event drum_off(channels_t channels, byte_t byte, byte_t velocity = 0);
    static Event drum_on(channels_t channels, byte_t byte, byte_t velocity);
    static Event aftertouch(channels_t channels, const Note& note, byte_t pressure);
    static Event controller(channels_t channels, byte_t controller, byte_t value = 0);
    static Event program_change(channels_t channels, byte_t program);
    static Event channel_pressure(channels_t channels, byte_t pressure);
    static Event pitch_wheel(channels_t channels, uint16_t pitch);
    static Event sys_ex(data_type data); /*!< data must provide every byte [0xf0, ..., 0xf7] (none event if problem) */
    static Event master_volume(uint16_t volume, byte_t sysex_channel = 0x7f);
    static Event mtc_frame(byte_t value);
    static Event song_position(uint16_t value);
    static Event song_select(byte_t value);
    static Event tune_request();
    static Event midi_clock();
    static Event midi_tick();
    static Event midi_start();
    static Event midi_continue();
    static Event midi_stop();
    static Event active_sense();
    static Event reset();
    static Event tempo(double bpm);
    static Event end_of_track();
    static Event custom(channels_t channels, const std::string& key);
    static Event custom(channels_t channels, const std::string& key, const std::string& value);
    static Event raw(bool is_realtime, data_type data); /*!< check data content to get a coherent event & updates associated family */

    // constructors & copy

    Event() = default; /*!< default constructor makes an empty event */

    // miscellaneous

    bool operator ==(const Event& event) const; /*!< comparison */

    // string

    std::string name() const; /*!< get event name based on its family */
    std::string description() const; /*!< get event description based on family & data */

    friend std::ostream& operator <<(std::ostream& os, const Event& event);

    // family accessors

    family_t family() const; /*!< family accessor */
    bool is(family_t families) const; /*!< true if family belongs to families */
    family_t extract_family(bool is_realtime) const; /*!< get family from raw data */

    explicit operator bool() const; /*!< true if family is not a no_family */

    // channels accessors

    channels_t channels() const;
    void set_channels(channels_t channels);

    // data accessors

    size_t size() const; /*!< returns number of bytes stored in data */

    const data_type& data() const; /*!< data accessor (first data byte is the status) */
    data_type& data(); /*!< @see data */

    byte_t at(size_t index, byte_t default_byte = 0x00) const; /*!< data byte accessor (default byte is returned if data is too short) */

    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

    const_iterator meta_begin() const; /*!< return position of the first data byte of meta events (removing encoded size) */

    // data observers (extracted values may have a different sense if the event is not the expected one)

    Note get_note() const; /*!< returns a null note if extraction fails */
    uint16_t get_14bits() const; /*!< song position or pitch (in range [0, 0x3fff]) */
    double get_bpm() const; /*!< (set tempo event only) */
    std::string get_meta_string() const; /*!< interprets meta data as a string */
    std::string get_custom_key() const; /*!< extract custom event key */
    std::string get_custom_value() const; /*!< extract custom event value */

    template<class T = int64_t>
    T get_meta_int() const { /*!< interprets meta data as an integer */
        return byte_traits<T>::read_little_endian(meta_begin(), end());
    }

private:
    Event(family_t family, channels_t channels, data_type data);

    family_t m_family;
    channels_t m_channels;
    data_type m_data;

};

#endif // CORE_EVENT_H
