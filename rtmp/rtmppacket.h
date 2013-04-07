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

#ifndef _GST_RTMP_PACKET_H_
#define _GST_RTMP_PACKET_H_


G_BEGIN_DECLS

#define GST_TYPE_RTMP_PACKET   (gst_rtmp_packet_get_type())
#define GST_RTMP_PACKET(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_PACKET,GstRtmpPacket))
#define GST_RTMP_PACKET_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_PACKET,GstRtmpPacketClass))
#define GST_IS_RTMP_PACKET(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_PACKET))
#define GST_IS_RTMP_PACKET_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_PACKET))

typedef struct _GstRtmpPacket GstRtmpPacket;
typedef struct _GstRtmpPacketClass GstRtmpPacketClass;

struct _GstRtmpPacket
{
  GObject object;

  char *request_data;
  gsize request_length;

  char *response_data;
  gsize response_length;

};

struct _GstRtmpPacketClass
{
  GObjectClass object_class;
};

GType gst_rtmp_packet_get_type (void);

G_END_DECLS

#endif
