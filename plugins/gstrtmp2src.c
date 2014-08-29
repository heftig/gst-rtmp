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
 * SECTION:element-gstrtmp2src
 *
 * The rtmp2src element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! rtmp2src ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstrtmp2src.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp2_src_debug_category);
#define GST_CAT_DEFAULT gst_rtmp2_src_debug_category

/* prototypes */


static void gst_rtmp2_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_dispose (GObject * object);
static void gst_rtmp2_src_finalize (GObject * object);

static GstCaps *gst_rtmp2_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_rtmp2_src_negotiate (GstBaseSrc * src);
static GstCaps *gst_rtmp2_src_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_rtmp2_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_rtmp2_src_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_rtmp2_src_start (GstBaseSrc * src);
static gboolean gst_rtmp2_src_stop (GstBaseSrc * src);
static void gst_rtmp2_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_rtmp2_src_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_rtmp2_src_is_seekable (GstBaseSrc * src);
static gboolean gst_rtmp2_src_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * seek, GstSegment * segment);
static gboolean gst_rtmp2_src_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_rtmp2_src_unlock (GstBaseSrc * src);
static gboolean gst_rtmp2_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_rtmp2_src_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_rtmp2_src_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_rtmp2_src_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_rtmp2_src_alloc (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_rtmp2_src_fill (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer * buf);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_rtmp2_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmp2Src, gst_rtmp2_src, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_rtmp2_src_debug_category, "rtmp2src", 0,
        "debug category for rtmp2src element"));

static void
gst_rtmp2_src_class_init (GstRtmp2SrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_rtmp2_src_src_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FIXME Long name", "Generic", "FIXME Description",
      "FIXME <fixme@example.com>");

  gobject_class->set_property = gst_rtmp2_src_set_property;
  gobject_class->get_property = gst_rtmp2_src_get_property;
  gobject_class->dispose = gst_rtmp2_src_dispose;
  gobject_class->finalize = gst_rtmp2_src_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_rtmp2_src_get_caps);
  base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_rtmp2_src_negotiate);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_rtmp2_src_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_rtmp2_src_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_rtmp2_src_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_rtmp2_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_rtmp2_src_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_rtmp2_src_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_rtmp2_src_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_rtmp2_src_is_seekable);
  base_src_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_rtmp2_src_prepare_seek_segment);
  base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_rtmp2_src_do_seek);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_rtmp2_src_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_rtmp2_src_unlock_stop);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_rtmp2_src_query);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_rtmp2_src_event);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_rtmp2_src_create);
  base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_rtmp2_src_alloc);
  base_src_class->fill = GST_DEBUG_FUNCPTR (gst_rtmp2_src_fill);

}

static void
gst_rtmp2_src_init (GstRtmp2Src * rtmp2src)
{
}

void
gst_rtmp2_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (object);

  GST_DEBUG_OBJECT (rtmp2src, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp2_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (object);

  GST_DEBUG_OBJECT (rtmp2src, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rtmp2_src_dispose (GObject * object)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (object);

  GST_DEBUG_OBJECT (rtmp2src, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_rtmp2_src_parent_class)->dispose (object);
}

void
gst_rtmp2_src_finalize (GObject * object)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (object);

  GST_DEBUG_OBJECT (rtmp2src, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_rtmp2_src_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_rtmp2_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "get_caps");

  return NULL;
}

/* decide on caps */
static gboolean
gst_rtmp2_src_negotiate (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "negotiate");

  return TRUE;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_rtmp2_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "fixate");

  return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_rtmp2_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "set_caps");

  return TRUE;
}

/* setup allocation query */
static gboolean
gst_rtmp2_src_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "decide_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_rtmp2_src_start (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "start");

  return TRUE;
}

static gboolean
gst_rtmp2_src_stop (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "stop");

  return TRUE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_rtmp2_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "get_times");

}

/* get the total size of the resource in bytes */
static gboolean
gst_rtmp2_src_get_size (GstBaseSrc * src, guint64 * size)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "get_size");

  return TRUE;
}

/* check if the resource is seekable */
static gboolean
gst_rtmp2_src_is_seekable (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "is_seekable");

  return TRUE;
}

/* Prepare the segment on which to perform do_seek(), converting to the
 * current basesrc format. */
static gboolean
gst_rtmp2_src_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "prepare_seek_segment");

  return TRUE;
}

/* notify subclasses of a seek */
static gboolean
gst_rtmp2_src_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "do_seek");

  return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_rtmp2_src_unlock (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_rtmp2_src_unlock_stop (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "unlock_stop");

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_rtmp2_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "query");

  return TRUE;
}

/* notify subclasses of an event */
static gboolean
gst_rtmp2_src_event (GstBaseSrc * src, GstEvent * event)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "event");

  return TRUE;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_rtmp2_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "create");

  return GST_FLOW_OK;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_rtmp2_src_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "alloc");

  return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_rtmp2_src_fill (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer * buf)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "fill");

  return GST_FLOW_OK;
}
