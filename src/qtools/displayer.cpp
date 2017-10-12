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

#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QMenu>
#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include "displayer.h"
#include "misc.h"
#include "tools/trace.h"

//==============
// DragDetector
//==============

DragDetector::DragDetector(QObject* parent) :
    QObject(parent), mHasPosition(false) {

}

bool DragDetector::eventFilter(QObject* obj, QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            mStartPosition = mouseEvent->pos();
            mHasPosition = true;
            return true;
        }
    } break;
    case QEvent::MouseButtonRelease: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            mHasPosition = false;
            return true;
        }
    } break;
    case QEvent::MouseMove: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
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

Receptacle::Receptacle(Qt::Orientation orientation, QWidget* parent) :
    QWidget(parent), mPosition(nullPosition) {

    setAcceptDrops(true);
    // free frame
    mLine = new QFrame;
    mLine->setFrameShadow(QFrame::Sunken);
    mLine->setAcceptDrops(false);
    // layout
    mLayout = new QBoxLayout(QBoxLayout::LeftToRight);
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
    int idx = mLayout->indexOf(widget);
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
    for (const QString& mimeType : mMimeTypes)
        if (mimeData->hasFormat(mimeType))
            return true;
    return false;
}

//===========
// Displayer
//===========

const QString Displayer::mimeType("application/x_displayer");

void Displayer::drag() {
    if (isLocked() || !isDraggable())
        return;
    QMimeData* data = new QMimeData;
    data->setData(mimeType, QByteArray());
    QDrag* drag = new QDrag(this);
    drag->setPixmap(grab());
    drag->setHotSpot(mapFromGlobal(QCursor::pos()));
    drag->setMimeData(data);
    drag->exec(Qt::MoveAction);
    if (drag->target() == nullptr)
        qWarning() << "displayer has not been dropped correctly";
}

//=================
// SingleDisplayer
//=================

SingleDisplayer::SingleDisplayer(QWidget* parent) : Displayer(parent), mWidget(nullptr) {
    // move button
    mMove = new QToolButton(this);
    mMove->setIcon(QIcon(":/data/move.svg"));
    mMove->setToolTip("Drag & drop this widget");
    connect(mMove, &QToolButton::pressed, this, &SingleDisplayer::onPress);
    // settings button
    mTool = new QToolButton(this);
    mTool->setIcon(QIcon(":/data/pin.svg"));
    mTool->setToolTip("Click to see available actions");
    mTool->setPopupMode(QToolButton::InstantPopup);
    mTool->setMenu(new QMenu(this));
    mTool->setVisible(false); /*!< @note unused for now */
    // toolbar
    mButtons = new QToolBar("settings", this);
    //mButtons->setOrientation(Qt::Vertical);
    mButtons->addWidget(mMove);
    mButtons->addWidget(mTool);
    mButtons->hide(); /*!< @note locked at creation */
    // layout
    setLayout(make_hbox(margin_tag{0}, spacing_tag{0}, mButtons));
}

bool SingleDisplayer::isDraggable() const {
    return true;
}

bool SingleDisplayer::isLocked() const {
    return mButtons->isHidden();
}

void SingleDisplayer::setLocked(bool locked) {
    if (mWidget)
        mWidget->setEnabled(locked);
    if (isLocked() != locked) {
        mButtons->setHidden(locked);
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
    connect(mWidget, &QWidget::destroyed, this, &SingleDisplayer::deleteLater);
}

QToolButton* SingleDisplayer::tool() {
    return mTool;
}

void SingleDisplayer::onPress() {
    mMove->setDown(false);
    drag();
}

//================
// MultiDisplayer
//================

QList<MultiDisplayer*> MultiDisplayer::topLevelDisplayers() {
    QList<MultiDisplayer*> displayers;
    for (QWidget* widget : qApp->topLevelWidgets()) {
        MultiDisplayer* displayer = dynamic_cast<MultiDisplayer*>(widget);
        if (displayer)
            displayers << displayer;
    }
    return displayers;
}

MultiDisplayer::MultiDisplayer(Qt::Orientation orientation, QWidget* parent) :
    Displayer(parent), mStretched(false) {

    // drag
    DragDetector* dd = new DragDetector(this);
    connect(dd, SIGNAL(dragRequest()), SLOT(drag()));
    installEventFilter(dd);
    // main widget
    mStreched = new QWidget(this);
    mStretchLayout = make_hbox(margin_tag{0});
    mStreched->setLayout(mStretchLayout);
    // receptacle widget
    mReceptacle = new Receptacle(orientation, mStreched);
    mReceptacle->setMimeTypes(QStringList(mimeType));
    mReceptacle->setAcceptDrops(false); /*!< @note locked at creation */
    mStretchLayout->addWidget(mReceptacle);
    connect(mReceptacle, &Receptacle::widgetInserted, this, &MultiDisplayer::onWidgetInsertion);
    // scrolling
    mScroller = new Scroller(mStreched, this);
    // add actions (@todo use shortcut)
    QAction* addContainerAction = new QAction(QIcon(":/data/plus.svg"), "Add Container", this);
    QAction* toggleScrollingAction = new QAction(QIcon(":/data/elevator.svg"), "Toggle Scrolling", this);
    QAction* toggleOrientationAction = new QAction(QIcon(":/data/transfer.svg"), "Flip Orientation", this);
    QAction* renameAction = new QAction(QIcon(":/data/text.svg"), "Edit Title", this);
    QAction* deleteAction = new QAction(QIcon(":/data/delete.svg"), "Delete", this);
    connect(addContainerAction, &QAction::triggered, this, &MultiDisplayer::onInsertionRequest);
    connect(toggleScrollingAction, &QAction::triggered, this, &MultiDisplayer::toggleScrolling);
    connect(toggleOrientationAction, &QAction::triggered, this, &MultiDisplayer::toggleOrientation);
    connect(renameAction, &QAction::triggered, this, &MultiDisplayer::changeTitle);
    connect(deleteAction, &QAction::triggered, this, &MultiDisplayer::onDeleteRequest);
    addAction(addContainerAction);
    addAction(toggleScrollingAction);
    addAction(toggleOrientationAction);
    addAction(renameAction);
    addAction(deleteAction);
    setContextMenuPolicy(Qt::NoContextMenu);
    // layout
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mScroller));
}

