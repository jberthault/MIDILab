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

#include <QMessageBox>
#include <QHeaderView>
#include <QtXml>
#include <QXmlSchema>
#include <QXmlSchemaValidator>
#include "qcore/programeditor.h"
#include "core/misc.h"

namespace  {

static constexpr byte_t default_program = 0xff;

Patch parsePatch(const QDomNode& node) {
    Patch patch;
    if (node.nodeName() == "Patches")
        patch.setName("root");
    else if (node.nodeName() == "Patch")
        patch.setName(node.toElement().attribute("name"));
    const auto childNodes = node.childNodes();
    for (int i=0 ; i < childNodes.count() ; i++) {
        const auto childNode = childNodes.at(i);
        if (childNode.nodeName() == "Patch")
            patch.addPatch(parsePatch(childNode));
        else if (childNode.nodeName() == "Program")
            patch.addProgram(to_byte(childNode.toElement().attribute("value").toInt()), childNode.firstChild().toCharacterData().data());
    }
    return patch;
}

}

//=======
// Patch
//=======

Patch::Patch(QString name) : mName{std::move(name)} {

}

const QString& Patch::name() const {
    return mName;
}

void Patch::setName(QString name) {
    mName = std::move(name);
}

const std::map<byte_t, QString>& Patch::programs() const {
    return mPrograms;
}

void Patch::addProgram(byte_t program, QString name) {
    auto& previous_name = mPrograms[program];
    if (!previous_name.isNull())
        TRACE_WARNING("overriding program " << byte_string(program) << " in patch " << mName);
    previous_name = std::move(name);
}

const std::vector<Patch>& Patch::children() const {
    return mChildren;
}

void Patch::addPatch(Patch patch) {
    mChildren.push_back(std::move(patch));
}

QString Patch::getProgram(byte_t program, const QString& defaultName) const {
    const auto it = mPrograms.find(program);
    if (it != mPrograms.end())
        return it->second;
    for (const auto& child : mChildren) {
        const auto name = child.getProgram(program, {});
        if (!name.isNull())
            return name;
    }
    return defaultName;
}

//===============
// PatchDelegate
//===============

QWidget* PatchDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const {
    const auto* programModel = static_cast<const ProgramModel*>(index.model());
    auto* editor = new TreeBox{parent};
    editor->setModel(new PatchModel{*programModel->patch(), editor});
    const auto channels = channels_t::wrap(static_cast<channel_t>(index.row()));
    connect(editor, &TreeBox::treeIndexChanged, [=](QModelIndex){
        const auto data = editor->currentData(Qt::UserRole);
        if (data.isValid())
            emit programModel->programEdited(channels, static_cast<byte_t>(data.toInt()));
    });
    return editor;
}

void PatchDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    const auto* programModel = static_cast<const ProgramModel*>(index.model());
    const auto channel = static_cast<channel_t>(index.row());
    if (programModel->hasProgram(channel)) {
        const auto program = programModel->program(channel);
        auto* tree = static_cast<TreeBox*>(editor);
        const auto* model = static_cast<const PatchModel*>(tree->model());
        QSignalBlocker sb{tree};
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

PatchModel::PatchModel(const Patch& patch, QObject *parent) : QStandardItemModel{parent} {
    invisibleRootItem()->appendRow(makeRow(patch));
}

QModelIndex PatchModel::indexForProgram(byte_t program) const {
    return indexForProgram(program, invisibleRootItem());
}

QModelIndex PatchModel::indexForProgram(byte_t program, QStandardItem* item) const {
    const auto itemData = item->data(Qt::UserRole);
    if (itemData.isValid() && itemData.toInt() == static_cast<int>(program))
        return item->index();
    for (int row=0 ; row < item->rowCount() ; row++) {
        const auto foundIndex = indexForProgram(program, item->child(row));
        if (foundIndex.isValid())
            return foundIndex;
    }
    return {};
}

QStandardItem* PatchModel::makeRow(const Patch &patch) {
    auto* parent = new QStandardItem{patch.name()};
    for (const auto& pair : patch.programs()) {
        auto text = QString{"%1 %2"}.arg(pair.first).arg(pair.second);
        auto* item = new QStandardItem{std::move(text)};
        item->setData(static_cast<int>(pair.first), Qt::UserRole);
        parent->appendRow(item);
    }
    for (const auto& child : patch.children())
        parent->appendRow(makeRow(child));
    return parent;
}

//==============
// ProgramModel
//==============

ProgramModel::ProgramModel(ChannelEditor* channelEditor, QObject* parent) : QStandardItemModel{channels_t::capacity(), 2, parent} {
    QStringList labels;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
        labels.append(QString::number(c));
        auto* head = new QStandardItem;
        head->setFlags(Qt::ItemIsEnabled);
        head->setBackground(QBrush{channelEditor->color(c)});
        setItem(c, 0, head);
        setItem(c, 1, new QStandardItem);
    }
    setVerticalHeaderLabels(labels);
    connect(channelEditor, &ChannelEditor::colorChanged, this, &ProgramModel::updateColor);
}

