
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

#include "rcl-time-utils.h"


#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


static const char *rtc_dev_name;
static int         rtc_dev_fd = -1;

/***************************************************************
  Static RTC functions:
 */
static void close_rtc( void )
{
  if( rtc_dev_fd != 1 )
    close( rtc_dev_fd );
  rtc_dev_fd = -1;
}

static int open_rtc( void )
{
  static const char *fls[] = {
    "/dev/rtc0",
    "/dev/rtc",
    "/dev/misc/rtc"
  };
  size_t i;

  if( rtc_dev_fd != -1 )
    return rtc_dev_fd;

  if( rtc_dev_name )
  {
    rtc_dev_fd = open( rtc_dev_name, O_RDONLY );
  }
  else
  {
    for( i = 0; i < ARRAY_SIZE(fls); ++i )
    {
      rtc_dev_fd = open( fls[i], O_RDONLY );

      if( rtc_dev_fd < 0 )
      {
        if( errno == ENOENT || errno == ENODEV )
          continue;
      }
      rtc_dev_name = fls[i];
      break;
    }
    if( rtc_dev_fd < 0 )
      rtc_dev_name = *fls; /* default for error messages */
  }

  if( rtc_dev_fd != -1 )
    atexit( close_rtc );

  return rtc_dev_fd;
}

/***************************************************************
  Static functions:
 */
static gboolean
symlink_atomic( const char *target, const char *link_path )
{
  if( symlink( target, link_path ) == 0 )
    return TRUE;
  else
    return FALSE;
}

static gchar *skip_root( const gchar *path )
{
  if( !path || *path == '\0' ) return NULL;

  return (gchar *)(path + 1);
}

/***************************************************************
  Clock functions:
 */
gboolean ntp_synchronized( void )
{
  struct timex txc = {};

  if( adjtimex( &txc ) < 0 )
    return FALSE;

  /*
    Consider the system clock synchronized if the reported maximum error is
    smaller than the maximum value (16 seconds). Ignore the STA_UNSYNC flag
    as it may have been set to prevent the kernel from touching the RTC.
   */
  /* return txc.maxerror < 16000000; */
  return txc.maxerror < 32000000;
}


guint64 timespec_load( const struct timespec *ts )
{
  if( !ts )
    return (guint64)0;

  if( ts->tv_sec < 0 || ts->tv_nsec < 0 )
    return USEC_INFINITY;

  if( (guint64)ts->tv_sec > (UINT64_MAX - (ts->tv_nsec / NSEC_PER_USEC)) / USEC_PER_SEC )
    return USEC_INFINITY;

  return
    (guint64)ts->tv_sec  * USEC_PER_SEC +
    (guint64)ts->tv_nsec / NSEC_PER_USEC;
}

guint64 timespec_load_nsec( const struct timespec *ts )
{
  if( !ts )
    return (guint64)0;

  if( ts->tv_sec < 0 || ts->tv_nsec < 0 )
    return NSEC_INFINITY;

  if( (guint64)ts->tv_sec >= (UINT64_MAX - ts->tv_nsec) / NSEC_PER_SEC )
    return NSEC_INFINITY;

  return (guint64) ts->tv_sec * NSEC_PER_SEC + (guint64)ts->tv_nsec;
}

struct timespec *timespec_store( struct timespec *ts, guint64 u )
{
  if( !ts )
    return NULL;

  if( u == USEC_INFINITY ||
      u / USEC_PER_SEC >= TIME_T_MAX )
  {
    ts->tv_sec = (time_t) -1;
    ts->tv_nsec = -1L;
    return ts;
  }

  ts->tv_sec = (time_t)(u / USEC_PER_SEC);
  ts->tv_nsec = (long)((u % USEC_PER_SEC) * NSEC_PER_USEC);

  return ts;
}

struct timespec *timespec_store_nsec( struct timespec *ts, guint64 n )
{
  if( !ts )
    return NULL;

  if( n == NSEC_INFINITY ||
      n / NSEC_PER_SEC >= TIME_T_MAX )
  {
    ts->tv_sec = (time_t) -1;
    ts->tv_nsec = -1L;
    return ts;
  }

  ts->tv_sec = (time_t)(n / NSEC_PER_SEC);
  ts->tv_nsec = (long)(n % NSEC_PER_SEC);

  return ts;
}

