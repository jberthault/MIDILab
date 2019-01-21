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

#include <sstream> // std::stringstream
#include <cstring>
#include <algorithm>
#include "event.h"
#include "tools/trace.h"

byte_cview make_view(const char* string) {
    return make_view(string, ::strlen(string));
}

byte_cview make_view(const char* string, size_t size) {
    const auto* data = reinterpret_cast<const byte_t*>(string);
    return {data, data + size};
}

byte_cview make_view(const std::string& string) {
    return make_view(string.data(), string.size());
}

vararray_t<byte_t, 4> encode_variable(uint32_t size) {
    vararray_t<byte_t, 4> result;
    if (size >> 28)
        throw std::out_of_range{"wrong variable value"};
    if (const auto v3 = size >> 21)
        result.push_back(0x80 | to_data_byte(v3));
    if (const auto v2 = size >> 14)
        result.push_back(0x80 | to_data_byte(v2));
    if (const auto v1 = size >> 7)
        result.push_back(0x80 | to_data_byte(v1));
    result.push_back(to_data_byte(size));
    return result;
}

//=========
// Channel
//=========

namespace channel_ns {

std::string channel_string(channel_t channel) {
    return marshall(channel);
}

std::string channels_string(channels_t channels) {
    switch (channels.size()) {
    case 0: return "";
    case 1: return channel_string(*channels.begin());
    case channels_t::capacity(): return "*";
    default: return "M";
    }
}

}

//======
// Drum
//======

namespace drum_ns {

std::ostream& print_drum(std::ostream& stream, byte_t byte) {
    switch (byte) {
    case high_q_drum: stream << "High Q"; break;
    case slap_drum: stream << "Slap"; break;
    case scratch_push_drum: stream << "Scratch Push"; break;
    case scratch_pull_drum: stream << "Scratch Pull"; break;
    case sticks_drum: stream << "Sticks"; break;
    case square_click_drum: stream << "Square Click"; break;
    case metronome_click_drum: stream << "Metronome Click"; break;
    case metronome_bell_drum: stream << "Metronome Bell"; break;
    case bass_2_drum: stream << "Bass Drum 2"; break;
    case bass_1_drum: stream << "Bass Drum 1"; break;
    case sidestick_drum: stream << "Side Stick/Rimshot"; break;
    case snare_1_drum: stream << "Snare Drum 1"; break;
    case handclap_drum: stream << "Hand Clap"; break;
    case snare_2_drum: stream << "Snare Drum 2"; break;
    case low_tom_2_drum: stream << "Low Tom 2"; break;
    case closed_hihat_drum: stream << "Closed Hi-hat"; break;
    case low_tom_1_drum: stream << "Low Tom 1"; break;
    case pedal_hihat_drum: stream << "Pedal Hi-hat"; break;
    case mid_tom_2_drum: stream << "Mid Tom 2"; break;
    case open_hihat_drum: stream << "Open Hi-hat"; break;
    case mid_tom_1_drum: stream << "Mid Tom 1"; break;
    case high_tom_2_drum: stream << "High Tom 2"; break;
    case crash_cymbal_1_drum: stream << "Crash Cymbal 1"; break;
    case high_tom_1_drum: stream << "High Tom 1"; break;
    case ride_cymbal_1_drum: stream << "Ride Cymbal 1"; break;
    case chinese_cymbal_drum: stream << "Chinese Cymbal"; break;
    case ride_bell_drum: stream << "Ride Bell"; break;
    case tambourine_drum: stream << "Tambourine"; break;
    case splash_cymbal_drum: stream << "Splash Cymbal"; break;
    case cowbell_drum: stream << "Cowbell"; break;
    case crash_cymbal_2_drum: stream << "Crash Cymbal 2"; break;
    case vibra_slap_drum: stream << "Vibra Slap"; break;
    case ride_cymbal_2_drum: stream << "Ride Cymbal 2"; break;
    case high_bongo_drum: stream << "High Bongo"; break;
    case low_bongo_drum: stream << "Low Bongo"; break;
    case mute_high_conga_drum: stream << "Mute High Conga"; break;
    case open_high_conga_drum: stream << "Open High Conga"; break;
    case low_conga_drum: stream << "Low Conga"; break;
    case high_timbale_drum: stream << "High Timbale"; break;
    case low_timbale_drum: stream << "Low Timbale"; break;
    case high_agogo_drum: stream << "High Agogô"; break;
    case low_agogo_drum: stream << "Low Agogô"; break;
    case cabasa_drum: stream << "Cabasa"; break;
    case maracas_drum: stream << "Maracas"; break;
    case short_whistle_drum: stream << "Short Whistle"; break;
    case long_whistle_drum: stream << "Long Whistle"; break;
    case short_guiro_drum: stream << "Short Güiro"; break;
    case long_guiro_drum: stream << "Long Güiro"; break;
    case claves_drum: stream << "Claves"; break;
    case high_wood_drum: stream << "High Wood Block"; break;
    case low_wood_drum: stream << "Low Wood Block"; break;
    case mute_cuica_drum: stream << "Mute Cuíca"; break;
    case open_cuica_drum: stream << "Open Cuíca"; break;
    case mute_triangle_drum: stream << "Mute Triangle"; break;
    case open_triangle_drum: stream << "Open Triangle"; break;
    case shaker_drum: stream << "Shaker"; break;
    case jingle_bell_drum: stream << "Jingle Bell"; break;
    case bell_tree_drum: stream << "Bell Tree"; break;
    case castinets_drum: stream << "Castinets"; break;
    case mute_surdo_drum: stream << "Mute Surdo"; break;
    case open_surdo_drum: stream << "Open Surdo"; break;

    default : stream << "Unknown Drum " << byte_string(byte);
    }
    return stream;
}

}

