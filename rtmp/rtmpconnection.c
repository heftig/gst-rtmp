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
static gboolean gst_rtmp_connection_input_ready (GInputStream * is,
    gpointer user_data);
static gboolean gst_rtmp_connection_output_ready (GOutputStream * os,
    gpointer user_data);
static void gst_rtmp_connection_client_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake1 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc);
static void gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc);
static void gst_rtmp_connection_server_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_connection_server_handshake2 (GstRtmpConnection * sc);
static void
gst_rtmp_connection_set_input_callback (GstRtmpConnection * connection,
    void (*input_callback) (GstRtmpConnection * connection),
    gsize needed_bytes);
static void gst_rtmp_connection_chunk_callback (GstRtmpConnection * sc);
static void gst_rtmp_connection_start_output (GstRtmpConnection * sc);


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
  rtmpconnection->input_chunk_cache = gst_rtmp_chunk_cache_new ();
  rtmpconnection->output_chunk_cache = gst_rtmp_chunk_cache_new ();
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
  gst_rtmp_chunk_cache_free (rtmpconnection->input_chunk_cache);
  gst_rtmp_chunk_cache_free (rtmpconnection->output_chunk_cache);

  G_OBJECT_CLASS (gst_rtmp_connection_parent_class)->finalize (object);
}

GstRtmpConnection *
gst_rtmp_connection_new (GSocketConnection * connection)
{
  GstRtmpConnection *sc;
  GInputStream *is;

  sc = g_object_new (GST_TYPE_RTMP_CONNECTION, NULL);
  sc->connection = connection;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));
  sc->input_source =
      g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (is),
      sc->cancellable);
  g_source_set_callback (sc->input_source,
      (GSourceFunc) gst_rtmp_connection_input_ready, sc, NULL);
  g_source_attach (sc->input_source, NULL);

  return sc;
}

static void
gst_rtmp_connection_start_output (GstRtmpConnection * sc)
{
  GSource *source;
  GOutputStream *os;

  if (!sc->handshake_complete)
    return;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  source =
      g_pollable_output_stream_create_source (G_POLLABLE_OUTPUT_STREAM (os),
      sc->cancellable);
  g_source_set_callback (source, (GSourceFunc) gst_rtmp_connection_output_ready,
      sc, NULL);
  g_source_attach (source, NULL);
}

static gboolean
gst_rtmp_connection_input_ready (GInputStream * is, gpointer user_data)
{
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  guint8 *data;
  gssize ret;
  GError *error = NULL;

  GST_DEBUG ("input ready");

  data = g_malloc (4096);
  ret =
      g_pollable_input_stream_read_nonblocking (G_POLLABLE_INPUT_STREAM (is),
      data, 4096, sc->cancellable, &error);
  if (ret < 0) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return G_SOURCE_REMOVE;
  }
  if (ret == 0) {
    /* FIXME probably closed */
    GST_ERROR ("closed?");
    return G_SOURCE_REMOVE;
  }

  GST_ERROR ("read %" G_GSIZE_FORMAT " bytes", ret);

  if (sc->input_bytes) {
    sc->input_bytes = gst_rtmp_bytes_append (sc->input_bytes, data, ret);
  } else {
    sc->input_bytes = g_bytes_new_take (data, ret);
  }

  //GST_ERROR("ic: %p", sc->input_callback);
  //GST_ERROR("in queue: %" G_GSIZE_FORMAT, g_bytes_get_size (sc->input_bytes));
  GST_ERROR ("needed: %" G_GSIZE_FORMAT, sc->input_needed_bytes);

  while (sc->input_callback && sc->input_bytes &&
      g_bytes_get_size (sc->input_bytes) >= sc->input_needed_bytes) {
    GstRtmpConnectionCallback callback;
    GST_ERROR ("got %" G_GSIZE_FORMAT " bytes, calling callback",
        g_bytes_get_size (sc->input_bytes));
    callback = sc->input_callback;
    sc->input_callback = NULL;
    (*callback) (sc);
  }

  return G_SOURCE_CONTINUE;
}

