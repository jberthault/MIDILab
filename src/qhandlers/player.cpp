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

#include <QHeaderView>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QMenu>
#include "qhandlers/player.h"
#include "handlers/trackfilter.h"

namespace {

constexpr range_t<double> distorsionRange = {0., 4.};

QString stringFromDistorsion(double distorsion) {
    return QString::number(decay_value<int>(100*distorsion)) + "%";
}

auto findCodecs() {
    QList<QTextCodec*> codecs;
    for (auto mib : QTextCodec::availableMibs()) {
        auto* codec = QTextCodec::codecForMib(mib);
        if (!codecs.contains(codec))
            codecs.append(codec);
    }
    return codecs;
}

bool isValid(const SharedSequence& sequence) {
    return sequence && !sequence->empty();
}

const auto& clockFromSequence(const SharedSequence& sequence) {
    static const Clock defaultClock;
    return sequence ? sequence->clock() : defaultClock;
}

const auto& itemFromSequence(const SharedSequence& sequence, size_t index) {
    static const TimedEvent defaultItem{0.};
    return sequence ? (*sequence)[index] : defaultItem;
}

auto qtimeFromDuration(const Clock::duration_type& time, double distorsion) {
    /// @note no std::duration_cast to avoid rounding errors
    return distorsion != 0. ? QTime{0, 0}.addMSecs(decay_value<int>(time.count() * 1.e-3 / distorsion)) : QTime{};
}

auto qtimeFromTimestamp(timestamp_t timestamp, const SharedSequence& sequence, double distorsion) {
    return qtimeFromDuration(clockFromSequence(sequence).timestamp2time(timestamp), distorsion);
}

auto qtimeFromTimestampRange(const range_t<timestamp_t>& ts, const SharedSequence& sequence, double distorsion) {
    return qtimeFromDuration(clockFromSequence(sequence).timestamp2time(ts.max) - clockFromSequence(sequence).timestamp2time(ts.min), distorsion);
}

auto qtimeRangeToTimestamp(const range_t<QTime>& ts, const SharedSequence& sequence, double distorsion) {
    return clockFromSequence(sequence).time2timestamp(Clock::duration_type{static_cast<double>(ts.min.msecsTo(ts.max)) * 1.e3 * distorsion});
}

auto qtimeToTimestamp(const QTime& time, const SharedSequence& sequence, double distorsion) {
    return qtimeRangeToTimestamp({QTime{0, 0}, time}, sequence, distorsion);
}

const QString timeFormat{"mm:ss.zzz"};

auto qstringFromTimestamp(timestamp_t timestamp, const SharedSequence& sequence, double distorsion) {
    return qtimeFromTimestamp(timestamp, sequence, distorsion).toString(timeFormat);
}

void showSystemTrayMessage(QSystemTrayIcon* systemTrayIcon, const QString& title, const QString& msg, const QIcon& icon, int msecs) {
#if QT_VERSION_MAJOR > 5 || (QT_VERSION_MAJOR == 5 && QT_VERSION_MINOR >= 9)
        systemTrayIcon->showMessage(title, msg, icon, msecs);
#else
        systemTrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, msecs);
#endif
}

}

//==============
// SequenceView
//==============

SequenceViewTrackItem::SequenceViewTrackItem(track_t track, SequenceView* view, QTreeWidget* parent) :
    QTreeWidgetItem{parent}, mTrack{track}, mView{view} {

}

track_t SequenceViewTrackItem::track() const {
    return mTrack;
}

void SequenceViewTrackItem::setTrack(track_t track) {
    mTrack = track;
}

SequenceView* SequenceViewTrackItem::view() const {
    return mView;
}

void SequenceViewTrackItem::setTrackNames(const QByteArrayList& names) {
    auto trackFont = font(0);
    if (names.isEmpty()) {
        trackFont.setWeight(QFont::Normal);
        trackFont.setItalic(true);
        mRawText.clear();
        setText(0, QString{"Track #%1"}.arg(mTrack+1));
    } else {
        trackFont.setWeight(QFont::Bold);
        trackFont.setItalic(false);
        mRawText = names.join(" / ");
        updateEncoding();
    }
    setFont(0, trackFont);
}

void SequenceViewTrackItem::updateEncoding() {
    if (!mRawText.isNull())
        setText(0, view()->codec()->toUnicode(mRawText));
}

SequenceViewItem::SequenceViewItem(size_t index, SequenceViewTrackItem* parent) :
    QTreeWidgetItem{parent}, mIndex{index} {

}

SequenceView* SequenceViewItem::view() const {
    return static_cast<SequenceViewTrackItem*>(parent())->view();
}

size_t SequenceViewItem::index() const {
    return mIndex;
}

void SequenceViewItem::setIndex(size_t index) {
    mIndex = index;
    emitDataChanged();
}

void SequenceViewItem::updateEncoding() {
    if (view()->timedEvent(mIndex).event.is(families_t::string()))
        emitDataChanged();
}

void SequenceViewItem::updateVisibiliy(families_t families, channels_t channels, const range_t<timestamp_t>& limits) {
    const auto& item = view()->timedEvent(mIndex);
    const bool familiesVisible = item.event.is(families);
    const bool channelsVisible = !item.event.is(families_t::voice()) || item.event.channels().any(channels);
    const bool boundsVisible = limits.min <= item.timestamp && item.timestamp <= limits.max;
    setHidden(!(familiesVisible && channelsVisible && boundsVisible));
}

QVariant SequenceViewItem::data(int column, int role) const {
    const auto& item = view()->timedEvent(mIndex);
    if (column == 0 && role == Qt::ToolTipRole)
        return qstringFromTimestamp(item.timestamp, view()->sequence(), view()->distorsion());
    if (column == 0 && role == Qt::DisplayRole)
        return QString::number(decay_value<long>(item.timestamp));
    if (column == 1 && role == Qt::DisplayRole)
        return ChannelsSelector::channelsToStringList(item.event.channels()).join(' ');
    if (column == 2 && role == Qt::DisplayRole)
        return eventName(item.event);
    if (column == 3 && role == Qt::DisplayRole) {
        QByteArray rawText = QByteArray::fromStdString(item.event.description());  // Qt 5.4+ only
        rawText.replace("\n", "\\n");
        rawText.replace("\r", "\\r");
        rawText.replace("\t", "\\t");
        if (item.event.is(families_t::string())) {
            return view()->codec()->toUnicode(rawText);
        } else {
            return rawText;
        }
    }
    return QTreeWidgetItem::data(column, role);
}