//============
// Controller
//============

const std::map<byte_t, std::string>& controller_ns::controller_names() {
    static std::map<byte_t, std::string> info_ = {
        {bank_select_controller.coarse, "Bank Select (coarse)"},
        {bank_select_controller.fine, "Bank Select (fine)"},
        {modulation_wheel_controller.coarse, "Modulation Wheel (coarse)"},
        {modulation_wheel_controller.fine, "Modulation Wheel (fine)"},
        {breath_controller.coarse, "Breath controller (coarse)"},
        {breath_controller.fine, "Breath controller (fine)"},
        {foot_pedal_controller.coarse, "Foot Pedal (coarse)"},
        {foot_pedal_controller.fine, "Foot Pedal (fine)"},
        {portamento_time_controller.coarse, "Portamento Time (coarse)"},
        {portamento_time_controller.fine, "Portamento Time (fine)"},
        {data_entry_controller.coarse, "Data Entry (coarse)"},
        {data_entry_controller.fine, "Data Entry (fine)"},
        {volume_controller.coarse, "Volume (coarse)"},
        {volume_controller.fine, "Volume (fine)"},
        {balance_controller.coarse, "Balance (coarse)"},
        {balance_controller.fine, "Balance (fine)"},
        {pan_position_controller.coarse, "Pan Position (coarse)"},
        {pan_position_controller.fine, "Pan position (fine)"},
        {expression_controller.coarse, "Expression (coarse)"},
        {expression_controller.fine, "Expression (fine)"},
        {effect_control_controllers[0].coarse, "Effect Control 1 (coarse)"},
        {effect_control_controllers[0].fine, "Effect Control 1 (fine)"},
        {effect_control_controllers[1].coarse, "Effect Control 2 (coarse)"},
        {effect_control_controllers[1].fine, "Effect Control 2 (fine)"},
        {general_purpose_slider_controllers[0], "General Purpose Slider 1"},
        {general_purpose_slider_controllers[1], "General Purpose Slider 2"},
        {general_purpose_slider_controllers[2], "General Purpose Slider 3"},
        {general_purpose_slider_controllers[3], "General Purpose Slider 4"},
        {hold_pedal_controller, "Hold Pedal (on/off)"},
        {portamento_controller, "Portamento (on/off)"},
        {sustenuto_pedal_controller, "Sustenuto Pedal (on/off)"},
        {soft_pedal_controller, "Soft Pedal (on/off)"},
        {legato_pedal_controller, "Legato Pedal (on/off)"},
        {hold_2_pedal_controller, "Hold 2 Pedal (on/off)"},
        {sound_controllers[0], "Sound Control 1 (Variation)"},
        {sound_controllers[1], "Sound Control 2 (Timbre)"},
        {sound_controllers[2], "Sound Control 3 (Release Time)"},
        {sound_controllers[3], "Sound Control 4 (Attack Time)"},
        {sound_controllers[4], "Sound Control 5 (Brightness)"},
        {sound_controllers[5], "Sound Control 6"},
        {sound_controllers[6], "Sound Control 7"},
        {sound_controllers[7], "Sound Control 8"},
        {sound_controllers[8], "Sound Control 9"},
        {sound_controllers[9], "Sound Control 10"},
        {general_purpose_button_controllers[0], "General Purpose Button 1 (on/off)"},
        {general_purpose_button_controllers[1], "General Purpose Button 2 (on/off)"},
        {general_purpose_button_controllers[2], "General Purpose Button 3 (on/off)"},
        {general_purpose_button_controllers[3], "General Purpose Button 4 (on/off)"},
        {effects_depth_controllers[0], "Effect Depth 1 (Reverb)"},
        {effects_depth_controllers[1], "Effect Depth 2 (Tremolo)"},
        {effects_depth_controllers[2], "Effect Depth 3 (Chorus)"},
        {effects_depth_controllers[3], "Effect Depth 4 (Celeste)"},
        {effects_depth_controllers[4], "Effect Depth 5 (Phaser)"},
        {data_button_increment_controller, "Data Button increment"},
        {data_button_decrement_controller, "Data Button decrement"},
        {non_registered_parameter_controller.coarse, "Non-registered Parameter (coarse)"},
        {non_registered_parameter_controller.fine, "Non-registered Parameter (fine)"},
        {registered_parameter_controller.coarse, "Registered Parameter (coarse)"},
        {registered_parameter_controller.fine, "Registered Parameter (fine)"},
        {all_sound_off_controller, "All Sound Off"},
        {all_controllers_off_controller, "All Controllers Off"},
        {local_keyboard_controller, "Local Keyboard (on/off)"},
        {all_notes_off_controller, "All Notes Off"},
        {omni_mode_off_controller, "Omni Mode Off"},
        {omni_mode_on_controller, "Omni Mode On"},
        {mono_operation_controller, "Mono Operation"},
        {poly_operation_controller, "Poly Operation"},
    };
    return info_;
}

