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

#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <boost/version.hpp>
#include "mainwindow.h"
#include "qhandlers/handlers.h"
#include "qtools/displayer.h"

namespace {

auto getSystemTrayIconPreferredVisibility() {
    QSettings settings;
    return settings.value("withTrayIcon", true).toBool();
}

void setSystemTrayIconPreferredVisibility(bool visibility) {
    QSettings settings;
    settings.setValue("withTrayIcon", visibility);
}

auto getConfigs() {
    QSettings settings;
    return settings.value("config").toStringList();
}

auto* addShowAction(QMenu* menu, QWidget* widget) {
    return menu->addAction(widget->windowIcon(), widget->windowTitle(), widget, SLOT(show()));
}

auto* addShowAction(QMenu* menu, QWidget* widget, const QKeySequence& keySequence) {
    auto* action = menu->addAction(widget->windowIcon(), widget->windowTitle());
    QObject::connect(action, &QAction::triggered, widget, [=] {
        widget->setVisible(!widget->isVisible());
        if (widget->isVisible())
            widget->raise();
    });
    action->setShortcut(keySequence);
    action->setShortcutContext(Qt::ApplicationShortcut);
    return action;
}

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
    " <li>Qt</li>"
    " <li><a href=\"http://www.boost.org\">Boost</a> (version " BOOST_LIB_VERSION ")</li>"
    " <li><a href=\"http://www.fluidsynth.org\">FluidSynth</a> (version " MIDILAB_FLUIDSYNTH_VERSION_STRING ")</li>"
    " <li><a href=\"https://github.com/iconic/open-iconic\">Open Iconic</a> (version 1.1.1)</li>"
    "</ul>"
    "<p>Copyright \u00a9 2017-2019 Julien Berthault</p>"
    "<p><i> " MIDILAB_MODE " " MIDILAB_PLATFORM " " MIDILAB_SIZE " </i></p>";

AboutWindow::AboutWindow(QWidget* parent) :
    QDialog{parent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint} {

    setWindowTitle("About");

    auto* textLabel = new QLabel{aboutText, this};
    textLabel->setOpenExternalLinks(true);

    auto* iconLabel = new QLabel{this};
    const auto icon = windowIcon();
    const auto size = icon.actualSize(QSize{64, 64});
    iconLabel->setPixmap(icon.pixmap(size));

    auto* aboutQtButton = new QPushButton{"About Qt", this};
    auto* okButton = new QPushButton{"OK"};
    connect(aboutQtButton, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    connect(okButton, &QPushButton::clicked, this, &AboutWindow::close);

    setLayout(make_vbox(make_hbox(make_vbox(iconLabel, stretch_tag{}), textLabel), make_hbox(stretch_tag{}, aboutQtButton, okButton)));
}

//============
// MainWindow
//============

MainWindow::MainWindow(QWidget* parent) : QMainWindow{parent} {

    // configure manager

    mManager = new Manager{this};
    mManager->metaHandlerPool()->addFactory(new StandardFactory{this});

    auto* channelEditor = new ChannelEditor{this};
    mManager->setChannelEditor(channelEditor);

    // configure windows

    mGraphEditor = new HandlerGraphEditor{mManager, this};
    auto* listEditor = new HandlerListEditor{mManager, this};
    auto* catalogEditor = new HandlerCatalogEditor{mManager, this};
    auto* programEditor = new ProgramEditor{mManager, this};

    setCentralWidget(new MultiDisplayer{Qt::Horizontal, this});

    // configure toolbars

    auto* menuToolBar = addToolBar("Menu actions");
    auto* handlerToolBar = addToolBar("Handler actions");
    mManager->setQuickToolBar(handlerToolBar);

    // configure menu

    auto* menu = new QMenuBar{this};
    auto* fileMenu = menu->addMenu("File");
    auto* handlersMenu = menu->addMenu("Handlers");
    auto* interfaceMenu = menu->addMenu("Interface");
    auto* helpMenu = menu->addMenu("Help");
    setMenuBar(menu);

    // configure file menu

    fileMenu->addAction(QIcon{":/data/rain.svg"}, "Unload configuration", this, SLOT(unloadConfig()));
    fileMenu->addAction(QIcon{":/data/cloud-download.svg"}, "Load configuration", this, SLOT(loadConfig()));
    fileMenu->addAction(QIcon{":/data/cloud-upload.svg"}, "Save configuration", this, SLOT(saveConfig()));
    mConfigMenu = fileMenu->addMenu(QIcon{":/data/cloud.svg"}, "Recent configurations");
    mConfigMenu->setToolTipsVisible(true);
    connect(mConfigMenu, &QMenu::triggered, this, &MainWindow::onConfigSelection);
    updateMenu(getConfigs()); // add recent config / midi files loaded
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(QIcon{":/data/power-standby.svg"}, "Exit", this, SLOT(closeRequest()), Qt::ALT | Qt::Key_Q);
    exitAction->setShortcutContext(Qt::ApplicationShortcut);

    // configure handler menu

    handlersMenu->setToolTipsVisible(true);
    addShowAction(handlersMenu, listEditor, Qt::ALT | Qt::Key_V)->setToolTip("Edit existing handlers");
    addShowAction(handlersMenu, mGraphEditor, Qt::ALT | Qt::Key_C)->setToolTip("Edit how handlers communicate");
    addShowAction(handlersMenu, catalogEditor, Qt::ALT | Qt::Key_N)->setToolTip("Create new handlers");
    handlersMenu->addSeparator();
    addShowAction(handlersMenu, programEditor, Qt::ALT | Qt::Key_B)->setToolTip("Edit programs set on each handler");
    handlersMenu->addSeparator();
    auto* panicAction = handlersMenu->addAction(QIcon{":/data/target.svg"}, "Panic", this, SLOT(panic()), Qt::ALT | Qt::Key_X);
    panicAction->setShortcutContext(Qt::ApplicationShortcut);
    panicAction->setToolTip("Close all handlers");
    menuToolBar->addAction(panicAction);

    // configure interface menu

    addShowAction(interfaceMenu, mManager->channelEditor());
    // interfaceMenu->addAction(QIcon{":/data/cog.svg"}, "Settings", this, SLOT(unimplemented()));

    auto* lockAction = new MultiStateAction{this};
    lockAction->addState(QIcon{":/data/lock-locked.svg"}, "Unlock layout"); // default is locked
    lockAction->addState(QIcon{":/data/lock-unlocked.svg"}, "Lock layout");
    connect(lockAction, &MultiStateAction::stateChanged, this, &MainWindow::onLockStateChange);
    interfaceMenu->addAction(lockAction);
    menuToolBar->addAction(lockAction);

    interfaceMenu->addAction(QIcon{":/data/plus.svg"}, "Add container", this, SLOT(newDisplayer()));

    const bool isSystemTrayVisible = getSystemTrayIconPreferredVisibility();
    setupSystemTray(isSystemTrayVisible);
    auto* systemTrayAction = interfaceMenu->addAction("Show system tray");
    systemTrayAction->setCheckable(true);
    systemTrayAction->setChecked(isSystemTrayVisible);
    connect(systemTrayAction, &QAction::toggled, this, &MainWindow::setSystemTrayVisible);

    // configure help menu

    helpMenu->addAction(QIcon{":/data/question-mark.svg"}, "Help", this, SLOT(unimplemented()));
    helpMenu->addSeparator();
    helpMenu->addAction(QIcon{":/data/info.svg"}, "About", this, SLOT(about()));

}

void MainWindow::unloadConfig() {
    if (QMessageBox::question(this, {}, "Do you want to unload the current configuration ?") == QMessageBox::Yes)
        clearConfig();
}

void MainWindow::loadConfig() {
    const auto fileName = mManager->pathRetrieverPool()->get("configuration")->getReadFile(this);
    if (!fileName.isEmpty())
        readConfig(fileName, true, false, true);
}

void MainWindow::saveConfig() {
    const auto configurations = getConfigs();
    const auto defaultfileName = configurations.empty() ? QString{} : configurations.front();
    const auto fileName = mManager->pathRetrieverPool()->get("configuration")->getWriteFile(this, defaultfileName);
    if (!fileName.isEmpty())
        writeConfig(fileName);
}

void MainWindow::clearConfig() {
    // If the main displayer contains views, we want it to be deleted by the manager.
    // We have to detach it from the mainwindow to enable this deletion.
    // We replace it by another main displayer.
    auto* previousDisplayer = dynamic_cast<MultiDisplayer*>(centralWidget());
    if (previousDisplayer && !previousDisplayer->isEmpty()) {
        auto* newDisplayer = new MultiDisplayer{Qt::Horizontal, this};
        newDisplayer->setLocked(previousDisplayer->isLocked());
        previousDisplayer->setParent(nullptr);
        setCentralWidget(newDisplayer);
    }
    // Close existing displayers to make space for the next configuration
    // It also deletes the empty ones
    // The others will be deleted by the manager
    for (auto* displayer : MultiDisplayer::topLevelDisplayers())
        displayer->close();
    // Clears configurations asynchronously
    mManager->clearConfiguration();
}

void MainWindow::readLastConfig(bool clear) {
    const auto configurations = getConfigs();
    const auto fileName = configurations.empty() ? QString{":/data/config.xml"} : configurations.front();
    readConfig(fileName, false, !configurations.empty(), clear);
}

void MainWindow::readConfig(const QString& fileName, bool raise, bool select, bool clear) {
    Configuration config;
    // read configuration
    try {
        QFile file{fileName};
        config = Configuration::read(&file);
    } catch (const QString& error) {
        QMessageBox::critical(this, {}, QString{"Failed reading configuration\n%1\n\n%2"}.arg(fileName, error));
        return;
    }
    // clear previous configuration
    if (clear)
        clearConfig();
    // set the new configuration
    mManager->setConfiguration(config);
    // redo the layout
    mGraphEditor->graph()->doLayout();
    // update config order and retriever
    if (raise)
        raiseConfig(fileName);
    if (select)
        mManager->pathRetrieverPool()->get("configuration")->setSelection(fileName);
}

void MainWindow::writeConfig(const QString& fileName) {
    bool saved = false;
    const auto config = mManager->getConfiguration();
    QSaveFile saveFile{fileName};
    if (saveFile.open(QSaveFile::WriteOnly)) {
        Configuration::write(&saveFile, config);
        saved = saveFile.commit();
    }
    if (saved) {
        raiseConfig(fileName);
        QMessageBox::information(this, {}, "Configuration saved");
    } else {
        QMessageBox::critical(this, {}, "Failed writing configuration");
    }
}

void MainWindow::raiseConfig(const QString& fileName) {
    QSettings settings;
    auto configurations = settings.value("config").toStringList();
    // move or insert file at the top
    configurations.removeAll(fileName);
    configurations.prepend(fileName);
    // restrict configuration size
    while (configurations.size() > 8)
        configurations.removeLast();
    settings.setValue("config", configurations);
    // update menu
    updateMenu(configurations);
}

void MainWindow::updateMenu(const QStringList& configs) {
    mConfigMenu->clear();
    if (!configs.isEmpty()) {
        for (const auto& configurationFile : configs) {
            const QFileInfo fileInfo{configurationFile};
            auto* fileAction = mConfigMenu->addAction(QIcon{":/data/grid-three-up.svg"}, fileInfo.completeBaseName());
            fileAction->setData(configurationFile);
            fileAction->setToolTip(configurationFile);
        }
        mConfigMenu->addSeparator();
    }
    auto* clearAction = mConfigMenu->addAction(QIcon{":/data/trash.svg"}, "Clear");
    clearAction->setEnabled(!configs.isEmpty());
}

void MainWindow::about() {
    auto* aboutWindow = new AboutWindow{this};
    aboutWindow->setAttribute(Qt::WA_DeleteOnClose);
    aboutWindow->exec();
}

void MainWindow::panic() {
    for (const auto& proxy : mManager->handlerProxies())
        proxy.sendCommand(HandlerProxy::Command::Close);
}

void MainWindow::newDisplayer() {
    if (auto* displayer = dynamic_cast<MultiDisplayer*>(centralWidget()))
        displayer->insertDetached()->show();
}

void MainWindow::unimplemented() {
    QMessageBox::warning(this, {}, "Feature not implemented yet");
}

void MainWindow::addFiles(const QStringList& files) {
    if (files.empty())
        return;
    for (const auto& proxy : mManager->handlerProxies()) {
        if (proxy.metaHandler()->identifier() == "Player") {
            proxy.setParameter({"playlist", files.join(";")});
            return;
        }
    }
}

void MainWindow::setupSystemTray(bool visible) {
    if (!visible)
        return;
    auto* systemTrayIcon = new QSystemTrayIcon{qApp->windowIcon(), this};
    auto* systemTrayMenu = new QMenu{this};
    systemTrayMenu->addAction(QIcon{":/data/hide.svg"}, "Minimize", this, SLOT(minimizeAll()));
    systemTrayMenu->addAction(QIcon{":/data/eye.svg"}, "Restore", this, SLOT(restoreAll()));
    systemTrayMenu->addAction(QIcon{":/data/power-standby.svg"}, "Exit", this, SLOT(close()));
    systemTrayIcon->setContextMenu(systemTrayMenu);
    systemTrayIcon->show();
    mManager->setSystemTrayIcon(systemTrayIcon);
}

void MainWindow::setSystemTrayVisible(bool visible) {
    if (auto* systemTrayIcon = mManager->systemTrayIcon())
        systemTrayIcon->setVisible(visible);
    else
        setupSystemTray(visible);
    setSystemTrayIconPreferredVisibility(visible);
}

void MainWindow::onConfigSelection(QAction* action) {
    const auto fileName = action->data().toString();
    if (fileName.isNull()) { // clear action triggerred
        if (QMessageBox::question(this, {}, "Do you want to clear configurations ?") == QMessageBox::Yes) {
            QSettings settings;
            settings.remove("config");
            updateMenu({});
        }
    } else {
        if (QMessageBox::question(this, {}, "Do you want to load this configuration ?") == QMessageBox::Yes)
            readConfig(fileName, true, true, true);
    }
}

void MainWindow::onLockStateChange(int state) {
    if (auto* displayer = dynamic_cast<MultiDisplayer*>(centralWidget()))
        displayer->setLocked(state == 0);
}

void MainWindow::minimizeAll() {
    for (auto* displayer : MultiDisplayer::topLevelDisplayers())
        displayer->showMinimized();
    showMinimized();
}

void MainWindow::restoreAll() {
    for (auto* displayer : MultiDisplayer::topLevelDisplayers())
        displayer->showNormal();
    showNormal();
}

void MainWindow::closeRequest() {
    if (QMessageBox::question(this, {}, "Do you want to exit the application ?") == QMessageBox::Yes)
        close();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    for (auto* displayer : MultiDisplayer::topLevelDisplayers())
        displayer->close();
    QMainWindow::closeEvent(event);
}
