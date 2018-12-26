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

#ifndef QHANDLERS_PLAYER_H
#define QHANDLERS_PLAYER_H

#include <random>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTextCodec>
#include <QTimeEdit>
#include <QTreeWidget>
#include "handlers/sequencereader.h"
#include "handlers/sequencewriter.h"
#include "qhandlers/common.h"
#include "qtools/misc.h"

struct NamedSequence {
    Sequence sequence;
    QString name;
};

//================
// DistordedClock
//================

class DistordedClock {

public:
    static const QString timeFormat; /*!< preferred format displayed by QTimeEdit */

    DistordedClock(Clock clock = {}, double distorsion = 1.);

    const Clock& clock() const;
    void setClock(Clock clock);

    double distorsion() const;
    void setDistorsion(double distorsion);

    QString toString(timestamp_t timestamp) const;

    QTime toTime(timestamp_t t0, timestamp_t t1) const;
    QTime toTime(timestamp_t timestamp) const;
    timestamp_t toTimestamp(const QTime& t0, const QTime& t1) const;
    timestamp_t toTimestamp(const QTime& time) const;

protected:
    QTime durationCast(const Clock::duration_type& time) const;

private:
    Clock mClock;
    double mDistorsion;

};

//==============
// SequenceView
//==============

class SequenceView;

class SequenceViewTrackItem : public QTreeWidgetItem {

public:
    explicit SequenceViewTrackItem(track_t track, SequenceView* view, QTreeWidget* parent);

    track_t track() const;

    SequenceView* view() const;

    void setRawText(const QByteArray& text, QTextCodec* codec);
    void setCodec(QTextCodec* codec);

private:
    track_t mTrack;
    SequenceView* mView;
    QByteArray mRawText;

};

class SequenceViewItem : public QTreeWidgetItem {

public:
    explicit SequenceViewItem(Sequence::Item item, SequenceViewTrackItem* parent);

    SequenceView* view() const;

    timestamp_t timestamp() const;
    const Event& event() const;

    void setCodec(QTextCodec* codec);

    void updateVisibiliy(families_t families, channels_t channels, timestamp_t lower, timestamp_t upper);

    QVariant data(int column, int role) const override;

private:
    Sequence::Item mItem;
    QByteArray mRawText;

};

class SequenceView : public QWidget {

    Q_OBJECT

public:
    explicit SequenceView(QWidget* parent);

    DistordedClock& distordedClock();
    FamilySelector* familySelector();
    ChannelsSelector* channelsSelector();

    ChannelEditor* channelEditor();
    void setChannelEditor(ChannelEditor* channelEditor);

    Handler* trackFilter();
    void setTrackFilter(Handler* handler);

    void setSequence(Sequence sequence, timestamp_t lower, timestamp_t upper);
    void cleanSequence();

    void setLower(timestamp_t timestamp);
    void setUpper(timestamp_t timestamp);

    SequenceViewTrackItem* itemForTrack(track_t track) const;

    QTextCodec* codec();

public slots:
    void setCodec(QTextCodec* codec); /*!< codec used to decode event description (model does not take ownership) */
    void setCodecByName(const QString& name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void addNextEvents();
    void addEvent(Sequence::Item item);
    void setChannelColor(channel_t channel, const QColor& color);
    void onItemChange(QTreeWidgetItem* item, int column);
    void onItemDoubleClick(QTreeWidgetItem* item, int column);
    void onFamilyFilterClick();
    void onChannelFilterClick();
    void setItemBackground(QTreeWidgetItem* item, channels_t channels);
    void updateItemsVisibility();
    void updateItemVisibility(SequenceViewItem* item);
    void onFamiliesChanged(families_t families);
    void onChannelsChanged(channels_t channels);

signals:
    void positionSelected(timestamp_t timestamp, Qt::MouseButton);

private:
    QTreeWidget* mTreeWidget;
    ChannelEditor* mChannelEditor {nullptr};
    FamilySelector* mFamilySelector;
    ChannelsSelector* mChannelsSelector;
    QPushButton* mFamilySelectorButton;
    QPushButton* mChannelSelectorButton;
    QTimer* mSequenceUpdater; /*!< timer filling sequence event asynchronously */
    Sequence mSequence;
    Sequence::iterator mSequenceIt; /*!< iterator pointing to the next event to add */
    QTextCodec* mCodec {QTextCodec::codecForLocale()};
    Handler* mTrackFilter {nullptr};
    DistordedClock mDistordedClock;
    Qt::MouseButton mLastButton {Qt::NoButton};
    timestamp_t mLower {0.};
    timestamp_t mUpper {0.};

};

//===============
// PlaylistTable
//===============

enum SequenceStatus {
    NO_STATUS,
    PLAYING,
    PAUSED,
    STOPPED
};

class PlaylistItem : public QTableWidgetItem {

public:
    using QTableWidgetItem::QTableWidgetItem;