//========
// Family
//========

const char* family_name(family_t family) {
    switch (family) {
    case family_t::invalid: return "Invalid Event";
    case family_t::note_off: return "Note Off";
    case family_t::note_on: return "Note On";
    case family_t::aftertouch: return "Aftertouch";
    case family_t::controller: return "Controller";
    case family_t::program_change: return "Program Change";
    case family_t::channel_pressure: return "Channel Pressure";
    case family_t::pitch_wheel: return "Pitch Wheel";
    case family_t::sysex: return "System Exclusive";
    case family_t::mtc_frame: return "MTC Quarter Frame Message";
    case family_t::song_position: return "Song Position Pointer";
    case family_t::song_select: return "Song Select";
    case family_t::xf4: return "System Common 0xf4";
    case family_t::xf5: return "System Common 0xf5";
    case family_t::tune_request: return "Tune Request";
    case family_t::end_of_sysex: return "End Of Sysex";
    case family_t::clock: return "MIDI Clock";
    case family_t::tick: return "Tick";
    case family_t::start: return "MIDI Start";
    case family_t::continue_: return "MIDI Continue";
    case family_t::stop: return "MIDI Stop";
    case family_t::xfd: return "System Realtime 0xfd";
    case family_t::active_sense: return "Active Sense";
    case family_t::reset: return "Reset";
    case family_t::sequence_number: return "Sequence Number";
    case family_t::text: return "Text Event";
    case family_t::copyright: return "Copyright Notice";
    case family_t::track_name: return "Track Name";
    case family_t::instrument_name: return "Instrument Name";
    case family_t::lyrics: return "Lyrics";
    case family_t::marker: return "Marker";
    case family_t::cue_point: return "Cue Point";
    case family_t::program_name: return "Program Name";
    case family_t::device_name: return "Device Name";
    case family_t::channel_prefix: return "Channel Prefix";
    case family_t::port: return "MIDI Port";
    case family_t::end_of_track: return "End Of Track";
    case family_t::tempo: return "Set Tempo";
    case family_t::smpte_offset: return "SMPTE Offset";
    case family_t::time_signature: return "Time Signature";
    case family_t::key_signature: return "Key Signature";
    case family_t::proprietary: return "Proprietary";
    case family_t::default_meta: return "Unknown MetaEvent";
    case family_t::extended_voice: return "Extended Voice Event";
    case family_t::extended_system: return "Extended System Event";
    case family_t::extended_meta: return "Extended Meta Event";
    default: return "Unknown Event";
    }
}

