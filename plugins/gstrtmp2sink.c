/* GStreamer
 * Copyright (C) 2014 David Schleef <ds@schleef.org>
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
/**
 * SECTION:element-gstrtmp2sink
 *
 * The rtmp2sink element sends audio and video streams to an RTMP
 * server.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! rtmp2sink ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstrtmp2sink.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp2_sink_debug_category);
#define GST_CAT_DEFAULT gst_rtmp2_sink_debug_category

/* prototypes */


static void gst_rtmp2_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp2_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp2_sink_dispose (GObject * object);
static void gst_rtmp2_sink_finalize (GObject * object);

static GstCaps *gst_rtmp2_sink_get_caps (GstBaseSink * sink, GstCaps * filter);
static gboolean gst_rtmp2_sink_set_caps (GstBaseSink * sink, GstCaps * caps);
static GstCaps *gst_rtmp2_sink_fixate (GstBaseSink * sink, GstCaps * caps);
static gboolean gst_rtmp2_sink_activate_pull (GstBaseSink * sink,
    gboolean active);
static void gst_rtmp2_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_rtmp2_sink_propose_allocation (GstBaseSink * sink,
    GstQuery * query);
static gboolean gst_rtmp2_sink_start (GstBaseSink * sink);
static gboolean gst_rtmp2_sink_stop (GstBaseSink * sink);
static gboolean gst_rtmp2_sink_unlock (GstBaseSink * sink);
static gboolean gst_rtmp2_sink_unlock_stop (GstBaseSink * sink);
static gboolean gst_rtmp2_sink_query (GstBaseSink * sink, GstQuery * query);
static gboolean gst_rtmp2_sink_event (GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_rtmp2_sink_wait_event (GstBaseSink * sink,
    GstEvent * event);
static GstFlowReturn gst_rtmp2_sink_prepare (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_rtmp2_sink_prepare_list (GstBaseSink * sink,
    GstBufferList * buffer_list);
static GstFlowReturn gst_rtmp2_sink_preroll (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_rtmp2_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_rtmp2_sink_render_list (GstBaseSink * sink,
    GstBufferList * buffer_list);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_rtmp2_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-flv")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmp2Sink, gst_rtmp2_sink, GST_TYPE_BASE_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp2_sink_debug_category, "rtmp2sink", 0,
        "debug category for rtmp2sink element"));

static void
gst_rtmp2_sink_class_init (GstRtmp2SinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_rtmp2_sink_sink_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FIXME Long name", "Generic", "FIXME Description",
      "FIXME <fixme@example.com>");

  gobject_class->set_property = gst_rtmp2_sink_set_property;
  gobject_class->get_property = gst_rtmp2_sink_get_property;
  gobject_class->dispose = gst_rtmp2_sink_dispose;
  gobject_class->finalize = gst_rtmp2_sink_finalize;
  base_sink_class->get_caps = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_get_caps);
  base_sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_set_caps);
  base_sink_class->fixate = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_fixate);
  base_sink_class->activate_pull =
      GST_DEBUG_FUNCPTR (gst_rtmp2_sink_activate_pull);
  base_sink_class->get_times = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_get_times);
  base_sink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_rtmp2_sink_propose_allocation);
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_stop);
  base_sink_class->unlock = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_unlock);
  base_sink_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_unlock_stop);
  base_sink_class->query = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_query);
  base_sink_class->event = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_event);
  base_sink_class->wait_event = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_wait_event);
  base_sink_class->prepare = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_prepare);
  base_sink_class->prepare_list =
      GST_DEBUG_FUNCPTR (gst_rtmp2_sink_prepare_list);
  base_sink_class->preroll = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_preroll);
  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_render);
  base_sink_class->render_list = GST_DEBUG_FUNCPTR (gst_rtmp2_sink_render_list);

}

static void
gst_rtmp2_sink_init (GstRtmp2Sink * rtmp2sink)
{
}

void
gst_rtmp2_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (object);

  GST_DEBUG_OBJECT (rtmp2sink, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp2_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (object);

  GST_DEBUG_OBJECT (rtmp2sink, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp2_sink_dispose (GObject * object)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (object);

  GST_DEBUG_OBJECT (rtmp2sink, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp2_sink_parent_class)->dispose (object);
}

void
gst_rtmp2_sink_finalize (GObject * object)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (object);

  GST_DEBUG_OBJECT (rtmp2sink, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp2_sink_parent_class)->finalize (object);
}

static GstCaps *
gst_rtmp2_sink_get_caps (GstBaseSink * sink, GstCaps * filter)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "get_caps");

  return NULL;
}

/* notify subclass of new caps */
static gboolean
gst_rtmp2_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "set_caps");

  return TRUE;
}

/* fixate sink caps during pull-mode negotiation */
static GstCaps *
gst_rtmp2_sink_fixate (GstBaseSink * sink, GstCaps * caps)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "fixate");

  return NULL;
}

/* start or stop a pulling thread */
static gboolean
gst_rtmp2_sink_activate_pull (GstBaseSink * sink, gboolean active)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "activate_pull");

  return TRUE;
}

/* get the start and end times for syncing on this buffer */
static void
gst_rtmp2_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "get_times");

}

/* propose allocation parameters for upstream */
static gboolean
gst_rtmp2_sink_propose_allocation (GstBaseSink * sink, GstQuery * query)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "propose_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_rtmp2_sink_start (GstBaseSink * sink)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "start");

  return TRUE;
}

static gboolean
gst_rtmp2_sink_stop (GstBaseSink * sink)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "stop");

  return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_rtmp2_sink_unlock (GstBaseSink * sink)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "unlock");

  return TRUE;
}

/* Clear a previously indicated unlock request not that unlocking is
 * complete. Sub-classes should clear any command queue or indicator they
 * set during unlock */
static gboolean
gst_rtmp2_sink_unlock_stop (GstBaseSink * sink)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "unlock_stop");

  return TRUE;
}

/* notify subclass of query */
static gboolean
gst_rtmp2_sink_query (GstBaseSink * sink, GstQuery * query)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "query");

  return TRUE;
}

/* notify subclass of event */
static gboolean
gst_rtmp2_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "event");

  return TRUE;
}

/* wait for eos or gap, subclasses should chain up to parent first */
static GstFlowReturn
gst_rtmp2_sink_wait_event (GstBaseSink * sink, GstEvent * event)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "wait_event");

  return GST_FLOW_OK;
}

/* notify subclass of buffer or list before doing sync */
static GstFlowReturn
gst_rtmp2_sink_prepare (GstBaseSink * sink, GstBuffer * buffer)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "prepare");

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_rtmp2_sink_prepare_list (GstBaseSink * sink, GstBufferList * buffer_list)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "prepare_list");

  return GST_FLOW_OK;
}

/* notify subclass of preroll buffer or real buffer */
static GstFlowReturn
gst_rtmp2_sink_preroll (GstBaseSink * sink, GstBuffer * buffer)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "preroll");

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_rtmp2_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "render");

  return GST_FLOW_OK;
}

/* Render a BufferList */
static GstFlowReturn
gst_rtmp2_sink_render_list (GstBaseSink * sink, GstBufferList * buffer_list)
{
  GstRtmp2Sink *rtmp2sink = GST_RTMP2_SINK (sink);

  GST_DEBUG_OBJECT (rtmp2sink, "render_list");

  return GST_FLOW_OK;
}
