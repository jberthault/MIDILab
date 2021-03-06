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

#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include "displayer.h"
#include "misc.h"
#include "tools/trace.h"

//==============
// DragDetector
//==============

bool DragDetector::eventFilter(QObject* obj, QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            mStartPosition = mouseEvent->pos();
            mHasPosition = true;
            return true;
        }
    } break;
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            mHasPosition = false;
            return true;
        }
    } break;
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mHasPosition && farEnough(mouseEvent->pos())) {
            mHasPosition = false;
            emit dragRequest();
            return true;
        }
    } break;
    default: break;
    }
    return QObject::eventFilter(obj, event);
}

bool DragDetector::farEnough(const QPoint& point) {
    return (point - mStartPosition).manhattanLength() >= QApplication::startDragDistance();
}

//==========
// Scroller
//==========

Scroller::Scroller(QWidget* widget, QWidget* parent) :
    QWidget(parent), mWidget(widget), mScrolling(false) {

    // free area
    mArea = new QScrollArea;
    mArea->setWidgetResizable(true);
    mArea->setVisible(false);

    mWidget->setParent(this);
    setLayout(make_vbox(margin_tag{0}, mWidget));
}

Scroller::~Scroller() {
    mArea->deleteLater();
}

bool Scroller::isScrolling() const {
    return mScrolling;
}

QWidget* Scroller::widget() {
    return mWidget;
}

QScrollArea* Scroller::scrollArea() {
    return mArea;
}

void Scroller::setScrolling(bool scrolling) {
    QLayout* wlayout = layout();
    if (scrolling == mScrolling)
        return;
    mScrolling = scrolling;
    if (scrolling) {
        wlayout->removeWidget(mWidget);
        mArea->setParent(this);
        mArea->setWidget(mWidget);
        mArea->setVisible(true);
        wlayout->addWidget(mArea);
    } else {
        wlayout->removeWidget(mArea);
        mArea->takeWidget();
        mArea->setParent(nullptr);
        mArea->setVisible(false);
        mWidget->setParent(this);
        wlayout->addWidget(mWidget);
    }
}

//============
// Receptacle
//===========

Receptacle::Receptacle(Qt::Orientation orientation, QWidget* parent) : QWidget{parent} {
    setAcceptDrops(true);
    // free frame
    mLine = new QFrame;
    mLine->setFrameShadow(QFrame::Sunken);
    mLine->setAcceptDrops(false);
    // layout
    mLayout = new QBoxLayout{QBoxLayout::LeftToRight};
    mLayout->setMargin(0);
    setLayout(mLayout);
    // layout
    setOrientation(orientation);
}

Receptacle::~Receptacle() {
    mLine->deleteLater();
}

const QStringList& Receptacle::mimeTypes() const {
    return mMimeTypes;
}

void Receptacle::setMimeTypes(const QStringList& mimeTypes) {
    mMimeTypes = mimeTypes;
}

Qt::Orientation Receptacle::orientation() const {
    return mOrientation;
}

void Receptacle::setOrientation(Qt::Orientation orientation) {
    Q_ASSERT(mPosition == nullPosition);
    mOrientation = orientation;
    if (mOrientation == Qt::Horizontal) {
        mLine->setFrameShape(QFrame::VLine);
        mLayout->setDirection(QBoxLayout::LeftToRight);
    } else  {
        mLine->setFrameShape(QFrame::HLine);
        mLayout->setDirection(QBoxLayout::TopToBottom);
    }
}

bool Receptacle::insertWidget(QWidget* widget, int position) {
    // avoid inserting a null widget or a widget that contains this
    if (!widget || widget->findChildren<Receptacle*>().contains(this))
        return false;
    // remove the line
    clearPosition();
    // adjust position if the widget is moved within this
    const int idx = mLayout->indexOf(widget);
    if (idx != -1 && idx < position)
        position--;
    // actually insert the widget
    mLayout->insertWidget(position, widget);
    // notify listeners
    emit widgetInserted(widget, position);
    return true;
}

void Receptacle::dropEvent(QDropEvent* de) {
    if (isSupported(de->mimeData()))
        if (insertWidget(dynamic_cast<QWidget*>(de->source()), mPosition))
            de->accept();
}

void Receptacle::dragMoveEvent(QDragMoveEvent* de) {
    updatePosition(de->pos());
    de->accept();
}

