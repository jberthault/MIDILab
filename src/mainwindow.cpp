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

#include <QApplication>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QtGlobal>
#include "mainwindow.h"
#include "qcore/core.h"
#include "qcore/configuration.h"
#include "qcore/managereditor.h"
#include "qtools/displayer.h"
#include "qtools/misc.h"
#include "handlers/systemhandler.h"
#include "qhandlers/handlers.h"

namespace {

//=====================
// ConfigurationPuller
//=====================

class ConfigurationPuller {

public:
    ConfigurationPuller(MultiDisplayer* mainDisplayer) : mMainDisplayer(mainDisplayer) {

    }

    void addConfiguration(const Configuration& configuration) {
        TRACE_MEASURE("applying configuration");
        // add frames
        if (!configuration.frames.empty())
            setFrame(mMainDisplayer, configuration.frames[0], true);
        for (int i=1 ; i < configuration.frames.size() ; i++)
            addFrame(mMainDisplayer, configuration.frames[i], true);
        // add handlers
        for (const auto& handler : configuration.handlers)
            addHandler(handler);
        // add connections
        for (const auto& connection : configuration.connections)
            addConnection(connection);
        // set colors
        for (channel_t c=0 ; c < qMin(0x10, configuration.colors.size()) ; ++c)
            Manager::instance->channelEditor()->setColor(c, configuration.colors.at(c));
        // display visible frames created
        for (auto displayer : mVisibleDisplayers)
            displayer->show();
    }

    void addConnection(const Configuration::Connection& connection) {
        bool hasSource = !connection.source.isEmpty();
        Handler* tail = mHandlersReferences.value(connection.tail, nullptr);
        Handler* head = mHandlersReferences.value(connection.head, nullptr);
        Handler* source = hasSource ? mHandlersReferences.value(connection.source, nullptr) : nullptr;
        if (!tail || !head || hasSource && !source) {
            qWarning() << "wrong connection handlers: " << handlerName(tail) << handlerName(head) << handlerName(source);
            return;
        }
        Manager::instance->insertConnection(tail, head, hasSource ? Filter::handler(source) : Filter());
    }

    void addHandler(const Configuration::Handler& handler) {
        // special treatment for system handlers (just register the id)
        if (handler.type == "System") {
            for (Handler* h : Manager::instance->getHandlers()) {
                if (handlerName(h) == handler.name) {
                    mHandlersReferences[handler.id] = h;
                    return;
                }
            }
            qWarning() << "Unknown system handler";
            return;
        }
        // prepare config
        HandlerConfiguration hc(handler.name);
        hc.host = mViewReferences.value(handler.id, nullptr);
        for (const auto& prop : handler.properties)
            hc.parameters.push_back(HandlerView::Parameter{prop.key, prop.value});
        hc.group = handler.group;
        // create the handler
        Handler* h = Manager::instance->loadHandler(handler.type, hc);
        if (h)
            mHandlersReferences[handler.id] = h;
    }

    void addWidget(MultiDisplayer* parent, const Configuration::Widget& widget) {
        if (widget.isFrame)
            addFrame(parent, widget.frame, false);
        else
            addView(parent, widget.view);
    }

    void addFrame(MultiDisplayer* parent, const Configuration::Frame& frame, bool isTopLevel) {
        MultiDisplayer* displayer = isTopLevel ? parent->insertDetached() : parent->insertMulti();
        setFrame(displayer, frame, isTopLevel);
        if (isTopLevel && frame.visible)
            mVisibleDisplayers.push_back(displayer);
    }

    void setFrame(MultiDisplayer* displayer, const Configuration::Frame& frame, bool isTopLevel) {
        displayer->setOrientation(frame.layout);
        for (const auto& widget : frame.widgets)
            addWidget(displayer, widget);
        if (isTopLevel) {
            auto window = displayer->window();
            window->setWindowTitle(frame.name);
            if (frame.size.isValid())
                window->resize(frame.size);
            if (!frame.pos.isNull())
                window->move(frame.pos);
        }
    }

    void addView(MultiDisplayer* parent, const Configuration::View& view) {
        mViewReferences[view.ref] = parent->insertSingle();
    }

private:
    MultiDisplayer* mMainDisplayer;
    QMap<QString, Handler*> mHandlersReferences;
    QMap<QString, SingleDisplayer*> mViewReferences;
    std::vector<MultiDisplayer*> mVisibleDisplayers;

};

//=====================
// ConfigurationPuller
//=====================

class ConfigurationPusher {

public:

    struct Info {
        Handler* handler;
        Manager::Data data;
        Configuration::Handler parsingData;
    };