constexpr family_t meta_family(byte_t meta_type) {
    switch (meta_type) {
    case 0x00: return family_t::sequence_number;
    case 0x01: return family_t::text;
    case 0x02: return family_t::copyright;
    case 0x03: return family_t::track_name;
    case 0x04: return family_t::instrument_name;
    case 0x05: return family_t::lyrics;
    case 0x06: return family_t::marker;
    case 0x07: return family_t::cue_point;
    case 0x08: return family_t::program_name;
    case 0x09: return family_t::device_name;
    case 0x20: return family_t::channel_prefix;
    case 0x21: return family_t::port;
    case 0x2f: return family_t::end_of_track;
    case 0x51: return family_t::tempo;
    case 0x54: return family_t::smpte_offset;
    case 0x58: return family_t::time_signature;
    case 0x59: return family_t::key_signature;
    case 0x7f: return family_t::proprietary;
    default  : return is_msb_cleared(meta_type) ? family_t::default_meta : family_t::invalid;
    }
}

constexpr byte_t family_status(family_t family, byte_t default_status = 0x00) {
    switch (family) {
    case family_t::note_off : return 0x80;
    case family_t::note_on: return 0x90;
    case family_t::aftertouch: return 0xa0;
    case family_t::controller: return 0xb0;
    case family_t::program_change: return 0xc0;
    case family_t::channel_pressure : return 0xd0;
    case family_t::pitch_wheel: return 0xe0;
    case family_t::sysex: return 0xf0;
    case family_t::mtc_frame: return 0xf1;
    case family_t::song_position: return 0xf2;
    case family_t::song_select: return 0xf3;
    case family_t::xf4: return 0xf4;
    case family_t::xf5: return 0xf5;
    case family_t::tune_request: return 0xf6;
    case family_t::end_of_sysex: return 0xf7;
    case family_t::clock: return 0xf8;
    case family_t::tick: return 0xf9;
    case family_t::start: return 0xfa;
    case family_t::continue_: return 0xfb;
    case family_t::stop: return 0xfc;
    case family_t::xfd: return 0xfd;
    case family_t::active_sense: return 0xfe;
    case family_t::reset:
    case family_t::sequence_number:
    case family_t::text:
    case family_t::copyright:
    case family_t::track_name:
    case family_t::instrument_name:
    case family_t::lyrics:
    case family_t::marker:
    case family_t::cue_point:
    case family_t::program_name:
    case family_t::device_name:
    case family_t::channel_prefix:
    case family_t::port:
    case family_t::end_of_track:
    case family_t::tempo:
    case family_t::smpte_offset:
    case family_t::time_signature:
    case family_t::key_signature:
    case family_t::proprietary:
    case family_t::default_meta: return 0xff;
    default: return default_status;
    }
}

std::ostream& operator<<(std::ostream& stream, family_t family) {
    return stream << family_name(family);
}

// event printers

