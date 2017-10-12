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
#include "note.h"
#include "tools/bytes.h"

//==========
// Tonality
//==========

const Tonality Tonality::no_tonality;
const Tonality Tonality::A('A');
const Tonality Tonality::B('B');
const Tonality Tonality::C('C');
const Tonality Tonality::D('D');
const Tonality Tonality::E('E');
const Tonality Tonality::F('F');
const Tonality Tonality::G('G');
const Tonality Tonality::Ad('H');
const Tonality Tonality::Bd('I');
const Tonality Tonality::Cd('J');
const Tonality Tonality::Dd('K');
const Tonality Tonality::Ed('L');
const Tonality Tonality::Fd('M');
const Tonality Tonality::Gd('N');
const Tonality Tonality::Ab('O');
const Tonality Tonality::Bb('P');
const Tonality Tonality::Cb('Q');
const Tonality Tonality::Db('R');
const Tonality Tonality::Eb('S');
const Tonality Tonality::Fb('T');
const Tonality Tonality::Gb('U');

Tonality Tonality::from_value(int value) {
    switch (value) {
    case 0: return A;
    case 1: return Ad;
    case 2: return B;
    case 3: return C;
    case 4: return Cd;
    case 5: return D;
    case 6: return Dd;
    case 7: return E;
    case 8: return F;
    case 9: return Fd;
    case 10: return G;
    case 11: return Gd;
    default: return no_tonality;
    }
}

Tonality Tonality::from_base(char base, Alteration alteration) {
    if (!('A' <= base && base <= 'G'))
        return no_tonality;
    switch (alteration) {
    case Alteration::sharp: return Tonality(base + 7);
    case Alteration::flat: return Tonality(base + 14);
    default /*natural*/ : return Tonality(base);
    }
}

Tonality Tonality::from_string(const std::string& string) {
    std::string::const_iterator it(string.begin()), end(string.end());
    if (it == end)
        return Tonality::no_tonality;
    char base = *it++;
    Alteration alteration = Alteration::natural;
    if (it != end) {
        if (*it == '#')
            alteration = Alteration::sharp;
        else if (*it == 'b')
            alteration = Alteration::flat;
    }
    return Tonality::from_base(base, alteration);
}

Tonality::Tonality() : m_id('\0') {

}

Tonality::Tonality(char id) : m_id(id) {

}

int Tonality::value() const {
    switch (m_id) {
    case 'A': return 0;
    case 'B': return 2;
    case 'C': return 3;
    case 'D': return 5;
    case 'E': return 7;
    case 'F': return 8;
    case 'G': return 10;
    case 'H': return 1;
    case 'I': return 3;
    case 'J': return 4;
    case 'K': return 6;
    case 'L': return 8;
    case 'M': return 9;
    case 'N': return 11;
    case 'O': return 11;
    case 'P': return 1;
    case 'Q': return 2;
    case 'R': return 4;
    case 'S': return 6;
    case 'T': return 7;
    case 'U': return 9;
    default: return 0;
    }
}

bool Tonality::is_black() const {
    switch (value()) {
    case 1:
    case 4:
    case 6:
    case 9:
    case 11:
        return true;
    default:
        return false;
    }
}

char Tonality::base() const {
    if ('H' <= m_id && m_id <= 'N')
        return m_id - 7;
    if ('O' <= m_id && m_id <= 'U')
        return m_id - 14;
    return m_id;
}

Alteration Tonality::alteration() const {
    if ('H' <= m_id && m_id <= 'N')
        return Alteration::sharp;
    if ('O' <= m_id && m_id <= 'U')
        return Alteration::flat;
    return Alteration::natural;
}

std::string Tonality::string() const {
    std::string result;
    if (*this) {
        result.push_back(base());
        switch (alteration()) {
        case Alteration::sharp: result.push_back('#'); break;
        case Alteration::flat: result.push_back('b'); break;
        default: break;
        }
    }
    return result;
}

bool Tonality::operator ==(const Tonality& tonality) const {
    return m_id == tonality.m_id;
}

bool Tonality::operator !=(const Tonality& tonality) const {
    return m_id != tonality.m_id;
}

Tonality::operator bool() const {
    return m_id != '\0';
}

std::istream& operator >>(std::istream& stream, Tonality& tonality) {
    Alteration alteration = Alteration::natural;
    char c1 = stream.get();
    char c2 = stream.peek();
    if (c2 == '#') {
        stream.get();
        alteration = Alteration::sharp;
    } else if (c2 == 'b') {
        stream.get();
        alteration = Alteration::flat;
    }
    tonality = Tonality::from_base(c1, alteration);
    if (!tonality)
        throw std::runtime_error("undefined tonality");
    return stream;
}

std::ostream& operator <<(std::ostream& stream, const Tonality& tonality) {
    if (!tonality)
        throw std::runtime_error("undefined tonality");
    stream << tonality.base();
    switch (tonality.alteration()) {
    case Alteration::sharp: stream << '#'; break;
    case Alteration::flat: stream << 'b'; break;
    default: break;
    }
    return stream;
}

//======
// Note
//======

const Note::tuning_type Note::tuning_reference(Note(Tonality::A, 3), 440.);

Note Note::from_code(int code) {
    int diff = code - offset;
    int octave = (diff >= 0 ? diff : (diff-12+1)) / 12; // true floor
    Tonality tonality = Tonality::from_value(diff - 12*octave); // true modulo
    return {tonality, octave};
}

Note Note::from_frequency(double frequency, const tuning_type& tuning) {
    double diff = 12. * log2(frequency / tuning.second);
    return Note::from_code(decay_value<int>(diff) + tuning.first.code());
}

Note Note::from_string(const std::string& string) {
    Note note;
    try {
        std::istringstream stream(string);
        stream >> note;
        if (!stream || stream.tellg() != (std::streampos)-1)
            throw std::runtime_error("string is not entirely consumed");
    } catch (const std::exception&) {
        note.tonality = Tonality::no_tonality;
    }
    return note;
}

Note::Note(Tonality tonality, int octave) : tonality(tonality), octave(octave) {

}

int Note::code() const {
    return *this ? 12*octave + tonality.value() + offset : 0;
}

double Note::frequency(const tuning_type& tuning) const {
    return tuning.second * exp2((code() - tuning.first.code())/12.);
}

std::string Note::string() const {
    std::string result;
    if (*this) {
        result += tonality.string();
        result += std::to_string(octave);
    }
    return result;
}

bool Note::operator ==(const Note& note) const {
    return tonality == note.tonality && octave == note.octave;
}

bool Note::operator !=(const Note& note) const {
    return tonality != note.tonality || octave != note.octave;
}

Note::operator bool() const {
    return bool(tonality);
}

std::istream& operator >>(std::istream& stream, Note& note) {
    stream >> note.tonality >> note.octave;
    if (!stream)
        throw std::runtime_error("undefined note");
    return stream;
}

std::ostream& operator <<(std::ostream& stream, const Note& note) {
    if (!note)
        throw std::runtime_error("undefined note");
    return stream << note.tonality << note.octave;
}
