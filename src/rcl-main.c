
/*
 * Copyright (C) 2023 Andrey V.Kosteltsev <kx@radix.pro>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <locale.h>


#include "rcl-timedate.h"

#define TIMEDATE_SERVICE_NAME "org.freedesktop.timedate1"

typedef struct RclState
{
  RclDaemon  *daemon;
  GMainLoop  *loop;
} RclState;

static void
rcl_state_free( RclState *state )
{
  rcl_daemon_shutdown( state->daemon );

  g_clear_object( &state->daemon );
  g_clear_pointer( &state->loop, g_main_loop_unref );

  g_free( state );
}

static RclState *
rcl_state_new( void )
{
  RclState *state = g_new0( RclState, 1 );

  state->daemon = rcl_daemon_new();
  state->loop = g_main_loop_new( NULL, FALSE );

  return state;
}

/************************
  rcl_main_bus_acquired:
 */
static void
rcl_main_bus_acquired( GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data )
{
  RclState *state = user_data;

  if( !rcl_daemon_startup( state->daemon, connection ) )
  {
    g_warning( "Could not startup daemon" );
    g_main_loop_quit( state->loop );
  }
}

/*************************
  rcl_main_name_acquired:
 */
static void
rcl_main_name_acquired( GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
  g_debug( "Acquired the name %s", name );
}

/*********************
  rcl_main_name_lost:
 */
static void
rcl_main_name_lost( GDBusConnection *connection,
                    const gchar     *name,
                    gpointer         user_data )
{
  RclState *state = user_data;
  g_debug( "Name lost, exiting" );
  g_main_loop_quit( state->loop );
}

/*********************
  rcl_main_sigint_cb:
 */
static gboolean
rcl_main_sigint_cb( gpointer user_data )
{
  RclState *state = user_data;
  g_debug( "Handling SIGINT" );
  g_main_loop_quit( state->loop );
  return FALSE;
}

static gboolean
rcl_main_sigterm_cb( gpointer user_data )
{
  RclState *state = user_data;
  g_debug( "Handling SIGTERM" );
  g_main_loop_quit( state->loop );
  return FALSE;
}

/*************************
  rcl_main_log_ignore_cb:
 */
static void
rcl_main_log_ignore_cb( const gchar    *log_domain,
                        GLogLevelFlags  log_level,
                        const gchar    *message,
                        gpointer        user_data )
{
}

/**************************
  rcl_main_log_handler_cb:
 */
static void
rcl_main_log_handler_cb( const gchar    *log_domain,
                         GLogLevelFlags  log_level,
                         const gchar    *message,
                         gpointer        user_data )
{
  gchar str_time[255];
  time_t the_time;

  /* header always in green */
  time( &the_time );
  strftime( str_time, 254, "%H:%M:%S", localtime( &the_time ) );
  g_print( "%c[%dmTI:%s\t", 0x1B, 32, str_time );

  /* critical is also in red */
  if( log_level == G_LOG_LEVEL_CRITICAL ||
      log_level == G_LOG_LEVEL_WARNING  ||
      log_level == G_LOG_LEVEL_ERROR )
  {
    g_print( "%c[%dm%s\n%c[%dm", 0x1B, 31, message, 0x1B, 0 );
  }
  else
  {
    /* debug in blue */
    g_print( "%c[%dm%s\n%c[%dm", 0x1B, 34, message, 0x1B, 0 );
  }
}

static gboolean
rcl_main_timed_cb( gpointer user_data )
{
  RclState *state = (RclState *)user_data;
  RclTimedateDaemon *object = RCL_TIMEDATE_DAEMON( state->daemon );

  /* g_debug( "Synchronize properties" ); */

  rcl_daemon_sync_dbus_properties( object );

  return TRUE;
}

/*******
  main:
 */
gint main( gint argc, gchar **argv )
{
  GError             *error    = NULL;
  GOptionContext     *context;
  guint               timer_id = 0;
  gboolean            debug    = FALSE;
  gboolean            verbose  = FALSE;
  RclState           *state;
  GBusNameOwnerFlags  bus_flags;
  gboolean            replace  = FALSE;

  const GOptionEntry options[] = {
    { "replace", 'r', 0, G_OPTION_ARG_NONE, &replace, _("Replace the old daemon"),               NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, _("Show extra debugging information"),     NULL },
    { "debug",   'd', 0, G_OPTION_ARG_NONE, &debug,   _("Enable debugging (implies --verbose)"), NULL },
    { NULL }
  };

#if !defined(GLIB_VERSION_2_36)
  g_type_init();
#endif
  setlocale( LC_ALL, "" );

  /* NOTE: do not use bind_textdomain() when use gi18n-lib.h */
  bind_textdomain_codeset( GETTEXT_PACKAGE, "UTF-8" );
  textdomain( GETTEXT_PACKAGE );

  context = g_option_context_new( "" );
  g_option_context_add_main_entries( context, options, NULL );
  if( !g_option_context_parse (context, &argc, &argv, &error ) )
  {
    g_warning( "Failed to parse command-line options: %s", error->message );
    g_error_free( error );
    return 1;
  }
  g_option_context_free( context );

  if( debug )
    verbose = TRUE;

  /* verbose? */
  if( verbose )
  {
    g_log_set_fatal_mask( NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL );
    g_log_set_handler( G_LOG_DOMAIN,
                       G_LOG_LEVEL_ERROR    |
                       G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_DEBUG    |
                       G_LOG_LEVEL_WARNING,
                       rcl_main_log_handler_cb, NULL );
    g_log_set_handler( "Timedate-Linux",
                       G_LOG_LEVEL_ERROR    |
                       G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_DEBUG    |
                       G_LOG_LEVEL_WARNING,
                       rcl_main_log_handler_cb, NULL );
  }
  else
  {
    /* hide all debugging */
    g_log_set_fatal_mask( NULL, G_LOG_LEVEL_ERROR );
    g_log_set_handler( G_LOG_DOMAIN,
                       G_LOG_LEVEL_DEBUG,
                       rcl_main_log_ignore_cb,
                       NULL );
    g_log_set_handler( "Timedate-Linux",
                       G_LOG_LEVEL_DEBUG,
                       rcl_main_log_ignore_cb,
                       NULL );
  }

  /* initialize state */
  state = rcl_state_new();
  rcl_daemon_set_debug( state->daemon, debug );

  /* do stuff on ctrl-c */
  g_unix_signal_add_full( G_PRIORITY_DEFAULT,
                          SIGINT,
                          rcl_main_sigint_cb,
                          state,
                          NULL );

  /* Clean shutdown on SIGTERM */
  g_unix_signal_add_full( G_PRIORITY_DEFAULT,
                          SIGTERM,
                          rcl_main_sigterm_cb,
                          state,
                          NULL );

  /* acquire name */
  bus_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if( replace )
    bus_flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  g_bus_own_name( G_BUS_TYPE_SYSTEM,
                  TIMEDATE_SERVICE_NAME,
                  bus_flags,
                  rcl_main_bus_acquired,
                  rcl_main_name_acquired,
                  rcl_main_name_lost,
                  state, NULL );

  g_debug( "Starting timedated version %s", PACKAGE_VERSION );

  /* in msec = 1/1000 sec */
  timer_id = g_timeout_add( 1000, (GSourceFunc)rcl_main_timed_cb, (gpointer)state );
  g_source_set_name_by_id( timer_id, "[timedate] rcl_main_timed_cb" );


  /* wait for input or timeout */
  g_main_loop_run( state->loop );
  rcl_state_free( state );

  return 0;
}
