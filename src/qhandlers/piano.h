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

#ifndef QHANDLERS_PIANO_H
#define QHANDLERS_PIANO_H

#include <QWidget>
#include <QEvent>
#include <QPaintEvent>
#include <QLayout>
#include <QLayoutItem>
#include <QRect>
#include <QSize>
#include <QList>
#include <QPair>
#include <QDebug>
#include <QPainter>
#include <QToolTip>
#include "qcore/core.h"

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
    static constexpr double whiteRatio = 7.; /*!< ratio of the height of a white key by its width */
    static constexpr double blackWidthRatio = .7; /*!< ratio of the black width by the white width */
    static constexpr double blackHeightRatio = .6; /*!< ratio of the black height by the white height */

    explicit PianoKey(const Note& note, Piano* parent);

    void setChannels(channels_t channels);
    void activate(channels_t channels);
    void deactivate(channels_t channels);

    const Note& note() const;
    bool isBlack() const;

    void enterEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void entered(QEvent*);

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

    using BlackItem = QPair<QLayoutItem*, unsigned>;

public:
    explicit PianoLayout(QWidget* parent);
    ~PianoLayout();

    void addKey(PianoKey* key);

    void addItem(QLayoutItem* item) override;
    Qt::Orientations expandingDirections() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    int count() const override;
    void setGeometry(const QRect& rect) override;

//    bool hasHeightForWidth() const;
//    int heightForWidth(int width) const;
    QSize sizeHint() const override;
//    QSize minimumSize() const;
//    QSize maximumSize() const;

private:
    QList<BlackItem> mBlack;
    QList<QLayoutItem*> mWhite;
    bool mFirstBlack;
    bool mLastBlack;

};

//===========
// MetaPiano
//===========

class MetaPiano : public MetaInstrument {

public:
    explicit MetaPiano(QObject* parent);

    Instance instantiate() override;

};

//=======
// Piano
//=======

/**
 * @todo features enchancement : freeze, snapshot, step by step, filtering, pulse ...
 */

class Piano : public Instrument {

    Q_OBJECT

public:
    explicit Piano();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    const QPair<Note, Note>& range() const;
    void setRange(const QPair<Note, Note>& range);

protected:
    void receiveNotesOff(channels_t channels) final;
    void receiveNoteOn(channels_t channels, const Note& note) final;
    void receiveNoteOff(channels_t channels, const Note& note) final;

    void enterEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void clearKeys();
    void buildKeys();
    void generateKeyOn(PianoKey* key, Qt::MouseButtons buttons);
    void generateKeyOff(PianoKey* key, Qt::MouseButtons buttons);

    PianoKey* mLastKey;
    QPair<Note, Note> mRange;
    std::array<PianoKey*, 0x80> mKeys;

};

#endif // QHANDLERS_PIANO_H