guint64 timeval_load( const struct timeval *tv )
{
  if( !tv )
    return (guint64)0;

  if( tv->tv_sec < 0 || tv->tv_usec < 0 )
    return USEC_INFINITY;

  if( (guint64)tv->tv_sec > (UINT64_MAX - tv->tv_usec) / USEC_PER_SEC )
    return USEC_INFINITY;

  return
    (guint64) tv->tv_sec * USEC_PER_SEC +
    (guint64) tv->tv_usec;
}

struct timeval *timeval_store( struct timeval *tv, guint64 u )
{
  if( !tv )
    return NULL;

  if( u == USEC_INFINITY ||
      u / USEC_PER_SEC > TIME_T_MAX )
  {
    tv->tv_sec = (time_t) -1;
    tv->tv_usec = (suseconds_t) -1;
  }
  else
  {
    tv->tv_sec = (time_t)(u / USEC_PER_SEC);
    tv->tv_usec = (suseconds_t)(u % USEC_PER_SEC);
  }

  return tv;
}

guint64 now( clockid_t clock_id )
{
  struct timespec ts;

  if( clock_gettime( CLOCK_REALTIME, &ts ) != 0 )
    return (guint64)0;

  return (guint64)timespec_load( &ts );
}

guint64 now_nsec( clockid_t clock_id )
{
  struct timespec ts;

  if( clock_gettime( CLOCK_REALTIME, &ts ) != 0 )
    return (guint64)0;

  return timespec_load_nsec( &ts );
}


struct tm *localtime_or_gmtime_r( const time_t *t, struct tm *tm, gboolean utc )
{
  if( !t || !tm ) return NULL;

  return utc ? gmtime_r(t, tm) : localtime_r(t, tm);
}

time_t mktime_or_timegm( struct tm *tm, gboolean utc )
{
  if( !tm ) return -1;

  return utc ? timegm(tm) : mktime(tm);
}

gboolean clock_get_hwclock( struct tm *tm )
{
  int    rc = -1;
  gchar *ioctlname;

  if( !tm )
    return FALSE;

  (void)open_rtc();

  if( rtc_dev_fd < 0 )
  {
    g_debug( "error: Canot open '%s' device", rtc_dev_name );
    return FALSE;
  }

  ioctlname = "RTC_RD_TIME";
  rc = ioctl( rtc_dev_fd, RTC_RD_TIME, tm );
  if( rc == -1 )
  {
    g_debug( "warning: ioctl(%s) to '%s' to read the time failed", ioctlname, rtc_dev_name );
  }

  tm->tm_isdst = -1; /* don't know whether it's dst */

  close_rtc();

  return TRUE;
}

gboolean clock_set_hwclock( const struct tm *tm )
{
  int    rc = -1;
  gchar *ioctlname;

  (void)open_rtc();

  if( rtc_dev_fd < 0 )
  {
    g_debug( "error: Canot open '%s' device", rtc_dev_name );
    return FALSE;
  }

  ioctlname = "RTC_SET_TIME";
  rc = ioctl( rtc_dev_fd, RTC_SET_TIME, tm );
  if( rc == -1 )
  {
    g_debug( "warning: ioctl(%s) to '%s' to set the time failed", ioctlname, rtc_dev_name );
  }

  close_rtc();

  return TRUE;
}

gboolean clock_set_timezone( int *ret_minutesdelta )
{
  struct timespec ts;
  struct tm       tm;
  int             minutesdelta;
  struct timezone tz;

  if( clock_gettime(CLOCK_REALTIME, &ts) != 0 )
    return FALSE;

  if( !localtime_r( &ts.tv_sec, &tm ) )
    return FALSE;

  minutesdelta = tm.tm_gmtoff / 60;

  tz = (struct timezone)
  {
    .tz_minuteswest = -minutesdelta,
    .tz_dsttime = 0, /* DST_NONE */
  };

  /* If the RTC does not run in UTC but in local time, the very first call to settimeofday() will set
   * the kernel's timezone and will warp the system clock, so that it runs in UTC instead of the local
   * time we have read from the RTC. */
  if( settimeofday( NULL, &tz ) < 0 )
    return FALSE;

  if( ret_minutesdelta )
    *ret_minutesdelta = minutesdelta;

  return TRUE;
}


/***************************************************************
  Timezone functions:
 */
