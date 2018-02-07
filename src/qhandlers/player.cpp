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

#include <QHeaderView>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QTextCodec>
#include <QTimer>
#include "qhandlers/player.h"
#include "handlers/trackfilter.h"
#include "qcore/manager.h"

static constexpr range_t<double> distorsionRange = {0., 4.};

namespace {

auto findCodecs() {
    QList<QTextCodec*> codecs;
    for (auto mib : QTextCodec::availableMibs()) {
        auto codec = QTextCodec::codecForMib(mib);
        if (!codecs.contains(codec))
            codecs.append(codec);
    }
    return codecs;
}

}

//===============
// DistordedClock
//===============

const QString DistordedClock::timeFormat("mm:ss.zzz");

DistordedClock::DistordedClock(Clock clock, double distorsion) :
    mClock(std::move(clock)), mDistorsion(distorsion) {

}

const Clock& DistordedClock::clock() const {
    return mClock;
}

void DistordedClock::setClock(Clock clock) {
    mClock = std::move(clock);
}

double DistordedClock::distorsion() const {
    return mDistorsion;
}

void DistordedClock::setDistorsion(double distorsion) {
    mDistorsion = distorsion;
}

QString DistordedClock::toString(timestamp_t timestamp) const {
    return fromTimestamp(timestamp).toString(timeFormat);
}

QTime DistordedClock::fromTimestamp(timestamp_t timestamp) const {
    /// @note no duration_cast to avoid rounding errors
    QTime time;
    if (mDistorsion != 0.)
        time = QTime(0, 0).addMSecs(decay_value<int>(mClock.timestamp2time(timestamp).count() * 1.e-3 / mDistorsion));
    return time;
}

timestamp_t DistordedClock::toTimestamp(const QTime& time) const {
    return mClock.time2timestamp(Clock::duration_type((double)QTime(0, 0).msecsTo(time) * 1.e3 * mDistorsion));
}

//==============
// SequenceView
//==============

SequenceViewTrackItem::SequenceViewTrackItem(track_t track, SequenceView* view, QTreeWidget* parent) :
    QTreeWidgetItem(parent), mTrack(track), mView(view) {

}

track_t SequenceViewTrackItem::track() const {
    return mTrack;
}

SequenceView* SequenceViewTrackItem::view() const {
    return mView;
}

void SequenceViewTrackItem::setRawText(const QByteArray& text, QTextCodec* codec) {
    mRawText = text;
    setText(0, codec->toUnicode(mRawText));
}

void SequenceViewTrackItem::setCodec(QTextCodec* codec) {
    if (!mRawText.isNull())
        setText(0, codec->toUnicode(mRawText));
}

SequenceViewItem::SequenceViewItem(Sequence::Item item, SequenceViewTrackItem *parent) :
    QTreeWidgetItem(parent), mItem(std::move(item)) {

    // timestamp
    setText(0, QString::number(decay_value<long>(mItem.timestamp)));
    // channels
    setText(1, ChannelsSelector::channelsToStringList(mItem.event.channels()).join(' '));
    // type
    setText(2, QString::fromStdString(mItem.event.name()));
    // data
    QByteArray rawText = QByteArray::fromStdString(mItem.event.description());  // Qt 5.4+ only
    rawText.replace("\n", "\\n");
    rawText.replace("\r", "\\r");
    rawText.replace("\t", "\\t");
    if (item.event.is(family_ns::string_families)) {
        mRawText = rawText;
        setText(3, view()->codec()->toUnicode(mRawText));
    } else {
        setText(3, rawText);
    }
}

SequenceView* SequenceViewItem::view() const {
    return static_cast<SequenceViewTrackItem*>(parent())->view();
}

timestamp_t SequenceViewItem::timestamp() const {
    return mItem.timestamp;
}

const Event& SequenceViewItem::event() const {
    return mItem.event;
}

void SequenceViewItem::setCodec(QTextCodec* codec) {
    if (!mRawText.isNull())
        setText(3, codec->toUnicode(mRawText));
}

void SequenceViewItem::updateVisibiliy(families_t families, channels_t channels, timestamp_t lower, timestamp_t upper) {
    bool familiesVisible = mItem.event.is(families);
    bool channelsVisible = !mItem.event.is(family_ns::voice_families) || mItem.event.channels().any(channels);
    bool boundsVisible = lower <= mItem.timestamp && mItem.timestamp <= upper;
    setHidden(!(familiesVisible && channelsVisible && boundsVisible));
}

QVariant SequenceViewItem::data(int column, int role) const {
    if (column == 0 && role == Qt::ToolTipRole)
        return view()->distordedClock().toString(mItem.timestamp);
    return QTreeWidgetItem::data(column, role);
}

SequenceView::SequenceView(QWidget *parent) :
    QWidget(parent), mChannelEditor(nullptr), mCodec(QTextCodec::codecForLocale()),
    mTrackFilter(nullptr), mDistordedClock(), mLastButton(Qt::NoButton), mLower(0.), mUpper(0.) {

    mSequenceIt = mSequence.begin(); // consistent iterator
    mEventUpdater = new QTimer(this);
    connect(mEventUpdater, &QTimer::timeout, this, &SequenceView::addNextEvent);

    mTreeWidget = new QTreeWidget(this);
    mTreeWidget->setAlternatingRowColors(true);
    mTreeWidget->setHeaderLabel(QString()); // clear the default "1" displayed
    mTreeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
    mTreeWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    mTreeWidget->viewport()->installEventFilter(this);
    connect(mTreeWidget, &QTreeWidget::itemChanged, this, &SequenceView::onItemChange);
    connect(mTreeWidget, &QTreeWidget::itemDoubleClicked, this, &SequenceView::onItemDoubleClick);

    mFamilySelectorButton = new QPushButton("Types", this);
    mFamilySelectorButton->setToolTip("Filter by type");
    connect(mFamilySelectorButton, &QPushButton::clicked, this, &SequenceView::onFamilyFilterClick);

    mFamilySelector = new FamilySelector(this);
    mFamilySelector->setFamilies(family_ns::all_families);
    mFamilySelector->setWindowFlags(Qt::Dialog);
    mFamilySelector->setVisible(false);
    connect(mFamilySelector, &FamilySelector::familiesChanged, this, &SequenceView::onFamiliesChanged);

    mChannelSelectorButton = new QPushButton("Channels", this);
    mChannelSelectorButton->setToolTip("Filter by channel");
    connect(mChannelSelectorButton, &QPushButton::clicked, this, &SequenceView::onChannelFilterClick);

    mChannelsSelector = new ChannelsSelector(this);
    mChannelsSelector->setChannels(all_channels);
    mChannelsSelector->setWindowFlags(Qt::Dialog);
    mChannelsSelector->setVisible(false);
    connect(mChannelsSelector, &ChannelsSelector::channelsChanged, this, &SequenceView::onChannelsChanged);

    auto codecSelector = new QComboBox(this);
    codecSelector->setToolTip("Text Encoding");
    for (auto codec : findCodecs())
        codecSelector->addItem(codec->name());
    connect(codecSelector, SIGNAL(currentIndexChanged(QString)), this, SLOT(setCodecByName(QString)));
    setCodecByName(codecSelector->currentText());

    auto collapseButton = new QToolButton(this);
    collapseButton->setToolTip("Collapse");
    collapseButton->setAutoRaise(true);
    collapseButton->setIcon(QIcon(":/data/collapse-up.svg"));
    connect(collapseButton, &QToolButton::clicked, mTreeWidget, &QTreeWidget::collapseAll);

    auto expandButton = new QToolButton(this);
    expandButton->setToolTip("Expand");
    expandButton->setAutoRaise(true);
    expandButton->setIcon(QIcon(":/data/expand-down.svg"));
    connect(expandButton, &QToolButton::clicked, mTreeWidget, &QTreeWidget::expandAll);

    setLayout(make_vbox(margin_tag{0}, mTreeWidget, make_hbox(stretch_tag{}, mChannelSelectorButton, mFamilySelectorButton, codecSelector, expandButton, collapseButton)));
}

