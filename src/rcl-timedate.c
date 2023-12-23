
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

#include "config.h"

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <locale.h>

#include <polkit/polkit.h>

#include "rcl-timedate.h"
#include "rcl-time-utils.h"
#include "rcl-ntpd-utils.h"
#include "rcl-zone-utils.h"

struct RclDaemonPrivate
{
  gboolean         debug;
  gchar           *timezone;
  gboolean         local_rtc;
  gboolean         can_ntp;
  gboolean         use_ntp;
  PolkitAuthority *auth;
};

G_DEFINE_TYPE_WITH_PRIVATE (RclDaemon, rcl_daemon, RCL_TYPE_TIMEDATE_DAEMON_SKELETON)

#define RCL_DAEMON_ACTION_DELAY  20 /* seconds */
#define RCL_INTERFACE_PREFIX     "org.freedesktop.timedate1."


/***************************************************************
  Polkit data and functions:
  =========================
 */
struct check_polkit_data
{
  const gchar         *unique_name;
  const gchar         *action_id;
  gboolean             user_interaction;
  GAsyncReadyCallback  callback;
  gpointer             user_data;

  PolkitAuthority     *authority;
  PolkitSubject       *subject;
};

static void
check_polkit_data_free( struct check_polkit_data *data )
{
  if( data == NULL )
    return;

  if( data->action_id != NULL )
    g_free( (gpointer)data->action_id );

  if( data->subject != NULL )
    g_object_unref( data->subject );
  if( data->authority != NULL )
    g_object_unref( data->authority );

  g_free( data );
}

static gboolean
check_polkit_finish( GAsyncResult  *result,
                     GError       **error  )
{
  g_return_val_if_fail( g_task_is_valid( result, NULL ), FALSE );

  return g_task_propagate_boolean( G_TASK( result ), error );
}

static void
_check_polkit_for_action_async( GDBusMethodInvocation *invocation,
                                const gchar           *function,
                                const gboolean         interactive,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data );


static void
_check_polkit_authorization_callback( GObject      *source_object,
                                      GAsyncResult *res,
                                      gpointer     _data)
{
  struct check_polkit_data  *data;
  PolkitAuthorizationResult *result;
  GTask                     *task;
  GError                    *error = NULL;

  data = (struct check_polkit_data *)_data;
  if( (result = polkit_authority_check_authorization_finish( data->authority, res, &error)) == NULL )
  {
    g_task_report_error( NULL, data->callback, data->user_data, NULL, error );
    goto out;
  }
 
  if( !polkit_authorization_result_get_is_authorized( result ) )
  {
    g_task_report_new_error( NULL,
                             data->callback,
                             data->user_data,
                             NULL,
                             POLKIT_ERROR,
                             POLKIT_ERROR_NOT_AUTHORIZED,
                             "Authorization for '%s': user is not authorized", data->action_id );
    goto out;
  }
  task = g_task_new( NULL, NULL, data->callback, data->user_data );
  g_task_set_source_tag( task, _check_polkit_for_action_async );
  g_task_return_boolean( task, TRUE );
  g_clear_object ( &task );

out:
  check_polkit_data_free( data );
  if( result != NULL )
    g_object_unref( result );
}

static void
_check_polkit_authority_callback( GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer     _data )
{
  struct check_polkit_data *data;
  GError                   *error = NULL;

  data = (struct check_polkit_data *)_data;

  if( (data->authority = polkit_authority_get_finish( result, &error )) == NULL)
  {
    g_task_report_error( NULL, data->callback, data->user_data, NULL, error );
    check_polkit_data_free( data );
    return;
  }
  if( data->unique_name == NULL ||
      data->action_id == NULL   ||
      (data->subject = polkit_system_bus_name_new( data->unique_name )) == NULL )
  {
    g_task_report_new_error( NULL,
                             data->callback,
                             data->user_data,
                             NULL,
                             POLKIT_ERROR,
                             POLKIT_ERROR_FAILED,
                             "Authorization for '%s': failed", data->action_id );
    check_polkit_data_free( data );
    return;
  }

  polkit_authority_check_authorization( data->authority,
                                        data->subject,
                                        data->action_id,
                                        NULL,
                                        (PolkitCheckAuthorizationFlags)data->user_interaction,
                                        NULL,
                                        _check_polkit_authorization_callback,
                                        data );
}

