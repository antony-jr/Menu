/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "appmenuwidget.h"
#include "appmenu/menuimporteradaptor.h"
#include <QProcess>
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QX11Info>
#include <QApplication>

#include <QAbstractItemView>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPushButton>
#include <QStyle>

#include <KF5/KWindowSystem/KWindowSystem>
#include <KF5/KWindowSystem/KWindowInfo>
#include <KF5/KWindowSystem/NETWM>

#include "actionsearch/actionsearch.h"


AppMenuWidget::AppMenuWidget(QWidget *parent)
    : QWidget(parent)
{
    // QProcess *process = new QProcess(this);
    // process->start("/usr/bin/gmenudbusmenuproxy", QStringList());

    QHBoxLayout *layout = new QHBoxLayout;
    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_systemMenu = new QMenu("System");

    QAction *aboutAction = m_systemMenu->addAction("About This Computer");
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(actionAbout()));

    QMenu *submenuPrefs = m_systemMenu->addMenu("Preferences");

    QAction *displaysAction = submenuPrefs->addAction("Displays");
    connect(displaysAction, SIGNAL(triggered()), this, SLOT(actionDisplays()));

    QAction *shortcutsAction = submenuPrefs->addAction("Shortcuts");
    connect(shortcutsAction, SIGNAL(triggered()), this, SLOT(actionShortcuts()));

    QAction *soundAction = submenuPrefs->addAction("Sound");
    connect(soundAction, SIGNAL(triggered()), this, SLOT(actionSound()));

    QAction *logoutAction = m_systemMenu->addAction("Log Out");
    connect(logoutAction, SIGNAL(triggered()), this, SLOT(actionLogout()));

    m_menuBar = new QMenuBar(this);
    m_menuBar->setAttribute(Qt::WA_TranslucentBackground);
    m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_menuBar->setStyleSheet("background: transparent");
    m_menuBar->setFont(qApp->font());
    // layout->addWidget(m_buttonsWidget, 0, Qt::AlignVCenter);

    // Add System Menu
    integrateSystemMenu(m_menuBar);

    // Add search box to menu
    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setMinimumWidth(300);
    searchLineEdit->setStyleSheet("border-radius: 9px"); // FIXME: Does not seem to work here, why?
    searchLineEdit->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false);
    searchLineEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    searchLineEdit->setPlaceholderText("Search in Menu");
    searchLineEdit->setStyleSheet("background: white");

    layout->addWidget(m_menuBar, 0, Qt::AlignLeft);

    layout->addSpacing(10);

    searchLineWidget = new QWidget(this);
    searchLineWidget->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false);
    auto searchLineLayout = new QHBoxLayout(searchLineWidget);
    searchLineLayout->setContentsMargins(0, 0, 0, 0);
    // searchLineLayout->setSpacing(3);
    searchLineLayout->addWidget(searchLineEdit);
    searchLineWidget->setLayout(searchLineLayout);
    searchLineWidget->setObjectName("SearchLineWidget");

    layout->addWidget(searchLineWidget, 0, Qt::AlignRight);
    searchLineWidget->show();

    layout->setContentsMargins(0, 0, 0, 0);

    MenuImporter *menuImporter = new MenuImporter(this);
    menuImporter->connectToBus();

    m_appMenuModel = new AppMenuModel(this);
    connect(m_appMenuModel, &AppMenuModel::modelNeedsUpdate, this, &AppMenuWidget::updateMenu);

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppMenuWidget::delayUpdateActiveWindow);
    connect(KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
            this, &AppMenuWidget::onWindowChanged);

    // Load action search
    actionSearch = nullptr;
    actionCompleter = nullptr;
    updateActionSearch(m_menuBar);
}

AppMenuWidget::~AppMenuWidget() {
    if(actionSearch) {
        delete actionSearch;
    }
}

void AppMenuWidget::integrateSystemMenu(QMenuBar *menuBar) {
    if(!menuBar || !m_systemMenu)
        return;

    menuBar->addMenu(m_systemMenu);
}

void AppMenuWidget::handleActivated(const QString &name) {
    searchLineEdit->selectAll();
    searchLineEdit->clearFocus();
    searchLineEdit->clear();
    actionSearch->execute(name);
}

