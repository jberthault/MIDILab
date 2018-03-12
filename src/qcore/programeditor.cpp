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

#include <QMessageBox>
#include <QHeaderView>
#include <QXmlSchema>
#include <QXmlSchemaValidator>
#include "qcore/manager.h"
#include "qcore/programeditor.h"
#include "core/misc.h"

//=======
// Patch
//=======

QList<Patch*> Patch::parsePatches(const QDomNode& node) {
    Q_ASSERT(node.nodeName() == "Patches");
    QList<Patch*> patches;
    QDomNodeList childs = node.childNodes();
    for (int i=0 ; i < childs.count() ; i++)
        patches.append(parsePatch(childs.at(i)));
    return patches;
}

Patch* Patch::parsePatch(const QDomNode& node) {
    Q_ASSERT(node.nodeName() == "Patch");
    Patch* patch = new Patch(node.toElement().attribute("name"));
    QDomNodeList childs = node.childNodes();
    for (int i=0 ; i < childs.count() ; i++) {
        QDomNode child = childs.at(i);
        if (child.nodeName() == "Patch")
            patch->insertChild(parsePatch(child));
        else if (child.nodeName() == "Program") {
            byte_t programValue = (byte_t)child.toElement().attribute("value").toInt();
            QString programName = child.firstChild().toCharacterData().data();
            patch->insertProgram(programValue, programName);
        }
    }
    return patch;
}

Patch::Patch(QString name) : mName(std::move(name)) {

}

Patch::~Patch() {
    for (Patch* child : mChilds)
        delete child;
}

const QString& Patch::name() const {
    return mName;
}

void Patch::setName(QString name) {
    mName = std::move(name);
}

const QMap<byte_t, QString>& Patch::programs() const {
    return mPrograms;
}

bool Patch::hasProgram(byte_t program) const {
    if (mPrograms.contains(program))
        return true;
    for (Patch* child : mChilds)
        if (child->hasProgram(program))
            return true;
    return false;
}

QString Patch::getProgram(byte_t program, const QString& defaultName) const {
    if (mPrograms.contains(program))
        return mPrograms[program];
    for (Patch* child : mChilds) {
        QString found = child->getProgram(program, QString());
        if (!found.isNull())
            return found;
    }
    return defaultName;
}

bool Patch::insertProgram(byte_t program, const QString& name) {
    if (hasProgram(program)) {
        TRACE_WARNING("can't insert program in patch " << mName);
        return false;
    }
    mPrograms[program] = name;
    return true;
}

bool Patch::removeProgram(byte_t program) {
    if (mPrograms.remove(program) != 0)
        return true;
    for (Patch* child : mChilds)
        if (child->removeProgram(program))
            return true;
    return false;
}

const QList<Patch*>& Patch::childs() const {
    return mChilds;
}

bool Patch::hasChild(Patch* patch) const {
    if (mChilds.contains(patch))
        return true;
    for (Patch* child : mChilds)
        if (child->hasChild(patch))
            return true;
    return false;
}

bool Patch::insertChild(Patch* patch) {
    Q_ASSERT(patch != nullptr);
    if (hasChild(patch)) {
        TRACE_WARNING("Patch " << name() << " already contains " << patch->name());
        return false;
    }
    mChilds.append(patch);
    return true;
}

bool Patch::removeChild(Patch* patch) {
    if (mChilds.removeAll(patch) != 0)
        return true;
    for (Patch* child : mChilds)
        if (child->removeChild(patch))
            return true;
    return false;
}

//===============
// PatchDelegate
//===============

PatchDelegate::PatchDelegate(QObject* parent) : QItemDelegate(parent) {

}

QWidget* PatchDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const {
    const ProgramModel* programModel = static_cast<const ProgramModel*>(index.model());
    TreeBox* editor = new TreeBox(parent);
    PatchModel* model = new PatchModel(editor);
    model->setPatch(programModel->patch());
    editor->setModel(model);
    channels_t channels = channels_t::wrap(index.row());
    connect(editor, &TreeBox::treeIndexChanged, [=](QModelIndex){
        QVariant data = editor->currentData(Qt::UserRole);
        if (data.isValid())
            emit programModel->programEdited(channels, (byte_t)data.toInt());
    });
    return editor;
}

void PatchDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    const ProgramModel* programModel = static_cast<const ProgramModel*>(index.model());
    channel_t channel = index.row();
    if (programModel->hasProgram(channel)) {
        byte_t program = programModel->program(channel);
        TreeBox* tree = static_cast<TreeBox*>(editor);
        PatchModel* model = static_cast<PatchModel*>(tree->model());
        QSignalBlocker sb(tree);
        tree->setTreeIndex(model->indexForProgram(program));
    }
}

void PatchDelegate::setModelData(QWidget* /*editor*/, QAbstractItemModel* /*model*/, const QModelIndex& /*index*/) const {

}

void PatchDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const {
    editor->setGeometry(option.rect);
}

//============
// PatchModel
//============

PatchModel::PatchModel(QObject* parent) : QStandardItemModel(parent), mPatch(nullptr) {

}

QModelIndex PatchModel::indexForProgram(byte_t program) const {
    return indexForProgram(program, invisibleRootItem());
}

QModelIndex PatchModel::indexForProgram(byte_t program, QStandardItem* item) const {
    QVariant itemData = item->data(Qt::UserRole);
    if (itemData.isValid() && (byte_t)itemData.toInt() == program)
        return item->index();
    for (int row=0 ; row < item->rowCount() ; row++) {
        QModelIndex foundIndex = indexForProgram(program, item->child(row));
        if (foundIndex.isValid())
            return foundIndex;
    }
    return {};
}

const Patch* PatchModel::patch() const {
    return mPatch;
}

void PatchModel::setPatch(const Patch* patch) {
    mPatch = patch;
    clear();
    addPatch(invisibleRootItem(), patch);
}

void PatchModel::addPatch(QStandardItem* root, const Patch* patch) {
    QStandardItem* parent = new QStandardItem(patch->name());
    root->appendRow(parent);
    QMapIterator<byte_t, QString> it(patch->programs());
    while (it.hasNext()) {
        it.next();
        QString text = QString("%1 %2").arg(it.key()).arg(it.value());
        QStandardItem* item = new QStandardItem(text);
        item->setData(it.key(), Qt::UserRole);
        parent->appendRow(item);
    }
    for (const Patch* child : patch->childs())
        addPatch(parent, child);
}

//==============
// ProgramModel
//==============

ProgramModel::ProgramModel(ChannelEditor* channelEditor, QObject* parent) : QStandardItemModel(channels_t::capacity(), 2, parent), mPatch(nullptr) {
    QStringList labels;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
        labels.append(QString::number(c));
        QStandardItem* head = new QStandardItem;
        head->setFlags(Qt::ItemIsEnabled);
        head->setBackground(QBrush(channelEditor->color(c)));
        setItem(c, 0, head);
        setItem(c, 1, new QStandardItem);
    }
    setVerticalHeaderLabels(labels);
    clearPrograms();
    connect(channelEditor, &ChannelEditor::colorChanged, this, &ProgramModel::updateColor);
}

const Patch* ProgramModel::patch() const {
    return mPatch;
}

void ProgramModel::setPatch(const Patch* patch) {
    mPatch = patch;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++)
        if (hasProgram(c))
            fillProgram(channels_t::wrap(c), program(c)); // change text
}

bool ProgramModel::hasProgram(channel_t channel) const {
    return item(channel, 1)->data(Qt::UserRole).toInt() != -1;
}

byte_t ProgramModel::program(channel_t channel) const {
    return (byte_t)item(channel, 1)->data(Qt::UserRole).toInt();
}

void ProgramModel::setPrograms(const QMap<channel_t, byte_t>& programs) {
    clearPrograms();
    QMapIterator<channel_t, byte_t> it(programs);
    while (it.hasNext()) {
        it.next();
        setProgram(it.key(), it.value());
    }
}

