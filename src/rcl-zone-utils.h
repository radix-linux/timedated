
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

#ifndef __RCL_ZONE_UTILS_H__
#define __RCL_ZONE_UTILS_H__

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

extern gboolean  get_timezones   ( const gchar *const **list );
extern void      timezones_free  ( const gchar *const **list );
extern void      timezones_print ( const gchar *const **list );


#endif /* __RCL_ZONE_UTILS_H__ */
