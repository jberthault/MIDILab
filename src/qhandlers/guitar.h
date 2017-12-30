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

#ifndef QHANDLERS_GUITAR_H
#define QHANDLERS_GUITAR_H

#include "qcore/core.h"

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

/**
 * @todo ...
 * - put eventFilter in common with piano
 * - enhance the binding algorithm (rebind active notes, prefer neighbors, ...)
 */

class Guitar : public Instrument {

    Q_OBJECT

public:
    using Tuning = std::vector<Note>;
    using StringState = std::array<channels_t, 25>;
    using State = std::vector<StringState>;
    using Location = std::pair<int, int>; /*!< string & fret */

    explicit Guitar();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    const Tuning& tuning() const;
    void setTuning(const Tuning& tuning); /*!< default tuning is E A D G B E */

    size_t capo() const;
    void setCapo(size_t capo);

    QSize sizeHint() const override;

protected slots:
    void receiveNotesOff(channels_t channels) final;
    void receiveNoteOn(channels_t channels, const Note& note) final;
    void receiveNoteOff(channels_t channels, const Note& note) final;

    bool event(QEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void clearNotes();
    void generateFretOn(const Location& loc, Qt::MouseButtons buttons);
    void generateFretOff(const Location& loc, Qt::MouseButtons buttons);

    void activate(const Location& loc, channels_t channels);
    void deactivate(const Location& loc, channels_t channels);

    bool isValid(const Location& loc) const;
    Note toNote(const Location& loc) const;
    Location fromNote(int string, const Note& note) const;
    Location locationAt(const QPoint& point) const;

private:
    Tuning mTuning;
    size_t mCapo;
    State mState;
    Location mActiveLocation;
    QPixmap mBackground;

};

#endif // QHANDLERS_GUITAR_H
