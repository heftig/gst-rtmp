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
#include "rtmpchunk.h"
#include "rtmputils.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_chunk_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_chunk_debug_category

/* prototypes */


static void gst_rtmp_chunk_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp_chunk_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp_chunk_dispose (GObject * object);
static void gst_rtmp_chunk_finalize (GObject * object);


enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmpChunk, gst_rtmp_chunk, G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_chunk_debug_category, "rtmpchunk", 0,
        "debug category for rtmpchunk element"));

static void
gst_rtmp_chunk_class_init (GstRtmpChunkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_rtmp_chunk_set_property;
  gobject_class->get_property = gst_rtmp_chunk_get_property;
  gobject_class->dispose = gst_rtmp_chunk_dispose;
  gobject_class->finalize = gst_rtmp_chunk_finalize;

}

static void
gst_rtmp_chunk_init (GstRtmpChunk * rtmpchunk)
{
}

void
gst_rtmp_chunk_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmpChunk *rtmpchunk = GST_RTMP_CHUNK (object);

  GST_DEBUG_OBJECT (rtmpchunk, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_chunk_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmpChunk *rtmpchunk = GST_RTMP_CHUNK (object);

  GST_DEBUG_OBJECT (rtmpchunk, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp_chunk_dispose (GObject * object)
{
  GstRtmpChunk *rtmpchunk = GST_RTMP_CHUNK (object);

  GST_DEBUG_OBJECT (rtmpchunk, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp_chunk_parent_class)->dispose (object);
}

void
gst_rtmp_chunk_finalize (GObject * object)
{
  GstRtmpChunk *rtmpchunk = GST_RTMP_CHUNK (object);

  GST_DEBUG_OBJECT (rtmpchunk, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp_chunk_parent_class)->finalize (object);
}

GstRtmpChunk *
gst_rtmp_chunk_new (void)
{
  return g_object_new (GST_TYPE_RTMP_CHUNK, NULL);
}

static GstRtmpChunkParseStatus
chunk_parse (GstRtmpChunk * chunk, GBytes * bytes, gsize * needed_bytes)
{
  int offset;
  const guint8 *data;
  gsize size;
  int header_fmt;

  if (*needed_bytes)
    *needed_bytes = 0;

  data = g_bytes_get_data (bytes, &size);
  if (size < 1)
    return GST_RTMP_CHUNK_PARSE_UNKNOWN;

  header_fmt = data[0] >> 6;    /* librtmp: m_headerType */
  chunk->stream_id = data[0] & 0x3f;    /* librtmp: m_nChannel */
  offset = 1;
  if (chunk->stream_id == 0) {
    if (size < 2)
      return GST_RTMP_CHUNK_PARSE_UNKNOWN;
    chunk->stream_id = 64 + data[1];
    offset = 2;
  } else if (chunk->stream_id == 1) {
    if (size < 3)
      return GST_RTMP_CHUNK_PARSE_UNKNOWN;
    chunk->stream_id = 64 + data[1] + (data[2] << 8);
    offset = 3;
  }
  if (header_fmt == 0) {
    if (size < offset + 11)
      return GST_RTMP_CHUNK_PARSE_UNKNOWN;
    /* librtmp: m_nTimeStamp */
    chunk->timestamp =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    /* librtmp: m_nBodySize */
    chunk->message_length =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    if (chunk->timestamp == 0xffffff) {
      if (size < offset + 4 + 1 + 4)
        return GST_RTMP_CHUNK_PARSE_UNKNOWN;
      chunk->timestamp = (data[offset] << 24) | (data[offset + 1] << 16) |
          (data[offset + 2] << 8) | data[offset + 3];
      offset += 4;
    }
    chunk->message_type_id = data[offset];      /* librtmp: m_packetType */
    offset += 1;

    /* librtmp: m_nInfoField2 */
    chunk->info = (data[offset] << 24) | (data[offset + 1] << 16) |
        (data[offset + 2] << 8) | data[offset + 3];
    offset += 4;
  } else if (header_fmt == 1) {
    if (size < offset + 7)
      return GST_RTMP_CHUNK_PARSE_UNKNOWN;
    GST_ERROR ("unimplemented: need previous chunk");
    chunk->timestamp =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    chunk->message_length =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
  } else if (header_fmt == 2) {
    if (size < offset + 3)
      return GST_RTMP_CHUNK_PARSE_UNKNOWN;
    chunk->timestamp = 0;
    GST_ERROR ("unimplemented: need previous chunk");
    return GST_RTMP_CHUNK_PARSE_ERROR;
  } else {
    chunk->timestamp = 0;
    GST_ERROR ("unimplemented: need previous chunk");
    return GST_RTMP_CHUNK_PARSE_ERROR;
  }

  if (needed_bytes)
    *needed_bytes = offset + chunk->message_length;
  if (size < offset + chunk->message_length)
    return GST_RTMP_CHUNK_PARSE_NEED_BYTES;

#if 0
  {
    GBytes *b;
    GST_ERROR ("PARSED CHUNK:");
    b = g_bytes_new_from_bytes (bytes, 0, offset + chunk->message_length);
    gst_rtmp_dump_data (b);
    g_bytes_unref (b);
  }
#endif

  chunk->payload =
      g_bytes_new_from_bytes (bytes, offset, chunk->message_length);
  return GST_RTMP_CHUNK_PARSE_OK;
}

GstRtmpChunkParseStatus
gst_rtmp_chunk_can_parse (GBytes * bytes, gsize * chunk_size)
{
  GstRtmpChunk *chunk;
  GstRtmpChunkParseStatus status;

  chunk = gst_rtmp_chunk_new ();
  status = chunk_parse (chunk, bytes, chunk_size);
  g_object_unref (chunk);

  return status;
}

GstRtmpChunk *
gst_rtmp_chunk_new_parse (GBytes * bytes, gsize * chunk_size)
{
  GstRtmpChunk *chunk;
  GstRtmpChunkParseStatus status;

  chunk = gst_rtmp_chunk_new ();
  status = chunk_parse (chunk, bytes, chunk_size);
  if (status == GST_RTMP_CHUNK_PARSE_OK)
    return chunk;

  g_object_unref (chunk);
  return NULL;
}

GBytes *
gst_rtmp_chunk_serialize (GstRtmpChunk * chunk)
{
  guint8 *data;
  const guint8 *chunkdata;
  gsize chunksize;
  int header_fmt;

  /* FIXME this is incomplete and inefficient */
  chunkdata = g_bytes_get_data (chunk->payload, &chunksize);
  data = g_malloc (chunksize + 12);
  header_fmt = 0;
  g_assert (chunk->stream_id < 64);
  data[0] = (header_fmt << 6) | (chunk->stream_id);
  g_assert (chunk->timestamp < 0xffffff);
  data[1] = (chunk->timestamp >> 16) & 0xff;
  data[2] = (chunk->timestamp >> 8) & 0xff;
  data[3] = chunk->timestamp & 0xff;
  data[4] = (chunk->message_length >> 16) & 0xff;
  data[5] = (chunk->message_length >> 8) & 0xff;
  data[6] = chunk->message_length & 0xff;
  data[7] = chunk->message_type_id;
  data[8] = 0;
  data[9] = 0;
  data[10] = 0;
  data[11] = 0;
  memcpy (data + 12, chunkdata, chunksize);

  return g_bytes_new_take (data, chunksize + 12);
}

void
gst_rtmp_chunk_set_stream_id (GstRtmpChunk * chunk, guint32 stream_id)
{
  chunk->stream_id = stream_id;
}

void
gst_rtmp_chunk_set_timestamp (GstRtmpChunk * chunk, guint32 timestamp)
{
  chunk->timestamp = timestamp;
}

void
gst_rtmp_chunk_set_payload (GstRtmpChunk * chunk, GBytes * payload)
{
  if (chunk->payload) {
    g_bytes_unref (chunk->payload);
  }
  chunk->payload = payload;
}

guint32
gst_rtmp_chunk_get_stream_id (GstRtmpChunk * chunk)
{
  return chunk->stream_id;
}

guint32
gst_rtmp_chunk_get_timestamp (GstRtmpChunk * chunk)
{
  return chunk->timestamp;
}

GBytes *
gst_rtmp_chunk_get_payload (GstRtmpChunk * chunk)
{
  return chunk->payload;
}
