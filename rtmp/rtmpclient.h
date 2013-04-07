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

#ifndef _GST_RTMP_CLIENT_H_
#define _GST_RTMP_CLIENT_H_


G_BEGIN_DECLS

#define GST_TYPE_RTMP_CLIENT   (gst_rtmp_client_get_type())
#define GST_RTMP_CLIENT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_CLIENT,GstRtmpClient))
#define GST_RTMP_CLIENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_CLIENT,GstRtmpClientClass))
#define GST_IS_RTMP_CLIENT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_CLIENT))
#define GST_IS_RTMP_CLIENT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_CLIENT))

typedef struct _GstRtmpClient GstRtmpClient;
typedef struct _GstRtmpClientClass GstRtmpClientClass;

struct _GstRtmpClient
{
  GObject base_rtmpclient;

};

struct _GstRtmpClientClass
{
  GObjectClass base_rtmpclient_class;
};

GType gst_rtmp_client_get_type (void);

G_END_DECLS

#endif
