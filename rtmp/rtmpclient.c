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
#include <string.h>
#include "rtmpclient.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_client_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_client_debug_category

/* prototypes */

static void gst_rtmp_client_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp_client_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp_client_dispose (GObject * object);
static void gst_rtmp_client_finalize (GObject * object);

static void
gst_rtmp_client_connect_start (GstRtmpClient * client,
    GCancellable * cancellable, GSimpleAsyncResult * async);
static void
gst_rtmp_client_connect_1 (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void
gst_rtmp_client_connect_2 (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void
gst_rtmp_client_connect_3 (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void
gst_rtmp_client_connect_4 (GObject * source, GAsyncResult * result,
    gpointer user_data);

enum
{
  PROP_0
};

/* pad templates */


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmpClient, gst_rtmp_client, G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_client_debug_category, "rtmpclient", 0,
        "debug category for GstRtmpClient class"));

static void
gst_rtmp_client_class_init (GstRtmpClientClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_rtmp_client_set_property;
  gobject_class->get_property = gst_rtmp_client_get_property;
  gobject_class->dispose = gst_rtmp_client_dispose;
  gobject_class->finalize = gst_rtmp_client_finalize;
}

static void
gst_rtmp_client_init (GstRtmpClient * rtmpclient)
{
}

void
gst_rtmp_client_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmpClient *rtmpclient = GST_RTMP_CLIENT (object);

  GST_DEBUG_OBJECT (rtmpclient, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_client_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmpClient *rtmpclient = GST_RTMP_CLIENT (object);

  GST_DEBUG_OBJECT (rtmpclient, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_client_dispose (GObject * object)
{
  GstRtmpClient *rtmpclient = GST_RTMP_CLIENT (object);

  GST_DEBUG_OBJECT (rtmpclient, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp_client_parent_class)->dispose (object);
}

void
gst_rtmp_client_finalize (GObject * object)
{
  GstRtmpClient *rtmpclient = GST_RTMP_CLIENT (object);

  GST_DEBUG_OBJECT (rtmpclient, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp_client_parent_class)->finalize (object);
}

/* API */

GstRtmpClient *
gst_rtmp_client_new (void)
{

  return g_object_new (GST_TYPE_RTMP_CLIENT, NULL);

}

void
gst_rtmp_client_set_host (GstRtmpClient * client, const char *host)
{
  g_free (client->host);
  client->host = g_strdup (host);
}

void
gst_rtmp_client_set_stream (GstRtmpClient * client, const char *stream)
{
  g_free (client->stream);
  client->stream = g_strdup (stream);
}

void
gst_rtmp_client_connect_async (GstRtmpClient * client,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;

  if (client->state != GST_RTMP_CLIENT_STATE_NEW) {
    g_simple_async_report_error_in_idle (G_OBJECT (client),
        callback, user_data, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "already connected");
    return;
  }

  simple = g_simple_async_result_new (G_OBJECT (client),
      callback, user_data, gst_rtmp_client_connect_async);

  gst_rtmp_client_connect_start (client, cancellable, simple);
}

gboolean
gst_rtmp_client_connect_finish (GstRtmpClient * client,
    GAsyncResult * result, GError ** error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (client), gst_rtmp_client_connect_async), FALSE);

  simple = (GSimpleAsyncResult *) result;

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}


/* async implementation */

/* connect */

typedef struct _ConnectContext ConnectContext;
struct _ConnectContext
{
  GstRtmpClient *client;
  GSimpleAsyncResult *async;
  GCancellable *cancellable;

  GSocketClient *socket_client;
  GSocketConnection *connection;
  guint8 *handshake_send;
  guint8 *handshake_recv;
};

static void
gst_rtmp_client_connect_complete (ConnectContext * context)
{
  GSimpleAsyncResult *async = context->async;

  if (context->socket_client)
    g_object_unref (context->socket_client);
  if (context->connection)
    g_object_unref (context->connection);
  g_free (context->handshake_send);
  g_free (context->handshake_recv);
  g_free (context);
  g_simple_async_result_complete (async);
}

static void
gst_rtmp_client_connect_start (GstRtmpClient * client,
    GCancellable * cancellable, GSimpleAsyncResult * async)
{
  ConnectContext *context;
  GSocketConnectable *addr;

  context = g_malloc0 (sizeof (ConnectContext));
  context->cancellable = cancellable;
  context->client = client;
  context->async = async;

  addr = g_network_address_new ("localhost", 1935);
  context->socket_client = g_socket_client_new ();

  GST_ERROR ("g_socket_client_connect_async");
  g_socket_client_connect_async (context->socket_client, addr,
      context->cancellable, gst_rtmp_client_connect_1, context);
}


static void
gst_rtmp_client_connect_1 (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  ConnectContext *context = user_data;
  GError *error = NULL;
  GOutputStream *os;

  GST_ERROR ("g_socket_client_connect_finish");
  context->connection = g_socket_client_connect_finish (context->socket_client,
      result, &error);
  if (context->connection == NULL) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (context->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    gst_rtmp_client_connect_complete (context);
    return;
  }

  context->handshake_send = g_malloc (1537);
  context->handshake_send[0] = 3;
  memset (context->handshake_send + 1, 0, 8);
  memset (context->handshake_send + 9, 0xa5, 1528);

  os = g_io_stream_get_output_stream (G_IO_STREAM (context->connection));

  GST_ERROR ("g_output_stream_write_async");
  g_output_stream_write_async (os, context->handshake_send, 1537,
      G_PRIORITY_DEFAULT, context->cancellable, gst_rtmp_client_connect_2,
      context);
}

static void
gst_rtmp_client_connect_2 (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  ConnectContext *context = user_data;
  GOutputStream *os;
  GInputStream *is;
  GError *error = NULL;
  gssize n;

  GST_ERROR ("g_output_stream_write_finish");
  os = g_io_stream_get_output_stream (G_IO_STREAM (context->connection));
  n = g_output_stream_write_finish (os, result, &error);
  GST_ERROR ("wrote %" G_GSIZE_FORMAT " bytes", n);
  if (error) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (context->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    gst_rtmp_client_connect_complete (context);
    return;
  }

  context->handshake_recv = g_malloc (1537);

  GST_ERROR ("g_input_stream_read_async");
  is = g_io_stream_get_input_stream (G_IO_STREAM (context->connection));
  g_input_stream_read_async (is, context->handshake_recv, 1537,
      G_PRIORITY_DEFAULT, context->cancellable, gst_rtmp_client_connect_3,
      context);
}

static void
gst_rtmp_client_connect_3 (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  ConnectContext *context = user_data;
  GInputStream *is;
  GError *error = NULL;
  gssize n;

  GST_ERROR ("g_input_stream_read_finish");
  is = g_io_stream_get_input_stream (G_IO_STREAM (context->connection));
  n = g_input_stream_read_finish (is, result, &error);
  GST_ERROR ("read %" G_GSIZE_FORMAT " bytes", n);
  if (error) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (context->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    gst_rtmp_client_connect_complete (context);
    return;
  }
#if 0
  GST_ERROR ("recv: %02x %02x %02x %02x %02x %02x %02x %02x %02x",
      context->handshake_recv[0], context->handshake_recv[1],
      context->handshake_recv[2], context->handshake_recv[3],
      context->handshake_recv[4], context->handshake_recv[5],
      context->handshake_recv[6], context->handshake_recv[7],
      context->handshake_recv[8]);
#endif

  n = g_output_stream_write (g_io_stream_get_output_stream (G_IO_STREAM
          (context->connection)), context->handshake_recv + 1, 1536,
      context->cancellable, &error);
  if (n < 1536) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (context->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    gst_rtmp_client_connect_complete (context);
    return;
  }

  GST_ERROR ("g_input_stream_read_async");
  is = g_io_stream_get_input_stream (G_IO_STREAM (context->connection));
  g_input_stream_read_async (is, context->handshake_recv + 1, 1536,
      G_PRIORITY_DEFAULT, context->cancellable, gst_rtmp_client_connect_4,
      context);

}

static void
gst_rtmp_client_connect_4 (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  ConnectContext *context = user_data;
  GInputStream *is;
  GError *error = NULL;
  gssize n;

  GST_ERROR ("g_input_stream_read_finish");
  is = g_io_stream_get_input_stream (G_IO_STREAM (context->connection));
  n = g_input_stream_read_finish (is, result, &error);
  GST_ERROR ("read %" G_GSIZE_FORMAT " bytes", n);
  if (error) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (context->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    gst_rtmp_client_connect_complete (context);
    return;
  }

  context->client->socket_client = context->socket_client;
  context->socket_client = NULL;
  context->client->connection = context->connection;
  context->connection = NULL;
  context->client->state = GST_RTMP_CLIENT_STATE_CONNECTED;

  GST_ERROR ("got here");
  gst_rtmp_client_connect_complete (context);
}