static void
_check_polkit_for_action_async( GDBusMethodInvocation *invocation,
                                const gchar           *function,
                                const gboolean         interactive,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data )
{
  const gchar               *action = g_strjoin( "", RCL_INTERFACE_PREFIX, function, NULL );
  struct check_polkit_data  *data;
  const gchar               *sender;

  sender  = g_dbus_method_invocation_get_sender( invocation );

  data = g_new0( struct check_polkit_data, 1 );
  data->unique_name      = sender;
  data->action_id        = action;
  data->user_interaction = interactive;
  data->callback         = callback;
  data->user_data        = user_data;

  polkit_authority_get_async( NULL, _check_polkit_authority_callback, data );
}


/***************************************************************
  DBus Properties:
  ===============
 */
static gboolean get_ntpsynchronized( RclTimedateDaemon *object )
{
  gboolean ntp_synced = ntp_synchronized();

  rcl_timedate_daemon_set_ntpsynchronized( object, ntp_synced );

  return ntp_synced;
}

static guint64 get_rtctime_usec( RclTimedateDaemon *object )
{
  struct tm tm   = {};
  guint64   usec = 0;

  if( !clock_get_hwclock( &tm ) )
  {
    g_debug( "get-ntpsynchronized: error: Cannot get RTC clock" );

    rcl_timedate_daemon_set_rtctime_usec( object, (guint64)0 );

    return (guint64)0;
  }

  usec = (guint64)timegm( &tm ) * USEC_PER_SEC;

  rcl_timedate_daemon_set_rtctime_usec( object, usec );

  return usec;
}

static guint64 get_time_usec( RclTimedateDaemon *object )
{
  guint64  usec;

  usec = now( CLOCK_REALTIME );

  rcl_timedate_daemon_set_time_usec( object, usec );

  return usec;
}

void rcl_daemon_sync_dbus_properties( RclTimedateDaemon *object )
{

  /* Update NTPSynchronized */
  (void)get_ntpsynchronized( object );

  /* Update RTCTimeUSec */
  (void)get_rtctime_usec( object );

  /* Update TimeUSec */
  (void)get_time_usec( object );
}


/***************************************************************
  DBus Handlers:
  =============
 */

/*******************************
  SetTimezone:
  -----------
 */
struct set_timezone_data
{
  RclTimedateDaemon     *object;
  GDBusMethodInvocation *invocation;
  const gchar           *timezone;
  gboolean               interactive;
  RclDaemon             *daemon;
};

static void
set_timezone_data_free( struct set_timezone_data * data )
{
  if( data == NULL )
    return;

  if( data->timezone != NULL )
    g_free( (gpointer)data->timezone );

  g_free( data );
}


static void
set_timezone_authorized_callback( GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer      user_data )
{
  GError                   *error = NULL;
  struct set_timezone_data *data  = (struct set_timezone_data *)user_data;
  gboolean                  ret   = TRUE;

  if( !check_polkit_finish( result, &error ) )
  {
    g_debug( "set-timezone: error: '%s'", "User is not privileged" );
    set_timezone_data_free( data );
    return;
  }

  ret = set_system_timezone( data->timezone );
  if( ret )
  {
    if( data->daemon->priv->local_rtc )
    {
      struct timespec ts;
      struct tm       tm;

      /* Sync RTC from system clock, with the new delta */
      if( clock_gettime(CLOCK_REALTIME, &ts) != 0 )
      {
        g_debug( "set-timezone: error: Sync RTC from system clock: '%s'", "clock_gettime(): failed" );
        set_timezone_data_free( data );
        return;
      }
      if( !localtime_r( &ts.tv_sec, &tm ) )
      {
        g_debug( "set-timezone: error: Sync RTC from system clock: '%s'", "localtime_r(): failed" );
        set_timezone_data_free( data );
        return;
      }
      if( !clock_set_hwclock( &tm ) )
      {
        g_debug( "set-timezone: error: Sync RTC from system clock: '%s'", "Cannot update '/dev/rtc' (ignoring)" );
      }
    }
  }
  else
  {
    g_debug( "set-timezone: error: Cannot set system timezone '%s'", data->timezone );
    set_timezone_data_free( data );
    return;
  }

  g_free( (gpointer)data->daemon->priv->timezone );
  data->daemon->priv->timezone  = g_strdup( data->timezone );
  rcl_timedate_daemon_set_timezone( data->object, (const gchar *)data->daemon->priv->timezone );


  g_debug( "set-timezone: SetTimezone to '%s' returns successful status (interactive=%s)",
                                          data->daemon->priv->timezone, (data->interactive) ? "true" : "false" );

  rcl_timedate_daemon_complete_set_timezone( data->object, data->invocation );

  set_timezone_data_free( data );
}