namespace {

const std::array<const char*, 0x80> program_names = {
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Rhodes Piano",
    "Chorused Piano",
    "Harpsichord",
    "Clavinet",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Hammond Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar Harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "Synth Brass 1",
    "Synth Brass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Bottle Blow",
    "Shaluhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope lead)",
    "Lead 4 (chiff lead)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shaminsen",
    "Koto",
    "Kalimba",
    "Bagpipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot"
};

std::ostream& print_controller(std::ostream& stream, const Event& event) {
    const auto id = extraction_ns::controller(event);
    const auto it = controller_ns::controller_names().find(id);
    if (it != controller_ns::controller_names().end())
        stream << it->second;
    else
        stream << "Unknown Controller " << byte_string(id);
    return stream << " (" << static_cast<int>(extraction_ns::controller_value(event)) << ')';
}

std::ostream& print_note(std::ostream& stream, const Event& event) {
    const auto note = extraction_ns::note(event);
    if (event.channels().any(channels_t::drums())) {
        drum_ns::print_drum(stream, note);
    } else {
        stream << Note::from_code(note);
    }
    return stream << " (" << static_cast<int>(extraction_ns::velocity(event)) << ')';
}

std::ostream& print_key_signature(std::ostream& stream, const Event& event) {
    const auto data = extraction_ns::get_meta_cview(event).min;
    const int key = static_cast<char>(data[0]); // key byte is signed
    const bool major = data[1] == 0;
    return stream << key << (major ? " major" : " minor");
}

std::ostream& print_time_signature(std::ostream& stream, const Event& event) {
    const auto data = extraction_ns::get_meta_cview(event).min;
    const int nn = data[0];
    const int dd = 1 << data[1];
    const int cc = data[2];
    const int bb = data[3];
    return stream << nn << '/' << dd << " (" << cc << ", " << bb << ")";
}

std::ostream& print_mtc_frame(std::ostream& stream, const Event&) {
    return stream; /// @todo
}

std::ostream& print_smpte_offset(std::ostream& stream, const Event& event) {
    const auto data = extraction_ns::get_meta_cview(event).min;
    const byte_t hours_byte = data[0];
    const int fps = (hours_byte & 0b01100000) >> 5;
    const int hours = hours_byte & 0b00011111;
    const int minutes = data[1];
    const int seconds = data[2];
    const int frames = data[3];
    const int subframes = data[4];
    switch (fps) {
    case 0b00: stream << "24"; break;
    case 0b01: stream << "25"; break;
    case 0b10: stream << "drop 30"; break;
    case 0b11: stream << "30"; break;
    }
    return stream << " fps " << hours << "h " << minutes << "m " << seconds << "s " << frames + subframes/100. << " frames";
}

std::ostream& print_event(std::ostream& stream, const Event& event) {
    switch (event.family()) {
    case family_t::note_off:
    case family_t::note_on:
    case family_t::aftertouch:
        return print_note(stream, event);
    case family_t::controller:
        return print_controller(stream, event);
    case family_t::program_change:
        return stream << program_names[extraction_ns::program(event)] << " (" << static_cast<int>(extraction_ns::program(event)) << ')';
    case family_t::channel_pressure:
    case family_t::song_select:
        return stream << static_cast<int>(extraction_ns::song(event));
    case family_t::pitch_wheel:
    case family_t::song_position:
        return stream << extraction_ns::get_14bits(event);
    case family_t::mtc_frame:
        return print_mtc_frame(stream, event);
    case family_t::smpte_offset:
        return print_smpte_offset(stream, event);
    case family_t::time_signature:
        return print_time_signature(stream, event);
    case family_t::key_signature:
        return print_key_signature(stream, event);
    case family_t::tempo:
        return stream << decay_value<int>(10. * extraction_ns::get_bpm(event)) / 10. << " bpm";
    case family_t::text:
    case family_t::copyright:
    case family_t::track_name:
    case family_t::instrument_name:
    case family_t::lyrics:
    case family_t::marker:
    case family_t::cue_point:
    case family_t::program_name:
    case family_t::device_name: {
        const auto view = extraction_ns::get_meta_cview(event);
        return stream << std::string{view.min, view.max};
    }
    case family_t::sequence_number:
    case family_t::channel_prefix:
    case family_t::port:
        return stream << extraction_ns::get_meta_int(event);
    case family_t::sysex:
    case family_t::proprietary:
    case family_t::default_meta: {
        const auto view = extraction_ns::dynamic_view(event);
        return print_bytes(stream, view.min, view.max);
    }
    case family_t::extended_voice:
    case family_t::extended_system:
    case family_t::extended_meta:
        return stream << extension_ns::extension_t::extract_key(event);
    default:
        return stream;
    }
}

}

class EventPrivate {

public:
    template<family_t family, typename = std::enable_if_t<families_t::size_1().test(family)>>
    static auto static_event(channels_t channels) noexcept {
        Event event{family, default_track, channels};
        event.m_static_data[0] = family_status(family);
        return event;
    }

