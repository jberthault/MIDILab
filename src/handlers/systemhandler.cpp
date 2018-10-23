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

#include "systemhandler.h"

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <map>
#include <set>

class WinSystemHandler : public Handler {

private:
    using buffer_type = std::pair<MIDIHDR, std::vector<char>>;

    HMIDIIN m_handle_in;
    HMIDIOUT m_handle_out;
    UINT m_id_in;
    UINT m_id_out;
    std::list<buffer_type> m_buffers;

public:
    explicit WinSystemHandler(Mode mode, UINT id_in, UINT id_out) :
        Handler{mode}, m_id_in{id_in}, m_id_out{id_out} {

    }

    ~WinSystemHandler() {
        close_system(State::duplex());
    }

protected:

    Result handle_open(State state) override {
        return to_result(open_system(state));
    }

    Result handle_close(State state) override {
        return to_result(close_system(state));
    }

    Result handle_message(const Message& message) override {
        update_buffers();
        if (message.event.is(families_t::standard_voice()))
            return to_result(handle_voice(message.event));
        if (message.event.family() == family_t::sysex)
            return to_result(handle_sysex(message.event));
        if (message.event.family() == family_t::reset)
            return to_result(handle_reset());
        return Result::unhandled;
    }

    families_t handled_families() const override {
        return families_t::fuse(families_t::standard_voice(), family_t::sysex, family_t::reset);
    }

private:

    Result to_result(size_t errors) {
        return errors != 0 ? Result::fail : Result::success;
    }

    size_t check_in(MMRESULT result) const {
        if (result != MMSYSERR_NOERROR) {
            char text[MAXERRORLENGTH];
            midiInGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_ERROR(name() << " (in): " << text);
        }
        return result == MMSYSERR_NOERROR ? 0 : 1;
    }

    size_t check_out(MMRESULT result) const {
        if (result != MMSYSERR_NOERROR) {
            char text[MAXERRORLENGTH];
            midiOutGetErrorTextA(result, text, MAXERRORLENGTH);
            TRACE_ERROR(name() << " (out): " << text);
        }
        return result == MMSYSERR_NOERROR ? 0 : 1;
    }

    static void CALLBACK callback_in(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD param1, DWORD) {
        /// @warning system realtime messages can be in between other messages
        /// @todo do something with all messages (LONGDATA ...)
        /// @note 'param2' could be used to set message time
        auto* handler = reinterpret_cast<WinSystemHandler*>(instance);
        switch (msg) {
        case MIM_OPEN: handler->activate_state(State::forward()); break;
        case MIM_CLOSE: handler->deactivate_state(State::forward()); break;
        case MIM_DATA: handler->read_event(param1); break;
        case MIM_LONGDATA: TRACE_DEBUG(handler << " long-data received"); break;
        case MIM_ERROR: TRACE_DEBUG(handler << " error received"); break;
        case MIM_LONGERROR: TRACE_DEBUG(handler << " long-error received"); break;
        case MIM_MOREDATA: TRACE_DEBUG(handler << " more-data received"); break;
        default: /*should never happen*/ break;
        }
    }

    static void CALLBACK callback_out(HMIDIOUT, UINT msg, DWORD_PTR instance, DWORD, DWORD) {
        auto* handler = reinterpret_cast<WinSystemHandler*>(instance);
        switch (msg) {
        case MOM_CLOSE: handler->deactivate_state(State::receive()); break;
        case MOM_OPEN: handler->activate_state(State::receive()); break;
        case MOM_DONE: break;
        default: /*should never happen*/ break;
        }
    }

    size_t open_system(State s) {
        size_t errors = 0;
        if (mode().any(Mode::in()) && s.any(State::forward()) && state().none(State::forward())) {
            errors += check_in(midiInOpen(&m_handle_in, m_id_in, (DWORD_PTR)callback_in, (DWORD_PTR)this, CALLBACK_FUNCTION));
            errors += check_in(midiInStart(m_handle_in));
        }
        if (mode().any(Mode::out()) && s.any(State::receive()) && state().none(State::receive())) {
            errors += check_out(midiOutOpen(&m_handle_out, m_id_out, (DWORD_PTR)callback_out, (DWORD_PTR)this, CALLBACK_FUNCTION));
            check_out(midiOutSetVolume(m_handle_out, 0xffffffff)); // full volume (volume settings are done using sysex messages) (ignoring errors here)
        }
        return errors;
    }