gboolean timezone_is_valid( const gchar *name )
{
  const gchar *p, *fname;
  int          fd;
  gchar        buf[4];
  gboolean     slash = FALSE;
  gboolean     ret   = TRUE;

  if( !name || *name == '\0' ) return FALSE;

  /*
    Always accept "UTC" as valid timezone,
    since it's the fallback, even if user
    has no timezones installed.
   */
  if( g_strcmp0( name, "UTC" ) == 0 ) return TRUE;

  if( name[0] == '/' ) return FALSE;
  for( p = (gchar *)name; *p; ++p )
  {
    if( !g_ascii_isdigit( *p ) && !g_ascii_isalpha( *p ) && ( *p != '-' && *p != '_' && *p != '+' && *p != '/' ) )
      return FALSE;
    if( *p == '/' )
    {
      if( slash ) return FALSE;
      slash = TRUE;
    }
    else
    {
      slash = FALSE;
    }
  }

  if( slash )
    return FALSE;

  if( p - name >= PATH_MAX )
    return FALSE; /* name too long */

  fname = g_strjoin( "/", SYSTEM_ZONEINFO_DIR, name, NULL );

  fd = g_open( fname, O_RDONLY | O_CLOEXEC );
  if( fd < 0 )
  {
    /* log: "Failed to open timezone file '%s': %s", fname, strerror() */
    g_free( (gpointer)fname );
    return FALSE;
  }

  if( !g_file_test( fname, G_FILE_TEST_IS_REGULAR ) )
  {
    /* log: "Timezone file '%s' is not a regular file: %s", fname, strerror() */
    g_free( (gpointer)fname );
    return FALSE;
  }

  if( read( fd, &buf, 4 ) != 4 )
  {
    /* log: "Failed to read from timezone file '%s': %m", fname, strerror() */
    g_free( (gpointer)fname );
    return FALSE;
  }

  /* Magic from tzfile(5) */
  if( memcmp( buf, "TZif", 4 ) != 0 )
  {
    /* log: "Timezone file '%s' has wrong magic bytes", fname */
    g_free( (gpointer)fname );
    return FALSE;
  }

  g_free( (gpointer)fname );

  return ret;
}

gboolean set_system_timezone( const gchar *name )
{
  const gchar *fname, *source;
  gboolean     ret = TRUE;


  if( !name || *name == '\0' || g_strcmp0( name, "UTC" ) == 0 )
  {
    fname = g_strjoin( "/", SYSTEM_ZONEINFO_DIR, "UTC", NULL );

    if( !g_file_test( fname, G_FILE_TEST_IS_REGULAR ) )
    {
      /* log: "Timezone file '%s' is not a regular file: %s", fname, strerror() */
      g_free( (gpointer)fname );
      return FALSE;
    }
    source = g_strjoin( "/", "..", skip_root( SYSTEM_ZONEINFO_DIR ), "UTC", NULL );
  }
  else
  {
    source = g_strjoin( "/", "..", skip_root( SYSTEM_ZONEINFO_DIR ), name, NULL );
  }

  /* Create symlink to the new timezone */
  unlink( "/etc/localtime" );
  ret = symlink_atomic( (const char *)source, "/etc/localtime" );
  g_free( (gpointer)source );

  /* Make glibc notice the new timezone */
  tzset();

  /* Tell the kernel our timezone */
  (void)clock_set_timezone( NULL );

  return ret;
}

gboolean get_system_timezone( gchar **ret )
{
  GError *error = NULL;
  gchar  *link_target;
  gchar  *timezone;

  if( !ret ) return FALSE;

  link_target = g_file_read_link( "/etc/localtime", &error );

  if( error != NULL )
  {
    timezone = g_strdup( "UTC" );
    if( !timezone )
    {
      g_error_free( error );
      g_free( (gpointer)link_target );
      return FALSE;
    }
    g_error_free( error );
    goto out;
  }

  if( !g_path_is_absolute( link_target ) )
  {
    gchar *absolute_link_target = g_strdup( link_target + 2 ); /* skip '..' */
    g_free( (gpointer)link_target );
    link_target = g_strdup( absolute_link_target );
    g_free( (gpointer)absolute_link_target );
  }

  timezone = g_strdup( link_target + strlen( SYSTEM_ZONEINFO_DIR ) + 1  );

  if( !timezone_is_valid( timezone ) )
  {
    g_free( (gpointer)timezone );
    g_free( (gpointer)link_target );
    return FALSE;
  }

out:
  g_free( (gpointer)link_target );
  *ret = timezone;

  return TRUE;
}

/***************************************************************
  LocalRTC functions:
 */
static
gboolean exec_cmd( const gchar *cmd )
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

