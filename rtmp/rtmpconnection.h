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

#ifndef _GST_RTMP_CONNECTION_H_
#define _GST_RTMP_CONNECTION_H_

#include <gio/gio.h>
#include <rtmp/rtmpchunk.h>
#include <rtmp/amf.h>

G_BEGIN_DECLS

#define GST_TYPE_RTMP_CONNECTION   (gst_rtmp_connection_get_type())
#define GST_RTMP_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_CONNECTION,GstRtmpConnection))
#define GST_RTMP_CONNECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_CONNECTION,GstRtmpConnectionClass))
#define GST_IS_RTMP_CONNECTION(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_CONNECTION))
#define GST_IS_RTMP_CONNECTION_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_CONNECTION))

typedef struct _GstRtmpConnection GstRtmpConnection;
typedef struct _GstRtmpConnectionClass GstRtmpConnectionClass;
typedef void (*GstRtmpConnectionCallback) (GstRtmpConnection *connection);

struct _GstRtmpConnection
{
  GObject object;

  /* should be properties */
  gboolean input_paused;

  /* private */
  GSocketConnection *connection;
  GSocketConnection *proxy_connection;
  GCancellable *cancellable;
  int state;
  GSocketClient *socket_client;
  GQueue *output_queue;
  GSimpleAsyncResult *async;
  gboolean writing;

  GSource *input_source;
  GBytes *input_bytes;
  gsize input_needed_bytes;
  GstRtmpConnectionCallback input_callback;
  gboolean handshake_complete;
  GstRtmpChunkCache *input_chunk_cache;
  GstRtmpChunkCache *output_chunk_cache;

  /* chunk currently being written */
  GstRtmpChunk *output_chunk;
  GBytes *output_bytes;

  /* RTMP configuration */
  gsize in_chunk_size;
  gsize out_chunk_size;
};

struct _GstRtmpConnectionClass
{
  GObjectClass object_class;

  /* signals */
  void (*got_chunk) (GstRtmpConnection *connection,
      GstRtmpChunk *chunk);
  void (*got_command) (GstRtmpConnection *connection,
      const char *command_name, int transaction_id,
      GstAmfNode *command_object, GstAmfNode *option_args);
};

GType gst_rtmp_connection_get_type (void);

GstRtmpConnection *gst_rtmp_connection_new (GSocketConnection *connection);

void gst_rtmp_connection_start_handshake (GstRtmpConnection *connection,
    gboolean is_server);
void gst_rtmp_connection_queue_chunk (GstRtmpConnection *connection,
    GstRtmpChunk *chunk);
void gst_rtmp_connection_dump (GstRtmpConnection *connection);

int gst_rtmp_connection_send_command (GstRtmpConnection *connection,
    int stream_id, const char *command_name, int transaction_id,
    GstAmfNode *command_object, GstAmfNode *optional_args);


G_END_DECLS

#endif