    size_t close_system(State s) {
        size_t errors = 0;
        if (mode().any(Mode::in()) && s.any(State::forward()) && state().any(State::forward())) {
            errors += check_in(midiInStop(m_handle_in));
            errors += check_in(midiInClose(m_handle_in));
        }
        if (mode().any(Mode::out()) && s.any(State::receive()) && state().any(State::receive())) {
            errors += handle_reset();
            while (!m_buffers.empty())
                update_buffers();
            errors += check_out(midiOutClose(m_handle_out));
        }
        return errors;
    }

    static Event event_from_data(DWORD data) {
        auto buf = range_ns::from_span(reinterpret_cast<const byte_t*>(&data), 4);
        try {
            return dumping::read_event(buf, true);
        } catch (...) {
            return {};
        }
    }

    void read_event(DWORD data) {
        if (state().any(State::forward()))
            if (auto event = event_from_data(data))
                produce_message(std::move(event));
    }

    size_t handle_sysex(const Event& event) {
        return handle_buffer({event.begin(), event.end()});
    }

    size_t handle_voice(const Event& event) {
        /// @note can't bufferize in a single midiOutLongMsg due to a lack of support of somes devices
        size_t errors = 0;
        const auto frame = *reinterpret_cast<const uint32_t*>(&event.data()[0]) & ~0xf; // event data with channel cleared
        for (channel_t channel : event.channels())
            errors += check_out(midiOutShortMsg(m_handle_out, frame | channel));
        return errors;
    }

    size_t handle_reset() {
        /// @note can't bufferize in a single midiOutLongMsg due to a lack of support of somes devices
        size_t errors = 0;
        for (byte_t controller : controller_ns::reset_controllers)
            errors += handle_voice(Event::controller(channels_t::full(), controller));
        errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.coarse, 0));
        errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.fine, 0));
        errors += handle_voice(Event::controller(channels_t::full(), controller_ns::data_entry_controller.coarse, 2));
        errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.coarse, 0x7f));
        errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.fine, 0x7f));
        return errors;
    }

    size_t handle_buffer(std::vector<char>&& buffer_data) {
        m_buffers.emplace_back(MIDIHDR{}, std::move(buffer_data));
        auto& buf = m_buffers.back();

        ::memset(&buf.first, 0, sizeof(MIDIHDR));
        buf.first.dwBufferLength = static_cast<DWORD>(buf.second.capacity());
        buf.first.dwBytesRecorded = static_cast<DWORD>(buf.second.size());
        buf.first.lpData = buf.second.data();

        size_t errors = 0;
        errors += check_out(midiOutPrepareHeader(m_handle_out, &buf.first, sizeof(MIDIHDR)));
        errors += check_out(midiOutLongMsg(m_handle_out, &buf.first, sizeof(MIDIHDR)));
        return errors;
    }

    void update_buffers() {
        auto it = m_buffers.begin();
        while (it != m_buffers.end()) {
            if (MIDIERR_STILLPLAYING == midiOutUnprepareHeader(m_handle_out, &it->first, sizeof(MIDIHDR))) {
                ++it;
            } else {
                it = m_buffers.erase(it);
            }
        }
    }

    // unused features:
    //midiOutMessage()
    //midiOutCacheDrumPatches()
    //midiOutCachePatches()
    //midiOutGetVolume()

};

struct SystemHandlerFactory::Impl {

    struct identifier_type {

        UINT ivalue() const { return in.empty() ? 0u : *in.begin(); }
        UINT ovalue() const { return out.empty() ? 0u : *out.begin(); }

        auto imode() const { return in.empty() ? Handler::Mode{} : Handler::Mode::in(); }
        auto omode() const { return out.empty() ? Handler::Mode{} : Handler::Mode::out(); }

        auto instantiate() const {
            auto handler = std::make_unique<WinSystemHandler>(imode() | omode(), ivalue(), ovalue());
            handler->set_name(name);
            return handler;
        }

        void update(const identifier_type& rhs) {
            in.insert(rhs.in .begin(), rhs.in.end());
            out.insert(rhs.out.begin(), rhs.out.end());
        }

        std::string name;
        std::set<UINT> in;
        std::set<UINT> out;

    };

    auto find(const std::string& name) {
        return std::find_if(identifiers.begin(), identifiers.end(), [&](const auto& id) { return id.name == name; });
    }