    template<family_t family, typename = std::enable_if_t<families_t::size_2().test(family)>>
    static auto static_event(channels_t channels, byte_t b0) noexcept {
        Event event{family, default_track, channels};
        event.m_static_data[0] = family_status(family);
        event.m_static_data[1] = b0;
        return event;
    }

    template<family_t family, typename = std::enable_if_t<families_t::size_3().test(family)>>
    static auto static_event(channels_t channels, byte_t b0, byte_t b1) noexcept {
        Event event{family, default_track, channels};
        event.m_static_data[0] = family_status(family);
        event.m_static_data[1] = b0;
        event.m_static_data[2] = b1;
        return event;
    }

    static auto dynamic_data(const Event& event) {
        std::unique_ptr<byte_t[]> data;
        if (const auto* src = event.dynamic_data()) {
            const auto size = static_cast<size_t>(event.dynamic_size());
            data = std::make_unique<byte_t[]>(size);
            std::copy_n(src, size, data.get());
        }
        return data;
    }

    static auto dynamic_event(family_t family, channels_t channels, size_t size) {
        Event event{family, default_track, channels};
        if (size >> 24)
            throw std::length_error{"event data is too large"};
        event.m_dynamic_data = std::make_unique<byte_t[]>(size);
        event.m_static_data = {to_byte(size), to_byte(size >> 8), to_byte(size >> 16)};
        return event;
    }

    static auto dynamic_event(family_t family, channels_t channels, byte_cview data) {
        auto event = dynamic_event(family, channels, static_cast<size_t>(span(data)));
        std::copy(data.min, data.max, event.dynamic_data());
        return event;
    }

    static auto dynamic_event(family_t family, channels_t channels, byte_cview data1, byte_cview data2) {
        auto event = dynamic_event(family, channels, static_cast<size_t>(span(data1) + span(data2)));
        std::copy(data2.min, data2.max, std::copy(data1.min, data1.max, event.dynamic_data()));
        return event;
    }

    static auto dynamic_event(family_t family, channels_t channels, byte_cview data1, byte_cview data2, byte_cview data3) {
        auto event = dynamic_event(family, channels, static_cast<size_t>(span(data1) + span(data2) + span(data3)));
        std::copy(data3.min, data3.max, std::copy(data2.min, data2.max, std::copy(data1.min, data1.max, event.dynamic_data())));
        return event;
    }

};

//=======
// Event
//=======

// event builders

Event Event::note_off(channels_t channels, byte_t note, byte_t velocity) noexcept {
    return EventPrivate::static_event<family_t::note_off>(channels, to_data_byte(note), to_data_byte(velocity));
}

Event Event::note_on(channels_t channels, byte_t note, byte_t velocity) noexcept {
    return EventPrivate::static_event<family_t::note_on>(channels, to_data_byte(note), to_data_byte(velocity));
}

Event Event::aftertouch(channels_t channels, byte_t note, byte_t pressure) noexcept {
    return EventPrivate::static_event<family_t::aftertouch>(channels, to_data_byte(note), to_data_byte(pressure));
}

Event Event::controller(channels_t channels, byte_t controller) noexcept {
    return EventPrivate::static_event<family_t::controller>(channels, to_data_byte(controller), controller_ns::default_value(controller));
}

Event Event::controller(channels_t channels, byte_t controller, byte_t value) noexcept {
    return EventPrivate::static_event<family_t::controller>(channels, to_data_byte(controller), to_data_byte(value));
}

Event Event::program_change(channels_t channels, byte_t program) noexcept {
    return EventPrivate::static_event<family_t::program_change>(channels, to_data_byte(program));
}

Event Event::channel_pressure(channels_t channels, byte_t pressure) noexcept {
    return EventPrivate::static_event<family_t::channel_pressure>(channels, to_data_byte(pressure));
}

Event Event::pitch_wheel(channels_t channels, short_ns::uint14_t pitch) noexcept {
    return EventPrivate::static_event<family_t::pitch_wheel>(channels, to_data_byte(pitch.fine), to_data_byte(pitch.coarse));
}

Event Event::sys_ex(byte_cview data) {
    if (!data || *(data.max-1) != 0xf7 || std::any_of(data.min, data.max - 1, is_msb_set<byte_t>))
        return {};
    const byte_t prefix[] = {0xf0};
    return EventPrivate::dynamic_event(family_t::sysex, {}, make_view(prefix), data);
}

