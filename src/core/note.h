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

//==========
// Tonality
//==========

enum class Alteration : int {
    natural = 0,
    sharp = 1,
    flat = -1
};

class Tonality {

public:
    static const Tonality
    no_tonality, // special tonality
    A, B, C, D, E, F, G, // natural notes
    Ad, Bd, Cd, Dd, Ed, Fd, Gd, // sharped notes
    Ab, Bb, Cb, Db, Eb, Fb, Gb; // flat notes

    /**
     * returns a tonality from the given value (in range [0, 12))
     * respectively A, A#/Bb, B, C, ..., G#/Ab
     * sharp notation is preferred to flat for black tonalities
     * if value is not in the given range, returns no_tonality
     */

    static Tonality from_value(int value);
    static Tonality from_base(char base, Alteration alteration); /*!< base describes tone (in range ['A'.. G']) */
    static Tonality from_string(const std::string& string); /*!< string must match [A-G][#b]? */

    Tonality(); /*!< no_tonality */

    int value() const; /*!< @see from_value (no_tonality yield 0) */
    bool is_black() const; /*!< returns true if altered (aka equivalent to any of A#, C#, D#, F# or G#) */
    char base() const; /*!< @see from_base (no_tonality yield \0) */
    Alteration alteration() const; /*!< @see from_base (no_tonality yield Alteration::natural) */
    std::string string() const; /*!< @see from_string (no_tonality yields an empty string) */

    bool operator ==(const Tonality& tonality) const; /*!< true if tonalities equals @note A# & Bb does not equal ! */
    bool operator !=(const Tonality& tonality) const; /*!< @see operator == */

    explicit operator bool() const; /*!< return false if no_tonality, true otherwise */

    friend std::istream& operator >>(std::istream& stream, Tonality& tonality);
    friend std::ostream& operator <<(std::ostream& stream, const Tonality& tonality);

private:
    Tonality(char id);

    char m_id; /*!< private id */

};

//======
// Note
//======

class Note {

public:
    using tuning_type = std::pair<Note, double>;
    static const tuning_type tuning_reference; /*!< tuning reference (A3, 440 Hz) */

    static const int offset = 21; /*!< midi code of A0 */

    static Note from_code(int code); /*!< code stands for the extended midi number */
    static Note from_frequency(double frequency, const tuning_type& tuning = tuning_reference); /*!< will get the note to the nearest frequency */
    static Note from_string(const std::string& string); /*!< string must match [A-G][#b]?(-?[0-9]+) otherwise an invalid note is returned */

    Note(Tonality tonality = {}, int octave = 0);

    int code() const; /*!< value correspond to the midi note: 12*octave + tonality.value() + offset, 0 if undefined */
    double frequency(const tuning_type& tuning = tuning_reference) const;
    std::string string() const; /*!< @see from_string (undefined note yields an empty string) */

    bool operator ==(const Note& note) const; /*!< true if tonalities & octave equals */
    bool operator !=(const Note& note) const; /*!< @see operator == */

    explicit operator bool() const; /*!< return true if tone is well formed */

    friend std::istream& operator >>(std::istream& stream, Note& note);
    friend std::ostream& operator <<(std::ostream& stream, const Note& note);

    Tonality tonality; /*!< tonality of the note */
    int octave; /*!< octave can be any whole number */

};

//class Scale {

//};

#endif // CORE_NOTE_H
