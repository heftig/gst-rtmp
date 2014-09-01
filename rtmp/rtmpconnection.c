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
static void gst_rtmp_connection_got_closed (GstRtmpConnection * connection);
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
static void
gst_rtmp_connection_handle_pcm (GstRtmpConnection * connection,
    GstRtmpChunk * chunk);
static void
gst_rtmp_connection_handle_user_control (GstRtmpConnection * connectin,
    guint32 event_type, guint32 event_data);
static void gst_rtmp_connection_handle_chunk (GstRtmpConnection * sc,
    GstRtmpChunk * chunk);

static void gst_rtmp_connection_server_handshake1 (GstRtmpConnection * sc);
static void gst_rtmp_connection_send_ack (GstRtmpConnection * connection);
static void
gst_rtmp_connection_send_ping_response (GstRtmpConnection * connection,
    guint32 event_data);
static void gst_rtmp_connection_send_window_size_request (GstRtmpConnection *
    connection);

typedef struct _CommandCallback CommandCallback;
struct _CommandCallback
{
  int stream_id;
  int transaction_id;
  GstRtmpCommandCallback func;
  gpointer user_data;
};

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
  g_signal_new ("got-control-chunk", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstRtmpConnectionClass,
          got_control_chunk), NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 1, GST_TYPE_RTMP_CHUNK);
  g_signal_new ("closed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstRtmpConnectionClass, closed),
      NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void
gst_rtmp_connection_init (GstRtmpConnection * rtmpconnection)
{
  rtmpconnection->cancellable = g_cancellable_new ();
  rtmpconnection->output_queue = g_queue_new ();
  rtmpconnection->input_chunk_cache = gst_rtmp_chunk_cache_new ();
  rtmpconnection->output_chunk_cache = gst_rtmp_chunk_cache_new ();

  rtmpconnection->in_chunk_size = 128;
  rtmpconnection->out_chunk_size = 128;
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
    if (error->code == G_IO_ERROR_TIMED_OUT) {
      /* should retry */
      GST_ERROR ("timeout, continuing");
      g_error_free (error);
      return G_SOURCE_CONTINUE;
    } else {
      GST_ERROR ("read error: %s %d %s", g_quark_to_string (error->domain),
          error->code, error->message);
    }
    g_error_free (error);
    return G_SOURCE_REMOVE;
  }
  if (ret == 0) {
    gst_rtmp_connection_got_closed (sc);
    return G_SOURCE_REMOVE;
  }

  GST_DEBUG ("read %" G_GSIZE_FORMAT " bytes", ret);

  if (sc->input_bytes) {
    sc->input_bytes = gst_rtmp_bytes_append (sc->input_bytes, data, ret);
  } else {
    sc->input_bytes = g_bytes_new_take (data, ret);
  }
  sc->total_input_bytes += ret;
  sc->bytes_since_ack += ret;
  if (sc->bytes_since_ack >= sc->window_ack_size) {
    gst_rtmp_connection_send_ack (sc);
  }

  GST_DEBUG ("needed: %" G_GSIZE_FORMAT, sc->input_needed_bytes);

  while (sc->input_callback && sc->input_bytes &&
      g_bytes_get_size (sc->input_bytes) >= sc->input_needed_bytes) {
    GstRtmpConnectionCallback callback;
    GST_DEBUG ("got %" G_GSIZE_FORMAT " bytes, calling callback",
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

  GST_DEBUG ("output ready");

  if (sc->writing)
    return G_SOURCE_REMOVE;

  if (sc->output_chunk) {
    chunk = sc->output_chunk;
  } else {
    GstRtmpChunkCacheEntry *entry;

    chunk = g_queue_pop_head (sc->output_queue);
    if (!chunk) {
      return G_SOURCE_REMOVE;
    }
    sc->output_chunk = chunk;

    entry = gst_rtmp_chunk_cache_get (sc->output_chunk_cache, chunk->stream_id);
    sc->output_bytes = gst_rtmp_chunk_serialize (chunk,
        &entry->previous_header, sc->out_chunk_size);
    g_bytes_ref (sc->output_bytes);
    gst_rtmp_chunk_cache_update (entry, chunk);
  }

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));
  g_output_stream_write_bytes_async (os, sc->output_bytes, G_PRIORITY_DEFAULT,
      sc->cancellable, gst_rtmp_connection_write_chunk_done, sc);
  sc->writing = TRUE;

  return G_SOURCE_REMOVE;
}

