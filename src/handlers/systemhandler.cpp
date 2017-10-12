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

#include "systemhandler.h"

using namespace family_ns;
using namespace controller_ns;

Event volume_event(uint16_t left, uint16_t right) {
    uint32_t volume = (right << 16) | left;
    return Event::custom({}, "System.volume", marshall(volume));
}

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <map>
#include <set>

class WinSystemHandler : public Handler {

public:

    WinSystemHandler(mode_type mode, UINT id_in, UINT id_out) :
        Handler(mode), m_id_in(id_in), m_id_out(id_out) {

    }

    ~WinSystemHandler() {
        if (is_receive_enabled()) {
            reset_system();
            close_system(handler_ns::endpoints_state);
        }
    }

    family_t handled_families() const override {
        return custom_family | voice_families | reset_family;
    }

    result_type handle_message(const Message& message) override {
        MIDI_HANDLE_OPEN;
        MIDI_CHECK_OPEN_RECEIVE;
        if (mode().any(handler_ns::receive_mode)) {
            if (message.event.is(voice_families))
                return write_event(message.event);
            if (message.event.is(sysex_family))
                return write_sysex(message.event);
            if (message.event.is(reset_family))
                return reset_system();
            if (message.event.is(custom_family) && message.event.get_custom_key() == "System.volume")
                return set_volume((DWORD)unmarshall<uint32_t>(message.event.get_custom_value()));
        }
        return handler_ns::unhandled_result;
    }

    result_type on_open(state_type state) override {
        return open_system(state);
    }

    result_type on_close(state_type state) override {
        if (is_receive_enabled())
            reset_system();
        return close_system(state);
    }

private:

    static result_type convert_result(MMRESULT result) {
        /// @todo enhance results
        return result == MMSYSERR_NOERROR ? handler_ns::success_result : handler_ns::fail_result;
    }

    result_type check_in(MMRESULT result) const {
        if (result != MMSYSERR_NOERROR) {
            char text[MAXERRORLENGTH];
            midiInGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_WARNING("IN " << name() << ": " << text);
        }
        return convert_result(result);
    }

    result_type check_out(MMRESULT result) const {
        if (result != MMSYSERR_NOERROR) {
            char text[MAXERRORLENGTH];
            midiOutGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_WARNING("OUT " << name() << ": " << text);
        }
        return convert_result(result);
    }

    static void CALLBACK callback_in(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD param1, DWORD) {
        /// @warning system realtime messages can be in between other messages
        /// @todo do something with all messages (LONGDATA ...)
        /// @note 'param2' could be used to set message time
        WinSystemHandler* handler = reinterpret_cast<WinSystemHandler*>(instance);
        switch (msg) {
        case MIM_OPEN: handler->alter_state(handler_ns::forward_state, true); break;
        case MIM_CLOSE: handler->alter_state(handler_ns::forward_state, false); break;
        case MIM_DATA: handler->read_event(param1); break;
        case MIM_LONGDATA: TRACE_DEBUG(handler << " long-data received"); break;
        case MIM_ERROR: TRACE_DEBUG(handler << " error received"); break;
        case MIM_LONGERROR: TRACE_DEBUG(handler << " long-error received"); break;
        case MIM_MOREDATA: TRACE_DEBUG(handler << " more-data received"); break;
        default: /*should never happen*/ break;
        }
    }

    static void CALLBACK callback_out(HMIDIOUT, UINT msg, DWORD_PTR instance, DWORD, DWORD) {
        WinSystemHandler* handler = reinterpret_cast<WinSystemHandler*>(instance);
        switch (msg) {
        case MOM_CLOSE: handler->alter_state(handler_ns::receive_state, false); break;
        case MOM_OPEN: handler->alter_state(handler_ns::receive_state, true); break;
        case MOM_DONE: break;
        default: /*should never happen*/ break;
        }
    }

    result_type set_volume(DWORD volume) {
        return check_out(midiOutSetVolume(m_handle_out, volume));
    }

    result_type open_system(state_type s) {
        result_type result;
        if (mode().any(handler_ns::in_mode) && s.any(handler_ns::forward_state) && state().none(handler_ns::forward_state)) {
            result |= check_in(midiInOpen(&m_handle_in, m_id_in, (DWORD_PTR)callback_in, (DWORD_PTR)this, CALLBACK_FUNCTION));
            result |= check_in(midiInStart(m_handle_in));
        }
        if (mode().any(handler_ns::out_mode) && s.any(handler_ns::receive_state) && state().none(handler_ns::receive_state)) {
            result |= check_out(midiOutOpen(&m_handle_out, m_id_out, (DWORD_PTR)callback_out, (DWORD_PTR)this, CALLBACK_FUNCTION));
            set_volume(0xffffffff); // full volume (volume settings are done using sysex messages)
        }
        return result;
    }

