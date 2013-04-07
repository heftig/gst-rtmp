/* GStreamer
 * Copyright (C) 2013 FIXME <fixme@example.com>
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

#ifndef _GST_RTMP_SERVER_H_
#define _GST_RTMP_SERVER_H_


G_BEGIN_DECLS

#define GST_TYPE_RTMP_SERVER   (gst_rtmp_server_get_type())
#define GST_RTMP_SERVER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_SERVER,GstRtmpServer))
#define GST_RTMP_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_SERVER,GstRtmpServerClass))
#define GST_IS_RTMP_SERVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_SERVER))
#define GST_IS_RTMP_SERVER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_SERVER))

typedef struct _GstRtmpServer GstRtmpServer;
typedef struct _GstRtmpServerClass GstRtmpServerClass;

struct _GstRtmpServer
{
  GObject base_rtmpserver;

};

struct _GstRtmpServerClass
{
  GObjectClass base_rtmpserver_class;
};

GType gst_rtmp_server_get_type (void);

G_END_DECLS

#endif