    virtual NamedSequence loadSequence() = 0;

};

class FileItem : public PlaylistItem {

public:
    explicit FileItem(const QFileInfo& fileInfo);

    const QFileInfo& fileInfo() const;

    NamedSequence loadSequence() override;

private:
    QFileInfo mFileInfo;

};

class WriterItem : public PlaylistItem {

public:
    explicit WriterItem(SequenceWriter* handler);

    SequenceWriter* handler();

    NamedSequence loadSequence() override;

private:
    SequenceWriter* mHandler;

};

class PlaylistTable : public QTableWidget {

    Q_OBJECT

public:
    explicit PlaylistTable(QWidget* parent);

    void insertItem(PlaylistItem* playlistItem);
    void insertItem(int row, PlaylistItem* playlistItem);

    QStringList paths() const;
    size_t addPaths(const QStringList& paths);
    size_t addPaths(QList<QUrl> urls);
    size_t addPath(const QString& path);
    size_t addPath(const QUrl& url);

    void setCurrentStatus(SequenceStatus status);

    bool isLoaded() const;
    NamedSequence loadRow(int row);
    NamedSequence loadRelative(int offset, bool wrap);

    void setContext(Context* context);

public slots:
    void browseFiles();
    void browseRecorders();
    void shuffle();
    void sortAscending();
    void sortDescending();
    void removeSelection();
    void removeAllRows();

protected slots:
    void renameHandler(Handler* handler);
    void removeHandler(Handler* handler);

protected:
    QStringList mimeTypes() const override;
    void dropEvent(QDropEvent* event) override;
    void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;

private:
    std::vector<int> selectedRows() const;
    void moveRows(std::vector<int> rows, int location);
    void removeRows(std::vector<int> rows);
    int rowAt(const QPoint& pos) const;

private:
    Context* mContext {nullptr};
    PlaylistItem* mCurrentItem {nullptr};
    std::default_random_engine mRandomEngine;

};

//==========
// Trackbar
//==========

class MarkerKnob : public ArrowKnob {

    Q_OBJECT

public:
    explicit MarkerKnob(QBoxLayout::Direction direction);

    timestamp_t timestamp() const;
    void setTimestamp(timestamp_t timestamp);

protected slots:
    void onClick(Qt::MouseButton button);

signals:
    void leftClicked(timestamp_t timestamp);
    void rightClicked(timestamp_t timestamp);

private:
    timestamp_t mTimestamp;

};

class TrackedKnob : public QTimeEdit {

    Q_OBJECT

public:
    explicit TrackedKnob(QWidget* parent);

    void setKnob(Knob* knob);

    bool isReversed() const;
    void setReversed(bool reversed); /*!< if true, time left is displayed */

    timestamp_t timestamp() const;
    void setTimestamp(timestamp_t timestamp);
    void updateTimestamp(timestamp_t timestamp); /*!< like setTimestamp but does not send timestampChanged */

    void setDistorsion(double distorsion);
    void initialize(Clock clock, timestamp_t timestamp, timestamp_t maxTimestamp);

protected:
    QTime toTime(timestamp_t timestamp) const;
    timestamp_t toTimestamp(const QTime& time) const;

protected slots:
    void onMove(qreal xvalue, qreal yvalue);
    void onPress(Qt::MouseButton button);
    void onRelease(Qt::MouseButton button);
    void onClick(Qt::MouseButton button);
    void onTimeChange(const QTime& time);

    void updateTime();
    void updateMaximumTime();
    void updateHandle();

signals:
    void timestampChanged(timestamp_t timestamp);
    void rightClicked(timestamp_t timestamp);
    void leftClicked(timestamp_t timestamp);

private:
    Knob* mKnob {nullptr};
    timestamp_t mTimestamp {0.};
    timestamp_t mMaxTimestamp {1.};
    DistordedClock mDistordedClock;
    bool mIsTracking {false};
    bool mIsReversed {false};

};

/**
 * The trackbar is an interactive widget that aims
 * to control sequence position and limits
 */

class Trackbar : public QWidget {