void AppMenuWidget::updateActionSearch(QMenuBar *menuBar) {
    if(!menuBar){
        return;
    }

    if(!actionSearch) {
        actionSearch = new ActionSearch;
    }

    /// Update the action search.
    actionSearch->clear();
    actionSearch->update(menuBar);

    /// Update completer
    if(actionCompleter) {
        actionCompleter->disconnect();
        actionCompleter->deleteLater();
    }

    actionCompleter = new QCompleter(actionSearch->getActionNames(), this);

    // Make the completer match search terms in the middle rather than just those at the beginning of the menu
    actionCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    actionCompleter->setFilterMode(Qt::MatchContains);

    // Style the completer; https://stackoverflow.com/a/38084484
    QAbstractItemView *popup = actionCompleter->popup();
    // popup->setStyleSheet("QListView { padding: 10px; margin: 10px; }"); // FIXME: This is just an example so far. QAbstractItemView describes the whole QListView widget, not the lines inside it
    /* Do the individual items get created only after this, and hence not get the styling? */

    // Set first result active; https://stackoverflow.com/q/17782277. FIXME: This does not work yet. Why?
    QItemSelectionModel* sm = new QItemSelectionModel(actionCompleter->completionModel());
    actionCompleter->popup()->setSelectionModel(sm);
    sm->select(actionCompleter->completionModel()->index(0,0), QItemSelectionModel::Select | QItemSelectionModel::Rows);

    popup->setAlternatingRowColors(true);
    searchLineEdit->setCompleter(actionCompleter);

    connect(actionCompleter,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &AppMenuWidget::handleActivated);
}

void AppMenuWidget::updateMenu()
{
    m_menuBar->clear();
    integrateSystemMenu(m_menuBar); // Insert the 'System' menu first

    if (!m_appMenuModel->menuAvailable()) {
        updateActionSearch(m_menuBar);
        return;
    }

    QMenu *menu = m_appMenuModel->menu();
    if (menu) {
        for (QAction *a : menu->actions()) {
            if (!a->isEnabled())
                continue;

            m_menuBar->addAction(a);
        }
    }

    updateActionSearch(m_menuBar);
}

void AppMenuWidget::toggleMaximizeWindow()
{
    KWindowInfo info(KWindowSystem::activeWindow(), NET::WMState);
    bool isMax = info.hasState(NET::Max);
    bool isWindow = !info.hasState(NET::SkipTaskbar) ||
            info.windowType(NET::UtilityMask) != NET::Utility ||
            info.windowType(NET::DesktopMask) != NET::Desktop;

    if (!isWindow)
        return;

    if (isMax) {
        restoreWindow();
    } else {
        maxmizeWindow();
    }
}

bool AppMenuWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ApplicationFontChange) {
        QMenu *menu = m_appMenuModel->menu();
        if (menu) {
            for (QAction *a : menu->actions()) {
                a->setFont(qApp->font());
            }
        }
        qDebug() << "gengxinle  !!!" << qApp->font().toString();
        m_menuBar->setFont(qApp->font());
        m_menuBar->update();
    }

    return QWidget::event(e);
}

bool AppMenuWidget::isAcceptWindow(WId id)
{
    QFlags<NET::WindowTypeMask> ignoreList;
    ignoreList |= NET::DesktopMask;
    ignoreList |= NET::DockMask;
    ignoreList |= NET::SplashMask;
    ignoreList |= NET::ToolbarMask;
    ignoreList |= NET::MenuMask;
    ignoreList |= NET::PopupMenuMask;
    ignoreList |= NET::NotificationMask;

    KWindowInfo info(id, NET::WMWindowType | NET::WMState, NET::WM2TransientFor | NET::WM2WindowClass);

    if (!info.valid())
        return false;

    if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), ignoreList))
        return false;

    if (info.state() & NET::SkipTaskbar)
        return false;

    // WM_TRANSIENT_FOR hint not set - normal window
    WId transFor = info.transientFor();
    if (transFor == 0 || transFor == id || transFor == (WId) QX11Info::appRootWindow())
        return true;

    info = KWindowInfo(transFor, NET::WMWindowType);

    QFlags<NET::WindowTypeMask> normalFlag;
    normalFlag |= NET::NormalMask;
    normalFlag |= NET::DialogMask;
    normalFlag |= NET::UtilityMask;

    return !NET::typeMatchesMask(info.windowType(NET::AllTypesMask), normalFlag);
}

void AppMenuWidget::delayUpdateActiveWindow()
{
    if (m_windowID == KWindowSystem::activeWindow())
        return;

    m_windowID = KWindowSystem::activeWindow();

    onActiveWindowChanged();
}

void AppMenuWidget::onActiveWindowChanged()
{
    KWindowInfo info(m_windowID, NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);
    // bool isMax = info.hasState(NET::Max);
}

