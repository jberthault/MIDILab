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
#include "qcore/configuration.h"
#include "qtools/displayer.h"

//=========
// Manager
//=========

class SingleDisplayer;

class Manager : public Context {

    Q_OBJECT

public:
    static Manager* instance;

    // structors

    explicit Manager(QObject* parent);
    ~Manager();

    // accessors

    MultiDisplayer* mainDisplayer() const;
    Observer* observer() const;
    MetaHandlerCollector* collector() const;

    // context

    ChannelEditor* channelEditor() override;
    const HandlerProxies& getProxies() const override;
    PathRetriever* pathRetriever(const QString& type) override;

    // configuration

    Configuration getConfiguration();
    void setConfiguration(const Configuration& configuration);
    void clearConfiguration();

    // proxies

    HandlerProxy loadHandler(MetaHandler* meta, const QString& name, SingleDisplayer* host, const QString& group);
    HandlerProxy loadHandler(const QString& type, const QString& name, SingleDisplayer* host, const QString& group);

    void insertHandler(const HandlerProxy& proxy, const QString& group);
    void removeHandler(Handler* handler);

    // signaling commands

    void renameHandler(Handler* handler, const QString& name);

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
    HandlerProxy takeProxy(const Handler* handler);
    Holder* getHolder(const QString& group);
    void quit();

    HandlerProxies mHandlers;
    std::vector<StandardHolder*> mHolders;
    std::map<QString, PathRetriever*> mPathRetrievers;
    MetaHandlerCollector* mCollector;
    GraphicalHolder* mGUIHolder;
    ChannelEditor* mChannelEditor;
    DefaultReceiver* mReceiver;

};

#endif // QCORE_MANAGER_H