gboolean handle_set_timezone( RclTimedateDaemon     *object,
                              GDBusMethodInvocation *invocation,
                              const gchar           *timezone,
                              gboolean               interactive,
                              RclDaemon             *daemon )
{
  struct set_timezone_data *data;

  if( !timezone_is_valid( timezone ) )
  {
    g_dbus_method_invocation_return_error( invocation,
                                           RCL_DAEMON_ERROR,
                                           RCL_DAEMON_ERROR_INVALID_TIMEZONE_FILE,
                                           "Requested timezone '%s' is invalid", timezone );
    return TRUE;
  }

  if( g_strcmp0( (const char *)daemon->priv->timezone, (const char *)timezone ) == 0 )
    goto out;

  data = g_new0( struct set_timezone_data, 1 );
  data->object         = object;
  data->invocation     = invocation;
  data->timezone       = (const gchar *)g_strdup( timezone );
  data->interactive    = interactive;
  data->daemon         = daemon;

  _check_polkit_for_action_async( invocation,
                                  "set-timezone",
                                  interactive,
                                  set_timezone_authorized_callback,
                                  data );
out:
  return TRUE;
}


/*******************************
  SetLocalRTC:
  -----------
 */
struct set_local_rtc_data
{
  RclTimedateDaemon     *object;
  GDBusMethodInvocation *invocation;
  gboolean               local_rtc;
  gboolean               fix_system;
  gboolean               interactive;
  RclDaemon             *daemon;
};

static void
set_local_rtc_data_free( struct set_local_rtc_data * data )
{
  if( data == NULL )
    return;

  g_free( data );
}

static void
set_local_rtc_authorized_callback( GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data )
{
  GError                    *error = NULL;
  struct set_local_rtc_data *data  = (struct set_local_rtc_data *)user_data;
  gboolean                   ret = TRUE;
  struct timespec            ts;

  if( !check_polkit_finish( result, &error ) )
  {
    g_debug( "set-local-rtc: error: '%s'", "User is not privileged" );
    set_local_rtc_data_free( data );
    return;
  }

  if( data->daemon->priv->local_rtc != data->local_rtc )
  {
    data->daemon->priv->local_rtc = data->local_rtc;

    /* Write new configuration files */
    ret = write_data_local_rtc( data->daemon->priv->local_rtc );
    if( !ret )
    {
      g_dbus_method_invocation_return_error( data->invocation,
                                             RCL_DAEMON_ERROR,
                                             RCL_DAEMON_ERROR_GENERAL,
                                             "Cannot set LocalRTC" );
      set_local_rtc_data_free( data );
      return;
    }

  }

  /* Tell the kernel our timezone */
  ret = clock_set_timezone( NULL );
  if( !ret )
  {
    g_debug( "set-local-rtc: error: Cannot set timezone clock after SetLocal_RTC" );
    set_local_rtc_data_free( data );
    return;
  }

  /* Synchronize clocks */
  if( clock_gettime(CLOCK_REALTIME, &ts) != 0 )
  {
    g_debug( "set-local-rtc: error: Sync RTC from system clock after SetLocalRTC: '%s'", "clock_gettime(): failed" );
    set_local_rtc_data_free( data );
    return;
  }

  if( data->fix_system )
  {
    struct tm tm;

    /* Sync system clock from RTC; first, initialize the timezone fields of struct tm. */
    localtime_or_gmtime_r( &ts.tv_sec, &tm, !data->daemon->priv->local_rtc);

    /* Override the main fields of struct tm, but not the timezone fields */
    ret = clock_get_hwclock( &tm );
    if( !ret)
    {
      g_debug( "set-local-rtc: error: Failed to get hardware clock (ignoring)" );
    }
    else
    {
      /* And set the system clock with this */
      ts.tv_sec = mktime_or_timegm( &tm, !data->daemon->priv->local_rtc );

      if( clock_settime( CLOCK_REALTIME, &ts ) < 0 )
      {
        g_debug( "set-local-rtc: error: Failed to update system clock (ignoring)" );
      }
    }
  }
  else
  {
    struct tm tm;

    /* Sync RTC from system clock */
    localtime_or_gmtime_r( &ts.tv_sec, &tm, !data->daemon->priv->local_rtc );

    ret = clock_set_hwclock( &tm );
    if( !ret )
    {
      g_debug( "set-local-rtc: error: Failed to sync time to hardware clock (ignoring)" );
    }
  }

  g_debug( "set-local-rtc: RTC configured to %s time", (data->daemon->priv->local_rtc) ? "localtime" : "UTC" );


  rcl_timedate_daemon_set_local_rtc( data->object, data->daemon->priv->local_rtc );

  g_debug( "set-local-rtc: SetLocalRTC to '%s' returns successful status (fix_sysrem=%s; interactive=%s)",
                                           (data->daemon->priv->local_rtc) ? "localtime" : "UTC",
                                           (data->fix_system)              ? "true"      : "false",
                                           (data->interactive)             ? "true"      : "false" );

  rcl_timedate_daemon_complete_set_local_rtc( data->object, data->invocation );

  set_local_rtc_data_free( data );
}

