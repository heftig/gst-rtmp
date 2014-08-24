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
#include "rtmpserverconnection.h"
#include "rtmpchunk.h"
#include "amf.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_server_connection_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_server_connection_debug_category

/* prototypes */

static void gst_rtmp_server_connection_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp_server_connection_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp_server_connection_dispose (GObject * object);
static void gst_rtmp_server_connection_finalize (GObject * object);

static void proxy_connect (GstRtmpServerConnection * sc);
static void gst_rtmp_server_connection_handshake1 (GstRtmpServerConnection *
    sc);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmpServerConnection, gst_rtmp_server_connection,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_server_connection_debug_category,
        "rtmpserverconnection", 0,
        "debug category for GstRtmpServerConnection class"));

static void
gst_rtmp_server_connection_class_init (GstRtmpServerConnectionClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_rtmp_server_connection_set_property;
  gobject_class->get_property = gst_rtmp_server_connection_get_property;
  gobject_class->dispose = gst_rtmp_server_connection_dispose;
  gobject_class->finalize = gst_rtmp_server_connection_finalize;

  g_signal_new ("got-chunk", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstRtmpServerConnectionClass,
          got_chunk), NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 1, GST_TYPE_RTMP_CHUNK);
}

static void
gst_rtmp_server_connection_init (GstRtmpServerConnection * rtmpserverconnection)
{
  rtmpserverconnection->cancellable = g_cancellable_new ();
}

void
gst_rtmp_server_connection_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmpServerConnection *rtmpserverconnection =
      GST_RTMP_SERVER_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpserverconnection, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_server_connection_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmpServerConnection *rtmpserverconnection =
      GST_RTMP_SERVER_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpserverconnection, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_server_connection_dispose (GObject * object)
{
  GstRtmpServerConnection *rtmpserverconnection =
      GST_RTMP_SERVER_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpserverconnection, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp_server_connection_parent_class)->dispose (object);
}

void
gst_rtmp_server_connection_finalize (GObject * object)
{
  GstRtmpServerConnection *rtmpserverconnection =
      GST_RTMP_SERVER_CONNECTION (object);

  GST_DEBUG_OBJECT (rtmpserverconnection, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp_server_connection_parent_class)->finalize (object);
}

GstRtmpServerConnection *
gst_rtmp_server_connection_new (GSocketConnection * connection)
{
  GstRtmpServerConnection *sc;

  sc = g_object_new (GST_TYPE_RTMP_SERVER_CONNECTION, NULL);
  sc->connection = connection;

  //proxy_connect (sc);
  //gst_rtmp_server_connection_read_chunk (sc);
  gst_rtmp_server_connection_handshake1 (sc);

  return sc;
}

typedef struct _ChunkRead ChunkRead;
struct _ChunkRead
{
  gpointer data;
  gsize alloc_size;
  gsize size;
  GstRtmpServerConnection *server_connection;
};


static void proxy_connect (GstRtmpServerConnection * sc);
static void proxy_connect_done (GObject * obj, GAsyncResult * res,
    gpointer user_data);
static void gst_rtmp_server_connection_read_chunk (GstRtmpServerConnection *
    sc);
static void gst_rtmp_server_connection_read_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void proxy_write_chunk (GstRtmpServerConnection * sc, ChunkRead * chunk);
static void proxy_write_done (GObject * obj, GAsyncResult * res,
    gpointer user_data);
static void proxy_read_chunk (GstRtmpServerConnection * sc);
static void proxy_read_done (GObject * obj, GAsyncResult * res,
    gpointer user_data);
static void gst_rtmp_server_connection_write_chunk (GstRtmpServerConnection *
    sc, ChunkRead * chunk);
static void gst_rtmp_server_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_server_connection_handshake1 (GstRtmpServerConnection *
    sc);
static void gst_rtmp_server_connection_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_server_connection_handshake2 (GstRtmpServerConnection * sc,
    GBytes * bytes);
static void gst_rtmp_server_connection_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);
static void gst_rtmp_server_connection_handshake3 (GstRtmpServerConnection *
    sc);
static void gst_rtmp_server_connection_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data);


static void
proxy_connect (GstRtmpServerConnection * sc)
{
  GSocketConnectable *addr;

  GST_ERROR ("proxy_connect");

  addr = g_network_address_new ("localhost", 1935);

  sc->socket_client = g_socket_client_new ();
  g_socket_client_connect_async (sc->socket_client, addr,
      sc->cancellable, proxy_connect_done, sc);
}

static void
proxy_connect_done (GObject * obj, GAsyncResult * res, gpointer user_data)
{
  GstRtmpServerConnection *sc = GST_RTMP_SERVER_CONNECTION (user_data);
  GError *error = NULL;

  GST_ERROR ("proxy_connect_done");

  sc->proxy_connection = g_socket_client_connect_finish (sc->socket_client,
      res, &error);
  if (sc->proxy_connection == NULL) {
    GST_ERROR ("connection error: %s", error->message);
    g_error_free (error);
    return;
  }

  gst_rtmp_server_connection_read_chunk (sc);
}

static void
gst_rtmp_server_connection_read_chunk (GstRtmpServerConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 4096,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_server_connection_read_chunk_done, sc);
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
gst_rtmp_server_connection_read_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpServerConnection *server_connection =
      GST_RTMP_SERVER_CONNECTION (user_data);
  GstRtmpChunk *chunk;
  GError *error = NULL;
  gsize chunk_size;
  GBytes *bytes;

  GST_ERROR ("gst_rtmp_server_connection_read_chunk_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  chunk = gst_rtmp_chunk_new_parse (bytes, &chunk_size);

  if (chunk) {
    g_signal_emit_by_name (server_connection, "got-chunk", chunk);

    g_object_unref (chunk);
  }
}

