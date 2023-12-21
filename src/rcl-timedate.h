
/*
 * Copyright (C) 2023 Andrey V.Kosteltsev <kx@radix.pro>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __RCL_DAEMON_H__
#define __RCL_DAEMON_H__

#include <dbus/rcl-timedate-generated.h>

G_BEGIN_DECLS

#define RCL_TYPE_DAEMON         (rcl_daemon_get_type ())
#define RCL_DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), RCL_TYPE_DAEMON, RclDaemon))
#define RCL_DAEMON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), RCL_TYPE_DAEMON, RclDaemonClass))
#define RCL_IS_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), RCL_TYPE_DAEMON))
#define RCL_IS_DAEMON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), RCL_TYPE_DAEMON))
#define RCL_DAEMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RCL_TYPE_DAEMON, RclDaemonClass))

typedef struct RclDaemonPrivate RclDaemonPrivate;

typedef struct
{
  RclTimedateDaemonSkeleton  parent;
  RclDaemonPrivate          *priv;
} RclDaemon;

typedef struct
{
  RclTimedateDaemonSkeletonClass parent_class;
} RclDaemonClass;

typedef enum
{
  RCL_DAEMON_ERROR_GENERAL,
  RCL_DAEMON_ERROR_NOT_PRIVILEGED,
  RCL_DAEMON_ERROR_INVALID_TIMEZONE_FILE,
  RCL_DAEMON_ERROR_INVALID_ARGS,
  RCL_DAEMON_ERROR_NOT_SUPPORTED,
  RCL_DAEMON_NUM_ERRORS
} RclDaemonError;

#define RCL_DAEMON_ERROR rcl_daemon_error_quark ()

GQuark     rcl_daemon_error_quark ( void );
GType      rcl_daemon_get_type    ( void );
RclDaemon *rcl_daemon_new         ( void );

/* private */
gboolean  rcl_daemon_startup     ( RclDaemon       *daemon,
                                   GDBusConnection *connection );
void      rcl_daemon_shutdown    ( RclDaemon       *daemon );
void      rcl_daemon_set_debug   ( RclDaemon       *daemon,
                                   gboolean         debug  );
gboolean  rcl_daemon_get_debug   ( RclDaemon       *daemon );

void      rcl_daemon_sync_dbus_properties( RclTimedateDaemon *object );

G_END_DECLS

#endif /* __RCL_DAEMON_H__ */
