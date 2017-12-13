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

#include "piano.h"
#include "qcore/editors.h"

//==========
// PianoKey
//==========

PianoKey::PianoKey(const Note& note, Piano* parent) :
    QWidget(parent), mNote(note), mChannels(), mParent(parent) {

    setToolTip(QString::fromStdString(note.string()));
}

void PianoKey::setChannels(channels_t channels) {
    if (mChannels != channels) {
        mChannels = channels;
        update();
    }
}

void PianoKey::activate(channels_t channels) {
    setChannels(mChannels | channels);
}

void PianoKey::deactivate(channels_t channels) {
    setChannels(mChannels & ~channels);
}

const Note& PianoKey::note() const {
    return mNote;
}

bool PianoKey::isBlack() const {
    return mNote.tonality.is_black();
}

void PianoKey::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setPen(QColor("#444")); // border color
    painter.setRenderHint(QPainter::Antialiasing); // rendering
    // coloration
    ChannelEditor* editor = mParent->channelEditor();
    if (mChannels && editor)
        painter.setBrush(editor->brush(mChannels));
    else
        painter.setBrush(QBrush(isBlack() ? Qt::black : Qt::white));
    // border radius
    painter.drawRoundedRect(rect(), 50, 5, Qt::RelativeSize);
}

void PianoKey::enterEvent(QEvent* event) {
    emit entered(event);
}

//=============
// PianoLayout
//=============

PianoLayout::PianoLayout(QWidget* parent) : QLayout(parent), mFirstBlack(false), mLastBlack(false) {

}

PianoLayout::~PianoLayout() {
    while (QLayoutItem* item = takeAt(0))
        delete item;
}

void PianoLayout::addKey(PianoKey* key) {
    mLastBlack = key->isBlack();
    if (mLastBlack) {
        key->raise();
        mBlack.append(BlackItem(new QWidgetItem(key), mWhite.size()));
        if (mWhite.isEmpty())
            mFirstBlack = true;
    } else {
        key->lower();
        mWhite.append(new QWidgetItem(key));
    }
}

void PianoLayout::addItem(QLayoutItem*) {
    qDebug() << "Can't add item for this layout";
}

Qt::Orientations PianoLayout::expandingDirections() const {
    return Qt::Horizontal | Qt::Vertical;
}

QLayoutItem* PianoLayout::itemAt(int index) const {
    if (index >= 0) {
        if (index < mWhite.size())
            return mWhite.at(index);
        if (index < mWhite.size() + mBlack.size())
            return mBlack.at(index - mWhite.size()).first;
    }
    return nullptr;
}

QLayoutItem* PianoLayout::takeAt(int index) {
    if (index >= 0) {
        if (index < mWhite.size())
            return mWhite.takeAt(index);
        if (index < mWhite.size() + mBlack.size())
            return mBlack.takeAt(index-mWhite.size()).first;
    }
    return nullptr;
}

int PianoLayout::count() const {
    return mWhite.size() + mBlack.size();
}

void PianoLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);

    double blackBounds = 0;
    if (mFirstBlack)
        blackBounds += .5;
    if (mLastBlack)
        blackBounds += .5;

    double count = mWhite.size() + blackBounds;
    if (count <= 0.5)
        return;

    // compute size
    int whiteWidth = (int)(rect.width() / count);
    int whiteHeight = qMin(rect.height(), int(PianoKey::whiteRatio * whiteWidth));
    int blackWidth = int(PianoKey::blackWidthRatio * whiteWidth);
    int blackHeight = int(PianoKey::blackHeightRatio * whiteHeight);
    // compute offset
    double totalWidth = mWhite.size() * whiteWidth + blackBounds;
    QPoint whiteOffset = rect.topLeft() + QPoint((rect.width() - (int)totalWidth)/2, (rect.height() - whiteHeight)/2);
    // update white keys position
    QRect whiteRect(whiteOffset, QSize(whiteWidth, whiteHeight));
    for (QLayoutItem* item : mWhite) {
        item->setGeometry(whiteRect);
        whiteRect.moveLeft(whiteRect.left() + whiteWidth);
    }
    // update black keys position
    QRect blackRect(whiteOffset, QSize(blackWidth, blackHeight));
    for (const BlackItem& pair : mBlack) {
        blackRect.moveLeft(whiteOffset.x() + whiteWidth * pair.second - blackWidth / 2);
        pair.first->setGeometry(blackRect);
    }
}

QSize PianoLayout::sizeHint() const {
    return {600, 100};
}

//===========
// MetaPiano
//===========

