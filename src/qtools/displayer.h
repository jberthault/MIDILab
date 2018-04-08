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

#ifndef QTOOLS_DISPLAYER_H
#define QTOOLS_DISPLAYER_H

#include <QWidget>
#include <QFrame>
#include <QMouseEvent>
#include <QBoxLayout>
#include <QToolButton>
#include <QScrollArea>
#include <QToolBar>

//==============
// DragDetector
//==============

class DragDetector : public QObject {

    Q_OBJECT

public:
    explicit DragDetector(QObject* parent);

    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void dragRequest();

private:
    bool farEnough(const QPoint& point);

    QPoint mStartPosition;
    bool mHasPosition;

};

//==========
// Scroller
//==========

class Scroller : public QWidget {

    Q_OBJECT

public:
    explicit Scroller(QWidget* widget, QWidget* parent);
    ~Scroller();

public:
    bool isScrolling() const;
    QWidget* widget();
    QScrollArea* scrollArea();

public slots:
    void setScrolling(bool scrolling);

private:
    QWidget* mWidget;
    QScrollArea* mArea;
    bool mScrolling;

};

//============
// Receptacle
//============

/**
 * A receptacle is a widget able to receive drops of widgets
 */

class Receptacle : public QWidget {

    Q_OBJECT

public:
    static const int nullPosition = -1;

    explicit Receptacle(Qt::Orientation orientation, QWidget* parent);
    ~Receptacle();

    const QStringList& mimeTypes() const;
    void setMimeTypes(const QStringList& mimeTypes);
    bool isSupported(const QMimeData* mimeData) const;

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    template<typename T = QWidget>
    auto widgets() const {
        std::vector<T*> result;
        result.reserve(mLayout->count());
        for (int i=0 ; i < mLayout->count() ; i++)
            if (auto* widget = dynamic_cast<T*>(mLayout->itemAt(i)->widget()))
                result.push_back(widget);
        return result;
    }

    bool insertWidget(QWidget* widget, int position = nullPosition);

signals:
    void widgetInserted(QWidget* widget, int position);

protected:
    void dropEvent(QDropEvent* de) override;
    void dragEnterEvent(QDragEnterEvent* de) override;
    void dragMoveEvent(QDragMoveEvent* de) override;
    void dragLeaveEvent(QDragLeaveEvent* de) override;

private:
    void clearPosition();
    void updatePosition(const QPoint& cursor);
    void moveLine(int position);
    void removeLine();
    void insertLine(int position);

private:
    QStringList mMimeTypes;
    QBoxLayout* mLayout;
    QFrame* mLine;
    Qt::Orientation mOrientation;
    int mPosition;

};

//===========
// Displayer
//===========

/**
 * @brief The Displayer class wraps Widget
 * so they can be rearranged by user on gui.
 * This is the common base for widgets that can be moved on a layer
 */

class Displayer : public QFrame {

    Q_OBJECT

public:
    static const QString mimeType;

    using QFrame::QFrame;

    virtual bool isDraggable() const = 0;

    virtual bool isLocked() const = 0;
    virtual void setLocked(bool locked) = 0;

signals:
    void lockChanged(bool locked);

public slots:
    void drag();

};

//=================
// SingleDisplayer
//=================

class SingleDisplayer : public Displayer {

    Q_OBJECT

public:
    explicit SingleDisplayer(QWidget* parent);

    bool isDraggable() const override;

    bool isLocked() const override;
    void setLocked(bool locked) override;

    QWidget* widget();
    void setWidget(QWidget* widget);

    QToolButton* tool();

private slots:
    void onPress();

private:
    QToolBar* mButtons;
    QWidget* mWidget;
    QToolButton* mTool;
    QToolButton* mMove;

};

//================
// MultiDisplayer
//================

class MultiDisplayer : public Displayer {

    Q_OBJECT

public:
    static std::vector<MultiDisplayer*> topLevelDisplayers();

    explicit MultiDisplayer(Qt::Orientation orientation, QWidget* parent);
    ~MultiDisplayer();

    bool isDraggable() const override;

    bool isLocked() const override;
    void setLocked(bool locked) override;

    bool isStreched() const;
    void setStretched(bool streched);

    bool isScrolling() const;
    void setScrolling(bool scrolling);

    Qt::Orientation orthogonal() const; /*!< returns the orthogonal orientation */
    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    SingleDisplayer* insertSingle(int position = Receptacle::nullPosition);
    MultiDisplayer* insertMulti(int position = Receptacle::nullPosition);
    MultiDisplayer* insertDetached(Qt::Orientation orientation = Qt::Vertical); /*!< @note lifetime management is up to the caller */

    std::vector<Displayer*> directChildren();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void toggleOrientation();
    void toggleScrolling();
    void onWidgetInsertion(QWidget* widget);
    void onInsertionRequest();
    void changeTitle();
    void onDeleteRequest();

private:
    bool isEmbedded() const;
    bool isEmpty() const; /*!< Will be true even if it contains empty displayers */
    void updateLocked(bool locked);

private:
    Scroller* mScroller;
    QWidget* mStreched;
    QHBoxLayout* mStretchLayout;
    Receptacle* mReceptacle;
    bool mStretched;

};

#endif // QTOOLS_DISPLAYER_H
