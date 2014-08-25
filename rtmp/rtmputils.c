/* GStreamer RTMP Library
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rtmputils.h"


void
gst_rtmp_dump_data (GBytes * bytes)
{
  const guint8 *data;
  gsize size;
  int i, j;

  data = g_bytes_get_data (bytes, &size);
  for (i = 0; i < size; i += 16) {
    g_print ("%04x: ", i);
    for (j = 0; j < 16; j++) {
      if (i + j < size) {
        g_print ("%02x ", data[i + j]);
      } else {
        g_print ("   ");
      }
    }
    for (j = 0; j < 16; j++) {
      if (i + j < size) {
        g_print ("%c", g_ascii_isprint (data[i + j]) ? data[i + j] : '.');
      }
    }
    g_print ("\n");
  }
}