    std::vector<std::string> available() const {
        std::vector<std::string> names(identifiers.size());
        std::transform(identifiers.begin(), identifiers.end(), names.begin(), [](const auto& id) { return id.name; });
        return names;
    }

    void insert(identifier_type id) {
        auto it = find(id.name);
        if (it != identifiers.end())
            it->update(id);
        else
            identifiers.push_back(std::move(id));
    }

    void insert_out(UINT value) {
        MIDIOUTCAPSA moc;
        MMRESULT result = midiOutGetDevCapsA(value, &moc, sizeof(MIDIOUTCAPSA));
        if (result == MMSYSERR_NOERROR) {
            insert({moc.szPname, {}, {value}});
        } else {
            char buffer[MAXERRORLENGTH];
            midiOutGetErrorTextA(result, buffer, MAXERRORLENGTH);
            TRACE_WARNING(buffer);
        }
    }

    void insert_in(UINT value) {
        MIDIINCAPSA mic;
        MMRESULT result = midiInGetDevCapsA(value, &mic, sizeof(MIDIINCAPSA));
        if (result == MMSYSERR_NOERROR) {
            insert({mic.szPname, {value}, {}});
        } else {
            char buffer[MAXERRORLENGTH];
            midiInGetErrorTextA(result, buffer, MAXERRORLENGTH);
            TRACE_WARNING(buffer);
        }
    }

    void update() {
        identifiers.clear();
        insert_out(-1);
        for (UINT i=0 ; i < midiOutGetNumDevs() ; i++)
            insert_out(i);
        for (UINT i=0 ; i < midiInGetNumDevs() ; i++)
            insert_in(i);
    }

    std::unique_ptr<Handler> instantiate(const std::string& name) {
        auto it = find(name);
        return it == identifiers.end() ? nullptr : it->instantiate();
    }

    std::vector<identifier_type> identifiers;

};

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

        LinuxSystemHandler(Mode mode, std::string hardware_name) :
            Handler(mode), m_hardware_name(std::move(hardware_name)) {

        }

        ~LinuxSystemHandler() {
            if (is_receive_enabled()) {
                handle_reset();
                close_system(State::duplex());
            }
        }

    protected:

        Result handle_open(State state) override {
            return to_result(open_system(state));
        }

        Result handle_close(State state) override {
            if (is_receive_enabled())
                handle_reset();
            return to_result(close_system(state));
        }

        Result handle_message(const Message& message) override {
            if (message.event.is(families_t::standard_voice()))
                return to_result(handle_voice(message.event));
            if (message.event.family() == family_t::reset)
                return to_result(handle_reset());
            return Result::unhandled;
        }

        families_t handled_families() const override {
            return families_t::fuse(families_t::standard_voice(), family_t::reset);
        }

    private:

        Result to_result(size_t errors) {
            return errors != 0 ? Result::fail : Result::success;
        }

        size_t check(int errnum) {
            if (errnum >= 0)
                return 0;
            TRACE_WARNING(name() << ": " << snd_strerror(errnum));
            return 1;
        }

        size_t open_system(State s) {
            size_t errors = 0;
            bool in_opening = mode().any(Mode::in()) && s.any(State::forward()) && state().none(State::forward());
            bool out_opening = mode().any(Mode::out()) && s.any(State::receive()) && state().none(State::receive());
            if (in_opening || out_opening) {
                snd_rawmidi_t** in = in_opening ? &m_i_handler : nullptr;
                snd_rawmidi_t** out = out_opening ? &m_o_handler : nullptr;
                errors += check(snd_rawmidi_open(in, out, m_hardware_name.c_str(), 0));
            } else {
                TRACE_WARNING("Can't open " << name() << ": nothing to open");
                ++errors;
            }
            if (!errors) {
                if (in_opening)
                    m_i_reader = std::thread([this](){i_callback();});
                activate_state(s);
            }
            return errors;
        }

        size_t close_system(State s) {
            size_t errors = 0;
            // close input handler
            if (mode().any(Mode::in()) && s.any(State::forward()) && state().any(State::forward())) {
                deactivate_state(State::forward());
                m_i_reader.join();
                errors += check(snd_rawmidi_close(m_i_handler));
            }
            // close output handler
            if (mode().any(Mode::out()) && s.any(State::receive()) && state().any(State::receive())) {
                deactivate_state(State::receive());
                errors += check(snd_rawmidi_close(m_o_handler));
            }
            return errors;
        }

        size_t handle_voice(const Event& event) {
            size_t errors = 0;
            Event::data_type data = event.data();
            for (channel_t c : event.channels()) {
                data[0] = (data[0] & ~0xf) | c;
                errors += check(snd_rawmidi_write(m_o_handler, data.data(), data.size()));
            }
            return errors;
        }

        void i_callback() {

            /// @todo integrate running status (with a dummy one on exceptions)

            snd_rawmidi_nonblock(m_i_handler, 1);
            static const size_t max_miss = 15;

            size_t missed = 0;
            byte_t readed;
            std::vector<byte_t> storage;

            while (state().any(State::forward())) {
                int err = snd_rawmidi_read(m_i_handler, &readed, 1);
                if (err < 0 && (err != -EBUSY) && (err != -EAGAIN)) {
                    TRACE_WARNING("Can't read data from " << name() << ": " << snd_strerror(err));
                    break;
                } else if (err >= 0) {
                    storage.push_back(readed);
                    try {
                        auto buf = range_ns::from_span<const byte*>(storage.data(), storage.size());
                        if (auto event = dumping::read_event(buf, true))
                            produce_message(std::move(event));
                        missed = 0;
                        storage.clear();
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
            return state().any(State::receive()) && mode().any(Mode::receive());
        }

        size_t handle_reset() {
            size_t errors = 0;
            for (byte_t controller : controller_ns::reset_controllers)
                errors += handle_voice(Event::controller(channels_t::full(), controller));
            errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.coarse, 0));
            errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.fine, 0));
            errors += handle_voice(Event::controller(channels_t::full(), controller_ns::data_entry_controller.coarse, 2));
            errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.coarse, 0x7f));
            errors += handle_voice(Event::controller(channels_t::full(), controller_ns::registered_parameter_controller.fine, 0x7f));
            return errors;
        }

};

