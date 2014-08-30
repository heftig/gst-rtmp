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

gboolean
gst_rtmp_chunk_parse_header1 (GstRtmpChunkHeader * header, GBytes * bytes)
{
  const guint8 *data;
  const gsize sizes[4] = { 12, 8, 4, 1 };
  int stream_id;
  gsize size;

  data = g_bytes_get_data (bytes, &size);
  header->format = data[0] >> 6;
  header->header_size = sizes[header->format];

  stream_id = data[0] & 0x3f;
  if (stream_id == 0) {
    if (size >= 2)
      header->stream_id = 64 + data[1];
    header->header_size += 1;
  } else if (stream_id == 1) {
    if (size >= 3)
      header->stream_id = 64 + data[1] + (data[2] << 8);
    header->header_size += 2;
  } else {
    header->stream_id = stream_id;
  }

  return (header->header_size <= size);
}

gboolean
gst_rtmp_chunk_parse_header2 (GstRtmpChunkHeader * header, GBytes * bytes,
    GstRtmpChunkHeader * previous_header)
{
  int offset;
  const guint8 *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  header->format = data[0] >> 6;
  header->stream_id = data[0] & 0x3f;
  offset = 1;
  if (header->stream_id == 0) {
    header->stream_id = 64 + data[1];
    offset = 2;
  } else if (header->stream_id == 1) {
    header->stream_id = 64 + data[1] + (data[2] << 8);
    offset = 3;
  }
  if (header->format == 0) {
    header->timestamp =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    header->message_length =
        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    if (header->timestamp == 0xffffff) {
      GST_ERROR ("unimplemented");
      g_assert_not_reached ();
      header->timestamp = (data[offset] << 24) | (data[offset + 1] << 16) |
          (data[offset + 2] << 8) | data[offset + 3];
      offset += 4;
    }
    header->message_type_id = data[offset];
    offset += 1;

    header->info = (data[offset + 3] << 24) | (data[offset + 2] << 16) |
        (data[offset + 1] << 8) | data[offset];
    offset += 4;
  } else {
    header->timestamp = previous_header->timestamp;
    header->message_length = previous_header->message_length;
    header->message_type_id = previous_header->message_type_id;
    header->info = previous_header->info;

    if (header->format == 1) {
      header->timestamp +=
          (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
      offset += 3;
      header->message_length =
          (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
      offset += 3;
      header->message_type_id = data[offset];
      offset += 1;
    } else if (header->format == 2) {
      header->timestamp +=
          (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
      offset += 3;
    } else {
      /* ok */
    }
  }

  header->header_size = offset;

  return (header->header_size <= size);
}

GBytes *
gst_rtmp_chunk_serialize (GstRtmpChunk * chunk,
    GstRtmpChunkHeader * previous_header, gsize max_chunk_size)
{
  guint8 *data;
  const guint8 *chunkdata;
  gsize chunksize;
  int header_fmt;
  guint32 timestamp;
  int offset;
  int i;

  /* FIXME this is incomplete and inefficient */
  chunkdata = g_bytes_get_data (chunk->payload, &chunksize);
  g_assert (chunk->message_length == chunksize);
  g_assert (chunk->stream_id < 64);
  data = g_malloc (chunksize + 12 + (chunksize / max_chunk_size));

  header_fmt = 0;
#if 0
  if (previous_header->message_length > 0) {
    header_fmt = 1;
    timestamp = chunk->timestamp - previous_header->timestamp;
  }
#endif

  g_assert (chunk->stream_id < 64);
  data[0] = (header_fmt << 6) | (chunk->stream_id);
  if (header_fmt == 0) {
    g_assert (chunk->timestamp < 0xffffff);
    data[1] = (chunk->timestamp >> 16) & 0xff;
    data[2] = (chunk->timestamp >> 8) & 0xff;
    data[3] = chunk->timestamp & 0xff;
    data[4] = (chunk->message_length >> 16) & 0xff;
    data[5] = (chunk->message_length >> 8) & 0xff;
    data[6] = chunk->message_length & 0xff;
    data[7] = chunk->message_type_id;
    data[8] = chunk->info & 0xff;
    data[9] = (chunk->info >> 8) & 0xff;
    data[10] = (chunk->info >> 16) & 0xff;
    data[11] = (chunk->info >> 24) & 0xff;
    offset = 12;
  } else {
    data[1] = (timestamp >> 16) & 0xff;
    data[2] = (timestamp >> 8) & 0xff;
    data[3] = timestamp & 0xff;
    data[4] = (chunk->message_length >> 16) & 0xff;
    data[5] = (chunk->message_length >> 8) & 0xff;
    data[6] = chunk->message_length & 0xff;
    data[7] = chunk->message_type_id;
    offset = 8;
  }
  if (chunk->message_type_id == 0x12 || chunk->message_type_id == 0x09 ||
      chunk->message_type_id == 0x14) {
    for (i = 0; i < chunksize; i += max_chunk_size) {
      if (i != 0) {
        data[offset] = 0xc0 | chunk->stream_id;
        offset++;
      }
      memcpy (data + offset, chunkdata + i, MIN (chunksize - i,
              max_chunk_size));
      offset += MIN (chunksize - i, max_chunk_size);
    }
  } else {
    memcpy (data + offset, chunkdata, chunksize);
    offset += chunksize;
  }
  GST_DEBUG ("type: %d in: %" G_GSIZE_FORMAT " out: %d", chunk->message_type_id,
      chunksize, offset);

  return g_bytes_new_take (data, offset);
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

/* chunk cache */

GstRtmpChunkCache *
gst_rtmp_chunk_cache_new (void)
{
  return g_array_new (FALSE, TRUE, sizeof (GstRtmpChunkCacheEntry));
}

void
gst_rtmp_chunk_cache_free (GstRtmpChunkCache * cache)
{
  g_array_free (cache, TRUE);

}

GstRtmpChunkCacheEntry *
gst_rtmp_chunk_cache_get (GstRtmpChunkCache * cache, int stream_id)
{
  int i;
  GstRtmpChunkCacheEntry *entry;
  for (i = 0; i < cache->len; i++) {
    entry = &g_array_index (cache, GstRtmpChunkCacheEntry, i);
    if (entry->previous_header.stream_id == stream_id)
      return entry;
  }
  g_array_set_size (cache, cache->len + 1);
  entry = &g_array_index (cache, GstRtmpChunkCacheEntry, cache->len - 1);
  entry->previous_header.stream_id = stream_id;
  return entry;
}

void
gst_rtmp_chunk_cache_update (GstRtmpChunkCacheEntry * entry,
    GstRtmpChunk * chunk)
{
  entry->previous_header.timestamp = chunk->timestamp;
  entry->previous_header.message_length = chunk->message_length;
  entry->previous_header.message_type_id = chunk->message_type_id;
  entry->previous_header.info = chunk->info;
}

gboolean
gst_rtmp_chunk_parse_message (GstRtmpChunk * chunk, char **command_name,
    double *transaction_id, GstAmfNode ** command_object,
    GstAmfNode ** optional_args)
{
  gsize n_parsed;
  const guint8 *data;
  gsize size;
  int offset;
  GstAmfNode *n1, *n2, *n3, *n4;

  offset = 0;
  data = g_bytes_get_data (chunk->payload, &size);
  n1 = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
  offset += n_parsed;
  n2 = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
  offset += n_parsed;
  n3 = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
  offset += n_parsed;
  if (offset < size) {
    n4 = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
  } else {
    n4 = NULL;
  }

  if (command_name) {
    *command_name = g_strdup (gst_amf_node_get_string (n1));
  }
  gst_amf_node_free (n1);

  if (transaction_id) {
    *transaction_id = gst_amf_node_get_number (n2);
  }
  gst_amf_node_free (n2);

  if (command_object) {
    *command_object = n3;
  } else {
    gst_amf_node_free (n3);
  }

  if (optional_args) {
    *optional_args = n4;
  } else {
    if (n4)
      gst_amf_node_free (n4);
  }

  return TRUE;
}
