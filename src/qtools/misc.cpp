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

#include "misc.h"

#include <QEvent>
#include <QDebug>
#include <QPainter>
#include <QTextDocument>
#include <QDropEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPushButton>
#include <QFileDialog>

//===============
// PathRetriever
//===============

PathRetriever::PathRetriever(QObject* parent) : QObject(parent) {

}

QString PathRetriever::caption() const {
    return mCaption;
}

void PathRetriever::setCaption(const QString& caption) {
    mCaption = caption;
}

QString PathRetriever::dir() const {
    return mDir;
}

void PathRetriever::setDir(const QString& dir) {
    mDir = dir;
}

QString PathRetriever::filter() const {
    return mFilter;
}

void PathRetriever::setFilter(const QString& filter) {
    mFilter = filter;
}

void PathRetriever::setSelection(const QString& selection) {
    if (!selection.isNull())
        mDir = QFileInfo(selection).dir().path();
}

QString PathRetriever::getReadFile(QWidget* parent) {
    QString selection = QFileDialog::getOpenFileName(parent, mCaption, mDir, mFilter);
    setSelection(selection);
    return selection;
}

QString PathRetriever::getWriteFile(QWidget *parent) {
    QString selection = QFileDialog::getSaveFileName(parent, mCaption, mDir, mFilter);
    setSelection(selection);
    return selection;
}

QStringList PathRetriever::getReadFiles(QWidget *parent) {
    QStringList selection = QFileDialog::getOpenFileNames(parent, mCaption, mDir, mFilter);
    if (!selection.empty())
        setSelection(selection.front());
    return selection;
}

//=================
// DialogContainer
//=================

DialogContainer::DialogContainer(QWidget* widget, QWidget* parent) :
    QDialog(parent), mWidget(widget) {

    mWidget->setParent(this);
    setWindowTitle(mWidget->windowTitle()); // forward window title

    QPushButton* okButton = new QPushButton("Ok", this);
    QPushButton* cancelButton = new QPushButton("Cancel", this);
    connect(okButton, &QPushButton::clicked, this, &DialogContainer::accept);
    connect(cancelButton, &QPushButton::clicked, this, &DialogContainer::reject);

    setLayout(make_vbox(mWidget, make_hbox(stretch_tag{}, okButton, cancelButton)));
}

QWidget* DialogContainer::widget() {
    return mWidget;
}

void DialogContainer::setWidget(QWidget* /*widget*/) {
    /// @todo implement
    qCritical() << "unimplemented DialogContainer::setWidget";
}

//==========
// TriState
//==========

TriState::TriState(QWidget* parent) : QCheckBox(parent),
    mDontUpdateThis(false), mDontUpdateChildren(false) {

    setTristate(true);
    connect(this, SIGNAL(clicked()), SLOT(onClick()));
    connect(this, SIGNAL(stateChanged(int)), SLOT(onThisChange(int)));
}

TriState::TriState(const QString& text, QWidget* parent) : TriState(parent) {
    setText(text);
}

void TriState::addCheckBox(QCheckBox* button) {
    if (mButtons.contains(button))
        return;
    mButtons.insert(button);
    connect(button, SIGNAL(stateChanged(int)), SLOT(onChildChange(int)));
    onChildChange(0); // update state
}

void TriState::onClick() {
    // we don't accept partial checking for user input
    if (checkState() == Qt::PartiallyChecked)
        setCheckState(Qt::Checked);
}

void TriState::onThisChange(int state) {
    Qt::CheckState checkState = (Qt::CheckState)state;
    if (mDontUpdateChildren || checkState == Qt::PartiallyChecked)
        return;
    mDontUpdateThis = true;
    for (QCheckBox* button : mButtons)
        button->setCheckState(checkState);
    mDontUpdateThis = false;
}

void TriState::onChildChange(int) {
    if (mDontUpdateThis)
        return;

    bool allChecked = true, anyChecked = false;
    for (QCheckBox* button : mButtons) {
        Qt::CheckState subState = button->checkState();
        allChecked = allChecked && subState == Qt::Checked;
        anyChecked = anyChecked || subState != Qt::Unchecked;
    }

    Qt::CheckState state = Qt::Checked;
    if (!allChecked)
        state = anyChecked ? Qt::PartiallyChecked : Qt::Unchecked;

    mDontUpdateChildren = true;
    setCheckState(state);
    mDontUpdateChildren = false;
}

//=========
// TreeBox
//=========

TreeBox::TreeBox(QWidget* parent) : QComboBox(parent), mAcceptNodes(false) {
    mTree = new QTreeView(this);
    mTree->setHeaderHidden(true);
    setView(mTree);
}

