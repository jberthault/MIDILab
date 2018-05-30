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

#ifndef QHANDLERS_HARMONICA_H
#define QHANDLERS_HARMONICA_H

#include <QAbstractButton>
#include "qhandlers/common.h"

//===============
// MetaHarmonica
//===============

class MetaHarmonica : public MetaInstrument {

public:
    explicit MetaHarmonica(QObject* parent);

    void setContent(HandlerProxy& proxy) override;

};

//===========
// Harmonica
//===========

/**
 * representation of a diatonic harmonica
 * @todo finish implementation
 * * ergonomy (resize, shape, ...)
 * * note sliding (like piano)
 * * configuration
 * * channels & buttons setting (like piano)
 *
 */

class Harmonica : public Instrument {

    Q_OBJECT

public:
    explicit Harmonica();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    void setTonality(const Note& note); /*!< richter tuning */

protected slots:
    void receiveNotesOff(channels_t channels) final;
    void receiveNoteOn(channels_t channels, const Note& note) final;
    void receiveNoteOff(channels_t channels, const Note& note) final;

    void onPress(QAbstractButton* button);
    void onRelease(QAbstractButton* button);

private:
    void build(int row, int col);
    void addElement(QWidget* widget, int true_row, int true_col);

    int getTrueRow(int row) const;
    int getTrueCol(int col) const;

    Note buttonNote(int row, int col);
    Note buttonNote(QAbstractButton* button);

    using Index = std::pair<int, int>;

    static const QMap<Index, int> defaultTuning;

    Note mTonality;

    QButtonGroup* mGroup;
    Index mOffset; // position in grid where harmonica starts (row, col)
    bool mReversed; // if true aspirated note are shown over blown

    QMap<Index, int> mTuning; /*!< harmonica tuning (offsets from tonality) */
    QMap<Index, QAbstractButton*> mButtons; /*!< buttons storage */
    std::multimap<int, QAbstractButton*> mForwardNotes; /*!< buttons associated to note values */
    QMap<QAbstractButton*, Note> mButtonsNotes; /*!< notes associated to buttons */

};

#endif // QHANDLERS_HARMONICA_H
