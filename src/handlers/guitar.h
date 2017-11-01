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

#ifndef HANDLERS_GUITAR_H
#define HANDLERS_GUITAR_H

#include "qcore/core.h"

//// @todo implement

//============
// MetaGuitar
//============

class MetaGuitar : public MetaInstrument {

public:
    explicit MetaGuitar(QObject* parent);

    Instance instantiate() override;

};

//========
// Guitar
//========

class Guitar : public Instrument {

    Q_OBJECT

    using ChannelAffectation = std::map<int, int>; /*!< Note : Guitar String */

public:
    static const QList<Note> guitarTuning; /*!< E3 A4 D4 G4 B5 E5 */
    static const QList<Note> bassTuning; /*!< E3 A4 D4 G4 */
    // static const QList<Note> ukulele_tuning; /*!< G C E A */

    explicit Guitar();

    bool setTuning(const QList<Note>& tuning); /*!< default tuning is empty */

    size_t size() const; /*!< default size is 0 */
    void setSize(size_t size); /*!< size must go from 0 to 30 */

protected slots:
    void onNotesOff(channels_t channels) override;
    void setNote(channels_t channels, const Note& note, bool on) override;
    void setSingle(channel_t channel, const Note& note, bool on);

private:
    channel_map_t<ChannelAffectation> mAffectations;
    size_t mSize;
    QList<Note> mTuning;

};

#endif // HANDLERS_GUITAR_H