G_GNUC_UNUSED static void
proxy_write_chunk (GstRtmpServerConnection * sc, ChunkRead * chunk)
{
  GOutputStream *os;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->proxy_connection));

  g_output_stream_write_async (os, chunk->data, chunk->size,
      G_PRIORITY_DEFAULT, sc->cancellable, proxy_write_done, chunk);
}

static void
proxy_write_done (GObject * obj, GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  ChunkRead *chunk = (ChunkRead *) user_data;
  GError *error = NULL;
  gssize ret;

  GST_ERROR ("proxy_write_done");

  ret = g_output_stream_write_finish (os, res, &error);
  if (ret < 0) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("proxy write %" G_GSSIZE_FORMAT " bytes", ret);

  proxy_read_chunk (chunk->server_connection);

  g_free (chunk->data);
  g_free (chunk);
}

static void
proxy_read_chunk (GstRtmpServerConnection * sc)
{
  GInputStream *is;
  ChunkRead *chunk;

  chunk = g_malloc0 (sizeof (ChunkRead));
  chunk->alloc_size = 4096;
  chunk->data = g_malloc (chunk->alloc_size);
  chunk->server_connection = sc;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->proxy_connection));

  g_input_stream_read_async (is, chunk->data, chunk->alloc_size,
      G_PRIORITY_DEFAULT, sc->cancellable, proxy_read_done, chunk);
}

static void
proxy_read_done (GObject * obj, GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  ChunkRead *chunk = (ChunkRead *) user_data;
  GError *error = NULL;
  gssize ret;

  GST_ERROR ("proxy_read_done");

  ret = g_input_stream_read_finish (is, res, &error);
  if (ret < 0) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("proxy read %" G_GSSIZE_FORMAT " bytes", ret);
  chunk->size = ret;

  gst_rtmp_server_connection_write_chunk (chunk->server_connection, chunk);
}

static void
gst_rtmp_server_connection_write_chunk (GstRtmpServerConnection * sc,
    ChunkRead * chunk)
{
  GOutputStream *os;

  os = g_io_stream_get_output_stream (G_IO_STREAM (sc->connection));

  g_output_stream_write_async (os, chunk->data, chunk->size,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_server_connection_write_chunk_done, chunk);
}

static void
gst_rtmp_server_connection_write_chunk_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  ChunkRead *chunk = (ChunkRead *) user_data;
  GError *error = NULL;
  gssize ret;

  GST_ERROR ("gst_rtmp_server_connection_write_chunk_done");

  ret = g_output_stream_write_finish (os, res, &error);
  if (ret < 0) {
    GST_ERROR ("write error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("write %" G_GSSIZE_FORMAT " bytes", ret);

  gst_rtmp_server_connection_read_chunk (chunk->server_connection);

  g_free (chunk->data);
  g_free (chunk);
}

static void
gst_rtmp_server_connection_handshake1 (GstRtmpServerConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 4096,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_server_connection_handshake1_done, sc);
}

static void
gst_rtmp_server_connection_handshake1_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpServerConnection *sc = GST_RTMP_SERVER_CONNECTION (user_data);
  GError *error = NULL;
  GBytes *bytes;

  GST_ERROR ("gst_rtmp_server_connection_handshake1_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_server_connection_handshake2 (sc, bytes);
}

static void
gst_rtmp_server_connection_handshake2 (GstRtmpServerConnection * sc,
    GBytes * bytes)
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
      gst_rtmp_server_connection_handshake2_done, sc);
}

static void
gst_rtmp_server_connection_handshake2_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GOutputStream *os = G_OUTPUT_STREAM (obj);
  GstRtmpServerConnection *sc = GST_RTMP_SERVER_CONNECTION (user_data);
  GError *error = NULL;
  gssize ret;

  GST_ERROR ("gst_rtmp_server_connection_handshake2_done");

  ret = g_output_stream_write_bytes_finish (os, res, &error);
  if (ret < 1 + 1536 + 1536) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("wrote %" G_GSSIZE_FORMAT " bytes", ret);

  gst_rtmp_server_connection_handshake3 (sc);
}

static void
gst_rtmp_server_connection_handshake3 (GstRtmpServerConnection * sc)
{
  GInputStream *is;

  is = g_io_stream_get_input_stream (G_IO_STREAM (sc->connection));

  g_input_stream_read_bytes_async (is, 1536,
      G_PRIORITY_DEFAULT, sc->cancellable,
      gst_rtmp_server_connection_handshake3_done, sc);
}

static void
gst_rtmp_server_connection_handshake3_done (GObject * obj,
    GAsyncResult * res, gpointer user_data)
{
  GInputStream *is = G_INPUT_STREAM (obj);
  GstRtmpServerConnection *sc = GST_RTMP_SERVER_CONNECTION (user_data);
  GError *error = NULL;
  GBytes *bytes;

  GST_ERROR ("gst_rtmp_server_connection_handshake3_done");

  bytes = g_input_stream_read_bytes_finish (is, res, &error);
  if (bytes == NULL) {
    GST_ERROR ("read error: %s", error->message);
    g_error_free (error);
    return;
  }
  GST_ERROR ("read %" G_GSSIZE_FORMAT " bytes", g_bytes_get_size (bytes));

  gst_rtmp_server_connection_read_chunk (sc);
  (void) &proxy_connect;
}