DistordedClock& SequenceView::distordedClock() {
    return mDistordedClock;
}

ChannelEditor* SequenceView::channelEditor() {
    return mChannelEditor;
}

void SequenceView::setChannelEditor(ChannelEditor* channelEditor) {
    /// @todo disconnect last
    mChannelEditor = channelEditor;
    mChannelsSelector->setChannelEditor(channelEditor);
    if (channelEditor)
        connect(mChannelEditor, &ChannelEditor::colorChanged, this, &SequenceView::setChannelColor);
}

Handler* SequenceView::trackFilter() {
    return mTrackFilter;
}

void SequenceView::setTrackFilter(Handler* handler) {
    mTrackFilter = handler;
}

QTextCodec* SequenceView::codec() {
    return mCodec;
}

void SequenceView::setCodec(QTextCodec* codec) {
    Q_ASSERT(codec);
    mCodec = codec;
    // prevent itemChanged, we want it to be emitted for checkstate only :(
    QSignalBlocker guard(mTreeWidget);
    Q_UNUSED(guard);
    for (auto trackItem : trackItems()) {
        trackItem->setCodec(codec);
        for (int i=0 ; i < trackItem->childCount() ; i++)
            static_cast<SequenceViewItem*>(trackItem->child(i))->setCodec(mCodec);
    }
}

void SequenceView::setCodecByName(const QString& name) {
    setCodec(QTextCodec::codecForName(name.toLocal8Bit()));
}

QVector<SequenceViewTrackItem*> SequenceView::trackItems() const {
    QVector<SequenceViewTrackItem*> result(mTreeWidget->topLevelItemCount());
    for (int i=0 ; i < result.size() ; ++i)
        result[i] = static_cast<SequenceViewTrackItem*>(mTreeWidget->topLevelItem(i));
    return result;
}

SequenceViewTrackItem* SequenceView::itemForTrack(track_t track) const {
    for (SequenceViewTrackItem* trackItem : trackItems())
        if (trackItem->track() == track)
            return trackItem;
    return nullptr;
}

bool SequenceView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease)
        mLastButton = static_cast<QMouseEvent*>(event)->button();
    return QWidget::eventFilter(watched, event);
}

const Sequence& SequenceView::sequence() const {
    return mSequence;
}

void SequenceView::setSequence(const Sequence& sequence, timestamp_t lower, timestamp_t upper) {

    // prevent signals
    QSignalBlocker guard(this);
    Q_UNUSED(guard);

    // clean previous sequence
    cleanSequence();

    // register sequence
    mSequence = sequence;
    mSequenceIt = mSequence.begin();
    mDistordedClock.setClock(sequence.clock());
    mLower = lower;
    mUpper = upper;

    // reenable all tracks
    if (mTrackFilter)
        mTrackFilter->send_message(TrackFilter::enable_all_event());

    QMap<track_t, channels_t> trackChannels;
    QMap<track_t, QByteArrayList> trackNames;
    for (const Sequence::Item& item : sequence) {
        if (item.event.is(family_ns::voice_families))
            trackChannels[item.track] |= item.event.channels();
        else if (item.event.family() == family_t::track_name)
            trackNames[item.track] << QByteArray::fromStdString(item.event.description());
    }

    for (track_t track : sequence.tracks()) {
        SequenceViewTrackItem* trackItem = new SequenceViewTrackItem(track, this, mTreeWidget);
        trackItem->setFirstColumnSpanned(true);
        // track text
        const QByteArrayList& names = trackNames.value(track);
        QFont trackFont = trackItem->font(0);
        if (names.isEmpty()) {
            trackItem->setText(0, QString("Track #%1").arg(track+1));
            trackFont.setItalic(true);
        } else {
            trackItem->setRawText(names.join(" / "), mCodec);
            trackFont.setWeight(QFont::Bold);
        }
        trackItem->setFont(0, trackFont);
        // track filter enabled
        if (mTrackFilter)
            trackItem->setCheckState(0, Qt::Checked);
        // background name
        channels_t channels = trackChannels[track];
        trackItem->setData(0, Qt::UserRole, channels.to_integral());
        setItemBackground(trackItem, channels);
    }

    // ideal widths for timestamp & channel
    mTreeWidget->setColumnWidth(0, 90);
    mTreeWidget->setColumnWidth(1, 60);
    // start filling events
    mEventUpdater->start();

}

void SequenceView::cleanSequence() {
    mEventUpdater->stop();
    mTreeWidget->clear();
    mTreeWidget->setHeaderLabels(QStringList() << "Timestamp" << "Channel" << "Type" << "Data");
}

void SequenceView::setLower(timestamp_t timestamp) {
    mLower = timestamp;
    updateItemsVisibility();
}

void SequenceView::setUpper(timestamp_t timestamp) {
    mUpper = timestamp;
    updateItemsVisibility();
}

void SequenceView::setItemBackground(QTreeWidgetItem* item, channels_t channels) {
    if (mChannelEditor)
        item->setBackground(0, mChannelEditor->brush(channels, Qt::Horizontal));
}

void SequenceView::updateItemsVisibility() {
    for (SequenceViewTrackItem* trackItem : trackItems())
        for (int i=0 ; i < trackItem->childCount() ; i++)
            updateItemVisibility(static_cast<SequenceViewItem*>(trackItem->child(i)));
}