gboolean handle_set_local_rtc( RclTimedateDaemon     *object,
                               GDBusMethodInvocation *invocation,
                               gboolean               local_rtc,
                               gboolean               fix_system,
                               gboolean               interactive,
                               RclDaemon             *daemon )
{
  struct set_local_rtc_data *data;

  if( daemon->priv->local_rtc == local_rtc && !fix_system )
    goto out;

  data = g_new0( struct set_local_rtc_data, 1 );
  data->object      = object;
  data->invocation  = invocation;
  data->local_rtc   = local_rtc;
  data->fix_system  = fix_system;
  data->interactive = interactive;
  data->daemon      = daemon;

  _check_polkit_for_action_async( invocation,
                                  "set-local-rtc",
                                  interactive,
                                  set_local_rtc_authorized_callback,
                                  data );
out:
  return TRUE;
}


/*******************************
  SetNTP:
  ------
 */
struct set_ntp_data
{
  RclTimedateDaemon     *object;
  GDBusMethodInvocation *invocation;
  gboolean               use_ntp;
  gboolean               interactive;
  RclDaemon             *daemon;
};

static void
set_ntp_data_free( struct set_ntp_data * data )
{
  if( data == NULL )
    return;

  g_free( data );
}

static void
set_ntp_authorized_callback( GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data )
{
  GError              *error = NULL;
  struct set_ntp_data *data  = (struct set_ntp_data *)user_data;

  if( !check_polkit_finish( result, &error ) )
  {
    g_debug( "set-ntp: error: '%s'", "User is not privileged" );
    set_ntp_data_free( data );
    return;
  }

  if( data->daemon->priv->use_ntp == data->use_ntp )
    goto out;

  if( data->use_ntp ) /* enable and start NTP daemon: */
  {
    if( ntp_daemon_enabled() )
    {
      if( ntp_daemon_status() )
      {
        g_debug( "set-ntp: The NTP Daemon already running" );
        data->daemon->priv->use_ntp = TRUE;
        /* SUCCESS */
      }
      else
      {
        if( !start_ntp_daemon() )
        {
          g_debug( "set-ntp: error: Cannot start NTPD daemon" );
          g_dbus_method_invocation_return_error( data->invocation,
                                                 RCL_DAEMON_ERROR,
                                                 RCL_DAEMON_ERROR_GENERAL,
                                                 "Cannot start NTP Daemon" );
          data->daemon->priv->use_ntp = FALSE;
          /* FAILURE */
          set_ntp_data_free( data );
          return;
        }
        else
        {
          g_debug( "set-ntp: The NTPD daemon started successful" );
          data->daemon->priv->use_ntp = TRUE;
          /* SUCCESS */
        }
      }
    }
    else
    {
      if( !enable_ntp_daemon() )
      {
        g_debug( "set-ntp: error: Cannot enable NTPD daemon" );
        g_dbus_method_invocation_return_error( data->invocation,
                                               RCL_DAEMON_ERROR,
                                               RCL_DAEMON_ERROR_GENERAL,
                                               "Cannot enable NTP Daemon" );
        data->daemon->priv->use_ntp = FALSE;
        /* FAILURE */
        set_ntp_data_free( data );
        return;
      }
      else
      {
        if( !start_ntp_daemon() )
        {
          g_debug( "set-ntp: error: Cannot start NTPD daemon" );
          g_dbus_method_invocation_return_error( data->invocation,
                                                 RCL_DAEMON_ERROR,
                                                 RCL_DAEMON_ERROR_GENERAL,
                                                 "Cannot start NTP Daemon" );
          data->daemon->priv->use_ntp = FALSE;
          /* FAILURE */
          set_ntp_data_free( data );
          return;
        }
        else
        {
          g_debug( "set-ntp: The NTPD daemon started successful" );
          data->daemon->priv->use_ntp = TRUE;
          /* SUCCESS */
        }
      }
    }
  }
  else /* stop and disable NTP daemon: */
  {
    if( ntp_daemon_enabled() )
    {
      if( ntp_daemon_status() )
      {
        /* daemon is running; stop it */
        if( !stop_ntp_daemon() )
        {
          g_debug( "set-ntp: error: Cannot stop NTPD daemon" );
          g_dbus_method_invocation_return_error( data->invocation,
                                                 RCL_DAEMON_ERROR,
                                                 RCL_DAEMON_ERROR_GENERAL,
                                                 "Cannot stop NTP Daemon" );
          data->daemon->priv->use_ntp = TRUE;
          /* FAILURE */
          set_ntp_data_free( data );
          return;
        }
        else
        {
          g_debug( "set-ntp: The NTPD daemon stopped successful" );
          if( !disable_ntp_daemon() )
          {
            g_debug( "set-ntp: Cannot disable NTPD daemon" );
            g_dbus_method_invocation_return_error( data->invocation,
                                                   RCL_DAEMON_ERROR,
                                                   RCL_DAEMON_ERROR_GENERAL,
                                                   "Cannot disable NTP Daemon" );
            data->daemon->priv->use_ntp = FALSE;
            /* daemon stopped but not disabled (will start after reboot) */
            /* SUCCESS */
          }
          else
          {
            g_debug( "set-ntp: The NTPD daemon disabled successful" );
            data->daemon->priv->use_ntp = FALSE;
            /* SUCCESS */
          }
        }
      }
      else
      {
        /* daemon is stopped; disable it */
        if( !disable_ntp_daemon() )
        {
          g_debug( "set-ntp: error: Cannot disable NTPD daemon" );
          g_dbus_method_invocation_return_error( data->invocation,
                                                 RCL_DAEMON_ERROR,
                                                 RCL_DAEMON_ERROR_GENERAL,
                                                 "Cannot disable NTP Daemon" );
          data->daemon->priv->use_ntp = FALSE;
          /* daemon stopped but not disabled (will start after reboot) */
          /* SUCCESS */
        }
        else
        {
          g_debug( "set-ntp: The NTPD daemon disabled successful" );
          data->daemon->priv->use_ntp = FALSE;
          /* SUCCESS */
        }
      }
    }
    else
    {
      g_debug( "set-ntp: The NTPD daemon already disabled" );
      data->daemon->priv->use_ntp = FALSE;
      /* SUCCESS */
    }
  }

  g_debug( "set-ntp: NTP configured to %s", (data->daemon->priv->use_ntp) ? "enabled" : "disabled" );

  rcl_timedate_daemon_set_ntp( data->object, data->daemon->priv->use_ntp );
  /* rcl_timedate_daemon_set_ntpsynchronized( object, daemon->priv->use_ntp ); */

out:
  g_debug( "set-ntp: SetNTP to '%s' returns successful status (interactive=%s)",
                                (data->daemon->priv->use_ntp) ? "true" : "false",
                                (data->interactive)           ? "true" : "false" );

  rcl_timedate_daemon_complete_set_ntp( data->object, data->invocation );

  set_ntp_data_free( data );
}

