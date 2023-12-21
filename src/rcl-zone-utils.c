
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

#include "rcl-zone-utils.h"

static gsize strv_lenght( const gchar *const *list )
{
  gsize   len = 0;
  gchar **p   = (gchar **)list;

  if( !list || *list == NULL )
    return 0;

  while( *p )
  {
    ++len;
    ++p;
  }

  return len;
}

static gboolean strv_append( const gchar *const **list, const gchar *value )
{
  gchar **c;
  gchar  *v;
  gsize   len;

  if( !list || !value )
    return FALSE;

  len = strv_lenght( *list );

  v = g_strdup( value );
  if( !v )
    return FALSE;

  c = g_realloc( (gpointer)*list, len * sizeof(gchar *) + sizeof(gchar *) * 2 );
  if( !c )
  {
    g_free( (gpointer)v );
    return FALSE;
  }

  c[len]   = v;
  c[len+1] = NULL;

  *list = (const gchar *const *)c;

  return TRUE;
}

static gint comparator( gconstpointer item1, gconstpointer item2 )
{
  return g_strcmp0( item1, item2 );
}

static GSList *g_slist_insert_sorted_unique( GSList *list, gpointer data, GCompareFunc func )
{
  if( !data || !func )
    return list;

  if( !g_slist_find_custom( list, data, func ) )
    list = g_slist_insert_sorted( list, data, func );

  return list;
}

static GSList *get_timezones_from_tzdata_zi( void )
{
  GSList *list = NULL;
  FILE   *fp   = NULL;
  gchar  *ln   = NULL, *line = NULL;

  fp = fopen( "/usr/share/zoneinfo/tzdata.zi", "r" );
  if( !fp )
    return list;

  line = (gchar *)g_malloc0( (gsize)PATH_MAX );
  if( !line )
  {
    fclose( fp );
    return list;
  }

  /**********************************************
    Zone line format is: 'Zone' 'timezone' ...
    Link line format is: 'Link' 'target' 'alias'
    See `man (8) zic' for infirmation.
   **********************************************/
  while( (ln = fgets( line, PATH_MAX, fp )) )
  {
    gchar  *p, *q;

    if( !g_ascii_strncasecmp( ln, "Z", 1 ) )
    {
      p = &ln[1];

      /* Skip spaces */
      while( (*p == ' ' || *p == '\t') && *p != '\n' )
        ++p;

      q = p;

      /* Take the first entry */
      while( *q != ' ' && *q != '\t' && *q != '\n' )
        ++q;

      *q = '\0';

      list = g_slist_insert_sorted_unique( list, (gpointer)g_strdup( p ), (GCompareFunc)comparator );
      continue;
    }
    else if( !g_ascii_strncasecmp( ln, "L", 1 ) )
    {
      p = &ln[1];

      /* Skip spaces */
      while( (*p == ' ' || *p == '\t') && *p != '\n' )
        ++p;

      q = p;

      /* Skip the first entry */
      while( *q != ' ' && *q != '\t' && *q != '\n' )
        ++q;

      p = q;

      /* Skip spaces */
      while( (*p == ' ' || *p == '\t') && *p != '\n' )
        ++p;

      q = p;

      /* Take the second entry */
      while( *q != ' ' && *q != '\t' && *q != '\n' )
        ++q;

      *q = '\0';

      list = g_slist_insert_sorted_unique( list, (gpointer)g_strdup( p ), (GCompareFunc)comparator );
    }
    else
    {
      continue;
    }
  }

  g_free( (gpointer)line );
  fclose( fp );

  return list;
}

void timezones_free( const gchar *const **list )
{
  gchar **c, **p;

  if( !list || *list == NULL )
    return;

  p = c = (gpointer)*list;

  while( *p )
  {
    g_free( (gpointer)*p );
    ++p;
  }

  g_free( (gpointer)c );

  *list = (const gchar *const *)NULL;
}

void timezones_print( const gchar *const **list )
{
  gchar **p;

  if( !list || *list == NULL )
    return;

  p = (gpointer)*list;

  while( *p )
  {
    g_print( "%s\n", *p );
    ++p;
  }
}

gboolean get_timezones( const gchar *const **list )
{
  GSList   *slist = NULL, *iterator = NULL;
  gboolean  ret   = TRUE;

  slist = get_timezones_from_tzdata_zi();
  if( !slist )
    return FALSE;

  for( iterator = slist; iterator; iterator = iterator->next )
  {
    gboolean rc = strv_append( list, (const gchar *)iterator->data );
    if( !rc )
      ret = FALSE;

    g_free( (gpointer)iterator->data );
  }
  g_slist_free(slist);

  return ret;
}