void AppMenuWidget::onWindowChanged(WId /*id*/, NET::Properties /*properties*/, NET::Properties2 /*properties2*/)
{
    if (m_windowID == KWindowSystem::activeWindow())
        onActiveWindowChanged();
}

void AppMenuWidget::minimizeWindow()
{
    KWindowSystem::minimizeWindow(KWindowSystem::activeWindow());
}

void AppMenuWidget::clsoeWindow()
{
    NETRootInfo(QX11Info::connection(), NET::CloseWindow).closeWindowRequest(KWindowSystem::activeWindow());
}

void AppMenuWidget::maxmizeWindow()
{
    KWindowSystem::setState(KWindowSystem::activeWindow(), NET::Max);
}

void AppMenuWidget::restoreWindow()
{
    KWindowSystem::clearState(KWindowSystem::activeWindow(), NET::Max);
}

void AppMenuWidget::actionAbout()
{
    qDebug() << "actionAbout() called";

    QString translatedTextAboutQtCaption;
    translatedTextAboutQtCaption = "<h3>About This Computer</h3>";
    QMessageBox *msgBox = new QMessageBox(QMessageBox::NoIcon, "Title", "Text");
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle("About This Computer");
    msgBox->setText("Kernel information goes here. To be implemented.");
    msgBox->setInformativeText("SMBIOS information goes here. To be implemented.");
    msgBox->setParent(this, Qt::Dialog); // setParent to this results in the menu not going away when the dialog is shown

    // On FreeBSD, get information about the machine
    if(which("kenv")){
        QProcess p;
        QString program = "kenv";
        QStringList arguments;
        arguments << "-q" << "smbios.system.maker";
        p.start(program, arguments);
        p.waitForFinished();
        QString vendorname(p.readAllStandardOutput());
        vendorname.replace("\n", "");
        vendorname = vendorname.trimmed();
        qDebug() << "vendorname:" << vendorname;
        QStringList arguments2;
        arguments2 << "-q" << "smbios.system.product";
        p.start(program, arguments2);
        p.waitForFinished();
        QString productname(p.readAllStandardOutput());
        productname.replace("\n", "");
        productname = productname.trimmed();
        qDebug() << "systemname:" << productname;
        msgBox->setInformativeText(vendorname + " " + productname);
        QString program2 = "uname";
        QStringList arguments3;
        arguments3 << "-v";
        p.start(program2, arguments3);
        p.waitForFinished();
        QString operatingsystem(p.readAllStandardOutput());
        operatingsystem.replace("\n", "");
        operatingsystem = operatingsystem.trimmed();
        qDebug() << "systemname:" << operatingsystem;
        msgBox->setText(operatingsystem);
    }

    QSize sz(48, 48);
    msgBox->setIconPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(sz));

    msgBox->exec();
}

void AppMenuWidget::actionDisplays()
{
    qDebug() << "actionDisplays() called";
    // TODO: Find working Qt based tool
    if(which("arandr")) {
        QProcess::startDetached("arandr"); // sudo pkg install arandr // Gtk
    } else if (which("lxrandr")) {
        QProcess::startDetached("lxrandr"); // sudo pkg install lxrandr // Gtk
    } else {
        qDebug() << "arandr, lxrandr not found";
    }
}

void AppMenuWidget::actionShortcuts()
{
    qDebug() << "actionShortcuts() called";
    QProcess::startDetached("lxqt-config-globalkeyshortcuts");
}

void AppMenuWidget::actionSound()
{
    qDebug() << "actionSound() called";
    QProcess::startDetached("dsbmixer");
}

void AppMenuWidget::actionLogout()
{
    qDebug() << "actionLogout() called";
    // Check if we have the Shutdown binary at hand
    if(QFileInfo(QCoreApplication::applicationDirPath() + QString("/Shutdown")).isExecutable()) {
        QProcess::execute(QCoreApplication::applicationDirPath() + QString("/Shutdown"));
    } else {
        qDebug() << "Shutdown executable not available next to Menubar executable, exiting";
        QApplication::exit(); // In case we are lacking the Shutdown executable
    }
}

bool AppMenuWidget::which(QString command)
{
    QProcess findProcess;
    QStringList arguments;
    arguments << command;
    findProcess.start("which", arguments);
    findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

    if(!findProcess.waitForFinished())
        return false; // Not found or which does not work

    QString retStr(findProcess.readAll());

    retStr = retStr.trimmed();

    QFile file(retStr);
    QFileInfo check_file(file);
    if (check_file.exists() && check_file.isFile())
        return true; // Found!
    else
        return false; // Not found!
}