    ConfigurationPusher(MultiDisplayer* mainDisplayer) : mMainDisplayer(mainDisplayer) {
        int id=0;
        const auto& storage = Manager::instance->storage();
        QMapIterator<Handler*, Manager::Data> it(storage);
        mCache.resize(storage.size());
        for (auto& info : mCache) {
            it.next();
            info.handler = it.key();
            info.data = it.value();
            info.parsingData.type = info.data.type;
            info.parsingData.id = QString("#%1").arg(id++);
            info.parsingData.name = handlerName(info.handler);
            auto holder = dynamic_cast<StandardHolder*>(info.handler->holder());
            if (holder)
                info.parsingData.group = QString::fromStdString(holder->name());
            HandlerView* view = dynamic_cast<GraphicalHandler*>(info.handler);
            if (!view)
                view = dynamic_cast<HandlerView*>(info.data.editor);
            if (view)
                for (const auto& parameter : view->getParameters())
                    info.parsingData.properties.push_back(Configuration::Property{parameter.name, parameter.value});
        }
    }

    auto getSource(const Filter& filter) const {
        QStringList sources;
        if (boost::logic::indeterminate(filter.match_nothing()))
            for(const auto& info : mCache)
                if (filter.match_handler(info.handler))
                    sources.append(info.parsingData.id);
        return sources.join("|");
    }

    const Info& getInfo(const Handler* handler) const {
        return *std::find_if(mCache.begin(), mCache.end(), [=](const auto& info) { return handler == info.handler; });
    }

    Configuration::Connection getConnection(const QString& tailId, const Listener& listener) const {
        return {tailId, getInfo(listener.handler).parsingData.id, getSource(listener.filter)};
    }

    Configuration::View getView(SingleDisplayer* displayer) const {
        for (const auto& info : mCache)
            if (displayer->widget() == dynamic_cast<GraphicalHandler*>(info.handler) || displayer->widget() == info.data.editor)
                return {info.parsingData.id};
    }

    Configuration::Frame getFrame(MultiDisplayer* displayer) const {
        auto window = displayer->window();
        Configuration::Frame frame;
        frame.layout = displayer->orientation();
        frame.name = window->windowTitle();
        frame.pos = window->pos();
        frame.size = window->size();
        frame.visible = window->isVisible();
        for (Displayer* child : displayer->directChildren())
            frame.widgets.append(getWidget(child));
        return frame;
    }

    Configuration::Widget getWidget(Displayer* displayer) const {
        auto multiDisplayer = dynamic_cast<MultiDisplayer*>(displayer);
        if (multiDisplayer)
            return {true, getFrame(multiDisplayer), {}};
        else
            return {false, {}, getView(static_cast<SingleDisplayer*>(displayer))};
    }

     auto getConfiguration() const {
         Configuration config;
         // handlers
         for(const auto& info : mCache)
             config.handlers.append(info.parsingData);
         // connections
         for(const auto& info : mCache)
             for (const auto& listener : info.handler->listeners())
                 config.connections.append(getConnection(info.parsingData.id, listener));
         // frames
         config.frames.append(getFrame(mMainDisplayer));
         for (auto displayer : MultiDisplayer::topLevelDisplayers())
             config.frames.append(getFrame(displayer));
         // colors
         for (channel_t c=0 ; c < 0x10 ; ++c)
             config.colors.append(Manager::instance->channelEditor()->color(c));
         return config;
    }

private:
     std::vector<Info> mCache;
     MultiDisplayer* mMainDisplayer;

};

const QVariant discardConfigsData = 1;

}

// ============
// AboutWindow
// ============

static const QString aboutText =
    "<p><b>MIDILab</b> (version " MIDILAB_VERSION_STRING ")</p>"
    "<p>A versatile MIDI laboratory</p>"
    "<ul>"
    " <li>MIDI controller: connect multiple devices</li>"
    " <li>MIDI player: play files or previous records</li>"
    " <li>MIDI recorder: save your playing sessions (<i>not quite finished</i>)</li>"
    " <li>MIDI editor: make or edit songs (<i>not quite started</i>)</li>"
    " <li>...</li>"
    "</ul>"
    "<p>This program is free software.<br/>"
    "It is licensed under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL v3</a>.<br/>"
    "The project is hosted on <a href=\"https://github.com/jberthault/MIDILab\">Github</a></p>"
    "<p>This project uses:</p>"
    "<ul>"
    " <li>Qt Project: see related About</li>"
    " <li><a href=\"http://www.fluidsynth.org\">fluidsynth " MIDILAB_FLUIDSYNTH_VERSION_STRING "</a>: SoundFont Synthetizer</li>"
    " <li><a href=\"https://github.com/iconic/open-iconic\">Open Iconic 1.1.1</a>: A great icon set</li>"
    "</ul>"
    "<p>Copyright \u00a9 2017 Julien Berthault</p>"
    "<p><i> " MIDILAB_MODE " " MIDILAB_PLATFORM " " MIDILAB_SIZE " </i></p>";