SequenceView::SequenceView(QWidget *parent) : QWidget{parent} {

    mSequenceUpdater = new QTimer{this};
    connect(mSequenceUpdater, &QTimer::timeout, this, &SequenceView::onSequenceUpdate);

    mTreeWidget = new QTreeWidget{this};
    mTreeWidget->setAlternatingRowColors(true);
    mTreeWidget->setHeaderLabels(QStringList{} << "Timestamp" << "Channel" << "Type" << "Data");
    mTreeWidget->header()->setDefaultAlignment(Qt::AlignCenter);
    mTreeWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    mTreeWidget->viewport()->installEventFilter(this);
    mTreeWidget->setColumnWidth(0, 90); // ideal width for timestamp
    mTreeWidget->setColumnWidth(1, 60); // ideal width for channel
    connect(mTreeWidget, &QTreeWidget::itemChanged, this, &SequenceView::onItemChange);
    connect(mTreeWidget, &QTreeWidget::itemDoubleClicked, this, &SequenceView::onItemDoubleClick);

    mFamilySelectorButton = new QPushButton{"Types", this};
    mFamilySelectorButton->setToolTip("Filter by type");
    connect(mFamilySelectorButton, &QPushButton::clicked, this, &SequenceView::onFamilyFilterClick);

    mFamilySelector = new FamilySelector{this};
    mFamilySelector->setFamilies(families_t::full());
    mFamilySelector->setWindowFlags(Qt::Dialog);
    mFamilySelector->setVisible(false);
    connect(mFamilySelector, &FamilySelector::familiesChanged, this, &SequenceView::onFamiliesChanged);

    mChannelSelectorButton = new QPushButton{"Channels", this};
    mChannelSelectorButton->setToolTip("Filter by channel");
    connect(mChannelSelectorButton, &QPushButton::clicked, this, &SequenceView::onChannelFilterClick);

    mChannelsSelector = new ChannelsSelector{this};
    mChannelsSelector->setChannels(channels_t::full());
    mChannelsSelector->setWindowFlags(Qt::Dialog);
    mChannelsSelector->setVisible(false);
    connect(mChannelsSelector, &ChannelsSelector::channelsChanged, this, &SequenceView::onChannelsChanged);

    auto* codecSelector = new QComboBox{this};
    codecSelector->setToolTip("Text Encoding");
    for (auto* codec : findCodecs())
        codecSelector->addItem(codec->name());
    connect(codecSelector, SIGNAL(currentIndexChanged(QString)), this, SLOT(onCodecChange(QString)));
    onCodecChange(codecSelector->currentText());

    auto* expandButton = new ExpandButton{mTreeWidget};
    auto* collapseButton = new CollapseButton{mTreeWidget};

    setLayout(make_vbox(margin_tag{0}, mTreeWidget, make_hbox(stretch_tag{}, mChannelSelectorButton, mFamilySelectorButton, codecSelector, expandButton, collapseButton)));
}

const TimedEvent& SequenceView::timedEvent(size_t index) const {
    return itemFromSequence(mSequence, index);
}

void SequenceView::setChannelEditor(ChannelEditor* channelEditor) {
    /// @todo disconnect last
    mChannelEditor = channelEditor;
    mChannelsSelector->setChannelEditor(channelEditor);
    if (channelEditor)
        connect(mChannelEditor, &ChannelEditor::colorChanged, this, &SequenceView::onColorChange);
}

void SequenceView::setTrackFilter(Handler* handler) {
    mTrackFilter = handler;
}

void SequenceView::setCodec(QTextCodec* codec) {
    Q_ASSERT(codec);
    mCodec = codec;
    // prevent itemChanged, we want it to be emitted for checkstate only :(
    QSignalBlocker guard{mTreeWidget};
    Q_UNUSED(guard);
    for (auto* trackItem : makeChildRange(mTreeWidget->invisibleRootItem())) {
        static_cast<SequenceViewTrackItem*>(trackItem)->updateEncoding();
        for (auto* item : makeChildRange(trackItem))
            static_cast<SequenceViewItem*>(item)->updateEncoding();
    }
}

void SequenceView::setSequence(const SharedSequence& sequence) {
    Q_ASSERT(sequence);
    setUpdatesEnabled(false);
    // prevent signals
    QSignalBlocker guard{this};
    Q_UNUSED(guard);
    // clean previous sequence
    cleanSequence();
    // register sequence
    mSequence = sequence;
    mLimits = {0, sequence->last_timestamp()};
    // reenable all tracks
    if (mTrackFilter)
        mTrackFilter->send_message(TrackFilter::enable_all_ext());
    // collect tracks data
    std::unordered_map<track_t, channels_t> trackChannels;
    std::unordered_map<track_t, QByteArrayList> trackNames;
    for (const auto& item : *mSequence) {
        if (item.event.is(families_t::voice()))
            trackChannels[item.event.track()] |= item.event.channels();
        else if (item.event.is(family_t::track_name))
            trackNames[item.event.track()] << QByteArray::fromStdString(item.event.description());
    }
    // make track items
    for (track_t track : mSequence->tracks()) {
        auto* trackItem = makeTrackItem(track);
        trackItem->setFirstColumnSpanned(true);
        // track text
        trackItem->setTrackNames(trackNames[track]);
        // track filter enabled
        if (mTrackFilter)
            trackItem->setCheckState(0, Qt::Checked);
        // background name
        const auto channels = trackChannels[track];
        trackItem->setData(0, Qt::UserRole, channels.to_integral());
        setItemBackground(trackItem, channels);
    }
    // start filling events
    mSequenceUpdater->start();
}

void SequenceView::cleanSequence() {
    mEventCount = 0;
    auto trackItems = mTreeWidget->invisibleRootItem()->takeChildren();
    for(auto* trackItem : trackItems) {
        mTrackReserve.emplace_back(static_cast<SequenceViewTrackItem*>(trackItem));
        auto eventItems = trackItem->takeChildren();
        for(auto* eventItem : eventItems)
            mEventReserve.emplace_back(static_cast<SequenceViewItem*>(eventItem));
    }
    mSequence.reset();
}

