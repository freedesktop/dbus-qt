// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* integrator.h: integrates D-BUS into Qt event loop
 *
 * Copyright (C) 2003  Zack Rusin <zack@kde.org>
 *
 * Licensed under the Academic Free License version 1.2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "integrator.h"
#include "connection.h"

#include <qtimer.h>
#include <qsocketnotifier.h>
#include <qintdict.h>
#include <qptrlist.h>

namespace DBusQt
{
namespace Internal {

struct QtWatch {
  QtWatch(): readSocket( 0 ), writeSocket( 0 ) { }

  DBusWatch *watch;
  QSocketNotifier *readSocket;
  QSocketNotifier *writeSocket;
};

struct DBusQtTimeout {
  DBusQtTimeout(): timer( 0 ), timeout( 0 ) { }
  ~DBusQtTimeout() {
    delete timer;
  }
  QTimer *timer;
  DBusTimeout *timeout;
};

void dbusWakeupMain( void* )
{
}

Integrator::Integrator( Connection *parent )
  : QObject( parent ), m_parent( parent )
{
  dbus_connection_set_watch_functions( m_parent->connection(),
                                       dbusAddWatch,
                                       dbusRemoveWatch,
                                       dbusToggleWatch,
                                       this, 0 );
  dbus_connection_set_timeout_functions( m_parent->connection(),
                                         dbusAddTimeout,
                                         dbusRemoveTimeout,
                                         dbusToggleTimeout,
                                         this, 0 );
  dbus_connection_set_wakeup_main_function( m_parent->connection(),
					    dbusWakeupMain,
					    this, 0 );
}

void Integrator::slotRead( int fd )
{
  Q_UNUSED( fd );
  emit readReady();
}

void Integrator::slotWrite( int fd )
{
  Q_UNUSED( fd );
}

void Integrator::addWatch( DBusWatch *watch )
{
  if ( !dbus_watch_get_enabled( watch ) )
    return;

  QtWatch *qtwatch = new QtWatch;
  qtwatch->watch = watch;

  int flags = dbus_watch_get_flags( watch );
  int fd = dbus_watch_get_fd( watch );

  if ( flags & DBUS_WATCH_READABLE ) {
    qtwatch->readSocket = new QSocketNotifier( fd, QSocketNotifier::Read, this );
    QObject::connect( qtwatch->readSocket, SIGNAL(activated(int)), SLOT(slotRead(int)) );
  }

  if (flags & DBUS_WATCH_WRITABLE) {
    qtwatch->writeSocket = new QSocketNotifier( fd, QSocketNotifier::Write, this );
    QObject::connect( qtwatch->writeSocket, SIGNAL(activated(int)), SLOT(slotWrite(int)) );
  }

  m_watches.insert( fd, qtwatch );
}

void Integrator::removeWatch( DBusWatch *watch )
{
  int key = dbus_watch_get_fd( watch );

  QtWatch *qtwatch = m_watches.take( key );

  if ( qtwatch ) {
    delete qtwatch->readSocket;  qtwatch->readSocket = 0;
    delete qtwatch->writeSocket; qtwatch->writeSocket = 0;
    delete qtwatch;
  }
}


}//end namespace Internal
}//end namespace DBusQt

#include "integrator.moc"