static gboolean
gst_rtmp_connection_output_ready (GOutputStream * os, gpointer user_data)
{
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GstRtmpChunk *chunk;
  GBytes *bytes;

  GST_DEBUG ("output ready");

  if (sc->writing) {
    GST_DEBUG ("busy writing");
    return G_SOURCE_REMOVE;
  }

  chunk = g_queue_pop_head (sc->output_queue);
  if (!chunk) {
    return G_SOURCE_REMOVE;
  }
  GST_ERROR ("popped");

  sc->writing = TRUE;
  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  bytes = gst_rtmp_chunk_serialize (chunk, sc->output_chunk_cache);
  gst_rtmp_chunk_cache_update (sc->output_chunk_cache, chunk);
  gst_rtmp_dump_data (bytes);
  g_output_stream_write_bytes_async (os, bytes, G_PRIORITY_DEFAULT,
      sc->cancellable, gst_rtmp_connection_write_chunk_done, chunk);

  //return G_SOURCE_CONTINUE;
  return G_SOURCE_REMOVE;
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

  gst_rtmp_connection_start_output (connection);
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

static GBytes *
gst_rtmp_connection_take_input_bytes (GstRtmpConnection * sc, gsize size)
{
  GBytes *bytes;
  gsize current_size;

  current_size = g_bytes_get_size (sc->input_bytes);
  g_return_val_if_fail (size <= current_size, NULL);

  if (current_size == size) {
    bytes = sc->input_bytes;
    sc->input_bytes = NULL;
  } else {
    GBytes *remaining_bytes;
    bytes = g_bytes_new_from_bytes (sc->input_bytes, 0, size);
    remaining_bytes = g_bytes_new_from_bytes (sc->input_bytes, size,
        current_size - size);
    g_bytes_unref (sc->input_bytes);
    sc->input_bytes = remaining_bytes;
  }

  return bytes;
}

static void
gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc)
{
  GOutputStream *os;
  GBytes *bytes;
  guint8 *data;

  bytes = gst_rtmp_connection_take_input_bytes (sc, 1537);
  data = g_malloc (1 + 1536 + 1536);
  memcpy (data, g_bytes_get_data (bytes, NULL), 1 + 1536);
  memset (data + 1537, 0, 8);
  memset (data + 1537 + 8, 0xef, 1528);
  g_bytes_unref (bytes);

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  g_output_stream_write_async (os, data, 1 + 1536 + 1536,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_connection_server_handshake1_done, sc);
}

static void
gst_rtmp_connection_server_handshake1_done (GObject * obj,
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

  gst_rtmp_connection_set_input_callback (sc,
      gst_rtmp_connection_server_handshake2, 1536);
}

static void
gst_rtmp_connection_server_handshake2 (GstRtmpConnection * sc)
{
  GBytes *bytes;

  bytes = gst_rtmp_connection_take_input_bytes (sc, 1536);
  g_bytes_unref (bytes);

  /* handshake finished */
  GST_ERROR ("server handshake finished");
  sc->handshake_complete = TRUE;

  if (sc->input_bytes && g_bytes_get_size (sc->input_bytes) >= 1) {
    GST_ERROR ("spare bytes after handshake: %" G_GSIZE_FORMAT,
        g_bytes_get_size (sc->input_bytes));
    gst_rtmp_connection_chunk_callback (sc);
  }

  gst_rtmp_connection_set_input_callback (sc,
      gst_rtmp_connection_chunk_callback, 0);

  gst_rtmp_connection_start_output (sc);
}

static void
gst_rtmp_connection_chunk_callback (GstRtmpConnection * sc)
{
  gsize needed_bytes = 0;
  gsize size = 0;

  while (1) {
    GstRtmpChunkHeader header = { 0 };
    GstRtmpChunkCacheEntry *entry;
    GBytes *bytes;
    gboolean ret;
    gsize remaining_bytes;
    gsize chunk_bytes;
    const guint8 *data;

    if (sc->input_bytes == NULL)
      break;

    ret = gst_rtmp_chunk_parse_header1 (&header, sc->input_bytes);
    if (!ret) {
      needed_bytes = header.header_size;
      break;
    }

    entry = gst_rtmp_chunk_cache_get (sc->input_chunk_cache, header.stream_id);

    if (entry->chunk && header.format != 3) {
      GST_ERROR ("expected message continuation, got new message");
    }

    ret = gst_rtmp_chunk_parse_header2 (&header, sc->input_bytes,
        &entry->previous_header);
    if (!ret) {
      needed_bytes = header.header_size;
      break;
    }

    remaining_bytes = header.message_length - entry->offset;
    chunk_bytes = MIN (remaining_bytes, 128);
    data = g_bytes_get_data (sc->input_bytes, &size);

    if (header.header_size + chunk_bytes > size) {
      needed_bytes = header.header_size + chunk_bytes;
      break;
    }

    if (entry->chunk == NULL) {
      entry->chunk = gst_rtmp_chunk_new ();
      entry->chunk->stream_id = header.stream_id;
      entry->chunk->timestamp = header.timestamp;
      entry->chunk->message_length = header.message_length;
      entry->chunk->message_type_id = header.message_type_id;
      entry->chunk->info = header.info;
      entry->payload = g_malloc (header.message_length);
    }
    memcpy (&entry->previous_header, &header, sizeof (header));

    memcpy (entry->payload + entry->offset, data + header.header_size,
        chunk_bytes);
    entry->offset += chunk_bytes;

    bytes = gst_rtmp_connection_take_input_bytes (sc,
        header.header_size + chunk_bytes);
    g_bytes_unref (bytes);

    if (entry->offset == header.message_length) {
      entry->chunk->payload = g_bytes_new_take (entry->payload,
          header.message_length);
      entry->payload = NULL;

      gst_rtmp_dump_data (entry->chunk->payload);

      GST_ERROR ("got chunk: %" G_GSIZE_FORMAT " bytes",
          entry->chunk->message_length);
      g_signal_emit_by_name (sc, "got-chunk", entry->chunk);
      g_object_unref (entry->chunk);

      entry->chunk = NULL;
      entry->offset = 0;
    }
  }
  GST_ERROR ("setting needed bytes to %" G_GSIZE_FORMAT ", have %"
      G_GSIZE_FORMAT, needed_bytes, size);
  gst_rtmp_connection_set_input_callback (sc,
      gst_rtmp_connection_chunk_callback, needed_bytes);
}

