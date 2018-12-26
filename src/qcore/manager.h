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

#ifndef QCORE_MANAGER_H
#define QCORE_MANAGER_H

#include "qcore/core.h"
#include "qcore/editors.h"
#include "qcore/configuration.h"
#include "qtools/displayer.h"

//=========
// Deleter
//=========

class Deleter final : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    void addProxy(const HandlerProxy& proxy);
    void addProxies(const HandlerProxies& proxies);

    void startDeletion();
    void stopDeletion();

signals:
    void deleted();

protected:
    void timerEvent(QTimerEvent*) override;

private:
    bool deleteProxies();

private:
    HandlerProxies mProxies;
    int mTimerId {0};

};

//=========
// Manager
//=========

class SingleDisplayer;

class Manager final : public Context {

    Q_OBJECT

public:
    explicit Manager(QObject* parent);

    // accessors

    MultiDisplayer* mainDisplayer() const;
    Observer* observer() const;
    MetaHandlerPool* metaHandlerPool() const;

    // context

    ChannelEditor* channelEditor() override;
    const HandlerProxies& handlerProxies() const override;
    PathRetrieverPool* pathRetrieverPool() override;
    QToolBar* quickToolBar() override;
    QSystemTrayIcon* systemTrayIcon() override;

    void setChannelEditor(ChannelEditor* editor);
    void setQuickToolBar(QToolBar* toolbar);
    void setSystemTrayIcon(QSystemTrayIcon* tray);

    // configuration

    Configuration getConfiguration();
    void setConfiguration(const Configuration& configuration);
    void clearConfiguration();

    // proxies

    HandlerProxy loadHandler(MetaHandler* meta, const QString& name, SingleDisplayer* host);
    HandlerProxy loadHandler(const QString& type, const QString& name, SingleDisplayer* host);
    void removeHandler(Handler* handler);

    // signaling commands

    void renameHandler(Handler* handler, const QString& name);

    void setListeners(Handler* handler, Listeners listeners);

    void insertConnection(Handler* tail, Handler* head, const Filter& filter = {});
    void removeConnection(Handler* tail, Handler* head);
    void removeConnection(Handler* tail, Handler* head, Handler* source);

private:
    void onDeletion();

    HandlerProxies mHandlerProxies;
    PathRetrieverPool* mPathRetrieverPool;
    MetaHandlerPool* mMetaHandlerPool;
    GraphicalSynchronizer* mGUISynchronizer;
    StandardSynchronizer<2> mDefaultSynchronizer; /*!< 2 threads are enough */
    Deleter* mDeleter;
    Observer* mObserver;
    SignalNotifier* mSignalNotifier;
    QSystemTrayIcon* mSystemTrayIcon {nullptr};
    ChannelEditor* mChannelEditor {nullptr};
    QToolBar* mQuickToolbar {nullptr};
};

#endif // QCORE_MANAGER_H