struct SystemHandlerFactory::Impl {

    struct identifier_type {

        auto instantiate() const {
            auto handler = std::make_unique<LinuxSystemHandler>(mode, hardware_name);
            handler->set_name(name);
            return handler;
        }

        std::string name;
        std::string hardware_name;
        Handler::Mode mode;

    };

    auto find(const std::string& name) {
        return std::find_if(identifiers.begin(), identifiers.end(), [&](const auto& id) { return id.name == name; });
    }

    std::vector<std::string> available() const {
        std::vector<std::string> names(identifiers.size());
        std::transform(identifiers.begin(), identifiers.end(), names.begin(), [](const auto& id) { return id.name; });
        return names;
    }

    void insert(identifier_type id) {
        auto it = find(id.name);
        if (it != identifiers.end())
            it->mode |= id.mode;
        else
            identifiers.push_back(std::move(id));
    }

    void update() {
        identifiers.clear();
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

                    auto mode = Handler::Mode::out();

                    while (mode) {
                        snd_rawmidi_info_set_stream(rawMidiInfo, mode == Handler::Mode::in() ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
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
                            insert(identifier_type{name, hw_stream.str(), mode});
                        }
                        mode = (mode == Handler::Mode::out()) ? Handler::Mode::in() : Handler::Mode{};
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
    }

    std::unique_ptr<Handler> instantiate(const std::string& name) {
        auto it = find(name);
        return it == identifiers.end() ? nullptr : it->instantiate();
    }

    std::vector<identifier_type> identifiers;

};

#else

#pragma message ("no system handler available for the current platform")

struct SystemHandlerFactory::Impl {
    std::vector<std::string> available() const { return {}; }
    void update() { }
    std::unique_ptr<Handler> instantiate(const std::string& /*name*/) { return nullptr; }
};

#endif

//======================
// SystemHandlerFactory
//======================

SystemHandlerFactory::SystemHandlerFactory() : m_impl{std::make_unique<Impl>()} {
    update();
}

SystemHandlerFactory::~SystemHandlerFactory() {

}

std::vector<std::string> SystemHandlerFactory::available() const {
    return m_impl->available();
}

void SystemHandlerFactory::update() {
    m_impl->update();
}

std::unique_ptr<Handler> SystemHandlerFactory::instantiate(const std::string& name) {
    return m_impl->instantiate(name);
}
