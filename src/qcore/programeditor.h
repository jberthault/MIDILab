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

#ifndef QCORE_PROGRAM_EDITOR_H
#define QCORE_PROGRAM_EDITOR_H

#include <QStandardItemModel>
#include <QItemDelegate>
#include <QtXml>
#include "qcore/editors.h"

//=======
// Patch
//=======

class Patch {

public:
    static std::vector<Patch*> parsePatches(const QDomNode& node);
    static Patch* parsePatch(const QDomNode& node);

    Patch(QString name);
    virtual ~Patch();

    const QString& name() const;
    void setName(QString name);

    const QMap<byte_t, QString>& programs() const;
    bool hasProgram(byte_t program) const; /*!< recursive approach */
    QString getProgram(byte_t program, const QString& defaultName) const; /*!< recursive approach */
    bool insertProgram(byte_t program, const QString& name);
    bool removeProgram(byte_t program); /*!< recursive approach */

    const std::vector<Patch*>& children() const;
    bool hasChild(Patch* patch) const; /*!< recursive approach */
    bool insertChild(Patch* patch);

private:
    QString mName;
    QMap<byte_t, QString> mPrograms;
    std::vector<Patch*> mChildren;

};

//===============
// PatchDelegate
//===============

class PatchDelegate : public QItemDelegate {

    Q_OBJECT

public:
    explicit PatchDelegate(QObject* parent);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

//============
// PatchModel
//============

class PatchModel : public QStandardItemModel {

    Q_OBJECT

public:
    explicit PatchModel(QObject* parent);

    const Patch* patch() const;
    void setPatch(const Patch* patch);

    QModelIndex indexForProgram(byte_t program) const;
    QModelIndex indexForProgram(byte_t program, QStandardItem* item) const;

protected:
    void addPatch(QStandardItem* root, const Patch* patch);

private:
    const Patch* mPatch;

};

//==============
// ProgramModel
//==============

class ProgramModel : public QStandardItemModel {

    Q_OBJECT

public:
    explicit ProgramModel(ChannelEditor* channelEditor, QObject* parent);

    const Patch* patch() const;
    void setPatch(const Patch* patch);

    bool hasProgram(channel_t channel) const;
    byte_t program(channel_t channel) const;

    void setPrograms(const QMap<channel_t, byte_t>& programs);
    void clearPrograms();

    void setProgram(channel_t channel, byte_t program);
    void fillProgram(channels_t channels, byte_t program);

signals:
    void programEdited(channels_t channels, byte_t program) const;

protected:
    void updateColor(channel_t channel, const QColor& color);

private:

    void setProgramData(channel_t channel, const QString& text, const QString& toolTip, const QVariant& data);
    void fillProgramData(channels_t channels, const QString& text, const QString& toolTip, const QVariant& data);

    const Patch* mPatch;

};

//===============
// ProgramEditor
//===============

class ProgramEditor : public QWidget {

    Q_OBJECT

    // index of patch / program mapping
    using HandlerData = std::pair<int, QMap<channel_t, byte_t>>;

public:
    explicit ProgramEditor(ChannelEditor* channelEditor, QWidget* parent);
    ~ProgramEditor();

    Handler* currentHandler();

protected slots:
    void insertHandler(Handler* handler);
    void removeHandler(Handler* handler);
    void showHandler(Handler* handler);

    void updatePatch(int patchIndex);

    void receiveProgram(Handler* handler, channels_t channels, byte_t program);
    void sendProgram(Handler* handler, channels_t channels, byte_t program);

    void editProgram(channels_t channels, byte_t program); // 'send' slot
    void updateSuccess(Handler* handler, Message message); // 'receive' slot

    void onClick(const QModelIndex& index);
    void onDoubleClick(const QModelIndex& index);

protected:
    bool matchSelection(channels_t channels) const;
    channels_t extend(channels_t channels) const;

private:
    std::vector<Patch*> mPatches;
    QMap<Handler*, HandlerData> mRecords;

    HandlerSelector* mHandlerSelector;
    QComboBox* mPatchesCombo;
    ProgramModel* mProgramModel;
    channels_t mSelection;

};

//class ProgramMapper {

//    struct program_storage_t {
//        std::unordered_map<byte_t, byte_t> bank_coarse;
//        std::unordered_map<byte_t, byte_t> bank_fine;
//        std::unordered_map<byte_t, byte_t> programs;
//    };

//private:

//    static byte_t transform(const std::unordered_map<byte_t, byte_t>& map, byte_t value);

//    std::unordered_map<channel_t, program_storage_t> m_storage;

//};

#endif // QCORE_PROGRAM_EDITOR_H