void SequenceView::updateItemVisibility(SequenceViewItem* item) {
    item->updateVisibiliy(mFamilySelector->families(), mChannelsSelector->channels(), mLower, mUpper);
}

void SequenceView::onFamiliesChanged(families_t families) {
    if (families.all(family_ns::midi_families))
        mFamilySelectorButton->setText("Types");
    else
        mFamilySelectorButton->setText("Types*");
    updateItemsVisibility();

}

void SequenceView::onChannelsChanged(channels_t channels) {
    if (channels == all_channels)
        mChannelSelectorButton->setText("Channels");
    else
        mChannelSelectorButton->setText("Channels*");
    updateItemsVisibility();
}

void SequenceView::addNextEvent() {
    if (mSequenceIt == mSequence.end())
        mEventUpdater->stop();
    else
        addEvent(*mSequenceIt++);
}

void SequenceView::addEvent(const Sequence::Item& item) {
    SequenceViewTrackItem* trackItem = itemForTrack(item.track);
    if (trackItem)
        updateItemVisibility(new SequenceViewItem(item, trackItem));
}

void SequenceView::onItemChange(QTreeWidgetItem* item, int /*column*/) {
    // called when root checkbox is clicked
    SequenceViewTrackItem* trackItem = dynamic_cast<SequenceViewTrackItem*>(item);
    if (trackItem && mTrackFilter) {
        track_t track = trackItem->track();
        bool checked = item->checkState(0) == Qt::Checked;
        mTrackFilter->send_message(checked ? TrackFilter::enable_event(track) : TrackFilter::disable_event(track));
    }
}

void SequenceView::onItemDoubleClick(QTreeWidgetItem* item, int column) {
    if (column == 0) { // timestamp column
        auto eventItem = dynamic_cast<SequenceViewItem*>(item);
        if (eventItem)
            emit positionSelected(eventItem->timestamp(), mLastButton);
    }
}

void SequenceView::onFamilyFilterClick() {
    mFamilySelector->setVisible(!mFamilySelector->isVisible());
}

void SequenceView::onChannelFilterClick() {
    mChannelsSelector->setVisible(!mChannelsSelector->isVisible());
}

void SequenceView::setChannelColor(channel_t channel, const QColor& /*color*/) {
    for (SequenceViewTrackItem* trackItem : trackItems()) {
        auto channels = channels_t::from_integral(trackItem->data(0, Qt::UserRole).toUInt());
        if (channels.contains(channel))
            setItemBackground(trackItem, channels);
    }
}

//===============
// PlaylistTable
//===============

FileItem::FileItem(const QFileInfo& fileInfo) : PlaylistItem(), mFileInfo(fileInfo) {
    mFileInfo = fileInfo;
    setText(mFileInfo.completeBaseName());
    setToolTip(mFileInfo.absoluteFilePath());
}

const QFileInfo& FileItem::fileInfo() const {
    return mFileInfo;
}

NamedSequence FileItem::loadSequence() {
    auto file = dumping::read_file(mFileInfo.absoluteFilePath().toLocal8Bit().constData());
    return {Sequence::from_file(file), text()};
}

WriterItem::WriterItem(SequenceWriter* handler) : PlaylistItem(), mHandler(handler) {
    Q_ASSERT(handler);
    setText(handlerName(mHandler));
    setToolTip("Recorder Handler");
    QFont f = font();
    f.setItalic(true);
    setFont(f);
}

SequenceWriter* WriterItem::handler() {
    return mHandler;
}

NamedSequence WriterItem::loadSequence() {
    return {mHandler->load_sequence(), {}};
}

PlaylistTable::PlaylistTable(QWidget* parent) : QTableWidget(0, 2, parent), mContext(nullptr), mCurrentItem(nullptr) {

    setHorizontalHeaderLabels(QStringList() << "Filename" << "Duration");

    connect(Manager::instance, &Manager::handlerRemoved, this, &PlaylistTable::removeHandler);
    connect(Manager::instance, &Manager::handlerRenamed, this, &PlaylistTable::renameHandler);

    setEditTriggers(NoEditTriggers);
    setAlternatingRowColors(true);
    setSelectionMode(QTableWidget::ExtendedSelection);
    setSelectionBehavior(QTableWidget::SelectRows);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    verticalHeader()->setDefaultSectionSize(20);

    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QTableWidget::DragDrop);
    setDragDropOverwriteMode(false);

    setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction* browseFilesAction = makeAction(QIcon(":/data/folder.svg"), "Browse Files");
    QAction* browseRecordersAction = makeAction(QIcon(":/data/media-record.svg"), "Browse Recorders");
    makeSeparator();
    QAction* shuffleAction = makeAction(QIcon(":/data/random.svg"), "Shuffle");
    QAction* sortAscendingAction = makeAction(QIcon(":/data/sort-ascending.svg"), "Sort Ascdending");
    QAction* sortDescendingAction = makeAction(QIcon(":/data/sort-descending.svg"), "Sort Descending");
    makeSeparator();
    QAction* discardSelectionAction = makeAction(QIcon(":/data/delete.svg"), "Discard");
    QAction* discardAllAction = makeAction(QIcon(":/data/trash.svg"), "Discard All");

    connect(browseFilesAction, &QAction::triggered, this, &PlaylistTable::browseFiles);
    connect(browseRecordersAction, &QAction::triggered, this, &PlaylistTable::browseRecorders);
    connect(shuffleAction, &QAction::triggered, this, &PlaylistTable::shuffle);
    connect(sortAscendingAction, &QAction::triggered, this, &PlaylistTable::sortAscending);
    connect(sortDescendingAction, &QAction::triggered, this, &PlaylistTable::sortDescending);
    connect(discardSelectionAction, &QAction::triggered, this, &PlaylistTable::removeSelection);
    connect(discardAllAction, &QAction::triggered, this, &PlaylistTable::removeAllRows);

}

void PlaylistTable::insertItem(PlaylistItem* playlistItem) {
    insertItem(rowCount(), playlistItem);
}

void PlaylistTable::insertItem(int row, PlaylistItem* playlistItem) {
    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText("*");
    durationItem->setTextAlignment(Qt::AlignCenter);
    playlistItem->setFlags(playlistItem->flags() & ~Qt::ItemIsDropEnabled);
    durationItem->setFlags(durationItem->flags() & ~Qt::ItemIsDropEnabled);
    insertRow(row);
    setItem(row, 0, playlistItem);
    setItem(row, 1, durationItem);
}