Event Event::master_volume(short_ns::uint14_t volume, byte_t sysex_channel) {
    const byte_t data[] = {0xf0, 0x7f, sysex_channel, 0x04, 0x01, to_data_byte(volume.fine), to_data_byte(volume.coarse), 0xf7};
    return EventPrivate::dynamic_event(family_t::sysex, {}, make_view(data));
}

Event Event::mtc_frame(byte_t value) noexcept {
    return EventPrivate::static_event<family_t::mtc_frame>({}, value);
}

Event Event::song_position(short_ns::uint14_t position) noexcept {
    return EventPrivate::static_event<family_t::song_position>({}, to_data_byte(position.fine), to_data_byte(position.coarse));
}

Event Event::song_select(byte_t value) noexcept {
    return EventPrivate::static_event<family_t::song_select>({}, to_data_byte(value));
}

Event Event::tune_request() noexcept {
    return EventPrivate::static_event<family_t::tune_request>({});
}

Event Event::clock() noexcept {
    return EventPrivate::static_event<family_t::clock>({});
}

Event Event::tick() noexcept {
    return EventPrivate::static_event<family_t::tick>({});
}

Event Event::start() noexcept {
    return EventPrivate::static_event<family_t::start>({});
}

Event Event::continue_() noexcept {
    return EventPrivate::static_event<family_t::continue_>({});
}

Event Event::stop() noexcept {
    return EventPrivate::static_event<family_t::stop>({});
}

Event Event::active_sense() noexcept {
    return EventPrivate::static_event<family_t::active_sense>({});
}

Event Event::reset() noexcept {
    return EventPrivate::static_event<family_t::reset>({});
}

Event Event::meta(byte_cview data) {
    const auto family = data ? meta_family(*data.min) : family_t::invalid;
    if (family == family_t::invalid)
        return {};
    const byte_t prefix[] = {0xff};
    return EventPrivate::dynamic_event(family, {}, make_view(prefix), data);
}

Event Event::tempo(double bpm) {
    if (bpm <= 0) {
        TRACE_WARNING("BPM value can't be set: " << bpm);
        return {};
    }
    const auto tempo = decay_value<uint32_t>(60000000. / bpm);
    const byte_t data[] = {0xff, 0x51, 0x03, to_byte(tempo >> 16), to_byte(tempo >> 8), to_byte(tempo)};
    return EventPrivate::dynamic_event(family_t::tempo, {}, make_view(data));
}

Event Event::track_name(byte_cview data) {
    const byte_t prefix[] = {0xff, 0x03};
    auto encoded_size = encode_variable(static_cast<uint32_t>(span(data)));
    return EventPrivate::dynamic_event(family_t::track_name, {}, make_view(prefix), make_view(encoded_size), data);
}

Event Event::time_signature(byte_t nn, byte_t dd, byte_t cc, byte_t bb) {
    const byte_t data[] = {0xff, 0x58, 0x04, to_data_byte(nn), to_data_byte(dd), to_data_byte(cc), to_data_byte(bb)};
    return EventPrivate::dynamic_event(family_t::time_signature, {}, make_view(data));
}

Event Event::end_of_track() {
    const byte_t data[] = {0xff, 0x2f, 0x00};
    return EventPrivate::dynamic_event(family_t::end_of_track, {}, make_view(data));
}

// constructors

Event::Event(const Event& event) :
    m_family{event.m_family},
    m_static_data{event.m_static_data},
    m_track{event.m_track},
    m_channels{event.m_channels},
    m_dynamic_data{EventPrivate::dynamic_data(event)} {

}

Event::Event(family_t family, track_t track, channels_t channels) noexcept :
    m_family{family}, m_track{track}, m_channels{channels} {

}

Event& Event::operator=(const Event& event) {
    m_family = event.m_family;
    m_static_data = event.m_static_data;
    m_track = event.m_track;
    m_channels = event.m_channels;
    m_dynamic_data = EventPrivate::dynamic_data(event);
    return *this;
}

// accessors

uint32_t Event::static_size() const noexcept {
    if (is(families_t::size_3())) return 3;
    if (is(families_t::size_2())) return 2;
    if (is(families_t::size_1())) return 1;
    return 0;
}