AboutWindow::AboutWindow(QWidget* parent) :
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint) {

    setWindowTitle("About");

    auto textLabel = new QLabel(aboutText, this);
    textLabel->setOpenExternalLinks(true);

    auto iconLabel = new QLabel(this);
    QIcon icon = windowIcon();
    QSize size = icon.actualSize(QSize(64, 64));
    iconLabel->setPixmap(icon.pixmap(size));

    auto aboutQtButton = new QPushButton("About Qt", this);
    auto okButton = new QPushButton("OK");
    connect(aboutQtButton, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    connect(okButton, &QPushButton::clicked, this, &AboutWindow::close);

    setLayout(make_vbox(make_hbox(make_vbox(iconLabel, stretch_tag{}), textLabel), make_hbox(stretch_tag{}, aboutQtButton, okButton)));
}

//============
// MainWindow
//============

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    new Manager(this);
    Manager::instance->collector()->addFactory(new StandardFactory(this));
    mManagerEditor = new ManagerEditor(this);
    mProgramEditor = new ProgramEditor(Manager::instance->channelEditor(), this);
    setCentralWidget(new MultiDisplayer(Qt::Horizontal, this));
    setupMenu();
}

QStringList MainWindow::getConfigs() const {
    QSettings settings;
    return settings.value("config").toStringList();
}

void MainWindow::readLastConfig() {
    auto configurations = getConfigs();
    auto fileName = configurations.empty() ? QString(":/data/config.xml") : configurations.front();
    if (readConfig(fileName) && !configurations.empty())
        Manager::instance->pathRetriever("configuration")->setSelection(fileName);
}

bool MainWindow::readConfig(const QString& fileName) {
    Configuration config;
    // make system handlers
    auto system_handlers = create_system();
    for (Handler* handler : system_handlers)
        Manager::instance->insertHandler(handler, nullptr, "System", "default");
    // read configuration
    try {
        QFile file(fileName);
        config = Configuration::read(&file);
    } catch (const QString& error) {
        QMessageBox::critical(this, QString(), QString("Failed reading configuration file\n%1\n\n%2").arg(fileName, error));
        return false;
    }
    // apply configuration
    ConfigurationPuller puller(static_cast<MultiDisplayer*>(centralWidget()));
    puller.addConfiguration(config);
    // redo the layout
    mManagerEditor->graphEditor()->graph()->doLayout();
    return true;
}

void MainWindow::loadConfig() {
    QString fileName = Manager::instance->pathRetriever("configuration")->getReadFile(this);
    if (!fileName.isEmpty())
        loadConfig(fileName);
}

void MainWindow::loadConfig(const QString& fileName) {
    raiseConfig(fileName);
    close();
    QProcess::startDetached(QString("\"%1\"").arg(qApp->arguments()[0]));
}

void MainWindow::writeConfig() {
    QString fileName = Manager::instance->pathRetriever("configuration")->getWriteFile(this);
    if (fileName.isEmpty())
        return;
    ConfigurationPusher pusher(static_cast<MultiDisplayer*>(centralWidget()));
    auto config = pusher.getConfiguration();
    QSaveFile saveFile(fileName);
    saveFile.open(QSaveFile::WriteOnly);
    Configuration::write(&saveFile, config);
    if (saveFile.commit()) {
        raiseConfig(fileName);
        updateConfigs();
    }
}

void MainWindow::raiseConfig(const QString& fileName) {
    QSettings settings;
    QVariant variant = settings.value("config");
    QStringList configurations = variant.toStringList();
    // move or insert file at the top
    configurations.removeAll(fileName);
    configurations.prepend(fileName);
    // restrict configuration size
    while (configurations.size() > 8)
        configurations.removeLast();
    settings.setValue("config", configurations);
}

void MainWindow::updateConfigs() {
    auto configs = getConfigs();
    mConfigMenu->clear();
    if (!configs.isEmpty()) {
        for (const QString& configurationFile : configs) {
            QFileInfo fileInfo(configurationFile);
            QAction* fileAction = mConfigMenu->addAction(QIcon(":/data/grid-three-up.svg"), fileInfo.completeBaseName());
            fileAction->setData(configurationFile);
            fileAction->setToolTip(configurationFile);
        }
        mConfigMenu->addSeparator();
    }
    QAction* clearAction = mConfigMenu->addAction(QIcon(":/data/trash.svg"), "Clear");
    clearAction->setEnabled(!configs.isEmpty());
}