QStringList PlaylistTable::paths() const {
    QStringList result;
    for (int i=0 ; i < rowCount() ; i++) {
        auto fileItem = dynamic_cast<FileItem*>(item(i, 0));
        if (fileItem)
            result.append(fileItem->fileInfo().absoluteFilePath());
    }
    return result;
}

void PlaylistTable::addPaths(const QStringList& paths) {
    for (const auto& path : paths)
        addPath(path);
}

void PlaylistTable::addPath(const QString& path) {
    static const QStringList suffixes = QStringList() << "mid" << "midi" << "kar";
    QFileInfo info(path);
    if (!info.exists()) {
        TRACE_WARNING("can't find file " << path);
        return;
    }
    if (info.isDir()) {
        for (const QFileInfo& child : QDir(path).entryInfoList(QDir::Files))
            if (suffixes.contains(child.suffix()))
                insertItem(new FileItem(child));
    } else {
        insertItem(new FileItem(info));
    }
}

void PlaylistTable::setCurrentStatus(SequenceStatus status) {
    if (mCurrentItem) {
        switch (status) {
        case NO_STATUS : mCurrentItem->setIcon(QIcon()); break;
        case PLAYING : mCurrentItem->setIcon(QIcon(":/data/media-play.svg")); break;
        case PAUSED : mCurrentItem->setIcon(QIcon(":/data/media-pause.svg")); break;
        case STOPPED : mCurrentItem->setIcon(QIcon(":/data/media-stop.svg")); break;
        }
    }
}

bool PlaylistTable::isLoaded() const {
    return mCurrentItem;
}

NamedSequence PlaylistTable::loadRow(int row) {
    NamedSequence sequence;
    auto playlistItem = dynamic_cast<PlaylistItem*>(item(row, 0));
    if (playlistItem) {
        sequence = playlistItem->loadSequence();
        if (!sequence.sequence.empty()) {
            // change status
            setCurrentStatus(NO_STATUS);
            mCurrentItem = playlistItem;
            // set duration
            item(row, 1)->setText(DistordedClock(sequence.sequence.clock()).toString(sequence.sequence.last_timestamp()));
            // ensure line is visible
            scrollToItem(playlistItem);
        }
    }
    return sequence;
}

NamedSequence PlaylistTable::loadRelative(int offset, bool wrap) {
    int row = 0;
    if (mCurrentItem) {
        row = mCurrentItem->row() + offset;
        if (wrap)
            row = safe_modulo(row, rowCount());
    }
    return loadRow(row);
}

void PlaylistTable::setContext(Context* context) {
    mContext = context;
}

void PlaylistTable::browseFiles() {
    addPaths(mContext->pathRetriever("midi")->getReadFiles(this));
}

void PlaylistTable::browseRecorders() {
    // get all sequence writers
    QList<Handler*> handlers;
    for (const auto& proxy : mContext->getProxies())
        if (dynamic_cast<SequenceWriter*>(proxy.handler()))
            handlers << proxy.handler();
    if (handlers.empty()) {
        QMessageBox::information(this, QString(), "No recorder available");
        return;
    }
    // make and fill a selector
    HandlerSelector* selector = new HandlerSelector(this);
    selector->setWindowTitle("Select the recorder to import");
    for (Handler* handler : handlers)
        selector->insertHandler(handler);
    // run it
    DialogContainer ask(selector, this);
    if (ask.exec() == QDialog::Accepted) {
        auto sw = dynamic_cast<SequenceWriter*>(selector->currentHandler());
        if (sw == nullptr) {
            QMessageBox::warning(this, QString(), "No recorder selected");
        } else {
            insertItem(new WriterItem(sw));
        }
    }
}

void PlaylistTable::shuffle() {
    int rows = rowCount(), cols = columnCount();
    // shuffle numbers in range [0, rows)
    std::vector<int> order(rows);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), mRandomEngine);
    // save all items
    std::map<std::pair<int, int>, QTableWidgetItem*> itemsCache;
    for (int r=0 ; r < rows ; r++)
        for (int c=0 ; c < cols ; c++)
            itemsCache[std::make_pair(r, c)] = takeItem(r, c);
    // set items to their new positions
    for (int r=0 ; r < rows ; r++)
        for (int c=0 ; c < cols ; c++)
            setItem(order[r], c, itemsCache[std::make_pair(r, c)]);
}

void PlaylistTable::sortAscending() {
    sortByColumn(0, Qt::AscendingOrder);
}

void PlaylistTable::sortDescending() {
    sortByColumn(0, Qt::DescendingOrder);
}

void PlaylistTable::removeSelection() {
    removeRows(selectedRows());
}

void PlaylistTable::removeAllRows() {
    setRowCount(0);
}

void PlaylistTable::renameHandler(Handler* handler) {
    for (int row=0 ; row < rowCount() ; row++) {
        WriterItem* writerItem = dynamic_cast<WriterItem*>(item(row, 0));
        if (writerItem && writerItem->handler() == handler)
            writerItem->setText(handlerName(handler));
    }
}

void PlaylistTable::removeHandler(Handler* handler) {
    for (int row=0 ; row < rowCount() ; ) {
        WriterItem* writerItem = dynamic_cast<WriterItem*>(item(row, 0));
        if (writerItem && writerItem->handler() == handler)
            removeRow(row);
        else
            ++row;
    }
}

QStringList PlaylistTable::mimeTypes() const {
    return QTableWidget::mimeTypes() << "text/uri-list";
}

void PlaylistTable::dropEvent(QDropEvent* event) {
    // drop from filesystem, sort urls as they are never ordered
    if (event->mimeData()->hasFormat("text/uri-list")) {
        QList<QUrl> urls = event->mimeData()->urls();
        qSort(urls);
        for (const QUrl& url : urls)
            addPath(url.toLocalFile());
        event->accept();
        return;
    }
    // drop selected rows from the table itself
    if (event->source() == this && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        moveRows(selectedRows(), rowAt(event->pos()));
        event->accept();
        return;
    }
    QTableWidget::dropEvent(event);
}

void PlaylistTable::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) {
    if (mCurrentItem && start <= mCurrentItem->row() && mCurrentItem->row() <= end)
        mCurrentItem = nullptr;
    QTableWidget::rowsAboutToBeRemoved(parent, start, end);
}

std::vector<int> PlaylistTable::selectedRows() const {
    std::vector<int> sourceRows;
    sourceRows.reserve(rowCount());
    for (const QModelIndex& index : selectionModel()->selectedRows())
        sourceRows.push_back(index.row());
    return sourceRows;
}