gboolean handle_set_ntp( RclTimedateDaemon     *object,
                         GDBusMethodInvocation *invocation,
                         gboolean               use_ntp,
                         gboolean               interactive,
                         RclDaemon             *daemon )
{
  struct set_ntp_data *data;

  /* check CanNTP (in case NTPD was uninstalled while timedated running) */
  if( !ntp_daemon_installed() )
  {
    daemon->priv->can_ntp = FALSE;
    rcl_timedate_daemon_set_can_ntp( object, daemon->priv->can_ntp );
  }

  if( !daemon->priv->can_ntp )
  {
    daemon->priv->use_ntp = FALSE;
    rcl_timedate_daemon_set_ntp( object, daemon->priv->use_ntp );
    return TRUE;
  }

  data = g_new0( struct set_ntp_data, 1 );
  data->object      = object;
  data->invocation  = invocation;
  data->use_ntp     = use_ntp;
  data->interactive = interactive;
  data->daemon      = daemon;

  _check_polkit_for_action_async( invocation,
                                  "set-ntp",
                                  interactive,
                                  set_ntp_authorized_callback,
                                  data );
  return TRUE;
}


/*******************************
  SetTime:
  -------
 */
struct set_time_data
{
  RclTimedateDaemon     *object;
  GDBusMethodInvocation *invocation;
  guint64                start;
  guint64                usec_utc;
  gboolean               relative;
  gboolean               interactive;
  RclDaemon             *daemon;
};

