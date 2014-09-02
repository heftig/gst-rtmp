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
#include <string.h>
#include "rtmpclient.h"
#include "rtmputils.h"


#define GETTEXT_PACKAGE NULL

GstRtmpClient *client;
GstRtmpConnection *connection;

gboolean verbose;
gboolean dump;

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
  {"dump", 'd', 0, G_OPTION_ARG_NONE, &dump, "Dump packets", NULL},
  {NULL}
};

static void connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void cmd_connect_done (GstRtmpConnection * connection,
    GstRtmpChunk * chunk, const char *command_name, int transaction_id,
    GstAmfNode * command_object, GstAmfNode * optional_args,
    gpointer user_data);
static void send_connect (void);
static void got_chunk (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data);
static void send_create_stream (void);
static void create_stream_done (GstRtmpConnection * connection,
    GstRtmpChunk * chunk, const char *command_name, int transaction_id,
    GstAmfNode * command_object, GstAmfNode * optional_args,
    gpointer user_data);
static void send_play (void);
static void play_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data);

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GMainLoop *main_loop;
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
  gst_rtmp_client_set_server_address (client,
      "ec2-54-185-55-241.us-west-2.compute.amazonaws.com");
  cancellable = g_cancellable_new ();

  main_loop = g_main_loop_new (NULL, TRUE);

  gst_rtmp_client_connect_async (client, cancellable, connect_done, client);

  g_main_loop_run (main_loop);

  exit (0);
}

static void
connect_done (GObject * source, GAsyncResult * result, gpointer user_data)
{
  GstRtmpClient *client = user_data;
  GError *error = NULL;
  gboolean ret;

  ret = gst_rtmp_client_connect_finish (client, result, &error);
  if (!ret) {
    GST_ERROR ("error: %s", error->message);
    g_error_free (error);
    return;
  }

  connection = gst_rtmp_client_get_connection (client);
  g_signal_connect (connection, "got-chunk", G_CALLBACK (got_chunk), NULL);

  send_connect ();
}

static void
dump_command (GstRtmpChunk * chunk)
{
  GstAmfNode *amf;
  gsize size;
  const guint8 *data;
  gsize n_parsed;
  int offset;

  offset = 0;
  data = g_bytes_get_data (chunk->payload, &size);
  while (offset < size) {
    amf = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
    gst_amf_node_dump (amf);
    gst_amf_node_free (amf);
    offset += n_parsed;
  }
}

static void
dump_chunk (GstRtmpChunk * chunk, gboolean dir)
{
  if (!dump)
    return;

  g_print ("%s chunk_stream_id:%-4d ts:%-8d len:%-6" G_GSIZE_FORMAT
      " type_id:%-4d info:%08x\n", dir ? ">>>" : "<<<",
      chunk->chunk_stream_id,
      chunk->timestamp,
      chunk->message_length, chunk->message_type_id, chunk->info);
  if (chunk->message_type_id == 20) {
    dump_command (chunk);
  }
  if (chunk->message_type_id == 18) {
    dump_command (chunk);
  }
  gst_rtmp_dump_data (gst_rtmp_chunk_get_payload (chunk));
}

static void
got_chunk (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data)
{
  dump_chunk (chunk, FALSE);
}

static void
send_connect (void)
{
  GstAmfNode *node;

  node = gst_amf_node_new (GST_AMF_TYPE_OBJECT);
  gst_amf_object_set_string (node, "app", "live");
  gst_amf_object_set_string (node, "type", "nonprivate");
  gst_amf_object_set_string (node, "tcUrl", "rtmp://localhost:1935/live");
  // "fpad": False,
  // "capabilities": 15,
  // "audioCodecs": 3191,
  // "videoCodecs": 252,
  // "videoFunction": 1,
  gst_rtmp_connection_send_command (connection, 3, "connect", 1, node, NULL,
      cmd_connect_done, NULL);
}

static void
cmd_connect_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  gboolean ret;

  ret = FALSE;
  if (optional_args) {
    const GstAmfNode *n;
    n = gst_amf_node_get_object (optional_args, "code");
    if (n) {
      const char *s;
      s = gst_amf_node_get_string (n);
      if (strcmp (s, "NetConnection.Connect.Success") == 0) {
        ret = TRUE;
      }
    }
  }

  if (ret) {
    GST_ERROR ("success");

    send_create_stream ();
  }
}

static void
send_create_stream (void)
{
  GstAmfNode *node;

  node = gst_amf_node_new (GST_AMF_TYPE_NULL);
  gst_rtmp_connection_send_command (connection, 3, "createStream", 2, node,
      NULL, create_stream_done, NULL);

}

static void
create_stream_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  gboolean ret;
  int stream_id;

  ret = FALSE;
  if (optional_args) {
    stream_id = gst_amf_node_get_number (optional_args);
    ret = TRUE;
  }

  if (ret) {
    GST_ERROR ("createStream success, stream_id=%d", stream_id);

    send_play ();
  }
}

static void
send_play (void)
{
  GstAmfNode *n1;
  GstAmfNode *n2;
  GstAmfNode *n3;

  n1 = gst_amf_node_new (GST_AMF_TYPE_NULL);
  n2 = gst_amf_node_new (GST_AMF_TYPE_STRING);
  gst_amf_node_set_string (n2, "myStream");
  n3 = gst_amf_node_new (GST_AMF_TYPE_NUMBER);
  gst_amf_node_set_number (n3, 0);
  gst_rtmp_connection_send_command2 (connection, 8, 1, "play", 3, n1, n2, n3,
      NULL, play_done, NULL);

}

static void
play_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  gboolean ret;
  int stream_id;

  ret = FALSE;
  if (optional_args) {
    stream_id = gst_amf_node_get_number (optional_args);
    ret = TRUE;
  }

  if (ret) {
    GST_ERROR ("play success, stream_id=%d", stream_id);

  }
}