    result_type close_system(state_type s) {
        result_type result;
        if (mode().any(handler_ns::in_mode) && s.any(handler_ns::forward_state) && state().any(handler_ns::forward_state)) {
            result |= check_in(midiInStop(m_handle_in));
            result |= check_in(midiInClose(m_handle_in));
        }
        if (mode().any(handler_ns::out_mode) && s.any(handler_ns::receive_state) && state().any(handler_ns::receive_state)) {
            result |= check_out(midiOutClose(m_handle_out));
        }
        return result;
    }

    result_type read_event(DWORD data) {
        if (state().any(handler_ns::forward_state)) {
            byte_t* bytes = reinterpret_cast<byte_t*>(&data);
            forward_message({Event::raw(true, {bytes, bytes+4}), this});
            return handler_ns::success_result;
        }
        return handler_ns::closed_result;
    }

    result_type write_sysex(const Event& event) {
        MIDIHDR hdr;
        memset(&hdr, 0, sizeof(MIDIHDR));
        std::vector<char> sys_ex_data(event.begin(), event.end());
        hdr.dwBufferLength = (DWORD)event.size();
        hdr.lpData = sys_ex_data.data();
        check_out(midiOutPrepareHeader(m_handle_out, &hdr, sizeof(MIDIHDR)));
        check_out(midiOutLongMsg(m_handle_out, &hdr, sizeof(MIDIHDR)));
        check_out(midiOutUnprepareHeader(m_handle_out, &hdr, sizeof(MIDIHDR)));
        return handler_ns::success_result;
    }

    result_type write_event(const Event& event) {
        /// merge all in a midiOutLongMsg() ?
        result_type result;
        uint32_t frame = *reinterpret_cast<const uint32_t*>(&event.data()[0]);
        frame &= ~0xf; // clear low nibble
        for (channel_t channel : event.channels())
            result |= check_out(midiOutShortMsg(m_handle_out, frame | channel));
        return result;
    }

    bool is_receive_enabled() const {
        return state().any(handler_ns::receive_state) && mode().any(handler_ns::receive_mode);
    }

    result_type reset_system() {
        // return check_out(midiOutReset(m_handle_out));
        result_type result;
        result |= write_event(Event::controller(all_channels, all_controllers_off_controller));
        result |= write_event(Event::controller(all_channels, all_sound_off_controller));
        // result |= write_event(Event::controller(all_channels, volume_coarse_controller, 100));
        return result;
    }

    HMIDIIN m_handle_in;
    HMIDIOUT m_handle_out;
    UINT m_id_in;
    UINT m_id_out;

    // unused features:
    //midiOutMessage()
    //midiOutLongMsg()
    //midiOutCacheDrumPatches()
    //midiOutCachePatches()
    //midiOutGetVolume()

};

/// @note do not build the MAPPER
std::list<Handler*> create_system() {
    /// @todo optimization & check that name is unique
    std::list<Handler*> handlers;
    std::set<std::string> names;
    std::unordered_map<std::string, UINT> in, out;
    MIDIOUTCAPSA moc;
    MIDIINCAPSA mic;
    MMRESULT result;
    char text[MAXERRORLENGTH];
    for (UINT i=0 ; i < midiOutGetNumDevs() ; i++) {
        result = midiOutGetDevCapsA(i, &moc, sizeof(MIDIOUTCAPSA));
        if (result != MMSYSERR_NOERROR) {
            midiOutGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_WARNING(text);
        } else {
            names.insert(moc.szPname);
            out[moc.szPname] = i;
        }
    }
    for (UINT i=0 ; i < midiInGetNumDevs() ; i++) {
        result = midiInGetDevCapsA(i, &mic, sizeof(MIDIINCAPSA));
        if (result != MMSYSERR_NOERROR) {
            midiInGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_WARNING(text);
        } else {
            names.insert(mic.szPname);
            in[mic.szPname] = i;
        }
    }
    for (const std::string& name: names) {
        Handler::mode_type mode;
        UINT id_in = 0, id_out = 0;
        auto in_pos = in.find(name);
        if (in_pos != in.end()) {
            mode |= handler_ns::in_mode;
            id_in = in_pos->second;
        }
        auto out_pos = out.find(name);
        if (out_pos != out.end()) {
            mode |= handler_ns::out_mode;
            id_out = out_pos->second;
        }
        Handler* handler = new WinSystemHandler(mode, id_in, id_out);
        handler->set_name(name);
        handlers.push_back(handler);
    }
    return handlers;
}

#elif defined(__linux__)

#include <alsa/asoundlib.h>
//#include <map>
//#include <vector>
//#include <thread>
#include "core/sequence.h"
#include <sstream>