void Receptacle::dragEnterEvent(QDragEnterEvent* de) {
    clearPosition();
    if (isSupported(de->mimeData())) {
        updatePosition(de->pos());
        /// @todo highlight
        de->accept();
    }
}

void Receptacle::dragLeaveEvent(QDragLeaveEvent* de) {
    clearPosition();
    de->accept();
}

void Receptacle::clearPosition() {
    moveLine(nullPosition);
}

void Receptacle::updatePosition(const QPoint& cursor) {
    int cursorLocation = mOrientation == Qt::Vertical ? cursor.y() : cursor.x();
    int position = 0;
    int offset = 0;
    for (; position < mLayout->count() ; position++) {
        // get the widget at the given position
        QWidget* item = mLayout->itemAt(position)->widget();
        // skip line
        if (item != mLine) {
            // get its rect (relative to his parent)
            QRect itemRect(item->mapToParent(QPoint()), item->rect().size());
            // get center
            QPoint itemCenter = itemRect.center();
            // get the relevant axis
            int itemLocation  = mOrientation == Qt::Vertical ? itemCenter.y() : itemCenter.x();
            // the item centers are ordered, we can stop here
            if (cursorLocation < itemLocation)
                break;
        } else {
            // the computed position should not contain the line
            offset = -1;
        }
    }
    moveLine(position + offset);
}

void Receptacle::moveLine(int position) {
    // don't do anything if position has not changed
    if (mPosition == position)
        return;
    // remove line if appears
    if (mPosition != nullPosition)
        removeLine();
    // insert line if position is available
    if (position != nullPosition)
        insertLine(position);
}

void Receptacle::removeLine() {
    Q_ASSERT(mPosition != nullPosition);
    mLine->setParent(nullptr);
    mLayout->removeWidget(mLine);
    mLine->setVisible(false);
    mPosition = nullPosition;
}

void Receptacle::insertLine(int position) {
    Q_ASSERT(mPosition == nullPosition);
    mLine->setParent(this);
    mLayout->insertWidget(position, mLine);
    mLine->setVisible(true);
    mPosition = position;
}

bool Receptacle::isSupported(const QMimeData* mimeData) const {
    return std::any_of(mMimeTypes.begin(), mMimeTypes.end(), [=](const auto& mimeType) { return mimeData->hasFormat(mimeType);});
}

//===========
// Displayer
//===========

const QString Displayer::mimeType{"application/x_displayer"};

void Displayer::drag() {
    auto* ancestor = nearestAncestor();
    if (ancestor == nullptr || isLocked())
        return;
    auto* data = new QMimeData;
    data->setData(mimeType, QByteArray{});
    auto* drag = new QDrag{this};
    drag->setPixmap(grab());
    drag->setHotSpot(mapFromGlobal(QCursor::pos()));
    drag->setMimeData(data);
    drag->exec(Qt::MoveAction);
    if (drag->target() == nullptr)
        TRACE_WARNING("displayer has not been dropped correctly");
    else if (ancestor->isEmpty() && ancestor->isIndependent())
        ancestor->deleteLaterRecursive();
}

bool Displayer::isEmpty() const {
    auto displayers = findChildren<SingleDisplayer*>();
    return std::all_of(displayers.begin(), displayers.end(), [](auto* displayer) { return displayer->mDying; });
}

bool Displayer::isEmbedded() const {
    return nearestAncestor() != nullptr;
}

bool Displayer::isIndependent() const {
    return bool(windowFlags() & Qt::Window) || isEmbedded();
}

Displayer* Displayer::nearestAncestor() const {
    if (windowFlags() & Qt::Window)
        return nullptr;
    for (auto* widget = parentWidget() ; widget != nullptr ; widget = widget->parentWidget()) {
        if (auto* displayer = dynamic_cast<Displayer*>(widget))
            return displayer;
        if (widget->windowFlags() & Qt::Window)
            break;
    }
    return nullptr;
}

void Displayer::deleteLaterRecursive() {
    mDying = true;
    auto* ancestor = nearestAncestor();
    if (ancestor && ancestor->isEmpty() && ancestor->isIndependent())
        ancestor->deleteLaterRecursive();
    else
        deleteLater();
}