const Patch* ProgramModel::patch() const {
    return mPatch;
}

void ProgramModel::setPatch(const Patch* patch) {
    mPatch = patch;
}

bool ProgramModel::hasProgram(channel_t channel) const {
    return item(channel, 1)->data(Qt::UserRole).canConvert<int>();
}

byte_t ProgramModel::program(channel_t channel) const {
    return to_byte(item(channel, 1)->data(Qt::UserRole).toInt());
}

void ProgramModel::setProgram(channels_t channels, byte_t program) {
    QString text;
    QString tooltip;
    QVariant data;
    if (program != default_program) {
        text = mPatch->getProgram(program, "????");
        tooltip = QString::number(program);
        data = static_cast<int>(program);
    }
    for (channel_t channel : channels) {
        auto* dataItem = item(channel, 1);
        dataItem->setData(data, Qt::UserRole);
        dataItem->setText(text);
        dataItem->setToolTip(tooltip);
    }
}

void ProgramModel::updateColor(channel_t channel, const QColor& color) {
    item(channel, 0)->setBackground(QBrush{color});
}

//===============
// ProgramEditor
//===============

ProgramEditor::ProgramEditor(Manager* manager, QWidget* parent) : QWidget{parent} {

    setWindowTitle("Programs");
    setWindowIcon(QIcon{":/data/trumpet.png"});
    setWindowFlags(Qt::Dialog);

    /// Read patches from file
    QByteArray programsData;
    QFile programsFile{":/data/programs.xml"};
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
        validated = QXmlSchemaValidator{xsd}.validate(programsData);

    /// construct DOM
    QDomDocument dom{"DOM Document"};
    if (validated && dom.setContent(programsData)) {
        mRootPatch = parsePatch(dom.documentElement());
    } else {
        mRootPatch.addPatch(Patch{"No Patch"});  // required because of at()
        TRACE_ERROR("programs.xml is illformed");
    }

    // patches combo
    mPatchesCombo = new QComboBox{this};
    for (const auto& patch : mRootPatch.children())
        mPatchesCombo->addItem(patch.name());
    connect(mPatchesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ProgramEditor::updatePatch);

    // handlers combo
    mHandlerSelector = new HandlerSelector{this};
    connect(manager, &Context::handlerRenamed, mHandlerSelector, &HandlerSelector::renameHandler);
    connect(mHandlerSelector, &HandlerSelector::handlerChanged, this, &ProgramEditor::showHandler);

    // model
    mProgramModel = new ProgramModel{manager->channelEditor(), this};
    connect(mProgramModel, &ProgramModel::programEdited, this, &ProgramEditor::editProgram);

    // table
    auto* table = new QTableView{this};
    table->setModel(mProgramModel);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(0, 20);
    table->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    table->verticalHeader()->setDefaultSectionSize(20);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setVisible(false);
    table->setItemDelegateForColumn(1, new PatchDelegate{this});
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setMinimumHeight(table->rowHeight(0) * channels_t::capacity() + 2); // force the table to show every row, crappy ...
    table->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(table, &QTableView::clicked, this, &ProgramEditor::onClick);
    connect(table, &QTableView::doubleClicked, this, &ProgramEditor::onDoubleClick);

    // layout
    setLayout(make_vbox(make_hbox(mHandlerSelector, mPatchesCombo), table));

    // manager signals
    connect(manager, &Context::handlerInserted, this, &ProgramEditor::insertHandler);
    connect(manager, &Context::handlerRemoved, this, &ProgramEditor::removeHandler);
    connect(manager->observer(), &Observer::messageHandled, this, &ProgramEditor::updateSuccess);

}

