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

#ifndef _GST_RTMP_CHUNK_H_
#define _GST_RTMP_CHUNK_H_

#include <glib.h>

G_BEGIN_DECLS

#define GST_TYPE_RTMP_CHUNK   (gst_rtmp_chunk_get_type())
#define GST_RTMP_CHUNK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_CHUNK,GstRtmpChunk))
#define GST_RTMP_CHUNK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_CHUNK,GstRtmpChunkClass))
#define GST_IS_RTMP_CHUNK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_CHUNK))
#define GST_IS_RTMP_CHUNK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_CHUNK))

typedef struct _GstRtmpChunk GstRtmpChunk;
typedef struct _GstRtmpChunkClass GstRtmpChunkClass;

struct _GstRtmpChunk
{
  GObject object;

  guint32 stream_id;
  guint32 timestamp;
  gsize message_length;
  int message_type_id;

  GBytes *payload;
};

struct _GstRtmpChunkClass
{
  GObjectClass object_class;
};

typedef enum {
  GST_RTMP_CHUNK_PARSE_ERROR = 0,
  GST_RTMP_CHUNK_PARSE_OK,
  GST_RTMP_CHUNK_PARSE_UNKNOWN,
  GST_RTMP_CHUNK_PARSE_NEED_BYTES,
} GstRtmpChunkParseStatus;

GType gst_rtmp_chunk_get_type (void);

GstRtmpChunk *gst_rtmp_chunk_new (void);
GstRtmpChunkParseStatus gst_rtmp_chunk_can_parse (GBytes *bytes,
    gsize *chunk_size);
GstRtmpChunk * gst_rtmp_chunk_new_parse (GBytes *bytes, gsize *chunk_size);

void gst_rtmp_chunk_set_stream_id (GstRtmpChunk *chunk, guint32 stream_id);
void gst_rtmp_chunk_set_timestamp (GstRtmpChunk *chunk, guint32 timestamp);
void gst_rtmp_chunk_set_payload (GstRtmpChunk *chunk, GBytes *payload);

guint32 gst_rtmp_chunk_get_stream_id (GstRtmpChunk *chunk);
guint32 gst_rtmp_chunk_get_timestamp (GstRtmpChunk *chunk);
GBytes * gst_rtmp_chunk_get_payload (GstRtmpChunk *chunk);

G_END_DECLS

#endif