void Displayer::closeEvent(QCloseEvent* event) {
    // Delete window if it does not contains leaf content
    TRACE_DEBUG("closing displayer ...");
    if (isEmpty())
        setAttribute(Qt::WA_DeleteOnClose);
    QFrame::closeEvent(event);
}

//=================
// SingleDisplayer
//=================

SingleDisplayer::SingleDisplayer(QWidget* parent) : Displayer{parent} {
    mMove = new QToolButton(this);
    mMove->setAutoRaise(true);
    mMove->setIcon(QIcon(":/data/move.svg"));
    mMove->setToolTip("Drag & drop this widget");
    connect(mMove, &QToolButton::pressed, this, &SingleDisplayer::onPress);
    setLayout(make_hbox(margin_tag{0}, spacing_tag{0}, mMove));
}

bool SingleDisplayer::isLocked() const {
    return mMove->isHidden();
}

void SingleDisplayer::setLocked(bool locked) {
    if (mWidget)
        mWidget->setEnabled(locked);
    if (isLocked() != locked) {
        mMove->setHidden(locked);
        emit lockChanged(locked);
    }
}

QWidget* SingleDisplayer::widget() {
    return mWidget;
}

void SingleDisplayer::setWidget(QWidget* widget) {
    Q_ASSERT(!mWidget);
    mWidget = widget;
    mWidget->setParent(this);
    mWidget->setEnabled(isLocked());
    static_cast<QHBoxLayout*>(layout())->insertWidget(0, mWidget);
    // this supposes that the widget will be deleted before this
    connect(mWidget, &QWidget::destroyed, this, &SingleDisplayer::deleteLaterRecursive);
}

void SingleDisplayer::onPress() {
    mMove->setDown(false);
    drag();
}

//================
// MultiDisplayer
//================

std::vector<MultiDisplayer*> MultiDisplayer::topLevelDisplayers() {
    std::vector<MultiDisplayer*> result;
    for (auto* widget : qApp->topLevelWidgets())
        if (auto* displayer = dynamic_cast<MultiDisplayer*>(widget))
            result.push_back(displayer);
    return result;
}

MultiDisplayer::MultiDisplayer(Qt::Orientation orientation, QWidget* parent) : Displayer{parent} {
    // drag
    auto* dragDetector = new DragDetector{this};
    connect(dragDetector, &DragDetector::dragRequest, this, &MultiDisplayer::drag);
    installEventFilter(dragDetector);
    // add actions
    connect(makeAction(QIcon{":/data/plus.svg"}, "Add Container", this), &QAction::triggered, this, &MultiDisplayer::onInsertionRequest);
    connect(makeAction(QIcon{":/data/elevator.svg"}, "Toggle Scrolling", this), &QAction::triggered, this, &MultiDisplayer::toggleScrolling);
    connect(makeAction(QIcon{":/data/fullscreen-exit.svg"}, "Toggle Stretching", this), &QAction::triggered, this, &MultiDisplayer::toggleStretched);
    connect(makeAction(QIcon{":/data/transfer.svg"}, "Flip Orientation", this), &QAction::triggered, this, &MultiDisplayer::toggleOrientation);
    connect(makeAction(QIcon{":/data/text.svg"}, "Edit Title", this), &QAction::triggered, this, &MultiDisplayer::changeTitle);
    connect(makeAction(QIcon{":/data/delete.svg"}, "Delete", this), &QAction::triggered, this, &MultiDisplayer::onDeleteRequest);
    setContextMenuPolicy(Qt::NoContextMenu);
    // receptacle widget
    mReceptacle = new Receptacle{orientation, this};
    mReceptacle->setMimeTypes(QStringList{mimeType});
    mReceptacle->setAcceptDrops(false); /*!< @note locked at creation */
    connect(mReceptacle, &Receptacle::widgetInserted, this, &MultiDisplayer::onWidgetInsertion);
    // stretching and scrolling
    mStretchLayout = make_hbox(margin_tag{0});
    auto* stretchWidget = new QWidget{this};
    stretchWidget->setLayout(mStretchLayout);
    mStretchLayout->addWidget(mReceptacle);
    mScroller = new Scroller{stretchWidget, this};
    setLayout(make_vbox(mScroller));
}

MultiDisplayer::~MultiDisplayer() {
    TRACE_DEBUG("deleting displayer ...");
}

