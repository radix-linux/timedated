
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

#include "rcl-ntpd-utils.h"

static gboolean exec_cmd( const gchar *cmd )
{
  int       exit_status;
  GError   *error = NULL;
  gboolean  ret = TRUE;

  if( !cmd || *cmd == '\0' ) return FALSE;

  if( !g_spawn_command_line_sync( cmd, NULL, NULL, &exit_status, &error ) )
  {
    g_error_free( error );
    ret = FALSE;
  }
  g_free( (gpointer)cmd );

  if( exit_status != 0 )
    ret = FALSE;

  return ret;
}

gboolean ntp_daemon_installed( void )
{
  if( g_file_test( NTPD_CONF, G_FILE_TEST_EXISTS ) &&
      g_file_test( NTPD_RC,   G_FILE_TEST_EXISTS )   )
    return TRUE;
  else
    return FALSE;
}

gboolean ntp_daemon_enabled( void )
{
  if( g_file_test( NTPD_CONF, G_FILE_TEST_EXISTS ) &&
      g_file_test( NTPD_RC,   G_FILE_TEST_EXISTS ) &&
      g_file_test( NTPD_RC,   G_FILE_TEST_IS_EXECUTABLE ) )
    return TRUE;
  else
    return FALSE;
}

gboolean ntp_daemon_status( void )
{
  gchar *cmd;

  if( g_file_test( NTPD_RC, G_FILE_TEST_EXISTS ) &&
      g_file_test( NTPD_RC, G_FILE_TEST_IS_EXECUTABLE ) )
  {
    cmd = g_strconcat( NTPD_RC, " status", NULL );
    if( !exec_cmd( (const gchar *)cmd ) )
      return FALSE;
    else
      return TRUE;
  }

  return FALSE;
}

gboolean stop_ntp_daemon( void )
{
  if( ntp_daemon_enabled() && !ntp_daemon_status() )
    return TRUE;

  if( ntp_daemon_enabled() )
  {
    gchar *cmd;
    cmd = g_strconcat( NTPD_RC, " stop", NULL );
    if( !exec_cmd( (const gchar *)cmd ) )
      return FALSE;
    else
      return TRUE;
  }

  return FALSE;
}

gboolean start_ntp_daemon( void )
{
  if( ntp_daemon_enabled() && ntp_daemon_status() )
    return TRUE;

  if( ntp_daemon_enabled() )
  {
    gchar *cmd;
    cmd = g_strconcat( NTPD_RC, " start", NULL );
    if( !exec_cmd( (const gchar *)cmd ) )
      return FALSE;
    else
      return TRUE;
  }

  return FALSE;
}

gboolean disable_ntp_daemon( void )
{
  gchar *cmd;

  if( !ntp_daemon_enabled() )
    return TRUE;

  if( ntp_daemon_status() )
    (void)stop_ntp_daemon();

  if(  g_file_test( NTPD_RC, G_FILE_TEST_EXISTS ) &&
      !g_file_test( NTPD_RC, G_FILE_TEST_IS_EXECUTABLE ) )
    return TRUE;

  if( g_file_test( NTPD_RC, G_FILE_TEST_EXISTS ) )
  {
    cmd = g_strconcat( "chmod 0644 ", NTPD_RC, NULL );
    if( !exec_cmd( (const gchar *)cmd ) )
      return FALSE;
    else
      return TRUE;
  }
  else
    return FALSE;
}

gboolean enable_ntp_daemon( void )
{
  gchar *cmd;

  if( ntp_daemon_enabled() )
    return TRUE;

  if( g_file_test( NTPD_RC, G_FILE_TEST_EXISTS ) &&
      g_file_test( NTPD_RC, G_FILE_TEST_IS_EXECUTABLE ) )
    return TRUE;

  if( g_file_test( NTPD_RC, G_FILE_TEST_EXISTS ) )
  {
    cmd = g_strconcat( "chmod 0755 ", NTPD_RC, NULL );
    if( !exec_cmd( (const gchar *)cmd ) )
      return FALSE;
    else
      return TRUE;
  }
  else
    return FALSE;
}
