/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2018 Julien Berthault

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
#include <QGroupBox>
#include <QStyledItemDelegate>
#include <QSocketNotifier>
#include <QTableView>
#include <QCheckBox>
#include <QDialog>
#include <QAction>
#include <QToolButton>
#include <chrono>
#include <set>

namespace qoperators {

std::ostream& operator<<(std::ostream& stream, const QByteArray& byteArray);

std::ostream& operator<<(std::ostream& stream, const QString& string);

}

template<typename T, typename ... Args>
auto number2string(T number, Args&& ... args) {
    auto text = QString::number(number, std::forward<Args>(args)...);
    if (number > 0)
        text.prepend('+');
    return text;
}

QAction* makeAction(const QIcon& icon, const QString& text, QWidget* parent);
QAction* makeSeparator(QWidget* parent);

//==============
// Layout Utils
//==============

struct stretch_tag {};
struct margin_tag { int margin; };
struct margins_tag { QMargins margins; };
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
void add_box(LayoutT* layout, margins_tag tag) {
    layout->setContentsMargins(tag.margins);
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
auto* make_hbox(Args&& ... args) {
    auto* hbox = new QHBoxLayout;
    fill_box(hbox, std::forward<Args>(args)...);
    return hbox;
}

template<typename ... Args>
auto* make_vbox(Args&& ... args) {
    auto* vbox = new QVBoxLayout;
    fill_box(vbox, std::forward<Args>(args)...);
    return vbox;
}

//===============
// PathRetriever
//===============

class PathRetriever : public QObject {

    Q_OBJECT

    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString dir READ dir WRITE setDir)
    Q_PROPERTY(QString filter READ filter WRITE setFilter)

public:
    using QObject::QObject;

    QString caption() const;
    void setCaption(const QString& caption);

    QString dir() const;
    void setDir(const QString& dir);

    QString filter() const;
    void setFilter(const QString& filter);

    void setSelection(const QString& selection); /*!< save dir from selection */

    QString getReadFile(QWidget* parent, const QString& path = {});
    QString getWriteFile(QWidget* parent, const QString& path = {});
    QStringList getReadFiles(QWidget* parent, const QString& path = {});

private:
    QString mCaption;
    QString mDir;
    QString mFilter;

};

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
    std::set<QCheckBox*> mButtons;

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

//================
// CollapseButton
//================

class CollapseButton : public QToolButton {

    Q_OBJECT

public:
    explicit CollapseButton(QTreeView* treeView);

};

//==============
// ExpandButton
//==============

class ExpandButton : public QToolButton {

    Q_OBJECT

public:
    explicit ExpandButton(QTreeView* treeView);

};

//==================
// MultiStateAction
//==================

class MultiStateAction : public QAction {

    Q_OBJECT

public:
    explicit MultiStateAction(QObject* parent);

    void addState(const QString& text);
    void addState(const QIcon& icon, const QString& text);

public slots:
    int state() const;
    void setState(int state);
    void setNextState();

signals:
    void stateChanged(int state);

private:
    std::vector<std::pair<QIcon, QString>> mStates;
    int mCurrentState {-1};

};

//==============
// HtmlDelegate
//==============

class HtmlDelegate : public QStyledItemDelegate {

public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

//================
// NoEditDelegate
//================

class NoEditDelegate: public QStyledItemDelegate {

public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

//==================
// FoldableGroupBox
//==================

class FoldableGroupBox : public QGroupBox {

    Q_OBJECT

public:
    explicit FoldableGroupBox(const QString& title, QWidget* parent);

    QWidget* widget();
    bool isFolded() const;

public slots:
    void setWidget(QWidget* widget);
    void setFolded(bool folded);

protected:
    bool eventFilter(QObject* watch, QEvent* event) override;

private slots:
    void onStateChange();

private:
    QWidget* mWidget {nullptr};
    MultiStateAction* mFoldAction;

};

//================
// SignalNotifier
//================

class SignalNotifier : public QObject {

    Q_OBJECT

public:
    explicit SignalNotifier(QObject* parent);

signals:
    void terminated();

private slots:
    void handleInt();
    void handleTerm();

private:
    QSocketNotifier* mSocketInt {nullptr};
    QSocketNotifier* mSocketTerm {nullptr};

};

//====================
// MenuDefaultTrigger
// ===================

class MenuDefaultTrigger : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

public:
    bool eventFilter(QObject* watched, QEvent* event) override;

};

#endif // QTOOLS_MISC_H
