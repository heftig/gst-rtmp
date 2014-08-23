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
#include "rtmpclient.h"

#define GETTEXT_PACKAGE NULL

gboolean verbose;

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
  {NULL}
};

static void
connect_done (GObject *source, GAsyncResult *result, gpointer user_data);

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GMainLoop *main_loop;
  GstRtmpClient *client;
  GCancellable *cancellable;

  context = g_option_context_new ("- FIXME");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }
  g_option_context_free (context);


  client = gst_rtmp_client_new ();
  cancellable = g_cancellable_new ();

  main_loop = g_main_loop_new (NULL, TRUE);

  gst_rtmp_client_connect_async (client, cancellable, connect_done,
      client);

  g_main_loop_run (main_loop);

  exit (0);
}

static void
connect_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GstRtmpClient *client = user_data;
  GError *error = NULL;
  gboolean ret;

  ret = gst_rtmp_client_connect_finish (client, result, &error);
  if (!ret) {
    GST_ERROR("error: %s", error->message);
    g_error_free (error);
  }

  GST_ERROR("got here");
}