static void
set_time_data_free( struct set_time_data * data )
{
  if( data == NULL )
    return;

  g_free( data );
}

static void
set_time_authorized_callback( GObject      *source_object,
                              GAsyncResult *result,
                              gpointer      user_data )
{
  GError               *error = NULL;
  struct set_time_data *data  = (struct set_time_data *)user_data;
  struct timespec       ts;
  struct tm             tm;

  if( !check_polkit_finish( result, &error ) )
  {
    g_debug( "set-local-rtc: error: '%s'", "User is not privileged" );
    set_time_data_free( data );
    return;
  }

  if( data->relative )
  {
    guint64  n, x;

    n = now( CLOCK_REALTIME );
    x = n + data->usec_utc;

    if( (data->usec_utc > 0 && x < n) ||
        (data->usec_utc < 0 && x > n)   )
    {
      g_debug( "set-time: error: Time value overflow" );
      g_dbus_method_invocation_return_error( data->invocation,
                                             RCL_DAEMON_ERROR,
                                             RCL_DAEMON_ERROR_INVALID_ARGS,
                                             "set-time: Time value overflow" );
      set_time_data_free( data );
      return;
    }
    timespec_store( &ts, x );
  }
  else
  {
    timespec_store( &ts, (guint64)data->usec_utc);
  }

  timespec_store( &ts, timespec_load( &ts ) + (now( CLOCK_MONOTONIC ) - data->start) );

  /* Set system clock */
  if( clock_settime( CLOCK_REALTIME, &ts ) < 0 )
  {
    g_debug( "set-time: error: Failed to set local time" );
    g_dbus_method_invocation_return_error( data->invocation,
                                           RCL_DAEMON_ERROR,
                                           RCL_DAEMON_ERROR_GENERAL,
                                           "set-time: Failed to set local time" );
    set_time_data_free( data );
    return;
  }

  /* Sync down to RTC */
  localtime_or_gmtime_r( &ts.tv_sec, &tm, !data->daemon->priv->local_rtc );

  if( !clock_set_hwclock( &tm ) )
  {
    g_debug( "set-time: error: Failed to update hardware clock (ignoring)" );
  }

  g_debug( "set-time: SetTime method returns successful status" );

  /* Update RTCTimeUSec (by the way) */
  (void)get_rtctime_usec( data->object );

  rcl_timedate_daemon_set_time_usec( data->object, timespec_load( &ts ) );

  g_debug( "set-time: SetTime to %" PRIu64 " returns successful status(relative=%s; interactive=%s)",
                                 data->usec_utc,
                                 (data->relative)    ? "true" : "false",
                                 (data->interactive) ? "true" : "false" );

  rcl_timedate_daemon_complete_set_time( data->object, data->invocation );

  set_time_data_free( data );
}

