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

#ifndef QTOOLS_MISC_H
#define QTOOLS_MISC_H

/// this module contains every Qt tools
/// used by the interface that are not related to midi

#include <QWidget>
#include <QLabel>
#include <QTreeView>
#include <QComboBox>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QCheckBox>
#include <QDialog>
#include <QAction>
#include <QTime>
#include <chrono>

//==============
// Layout Utils
//==============

struct stretch_tag {};
struct margin_tag { int margin; };
struct spacing_tag { int spacing; };

template<typename LayoutT>
void add_box(LayoutT* layout, stretch_tag) {
    layout->addStretch();
}

template<typename LayoutT>
void add_box(LayoutT* layout, margin_tag tag) {
    layout->setMargin(tag.margin);
}

template<typename LayoutT>
void add_box(LayoutT* layout, spacing_tag tag) {
    layout->setSpacing(tag.spacing);
}

template<typename LayoutT>
void add_box(LayoutT* layout, QWidget* widget) {
    layout->addWidget(widget);
}

template<typename LayoutT>
void add_box(LayoutT* layout, QLayout* layout_rhs) {
    layout->addLayout(layout_rhs);
}

template<typename LayoutT, typename ... Args>
void fill_box(LayoutT* layout, Args&& ... args);

template<typename LayoutT>
void fill_box(LayoutT* /*layout*/) {}

template<typename LayoutT, typename Arg, typename ... Args>
void fill_box(LayoutT* layout, Arg&& arg, Args&& ... args) {
    add_box(layout, std::forward<Arg>(arg));
    fill_box(layout, std::forward<Args>(args)...);
}

template<typename ... Args>
QHBoxLayout* make_hbox(Args&& ... args) {
    auto hbox = new QHBoxLayout;
    fill_box(hbox, std::forward<Args>(args)...);
    return hbox;
}

template<typename ... Args>
QVBoxLayout* make_vbox(Args&& ... args) {
    auto vbox = new QVBoxLayout;
    fill_box(vbox, std::forward<Args>(args)...);
    return vbox;
}

//=================
// DialogContainer
//=================

class DialogContainer : public QDialog {

    Q_OBJECT

public:
    explicit DialogContainer(QWidget* widget, QWidget* parent);

    QWidget* widget();
    void setWidget(QWidget* widget);

private:
    QWidget* mWidget;

};

//==========
// TriState
//==========

/// @todo enhance for recursive and use it in family selector

class TriState : public QCheckBox {

    Q_OBJECT

public:
    explicit TriState(QWidget* parent);
    explicit TriState(const QString& text, QWidget* parent);

    void addCheckBox(QCheckBox* button);

protected slots:
    void onClick();
    void onThisChange(int state);
    void onChildChange(int);

private:
    bool mDontUpdateThis;
    bool mDontUpdateChildren;
    QSet<QCheckBox*> mButtons;

};

//=========
// TreeBox
//=========

class TreeBox : public QComboBox {

    Q_OBJECT

public:
    explicit TreeBox(QWidget* parent);

    QTreeView* tree();

    bool acceptNodes() const; /*!< default is false */
    void setAcceptNodes(bool accepted);

    bool isLeafIndex(const QModelIndex& index) const;

    QModelIndex treeIndex() const;
    void setTreeIndex(const QModelIndex& index);

    QModelIndex findNext() const;
    QModelIndex findNext(const QModelIndex& origin) const;

    QModelIndex findPrevious() const;
    QModelIndex findPrevious(const QModelIndex& origin) const;

    void showPopup() override;
    void hidePopup() override;

    void wheelEvent(QWheelEvent* event) override;

signals:
    void treeIndexChanged(QModelIndex index);

protected:
    void storeTreeIndex(const QModelIndex& index);

    QModelIndex findLastChild(const QModelIndex& origin, const QModelIndex& defaultIndex) const;

private:
    bool mAcceptNodes;
    QTreeView* mTree;
    mutable QModelIndex mTreeIndex;

};

//==================
// MultiStateAction
//==================

class MultiStateAction : public QAction {

    Q_OBJECT

public:
    explicit MultiStateAction(QObject* parent);
    explicit MultiStateAction(const QString& text, QObject* parent);
    explicit MultiStateAction(const QIcon& icon, const QString& text, QObject* parent);

    void addState(const QString& text);
    void addState(const QIcon& icon, const QString& text);

public slots:
    int state() const;
    void setState(int state);
    void setNextState();

signals:
    void stateChanged(int state);

private:
    QVector<QPair<QIcon, QString>> mStates;
    int mCurrentState;

};

//==============
// HtmlDelegate
//==============

class HtmlDelegate : public QStyledItemDelegate {

public:
    explicit HtmlDelegate(QObject* parent);

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

#endif // QTOOLS_MISC_H
