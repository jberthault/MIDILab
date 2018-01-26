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

#ifndef CORE_NOTE_H
#define CORE_NOTE_H

#include <vector>   // std::vector
#include <string>   // std::string
#include <iostream> // std::istream sdt::ostream

//======
// Note
//======

enum class alteration_t : int8_t {
    natural = 0,
    sharp = 1,
    flat = -1
};

enum class tonality_t {
    no_tonality, // special tonality
    A, B, C, D, E, F, G, // natural notes
    Ad, Bd, Cd, Dd, Ed, Fd, Gd, // sharped notes
    Ab, Bb, Cb, Db, Eb, Fb, Gb // flat notes
};

std::istream& operator>>(std::istream& stream, tonality_t& tonality);
std::ostream& operator<<(std::ostream& stream, const tonality_t& tonality);


class Note {

public:
    using tuning_type = std::pair<Note, double>;
    static const tuning_type tuning_reference; /*!< tuning reference (A3, 440 Hz) */

    static Note from_code(int code); /*!< code stands for the extended midi number */
    static Note from_frequency(double frequency, const tuning_type& tuning = tuning_reference); /*!< will get the note to the nearest frequency */
    static Note from_string(const std::string& string); /*!< string must match [A-G][#b]?(-?[0-9]+) otherwise an invalid note is returned */

    Note(tonality_t tonality = tonality_t::no_tonality, int octave = 0);

    bool is_black() const; /*!< returns true if altered (aka equivalent to any of A#, C#, D#, F# or G#) */
    alteration_t alteration() const; /*!< @see from_base (no_tonality yield Alteration::natural) */
    int code() const; /*!< value correspond to the midi note: 12*octave + tonality.value() + offset, 0 if undefined */
    double frequency(const tuning_type& tuning = tuning_reference) const;
    std::string string() const; /*!< @see from_string (undefined note yields an empty string) */

    friend bool operator==(const Note& lhs, const Note& rhs); /*!< true if tonalities & octave equals */
    friend bool operator!=(const Note& lhs, const Note& rhs); /*!< @see operator== */

    explicit operator bool() const; /*!< return true if tonality is well formed */

    friend std::istream& operator>>(std::istream& stream, Note& note);
    friend std::ostream& operator<<(std::ostream& stream, const Note& note);

private:
    tonality_t m_tonality; /*!< tonality of the note */
    int m_octave; /*!< octave can be any whole number */

};

namespace note_ns {

template<tonality_t T>
struct tonality_constant : std::integral_constant<tonality_t, T> {
    inline Note operator()(int octave) const { return {T, octave}; }
};

constexpr auto A = tonality_constant<tonality_t::A>{};
constexpr auto B = tonality_constant<tonality_t::B>{};
constexpr auto C = tonality_constant<tonality_t::C>{};
constexpr auto D = tonality_constant<tonality_t::D>{};
constexpr auto E = tonality_constant<tonality_t::E>{};
constexpr auto F = tonality_constant<tonality_t::F>{};
constexpr auto G = tonality_constant<tonality_t::G>{};
constexpr auto Ad = tonality_constant<tonality_t::Ad>{};
constexpr auto Bd = tonality_constant<tonality_t::Bd>{};
constexpr auto Cd = tonality_constant<tonality_t::Cd>{};
constexpr auto Dd = tonality_constant<tonality_t::Dd>{};
constexpr auto Ed = tonality_constant<tonality_t::Ed>{};
constexpr auto Fd = tonality_constant<tonality_t::Fd>{};
constexpr auto Gd = tonality_constant<tonality_t::Gd>{};
constexpr auto Ab = tonality_constant<tonality_t::Ab>{};
constexpr auto Bb = tonality_constant<tonality_t::Bb>{};
constexpr auto Cb = tonality_constant<tonality_t::Cb>{};
constexpr auto Db = tonality_constant<tonality_t::Db>{};
constexpr auto Eb = tonality_constant<tonality_t::Eb>{};
constexpr auto Fb = tonality_constant<tonality_t::Fb>{};
constexpr auto Gb = tonality_constant<tonality_t::Gb>{};

}

#endif // CORE_NOTE_H