gboolean handle_set_time( RclTimedateDaemon     *object,
                          GDBusMethodInvocation *invocation,
                          gint64                 usec_utc,
                          gboolean               relative,
                          gboolean               interactive,
                          RclDaemon             *daemon )
{
  struct set_time_data *data;
  guint64               start;

  if( ntp_daemon_installed() && ntp_daemon_enabled() && ntp_daemon_status() )
  {
    /* NTP Daemon is running */
    g_debug( "set-time: error: Automatic time synchronization is enabled" );
    g_dbus_method_invocation_return_error( invocation,
                                           RCL_DAEMON_ERROR,
                                           RCL_DAEMON_ERROR_GENERAL,
                                           "set-time: Automatic time synchronization is enabled" );
    return TRUE;
  }

  start = now( CLOCK_MONOTONIC );

  if( !relative && usec_utc <= 0 )
  {
    g_debug( "set-time: error: Invalid absolute time" );
    g_dbus_method_invocation_return_error( invocation,
                                           RCL_DAEMON_ERROR,
                                           RCL_DAEMON_ERROR_INVALID_ARGS,
                                           "set-time: Invalid absolute time" );
    return TRUE;
  }

  if( relative && usec_utc == 0 )
  {
    /* Nothing to do */
    goto out;
  }

  data = g_new0( struct set_time_data, 1 );
  data->object      = object;
  data->invocation  = invocation;
  data->start       = start;
  data->usec_utc    = usec_utc;
  data->relative    = relative;
  data->interactive = interactive;
  data->daemon      = daemon;

  _check_polkit_for_action_async( invocation,
                                  "set-time",
                                  interactive,
                                  set_time_authorized_callback,
                                  data );

out:
  return TRUE;
}


/*******************************
  ListTimezones:
  -------------
 */
gboolean handle_list_timezones( RclTimedateDaemon     *object,
                                GDBusMethodInvocation *invocation,
                                RclDaemon             *daemon )
{
  gboolean ret;
  const gchar *const *zones = { NULL };

  ret = get_timezones( &zones );
  if( !ret )
  {
    g_debug( "list-timezones: error: Failed to read list of time zones" );
    g_dbus_method_invocation_return_error( invocation,
                                           RCL_DAEMON_ERROR,
                                           RCL_DAEMON_ERROR_GENERAL,
                                           "list-timezones: Failed to read list of time zones" );
    return TRUE;
  }

  g_debug( "list-timezones: ListTimesones returns successful status" );

  rcl_timedate_daemon_complete_list_timezones( object, invocation, zones );

  timezones_free( &zones );

  return TRUE;
}


/***************************************************************
  Daemon functions:
  ================
 */

void
rcl_daemon_set_debug( RclDaemon *daemon,
                      gboolean  debug )
{
  daemon->priv->debug = debug;
}

gboolean
rcl_daemon_get_debug( RclDaemon *daemon )
{
  return daemon->priv->debug;
}


/***************************************************************
  rcl_daemon_register_timedate_daemon:
 */
static gboolean
rcl_daemon_register_timedate_daemon( RclDaemon       *daemon,
                                     GDBusConnection *connection )
{
  GError *error = NULL;

  daemon->priv->auth = polkit_authority_get_sync( NULL, &error );
  if( daemon->priv->auth == NULL )
  {
    if( error != NULL )
    {
      g_critical ("timedated: error: Cannot get system bus: %s", error->message );
      g_error_free( error );
    }
    return FALSE;
  }

  /* export our interface on the bus */
  g_dbus_interface_skeleton_export( G_DBUS_INTERFACE_SKELETON( daemon ),
                                    connection,
                                    "/org/freedesktop/timedate1",
                                    &error );

  if( error != NULL )
  {
    g_critical( "timedated: error: Cannot register the daemon on system bus: %s", error->message );
    g_error_free( error );
    return FALSE;
  }

  return TRUE;
}

/***************************************************************
  rcl_daemon_startup:
 */
gboolean
rcl_daemon_startup( RclDaemon       *daemon,
                    GDBusConnection *connection )
{
  gboolean ret;

  /* register on bus */
  ret = rcl_daemon_register_timedate_daemon( daemon, connection );
  if( !ret )
  {
    g_warning( "timedated: warning: Failed to register the daemon on bus" );
    goto out;
  }

  g_debug( "Daemon now started" );

out:
  return ret;
}

/***************************************************************
  rcl_daemon_shutdown:

  Stop the daemon, release all resources.
 */
void
rcl_daemon_shutdown( RclDaemon *daemon )
{
}


/***************************************************************
  rcl_daemon_init:
 */