class LinuxSystemHandler : public Handler {

    private:

        const std::string m_hardware_name;
        snd_rawmidi_t* m_i_handler;
        snd_rawmidi_t* m_o_handler;
        std::thread m_i_reader;

    public:

        LinuxSystemHandler(mode_type mode, std::string hardware_name) :
            Handler(mode), m_hardware_name(std::move(hardware_name)) {

        }

        ~LinuxSystemHandler() {
            if (is_receive_enabled()) {
                reset_system();
                close_system(handler_ns::endpoints_state);
            }
        }

        std::string hardware_name() const {
            return m_hardware_name;
        }

        result_type on_open(state_type state) override {
            return open_system(state);
        }

        result_type on_close(state_type state) override {
            if (is_receive_enabled())
                reset_system();
            return close_system(state);
        }

    private:

        result_type open_system(state_type s) {
            bool in_opening = mode().any(handler_ns::in_mode) && s.any(handler_ns::forward_state) && state().none(handler_ns::forward_state);
            bool out_opening = mode().any(handler_ns::out_mode) && s.any(handler_ns::receive_state) && state().none(handler_ns::receive_state);
            if (in_opening || out_opening) {
                snd_rawmidi_t** in = in_opening ? &m_i_handler : nullptr;
                snd_rawmidi_t** out = out_opening ? &m_o_handler : nullptr;
                int err = snd_rawmidi_open(in, out, m_hardware_name.c_str(), 0);
                if (err < 0) {
                    TRACE_WARNING("Can't open " << name() << ": " << snd_strerror(err));
                    return handler_ns::fail_result;
                }
            } else {
                TRACE_WARNING("Can't open " << name() << ": nothing to open");
                return handler_ns::fail_result;
            }
            if (in_opening)
                m_i_reader = std::thread([this](){i_callback();});
            alter_state(s, true);
            return handler_ns::success_result;
        }

        result_type close_system(state_type s) {
            // close input handler
            if (mode().any(handler_ns::in_mode) && s.any(handler_ns::forward_state) && state().any(handler_ns::forward_state)) {
                alter_state(handler_ns::forward_state, false);
                m_i_reader.join();
                int err_i = snd_rawmidi_close(m_i_handler);
                if (err_i < 0)
                    TRACE_WARNING("Error while closing " << name() << ": " << snd_strerror(err_i));
            }
            // close output handler
            if (mode().any(handler_ns::out_mode) && s.any(handler_ns::receive_state) && state().any(handler_ns::receive_state)) {
                alter_state(handler_ns::receive_state, false);
                int err_o = snd_rawmidi_close(m_o_handler);
                if (err_o < 0)
                    TRACE_WARNING("Error while closing " << name() << ": " << snd_strerror(err_o));
            }
            return handler_ns::success_result;
        }

        result_type write_event(const Event& event) {
            result_type result;
            Event::data_type data = event.data();
            for (channel_t c : event.channels()) {
                data[0] = (data[0] & ~0xf) | c;
                snd_rawmidi_write(m_o_handler, data.data(), data.size());
                result |= handler_ns::success_result;
            }
            return result;
        }

        void i_callback() {

            /// @todo improve using create a special stl stream using rawmidi_read
            /// @todo integrate running status (with a dummy one on exceptions)

            std::stringbuf buffer;

            snd_rawmidi_nonblock(m_i_handler, 1);
            static const size_t max_miss = 15;

            size_t missed = 0;
            int err;
            byte_t readed;
            Event event;
            std::stringstream stream;
            stream.exceptions(std::ios_base::failbit);

            while (state().any(handler_ns::forward_state)) {
                err = snd_rawmidi_read(m_i_handler, &readed, 1);
                if (err < 0 && (err != -EBUSY) && (err != -EAGAIN)) {
                    TRACE_WARNING("Can't read data from " << name() << ": " << snd_strerror(err));
                    break;
                } else if (err >= 0) {
                    buffer.sputc(readed);
                    stream.clear();
                    stream.str(buffer.str());
                    try {
                        event = dumping::read_event(stream, true);
                        // stream.tellg()
                        missed = 0;
                        buffer.str(""); // clear buffer
                        forward_message(event);
                    } catch (const std::exception& /*err*/) {
                        missed++;
                        if (missed > max_miss) {
                            TRACE_WARNING("Too many missed");
                            // change by closing set_status(open_forward_status, false);
                            //break;
                        }
                    }
                }
                std::this_thread::yield();
            }
        }

        bool is_receive_enabled() const {
            return state().any(handler_ns::receive_state) && mode().any(handler_ns::receive_mode);
        }