static void
gst_rtmp_connection_got_closed (GstRtmpConnection * connection)
{
  connection->closed = TRUE;
  g_signal_emit_by_name (connection, "closed");
}

static void
gst_rtmp_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpConnection *connection = GST_RTMP_CONNECTION (user_data);
  GstRtmpChunk *chunk = connection->output_chunk;
  GError *error = NULL;
  gssize ret;

  GST_DEBUG ("gst_rtmp_connection_write_chunk_done");

  connection->writing = FALSE;

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 0) {
    GST_DEBUG ("write error: %s", error->message);
    gst_rtmp_connection_got_closed (connection);
    g_error_free (error);
    return;
  }
  if (ret < g_bytes_get_size (connection->output_bytes)) {
    GST_DEBUG ("short write %" G_GSIZE_FORMAT " < %" G_GSIZE_FORMAT,
        ret, g_bytes_get_size (connection->output_bytes));

    connection->output_bytes =
        gst_rtmp_bytes_remove (connection->output_bytes, ret);
  } else {
    g_bytes_unref (connection->output_bytes);
    connection->output_bytes = NULL;
    g_object_unref (chunk);
    connection->output_chunk = NULL;
  }

  gst_rtmp_connection_start_output (connection);
}


G_GNUC_UNUSED static void
parse_message (guint8 * data, int size)
{
  int offset;
  gsize bytes_read;
  GstAmfNode *node;

  offset = 4;

  node = gst_amf_node_new_parse (data + offset, size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %" G_GSIZE_FORMAT "\n", bytes_read);
  if (node)
    gst_amf_node_free (node);

  node = gst_amf_node_new_parse (data + offset, size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %" G_GSIZE_FORMAT "\n", bytes_read);
  if (node)
    gst_amf_node_free (node);

  node = gst_amf_node_new_parse (data + offset, size - offset, &bytes_read);
  offset += bytes_read;
  g_print ("bytes_read: %" G_GSIZE_FORMAT "\n", bytes_read);
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
  GST_INFO ("server handshake finished");
  sc->handshake_complete = TRUE;

  if (sc->input_bytes && g_bytes_get_size (sc->input_bytes) >= 1) {
    GST_DEBUG ("spare bytes after handshake: %" G_GSIZE_FORMAT,
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
      GST_ERROR ("expected message continuation, but got new message");
    }

    ret = gst_rtmp_chunk_parse_header2 (&header, sc->input_bytes,
        &entry->previous_header);
    if (!ret) {
      needed_bytes = header.header_size;
      break;
    }

    remaining_bytes = header.message_length - entry->offset;
    if (header.message_type_id == 0x12 || header.message_type_id == 0x09 ||
        header.message_type_id == 0x14) {
      chunk_bytes = MIN (remaining_bytes, sc->in_chunk_size);
    } else {
      chunk_bytes = remaining_bytes;
    }
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

      gst_rtmp_connection_handle_chunk (sc, entry->chunk);
      g_object_unref (entry->chunk);

      entry->chunk = NULL;
      entry->offset = 0;
    }
  }
  GST_DEBUG ("setting needed bytes to %" G_GSIZE_FORMAT ", have %"
      G_GSIZE_FORMAT, needed_bytes, size);
  gst_rtmp_connection_set_input_callback (sc,
      gst_rtmp_connection_chunk_callback, needed_bytes);
}

static void
gst_rtmp_connection_handle_chunk (GstRtmpConnection * sc, GstRtmpChunk * chunk)
{
  if (chunk->stream_id == 0x02) {
    GST_DEBUG ("got protocol control message, type: %d",
        chunk->message_type_id);
    gst_rtmp_connection_handle_pcm (sc, chunk);
    g_signal_emit_by_name (sc, "got-control-chunk", chunk);
  } else {
    if (chunk->message_type_id == 0x14) {
      CommandCallback *cb = NULL;
      GList *g;
      char *command_name;
      double transaction_id;
      GstAmfNode *command_object;
      GstAmfNode *optional_args;

      gst_rtmp_chunk_parse_message (chunk, &command_name, &transaction_id,
          &command_object, &optional_args);
      for (g = sc->command_callbacks; g; g = g_list_next (g)) {
        cb = g->data;
        if (cb->stream_id == chunk->stream_id &&
            cb->transaction_id == transaction_id) {
          break;
        }
      }
      if (cb) {
        sc->command_callbacks = g_list_remove (sc->command_callbacks, cb);
        cb->func (sc, chunk, command_name, transaction_id, command_object,
            optional_args, cb->user_data);
        g_free (command_name);
        gst_amf_node_free (command_object);
        if (optional_args)
          gst_amf_node_free (optional_args);
        g_free (cb);
      }
    }
    GST_DEBUG ("got chunk: %" G_GSIZE_FORMAT " bytes", chunk->message_length);
    g_signal_emit_by_name (sc, "got-chunk", chunk);
  }
}

static void
gst_rtmp_connection_handle_pcm (GstRtmpConnection * connection,
    GstRtmpChunk * chunk)
{
  const guint8 *data;
  gsize size;
  guint32 moo;
  guint32 moo2;
  data = g_bytes_get_data (chunk->payload, &size);
  switch (chunk->message_type_id) {
    case 0x01:
      moo = GST_READ_UINT32_BE (data);
      GST_INFO ("new chunk size %d", moo);
      connection->in_chunk_size = moo;
      break;
    case 0x02:
      moo = GST_READ_UINT32_BE (data);
      GST_ERROR ("unimplemented: chunk abort, stream_id = %d", moo);
      break;
    case 0x03:
      moo = GST_READ_UINT32_BE (data);
      /* We don't really send ack requests that we care about, so ignore */
      GST_ERROR ("acknowledgement %d", moo);
      break;
    case 0x04:
      moo = GST_READ_UINT16_BE (data);
      moo2 = GST_READ_UINT32_BE (data + 2);
      GST_INFO ("user control: %d, %d", moo, moo2);
      gst_rtmp_connection_handle_user_control (connection, moo, moo2);
      break;
    case 0x05:
      moo = GST_READ_UINT32_BE (data);
      GST_INFO ("window ack size: %d", moo);
      connection->window_ack_size = GST_READ_UINT32_BE (data);
      break;
    case 0x06:
      moo = GST_READ_UINT32_BE (data);
      moo2 = data[4];
      GST_DEBUG ("set peer bandwidth: %d, %d", moo, moo2);
      /* FIXME this is not correct, but close enough */
      if (connection->peer_bandwidth != moo) {
        connection->peer_bandwidth = moo;
        gst_rtmp_connection_send_window_size_request (connection);
      }
      break;
    default:
      GST_ERROR ("unimplemented");
      break;
  }
}

static void
gst_rtmp_connection_handle_user_control (GstRtmpConnection * connection,
    guint32 event_type, guint32 event_data)
{
  switch (event_type) {
    case 0x00:
      GST_DEBUG ("stream begin: %d", event_data);
      break;
    case 0x01:
      GST_ERROR ("stream EOF: %d", event_data);
      break;
    case 0x02:
      GST_ERROR ("stream dry: %d", event_data);
      break;
    case 0x03:
      GST_ERROR ("set buffer length: %d", event_data);
      break;
    case 0x04:
      GST_ERROR ("stream is recorded: %d", event_data);
      break;
    case 0x06:
      GST_DEBUG ("ping request: %d", event_data);
      gst_rtmp_connection_send_ping_response (connection, event_data);
      break;
    case 0x07:
      GST_ERROR ("ping response: %d", event_data);
      break;
    default:
      GST_ERROR ("unimplemented: %d, %d", event_type, event_data);
      break;
  }
}

void
gst_rtmp_connection_queue_chunk (GstRtmpConnection * connection,
    GstRtmpChunk * chunk)
{
  g_return_if_fail (GST_IS_RTMP_CONNECTION (connection));
  g_return_if_fail (GST_IS_RTMP_CHUNK (chunk));

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
    GST_DEBUG ("got %" G_GSIZE_FORMAT " bytes, calling callback",
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
  GST_INFO ("client handshake finished");
  sc->handshake_complete = TRUE;

  if (sc->input_bytes && g_bytes_get_size (sc->input_bytes) >= 1) {
    GST_DEBUG ("spare bytes after handshake: %" G_GSIZE_FORMAT,
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

int
gst_rtmp_connection_send_command (GstRtmpConnection * connection, int stream_id,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, GstRtmpCommandCallback response_command,
    gpointer user_data)
{
  GstRtmpChunk *chunk;

  chunk = gst_rtmp_chunk_new ();
  chunk->stream_id = stream_id;
  chunk->timestamp = 0;         /* FIXME */
  chunk->message_type_id = 0x14;
  chunk->info = 0;              /* FIXME */

  chunk->payload = gst_amf_serialize_command (command_name, transaction_id,
      command_object, optional_args);
  chunk->message_length = g_bytes_get_size (chunk->payload);

  gst_rtmp_connection_queue_chunk (connection, chunk);

  if (response_command) {
    CommandCallback *callback;

    callback = g_malloc0 (sizeof (CommandCallback));
    callback->stream_id = stream_id;
    callback->transaction_id = transaction_id;
    callback->func = response_command;
    callback->user_data = user_data;

    connection->command_callbacks =
        g_list_append (connection->command_callbacks, callback);
  }

  return transaction_id;
}

int
gst_rtmp_connection_send_command2 (GstRtmpConnection * connection,
    int chunk_stream_id, int stream_id,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, GstAmfNode * n3, GstAmfNode * n4,
    GstRtmpCommandCallback response_command, gpointer user_data)
{
  GstRtmpChunk *chunk;

  chunk = gst_rtmp_chunk_new ();
  chunk->stream_id = chunk_stream_id;
  chunk->timestamp = 0;         /* FIXME */
  chunk->message_type_id = 0x14;
  chunk->info = stream_id;

  chunk->payload = gst_amf_serialize_command2 (command_name, transaction_id,
      command_object, optional_args, n3, n4);
  chunk->message_length = g_bytes_get_size (chunk->payload);

  gst_rtmp_connection_queue_chunk (connection, chunk);

  if (response_command) {
    CommandCallback *callback;

    callback = g_malloc0 (sizeof (CommandCallback));
    callback->stream_id = stream_id;
    callback->transaction_id = transaction_id;
    callback->func = response_command;
    callback->user_data = user_data;

    connection->command_callbacks =
        g_list_append (connection->command_callbacks, callback);
  }

  return transaction_id;
}

static void
gst_rtmp_connection_send_ack (GstRtmpConnection * connection)
{
  GstRtmpChunk *chunk;
  guint8 *data;

  chunk = gst_rtmp_chunk_new ();
  chunk->stream_id = 2;
  chunk->timestamp = 0;
  chunk->message_type_id = 3;
  chunk->info = 0;

  data = g_malloc (4);
  GST_WRITE_UINT32_BE (data, connection->total_input_bytes);
  chunk->payload = g_bytes_new_take (data, 4);
  chunk->message_length = g_bytes_get_size (chunk->payload);

  gst_rtmp_connection_queue_chunk (connection, chunk);

  connection->bytes_since_ack = 0;
}

static void
gst_rtmp_connection_send_ping_response (GstRtmpConnection * connection,
    guint32 event_data)
{
  GstRtmpChunk *chunk;
  guint8 *data;

  chunk = gst_rtmp_chunk_new ();
  chunk->stream_id = 2;
  chunk->timestamp = 0;
  chunk->message_type_id = 4;
  chunk->info = 0;

  data = g_malloc (8);
  GST_WRITE_UINT32_BE (data, 7);
  GST_WRITE_UINT32_BE (data + 4, event_data);
  chunk->payload = g_bytes_new_take (data, 8);
  chunk->message_length = g_bytes_get_size (chunk->payload);

  gst_rtmp_connection_queue_chunk (connection, chunk);
}

static void
gst_rtmp_connection_send_window_size_request (GstRtmpConnection * connection)
{
  GstRtmpChunk *chunk;
  guint8 *data;

  chunk = gst_rtmp_chunk_new ();
  chunk->stream_id = 2;
  chunk->timestamp = 0;
  chunk->message_type_id = 5;
  chunk->info = 0;

  data = g_malloc (4);
  GST_WRITE_UINT32_BE (data, connection->peer_bandwidth);
  chunk->payload = g_bytes_new_take (data, 4);
  chunk->message_length = g_bytes_get_size (chunk->payload);

  gst_rtmp_connection_queue_chunk (connection, chunk);
}