static void
rcl_daemon_init( RclDaemon *daemon )
{
  gboolean rtc = TRUE;  /* default */
  gboolean ntp = FALSE; /* default */

  daemon->priv = rcl_daemon_get_instance_private( daemon );

  /**********************
    Get current Timezone:
   */
  if( !get_system_timezone( (gchar **)&daemon->priv->timezone ) )
  {
    daemon->priv->timezone  = g_strdup( "Europe/Moscow" );
  }

  /******************
    Init Properties:
   */
  /* Timezone: */
  rcl_timedate_daemon_set_daemon_version( RCL_TIMEDATE_DAEMON( daemon ), PACKAGE_VERSION );
  rcl_timedate_daemon_set_timezone( RCL_TIMEDATE_DAEMON( daemon ), (const gchar *)daemon->priv->timezone );

  /* LocalRTC: */
  (void)read_data_local_rtc( &rtc );
  daemon->priv->local_rtc = rtc;
  rcl_timedate_daemon_set_local_rtc( RCL_TIMEDATE_DAEMON( daemon ), daemon->priv->local_rtc );

  /* CanNTP: */
  ntp = ntp_daemon_installed();
  daemon->priv->can_ntp = ntp;
  rcl_timedate_daemon_set_can_ntp( RCL_TIMEDATE_DAEMON( daemon ), daemon->priv->can_ntp );

  /* NTP: */
  ntp = ( ntp_daemon_installed() && ntp_daemon_enabled() && ntp_daemon_status() );
  daemon->priv->use_ntp = ntp;
  rcl_timedate_daemon_set_ntp( RCL_TIMEDATE_DAEMON( daemon ), daemon->priv->use_ntp );

  /* NTPSynchronized: */
  rcl_timedate_daemon_set_ntpsynchronized( RCL_TIMEDATE_DAEMON( daemon ), ntp_synchronized() );


  /******************
    Handlers:
   */
  g_signal_connect( RCL_TIMEDATE_DAEMON( daemon ),
                    "handle-set-timezone",
                    G_CALLBACK( handle_set_timezone ),
                    daemon ); /* user_data */

  g_signal_connect( RCL_TIMEDATE_DAEMON( daemon ),
                    "handle-set-local-rtc",
                    G_CALLBACK( handle_set_local_rtc ),
                    daemon ); /* user_data */

  g_signal_connect( RCL_TIMEDATE_DAEMON( daemon ),
                    "handle-set-ntp",
                    G_CALLBACK( handle_set_ntp ),
                    daemon ); /* user_data */

  g_signal_connect( RCL_TIMEDATE_DAEMON( daemon ),
                    "handle-set-time",
                    G_CALLBACK( handle_set_time ),
                    daemon ); /* user_data */

  g_signal_connect( RCL_TIMEDATE_DAEMON( daemon ),
                    "handle-list-timezones",
                    G_CALLBACK( handle_list_timezones ),
                    daemon ); /* user_data */

}


static const GDBusErrorEntry rcl_daemon_error_entries[] = {
  { RCL_DAEMON_ERROR_GENERAL,               RCL_INTERFACE_PREFIX "GeneralError" },
  { RCL_DAEMON_ERROR_NOT_PRIVILEGED,        RCL_INTERFACE_PREFIX "NotPrivileged" },
  { RCL_DAEMON_ERROR_INVALID_TIMEZONE_FILE, RCL_INTERFACE_PREFIX "InvalidTimezoneFile" },
  { RCL_DAEMON_ERROR_INVALID_ARGS,          RCL_INTERFACE_PREFIX "InvalidArguments" },
  { RCL_DAEMON_ERROR_NOT_SUPPORTED,         RCL_INTERFACE_PREFIX "NotSupported" },
};

/***************************************************************
  rcl_daemon_error_quark:
 */
GQuark
rcl_daemon_error_quark( void )
{
  static volatile gsize quark_volatile = 0;

  g_dbus_error_register_error_domain( "rcl_timedated_error",
                                      &quark_volatile,
                                      rcl_daemon_error_entries,
                                      G_N_ELEMENTS( rcl_daemon_error_entries ) );
  return quark_volatile;
}


/***************************************************************
  rcl_daemon_finalize:
 */
static void
rcl_daemon_finalize( GObject *object )
{
  RclDaemon *daemon = RCL_DAEMON( object );

  g_free( daemon->priv->timezone );
  g_clear_object( &daemon->priv->auth );

  G_OBJECT_CLASS( rcl_daemon_parent_class)->finalize( object );
}

/***************************************************************
  rcl_daemon_class_init:
 */
static void
rcl_daemon_class_init( RclDaemonClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );
  object_class->finalize = rcl_daemon_finalize;
}

/***************************************************************
  rcl_daemon_new:
 */
RclDaemon *
rcl_daemon_new( void )
{
  return RCL_DAEMON( g_object_new( RCL_TYPE_DAEMON, NULL ) );
}