QTreeView* TreeBox::tree() {
    return mTree;
}

bool TreeBox::acceptNodes() const {
    return mAcceptNodes;
}

void TreeBox::setAcceptNodes(bool accepted) {
    mAcceptNodes = accepted;
}

bool TreeBox::isLeafIndex(const QModelIndex& index) const {
    return model() && !model()->hasChildren(index);
}

QModelIndex TreeBox::treeIndex() const {
    if (!mTreeIndex.isValid())
        mTreeIndex = model()->index(currentIndex(), modelColumn(), rootModelIndex());
    return mTreeIndex;
}

void TreeBox::setTreeIndex(const QModelIndex& index) {
    if (index.isValid()) {
        setRootModelIndex(index.parent());
        setModelColumn(index.column());
        setCurrentIndex(index.row());
        storeTreeIndex(index);
    }
}

void TreeBox::storeTreeIndex(const QModelIndex& index) {
    if (index != mTreeIndex) {
        mTreeIndex = index;
        // qDebug() << index;
        emit treeIndexChanged(index);
    }
}

void TreeBox::showPopup() {
    setRootModelIndex(QModelIndex());
    mTree->expandToDepth(0);
    QComboBox::showPopup();
}

void TreeBox::hidePopup() {
    setTreeIndex(mTree->currentIndex());
    if (mAcceptNodes || isLeafIndex(treeIndex())) {
        QComboBox::hidePopup();
    }
}

void TreeBox::wheelEvent(QWheelEvent* event) {
    setTreeIndex(event->delta() < 0 ? findNext() : findPrevious());
}

QModelIndex TreeBox::findNext() const {
    return findNext(treeIndex());
}

QModelIndex TreeBox::findNext(const QModelIndex& origin) const {
    QModelIndex originParent = origin.parent();
    QModelIndex result = origin.child(0, 0);
    if (!result.isValid())
        result = origin.sibling(origin.row()+1, origin.column());
    if (!result.isValid())
        result = originParent.sibling(originParent.row()+1, originParent.column());
    if (!mAcceptNodes && result.isValid() && !isLeafIndex(result))
        result = findNext(result);
    return result;
}

QModelIndex TreeBox::findPrevious() const {
    return findPrevious(treeIndex());
}

QModelIndex TreeBox::findPrevious(const QModelIndex& origin) const {
    QModelIndex sibling = origin.sibling(origin.row()-1, origin.column());
    QModelIndex result = findLastChild(sibling, origin.parent());
    if (!mAcceptNodes && result.isValid() && !isLeafIndex(result))
        result = findPrevious(result);
    return result;
}

QModelIndex TreeBox::findLastChild(const QModelIndex& origin, const QModelIndex& defaultIndex) const {
    if (!origin.isValid())
        return defaultIndex;
    int rows = model()->rowCount(origin);
    return findLastChild(origin.child(rows-1, 0), origin);
}

//==================
// MultiStateAction
//==================

MultiStateAction::MultiStateAction(QObject* parent) : QAction(parent), mCurrentState(-1) {
    connect(this, &MultiStateAction::triggered, this, &MultiStateAction::setNextState);
}

MultiStateAction::MultiStateAction(const QString& text, QObject* parent) : MultiStateAction(parent) {
    addState(text);
}

MultiStateAction::MultiStateAction(const QIcon& icon, const QString& text, QObject* parent) : MultiStateAction(parent) {
    addState(icon, text);
}

void MultiStateAction::addState(const QString& text) {
    addState(QIcon(), text);
}

void MultiStateAction::addState(const QIcon& icon, const QString& text) {
    mStates.append(qMakePair(icon, text));
    if (mStates.size() == 1)
        setState(0);
}

int MultiStateAction::state() const {
    return mCurrentState;
}

void MultiStateAction::setState(int state) {
    if (0 <= state && state < mStates.size()) {
        mCurrentState = state;
        setIcon(mStates[state].first);
        setText(mStates[state].second);
        emit stateChanged(state);
    }
}

void MultiStateAction::setNextState() {
    setState((mCurrentState + 1) % mStates.size());
}

//==============
// HtmlDelegate
//==============

HtmlDelegate::HtmlDelegate(QObject* parent) : QStyledItemDelegate(parent) {

}

void HtmlDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);
    painter->save();
    QTextDocument doc;
    doc.setHtml(options.text);
    // options.text.clear();
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);
    painter->translate(options.rect.left(), options.rect.top());
    QRect clip(QPoint(), option.rect.size());
    doc.drawContents(painter, clip);
    painter->restore();
}

QSize HtmlDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);
    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());
    return {(int)doc.idealWidth(), (int)doc.size().height()}; // 19 may be enough
}

