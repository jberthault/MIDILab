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

#include <sstream> // std::stringstream
#include <algorithm>
#include "event.h"
#include "tools/trace.h"

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
    case family_t::custom: return "Custom Event";
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
    default: return "Unknown Event";
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
    byte_t id = event.at(1);
    auto it = controller_ns::controller_names().find(id);
    if (it != controller_ns::controller_names().end())
        stream << it->second;
    else
        stream << "Unknown Controller " << byte_string(id);
    return stream << " (" << (int)event.at(2) << ')';
}

std::ostream& print_note(std::ostream& stream, const Event& event) {
    byte_t note = event.at(1);
    if (event.channels().any(channels_t::drums())) {
        drum_ns::print_drum(stream, note);
    } else {
        stream << Note::from_code(note);
    }
    return stream << " (" << (int)event.at(2) << ')'; /// add velocity/aftertouch
}

std::ostream& print_key_signature(std::ostream& stream, const Event& event) {
    Event::const_iterator it = event.meta_begin();
    return stream << (int)(char)*it++ << ((*it == 0) ? " major" : " minor");
}

std::ostream& print_time_signature(std::ostream& stream, const Event& event) {
    Event::const_iterator it = event.meta_begin();
    int nn = *it++;
    int dd = 1 << *it++;
    int cc = *it++;
    int bb = *it;
    return stream << nn << '/' << dd << " (" << cc << ", " << bb << ")";
}

std::ostream& print_mtc_frame(std::ostream& stream, const Event&) {
    return stream; /// @todo
}

std::ostream& print_smpte_offset(std::ostream& stream, const Event& event) {
    Event::const_iterator it = event.meta_begin();
    byte_t hours_byte = *it++;
    int fps = (hours_byte & 0b01100000) >> 5;
    int hours = hours_byte & 0b00011111;
    int minutes = *it++;
    int seconds = *it++;
    int frames = *it++;
    int subframes = *it;
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
    case family_t::custom:
        return stream << event.get_custom_key();
    case family_t::note_off:
    case family_t::note_on:
    case family_t::aftertouch:
        return print_note(stream, event);
    case family_t::controller:
        return print_controller(stream, event);
    case family_t::program_change:
        return stream << program_names[event.at(1) & 0x7f] << " (" << (int)event.at(1) << ')';
    case family_t::channel_pressure:
    case family_t::song_select:
        return stream << (int)event.at(1);
    case family_t::pitch_wheel:
    case family_t::song_position:
        return stream << event.get_14bits();
    case family_t::mtc_frame:
        return print_mtc_frame(stream, event);
    case family_t::smpte_offset:
        return print_smpte_offset(stream, event);
    case family_t::time_signature:
        return print_time_signature(stream, event);
    case family_t::key_signature:
        return print_key_signature(stream, event);
    case family_t::tempo:
        return stream << decay_value<int>(10. * event.get_bpm()) / 10. << " bpm";
    case family_t::text:
    case family_t::copyright:
    case family_t::track_name:
    case family_t::instrument_name:
    case family_t::lyrics:
    case family_t::marker:
    case family_t::cue_point:
    case family_t::program_name:
    case family_t::device_name:
        return stream << event.get_meta_string();
    case family_t::sequence_number:
    case family_t::channel_prefix:
    case family_t::port:
        return stream << event.get_meta_int();
    case family_t::sysex:
    case family_t::proprietary:
    case family_t::default_meta:
        return print_bytes(stream, event.begin(), event.end());
    default:
        return stream;
    }
}

}

//=======
// Event
//=======

// event builders

Event Event::note_off(channels_t channels, byte_t note, byte_t velocity) {
    return {family_t::note_off, channels, {0x80, to_data_byte(note), to_data_byte(velocity)}};
}

Event Event::note_on(channels_t channels, byte_t note, byte_t velocity) {
    return {family_t::note_on, channels, {0x90, to_data_byte(note), to_data_byte(velocity)}};
}

Event Event::aftertouch(channels_t channels, byte_t note, byte_t pressure) {
    return {family_t::aftertouch, channels, {0xa0, to_data_byte(note), to_data_byte(pressure)}};
}

Event Event::controller(channels_t channels, byte_t controller) {
    return {family_t::controller, channels, {0xb0, to_data_byte(controller), controller_ns::default_value(controller)}};
}