void MainWindow::onConfigSelection(QAction* action) {
    auto fileName = action->data().toString();
    if (fileName.isNull()) { // clear action triggerred
        auto result = QMessageBox::question(this, QString(), "Do you want to clear configurations ?");
        if (result == QMessageBox::Yes) {
            QSettings settings;
            settings.remove("config");
            updateConfigs();
         }
    } else {
        auto result = QMessageBox::question(this, QString(), "Do you want to reload the application ?");
        if (result == QMessageBox::Yes)
            loadConfig(fileName);
    }
}

void MainWindow::setupMenu() {

    MultiDisplayer* displayer = static_cast<MultiDisplayer*>(centralWidget());

//    QToolBar* toolbar = addToolBar("quick actions");

    QMenuBar* menu = new QMenuBar(this);
    setMenuBar(menu);

    // extends menu
    QMenu* fileMenu = menu->addMenu("File");
    fileMenu->addAction(QIcon(":/data/cloud-download.svg"), "Load configuration", this, SLOT(loadConfig()));
    fileMenu->addAction(QIcon(":/data/cloud-upload.svg"), "Save configuration", this, SLOT(writeConfig()));
    mConfigMenu = fileMenu->addMenu(QIcon(":/data/cloud.svg"), "Recent configurations");
    mConfigMenu->setToolTipsVisible(true);
    connect(mConfigMenu, &QMenu::triggered, this, &MainWindow::onConfigSelection);
    updateConfigs();

    // add recent config / midi files loaded
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon(":/data/power-standby.svg"), "Exit", this, SLOT(close()));

    QMenu* handlersMenu = menu->addMenu("Handlers");
    handlersMenu->addAction(mManagerEditor->windowIcon(), "Handlers", mManagerEditor, SLOT(show()));
    handlersMenu->addAction(mProgramEditor->windowIcon(), "Programs", mProgramEditor, SLOT(show()));
    handlersMenu->addSeparator();
    handlersMenu->addAction(QIcon(":/data/target.svg"), "Panic", this, SLOT(panic()));

    QMenu* interfaceMenu = menu->addMenu("Interface");
    interfaceMenu->addAction(Manager::instance->channelEditor()->windowIcon(), Manager::instance->channelEditor()->windowTitle(), Manager::instance->channelEditor(), SLOT(show()));
    // interfaceMenu->addAction(QIcon(":/data/cog.svg"), "Settings", this, SLOT(unimplemented()));

    QIcon lockIcon;
    lockIcon.addFile(":/data/lock-locked.svg", QSize(), QIcon::Normal, QIcon::On);
    lockIcon.addFile(":/data/lock-unlocked.svg", QSize(), QIcon::Normal, QIcon::Off);
    QAction* lockAction = new QAction(lockIcon, "Lock Layout", this);
    lockAction->setCheckable(true);
    lockAction->setChecked(displayer->isLocked());
    connect(lockAction, &QAction::toggled, displayer, &MultiDisplayer::setLocked);
    interfaceMenu->addAction(lockAction);

    interfaceMenu->addAction(QIcon(":/data/plus.svg"), "Add Container", this, SLOT(newDisplayer()));

    /// show windows opened ...

    QMenu* helpMenu = menu->addMenu("Help");
    helpMenu->addAction(QIcon(":/data/question-mark.svg"), "Help", this, SLOT(unimplemented()));
    // helpMenu->addAction("What's new ?", this, SLOT(unimplemented()));
    helpMenu->addSeparator();
    helpMenu->addAction(QIcon(":/data/info.svg"), "About", this, SLOT(about()));

}

void MainWindow::addFiles(const QStringList& files) {
    if (files.empty())
        return;
    QMapIterator<Handler*, Manager::Data> it(Manager::instance->storage());
    while (it.hasNext()) {
        it.next();
        if (it.value().type == "Player") {
            Manager::instance->setParameters(it.key(), {HandlerView::Parameter{"playlist", files.join(";")}});
            return;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    for (auto displayer : MultiDisplayer::topLevelDisplayers())
        displayer->close();
    QMainWindow::closeEvent(event);
}

void MainWindow::newDisplayer() {
    static_cast<MultiDisplayer*>(centralWidget())->insertDetached()->show();
}

void MainWindow::about() {
    auto aboutWindow = new AboutWindow(this);
    aboutWindow->setAttribute(Qt::WA_DeleteOnClose);
    aboutWindow->exec();
}

void MainWindow::panic() {
    for (Handler* handler : Manager::instance->getHandlers())
        Manager::instance->setHandlerOpen(handler, false);
}

void MainWindow::unimplemented() {
    QMessageBox::warning(this, QString(), "Feature not implemented yet");
}
