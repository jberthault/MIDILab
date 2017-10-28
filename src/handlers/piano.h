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

#ifndef HANDLERS_PIANO_H
#define HANDLERS_PIANO_H

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

    void setState(channels_t channels, bool on);

    const Note& note() const;
    bool isBlack() const;

    void enterEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void entered(QEvent*);

private:
    Note mNote;
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
    MetaPiano(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;

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
    explicit Piano(const QString& name, QWidget* parent);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    const QPair<Note, Note>& range() const;
    void setRange(const QPair<Note, Note>& range);

protected:
    void onNotesOff(channels_t channels) override;
    void setNote(channels_t channels, const Note& note, bool on) override;

    void enterEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildKeys(const Note& lower, const Note& upper); /*!< note that bound are included */
    void receiveKeys(bool on, PianoKey* key, Qt::MouseButtons buttons); /*!< change key state and notify the forwarder depending on buttons */

    PianoKey* mLastKey;
    QPair<Note, Note> mRange;
    QMap<int, PianoKey*> mKeys;

};

#endif // HANDLERS_PIANO_H