void PlaylistTable::moveRows(std::vector<int> rows, int location) {
    // insert new rows
    for (size_t i=0 ; i < rows.size() ; i++)
        insertRow(location);
    // adjust row selection after insertion
    for (int& row : rows)
        if (row >= location)
            row += rows.size();
    // copy selected rows
    for (int row : rows) {
        for (int col=0 ; col < columnCount() ; ++col)
            setItem(location, col, takeItem(row, col));
        ++location;
    }
    // remove inner rows
    removeRows(std::move(rows));
}

void PlaylistTable::removeRows(std::vector<int> rows) {
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (int row : rows)
        removeRow(row);
}

int PlaylistTable::rowAt(const QPoint& pos) const {
    QModelIndex index = indexAt(pos);
    // append item if it is dropped in the viewport
    if (!index.isValid())
        return rowCount();
    QRect itemRect = visualRect(index);
    QPoint relPos = pos - itemRect.topLeft();
    // formula to get vertical position (match the drop indicator)
    bool isBefore = relPos.y() < (itemRect.height()-1)/2;
    return isBefore ? index.row() : index.row() + 1;
}

QAction* PlaylistTable::makeAction(const QIcon& icon, const QString& text) {
    auto action = new QAction(icon, text, this);
    addAction(action);
    return action;
}

QAction* PlaylistTable::makeSeparator() {
    auto action = new QAction(this);
    action->setSeparator(true);
    addAction(action);
    return action;
}

//==========
// Trackbar
//==========

MarkerKnob::MarkerKnob(QBoxLayout::Direction direction) : ArrowKnob(direction) {
    setFlag(MarkerKnob::ItemIsMovable, false);
    connect(this, &ArrowKnob::knobDoubleClicked, this, &MarkerKnob::onClick);
}

timestamp_t MarkerKnob::timestamp() const {
    return mTimestamp;
}

void MarkerKnob::setTimestamp(timestamp_t timestamp) {
    mTimestamp = timestamp;
}

void MarkerKnob::onClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        emit leftClicked(mTimestamp);
    } else if (button == Qt::RightButton) {
        emit rightClicked(mTimestamp);
    }
}

TrackedKnob::TrackedKnob(QWidget* parent) :
    QTimeEdit(parent), mKnob(nullptr), mTimestamp(0.), mMaxTimestamp(1.), mDistordedClock(), mIsTracking(false) {

    connect(this, &TrackedKnob::timeChanged, this, &TrackedKnob::onTimeChange);
}

Knob* TrackedKnob::handle() {
    return mKnob;
}

void TrackedKnob::setKnob(Knob* knob) {
    mKnob = knob;
    mKnob->setToolTip(toolTip());
    mKnob->xScale().margins = {8., 8.};
    mKnob->yScale().clamp(.5);
    connect(mKnob, &Knob::knobMoved, this, &TrackedKnob::onMove);
    connect(mKnob, &Knob::knobPressed, this, &TrackedKnob::onPress);
    connect(mKnob, &Knob::knobReleased, this, &TrackedKnob::onRelease);
    connect(mKnob, &Knob::knobDoubleClicked, this, &TrackedKnob::onClick);
}

timestamp_t TrackedKnob::timestamp() const {
    return mTimestamp;
}

void TrackedKnob::setTimestamp(timestamp_t timestamp) {
    if (!mIsTracking) {
        mTimestamp = timestamp;
        updateTime();
        updateHandle();
        emit timestampChanged(timestamp);
    }
}

void TrackedKnob::updateTimestamp(timestamp_t timestamp) {
    if (!mIsTracking) {
        mTimestamp = timestamp;
        updateTime();
        updateHandle();
    }
}

void TrackedKnob::setDistorsion(double distorsion) {
    mDistordedClock.setDistorsion(distorsion);
    updateMaximumTime();
    updateTime();
}

void TrackedKnob::initialize(Clock clock, timestamp_t timestamp, timestamp_t maxTimestamp) {
    mDistordedClock.setClock(std::move(clock));
    mMaxTimestamp = maxTimestamp;
    mTimestamp = timestamp;
    updateMaximumTime();
    updateTime();
    updateHandle();
}

void TrackedKnob::onMove(qreal xvalue, qreal /*yvalue*/) {
    mTimestamp = xvalue * mMaxTimestamp;
    setTime(mDistordedClock.fromTimestamp(mTimestamp));
}

void TrackedKnob::onPress(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        mIsTracking = true;
        blockSignals(true);
    }
}

void TrackedKnob::onRelease(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        mIsTracking = false;
        blockSignals(false);
        emit timestampChanged(mTimestamp);
    }
}

void TrackedKnob::onClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        emit leftClicked(mTimestamp);
    } else if (button == Qt::RightButton) {
        emit rightClicked(mTimestamp);
    }
}

void TrackedKnob::onTimeChange(const QTime& time) {
    mTimestamp = mDistordedClock.toTimestamp(time);
    updateHandle();
    emit timestampChanged(mTimestamp);
}

void TrackedKnob::updateTime() {
    blockSignals(true);
    setTime(mDistordedClock.fromTimestamp(mTimestamp));
    blockSignals(false);
}

void TrackedKnob::updateMaximumTime() {
    blockSignals(true);
    setMaximumTime(mDistordedClock.fromTimestamp(mMaxTimestamp));
    blockSignals(false);
}

void TrackedKnob::updateHandle() {
    if (mKnob) {
        mKnob->xScale().value = mTimestamp / mMaxTimestamp;
        mKnob->moveToFit();
    }
}