Event Event::controller(channels_t channels, byte_t controller, byte_t value) {
    return {family_t::controller, channels, {0xb0, to_data_byte(controller), to_data_byte(value)}};
}

Event Event::program_change(channels_t channels, byte_t program) {
    return {family_t::program_change, channels, {0xc0, to_data_byte(program)}};
}

Event Event::channel_pressure(channels_t channels, byte_t pressure) {
    return {family_t::channel_pressure, channels, {0xd0, to_data_byte(pressure)}};
}

Event Event::pitch_wheel(channels_t channels, uint16_t pitch) {
    return {family_t::pitch_wheel, channels, {0xe0, short_ns::fine(pitch), short_ns::coarse(pitch)}};
}

Event Event::sys_ex(data_type data) {
    if (data.empty() || data.front() != 0xf0 || data.back() != 0xf7 || std::any_of(++data.begin(), --data.end(), is_msb_set<byte_t>))
        return {};
    return {family_t::sysex, {}, std::move(data)};
}

Event Event::master_volume(uint16_t volume, byte_t sysex_channel) {
    return {family_t::sysex, {}, {0xf0, 0x7f, sysex_channel, 0x04, 0x01, short_ns::fine(volume), short_ns::coarse(volume), 0xf7}};
}

Event Event::mtc_frame(byte_t value) {
    return {family_t::mtc_frame, {}, {0xf1, value}};
}

Event Event::song_position(uint16_t value) {
    return {family_t::song_position, {}, {0xf2, short_ns::fine(value), short_ns::coarse(value)}};
}

Event Event::song_select(byte_t value) {
    return {family_t::song_select, {}, {0xf3, to_data_byte(value)}};
}

Event Event::tune_request() {
    return {family_t::tune_request, {}, {0xf6}};
}

Event Event::clock() {
    return {family_t::clock, {}, {0xf8}};
}

Event Event::tick() {
    return {family_t::tick, {}, {0xf9}};
}

Event Event::start() {
    return {family_t::start, {}, {0xfa}};
}

Event Event::continue_() {
    return {family_t::continue_, {}, {0xfb}};
}

Event Event::stop() {
    return {family_t::stop, {}, {0xfc}};
}

Event Event::active_sense() {
    return {family_t::active_sense, {}, {0xfe}};
}

Event Event::reset() {
    return {family_t::reset, {}, {0xff}};
}

Event Event::tempo(double bpm) {
    if (bpm <= 0) {
        TRACE_WARNING("BPM value can't be set: " << bpm);
        return {};
    }
    auto tempo = decay_value<uint32_t>(60000000. / bpm);
    return {family_t::tempo, {}, {0xff, 0x51, 0x03, to_byte(tempo >> 16), to_byte(tempo >> 8), to_byte(tempo)}};
}

Event Event::end_of_track() {
    return {family_t::end_of_track, {}, {0xff, 0x2f, 0x00}};
}

Event Event::custom(channels_t channels, const std::string& key) {
    return {family_t::custom, channels, {key.begin(), key.end()}};
}

Event Event::custom(channels_t channels, const std::string& key, const std::string& value) {
    data_type data(key.size() + value.size() + 1, 0x00);
    auto pos = std::copy(key.begin(), key.end(), data.begin());
    std::copy(value.begin(), value.end(), ++pos);
    return {family_t::custom, channels, std::move(data)};
}

Event Event::raw(bool is_realtime, data_type data) {
    Event event({}, {}, std::move(data));
    /// @todo check data integrity
    channel_t channel = event.at(0) & 0xf;
    // transform note on event with null velocity to note off event
    if (event.size() > 2 && (event.m_data[0] & 0xf0) == 0x90 && event.m_data[2] == 0)
        event.m_data[0] = 0x80 | channel;
    // get family
    event.m_family = event.extract_family(is_realtime);
    // get channel mask
    if (event.is(families_t::voice()))
        event.m_channels.set(channel);
    return event;
}

// constructors

Event::Event() :
    m_family(family_t::invalid), m_channels(), m_data() {

}

Event::Event(family_t family, channels_t channels, data_type data) :
    m_family(family), m_channels(channels), m_data(std::move(data)) {

}

// comparison