gboolean write_data_local_rtc( gboolean local_rtc )
{
  gchar    *w   = NULL;
  gboolean  ret = TRUE;
  gchar    *cmd;

  if( !g_file_test( ADJTIME_CONF, G_FILE_TEST_EXISTS ) )
  {
    if( !local_rtc )
    {
      if( !(w = g_strdup( NULL_ADJTIME_UTC )) ) return FALSE;
      if( !(g_file_set_contents( ADJTIME_CONF, w, -1, NULL )) )
      {
        g_free( (gpointer)w );
        return FALSE;
      }
      g_free( (gpointer)w );
    }
    else
    {
      if( !(w = g_strdup( NULL_ADJTIME_LOCAL )) ) return FALSE;
      if( !(g_file_set_contents( ADJTIME_CONF, w, -1, NULL )) )
      {
        g_free( (gpointer)w );
        return FALSE;
      }
      g_free( (gpointer)w );
    }

    return ret;
  }

  if( !g_file_test( HWCLOCK_CONF, G_FILE_TEST_EXISTS ) )
  {
    const gchar *localtime = "#\n"
                             "# /etc/hardwareclockn\n"
                             "#\n"
                             "# Tells how the hardware clock time is stored.\n"
                             "# You should run timeconfig to edit this file.\n"
                             "\n"
                             "localtime\n";

    const gchar *UTC = "#\n"
                       "# /etc/hardwareclockn\n"
                       "#\n"
                       "# Tells how the hardware clock time is stored.\n"
                       "# You should run timeconfig to edit this file.\n"
                       "\n"
                       "UTC\n";

    if( !local_rtc )
    {
      if( !(g_file_set_contents( HWCLOCK_CONF, UTC, -1, NULL )) ) return FALSE;
    }
    else
    {
      if( !(g_file_set_contents( HWCLOCK_CONF, localtime, -1, NULL )) ) return FALSE;
    }

    return ret;
  }


  if( !local_rtc )
  {
    cmd = g_strconcat( "sed -i 's,^LOCAL,UTC,' ", ADJTIME_CONF, NULL );
    if( !exec_cmd( (const gchar *)cmd ) ) ret = FALSE;

    cmd = g_strconcat( "sed -i 's,^localtime,UTC,' ", HWCLOCK_CONF, NULL );
    if( !exec_cmd( (const gchar *)cmd ) ) ret = FALSE;
  }
  else
  {
    cmd = g_strconcat( "sed -i 's,^UTC,LOCAL,' ", ADJTIME_CONF, NULL );
    if( !exec_cmd( (const gchar *)cmd ) ) ret = FALSE;

    cmd = g_strconcat( "sed -i 's,^UTC,localtime,' ", HWCLOCK_CONF, NULL );
    if( !exec_cmd( (const gchar *)cmd ) ) ret = FALSE;
  }

  return ret;
}

gboolean read_data_local_rtc( gboolean *local_rtc )
{
  gboolean ret = FALSE;

  if( !local_rtc ) return FALSE;

  if( g_file_test( HWCLOCK_CONF, G_FILE_TEST_EXISTS ) )
  {
    gchar *s = NULL;
    gsize  len;

    ret = g_file_get_contents( HWCLOCK_CONF, &s, &len, NULL );
    if( !ret )
      return FALSE;

    ret = g_regex_match_simple( "localtime", (const gchar *)s, 0, 0 );
    if( ret )
    {
      *local_rtc = TRUE;
      g_free( (gpointer)s );
      return TRUE;
    }

    ret = g_regex_match_simple ( "UTC", (const gchar *)s, 0, 0 );
    if( ret )
    {
      *local_rtc = FALSE;
      g_free( (gpointer)s );
      return TRUE;
    }

    return FALSE;
  }
  else if( g_file_test( ADJTIME_CONF, G_FILE_TEST_EXISTS ) )
  {
    gchar *s = NULL;
    gsize  len;

    ret = g_file_get_contents( ADJTIME_CONF, &s, &len, NULL );
    if( !ret )
      return FALSE;

    ret = g_regex_match_simple( "LOCAL", (const gchar *)s,  0, 0 );
    if( ret )
    {
      *local_rtc = TRUE;
      g_free( (gpointer)s );
      return TRUE;
    }

    ret = g_regex_match_simple ( "UTC", (const gchar *)s,  0, 0 );
    if( ret )
    {
      *local_rtc = FALSE;
      g_free( (gpointer)s );
      return TRUE;
    }

    return FALSE;
  }
  else
  {
    return ret;
  }

  return ret;
}