Trackbar::Trackbar(QWidget* parent) : QWidget(parent) {

    // Position
    mPositionKnob = new ParticleKnob();
    mPositionKnob->setZValue(1.);
    mPositionEdit = new TrackedKnob(this);
    mPositionEdit->setDisplayFormat(DistordedClock::timeFormat);
    mPositionEdit->setToolTip("Position");
    mPositionEdit->setKnob(mPositionKnob);
    connect(mPositionEdit, &TrackedKnob::leftClicked, this, &Trackbar::onPositionLeftClick);
    connect(mPositionEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mPositionEdit, &TrackedKnob::timestampChanged, this, &Trackbar::positionChanged);

    // Lower Position
    mLowerKnob = new BracketKnob(QBoxLayout::LeftToRight);
    mLowerEdit = new TrackedKnob(this);
    mLowerEdit->setDisplayFormat("[ " + DistordedClock::timeFormat);
    mLowerEdit->setToolTip("Lower Limit");
    mLowerEdit->setKnob(mLowerKnob);
    connect(mLowerEdit, &TrackedKnob::leftClicked, this, &Trackbar::onLowerLeftClick);
    connect(mLowerEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mLowerEdit, &TrackedKnob::timestampChanged, this, &Trackbar::lowerChanged);

    // Upper position
    mUpperKnob = new BracketKnob(QBoxLayout::RightToLeft);
    mUpperEdit= new TrackedKnob(this);
    mUpperEdit->setDisplayFormat(DistordedClock::timeFormat + " ]");
    mUpperEdit->setToolTip("Upper Limit");
    mUpperEdit->setKnob(mUpperKnob);
    mUpperKnob->xScale().value = 1.;
    connect(mUpperEdit, &TrackedKnob::leftClicked, this, &Trackbar::onUpperLeftClick);
    connect(mUpperEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mUpperEdit, &TrackedKnob::timestampChanged, this, &Trackbar::upperChanged);

    // Last Position
    mLastEdit = new TrackedKnob(this);
    mLastEdit->setReadOnly(true);
    mLastEdit->setAlignment(Qt::AlignHCenter);
    mLastEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    mLastEdit->setDisplayFormat(DistordedClock::timeFormat);
    mLastEdit->setToolTip("Duration");

    // View
    mKnobView = new KnobView(this);
    mKnobView->setFixedHeight(31);
    mKnobView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mKnobView->insertKnob(mPositionKnob);
    mKnobView->insertKnob(mLowerKnob);
    mKnobView->insertKnob(mUpperKnob);

    setLayout(make_vbox(margin_tag{0}, mKnobView, make_hbox(margin_tag{0}, mLowerEdit, mPositionEdit, mUpperEdit, mLastEdit)));
}

const QBrush& Trackbar::knobColor() const {
    return mPositionKnob->color();
}

void Trackbar::setKnobColor(const QBrush& brush) {
    mPositionKnob->setColor(brush);
    for (MarkerKnob* knob : mMarkerKnobs)
        knob->setColor(brush);
    for (MarkerKnob* knob : mCustomMarkerKnobs)
        knob->setColor(brush);
    QPen pen = mLowerKnob->pen();
    pen.setBrush(brush);
    mLowerKnob->setPen(pen);
    mUpperKnob->setPen(pen);
}

int Trackbar::knobWidth() const {
    return mLowerKnob->pen().width();
}

void Trackbar::setKnobWidth(int width) {
    QPen pen = mLowerKnob->pen();
    pen.setWidth(width);
    mLowerKnob->setPen(pen);
    mUpperKnob->setPen(pen);
}

void Trackbar::onPositionLeftClick(timestamp_t timestamp) {
    mLowerEdit->setTimestamp(timestamp);
}

void Trackbar::onLowerLeftClick() {
    mLowerEdit->setTimestamp(0.);
}

void Trackbar::onUpperLeftClick() {
    mUpperEdit->setTimestamp(mLastEdit->timestamp());
}

void Trackbar::updateTimestamp(timestamp_t timestamp) {
    mPositionEdit->updateTimestamp(timestamp);
}

MarkerKnob* Trackbar::addMarker(timestamp_t timestamp, const QString& tooltip, bool isCustom) {
    auto knob = addMarker(isCustom);
    knob->setTimestamp(timestamp);
    knob->xScale().value = timestamp / mLastEdit->timestamp();
    knob->setToolTip(tooltip);
    knob->moveToFit();
    return knob;
}

MarkerKnob* Trackbar::addMarker(bool isCustom) {
    QVector<MarkerKnob*>& knobs = isCustom ? mCustomMarkerKnobs : mMarkerKnobs;
    // look for a handle available
    for (auto knob : knobs) {
        if (!knob->isVisible()) {
            knob->setVisible(true);
            return knob;
        }
    }
    // no handle is available, make it on the fly
    MarkerKnob* knob = new MarkerKnob(isCustom ? QBoxLayout::BottomToTop : QBoxLayout::TopToBottom);
    connect(knob, &MarkerKnob::leftClicked, mPositionEdit, &TrackedKnob::setTimestamp);
    if (isCustom) {
        // connection is queued to let the item process its events before being hidden
        connect(knob, &MarkerKnob::rightClicked, this, [knob](){ knob->hide(); }, Qt::QueuedConnection);
    }
    knob->setColor(knobColor());
    knob->xScale().margins = {8., 8.};
    knob->yScale().value = isCustom ? .7 : .3;
    mKnobView->insertKnob(knob);
    knobs.append(knob);
    return knob;
}

void Trackbar::setSequence(const Sequence& sequence, timestamp_t lower, timestamp_t upper) {
    timestamp_t lastTimestamp = sequence.last_timestamp();
    if (lastTimestamp == 0.)
        lastTimestamp = 1.;
    // reinitialize editors
    mLowerEdit->initialize(sequence.clock(), lower, lastTimestamp);
    mUpperEdit->initialize(sequence.clock(), upper, lastTimestamp);
    mPositionEdit->initialize(sequence.clock(), lower, lastTimestamp);
    mLastEdit->initialize(sequence.clock(), lastTimestamp, lastTimestamp);
    // reinitialize markers
    cleanMarkers();
    for (const Sequence::Item& item : sequence)
        if (item.event.family() == family_t::marker)
            addMarker(item.timestamp, QString::fromStdString(item.event.description()), false);
}

void Trackbar::setDistorsion(double distorsion) {
    mLowerEdit->setDistorsion(distorsion);
    mUpperEdit->setDistorsion(distorsion);
    mPositionEdit->setDistorsion(distorsion);
    mLastEdit->setDistorsion(distorsion);
}

void Trackbar::addCustomMarker(timestamp_t timestamp) {
    addMarker(timestamp, "Custom Marker", true);
}

void Trackbar::cleanMarkers() {
    for (auto knob : mMarkerKnobs)
        knob->setVisible(false);
    for (auto knob : mCustomMarkerKnobs)
        knob->setVisible(false);
}

//===========
// MediaView
//===========

MediaView::MediaView(QWidget* parent) : QToolBar("Media", parent) {
    /// @todo get a finer arrow for move-down
    mMediaActions[PLAY_LAST_ACTION] = addAction(QIcon(":/data/media-step-backward.svg"), "Play Previous");
    mMediaActions[PLAY_ACTION] = addAction(QIcon(":/data/media-play.svg"), "Play");
    mMediaActions[PAUSE_ACTION] = addAction(QIcon(":/data/media-pause.svg"), "Pause");
    mMediaActions[STOP_ACTION] = addAction(QIcon(":/data/media-stop.svg"), "Stop");
    mMediaActions[PLAY_NEXT_ACTION] = addAction(QIcon(":/data/media-step-forward.svg"), "Play Next");
    mMediaActions[PLAY_STEP_ACTION] = addAction(QIcon(":/data/chevron-right.svg"), "Play Step");
    addSeparator();
    mLoopAction = new MultiStateAction(this);
    addAction(mLoopAction);
    mLoopAction->addState(QIcon(":/data/move-down.svg"), "No Loop");
    mLoopAction->addState(QIcon(":/data/loop-square.svg"), "Loop");
    mModeAction = new MultiStateAction(this);
    addAction(mModeAction);
    mModeAction->addState(QIcon(":/data/lines.svg"), "Play All");
    mModeAction->addState(QIcon(":/data/highlighted-lines.svg"), "Play Current");
    addSeparator();
    mMediaActions[SAVE_ACTION] = addAction(QIcon(":/data/save.svg"), "Save Current Sequence");

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
}

