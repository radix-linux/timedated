
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

#ifndef __RCL_TIME_UTILS_H__
#define __RCL_TIME_UTILS_H__

#include "config.h"

#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <locale.h>

#define USEC_INFINITY   ((guint64) UINT64_MAX)
#define NSEC_INFINITY   ((guint64) UINT64_MAX)

#define MSEC_PER_SEC    1000ULL
#define USEC_PER_SEC    ((guint64) 1000000ULL)
#define USEC_PER_MSEC   ((guint64) 1000ULL)
#define NSEC_PER_SEC    ((guint64) 1000000000ULL)
#define NSEC_PER_MSEC   ((guint64) 1000000ULL)
#define NSEC_PER_USEC   ((guint64) 1000ULL)

#define USEC_PER_MINUTE ((guint64) (60ULL*USEC_PER_SEC))
#define NSEC_PER_MINUTE ((guint64) (60ULL*NSEC_PER_SEC))
#define USEC_PER_HOUR   ((guint64) (60ULL*USEC_PER_MINUTE))
#define NSEC_PER_HOUR   ((guint64) (60ULL*NSEC_PER_MINUTE))
#define USEC_PER_DAY    ((guint64) (24ULL*USEC_PER_HOUR))
#define NSEC_PER_DAY    ((guint64) (24ULL*NSEC_PER_HOUR))
#define USEC_PER_WEEK   ((guint64) (7ULL*USEC_PER_DAY))
#define NSEC_PER_WEEK   ((guint64) (7ULL*NSEC_PER_DAY))
#define USEC_PER_MONTH  ((guint64) (2629800ULL*USEC_PER_SEC))
#define NSEC_PER_MONTH  ((guint64) (2629800ULL*NSEC_PER_SEC))
#define USEC_PER_YEAR   ((guint64) (31557600ULL*USEC_PER_SEC))
#define NSEC_PER_YEAR   ((guint64) (31557600ULL*NSEC_PER_SEC))

#define TIME_T_MAX (time_t)((UINTMAX_C(1) << ((sizeof(time_t) << 3) - 1)) - 1)


#define SYSTEM_ZONEINFO_DIR "/usr/share/zoneinfo"

#define NULL_ADJTIME_UTC "0.0 0 0.0\n0\nUTC\n"
#define NULL_ADJTIME_LOCAL "0.0 0 0.0\n0\nLOCAL\n"

#if !defined( HWCLOCK_CONF )
#define HWCLOCK_CONF "/etc/hardwareclock"
#endif

#if !defined( ADJTIME_CONF )
#define ADJTIME_CONF "/etc/adjtime"
#endif


extern gboolean   ntp_synchronized      ( void );

extern guint64          timespec_load       ( const struct timespec *ts );
extern guint64          timespec_load_nsec  ( const struct timespec *ts );
extern struct timespec *timespec_store      ( struct timespec *ts, guint64 u );
extern struct timespec *timespec_store_nsec ( struct timespec *ts, guint64 n );
extern guint64          timeval_load        ( const struct timeval *tv );
extern struct timeval  *timeval_store       ( struct timeval *tv, guint64 u );
extern guint64          now                 ( clockid_t clock_id );
extern guint64          now_nsec            ( clockid_t clock_id );

extern struct tm *localtime_or_gmtime_r ( const time_t *t, struct tm *tm, gboolean utc );
extern time_t     mktime_or_timegm      ( struct tm *tm, gboolean utc );

extern gboolean   clock_get_hwclock     ( struct tm *tm );
extern gboolean   clock_set_hwclock     ( const struct tm *tm );

extern gboolean   clock_set_timezone    ( int *ret_minutesdelta );

extern gboolean   timezone_is_valid     ( const gchar *name );
extern gboolean   set_system_timezone   ( const gchar *name );
extern gboolean   get_system_timezone   ( gchar **ret );

extern gboolean   write_data_local_rtc  ( gboolean local_rtc );
extern gboolean   read_data_local_rtc   ( gboolean *local_rtc );


#endif /* __RCL_TIME_UTILS_H__ */