bool Event::equivalent(const Event& lhs, const Event& rhs) {
    if (lhs.m_family != rhs.m_family)
        return false;
    auto ilhs = lhs.begin();
    auto irhs = rhs.begin();
    // skip status byte
    if (lhs.is(families_t::midi())) {
        ++ilhs;
        ++irhs;
    }
    return equal_padding(ilhs, lhs.end(), irhs, rhs.end(), [](byte_t value) { return value == 0x00; });
}

bool operator==(const Event& lhs, const Event& rhs) {
    return lhs.m_channels == rhs.m_channels && Event::equivalent(lhs, rhs);
}

bool operator!=(const Event& lhs, const Event& rhs) {
    return !(lhs == rhs);
}

// string

const char* Event::name() const {
    return family_name(m_family);
}

std::string Event::description() const {
    std::stringstream stream;
    print_event(stream, *this);
    return stream.str();
}

std::ostream& operator<<(std::ostream& os, const Event& event) {
    // add name
    os << event.name();
    // add channels
    channels_t channels = event.channels();
    if (channels)
        os << " [" << channel_ns::channels_string(channels) << "]";
    // add description
    std::string description = event.description();
    if (!description.empty())
        os << ": " << description;
    return os;
}

// family accessors

family_t Event::family() const {
    return m_family;
}

Event::operator bool() const {
    return m_family != family_t::invalid;
}

bool Event::is(families_t families) const {
    return families.test(m_family);
}

family_t Event::extract_family(bool is_realtime) const {
    switch (at(0) & 0xf0) { // VOICE EVENT
    case 0x80: return family_t::note_off;
    case 0x90: return family_t::note_on;
    case 0xa0: return family_t::aftertouch;
    case 0xb0: return family_t::controller;
    case 0xc0: return family_t::program_change;
    case 0xd0: return family_t::channel_pressure;
    case 0xe0: return family_t::pitch_wheel;
    case 0xf0:
        switch (at(0)) { // SYSTEM EVENT
        case 0xf0: return family_t::sysex;
        case 0xf1: return family_t::mtc_frame;
        case 0xf2: return family_t::song_position;
        case 0xf3: return family_t::song_select;
        case 0xf4: return family_t::xf4;
        case 0xf5: return family_t::xf5;
        case 0xf6: return family_t::tune_request;
        case 0xf7: return family_t::end_of_sysex;
        case 0xf8: return family_t::clock;
        case 0xf9: return family_t::tick;
        case 0xfa: return family_t::start;
        case 0xfb: return family_t::continue_;
        case 0xfc: return family_t::stop;
        case 0xfd: return family_t::xfd;
        case 0xfe: return family_t::active_sense;
        default: // 0xff
            if (is_realtime)
                return family_t::reset;
            switch (at(1, 0xff)) { // META EVENT
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
            default  : return family_t::default_meta;
            }
        }
    default: return family_t::invalid;
    }
}

// channel accessors

channels_t Event::channels() const {
    return m_channels;
}

void Event::set_channels(channels_t channels) {
    m_channels = channels;
}

// data accessors

size_t Event::size() const {
    return m_data.size();
}

const Event::data_type& Event::data() const {
    return m_data;
}

Event::data_type& Event::data() {
    return m_data;
}

byte_t Event::at(size_t index, byte_t default_byte) const {
    return index < size() ? m_data[index] : default_byte;
}

Event::const_iterator Event::begin() const {
    return m_data.begin();
}

Event::const_iterator Event::end() const {
    return m_data.end();
}

Event::iterator Event::begin() {
    return m_data.begin();
}

Event::iterator Event::end() {
    return m_data.end();
}

Event::const_iterator Event::meta_begin() const {
    /// 0xff type <variable> <message_start>
    const_iterator it = begin() + 2, last = end();
    while (it != last && is_msb_set(*it++));
    return it;
}

// data observers

Note Event::get_note() const {
    return Note::from_code(at(1));
}

uint16_t Event::get_14bits() const {
    return short_ns::glue({at(2), at(1)});
}

double Event::get_bpm() const {
    return 60000000. / get_meta_int();
}

std::string Event::get_meta_string() const {
    return {meta_begin(), end()};
}

std::string Event::get_custom_key() const {
    return {begin(), std::find(begin(), end(), 0x00)};
}

std::string Event::get_custom_value() const {
    auto it = std::find(begin(), end(), 0x00);
    if (it != end())
        ++it;
    return {it, end()};
}
