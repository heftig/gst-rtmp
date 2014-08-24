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

#include <gst/gst.h>
#include <gio/gio.h>
#include <stdlib.h>
#include "rtmpserver.h"

#define GETTEXT_PACKAGE NULL

static void
add_connection (GstRtmpServer * server, GstRtmpServerConnection * connection,
    gpointer user_data);
static void
got_chunk (GstRtmpServerConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data);
static void dump_data (GBytes * bytes);


gboolean verbose;

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
  {NULL}
};


int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GMainLoop *main_loop;
  GstRtmpServer *server;

  context = g_option_context_new ("- FIXME");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }
  g_option_context_free (context);

  server = gst_rtmp_server_new ();
  g_signal_connect (server, "add-connection", G_CALLBACK (add_connection),
      NULL);
  gst_rtmp_server_start (server);

  main_loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (main_loop);

  exit (0);
}

static void
add_connection (GstRtmpServer * server, GstRtmpServerConnection * connection,
    gpointer user_data)
{
  GST_ERROR ("new connection");

  g_signal_connect (connection, "got-chunk", G_CALLBACK (got_chunk), NULL);
}

static void
got_chunk (GstRtmpServerConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data)
{
  GBytes *bytes;

  GST_ERROR ("got chunk");

  bytes = gst_rtmp_chunk_get_payload (chunk);
  dump_data (bytes);
}

static void
dump_data (GBytes * bytes)
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