void SequenceView::setLower(timestamp_t timestamp) {
    mLimits.min = timestamp;
    updateItemsVisibility();
}

void SequenceView::setUpper(timestamp_t timestamp) {
    mLimits.max = timestamp;
    updateItemsVisibility();
}

bool SequenceView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease)
        mLastButton = static_cast<QMouseEvent*>(event)->button();
    return QWidget::eventFilter(watched, event);
}

void SequenceView::onSequenceUpdate() {
    Q_ASSERT(mSequence);
    const auto n = std::min(mEventCount + 64, mSequence->size());
    while (mEventCount < n)
        updateItemVisibility(makeEventItem(mEventCount++));
    if (mEventCount == mSequence->size()) {
        mSequenceUpdater->stop();
        setUpdatesEnabled(true);
    }
}

void SequenceView::onColorChange(channel_t channel, const QColor& /*color*/) {
    for (auto* trackItem : makeChildRange(mTreeWidget->invisibleRootItem())) {
        const auto channels = channels_t::from_integral(trackItem->data(0, Qt::UserRole).toUInt());
        if (channels.test(channel))
            setItemBackground(trackItem, channels);
    }
}

void SequenceView::onItemChange(QTreeWidgetItem* item, int /*column*/) {
    // called when root checkbox is clicked
    auto* trackItem = dynamic_cast<SequenceViewTrackItem*>(item);
    if (trackItem && mTrackFilter) {
        const auto track = trackItem->track();
        const bool checked = item->checkState(0) == Qt::Checked;
        mTrackFilter->send_message(checked ? TrackFilter::enable_ext(track) : TrackFilter::disable_ext(track));
    }
}

void SequenceView::onItemDoubleClick(QTreeWidgetItem* item, int column) {
    if (column == 0) // timestamp column
        if (auto* eventItem = dynamic_cast<SequenceViewItem*>(item))
            emit positionSelected(timedEvent(eventItem->index()).timestamp, mLastButton);
}

void SequenceView::onFamilyFilterClick() {
    mFamilySelector->setVisible(!mFamilySelector->isVisible());
}

void SequenceView::onChannelFilterClick() {
    mChannelsSelector->setVisible(!mChannelsSelector->isVisible());
}

void SequenceView::onFamiliesChanged(families_t families) {
    if (families.all(families_t::standard()))
        mFamilySelectorButton->setText("Types");
    else
        mFamilySelectorButton->setText("Types*");
    updateItemsVisibility();
}

void SequenceView::onChannelsChanged(channels_t channels) {
    if (channels == channels_t::full())
        mChannelSelectorButton->setText("Channels");
    else
        mChannelSelectorButton->setText("Channels*");
    updateItemsVisibility();
}

void SequenceView::onCodecChange(const QString& name) {
    setCodec(QTextCodec::codecForName(name.toLocal8Bit()));
}

SequenceViewTrackItem* SequenceView::itemForTrack(track_t track) const {
    for (auto* item : makeChildRange(mTreeWidget->invisibleRootItem())) {
        auto* trackItem = static_cast<SequenceViewTrackItem*>(item);
        if (trackItem->track() == track)
            return trackItem;
    }
    return nullptr;
}

SequenceViewTrackItem* SequenceView::makeTrackItem(track_t track) {
    if (mTrackReserve.empty())
        return new SequenceViewTrackItem{track, this, mTreeWidget};
    auto* result = mTrackReserve.back().release();
    mTreeWidget->invisibleRootItem()->addChild(result);
    result->setTrack(track);
    mTrackReserve.pop_back();
    return result;
}

SequenceViewItem* SequenceView::makeEventItem(size_t index) {
    if (auto* trackItem = itemForTrack(timedEvent(index).event.track())) {
        if (mEventReserve.empty())
            return new SequenceViewItem{index, trackItem};
        auto* result = mEventReserve.back().release();
        trackItem->addChild(result); // assuming index is always larger than the previous one
        result->setIndex(index);
        mEventReserve.pop_back();
        return result;
    }
    return nullptr;
}

void SequenceView::updateItemsVisibility() {
    for (auto* trackItem : makeChildRange(mTreeWidget->invisibleRootItem()))
        for (auto* item : makeChildRange(trackItem))
            updateItemVisibility(static_cast<SequenceViewItem*>(item));
}

void SequenceView::updateItemVisibility(SequenceViewItem* item) {
    item->updateVisibiliy(mFamilySelector->families(), mChannelsSelector->channels(), mLimits);
}

void SequenceView::setItemBackground(QTreeWidgetItem* item, channels_t channels) {
    if (mChannelEditor)
        item->setBackground(0, mChannelEditor->brush(channels, Qt::Horizontal));
}

//===============
// PlaylistTable
//===============

FileItem::FileItem(const QFileInfo& fileInfo) : PlaylistItem{}, mFileInfo{fileInfo} {
    mFileInfo = fileInfo;
    setText(mFileInfo.completeBaseName());
    setToolTip(mFileInfo.absoluteFilePath());
}

const QFileInfo& FileItem::fileInfo() const {
    return mFileInfo;
}

NamedSequence FileItem::loadSequence() {
    auto file = dumping::read_file(mFileInfo.absoluteFilePath().toLocal8Bit().constData());
    return {std::make_shared<Sequence>(Sequence::from_file(std::move(file))), text()};
}

WriterItem::WriterItem(SequenceWriter* handler) : PlaylistItem{}, mHandler{handler} {
    Q_ASSERT(handler);
    setText(handlerName(mHandler));
    setToolTip("Recorder Handler");
    auto f = font();
    f.setItalic(true);
    setFont(f);
}

SequenceWriter* WriterItem::handler() {
    return mHandler;
}

NamedSequence WriterItem::loadSequence() {
    return {std::make_shared<Sequence>(mHandler->load_sequence()), handlerName(mHandler)};
}