void
gst_rtmp_connection_queue_chunk (GstRtmpConnection * connection,
    GstRtmpChunk * chunk)
{
  g_return_if_fail (GST_IS_RTMP_CONNECTION (connection));
  g_return_if_fail (GST_IS_RTMP_CHUNK (chunk));

  GST_ERROR ("queued");
  chunk->priv = connection;
  g_queue_push_tail (connection->output_queue, chunk);
  gst_rtmp_connection_start_output (connection);
}

static void
gst_rtmp_connection_set_input_callback (GstRtmpConnection * connection,
    void (*input_callback) (GstRtmpConnection * connection), gsize needed_bytes)
{
  if (connection->input_callback) {
    GST_ERROR ("already an input callback");
  }
  connection->input_callback = input_callback;
  connection->input_needed_bytes = needed_bytes;
  if (needed_bytes == 0) {
    connection->input_needed_bytes = 1;
  }

  if (connection->input_callback && connection->input_bytes &&
      g_bytes_get_size (connection->input_bytes) >=
      connection->input_needed_bytes) {
    GstRtmpConnectionCallback callback;
    GST_ERROR ("got %" G_GSIZE_FORMAT " bytes, calling callback",
        g_bytes_get_size (connection->input_bytes));
    callback = connection->input_callback;
    connection->input_callback = NULL;
    (*callback) (connection);
  }

}

void
gst_rtmp_connection_start_handshake (GstRtmpConnection * connection,
    gboolean is_server)
{
  if (is_server) {
    gst_rtmp_connection_set_input_callback (connection,
        gst_rtmp_connection_server_handshake1, 1537);
  } else {
    gst_rtmp_connection_client_handshake1 (connection);
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

  GST_DEBUG ("gst_rtmp_connection_client_handshake1_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1 + 1536) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  gst_rtmp_connection_set_input_callback (sc,
      gst_rtmp_connection_client_handshake2, 1 + 1536 + 1536);
}

static void
gst_rtmp_connection_client_handshake2 (GstRtmpConnection * sc)
{
  GBytes *bytes;
  GBytes *out_bytes;
  GOutputStream *os;

  bytes = gst_rtmp_connection_take_input_bytes (sc, 1 + 1536 + 1536);
  out_bytes = g_bytes_new_from_bytes (bytes, 1 + 1536, 1536);
  g_bytes_unref (bytes);

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  g_output_stream_write_bytes_async (os, out_bytes, G_PRIORITY_DEFAULT,
      sc->cancellable, gst_rtmp_connection_client_handshake2_done, sc);
}

static void
gst_rtmp_connection_client_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpConnection *sc = GST_RTMP_CONNECTION (user_data);
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_client_handshake2_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1536) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_DEBUG ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  /* handshake finished */
  GST_ERROR ("client handshake finished");
  sc->handshake_complete = TRUE;

  if (sc->input_bytes && g_bytes_get_size (sc->input_bytes) >= 1) {
    GST_ERROR ("spare bytes after handshake: %" G_GSIZE_FORMAT,
        g_bytes_get_size (sc->input_bytes));
    gst_rtmp_connection_chunk_callback (sc);
  } else {
    gst_rtmp_connection_set_input_callback (sc,
        gst_rtmp_connection_chunk_callback, 0);
  }
  gst_rtmp_connection_start_output (sc);
}

void
gst_rtmp_connection_dump (GstRtmpConnection * connection)
{
  g_print ("  output_queue: %d\n",
      g_queue_get_length (connection->output_queue));
  g_print ("  input_bytes: %" G_GSIZE_FORMAT "\n",
      connection->input_bytes ? g_bytes_get_size (connection->input_bytes) : 0);
  g_print ("  needed: %" G_GSIZE_FORMAT "\n", connection->input_needed_bytes);

}