void ProgramModel::fillProgram(channels_t channels, byte_t program) {
    fillProgramData(channels, mPatch->getProgram(program, "????"), QString::number(program), (int)program);
}

void ProgramModel::setProgram(channel_t channel, byte_t program) {
    fillProgram(channels_t::wrap(channel), program);
}

void ProgramModel::clearPrograms() {
    fillProgramData(channels_t::full(), "", "", -1);
}

void ProgramModel::setProgramData(channel_t channel, const QString& text, const QString& toolTip, const QVariant& data) {
    QStandardItem* dataItem = item(channel, 1);
    dataItem->setData(data, Qt::UserRole);
    dataItem->setText(text);
    dataItem->setToolTip(toolTip);
}

void ProgramModel::fillProgramData(channels_t channels, const QString& text, const QString& toolTip, const QVariant& data) {
    for (channel_t channel : channels)
        setProgramData(channel, text, toolTip, data);
}

void ProgramModel::updateColor(channel_t channel, const QColor& color) {
    item(channel, 0)->setBackground(QBrush(color));
}

//===============
// ProgramEditor
//===============

ProgramEditor::ProgramEditor(ChannelEditor* channelEditor, QWidget* parent) : QWidget(parent) {

    setWindowTitle("Programs");
    setWindowIcon(QIcon(":/data/trumpet.png"));
    setWindowFlags(Qt::Dialog);

    /// Read patches from file
    QByteArray programsData;
    QFile programsFile(":/data/programs.xml");
    if (programsFile.open(QIODevice::ReadOnly)) {
        programsData = programsFile.readAll();
        programsFile.close();
    } else {
        TRACE_ERROR("Can't read file programs.xml");
    }

    /// Construct XSD Validator and check
    QXmlSchema xsd;
    bool validated = true;
    if (xsd.load(QUrl::fromLocalFile(":/data/programs.xsd")))
        validated = QXmlSchemaValidator(xsd).validate(programsData);

    /// construct DOM
    QDomDocument dom("DOM Document");
    if (validated && dom.setContent(programsData)) {
        mPatches = QVector<Patch*>::fromList(Patch::parsePatches(dom.documentElement()));
    } else {
        mPatches << new Patch("No Patch");  // required because of at()
        TRACE_ERROR("programs.xml is illformed");
    }

    // PATCHES COMBO
    mPatchesCombo = new QComboBox(this);
    for (int i=0 ; i < mPatches.size() ; i++)
        mPatchesCombo->addItem(mPatches.at(i)->name(), i);
    connect(mPatchesCombo, SIGNAL(currentIndexChanged(int)), SLOT(updatePatch(int)));

    // HANDLERS COMBO
    mHandlerSelector = new HandlerSelector(this);
    connect(Manager::instance, &Manager::handlerRenamed, mHandlerSelector, &HandlerSelector::renameHandler);
    connect(mHandlerSelector, &HandlerSelector::handlerChanged, this, &ProgramEditor::showHandler);

    // CHANNELS
    mProgramModel = new ProgramModel(channelEditor, this);
    connect(mProgramModel, &ProgramModel::programEdited, this, &ProgramEditor::editProgram);
    QTableView* table = new QTableView(this);
    table->setModel(mProgramModel);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(0, 20);
    table->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    table->verticalHeader()->setDefaultSectionSize(20);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setVisible(false);
    table->setItemDelegateForColumn(1, new PatchDelegate(this));
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setMinimumHeight(20 * channels_t::capacity() + 2); // force the table to show every row, crappy ...
    table->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(table, &QTableView::clicked, this, &ProgramEditor::onClick);
    connect(table, &QTableView::doubleClicked, this, &ProgramEditor::onDoubleClick);

    // LAYOUT
    setLayout(make_vbox(make_hbox(mHandlerSelector, mPatchesCombo), table));

    connect(Manager::instance, &Manager::handlerInserted, this, &ProgramEditor::insertHandler);
    connect(Manager::instance, &Manager::handlerRemoved, this, &ProgramEditor::removeHandler);
    connect(Manager::instance->observer(), &Observer::messageHandled, this, &ProgramEditor::updateSuccess);

}

