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

using namespace handler_ns;

//==========
// PianoKey
//==========

PianoKey::PianoKey(const Note& note, Piano* parent) :
    QWidget(parent), mNote(note), mChannels(), mParent(parent) {

    setToolTip(QString::fromStdString(note.string()));
}

void PianoKey::setState(channels_t channels, bool on) {
    auto previousChannels = mChannels;
    mChannels.commute(channels, on);
    if (mChannels != previousChannels)
        update();
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
        return nullptr;
    }
    return nullptr;
}

QLayoutItem* PianoLayout::takeAt(int index) {
    if (index >= 0) {
        if (index < mWhite.size())
            return mWhite.takeAt(index);
        if (index < mWhite.size() + mBlack.size())
            return mBlack.takeAt(index-mWhite.size()).first;
        return nullptr;
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

MetaHandler::instance_type MetaPiano::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new Piano(name, parent), nullptr);
}

//=======
// Piano
//=======

Piano::Piano(const QString& name, QWidget* parent) :
    Instrument(io_mode, name, parent), mLastKey(nullptr) {

    installEventFilter(this);
    setRange(qMakePair(Note(Tonality::A, 0), Note(Tonality::C, 7)));
}

QMap<QString, QString> Piano::getParameters() const {
    auto result = Instrument::getParameters();
    result["range"] = serial::serializeRange(mRange);
    return result;
}

size_t Piano::setParameter(const QString& key, const QString& value) {
    if (key == "range")
        UNSERIALIZE(setRange, serial::parseRange, value);
    return Instrument::setParameter(key, value);
}

const QPair<Note, Note>& Piano::range() const {
    return mRange;
}

void Piano::setRange(const QPair<Note, Note>& range) {
    mRange = range;
    buildKeys(range.first, range.second);
}

void Piano::buildKeys(const Note& lower, const Note& upper) {
    // clear previous keys
    for (PianoKey* key : mKeys.values())
        key->deleteLater();
    mKeys.clear();
    // create new layout
    PianoLayout* pianoLayout = new PianoLayout(nullptr);
    for (int code = lower.code() ; code <= upper.code() ; code++) {
        PianoKey* key = new PianoKey(Note::from_code(code), this);
        mKeys[code] = key;
        connect(key, &PianoKey::entered, this, &Piano::enterEvent);
        pianoLayout->addKey(key);
    }
    // delete previous layout and set the new one
    delete layout();
    setLayout(pianoLayout);
}

void Piano::onNotesOff(channels_t channels) {
    for (PianoKey* key : mKeys.values())
        key->setState(channels, false);
}

void Piano::setNote(channels_t channels, const Note& note, bool on) {
    PianoKey* key = mKeys.value(note.code(), nullptr);
    if (key != nullptr)
        key->setState(channels, on);
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
    case QEvent::MouseButtonDblClick: receiveKeys(true, key, mouseEvent->button()); break;
    case QEvent::MouseButtonPress: receiveKeys(true, key, mouseEvent->button()); break;
    case QEvent::MouseButtonRelease: receiveKeys(false, key, mouseEvent->button()); break;
    case QEvent::MouseMove:
        if (mLastKey != key && mouseEvent->buttons() != Qt::NoButton) {
            receiveKeys(false, mLastKey, mouseEvent->buttons());
            receiveKeys(true, key, mouseEvent->buttons());
        }
    default: break; // should never happen
    }

    mLastKey = key;
    return true;
}

void Piano::receiveKeys(bool on, PianoKey* key, Qt::MouseButtons buttons) {
    ChannelEditor* editor = channelEditor();
    channels_t channels = editor ? editor->channelsFromButtons(buttons) : channels_t::merge(0);
    if (key != nullptr && channels) {
        Event event = on ? Event::note_on(channels, key->note().code(), velocity()) :
                           Event::note_off(channels, key->note().code());
        generate(std::move(event));
        key->setState(channels, on);
    }
}