    Q_OBJECT

    Q_PROPERTY(QBrush knobColor READ knobColor WRITE setKnobColor)
    Q_PROPERTY(int knobWidth READ knobWidth WRITE setKnobWidth)

public:

    explicit Trackbar(QWidget* parent);

    const QBrush& knobColor() const;
    void setKnobColor(const QBrush& brush);

    int knobWidth() const;
    void setKnobWidth(int width);

public slots:
    void updateTimestamp(timestamp_t timestamp); /*!< notify trackbar that position has changed */
    void setSequence(const Sequence& sequence, timestamp_t lower, timestamp_t upper); /*!< notify trackbar that a new sequence is played */
    void setDistorsion(double distorsion); /*!< notify trackbar that distorsion has changed */
    void addCustomMarker(timestamp_t timestamp);

protected slots:
    void onPositionLeftClick(timestamp_t timestamp);
    void onLowerLeftClick();
    void onUpperLeftClick();

signals:
    void positionChanged(timestamp_t timestamp);
    void lowerChanged(timestamp_t timestamp);
    void upperChanged(timestamp_t timestamp);

private:
    MarkerKnob* addMarker(timestamp_t timestamp, const QString& tooltip, bool isCustom);
    MarkerKnob* addMarker(bool isCustom);
    void cleanMarkers();

    KnobView* mKnobView;

    TrackedKnob* mPositionEdit;
    TrackedKnob* mLowerEdit;
    TrackedKnob* mUpperEdit;
    TrackedKnob* mLastEdit;

    ParticleKnob* mPositionKnob;
    BracketKnob* mLowerKnob;
    BracketKnob* mUpperKnob;

    std::vector<MarkerKnob*> mMarkerKnobs;
    std::vector<MarkerKnob*> mCustomMarkerKnobs;

};

//===========
// TempoView
//===========

class TempoView : public QWidget {

    Q_OBJECT

public:
    explicit TempoView(QWidget* parent);

public slots:
    void clearTempo();
    void updateTimestamp(timestamp_t timestamp);
    void setClock(Clock clock);

    double distorsion() const;
    void setDistorsion(double distorsion);

private slots:
    void updateDistorted(double distorsion);
    void setTempo(const Event& event);

signals:
    void distorsionChanged(double distorsion);

private:
    Event mLastTempo;
    QDoubleSpinBox* mTempoSpin;
    QDoubleSpinBox* mDistortedTempoSpin;
    ContinuousSlider* mDistorsionSlider;
    Clock mClock;

};

//========
// Player
//========

MetaHandler* makeMetaPlayer(QObject* parent);

class Player : public HandlerEditor {

    Q_OBJECT
    
public:
    explicit Player();

    bool isSingle() const; /*!< end the playlist after the current one */
    bool isLooping() const; /*!< restart from begining when playlist os over */

    bool setNextSequence(int offset); /*!< returns true if a sequence has been set */
    void setSequence(NamedSequence sequence);
    void setTrackFilter(Handler* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    Handler* getHandler() override;

protected:
    void updateContext(Context* context) override;

protected slots:

    void saveSequence();

    void launch(QTableWidgetItem *item);
    void onPositionSelected(timestamp_t timestamp, Qt::MouseButton button);

    /// subwidgets signals modifying handler
    void changePosition(timestamp_t timestamp);
    void changeLower(timestamp_t timestamp);
    void changeUpper(timestamp_t timestamp);
    void changeDistorsion(double distorsion);

    void playSequence();
    void playCurrentSequence(bool resetStepping=true);
    void pauseSequence();
    void resetSequence();
    void playNextSequence();
    void playLastSequence();
    void stepForward();

    void updatePosition();
    void refreshPosition();

private:

    Trackbar* mTracker;
    TempoView* mTempoView;
    SequenceView* mSequenceView;
    PlaylistTable* mPlaylist;
    QTimer* mRefreshTimer;
    MultiStateAction* mModeAction;
    MultiStateAction* mLoopAction;

    SequenceReader mHandler;

    bool mIsStepping {false};
    timestamp_t mNextStep;

};

#endif // QHANDLERS_PLAYER_H