Handler* ProgramEditor::currentHandler() {
    return mHandlerSelector->currentHandler();
}

void ProgramEditor::insertHandler(Handler* handler) {
    if (handler->mode().any(Handler::Mode::out()) && handler->received_families().test(family_t::program_change)) {
        auto& handlerData = mRecords[handler];
        handlerData.first = 0;
        handlerData.second.fill(default_program);
        mHandlerSelector->insertHandler(handler);
    }
}

void ProgramEditor::removeHandler(Handler* handler) {
    mRecords.erase(handler);
    mHandlerSelector->removeHandler(handler);
}

void ProgramEditor::showHandler(Handler* handler) {
    const auto it = mRecords.find(handler);
    if (it != mRecords.end()) {
        selectHandler(it->second);
        QSignalBlocker sb{mPatchesCombo};
        mPatchesCombo->setCurrentIndex(it->second.first);
    } else {
        mProgramModel->setProgram(channels_t::full(), default_program);
    }
}

void ProgramEditor::updatePatch(int patchIndex) {
    auto it = mRecords.find(currentHandler());
    if (it != mRecords.end()) {
        it->second.first = patchIndex;
        selectHandler(it->second);
    }
}

void ProgramEditor::selectHandler(const HandlerData& handlerData) {
    mProgramModel->setPatch(&mRootPatch.children().at(handlerData.first));
    for (const auto& pair : channel_ns::reverse(handlerData.second, channels_t::full()))
        mProgramModel->setProgram(pair.second, pair.first);
}

void ProgramEditor::receiveProgram(Handler* handler, channels_t channels, byte_t program) {
    channel_ns::store(mRecords[handler].second, channels, program);
    if (handler == mHandlerSelector->currentHandler())
        mProgramModel->setProgram(channels, program);
}

void ProgramEditor::sendProgram(Handler* handler, channels_t channels, byte_t program) {
    if (handler->mode().any(Handler::Mode::out()))
        handler->send_message(Event::program_change(channels, program));
    else
        QMessageBox::warning(this, {}, "You can't change program of an non-output handler");
}

void ProgramEditor::editProgram(channels_t channels, byte_t program) {
    if (auto* handler = currentHandler()) {
        const auto extension = extend(channels);
        receiveProgram(handler, extension & ~channels, program);
        sendProgram(handler, extension, program);
    }
}

void ProgramEditor::updateSuccess(Handler* handler, const Message& message) {
    /// @todo treat bank messages
    switch (message.event.family()) {
    case family_t::program_change:
        receiveProgram(handler, message.event.channels(), extraction_ns::program(message.event));
        break;
    case family_t::extended_system:
        if (Handler::close_ext.affects(message.event) && Handler::close_ext.decode(message.event).any(Handler::State::receive()))
            receiveProgram(handler, channels_t::full(), default_program);
        break;
    default:
        break;
    }
}

void ProgramEditor::onClick(const QModelIndex& index) {
    if (index.column() == 0) {
        const auto channel = static_cast<channel_t>(index.row());
        mSelection.flip(channel);
        auto icon = mSelection.test(channel) ? QIcon{":/data/link-intact.svg"} : QIcon{};
        mProgramModel->setData(index, std::move(icon), Qt::DecorationRole);
    }
}

void ProgramEditor::onDoubleClick(const QModelIndex& index) {
    if (index.column() == 0) {
        mSelection ^= channels_t::full();
        for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
            auto icon = mSelection.test(c) ? QIcon{":/data/link-intact.svg"} : QIcon{};
            mProgramModel->setData(index.sibling(c, 0), std::move(icon), Qt::DecorationRole);
        }
    }
}

bool ProgramEditor::matchSelection(channels_t channels) const {
    return !channels || !mSelection || static_cast<bool>(channels & mSelection);
}

channels_t ProgramEditor::extend(channels_t channels) const {
    return matchSelection(channels) ? channels | mSelection : channels;
}
