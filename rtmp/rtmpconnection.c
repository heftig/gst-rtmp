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
#include "rtmpconnection.h"
#include "rtmpchunk.h"
#include "amf.h"
#include "rtmputils.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_connection_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_connection_debug_category

/* prototypes */

static void gst_rtmp_connection_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp_connection_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp_connection_dispose (GObject * object);
static void gst_rtmp_connection_finalize (GObject * object);
static void gst_rtmp_connection_client_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake3 (GstRtmpConnection * sc,
    GBytes * bytes);
static void gst_rtmp_connection_client_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake1 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake3 (GstRtmpConnection * sc,
    GBytes * bytes);
static void gst_rtmp_connection_client_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_read_chunk (GstRtmpConnection * sc);
static void gst_rtmp_connection_read_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_write_chunk (GstRtmpConnection * sc);
static void gst_rtmp_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc);
static void gst_rtmp_connection_server_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_server_handshake2 (GstRtmpConnection * sc,
    GBytes * bytes);
static void gst_rtmp_connection_server_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_server_handshake3 (GstRtmpConnection * sc);
static void gst_rtmp_connection_server_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);


static void gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmpConnection, gst_rtmp_connection,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_connection_debug_category,
        "rtmpconnection", 0, "debug category for GstRtmpConnection class"));

static void
gst_rtmp_connection_class_init (GstRtmpConnectionClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_rtmp_connection_set_property;
  gobject_class->get_property = gst_rtmp_connection_get_property;
  gobject_class->dispose = gst_rtmp_connection_dispose;
  gobject_class->finalize = gst_rtmp_connection_finalize;

  g_signal_new ("got-chunk", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstRtmpConnectionClass,
          got_chunk), NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 1, GST_TYPE_RTMP_CHUNK);
}

static void
gst_rtmp_connection_init (GstRtmpConnection * rtmpconnection)
{
  rtmpconnection->cancellable = g_cancellable_new ();
  rtmpconnection->output_queue = g_queue_new ();
}

void
gst_rtmp_connection_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmpConnection *rtmpconnection = GST_RTMP_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpconnection, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_connection_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmpConnection *rtmpconnection = GST_RTMP_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpconnection, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_connection_dispose (GObject * object)
{
  GstRtmpConnection *rtmpconnection = GST_RTMP_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpconnection, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp_connection_parent_class)->dispose (object);
}

void
gst_rtmp_connection_finalize (GObject * object)
{
  GstRtmpConnection *rtmpconnection = GST_RTMP_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpconnection, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp_connection_parent_class)->finalize (object);
}

GstRtmpConnection *
gst_rtmp_connection_new (GSocketConnection * connection)
{
  GstRtmpConnection *sc;

  sc = g_object_new (GST_TYPE_RTMP_CONNECTION, NULL);
  sc->connection = connection;

  return sc;
}



static void
gst_rtmp_connection_read_chunk (GstRtmpConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 4096,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_read_chunk_done, sc);
}

G_GNUC_UNUSED static void
parse_message (guint8 * data, int size)
{
  int offset;
  int bytes_read;
  GstAmfNode *node;

  offset = 4;

  node = gst_amf_node_new_parse ((const char *) (data + offset),
      size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %d\n", bytes_read);
  if (node)
    gst_amf_node_free (node);

  node = gst_amf_node_new_parse ((const char *) (data + offset),
      size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %d\n", bytes_read);
  if (node)
    gst_amf_node_free (node);

  node = gst_amf_node_new_parse ((const char *) (data + offset),
      size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %d\n", bytes_read);
  if (node)
    gst_amf_node_free (node);

}

static void
gst_rtmp_connection_read_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpConnection *connection = GST_RTMP_CONNECTION (user_data);
  GstRtmpChunk *chunk;
  GError *error = NULL;
  gsize chunk_size;
  GBytes *bytes;

  GST_DEBUG ("gst_rtmp_connection_read_chunk_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_dump_data (bytes);
  chunk = gst_rtmp_chunk_new_parse (bytes, &chunk_size);

  if (chunk) {
    g_signal_emit_by_name (connection, "got-chunk", chunk);

    g_object_unref (chunk);
  }

  gst_rtmp_connection_read_chunk (connection);
}

static void
gst_rtmp_connection_write_chunk (GstRtmpConnection * sc)
{
  GOutputStream *os;
  GBytes *bytes;
  GstRtmpChunk *chunk;

  if (sc->writing) {
    GST_WARNING ("gst_rtmp_connection_write_chunk while writing == TRUE");
    return;
  }

  chunk = g_queue_pop_head (sc->output_queue);
  if (!chunk) {
    return;
  }

  sc->writing = TRUE;
  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  bytes = gst_rtmp_chunk_serialize (chunk);
  gst_rtmp_dump_data (bytes);
  g_output_stream_write_bytes_async (os, bytes, G_PRIORITY_DEFAULT,
      sc->cancellable, gst_rtmp_connection_write_chunk_done, chunk);
}

static void
gst_rtmp_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpChunk *chunk = GST_RTMP_CHUNK (user_data);
  GstRtmpConnection *connection = GST_RTMP_CONNECTION (chunk->priv);
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_write_chunk_done");

  connection->writing = FALSE;

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 0) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("write %" G_GSSIZE_FORMAT " bytes", ret);
  g_object_unref (chunk);

  gst_rtmp_connection_write_chunk (connection);
}

static void
gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 4096,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_server_handshake1_done, sc);
}