bool MediaView::isSingle() const {
    return mModeAction->state() == 1;
}

bool MediaView::isLooping() const {
    return mLoopAction->state() == 1;
}

QAction* MediaView::getAction(MediaActionType type) {
    return mMediaActions.value(type, nullptr);
}

//===========
// TempoView
//===========

namespace {

QDoubleSpinBox* newTempoSpinBox(QWidget* parent) {
    auto spin = new QDoubleSpinBox(parent);
    spin->setSpecialValueText("...");
    spin->setReadOnly(true);
    spin->setSuffix(" bpm");
    spin->setAlignment(Qt::AlignCenter);
    spin->setDecimals(1);
    spin->setMaximum(2000.);
    spin->setButtonSymbols(QDoubleSpinBox::NoButtons);
    return spin;
}

}

TempoView::TempoView(QWidget* parent) : QWidget(parent) {

    mTempoSpin = newTempoSpinBox(this);
    mTempoSpin->setToolTip("Base Tempo");

    mDistortedTempoSpin = newTempoSpinBox(this);
    mDistortedTempoSpin->setToolTip("Current Tempo");

    /// @todo turn slider label into a rich editor
    mDistorsionSlider = new SimpleSlider(Qt::Horizontal, this);
    mDistorsionSlider->setTextWidth(35);
    mDistorsionSlider->setToolTip("Tempo Distorsion");
    mDistorsionSlider->setDefaultRatio(distorsionRange.reduce(1.));
    connect(mDistorsionSlider, &SimpleSlider::knobChanged, this, &TempoView::updateSliderText);
    connect(mDistorsionSlider, &SimpleSlider::knobMoved, this, &TempoView::onSliderMove);
    mDistorsionSlider->setDefault();

    setLayout(make_hbox(margin_tag{0}, mDistorsionSlider, mTempoSpin, mDistortedTempoSpin));
}

void TempoView::clearTempo() {
    setTempo(Event());
}

void TempoView::updateTimestamp(timestamp_t timestamp) {
    setTempo(mDistordedClock.clock().last_tempo(timestamp));
}

void TempoView::setSequence(const Sequence& sequence) {
    mDistordedClock.setClock(sequence.clock());
}

double TempoView::distorsion() const {
    return mDistordedClock.distorsion();
}

void TempoView::setDistorsion(double distorsion) {
    if (distorsionRange.min <= distorsion && distorsion <= distorsionRange.max) {
        mDistordedClock.setDistorsion(distorsion);
        mDistorsionSlider->setRatio(distorsionRange.reduce(distorsion));
        updateDistorted();
        emit distorsionChanged(distorsion);
    }
}

void TempoView::setTempo(const Event& event) {
    if (event == mLastTempo)
        return;
    mLastTempo = event;
    double bpm = event.family() == family_t::tempo ? event.get_bpm() : 0.;
    mTempoSpin->setValue(bpm);
    updateDistorted();
}

void TempoView::updateSliderText(qreal ratio) {
    double distorsion = distorsionRange.expand(ratio);
    mDistorsionSlider->setText(QString::number(decay_value<int>(100*distorsion)) + "%");
}

void TempoView::onSliderMove(qreal ratio) {
    double distorsion = distorsionRange.expand(ratio);
    mDistordedClock.setDistorsion(distorsion);
    updateDistorted();
    updateSliderText(ratio);
    emit distorsionChanged(distorsion);
}

void TempoView::updateDistorted() {
    mDistortedTempoSpin->setValue(mTempoSpin->value() * mDistordedClock.distorsion());
}

//============
// MetaPlayer
//============

MetaPlayer::MetaPlayer(QObject* parent) : OpenMetaHandler(parent) {
    setIdentifier("Player");
}

void MetaPlayer::setContent(HandlerProxy& proxy) {
    proxy.setContent(new Player);
}

//========
// Player
//========

Player::Player() :
    HandlerEditor(), mIsPlaying(false), mPlayer(std::make_unique<SequenceReader>()), mIsStepping(false) {

    mPlaylist = new PlaylistTable(this);
    connect(mPlaylist, &PlaylistTable::itemActivated, this, &Player::launch);

    mSequenceView = new SequenceView(this);
    connect(mSequenceView, &SequenceView::positionSelected, this, &Player::onPositionSelected);

    mMediaView = new MediaView(this);
    connect(mMediaView->getAction(MediaView::PLAY_ACTION), &QAction::triggered, this, &Player::playSequence);
    connect(mMediaView->getAction(MediaView::PAUSE_ACTION), &QAction::triggered, this, &Player::pauseSequence);
    connect(mMediaView->getAction(MediaView::STOP_ACTION), &QAction::triggered, this, &Player::resetSequence);
    connect(mMediaView->getAction(MediaView::PLAY_LAST_ACTION), &QAction::triggered, this, &Player::playLastSequence);
    connect(mMediaView->getAction(MediaView::PLAY_NEXT_ACTION), &QAction::triggered, this, &Player::playNextSequence);
    connect(mMediaView->getAction(MediaView::SAVE_ACTION), &QAction::triggered, this, &Player::saveSequence);
    connect(mMediaView->getAction(MediaView::PLAY_STEP_ACTION), &QAction::triggered, this, &Player::stepForward);

    mTracker = new Trackbar(this);
    connect(mTracker, &Trackbar::positionChanged, this, &Player::changePosition);
    connect(mTracker, &Trackbar::lowerChanged, this, &Player::changeLower);
    connect(mTracker, &Trackbar::upperChanged, this, &Player::changeUpper);

    mTempoView = new TempoView(this);
    connect(mTempoView, &TempoView::distorsionChanged, this, &Player::changeDistorsion);

    mRefreshTimer = new QTimer(this);
    mRefreshTimer->setInterval(75); /// @note update every 75ms
    connect(mRefreshTimer, &QTimer::timeout, this, &Player::refreshPosition);

    QTabWidget* tab = new QTabWidget(this);
    tab->addTab(mPlaylist, "Playlist");
    tab->addTab(mSequenceView, "Events");

    setLayout(make_vbox(margin_tag{0}, mMediaView, mTracker, mTempoView, tab));
}

