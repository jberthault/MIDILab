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

#include <cmath>     // exp2, log2
#include <sstream>   // std::istringstream
#include <stdexcept> // std::runtime_error
#include <type_traits>
#include "note.h"
#include "tools/bytes.h"

namespace {

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr auto safe_div(T num, T den) {
    return (num >= 0 ? num : (num - den + 1)) / den;
}

constexpr int code_offset = 12; /*!< midi code of Note(tonalities[0], 0) */

}

namespace tonality_ns {

using namespace note_ns;

constexpr tonality_t tonalities[] = {C, Cd, D, Dd, E, F, Fd, G, Gd, A, Ad, B};

constexpr alteration_t alteration(tonality_t tonality) {
    switch (tonality) {
    case A:  case B:  case C:  case D:  case E:  case F:  case G:  return alteration_t::natural;
    case Ad: case Bd: case Cd: case Dd: case Ed: case Fd: case Gd: return alteration_t::sharp;
    case Ab: case Bb: case Cb: case Db: case Eb: case Fb: case Gb: return alteration_t::flat;
    }
    return alteration_t::natural;
}

const char* to_string(tonality_t tonality) {
    switch (tonality) {
    case A:  return "A";
    case B:  return "B";
    case C:  return "C";
    case D:  return "D";
    case E:  return "E";
    case F:  return "F";
    case G:  return "G";
    case Ad: return "A#";
    case Bd: return "B#";
    case Cd: return "C#";
    case Dd: return "D#";
    case Ed: return "E#";
    case Fd: return "F#";
    case Gd: return "G#";
    case Ab: return "Ab";
    case Bb: return "Bb";
    case Cb: return "Cb";
    case Db: return "Db";
    case Eb: return "Eb";
    case Fb: return "Fb";
    case Gb: return "Gb";
    }
    return "";
}

constexpr int index(tonality_t tonality) {
    switch (tonality) {
    case C:  case Bd: return 0;
    case Cd: case Db: return 1;
    case D:           return 2;
    case Dd: case Eb: return 3;
    case Fb: case E:  return 4;
    case F:  case Ed: return 5;
    case Fd: case Gb: return 6;
    case G:           return 7;
    case Gd: case Ab: return 8;
    case A:           return 9;
    case Ad: case Bb: return 10;
    case B:  case Cb: return 11;
    }
    return 0;
}

constexpr bool is_black(tonality_t tonality) {
    switch (tonality) {
    case A:
    case B: case Cb:
    case C: case Bd:
    case D:
    case E: case Fb:
    case F: case Ed:
    case G:
        return false;
    }
    return true;
}

constexpr int16_t merge(char base, alteration_t alteration) {
    return (uint16_t)base << 8 | (uint8_t)alteration;
}

constexpr tonality_t from_base(char base, alteration_t alteration) {
    switch (merge(base, alteration)) {
    case merge('A', alteration_t::natural): return A;
    case merge('B', alteration_t::natural): return B;
    case merge('C', alteration_t::natural): return C;
    case merge('D', alteration_t::natural): return D;
    case merge('E', alteration_t::natural): return E;
    case merge('F', alteration_t::natural): return F;
    case merge('G', alteration_t::natural): return G;
    case merge('A', alteration_t::sharp): return Ad;
    case merge('B', alteration_t::sharp): return Bd;
    case merge('C', alteration_t::sharp): return Cd;
    case merge('D', alteration_t::sharp): return Dd;
    case merge('E', alteration_t::sharp): return Ed;
    case merge('F', alteration_t::sharp): return Fd;
    case merge('G', alteration_t::sharp): return Gd;
    case merge('A', alteration_t::flat): return Ab;
    case merge('B', alteration_t::flat): return Bb;
    case merge('C', alteration_t::flat): return Cb;
    case merge('D', alteration_t::flat): return Db;
    case merge('E', alteration_t::flat): return Eb;
    case merge('F', alteration_t::flat): return Fb;
    case merge('G', alteration_t::flat): return Gb;
    }
    return tonality_t::no_tonality;
}

}

std::istream& operator>>(std::istream& stream, tonality_t& tonality) {
    alteration_t alteration = alteration_t::natural;
    char c1 = stream.get();
    char c2 = stream.peek();
    if (c2 == '#') {
        stream.get();
        alteration = alteration_t::sharp;
    } else if (c2 == 'b') {
        stream.get();
        alteration = alteration_t::flat;
    }
    tonality = tonality_ns::from_base(c1, alteration);
    if (tonality == tonality_t::no_tonality)
        throw std::runtime_error("undefined tonality");
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const tonality_t& tonality) {
    if (tonality == tonality_t::no_tonality)
        throw std::runtime_error("undefined tonality");
    return stream << tonality_ns::to_string(tonality);
}

//======
// Note
//======

const Note::tuning_type Note::tuning_reference(note_ns::A(4), 440.);

Note Note::from_code(int code) {
    int diff = code - code_offset;
    int octave = safe_div(diff, 12);
    return {tonality_ns::tonalities[diff - 12*octave], octave};
}

Note Note::from_frequency(double frequency, const tuning_type& tuning) {
    double diff = 12. * log2(frequency / tuning.second);
    return from_code(decay_value<int>(diff) + tuning.first.code());
}

Note Note::from_string(const std::string& string) {
    Note note;
    try {
        std::istringstream stream(string);
        stream >> note;
        if (!stream || stream.tellg() != (std::streampos)-1)
            throw std::runtime_error("string is not entirely consumed");
    } catch (const std::exception&) {
        note.m_tonality = tonality_t::no_tonality;
    }
    return note;
}

Note::Note(tonality_t tonality, int octave) : m_tonality(tonality), m_octave(octave) {

}

bool Note::is_black() const {
    return tonality_ns::is_black(m_tonality);
}

alteration_t Note::alteration() const {
    return tonality_ns::alteration(m_tonality);
}

int Note::code() const {
    return *this ? 12*m_octave + tonality_ns::index(m_tonality) + code_offset : 0;
}

double Note::frequency(const tuning_type& tuning) const {
    return tuning.second * exp2((code() - tuning.first.code())/12.);
}

std::string Note::string() const {
    std::string result;
    if (*this) {
        result += tonality_ns::to_string(m_tonality);
        result += std::to_string(m_octave);
    }
    return result;
}

bool operator==(const Note& lhs, const Note& rhs) {
    return lhs.m_tonality == rhs.m_tonality && lhs.m_octave == rhs.m_octave;
}

bool operator!=(const Note& lhs, const Note& rhs) {
    return !(lhs == rhs);
}

Note::operator bool() const {
    return m_tonality != tonality_t::no_tonality;
}

std::istream& operator>>(std::istream& stream, Note& note) {
    stream >> note.m_tonality >> note.m_octave;
    if (!stream)
        throw std::runtime_error("undefined note");
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Note& note) {
    if (!note)
        throw std::runtime_error("undefined note");
    return stream << note.m_tonality << note.m_octave;
}
