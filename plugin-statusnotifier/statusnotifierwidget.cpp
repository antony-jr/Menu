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

#include "statusnotifierwidget.h"
#include <QApplication>
#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDBusConnectionInterface>

StatusNotifierWidget::StatusNotifierWidget(QWidget *parent)
  : QWidget(parent),
    m_layout(new QHBoxLayout(this))
{
    m_layout->setSpacing(10);
    m_layout->setMargin(0);

    QFutureWatcher<StatusNotifierWatcher *> * future_watcher = new QFutureWatcher<StatusNotifierWatcher *>;
    connect(future_watcher, &QFutureWatcher<StatusNotifierWatcher *>::finished, this, [this, future_watcher]
        {
            m_watcher = future_watcher->future().result();

            connect(m_watcher, &StatusNotifierWatcher::StatusNotifierItemRegistered,
                    this, &StatusNotifierWidget::itemAdded);
            connect(m_watcher, &StatusNotifierWatcher::StatusNotifierItemUnregistered,
                    this, &StatusNotifierWidget::itemRemoved);

            qDebug() << m_watcher->RegisteredStatusNotifierItems();

            future_watcher->deleteLater();
        });

    QFuture<StatusNotifierWatcher *> future = QtConcurrent::run([]
        {
            QString dbusName = QStringLiteral("org.kde.StatusNotifierHost-%1-%2").arg(QApplication::applicationPid()).arg(1);
            if (QDBusConnectionInterface::ServiceNotRegistered == QDBusConnection::sessionBus().interface()->registerService(dbusName, QDBusConnectionInterface::DontQueueService))
                qDebug() << "unable to register service for " << dbusName;

            StatusNotifierWatcher * watcher = new StatusNotifierWatcher;
            watcher->RegisterStatusNotifierHost(dbusName);
            watcher->moveToThread(QApplication::instance()->thread());
            return watcher;
        });

    future_watcher->setFuture(future);
}

StatusNotifierWidget::~StatusNotifierWidget()
{
    delete m_watcher;
}

void StatusNotifierWidget::itemAdded(QString serviceAndPath)
{
    int slash = serviceAndPath.indexOf(QLatin1Char('/'));
    QString serv = serviceAndPath.left(slash);
    QString path = serviceAndPath.mid(slash);
    StatusNotifierButton *button = new StatusNotifierButton(serv, path, this);
    button->setFixedSize(30, 30);

    m_services.insert(serviceAndPath, button);
    m_layout->insertWidget(0, button);
    button->show();
}

void StatusNotifierWidget::itemRemoved(const QString &serviceAndPath)
{
    StatusNotifierButton *button = m_services.value(serviceAndPath, nullptr);
    if (button) {
        button->deleteLater();
        m_layout->removeWidget(button);
    }
}