PlaylistTable::PlaylistTable(QWidget* parent) : QTableWidget{0, 2, parent} {

    setHorizontalHeaderLabels(QStringList{} << "Filename" << "Duration");

    setEditTriggers(NoEditTriggers);
    setAlternatingRowColors(true);
    setSelectionMode(QTableWidget::ExtendedSelection);
    setSelectionBehavior(QTableWidget::SelectRows);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    verticalHeader()->setDefaultSectionSize(20);

    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(false);
    setDragDropMode(QTableWidget::DragDrop);
    setDragDropOverwriteMode(false);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &PlaylistTable::customContextMenuRequested, this, &PlaylistTable::showMenu);
    auto* trigger = new MenuDefaultTrigger{this};
    mMenu = new QMenu{this};
    mMenu->setToolTipsVisible(true);
    auto* browseMenu = mMenu->addMenu(QIcon{":/data/magnifying-glass.svg"}, "Browse");
    auto* browseFilesAction = browseMenu->addAction(QIcon{":/data/file.svg"}, "Files", this, SLOT(browseFiles()));
    browseMenu->addAction(QIcon{":/data/folder.svg"}, "Dirs", this, SLOT(browseDirsShallow()));
    browseMenu->addAction(QIcon{":/data/briefcase.svg"}, "Dirs (Recursive)", this, SLOT(browseDirsDeep()));
    browseMenu->setDefaultAction(browseFilesAction);
    browseMenu->installEventFilter(trigger);
    mMenu->addAction(QIcon{":/data/media-record.svg"}, "Import Recorder", this, SLOT(browseRecorders()));
    mMenu->addSeparator();
    mMenu->addAction(QIcon{":/data/random.svg"}, "Shuffle", this, SLOT(shuffle()));
    mMenu->addAction(QIcon{":/data/sort-ascending.svg"}, "Sort Ascending", this, SLOT(sortAscending()));
    mMenu->addAction(QIcon{":/data/sort-descending.svg"}, "Sort Descending", this, SLOT(sortDescending()));
    mMenu->addSeparator();
    mMenu->addAction(QIcon{":/data/delete.svg"}, "Discard", this, SLOT(removeSelection()));
    mMenu->addAction(QIcon{":/data/trash.svg"}, "Discard All", this, SLOT(removeAllRows()));

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
    for (int i=0 ; i < rowCount() ; i++)
        if (auto* fileItem = dynamic_cast<FileItem*>(item(i, 0)))
            result.append(fileItem->fileInfo().absoluteFilePath());
    return result;
}

size_t PlaylistTable::addPaths(const QStringList& paths) {
    size_t count = 0;
    for (const auto& path : paths)
        count += addPath(path);
    return count;
}

size_t PlaylistTable::addPaths(QList<QUrl> urls) {
    qSort(urls);
    size_t count = 0;
    for (const auto& url : urls)
        count += addPath(url);
    return count;
}

size_t PlaylistTable::addPath(const QUrl& url) {
    return addPath(url.toLocalFile());
}

size_t PlaylistTable::addPath(const QString& path) {
    return addPath(QFileInfo{path});
}

size_t PlaylistTable::addPath(const QFileInfo& fileInfo) {
    if (fileInfo.isDir())
        return addDir(fileInfo);
    if (fileInfo.isFile())
        return addFile(fileInfo);
    TRACE_WARNING("can't find file " << fileInfo.absoluteFilePath());
    return 0;
}

size_t PlaylistTable::addFile(const QFileInfo& fileInfo) {
    insertItem(new FileItem{fileInfo});
    return 1;
}

