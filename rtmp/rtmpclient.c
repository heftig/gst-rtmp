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
gst_rtmp_client_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
#if 0
static void
gst_rtmp_client_handshake_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
#endif

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
  GSimpleAsyncResult *async;
  GSocketConnectable *addr;

  if (client->state != GST_RTMP_CLIENT_STATE_NEW) {
    g_simple_async_report_error_in_idle (G_OBJECT (client),
        callback, user_data, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "already connected");
    return;
  }

  async = g_simple_async_result_new (G_OBJECT (client),
      callback, user_data, gst_rtmp_client_connect_async);
  g_simple_async_result_set_check_cancellable (async, cancellable);

  client->cancellable = cancellable;
  client->async = async;

  addr =
      g_network_address_new
      ("ec2-54-188-128-44.us-west-2.compute.amazonaws.com", 1935);
  client->socket_client = g_socket_client_new ();

  GST_DEBUG ("g_socket_client_connect_async");
  g_socket_client_connect_async (client->socket_client, addr,
      client->cancellable, gst_rtmp_client_connect_done, client);
}

static void
gst_rtmp_client_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GstRtmpClient *client = GST_RTMP_CLIENT (user_data);
  GError *error = NULL;

  GST_DEBUG ("g_socket_client_connect_done");
  client->socket_connection =
      g_socket_client_connect_finish (client->socket_client, result, &error);
  if (client->socket_connection == NULL) {
    GST_ERROR ("error");
    g_simple_async_result_set_error (client->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    client->cancellable = NULL;
    g_simple_async_result_complete (client->async);
    return;
  }

  client->connection = gst_rtmp_connection_new (client->socket_connection);
  gst_rtmp_connection_start_handshake (client->connection, FALSE);

  g_simple_async_result_complete (client->async);
}

#if 0
G_GNUC_UNUSED static void
gst_rtmp_client_handshake_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GstRtmpClient *client = GST_RTMP_CLIENT (user_data);
  GError *error = NULL;
  gboolean ret;

  GST_DEBUG ("g_socket_client_connect_done");
  ret = gst_rtmp_connection_handshake_finish (client->connection,
      result, &error);
  if (!ret) {
    g_simple_async_result_set_error (client->async, GST_RTMP_ERROR,
        GST_RTMP_ERROR_TOO_LAZY, "%s", error->message);
    g_error_free (error);
    client->cancellable = NULL;
    g_simple_async_result_complete (client->async);
    return;
  }

  client->cancellable = NULL;
  g_simple_async_result_complete (client->async);
}
#endif

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

GstRtmpConnection *
gst_rtmp_client_get_connection (GstRtmpClient * client)
{
  return client->connection;
}
