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

#ifndef QHANDLERS_PLAYER_H
#define QHANDLERS_PLAYER_H

#include <random>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTimeEdit>
#include <QToolBar>
#include <QTreeWidget>
#include "handlers/sequencereader.h"
#include "handlers/sequencewriter.h"
#include "qcore/core.h"
#include "qcore/editors.h"
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

    QTime fromTimestamp(timestamp_t timestamp) const;
    timestamp_t toTimestamp(const QTime& time) const;

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

public slots:
    DistordedClock& distordedClock();

    ChannelEditor* channelEditor();
    void setChannelEditor(ChannelEditor* channelEditor);

    Handler* trackFilter();
    void setTrackFilter(Handler* handler);

    const Sequence& sequence() const;
    void setSequence(const Sequence& sequence, timestamp_t lower, timestamp_t upper);
    void cleanSequence();

    void setLower(timestamp_t timestamp);
    void setUpper(timestamp_t timestamp);

    QTextCodec* codec();
    void setCodec(QTextCodec* codec); /*!< codec used to decode event description (model does not take ownership) */
    void setCodecByName(const QString& name);

    QVector<SequenceViewTrackItem*> trackItems() const;
    SequenceViewTrackItem* itemForTrack(track_t track) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void addNextEvent();
    void addEvent(const Sequence::Item& item);
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
    ChannelEditor* mChannelEditor;
    FamilySelector* mFamilySelector;
    ChannelsSelector* mChannelsSelector;
    QPushButton* mFamilySelectorButton;
    QPushButton* mChannelSelectorButton;
    QTimer* mEventUpdater; /*!< timer filling sequence event asynchronously */
    Sequence mSequence;
    Sequence::const_iterator mSequenceIt; /*!< iterator pointing to the next event to add */
    QTextCodec* mCodec;
    Handler* mTrackFilter;
    DistordedClock mDistordedClock;
    Qt::MouseButton mLastButton;
    timestamp_t mLower;
    timestamp_t mUpper;

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
    void addPaths(const QStringList& paths);
    void addPath(const QString& path);

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
    QAction* makeAction(const QIcon& icon, const QString& text);
    QAction* makeSeparator();

private:
    Context* mContext;
    PlaylistItem* mCurrentItem;
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

    Knob* handle();
    void setKnob(Knob* knob);

    timestamp_t timestamp() const;
    void setTimestamp(timestamp_t timestamp);
    void updateTimestamp(timestamp_t timestamp); /*!< like setTimestamp but does not send timestampChanged */

    void setDistorsion(double distorsion);
    void initialize(Clock clock, timestamp_t timestamp, timestamp_t maxTimestamp);

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
    Knob* mKnob;
    timestamp_t mTimestamp;
    timestamp_t mMaxTimestamp;
    DistordedClock mDistordedClock;
    bool mIsTracking;

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

    QVector<MarkerKnob*> mMarkerKnobs;
    QVector<MarkerKnob*> mCustomMarkerKnobs;

};

//===========
// MediaView
//===========

class MediaView : public QToolBar {

    Q_OBJECT

public:

    enum MediaActionType {
        PLAY_ACTION,
        PAUSE_ACTION,
        STOP_ACTION,
        PLAY_NEXT_ACTION,
        PLAY_LAST_ACTION,
        PLAY_STEP_ACTION,
        SAVE_ACTION
    };

    explicit MediaView(QWidget* parent);

    QAction* getAction(MediaActionType type);

    bool isSingle() const; /*!< end the playlist after the current one */
    bool isLooping() const; /*!< restart from begining when playlist os over */

private:

    QMap<MediaActionType, QAction*> mMediaActions;
    MultiStateAction* mModeAction;
    MultiStateAction* mLoopAction;

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
    void setSequence(const Sequence& sequence);

    double distorsion() const;
    void setDistorsion(double distorsion);

private slots:
    void updateSliderText(qreal ratio);
    void onSliderMove(qreal ratio);
    void updateDistorted();
    void setTempo(const Event& event);

signals:
    void distorsionChanged(double distorsion);

private:
    Event mLastTempo;
    QDoubleSpinBox* mTempoSpin;
    QDoubleSpinBox* mDistortedTempoSpin;
    SimpleSlider* mDistorsionSlider;
    DistordedClock mDistordedClock;


};

//============
// MetaPlayer
//============

class MetaPlayer : public OpenMetaHandler {

public:
    explicit MetaPlayer(QObject* parent);

    void setContent(HandlerProxy& proxy) override;

};

//========
// Player
//========

class Player : public HandlerEditor {

    Q_OBJECT
    
public:
    explicit Player();

    void setNextSequence(bool play, int offset);
    void setSequence(const NamedSequence& sequence, bool play);
    void setTrackFilter(Handler* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    Handler* getHandler() const override;

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

    void playSequence(bool resetStepping=true);
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
    MediaView* mMediaView;
    SequenceView* mSequenceView;
    PlaylistTable* mPlaylist;

    QTimer* mRefreshTimer;
    bool mIsPlaying;

    std::unique_ptr<SequenceReader> mPlayer;

    bool mIsStepping;
    timestamp_t mNextStep;

};

#endif // QHANDLERS_PLAYER_H