size_t PlaylistTable::addDir(const QFileInfo& fileInfo, bool recurse) {
    static const auto nameFilters = QStringList{} << "*.mid" << "*.midi" << "*.kar";
    QDir dir{fileInfo.filePath()};
    size_t count = 0;
    for(const auto& info : dir.entryInfoList(nameFilters, QDir::Files))
        count += addFile(info);
    if (recurse)
        for(const auto& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            count += addDir(info, true);
    return count;
}

void PlaylistTable::setCurrentStatus(SequenceStatus status) {
    if (mCurrentItem) {
        switch (status) {
        case NO_STATUS : mCurrentItem->setIcon(QIcon{}); break;
        case PLAYING : mCurrentItem->setIcon(QIcon{":/data/media-play.svg"}); break;
        case PAUSED : mCurrentItem->setIcon(QIcon{":/data/media-pause.svg"}); break;
        case STOPPED : mCurrentItem->setIcon(QIcon{":/data/media-stop.svg"}); break;
        }
    }
}

bool PlaylistTable::isLoaded() const {
    return mCurrentItem;
}

NamedSequence PlaylistTable::loadRow(int row) {
    NamedSequence namedSequence;
    if (auto* playlistItem = dynamic_cast<PlaylistItem*>(item(row, 0))) {
        namedSequence = playlistItem->loadSequence();
        if (isValid(namedSequence.sequence)) {
            // change status
            setCurrentStatus(NO_STATUS);
            mCurrentItem = playlistItem;
            // set duration
            item(row, 1)->setText(qstringFromTimestamp(namedSequence.sequence->last_timestamp(), namedSequence.sequence, 1.));
            // ensure line is visible
            scrollToItem(playlistItem);
        } else {
            item(row, 1)->setText("\u00d8");
        }
    }
    return namedSequence;
}

NamedSequence PlaylistTable::loadRelative(int offset, bool wrap) {
    NamedSequence namedSequence;
    const int rows = rowCount(); // number of rows available
    int row = mCurrentItem ? mCurrentItem->row() + offset : 0; // next row to test
    if (wrap) { // with wrapping, we check all available rows (the current one may be reloaded)
        for (int i=0 ; i < rows ; ++i, row += offset) {
            namedSequence = loadRow(safe_modulo(row, rows));
            if (isValid(namedSequence.sequence))
                break;
        }
    } else { // without wrapping, we continue until the row is no longer valid
        for ( ; 0 <= row && row < rows ; row += offset) {
            namedSequence = loadRow(row);
            if (isValid(namedSequence.sequence))
                break;
        }
    }
    return namedSequence;
}

void PlaylistTable::setContext(Context* context) {
    mContext = context;
    connect(context, &Context::handlerRemoved, this, &PlaylistTable::removeHandler);
    connect(context, &Context::handlerRenamed, this, &PlaylistTable::renameHandler);
}

void PlaylistTable::browseFiles() {
    if (addPaths(mContext->pathRetrieverPool()->get("midi")->getReadFiles(this)))
        scrollToBottom();
}

void PlaylistTable::browseDirsShallow() {
    browseDirs(false);
}

void PlaylistTable::browseDirsDeep() {
    browseDirs(true);
}

void PlaylistTable::browseDirs(bool recursive) {
    const auto dir = mContext->pathRetrieverPool()->get("midi")->getReadDir(this);
    setUpdatesEnabled(false);
    if (!dir.isNull() && addDir(QFileInfo{dir}, recursive) != 0)
        scrollToBottom();
    setUpdatesEnabled(true);
}

void PlaylistTable::browseRecorders() {
    // get all sequence writers
    std::vector<Handler*> handlers;
    for (const auto& proxy : mContext->handlerProxies())
        if (dynamic_cast<SequenceWriter*>(proxy.handler()))
            handlers.push_back(proxy.handler());
    if (handlers.empty()) {
        QMessageBox::information(this, {}, "No recorder available");
        return;
    }
    // make and fill a selector
    auto* selector = new HandlerSelector{this};
    selector->setWindowTitle("Select the recorder to import");
    for (auto* handler : handlers)
        selector->insertHandler(handler);
    // run it
    DialogContainer ask{selector, this};
    if (ask.exec() == QDialog::Accepted) {
        if (auto* sw = dynamic_cast<SequenceWriter*>(selector->currentHandler())) {
            insertItem(new WriterItem{sw});
            scrollToBottom();
        } else {
            QMessageBox::warning(this, {}, "No recorder selected");
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
        auto* writerItem = dynamic_cast<WriterItem*>(item(row, 0));
        if (writerItem && writerItem->handler() == handler)
            writerItem->setText(handlerName(handler));
    }
}

void PlaylistTable::removeHandler(Handler* handler) {
    for (int row=0 ; row < rowCount() ; ) {
        auto* writerItem = dynamic_cast<WriterItem*>(item(row, 0));
        if (writerItem && writerItem->handler() == handler)
            removeRow(row);
        else
            ++row;
    }
}

void PlaylistTable::showMenu(const QPoint& point) {
    mMenu->exec(mapToGlobal(point));
}

QStringList PlaylistTable::mimeTypes() const {
    return QTableWidget::mimeTypes() << "text/uri-list";
}

void PlaylistTable::dropEvent(QDropEvent* event) {
    // drop from filesystem, sort urls as they are never ordered
    if (event->mimeData()->hasFormat("text/uri-list")) {
        if (addPaths(event->mimeData()->urls()))
            scrollToBottom();
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
    const auto index = indexAt(pos);
    // append item if it is dropped in the viewport
    if (!index.isValid())
        return rowCount();
    const auto itemRect = visualRect(index);
    const auto relPos = pos - itemRect.topLeft();
    // formula to get vertical position (match the drop indicator)
    const bool isBefore = relPos.y() < (itemRect.height()-1)/2;
    return isBefore ? index.row() : index.row() + 1;
}

//==========
// Trackbar
//==========

MarkerKnob::MarkerKnob(QBoxLayout::Direction direction) : ArrowKnob{direction} {
    setMovable(false);
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

TrackedKnob::TrackedKnob(QWidget* parent) : QTimeEdit{parent} {
    connect(this, &TrackedKnob::timeChanged, this, &TrackedKnob::onTimeChange);
}

void TrackedKnob::setKnob(Knob* knob) {
    mKnob = knob;
    mKnob->setToolTip(toolTip());
    mKnob->xScale().margins = {8., 8.};
    mKnob->yScale().pin(.5);
    connect(mKnob, &Knob::knobMoved, this, &TrackedKnob::onMove);
    connect(mKnob, &Knob::knobPressed, this, &TrackedKnob::onPress);
    connect(mKnob, &Knob::knobReleased, this, &TrackedKnob::onRelease);
    connect(mKnob, &Knob::knobDoubleClicked, this, &TrackedKnob::onClick);
}

bool TrackedKnob::isReversed() const {
    return mIsReversed;
}

void TrackedKnob::setReversed(bool reversed) {
    mIsReversed = reversed;
    updateTime();
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
    mDistorsion = distorsion;
    updateMaximumTime();
    updateTime();
}

void TrackedKnob::initialize(const SharedSequence& sequence, timestamp_t timestamp, timestamp_t maxTimestamp) {
    mSequence = sequence;
    mMaxTimestamp = maxTimestamp;
    mTimestamp = timestamp;
    updateMaximumTime();
    updateTime();
    updateHandle();
}

QTime TrackedKnob::toTime(timestamp_t timestamp) const {
    if (mIsReversed)
        return qtimeFromTimestampRange({timestamp, mMaxTimestamp}, mSequence, mDistorsion);
    return qtimeFromTimestamp(timestamp, mSequence, mDistorsion);
}

timestamp_t TrackedKnob::toTimestamp(const QTime& time) const {
    if (mIsReversed)
        return qtimeRangeToTimestamp({time, maximumTime()}, mSequence, mDistorsion);
    return qtimeToTimestamp(time, mSequence, mDistorsion);
}

void TrackedKnob::onMove(qreal xvalue, qreal /*yvalue*/) {
    mTimestamp = xvalue * mMaxTimestamp;
    setTime(toTime(mTimestamp));
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
    mTimestamp = toTimestamp(time);
    updateHandle();
    emit timestampChanged(mTimestamp);
}

void TrackedKnob::updateTime() {
    QSignalBlocker guard{this};
    setTime(toTime(mTimestamp));
}

void TrackedKnob::updateMaximumTime() {
    QSignalBlocker guard{this};
    setMaximumTime(qtimeFromTimestamp(mMaxTimestamp, mSequence, mDistorsion));
}

void TrackedKnob::updateHandle() {
    if (mKnob) {
        mKnob->xScale().value = mTimestamp / mMaxTimestamp;
        mKnob->moveToFit();
    }
}

Trackbar::Trackbar(QWidget* parent) : QWidget{parent} {

    // Position
    mPositionKnob = new ParticleKnob{7.};
    mPositionKnob->setZValue(1.);
    mPositionEdit = new TrackedKnob{this};
    mPositionEdit->setDisplayFormat(timeFormat);
    mPositionEdit->setToolTip("Position");
    mPositionEdit->setKnob(mPositionKnob);
    connect(mPositionEdit, &TrackedKnob::leftClicked, this, &Trackbar::onPositionLeftClick);
    connect(mPositionEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mPositionEdit, &TrackedKnob::timestampChanged, this, &Trackbar::positionChanged);

    // Lower Position
    mLowerKnob = new BracketKnob{QBoxLayout::LeftToRight};
    mLowerEdit = new TrackedKnob{this};
    mLowerEdit->setDisplayFormat("[ " + timeFormat);
    mLowerEdit->setToolTip("Lower Limit");
    mLowerEdit->setKnob(mLowerKnob);
    connect(mLowerEdit, &TrackedKnob::leftClicked, this, &Trackbar::onLowerLeftClick);
    connect(mLowerEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mLowerEdit, &TrackedKnob::timestampChanged, this, &Trackbar::lowerChanged);

    // Upper position
    mUpperKnob = new BracketKnob{QBoxLayout::RightToLeft};
    mUpperEdit= new TrackedKnob{this};
    mUpperEdit->setDisplayFormat(timeFormat + " ]");
    mUpperEdit->setToolTip("Upper Limit");
    mUpperEdit->setKnob(mUpperKnob);
    mUpperKnob->xScale().value = 1.;
    connect(mUpperEdit, &TrackedKnob::leftClicked, this, &Trackbar::onUpperLeftClick);
    connect(mUpperEdit, &TrackedKnob::rightClicked, this, &Trackbar::addCustomMarker);
    connect(mUpperEdit, &TrackedKnob::timestampChanged, this, &Trackbar::upperChanged);

    // Last Position
    mLastEdit = new TrackedKnob{this};
    mLastEdit->setReadOnly(true);
    mLastEdit->setAlignment(Qt::AlignHCenter);
    mLastEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    mLastEdit->setDisplayFormat(timeFormat);
    mLastEdit->setToolTip("Duration");

    // View
    mKnobView = new KnobView{this};
    mKnobView->setFixedHeight(31);
    mKnobView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mKnobView->insertKnob(mPositionKnob);
    mKnobView->insertKnob(mLowerKnob);
    mKnobView->insertKnob(mUpperKnob);

    // Menu

    auto* buttonMenu = new QMenu{this};

    auto* reverseAction = buttonMenu->addAction(QIcon{":/data/transfer.svg"}, "Reverse Time");
    reverseAction->setCheckable(true);
    connect(reverseAction, &QAction::toggled, mPositionEdit, &TrackedKnob::setReversed);

    auto* button = new QToolButton{this};
    button->setAutoRaise(true);
    button->setIcon(QIcon{":/data/menu.svg"});
    button->setPopupMode(QToolButton::InstantPopup);
    button->setMenu(buttonMenu);

    setLayout(make_vbox(margin_tag{0}, mKnobView, make_hbox(margin_tag{0}, button, mLowerEdit, mPositionEdit, mUpperEdit, mLastEdit)));
}

const QBrush& Trackbar::knobColor() const {
    return mPositionKnob->brush();
}

void Trackbar::setKnobColor(const QBrush& brush) {
    mPositionKnob->setBrush(brush);
    for (auto* knob : mMarkerKnobs)
        knob->setBrush(brush);
    for (auto* knob : mCustomMarkerKnobs)
        knob->setBrush(brush);
    auto pen = mLowerKnob->pen();
    pen.setBrush(brush);
    mLowerKnob->setPen(pen);
    mUpperKnob->setPen(pen);
}

int Trackbar::knobWidth() const {
    return mLowerKnob->pen().width();
}

void Trackbar::setKnobWidth(int width) {
    auto pen = mLowerKnob->pen();
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
    auto* knob = addMarker(isCustom);
    knob->setTimestamp(timestamp);
    knob->xScale().value = timestamp / mLastEdit->timestamp();
    knob->setToolTip(tooltip);
    knob->moveToFit();
    return knob;
}

MarkerKnob* Trackbar::addMarker(bool isCustom) {
    auto& knobs = isCustom ? mCustomMarkerKnobs : mMarkerKnobs;
    // look for a handle available
    for (auto* knob : knobs) {
        if (!knob->isVisible()) {
            knob->setVisible(true);
            return knob;
        }
    }
    // no handle is available, make it on the fly
    auto* knob = new MarkerKnob{isCustom ? QBoxLayout::BottomToTop : QBoxLayout::TopToBottom};
    connect(knob, &MarkerKnob::leftClicked, mPositionEdit, &TrackedKnob::setTimestamp);
    if (isCustom) {
        // connection is queued to let the item process its events before being hidden
        connect(knob, &MarkerKnob::rightClicked, this, [knob](){ knob->hide(); }, Qt::QueuedConnection);
    }
    knob->setBrush(knobColor());
    knob->xScale().margins = {8., 8.};
    knob->yScale().value = isCustom ? .7 : .3;
    mKnobView->insertKnob(knob);
    knobs.push_back(knob);
    return knob;
}

void Trackbar::setSequence(const SharedSequence& sequence) {
    Q_ASSERT(sequence);
    auto lastTimestamp = sequence->last_timestamp();
    if (lastTimestamp == 0.)
        lastTimestamp = 1.;
    // reinitialize editors
    mLowerEdit->initialize(sequence, 0., lastTimestamp);
    mUpperEdit->initialize(sequence, lastTimestamp, lastTimestamp);
    mPositionEdit->initialize(sequence, 0., lastTimestamp);
    mLastEdit->initialize(sequence, lastTimestamp, lastTimestamp);
    // reinitialize markers
    cleanMarkers();
    for (const auto& item : *sequence)
        if (item.event.is(family_t::marker))
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
    for (auto* knob : mMarkerKnobs)
        knob->setVisible(false);
    for (auto* knob : mCustomMarkerKnobs)
        knob->setVisible(false);
}

//===========
// TempoView
//===========

namespace {

auto* newTempoSpinBox(QWidget* parent) {
    auto* spin = new QDoubleSpinBox{parent};
    spin->setSpecialValueText("...");
    spin->setReadOnly(true);
    spin->setSuffix(" bpm");
    spin->setAlignment(Qt::AlignCenter);
    spin->setDecimals(1);
    spin->setMaximum(2000.);
    spin->setButtonSymbols(QDoubleSpinBox::NoButtons);
    auto policy = spin->sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Minimum);
    spin->setSizePolicy(policy);
    return spin;
}

}

TempoView::TempoView(QWidget* parent) : QWidget{parent} {

    mTempoSpin = newTempoSpinBox(this);
    mTempoSpin->setToolTip("Base Tempo");

    mDistortedTempoSpin = newTempoSpinBox(this);
    mDistortedTempoSpin->setToolTip("Current Tempo");

    mDistorsionSlider = makeHorizontalSlider(distorsionRange, 1., this);
    mDistorsionSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mDistorsionSlider->setFormatter(stringFromDistorsion);
    mDistorsionSlider->setToolTip("Tempo Distorsion");
    mDistorsionSlider->setNotifier([this](double value) {
        updateDistorted(value);
        emit distorsionChanged(value);
    });
    mDistorsionSlider->setDefault();

    setLayout(make_hbox(margin_tag{0}, spacing_tag{0}, mDistorsionSlider, mTempoSpin, mDistortedTempoSpin));
}

void TempoView::clearTempo() {
    updateBpm(0., -1.);
}

void TempoView::updateTimestamp(timestamp_t timestamp) {
    const auto& item = clockFromSequence(mSequence).last_tempo(timestamp);
    if (item.timestamp != mLastTempoTimestamp)
        updateBpm(extraction_ns::get_bpm(item.event), item.timestamp);
}

void TempoView::setSequence(const SharedSequence& sequence) {
    mSequence = sequence;
    clearTempo();
}

double TempoView::distorsion() const {
    return mDistorsionSlider->value();
}

void TempoView::setDistorsion(double distorsion) {
    mDistorsionSlider->setClampedValue(distorsion);
}

void TempoView::updateBpm(double bpm, timestamp_t timestamp) {
    mLastTempoTimestamp = timestamp;
    mTempoSpin->setValue(bpm);
    updateDistorted(distorsion());
}

void TempoView::updateDistorted(double distorsion) {
    mDistortedTempoSpin->setValue(mTempoSpin->value() * distorsion);
}

//========
// Player
//========

MetaHandler* makeMetaPlayer(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("Player");
    meta->setDescription("Generates events from MIDI files");
    meta->addParameter({"distorsion", "speedup factor applied to files played", "1", MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"view.families", "bitmask of families displayed", serial::serializeFamilies(families_t::standard()), MetaHandler::MetaParameter::Visibility::advanced});
    meta->addParameter({"view.channels", "bitmask of channels displayed", serial::serializeChannels(channels_t::full()), MetaHandler::MetaParameter::Visibility::advanced});
    meta->setFactory(new OpenProxyFactory<Player>);
    return meta;
}

Player::Player() : HandlerEditor{} {

    mPlaylist = new PlaylistTable{this};
    connect(mPlaylist, &PlaylistTable::itemActivated, this, &Player::launch);

    mSequenceView = new SequenceView{this};
    connect(mSequenceView, &SequenceView::positionSelected, this, &Player::onPositionSelected);

    mTracker = new Trackbar{this};
    connect(mTracker, &Trackbar::positionChanged, this, &Player::changePosition);
    connect(mTracker, &Trackbar::lowerChanged, this, &Player::changeLower);
    connect(mTracker, &Trackbar::upperChanged, this, &Player::changeUpper);

    mTempoView = new TempoView{this};
    connect(mTempoView, &TempoView::distorsionChanged, this, &Player::changeDistorsion);

    mRefreshTimer = new QTimer{this};
    mRefreshTimer->setInterval(75); /// @note update every 75ms
    connect(mRefreshTimer, &QTimer::timeout, this, &Player::refreshPosition);

    auto* tab = new QTabWidget{this};
    tab->addTab(mPlaylist, "Playlist");
    tab->addTab(mSequenceView, "Events");

    connect(makeAction(QIcon{":/data/media-step-backward.svg"}, "Play Previous", this), &QAction::triggered, this, &Player::playLastSequence);
    connect(makeAction(QIcon{":/data/media-play.svg"}, "Play", this), &QAction::triggered, this, &Player::playSequence);
    connect(makeAction(QIcon{":/data/media-pause.svg"}, "Pause", this), &QAction::triggered, this, &Player::pauseSequence);
    connect(makeAction(QIcon{":/data/media-stop.svg"}, "Stop", this), &QAction::triggered, this, &Player::resetSequence);
    connect(makeAction(QIcon{":/data/media-step-forward.svg"}, "Play Next", this), &QAction::triggered, this, &Player::playNextSequence);
    connect(makeAction(QIcon{":/data/chevron-right.svg"}, "Play Step", this), &QAction::triggered, this, &Player::stepForward);
    makeSeparator(this);
    mMetronomeAction = makeAction(QIcon{":/data/metronome.svg"}, "Metronome", this);
    mMetronomeAction->setCheckable(true);
    connect(mMetronomeAction, &QAction::toggled, this, &Player::setMetronome);
    makeSeparator(this);
    mLoopAction = new MultiStateAction{this};
    mLoopAction->addState(QIcon{":/data/move-down.svg"}, "No Loop"); /// @todo get a thinner arrow
    mLoopAction->addState(QIcon{":/data/loop-square.svg"}, "Loop");
    addAction(mLoopAction);
    mModeAction = new MultiStateAction{this};
    mModeAction->addState(QIcon{":/data/lines.svg"}, "Play All");
    mModeAction->addState(QIcon{":/data/highlighted-lines.svg"}, "Play Current");
    addAction(mModeAction);
    makeSeparator(this);
    connect(makeAction(QIcon{":/data/save.svg"}, "Save Current Sequence", this), &QAction::triggered, this, &Player::saveSequence);

    setLayout(make_vbox(margin_tag{0}, mTracker, mTempoView, tab));
}

const SharedSequence& Player::sequence() const {
    return mSequenceView->sequence();
}

bool Player::isSingle() const {
    return mModeAction->state() == 1;
}

bool Player::isLooping() const {
    return mLoopAction->state() == 1;
}

HandlerView::Parameters Player::getParameters() const {
    /// @todo get track filter
    auto result = HandlerEditor::getParameters();
    auto paths = mPlaylist->paths();
    if (!paths.empty())
        result.push_back(Parameter{"playlist", paths.join(';')});
    SERIALIZE("distorsion", serial::serializeNumber, mTempoView->distorsion(), result);
    SERIALIZE("view.families", serial::serializeFamilies, mSequenceView->familySelector()->families(), result);
    SERIALIZE("view.channels", serial::serializeChannels, mSequenceView->channelsSelector()->channels(), result);
    return result;
}

size_t Player::setParameter(const Parameter& parameter) {
    /// @todo set track filter
    if (parameter.name == "playlist") {
        mPlaylist->addPaths(parameter.value.split(';'));
        return 1;
    }
    UNSERIALIZE("distorsion", serial::parseDouble, mTempoView->setDistorsion, parameter);
    UNSERIALIZE("view.families", serial::parseFamilies, mSequenceView->familySelector()->setFamilies, parameter);
    UNSERIALIZE("view.channels", serial::parseChannels, mSequenceView->channelsSelector()->setChannels, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* Player::getHandler() {
    return &mHandler;
}

void Player::updateContext(Context* context) {
    mSequenceView->setChannelEditor(context->channelEditor());
    mPlaylist->setContext(context);
    context->quickToolBar()->addActions(actions());
}

void Player::changePosition(timestamp_t timestamp) {
    // event comes from trackbar
    mHandler.set_position(timestamp);
    mTempoView->updateTimestamp(timestamp);
}

void Player::changeLower(timestamp_t timestamp) {
    // event comes from trackbar
    mHandler.set_lower(timestamp);
    mSequenceView->setLower(timestamp);
}

void Player::changeUpper(timestamp_t timestamp) {
    // event comes from trackbar
    mHandler.set_upper(timestamp);
    mSequenceView->setUpper(timestamp);
}

void Player::changeDistorsion(double distorsion) {
    // event comes from tempoview, don't need to update
    mHandler.set_distorsion(distorsion);
    mTracker->setDistorsion(distorsion);
    mSequenceView->setDistorsion(distorsion);
}

void Player::setTrackFilter(Handler* handler) {
    mSequenceView->setTrackFilter(handler);
}

bool Player::setNextSequence(int offset) {
    resetSequence();
    if (isSingle() && mPlaylist->isLoaded())
        return isLooping();
    return setSequence(mPlaylist->loadRelative(offset, isLooping()));
}

void Player::updatePosition() {
    const auto pos = mHandler.position();
    mTempoView->updateTimestamp(pos);
    mTracker->updateTimestamp(pos);
}

void Player::refreshPosition() {
    updatePosition();
    if (mHandler.is_completed()) {
        playNextSequence();
    } else if (!mHandler.is_playing()) { // stopped by an event
        mIsStepping = false;
        mRefreshTimer->stop();
        mTempoView->clearTempo();
        mPlaylist->setCurrentStatus(STOPPED);
    } else if (mIsStepping && mHandler.position() >= mNextStep) {
        pauseSequence();
    }
}

bool Player::setSequence(NamedSequence sequence) {
    if (!isValid(sequence.sequence))
        return false;
    if (auto* systemTrayIcon = context()->systemTrayIcon())
        showSystemTrayMessage(systemTrayIcon, handlerName(&mHandler), sequence.name, QIcon{":/data/media-play.svg"}, 2000);
    mTempoView->setSequence(sequence.sequence);
    mSequenceView->setSequence(sequence.sequence);
    mTracker->setSequence(sequence.sequence);
    auto seq{*sequence.sequence};
    if (mMetronomeAction->isChecked())
        seq.insert_items(seq.make_metronome());
    mHandler.set_sequence(std::move(seq));
    return true;
}

void Player::saveSequence() {
    const auto seq = sequence();
    if (!isValid(seq)) {
        QMessageBox::critical(this, {}, "No sequence to save");
        return;
    }
    const auto filename = context()->pathRetrieverPool()->get("midi")->getWriteFile(this);
    if (filename.isNull())
        return;
    if (dumping::write_file(seq->to_file(), filename.toStdString()) == 0)
        QMessageBox::critical(this, {}, "Unable to write sequence");
    else
        QMessageBox::information(this, {}, "Sequence saved");
}

void Player::setMetronome(bool enabled) {
    if (isValid(sequence())) {
        auto seq{*sequence()};
        if (enabled)
            seq.insert_items(seq.make_metronome());
        mHandler.replace_sequence(std::move(seq));
    }
}

void Player::launch(QTableWidgetItem* item) {
    auto namedSequence = mPlaylist->loadRow(item->row());
    if (isValid(namedSequence.sequence)) {
        resetSequence();
        setSequence(std::move(namedSequence));
        playCurrentSequence();
    } else { /// @todo get reason from model
        QMessageBox::critical(this, {}, "Can't read MIDI File");
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

void Player::playSequence() {
    if (!mHandler.is_playing())
        if (mPlaylist->isLoaded() || setNextSequence(1))
            playCurrentSequence();
}

void Player::playCurrentSequence(bool resetStepping) {
    if (mHandler.start_playing(false)) {
        if (resetStepping)
            mIsStepping = false;
        mRefreshTimer->start();
        mPlaylist->setCurrentStatus(PLAYING);
    }
}

void Player::pauseSequence() {
    if (mHandler.stop_playing(Event::controller(channels_t::full(), controller_ns::all_sound_off_controller), false, false)) {
        mIsStepping = false;
        mRefreshTimer->stop();
        mPlaylist->setCurrentStatus(PAUSED);
    }
}

void Player::resetSequence() {
    mHandler.stop_playing(Event::reset(), true, true);
    mIsStepping = false;
    mRefreshTimer->stop();
    mPlaylist->setCurrentStatus(STOPPED);
    updatePosition();
}

void Player::playNextSequence() {
    if (setNextSequence(1))
        playCurrentSequence();
}

void Player::playLastSequence() {
    if (setNextSequence(-1))
        playCurrentSequence();
}

void Player::stepForward() {
    mIsStepping = false;
    const auto pos = mHandler.position();
    const auto& seq = mHandler.sequence();
    auto first = seq.begin(), last = seq.end();
    for (auto it = std::lower_bound(first, last, pos) ; it != last ; ++it) {
        if (it->event.is(family_t::note_on)) {
            mNextStep = it->timestamp;
            mIsStepping = true;
            break;
        }
    }
    playCurrentSequence(false);
}