static void
gst_rtmp_connection_server_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  GBytes *bytes;

  GST_DEBUG ("gst_rtmp_connection_server_handshake1_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_connection_server_handshake2 (sc, bytes);
}

static void
gst_rtmp_connection_server_handshake2 (GstRtmpConnection * sc, GBytes * bytes)
{
  GOutputStream *os;
  guint8 *data;
  GBytes *out_bytes;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));

  data = g_malloc (1 + 1536 + 1536);
  memcpy (data, g_bytes_get_data (bytes, NULL), 1 + 1536);
  memset (data + 1537, 0, 8);
  memset (data + 1537 + 8, 0xef, 1528);

  out_bytes = g_bytes_new_take (data, 1 + 1536 + 1536);

  g_output_stream_write_bytes_async (os, out_bytes,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_server_handshake2_done, sc);
}

static void
gst_rtmp_connection_server_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_server_handshake2_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1 + 1536 + 1536) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  gst_rtmp_connection_server_handshake3 (sc);
}

static void
gst_rtmp_connection_server_handshake3 (GstRtmpConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 1536,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_server_handshake3_done, sc);
}

static void
gst_rtmp_connection_server_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  GBytes *bytes;

  GST_DEBUG ("gst_rtmp_connection_server_handshake3_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_connection_read_chunk (sc);
}

void
gst_rtmp_connection_queue_chunk (GstRtmpConnection * connection,
    GstRtmpChunk * chunk)
{
  g_return_if_fail (GST_IS_RTMP_CONNECTION (connection));
  g_return_if_fail (GST_IS_RTMP_CHUNK (chunk));

  chunk->priv = connection;
  g_queue_push_tail (connection->output_queue, chunk);
  if (!connection->writing) {
    gst_rtmp_connection_write_chunk (connection);
  }
}

void
gst_rtmp_connection_handshake_async (GstRtmpConnection * connection,
    gboolean is_server, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  GSimpleAsyncResult *async;

  async = g_simple_async_result_new (G_OBJECT (connection),
      callback, user_data, gst_rtmp_connection_handshake_async);
  g_simple_async_result_set_check_cancellable (async, cancellable);

  connection->cancellable = cancellable;
  connection->async = async;

  if (is_server) {
    gst_rtmp_connection_server_handshake1 (connection);
  } else {
    gst_rtmp_connection_client_handshake1 (connection);
  }
}

static void
gst_rtmp_connection_client_handshake1 (GstRtmpConnection * sc)
{
  GOutputStream *os;
  guint8 *data;
  GBytes *bytes;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));

  data = g_malloc (1 + 1536);
  data[0] = 3;
  memset (data + 1, 0, 8);
  memset (data + 9, 0xa5, 1528);
  bytes = g_bytes_new_take (data, 1 + 1536);

  g_output_stream_write_bytes_async (os, bytes,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_client_handshake1_done, sc);
}

static void
gst_rtmp_connection_client_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_client_handshake2_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1 + 1536) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  gst_rtmp_connection_client_handshake2 (sc);
}

static void
gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 1 + 1536 + 1536,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_client_handshake2_done, sc);
}

static void
gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  GBytes *bytes;

  GST_DEBUG ("gst_rtmp_connection_client_handshake2_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_connection_client_handshake3 (sc, bytes);
}

static void
gst_rtmp_connection_client_handshake3 (GstRtmpConnection * sc, GBytes * bytes)
{
  GOutputStream *os;
  GBytes *out_bytes;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));

  out_bytes = g_bytes_new_from_bytes (bytes, 1 + 1536, 1536);

  g_output_stream_write_bytes_async (os, out_bytes, G_PRIORITY_DEFAULT,
      sc->cancellable, gst_rtmp_connection_client_handshake3_done, sc);
}

static void
gst_rtmp_connection_client_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_client_handshake3_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1536) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  g_simple_async_result_complete (sc->async);

  gst_rtmp_connection_read_chunk (sc);
}

gboolean
gst_rtmp_connection_handshake_finish (GstRtmpConnection * connection,
    GAsyncResult * result, GError ** error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (connection), gst_rtmp_connection_handshake_async), FALSE);

  simple = (GSimpleAsyncResult *) result;

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}
