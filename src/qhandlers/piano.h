/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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

#ifndef QHANDLERS_PIANO_H
#define QHANDLERS_PIANO_H

#include "qhandlers/common.h"

//==========
// PianoKey
//==========

/**
 * @todo let the ratio be configurable in the piano
 */

class Piano;

class PianoKey : public QWidget {

    Q_OBJECT

public:
    explicit PianoKey(const Note& note, Piano* parent);

    void setChannels(channels_t channels);
    void activate(channels_t channels);
    void deactivate(channels_t channels);

    const Note& note() const;
    bool isBlack() const;

    void paintEvent(QPaintEvent* event) override;

private:
    const Note mNote;
    channels_t mChannels; /*!< channels currently active */
    Piano* mParent; /*!< keeps a reference to the parent piano to get channel editor */

};

//=============
// PianoLayout
//=============

class PianoLayout : public QLayout {

    Q_OBJECT

    using BlackItem = std::pair<QLayoutItem*, unsigned>;

public:
    using QLayout::QLayout;
    ~PianoLayout();

    void addKey(PianoKey* key);

    void addItem(QLayoutItem* item) override;
    Qt::Orientations expandingDirections() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    int count() const override;
    void setGeometry(const QRect& rect) override;

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

private:
    QList<BlackItem> mBlack;
    QList<QLayoutItem*> mWhite;
    bool mFirstBlack {false};
    bool mLastBlack {false};

};

//=======
// Piano
//=======

MetaHandler* makeMetaPiano(QObject* parent);

/**
 * @todo features enchancement : freeze, snapshot, step by step, filtering, pulse ...
 */

class Piano : public Instrument {

    Q_OBJECT

public:
    explicit Piano();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    const range_t<Note>& range() const;
    void setRange(const range_t<Note>& range);

protected:
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

private:
    void clearKeys();
    void buildKeys();
    void generateKeyOn(PianoKey* key, Qt::MouseButtons buttons);
    void generateKeyOff(PianoKey* key, Qt::MouseButtons buttons);

    PianoKey* mActiveKey {nullptr};
    range_t<Note> mRange {note_ns::A(0), note_ns::C(8)};
    std::array<PianoKey*, 0x80> mKeys;

};

#endif // QHANDLERS_PIANO_H
