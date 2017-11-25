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
#include <algorithm> // std::none_of
#include "event.h"
#include "tools/trace.h"

using namespace family_ns;
using namespace controller_ns;

//=======
// Short
//=======

uint16_t short_tools::glue(byte_t coarse, byte_t fine) {
    return (coarse << 7) | fine;
}

byte_t short_tools::coarse(uint16_t value) {
    return (value >> 7) & 0x7f;
}

byte_t short_tools::fine(uint16_t value) {
    return value & 0x7f;
}

uint16_t short_tools::alter_coarse(uint16_t value, byte_t coarse) {
    return ((coarse & 0x7f) << 7) | (value & 0x007f);
}

uint16_t short_tools::alter_fine(uint16_t value, byte_t fine) {
    return (value & 0x3f80) | (fine & 0x7f);
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
    case 16: return "*";
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

const controller_tools::storage_type& controller_tools::info() {
    /// @todo find a better way to store info (group coarse/fine)
    static storage_type info_ = {
        {bank_select_coarse_controller, {"Bank Select (coarse)", 0x00, false}},
        {bank_select_fine_controller, {"Bank Select (fine)", 0x00, false}},
        {modulation_wheel_coarse_controller, {"Modulation Wheel (coarse)", 0x00, false}},
        {modulation_wheel_fine_controller, {"Modulation Wheel (fine)", 0x00, false}},
        {breath_coarse_controller, {"Breath controller (coarse)", 0x00, false}},
        {breath_fine_controller, {"Breath controller (fine)", 0x00, false}},
        {foot_pedal_coarse_controller, {"Foot Pedal (coarse)", 0x00, false}},
        {foot_pedal_fine_controller, {"Foot Pedal (fine)", 0x00, false}},
        {portamento_time_coarse_controller, {"Portamento Time (coarse)", 0x00, false}},
        {portamento_time_fine_controller, {"Portamento Time (fine)", 0x00, false}},
        {data_entry_coarse_controller, {"Data Entry (coarse)", 0x00, false}},
        {data_entry_fine_controller, {"Data Entry (fine)", 0x00, false}},
        {volume_coarse_controller, {"Volume (coarse)", 100, false}},
        {volume_fine_controller, {"Volume (fine)", 0x00, false}},
        {balance_coarse_controller, {"Balance (coarse)", 0x7f >> 1, false}},
        {balance_fine_controller, {"Balance (fine)", 0x00, false}},
        {pan_position_coarse_controller, {"Pan Position (coarse)", 0x7f >> 1, false}},
        {pan_position_fine_controller, {"Pan position (fine)", 0x00, false}},
        {expression_coarse_controller, {"Expression (coarse)", 0x7f, false}},
        {expression_fine_controller, {"Expression (fine)", 0x00, false}},
        {effect_control_1_coarse_controllers, {"Effect Control 1 (coarse)", 0x00, false}},
        {effect_control_1_fine_controllers, {"Effect Control 1 (fine)", 0x00, false}},
        {effect_control_2_coarse_controllers, {"Effect Control 2 (coarse)", 0x00, false}},
        {effect_control_2_fine_controllers, {"Effect Control 2 (fine)", 0x00, false}},
        {general_purpose_slider_controllers[0], {"General Purpose Slider 1", 0x00, false}},
        {general_purpose_slider_controllers[1], {"General Purpose Slider 2", 0x00, false}},
        {general_purpose_slider_controllers[2], {"General Purpose Slider 3", 0x00, false}},
        {general_purpose_slider_controllers[3], {"General Purpose Slider 4", 0x00, false}},
        {hold_pedal_controller, {"Hold Pedal (on/off)", 0x00, false}},
        {portamento_controller, {"Portamento (on/off)", 0x00, false}},
        {sustenuto_pedal_controller, {"Sustenuto Pedal (on/off)", 0x00, false}},
        {soft_pedal_controller, {"Soft Pedal (on/off)", 0x00, false}},
        {legato_pedal_controller, {"Legato Pedal (on/off)", 0x00, false}},
        {hold_2_pedal_controller, {"Hold 2 Pedal (on/off)", 0x00, false}},
        {sound_controllers[0], {"Sound Variation", 0x00, false}},
        {sound_controllers[1], {"Sound Timbre", 0x00, false}},
        {sound_controllers[2], {"Sound Release Time", 0x00, false}},
        {sound_controllers[3], {"Sound Attack Time", 0x00, false}},
        {sound_controllers[4], {"Sound Brightness", 0x00, false}},
        {sound_controllers[5], {"Sound Control 6", 0x00, false}},
        {sound_controllers[6], {"Sound Control 7", 0x00, false}},
        {sound_controllers[7], {"Sound Control 8", 0x00, false}},
        {sound_controllers[8], {"Sound Control 9", 0x00, false}},
        {sound_controllers[9], {"Sound Control 10", 0x00, false}},
        {general_purpose_button_controllers[0], {"General Purpose Button 1 (on/off)", 0x00, false}},
        {general_purpose_button_controllers[1], {"General Purpose Button 2 (on/off)", 0x00, false}},
        {general_purpose_button_controllers[2], {"General Purpose Button 3 (on/off)", 0x00, false}},
        {general_purpose_button_controllers[3], {"General Purpose Button 4 (on/off)", 0x00, false}},
        {effects_level_controller, {"Effects Level", 0x00, false}},
        {tremulo_level_controller, {"Tremulo Level", 0x00, false}},
        {chorus_level_controller, {"Chorus Level", 0x00, false}},
        {celeste_level_controller, {"Celeste Level", 0x00, false}},
        {phaser_level_controller, {"Phaser Level", 0x00, false}},
        {data_button_increment_controller, {"Data Button increment", 0x00, false}},
        {data_button_decrement_controller, {"Data Button decrement", 0x00, false}},
        {non_registered_parameter_coarse_controller, {"Non-registered Parameter (coarse)", 0x00, false}},
        {non_registered_parameter_fine_controller, {"Non-registered Parameter (fine)", 0x00, false}},
        {registered_parameter_coarse_controller, {"Registered Parameter (coarse)", 0x00, false}},
        {registered_parameter_fine_controller, {"Registered Parameter (fine)", 0x00, false}},
        {all_sound_off_controller, {"All Sound Off", 0x00, true}},
        {all_controllers_off_controller, {"All Controllers Off", 0x00, true}},
        {local_keyboard_controller, {"Local Keyboard (on/off)", 0x00, true}},
        {all_notes_off_controller, {"All Notes Off", 0x00, true}},
        {omni_mode_off_controller, {"Omni Mode Off", 0x00, true}},
        {omni_mode_on_controller, {"Omni Mode On", 0x00, true}},
        {mono_operation_controller, {"Mono Operation", 0x00, true}},
        {poly_operation_controller, {"Poly Operation", 0x00, true}},
    };
    return info_;
}

//========
// Family
//========

static const std::array<const char*, 0x80> program_names = {
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

namespace {

std::ostream& print_bytes(std::ostream& stream, const Event& event) {
    return ::print_bytes(stream, event.begin(), event.end());
}

std::ostream& print_controller(std::ostream& stream, const Event& event) {
    byte_t id = event.at(1);
    auto it = controller_tools::info().find(id);
    if (it != controller_tools::info().end())
        stream << it->second.name;
    else
        stream << "Unknown Controller " << byte_string(id);
    return stream << " (" << (int)event.at(2) << ')';
}

std::ostream& print_program(std::ostream& stream, const Event& event) {
    return stream << program_names[event.at(1) & 0x7f] << " (" << (int)event.at(1) << ')';
}

std::ostream& print_note(std::ostream& stream, const Event& event) {
    byte_t note = event.at(1);
    if (event.channels().contains(drum_channel)) {
        drum_ns::print_drum(stream, note);
    } else {
        stream << Note::from_code(note);
    }
    return stream << " (" << (int)event.at(2) << ')'; /// add velocity/aftertouch
}

std::ostream& print_8bits(std::ostream& stream, const Event& event) {
    return stream << (int)event.at(1);
}

std::ostream& print_14bits(std::ostream& stream, const Event& event) {
    return stream << event.get_14bits();
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

std::ostream& print_meta_string(std::ostream& stream, const Event& event) {
    return stream << event.get_meta_string();
}

std::ostream& print_meta_int(std::ostream& stream, const Event& event) {
    return stream << event.get_meta_int();
}

std::ostream& print_bpm(std::ostream& stream, const Event& event) {
    return stream << decay_value<int>(10. * event.get_bpm()) / 10. << " bpm";
}

std::ostream& print_noop(std::ostream& stream, const Event&) {
    return stream;
}

std::ostream& print_custom(std::ostream& stream, const Event& event) {
    return stream << event.get_custom_key();
}

static const std::unordered_map<family_t, family_tools::info_type> family_infos = {
    {family_t::invalid, {"Invalid Event", print_noop, false}},
    {family_t::custom, {"Custom Event", print_custom, true}},
    {family_t::note_off, {"Note Off", print_note, true}},
    {family_t::note_on, {"Note On", print_note, true}},
    {family_t::aftertouch, {"Aftertouch", print_note, true}},
    {family_t::controller, {"Controller", print_controller, true}},
    {family_t::program_change, {"Program Change", print_program, true}},
    {family_t::channel_pressure, {"Channel Pressure", print_8bits, true}},
    {family_t::pitch_wheel, {"Pitch Wheel", print_14bits, true}},
    {family_t::sysex, {"System Exclusive", print_bytes, false}},
    {family_t::mtc_frame, {"MTC Quarter Frame Message", print_mtc_frame, false}},
    {family_t::song_position, {"Song Position Pointer", print_14bits, false}},
    {family_t::song_select, {"Song Select", print_8bits, false}},
    {family_t::xf4, {"System Common 0xf4", print_noop, false}},
    {family_t::xf5, {"System Common 0xf5", print_noop, false}},
    {family_t::tune_request, {"Tune Request", print_noop, false}},
    {family_t::end_of_sysex, {"End Of Sysex", print_noop, false}},
    {family_t::clock, {"MIDI Clock", print_noop, false}},
    {family_t::tick, {"Tick", print_noop, false}},
    {family_t::start, {"MIDI Start", print_noop, false}},
    {family_t::continue_, {"MIDI Continue", print_noop, false}},
    {family_t::stop, {"MIDI Stop", print_noop, false}},
    {family_t::xfd, {"System Realtime 0xfd", print_noop, false}},
    {family_t::active_sense, {"Active Sense", print_noop, false}},
    {family_t::reset, {"Reset", print_noop, false}},
    {family_t::sequence_number, {"Sequence Number", print_meta_int, false}},
    {family_t::text, {"Text Event", print_meta_string, false}},
    {family_t::copyright, {"Copyright Notice", print_meta_string, false}},
    {family_t::track_name, {"Track Name", print_meta_string, false}},
    {family_t::instrument_name, {"Instrument Name", print_meta_string, false}},
    {family_t::lyrics, {"Lyrics", print_meta_string, false}},
    {family_t::marker, {"Marker", print_meta_string, false}},
    {family_t::cue_point, {"Cue Point", print_meta_string, false}},
    {family_t::program_name, {"Program Name", print_meta_string, false}},
    {family_t::device_name, {"Device Name", print_meta_string, false}},
    {family_t::channel_prefix, {"Channel Prefix", print_meta_int, false}},
    {family_t::port, {"MIDI Port", print_meta_int, false}},
    {family_t::end_of_track, {"End Of Track", print_noop, false}},
    {family_t::tempo, {"Set Tempo", print_bpm, false}},
    {family_t::smpte_offset, {"SMPTE Offset", print_smpte_offset, false}},
    {family_t::time_signature, {"Time Signature", print_time_signature, false}},
    {family_t::key_signature, {"Key Signature", print_key_signature, false}},
    {family_t::proprietary, {"Proprietary", print_bytes, false}},
    {family_t::default_meta, {"Unknown MetaEvent", print_bytes, false}},
};

}

std::ostream& family_tools::print_event(std::ostream& stream, const Event& event) {
    return info(event.family()).printer(stream, event);
}

const family_tools::info_type& family_tools::info(family_t family) {
    static const info_type default_info {"Unknown Event", print_bytes, false};
    auto it = family_infos.find(family);
    return it == family_infos.end() ? default_info : it->second;
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
    return {family_t::pitch_wheel, channels, {0xe0, short_tools::fine(pitch), short_tools::coarse(pitch)}};
}

Event Event::sys_ex(data_type data) {
    if (data.empty() || data.front() != 0xf0 || data.back() != 0xf7 || std::any_of(++data.begin(), --data.end(), is_msb_set<byte_t>))
        return {};
    return {family_t::sysex, {}, std::move(data)};
}

Event Event::master_volume(uint16_t volume, byte_t sysex_channel) {
    return {family_t::sysex, {}, {0xf0, 0x7f, sysex_channel, 0x04, 0x01, short_tools::fine(volume), short_tools::coarse(volume), 0xf7}};
}

Event Event::mtc_frame(byte_t value) {
    return {family_t::mtc_frame, {}, {0xf1, value}};
}

Event Event::song_position(uint16_t value) {
    return {family_t::song_position, {}, {0xf2, short_tools::fine(value), short_tools::coarse(value)}};
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
    if (event.is(voice_families))
        event.m_channels = channels_t::merge(channel);
    return event;
}

// constructors

Event::Event() :
    m_family(family_t::invalid), m_channels(), m_data() {

}

Event::Event(family_t family, channels_t channels, data_type data) :
    m_family(family), m_channels(channels), m_data(std::move(data)) {

}

// miscellaneous

bool data_equals(const Event& e1, const Event& e2) {
    /// @note this will return true when (0xf9, nn, vv) == (0xf9, nn, vv, 00)
    size_t max_size = std::max(e1.size(), e2.size());
    for (size_t i=0 ; i < max_size ; i++)
        if (e1.at(i) != e2.at(i))
            return false;
    return true;
}

bool Event::operator ==(const Event& event) const  {
    return m_family == event.m_family && m_channels == event.m_channels && data_equals(*this, event);
}

// string

std::string Event::name() const {
    return family_tools::info(m_family).name;
}

std::string Event::description() const {
    std::stringstream stream;
    family_tools::print_event(stream, *this);
    return stream.str();
}

std::ostream& operator <<(std::ostream& os, const Event& event) {
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
    return families.contains(m_family);
}

family_t Event::extract_family(bool is_realtime) const {
    switch (at(0, 0x00) & 0xf0) { // VOICE EVENT
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
    return short_tools::glue(at(2), at(1));
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