MultiDisplayer::~MultiDisplayer() {
    TRACE_DEBUG("deleting displayer ...");
}

bool MultiDisplayer::isDraggable() const {
    return isEmbedded();
}

bool MultiDisplayer::isLocked() const {
    return !mReceptacle->acceptDrops();
}

void MultiDisplayer::setLocked(bool locked) {
    // update all nodes
    updateLocked(locked);
    for (MultiDisplayer* displayer : findChildren<MultiDisplayer*>())
        displayer->updateLocked(locked);
    // update all leaves
    for (SingleDisplayer* displayer : findChildren<SingleDisplayer*>())
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
    auto displayer = new SingleDisplayer(this);
    mReceptacle->insertWidget(displayer, position);
    return displayer;
}

MultiDisplayer* MultiDisplayer::insertMulti(int position) {
    auto displayer = new MultiDisplayer(orthogonal(), this);
    mReceptacle->insertWidget(displayer, position);
    return displayer;
}

MultiDisplayer* MultiDisplayer::insertDetached(Qt::Orientation orientation) {
    auto displayer = new MultiDisplayer(orientation, nullptr);
    displayer->setWindowFlag(Qt::Window);
    displayer->setLocked(isLocked());
    displayer->resize(150, 60); /*!< abritrary size when empty */
    connect(this, &MultiDisplayer::lockChanged, displayer, &MultiDisplayer::setLocked);
    return displayer;
}

QList<Displayer*> MultiDisplayer::directChildren() {
    return mReceptacle->widgets<Displayer>();
}

void MultiDisplayer::closeEvent(QCloseEvent* event) {
    // Delete window if it does not contains leaf content
    TRACE_DEBUG("closing displayer ...");
    if (isEmpty())
        setAttribute(Qt::WA_DeleteOnClose);
    Displayer::closeEvent(event);
}

void MultiDisplayer::toggleOrientation() {
    setOrientation(orthogonal());
}

void MultiDisplayer::toggleScrolling() {
    setScrolling(!isScrolling());
}

void MultiDisplayer::onWidgetInsertion(QWidget* widget) {
    // widget has been dropped,
    // update locked status if it is a displayer
    Displayer* displayer = dynamic_cast<Displayer*>(widget);
    if (displayer)
        displayer->setLocked(isLocked());
}

void MultiDisplayer::onInsertionRequest() {
    // special slot discarding signal arguments
    // so that no implicit conversion to position occurs
    insertMulti();
}

void MultiDisplayer::changeTitle() {
    bool ok;
    QString title = QInputDialog::getText(this, QString(), "Select the new title", QLineEdit::Normal, windowTitle(), &ok);
    if (!ok)
        return;
    setWindowTitle(title);
    if (window() != this)
        window()->setWindowTitle(title);
}

void MultiDisplayer::onDeleteRequest() {
    if (!isEmpty()) {
        QMessageBox::warning(this, QString(), "You can't delete a nonempty container");
    } else if (isEmbedded() || bool(windowFlags() & Qt::Window)) {
        deleteLater();
    } else {
        QMessageBox::warning(this, QString(), "You can't delete this container");
    }
}

bool MultiDisplayer::isEmbedded() const {
    const QWidget* widget = this;
    while (true) {
        // found the top level window
        if (widget->windowFlags() & Qt::Window)
            return false;
        // check parent
        widget = widget->parentWidget();
        if (!widget)
            return false;
        if (dynamic_cast<const MultiDisplayer*>(widget))
            return true;
    }
}

bool MultiDisplayer::isEmpty() const {
    return findChildren<SingleDisplayer*>().isEmpty();
}

void MultiDisplayer::updateLocked(bool locked) {
    /// using dynamic properties, style sheet does not update when property changes :(
    static const QString borderStyle(".MultiDisplayer{border: 2px solid gray;border-radius: 5px;}");
    if (isLocked() != locked) {
        // disable drops if locked (updating locking status)
        mReceptacle->setAcceptDrops(!locked);
        // disable menu when locked
        setContextMenuPolicy(locked ? Qt::NoContextMenu : Qt::ActionsContextMenu);
        // draw a border when unlocked
        setStyleSheet(!locked && isEmbedded() ? borderStyle : QString());
        // notify
        emit lockChanged(locked);
    }
}