uint32_t Event::dynamic_size() const noexcept {
    return byte_traits<uint32_t>::make_le(m_static_data[0], m_static_data[1], m_static_data[2]);
}

// comparison

bool Event::equivalent(const Event& lhs, const Event& rhs) {
    if (lhs.m_family != rhs.m_family)
        return false;
    if (lhs.m_static_data != rhs.m_static_data)
        return false;
    if (const auto* lhs_data = lhs.dynamic_data())
        return std::equal(lhs_data, lhs_data + lhs.dynamic_size(), rhs.dynamic_data());
    return true;
}

// string

std::string Event::description() const {
    std::stringstream stream;
    print_event(stream, *this);
    return stream.str();
}

std::ostream& operator<<(std::ostream& os, const Event& event) {
    // add name
    os << event.family();
    // add channels
    if (const auto channels = event.channels())
        os << " [" << channel_ns::channels_string(channels) << "]";
    // add description
    const auto description = event.description();
    if (!description.empty())
        os << ": " << description;
    return os;
}

// ==========
// Extraction
// ==========

namespace extraction_ns {

template<typename RangeT>
auto meta_start(RangeT range) {
    /// 0xff type <variable> <message_start>
    range.min += 2;
    while (range && is_msb_set(*range.min++));
    return range;
}

byte_cview get_meta_cview(const Event& event) {
    return meta_start(dynamic_view(event));
}

byte_view get_meta_view(Event& event) {
    return meta_start(dynamic_view(event));
}

Note get_note(const Event& event) {
    return Note::from_code(note(event));
}

uint16_t get_14bits(const Event& event) {
    const auto* data = event.static_data();
    return short_ns::glue({data[2], data[1]});
}

double get_bpm(const Event& event) {
    return 60000000. / get_meta_int(event);
}

bool get_master_volume(const Event& event, short_ns::uint14_t& volume, byte_t& sysex_channel) {
    // pattern 0xf0 0x7f <channel> 0x04 0x01 <fine> <coarse> 0xf7
    if (event.dynamic_size() == 8) {
        const auto* data = event.dynamic_data();
        if (data[1] == 0x7f && data[3] == 0x04 && data[4] == 0x01) {
            sysex_channel = data[2];
            volume.fine = data[5];
            volume.coarse = data[6];
            return true;
        }
    }
    return false;
}

channels_t use_for_rhythm_part(const Event& event) {
    const auto* data = event.dynamic_data();
    if (   event.dynamic_size() > 8
        && data[1] == 0x41                       // roland manufacturer
        && data[4] == 0x12                       // sending data
        && data[5] == 0x40                       //
        && (data[6] & 0xf0) == 0x10              //
        && data[7] == 0x15                       // 0x40 0x1X 0x15 <=> address of USE_FOR_RHYTHM_PART
        && (data[8] == 0x01 || data[8] == 0x02)) // value {0x00: OFF, 0x01: MAP1, 0x02: MAP2}
        return channels_t::wrap(data[6] & 0x0f);
    return {};
}

}

// =========
// Extension
// =========

namespace extension_ns {

extension_t::extension_t(family_t family, std::string key) : family{family}, key{std::move(key)} {

}

bool extension_t::affects(const Event& event) const {
    const auto key_view = make_view(key);
    return event.dynamic_size() >= key.size() && std::equal(key_view.min, key_view.max, event.dynamic_data());
}

std::string extension_t::extract_key(const Event& event) {
    const auto* data = event.dynamic_data();
    return {data, std::find(data, data + event.dynamic_size(), 0x00)};
}

std::string extension_t::extract_value(const Event& event) const {
    const auto* data = event.dynamic_data();
    return {data + key.size() + 1, data + event.dynamic_size()};
}

Event extension_t::make_event(channels_t channels) const {
    return EventPrivate::dynamic_event(family, channels, make_view(key));
}

Event extension_t::make_event(channels_t channels, byte_cview value) const {
    return EventPrivate::dynamic_event(family, channels, make_view(key.c_str(), key.size() + 1), value); // write null terminator
}

Event extension_t::make_event(channels_t channels, const std::string& value) const {
    return make_event(channels, make_view(value));
}

}