HandlerView::Parameters Player::getParameters() const {
    /// @todo get track filter
    auto result = HandlerEditor::getParameters();
    auto paths = mPlaylist->paths();
    if (!paths.empty())
        result.push_back(Parameter{"playlist", paths.join(';')});
    SERIALIZE("distorsion", serial::serializeNumber, mTempoView->distorsion(), result);
    return result;
}

size_t Player::setParameter(const Parameter& parameter) {
    /// @todo set track filter
    if (parameter.name == "playlist") {
        mPlaylist->addPaths(parameter.value.split(';'));
        return 1;
    }
    UNSERIALIZE("distorsion", serial::parseDouble, mTempoView->setDistorsion, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* Player::getHandler() const {
    return mPlayer.get();
}

void Player::updateContext(Context* context) {
    mSequenceView->setChannelEditor(context->channelEditor());
    mPlaylist->setContext(context);
}

void Player::changePosition(timestamp_t timestamp) {
    // event comes from trackbar
    mPlayer->set_position(timestamp);
}

void Player::changeLower(timestamp_t timestamp) {
    // event comes from trackbar
    mPlayer->set_lower(timestamp);
    mSequenceView->setLower(timestamp);
}

void Player::changeUpper(timestamp_t timestamp) {
    // event comes from trackbar
    mPlayer->set_upper(timestamp);
    mSequenceView->setUpper(timestamp);
}

void Player::changeDistorsion(double distorsion) {
    // event comes from tempoview, don't need to update
    mPlayer->set_distorsion(distorsion);
    mTracker->setDistorsion(distorsion);
    mSequenceView->distordedClock().setDistorsion(distorsion);
}

void Player::setTrackFilter(Handler* handler) {
    mSequenceView->setTrackFilter(handler);
}

void Player::setNextSequence(bool play, int offset) {
    resetSequence();
    if (mMediaView->isSingle() && mPlaylist->isLoaded()) {
        if (mMediaView->isLooping()) {
            if (play)
                playCurrentSequence();
            return;
        }
    } else {
        auto nextSequence = mPlaylist->loadRelative(offset, mMediaView->isLooping());
        if (!nextSequence.sequence.empty()) {
            setSequence(nextSequence, play);
            return;
        }
    }
    TRACE_DEBUG("no more sequence to play");
}

void Player::updatePosition() {
    timestamp_t pos = mPlayer->position();
    mTempoView->updateTimestamp(pos);
    mTracker->updateTimestamp(pos);
}

void Player::refreshPosition() {
    updatePosition();
    if (mPlayer->is_completed()) {
        playNextSequence();
    } else if (!mPlayer->is_playing()) { // stopped by an event
        if (mIsPlaying) {
            mIsPlaying = false;
            mIsStepping = false;
            mRefreshTimer->stop();
            mTempoView->clearTempo();
        }
        mPlaylist->setCurrentStatus(STOPPED);
    } else if (mIsStepping && mPlayer->position() >= mNextStep) {
        pauseSequence();
    }
}

void Player::setSequence(const NamedSequence& sequence, bool play) {
    resetSequence();
    window()->setWindowTitle(sequence.name);
    mPlayer->set_sequence(sequence.sequence);
    mTempoView->setSequence(sequence.sequence);
    mSequenceView->setSequence(sequence.sequence, mPlayer->lower(), mPlayer->upper());
    mTracker->setSequence(sequence.sequence, mPlayer->lower(), mPlayer->upper());
    if (play)
        playCurrentSequence();
}

void Player::saveSequence() {
    QString filename = context()->pathRetriever("midi")->getWriteFile(this);
    if (filename.isNull())
        return;
    Sequence sequence = mPlayer->sequence();
    if (sequence.empty()) {
        QMessageBox::critical(this, QString(), "Empty sequence");
    } else {
        size_t bytes = dumping::write_file(sequence.to_file(), filename.toStdString(), true);
        if (bytes == 0) {
            QMessageBox::critical(this, QString(), "Unable to write sequence");
        } else {
            QMessageBox::information(this, QString(), "Sequence saved");
        }
    }
}

void Player::launch(QTableWidgetItem* item) {
    auto sequence = mPlaylist->loadRow(item->row());
    if (!sequence.sequence.empty()) {
        setSequence(sequence, true);
    } else { /// @todo get reason from model
        QMessageBox::critical(this, QString(), "Can't read MIDI File");
    }
}

void Player::onPositionSelected(timestamp_t timestamp, Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        changePosition(timestamp);
        updatePosition();
    } else {
        mTracker->addCustomMarker(timestamp);
    }
}

void Player::playSequence(bool resetStepping) {
    if (!mIsPlaying) {
        if (mPlaylist->isLoaded())
            playCurrentSequence(resetStepping);
        else
            playNextSequence();
    }
}

void Player::playCurrentSequence(bool resetStepping) {
    if (!mIsPlaying) {
        if (resetStepping)
            mIsStepping = false;
        /// @warning why ??
        // mPlayer->forward_message({Event::controller(all_channels, all_notes_off_controller), mPlayer});
        if (mPlayer->start_playing(false)) {
            mIsPlaying = true;
            mRefreshTimer->start();
            mPlaylist->setCurrentStatus(PLAYING);
        }
    }
}

void Player::pauseSequence() {
    if (mIsPlaying) {
        mIsPlaying = false;
        mIsStepping = false;
        mPlayer->stop_playing(Event::controller(all_channels, controller_ns::all_sound_off_controller));
        mRefreshTimer->stop();
        mTempoView->clearTempo();
        mPlaylist->setCurrentStatus(PAUSED);
    }
}

void Player::resetSequence() {
    mPlayer->stop_playing(true);
    mPlayer->forward_message({Event::reset(), mPlayer.get()}); // force reset
    if (mIsPlaying) {
        mIsPlaying = false;
        mIsStepping = false;
        mRefreshTimer->stop();
        mTempoView->clearTempo();
    }
    mPlaylist->setCurrentStatus(STOPPED);
    updatePosition();
}

void Player::playNextSequence() {
    setNextSequence(true, 1);
}

void Player::playLastSequence() {
    setNextSequence(true, -1);
}

void Player::stepForward() {
    mIsStepping = false;
    timestamp_t pos = mPlayer->position();
    const Sequence& seq = mPlayer->sequence();
    auto first = seq.begin(), last = seq.end();
    for (auto it = std::lower_bound(first, last, pos) ; it != last ; ++it) {
        if (it->event.family() == family_t::note_on) {
            mNextStep = it->timestamp;
            mIsStepping = true;
            break;
        }
    }
    playCurrentSequence(false);
}