MetaPiano::MetaPiano(QObject* parent) : MetaInstrument(parent) {
    setIdentifier("Piano");
    setDescription("Interactive Piano Keyboard");
    addParameter("range", ":NoteRange", "closed range \"<first_note>:<last_note>\" of notes composing the keyboard", "A0:C7");
    // addParameter("sound_off", ":bool", "", "");
}

MetaHandler::Instance MetaPiano::instantiate() {
    return Instance(new Piano, nullptr);
}

//=======
// Piano
//=======

Piano::Piano() : Instrument(handler_ns::io_mode), mLastKey(nullptr), mRange(qMakePair(Note(Tonality::A, 0), Note(Tonality::C, 7))) {
    mKeys.fill(nullptr);
    installEventFilter(this);
    buildKeys();
}

HandlerView::Parameters Piano::getParameters() const {
    auto result = Instrument::getParameters();
    SERIALIZE("range", serial::serializeRange, mRange, result);
    return result;
}

size_t Piano::setParameter(const Parameter& parameter) {
    UNSERIALIZE("range", serial::parseRange, setRange, parameter);
    return Instrument::setParameter(parameter);
}

const QPair<Note, Note>& Piano::range() const {
    return mRange;
}

void Piano::setRange(const QPair<Note, Note>& range) {
    if (range != mRange && 0 <= range.first.code() && range.second.code() < 0x80) {
        mRange = range;
        clearKeys();
        buildKeys();
    }
}

void Piano::clearKeys() {
    for (PianoKey* key : mKeys)
        if (key != nullptr)
            key->deleteLater();
    mKeys.fill(nullptr);
    delete layout();
}

void Piano::buildKeys() {
    auto pianoLayout = new PianoLayout(nullptr);
    for (int code = mRange.first.code() ; code <= mRange.second.code() ; code++) {
        auto key = new PianoKey(Note::from_code(code), this);
        mKeys[code] = key;
        connect(key, &PianoKey::entered, this, &Piano::enterEvent);
        pianoLayout->addKey(key);
    }
    setLayout(pianoLayout);
}

void Piano::receiveNotesOff(channels_t channels) {
    for (PianoKey* key : mKeys)
        if (key != nullptr)
            key->deactivate(channels);
}

void Piano::receiveNoteOn(channels_t channels, const Note& note) {
    PianoKey* key = mKeys[note.code()];
    if (key != nullptr)
        key->activate(channels);
}

void Piano::receiveNoteOff(channels_t channels, const Note& note) {
    PianoKey* key = mKeys[note.code()];
    if (key != nullptr)
        key->deactivate(channels);
}

void Piano::enterEvent(QEvent*) {
    if (canGenerate()) {
        grabMouse();
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }
}

bool Piano::eventFilter(QObject* obj, QEvent* event) {

    Q_ASSERT(obj == this);

    /// handle tooltip event
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        PianoKey* key = qobject_cast<PianoKey*>(childAt(helpEvent->pos()));
        if (key) {
            QToolTip::showText(helpEvent->globalPos(), key->toolTip());
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }

    //QEvent::EnabledChange

    /// try to get mouse events
    QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if (mouseEvent == nullptr)
        return Instrument::eventFilter(obj, event);

    PianoKey* key = qobject_cast<PianoKey*>(childAt(mouseEvent->pos()));

    /// stop tracking on leave
    if (key == nullptr && hasMouseTracking() && mouseEvent->buttons() == Qt::NoButton) {
        releaseMouse();
        setMouseTracking(false);
        unsetCursor();
    }

    /// don't process events if closed or disable
    if (!canGenerate())
        return true;

    /// check mouse event type
    switch (mouseEvent->type()) {
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
        generateKeyOn(key, mouseEvent->button());
        break;
    case QEvent::MouseButtonRelease:
        generateKeyOff(key, mouseEvent->button());
        break;
    case QEvent::MouseMove:
        if (mLastKey != key && mouseEvent->buttons() != Qt::NoButton) {
            generateKeyOff(mLastKey, mouseEvent->buttons());
            generateKeyOn(key, mouseEvent->buttons());
        }
        break;
    }

    mLastKey = key;
    return true;
}

void Piano::generateKeyOn(PianoKey* key, Qt::MouseButtons buttons) {
    ChannelEditor* editor = channelEditor();
    channels_t channels = editor ? editor->channelsFromButtons(buttons) : channels_t::merge(0);
    if (key != nullptr && channels) {
        generateNoteOn(channels, key->note());
        key->activate(channels);
    }
}

void Piano::generateKeyOff(PianoKey* key, Qt::MouseButtons buttons) {
    ChannelEditor* editor = channelEditor();
    channels_t channels = editor ? editor->channelsFromButtons(buttons) : channels_t::merge(0);
    if (key != nullptr && channels) {
        generateNoteOff(channels, key->note());
        key->deactivate(channels);
    }
}