        family_t handled_families() const override {
            return custom_family | voice_families | reset_family;
        }

        result_type handle_message(const Message& message) override {
            MIDI_HANDLE_OPEN;
            MIDI_CHECK_OPEN_RECEIVE;
            if (mode().any(handler_ns::receive_mode)) {
                if (message.event.is(voice_families))
                    return write_event(message.event);
                if (message.event.is(reset_family))
                    return reset_system();
            }
            return handler_ns::unhandled_result;
        }

        result_type reset_system() {
            result_type result;
            result |= write_event(Event::controller(all_channels, all_controllers_off_controller));
            result |= write_event(Event::controller(all_channels, all_sound_off_controller));
            return result;
        }

};

std::list<Handler*> create_system() {
    // {(HardwareName, HandlerName): Mode}
    std::map<std::pair<std::string, std::string>, handler_ns::mode_t> names;
    // Start with first card
    int cardNum = -1;
    while (true) {
        int err;
        snd_ctl_t *cardHandle;
        // Get next sound card's card number. When "cardNum" == -1, then ALSA fetches the first card
        if ((err = snd_card_next(&cardNum)) < 0) {
            TRACE_WARNING("Can't get the next card number: " << snd_strerror(err));
            break;
        }
        // No more cards? ALSA sets "cardNum" to -1 if so
        if (cardNum < 0)
            break;
        // Open this card's control interface. We specify only the card number -- not any device nor sub-device too
        {
            char str[64];
            sprintf(str, "hw:%i", cardNum);
            if ((err = snd_ctl_open(&cardHandle, str, 0)) < 0) {
                TRACE_WARNING("Can't open card " << cardNum << ": " << snd_strerror(err));
                continue;
            }
        }
        {
            // Start with the first MIDI device on this card
            int devNum = -1;
            while (true) {
                // Get the number of the next MIDI device on this card
                if ((err = snd_ctl_rawmidi_next_device(cardHandle, &devNum)) < 0) {
                    TRACE_WARNING("Can't get next MIDI device number: " << snd_strerror(err));
                    break;
                }
                // No more MIDI devices on this card? ALSA sets "devNum" to -1 if so.
                if (devNum < 0)
                    break;
                // To get some info about the subdevices of this MIDI device (on the card), we need a
                // snd_rawmidi_info_t, so let's allocate one on the stack
                snd_rawmidi_info_t *rawMidiInfo;
                snd_rawmidi_info_alloca(&rawMidiInfo);
                memset(rawMidiInfo, 0, snd_rawmidi_info_sizeof());

                // Tell ALSA which device (number) we want info about
                snd_rawmidi_info_set_device(rawMidiInfo, devNum);

                handler_ns::mode_t mode = handler_ns::out_mode;

                while (mode) {
                    snd_rawmidi_info_set_stream(rawMidiInfo, mode == handler_ns::in_mode ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
                    int i = -1;
                    int subDevCount = 1;
                    while (++i < subDevCount) {
                        snd_rawmidi_info_set_subdevice(rawMidiInfo, i);
                        if ((err = snd_ctl_rawmidi_info(cardHandle, rawMidiInfo)) < 0) {
                            TRACE_WARNING("Can't get info for MIDI subdevice " << cardNum << "," << devNum << "," << i << ": " << snd_strerror(err));
                            continue;
                        }
                        if (!i)
                            subDevCount = snd_rawmidi_info_get_subdevices_count(rawMidiInfo);
                        const char* name = snd_rawmidi_info_get_name(rawMidiInfo);
                        std::stringstream hw_stream;
                        hw_stream << "hw:" << cardNum << ',' << devNum << ',' << i;
                        std::pair<std::string, std::string> key(hw_stream.str(), name);
                        // std::cout << key.first << " " << key.second;
                        names[key] |= mode;
                    }
                    mode = (mode == handler_ns::out_mode) ? handler_ns::in_mode : handler_ns::mode_t{};
                }
            }
        }
        // Close the card's control interface after we're done with it
        snd_ctl_close(cardHandle);
    }
    // ALSA allocates some mem to load its config file when we call some of the
    // above functions. Now that we're done getting the info, let's tell ALSA
    // to unload the info and free up that mem
    snd_config_update_free_global();
    // Fill handler list
    std::list<Handler*> handlers;
    for (const auto& entry : names) {
        LinuxSystemHandler* handler = new LinuxSystemHandler(entry.second, entry.first.first);
        handler->set_name(entry.first.second);
        handlers.push_back(handler);
        TRACE_DEBUG("found device " << handler->name() << " at " << handler->hardware_name());
    }
    return handlers;
}

#else

#pragma message ("no system handler available for the current platform")
std::list<Handler*> create_system() {
    return std::list<Handler*>();
}

#endif