ProgramEditor::~ProgramEditor() {
    for (Patch* patch : mPatches)
        delete patch;
}

Handler* ProgramEditor::currentHandler() {
    return mHandlerSelector->currentHandler();
}

void ProgramEditor::insertHandler(Handler* handler) {
    if (handler->mode().any(Handler::Mode::out()) && handler->handled_families().test(family_t::program_change)) {
        mRecords[handler] = HandlerData(0, QMap<channel_t, byte_t>());
        mHandlerSelector->insertHandler(handler);
    }
}

void ProgramEditor::removeHandler(Handler* handler) {
    mRecords.remove(handler);
    mHandlerSelector->removeHandler(handler);
}

void ProgramEditor::showHandler(Handler* handler) {
    mPatchesCombo->blockSignals(true);
    mPatchesCombo->setCurrentIndex(mRecords[handler].first);
    mPatchesCombo->blockSignals(false);
    mProgramModel->setPatch(mPatches.at(mRecords[handler].first));
    mProgramModel->setPrograms(mRecords[handler].second);
}

void ProgramEditor::updatePatch(int patchIndex) {
    Handler* handler = currentHandler();
    if (handler) {
        mRecords[handler].first = patchIndex;
        mProgramModel->setPatch(mPatches.at(patchIndex));
    }
}

void ProgramEditor::receiveProgram(Handler* handler, channels_t channels, byte_t program) {
    for (channel_t channel : channels)
        mRecords[handler].second[channel] = program;
    if (handler == mHandlerSelector->currentHandler())
        mProgramModel->fillProgram(channels, program);
}

void ProgramEditor::sendProgram(Handler* handler, channels_t channels, byte_t program) {
    if (handler->mode().any(Handler::Mode::out()))
        handler->send_message(Event::program_change(channels, program));
    else
        QMessageBox::warning(this, QString(), "You can't change program of an non-output handler");
}

void ProgramEditor::editProgram(channels_t channels, byte_t program) {
    Handler* handler = currentHandler();
    if (handler) {
        channels_t extension = extend(channels);
        receiveProgram(handler, extension & ~channels, program);
        sendProgram(handler, extension, program);
    }
}

void ProgramEditor::updateSuccess(Handler* handler, Message message) {
    /// @todo treat bank messages
    switch (message.event.family()) {
    case family_t::program_change:
        receiveProgram(handler, message.event.channels(), message.event.at(1));
        break;
    case family_t::custom:
        if (message.event.has_custom_key("Close")) {
            mRecords[handler].second.clear();
            if (handler == mHandlerSelector->currentHandler())
                mProgramModel->setPrograms(QMap<channel_t, byte_t>());
        }
        break;
    }
}

void ProgramEditor::onClick(const QModelIndex& index) {
    if (index.column() == 0) {
        auto channel = (channel_t)(index.row());
        mSelection.flip(channel);
        auto icon = mSelection.test(channel) ? QIcon(":/data/link-intact.svg") : QIcon();
        mProgramModel->setData(index, icon, Qt::DecorationRole);
    }
}

void ProgramEditor::onDoubleClick(const QModelIndex& index) {
    if (index.column() == 0) {
        mSelection ^= channels_t::full();
        for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
            auto icon = mSelection.test(c) ? QIcon(":/data/link-intact.svg") : QIcon();
            mProgramModel->setData(index.sibling(c, 0), icon, Qt::DecorationRole);
        }
    }
}

bool ProgramEditor::matchSelection(channels_t channels) const {
    return !channels || !mSelection || bool(channels & mSelection);
}

channels_t ProgramEditor::extend(channels_t channels) const {
    return matchSelection(channels) ? channels_t(channels | mSelection) : channels;
}