bool MultiDisplayer::isLocked() const {
    return !mReceptacle->acceptDrops();
}

void MultiDisplayer::setLocked(bool locked) {
    // update all nodes
    updateLocked(locked);
    for (auto* displayer : findChildren<MultiDisplayer*>())
        displayer->updateLocked(locked);
    // update all leaves
    for (auto* displayer : findChildren<SingleDisplayer*>())
        displayer->setLocked(locked);
}

bool MultiDisplayer::isStreched() const {
    return mStretched;
}

void MultiDisplayer::setStretched(bool streched) {
    if (streched == mStretched)
        return;
    if (streched) {
        mStretchLayout->insertStretch(0);
        mStretchLayout->addStretch();
    } else {
        delete mStretchLayout->takeAt(2);
        delete mStretchLayout->takeAt(0);
    }
    mStretched = streched;
}

bool MultiDisplayer::isScrolling() const {
    return mScroller->isScrolling();
}

void MultiDisplayer::setScrolling(bool scrolling) {
    mScroller->setScrolling(scrolling);
}

Qt::Orientation MultiDisplayer::orthogonal() const {
    return orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
}

Qt::Orientation MultiDisplayer::orientation() const {
    return mReceptacle->orientation();
}

void MultiDisplayer::setOrientation(Qt::Orientation orientation) {
    mReceptacle->setOrientation(orientation);
}

SingleDisplayer* MultiDisplayer::insertSingle(int position) {
    auto* displayer = new SingleDisplayer{this};
    mReceptacle->insertWidget(displayer, position);
    return displayer;
}

MultiDisplayer* MultiDisplayer::insertMulti(int position) {
    auto* displayer = new MultiDisplayer{orthogonal(), this};
    mReceptacle->insertWidget(displayer, position);
    return displayer;
}

MultiDisplayer* MultiDisplayer::insertDetached(Qt::Orientation orientation) {
    auto* displayer = new MultiDisplayer{orientation, nullptr};
    displayer->setLocked(isLocked());
    displayer->resize(150, 60); /*!< abritrary size when empty */
    connect(this, &MultiDisplayer::lockChanged, displayer, &MultiDisplayer::setLocked);
    return displayer;
}

std::vector<Displayer*> MultiDisplayer::directChildren() {
    return mReceptacle->widgets<Displayer>();
}

void MultiDisplayer::toggleStretched() {
    setStretched(!isStreched());
}

void MultiDisplayer::toggleScrolling() {
    setScrolling(!isScrolling());
}

void MultiDisplayer::toggleOrientation() {
    setOrientation(orthogonal());
}

void MultiDisplayer::onWidgetInsertion(QWidget* widget) {
    // widget has been dropped,
    // update locked status if it is a displayer
    if (auto* displayer = dynamic_cast<Displayer*>(widget))
        displayer->setLocked(isLocked());
}

void MultiDisplayer::onInsertionRequest() {
    // special slot discarding signal arguments
    // so that no implicit conversion to position occurs
    insertMulti();
}

void MultiDisplayer::changeTitle() {
    bool ok;
    auto title = QInputDialog::getText(this, {}, "Select the new title", QLineEdit::Normal, windowTitle(), &ok);
    if (!ok)
        return;
    setWindowTitle(title);
    if (window() != this)
        window()->setWindowTitle(title);
}

void MultiDisplayer::onDeleteRequest() {
    if (!isEmpty()) {
        QMessageBox::warning(this, {}, "You can't delete a nonempty container");
    } else if (isIndependent()) {
        deleteLater();
    } else {
        QMessageBox::warning(this, {}, "You can't delete this container");
    }
}

void MultiDisplayer::updateLocked(bool locked) {
    /// using dynamic properties, style sheet does not update when property changes :(
    static const QString borderStyle{".MultiDisplayer{border: 2px solid gray;border-radius: 5px;}"};
    if (isLocked() != locked) {
        // disable drops if locked (updating locking status)
        mReceptacle->setAcceptDrops(!locked);
        // disable menu when locked
        setContextMenuPolicy(locked ? Qt::NoContextMenu : Qt::ActionsContextMenu);
        // draw a border when unlocked
        setStyleSheet(!locked && isEmbedded() ? borderStyle : QString{});
        // notify
        emit lockChanged(locked);
    }
}
