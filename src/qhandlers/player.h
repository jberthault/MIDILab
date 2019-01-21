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

using SharedSequence = std::shared_ptr<const Sequence>;

struct NamedSequence {
    SharedSequence sequence;
    QString name;
};

//==============
// SequenceView
//==============

class SequenceView;

class SequenceViewTrackItem : public QTreeWidgetItem {

public:
    explicit SequenceViewTrackItem(track_t track, SequenceView* view, QTreeWidget* parent);

    track_t track() const;
    void setTrack(track_t track);

    SequenceView* view() const;

    void setTrackNames(const QByteArrayList& names);
    void updateEncoding();

private:
    track_t mTrack;
    SequenceView* mView;
    QByteArray mRawText;

};

class SequenceViewItem : public QTreeWidgetItem {

public:
    explicit SequenceViewItem(size_t index, SequenceViewTrackItem* parent);

    SequenceView* view() const;

    size_t index() const;
    void setIndex(size_t index);

    void updateEncoding();
    void updateVisibiliy(families_t families, channels_t channels, const range_t<timestamp_t>& limits);

    QVariant data(int column, int role) const override;

private:
    size_t mIndex;

};

class SequenceView : public QWidget {

    Q_OBJECT

public:
    explicit SequenceView(QWidget* parent);

    inline FamilySelector* familySelector() { return mFamilySelector; }
    inline ChannelsSelector* channelsSelector() { return mChannelsSelector; }

    inline ChannelEditor* channelEditor() { return mChannelEditor; }
    inline Handler* trackFilter() { return mTrackFilter; }
    inline QTextCodec* codec() { return mCodec; }

    inline const SharedSequence& sequence() { return mSequence; }
    const TimedEvent& timedEvent(size_t index) const;

    inline double distorsion() const { return mDistorsion; }
    inline void setDistorsion(double distorsion) { mDistorsion = distorsion; }

    void setChannelEditor(ChannelEditor* channelEditor);
    void setTrackFilter(Handler* handler);
    void setCodec(QTextCodec* codec); /*!< codec used to decode event description (model does not take ownership) */
    void setSequence(const SharedSequence& sequence);
    void cleanSequence();

    void setLower(timestamp_t timestamp);
    void setUpper(timestamp_t timestamp);

signals:
    void positionSelected(timestamp_t timestamp, Qt::MouseButton);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSequenceUpdate();
    void onColorChange(channel_t channel, const QColor& color);
    void onItemChange(QTreeWidgetItem* item, int column);
    void onItemDoubleClick(QTreeWidgetItem* item, int column);
    void onFamilyFilterClick();
    void onChannelFilterClick();
    void onFamiliesChanged(families_t families);
    void onChannelsChanged(channels_t channels);
    void onCodecChange(const QString& name);

private:
    SequenceViewTrackItem* itemForTrack(track_t track) const;
    SequenceViewTrackItem* makeTrackItem(track_t track);
    SequenceViewItem* makeEventItem(size_t index);
    void updateItemsVisibility();
    void updateItemVisibility(SequenceViewItem* item);
    void setItemBackground(QTreeWidgetItem* item, channels_t channels);

private:
    QTreeWidget* mTreeWidget;
    ChannelEditor* mChannelEditor {nullptr};
    FamilySelector* mFamilySelector;
    ChannelsSelector* mChannelsSelector;
    QPushButton* mFamilySelectorButton;
    QPushButton* mChannelSelectorButton;
    QTimer* mSequenceUpdater; /*!< timer filling sequence event asynchronously */
    SharedSequence mSequence;
    size_t mEventCount {0}; /*!< the number of event items properly initialized */
    QTextCodec* mCodec {QTextCodec::codecForLocale()};
    Handler* mTrackFilter {nullptr};
    double mDistorsion {1.};
    Qt::MouseButton mLastButton {Qt::NoButton};
    range_t<timestamp_t> mLimits {0., 0.};
    std::vector<std::unique_ptr<SequenceViewTrackItem>> mTrackReserve;
    std::vector<std::unique_ptr<SequenceViewItem>> mEventReserve;


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
    void initialize(const SharedSequence& sequence, timestamp_t timestamp, timestamp_t maxTimestamp);

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
    SharedSequence mSequence;
    double mDistorsion {1.};
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
    void setSequence(const SharedSequence& sequence); /*!< notify trackbar that a new sequence is played */
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
    void setSequence(const SharedSequence& sequence);

    double distorsion() const;
    void setDistorsion(double distorsion);

private:
    void updateBpm(double bpm, timestamp_t timestamp);
    void updateDistorted(double distorsion);

signals:
    void distorsionChanged(double distorsion);

private:
    timestamp_t mLastTempoTimestamp {-1.};
    QDoubleSpinBox* mTempoSpin;
    QDoubleSpinBox* mDistortedTempoSpin;
    ContinuousSlider* mDistorsionSlider;
    SharedSequence mSequence;

};

//========
// Player
//========

MetaHandler* makeMetaPlayer(QObject* parent);

class Player : public HandlerEditor {

    Q_OBJECT
    
public:
    explicit Player();

    const SharedSequence& sequence() const;

    bool isSingle() const; /*!< end the playlist after the current one */
    bool isLooping() const; /*!< restart from begining when playlist os over */

    bool setNextSequence(int offset); /*!< returns true if a sequence has been set */
    bool setSequence(NamedSequence sequence); /*!< returns true if the sequence has been set */
    void setTrackFilter(Handler* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    Handler* getHandler() override;

protected:
    void updateContext(Context* context) override;

protected slots:
    void saveSequence();
    void setMetronome(bool enabled);

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
    QAction* mMetronomeAction;

    SequenceReader mHandler;

    bool mIsStepping {false};
    timestamp_t mNextStep;

};

#endif // QHANDLERS_PLAYER_H
