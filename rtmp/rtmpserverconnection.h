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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_RTMP_SERVER_CONNECTION_H_
#define _GST_RTMP_SERVER_CONNECTION_H_

#include <gio/gio.h>
#include <rtmp/rtmpchunk.h>

G_BEGIN_DECLS

#define GST_TYPE_RTMP_SERVER_CONNECTION   (gst_rtmp_server_connection_get_type())
#define GST_RTMP_SERVER_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_SERVER_CONNECTION,GstRtmpServerConnection))
#define GST_RTMP_SERVER_CONNECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_SERVER_CONNECTION,GstRtmpServerConnectionClass))
#define GST_IS_RTMP_SERVER_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_SERVER_CONNECTION))
#define GST_IS_RTMP_SERVER_CONNECTION_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_SERVER_CONNECTION))

typedef struct _GstRtmpServerConnection GstRtmpServerConnection;
typedef struct _GstRtmpServerConnectionClass GstRtmpServerConnectionClass;

struct _GstRtmpServerConnection
{
  GObject object;

  /* private */
  GSocketConnection *connection;
  GSocketConnection *proxy_connection;
  GCancellable *cancellable;
  int state;
  GSocketClient *socket_client;
};

struct _GstRtmpServerConnectionClass
{
  GObjectClass object_class;

  /* signals */
  void (*got_chunk) (GstRtmpServerConnection *connection,
      GstRtmpChunk *chunk);
};

GType gst_rtmp_server_connection_get_type (void);

GstRtmpServerConnection *gst_rtmp_server_connection_new (
    GSocketConnection *connection);

G_END_DECLS

#endif
