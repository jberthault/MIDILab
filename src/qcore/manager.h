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

#ifndef QCORE_MANAGER_H
#define QCORE_MANAGER_H

#include "qcore/core.h"
#include "qcore/editors.h"

//======================
// HandlerConfiguration
//======================

class SingleDisplayer;

struct HandlerConfiguration {

    HandlerConfiguration(QString name);

    QString name; /*!< handler's name */
    SingleDisplayer* host; /*!< widget receiving the handler, new detached handler if null */
    QString group;
    HandlerView::Parameters parameters; /*!< parameters to apply to the handler */

};

//=========
// Manager
//=========

class Manager : public Context {

    Q_OBJECT

public:

    struct Data {
        HandlerEditor* editor; /*!< editor attached to the handler */
        QString type; /*!< meta handler name that instantiated the handler */
        bool owns; /*!< true if handler should be deleted explicitly */
    };

    static Manager* instance;

    explicit Manager(QObject* parent);
    ~Manager();

    Observer* observer() const;
    MetaHandlerCollector* collector() const;
    const QMap<Handler*, Data>& storage() const;
    QWidget* editor(Handler* handler);

    ChannelEditor* channelEditor() override;
    QList<Handler*> getHandlers() override;
    PathRetriever* pathRetriever(const QString& type) override;

public slots:
    // -----------------------
    // metahandlers management
    // -----------------------

    Handler* loadHandler(MetaHandler* meta, const HandlerConfiguration& config);
    Handler* loadHandler(const QString& type, const HandlerConfiguration& config);

    // -----------------
    // handlers features
    // -----------------

    void setHandlerOpen(Handler* handler, bool open);
    void setHandlerState(Handler* handler, Handler::state_type state, bool open);
    void toggleHandler(Handler* handler);
    void toggleHandler(Handler* handler, Handler::state_type state);
    void renameHandler(Handler* handler, const QString& name);
    bool editHandler(Handler* handler); /*!< returns false if handler has no editor */
    void setParameters(Handler* handler, const HandlerView::Parameters& parameters);

    // -------------------
    // handlers management
    // -------------------

    void insertHandler(Handler* handler, HandlerEditor* editor, const QString& type, const QString& group);
    void removeHandler(Handler* handler);

    // --------------------
    // listeners management
    // --------------------

    void setListeners(Handler* handler, Listeners listeners);

    void insertConnection(Handler* tail, Handler* head, const Filter& filter = {});
    void removeConnection(Handler* tail, Handler* head);
    void removeConnection(Handler* tail, Handler* head, Handler* source);

signals:
    void handlerInserted(Handler* handler);
    void handlerRenamed(Handler* handler);
    void handlerRemoved(Handler* handler);
    void handlerListenersChanged(Handler* handler);

private:
    void quit();

    Holder* holderAt(const QString& group);

    QMap<QString, PathRetriever*> mPathRetrievers;
    QMap<Handler*, Data> mStorage;
    std::vector<std::unique_ptr<StandardHolder>> mHolders; /*!< holder-pool */
    MetaHandlerCollector* mCollector;
    GraphicalHolder* mGUIHolder; /*!< holder using Qt event loop */
    ChannelEditor* mChannelEditor;
    DefaultReceiver* mReceiver;

};

#endif // QCORE_MANAGER_H
