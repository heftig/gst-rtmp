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
 * The rtmp2src element receives input streams from an RTMP server.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v rtmp2src ! decodebin ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>
#include "gstrtmp2src.h"
#include <rtmp/rtmpchunk.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp2_src_debug_category);
#define GST_CAT_DEFAULT gst_rtmp2_src_debug_category

/* prototypes */


static void gst_rtmp2_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_dispose (GObject * object);
static void gst_rtmp2_src_finalize (GObject * object);
static void gst_rtmp2_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static void gst_rtmp2_src_task (gpointer user_data);
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

static void got_chunk (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data);
static void connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void send_connect (GstRtmp2Src * src);
static void cmd_connect_done (GstRtmpConnection * connection,
    GstRtmpChunk * chunk, const char *command_name, int transaction_id,
    GstAmfNode * command_object, GstAmfNode * optional_args,
    gpointer user_data);
static void send_create_stream (GstRtmp2Src * src);
static void create_stream_done (GstRtmpConnection * connection,
    GstRtmpChunk * chunk, const char *command_name, int transaction_id,
    GstAmfNode * command_object, GstAmfNode * optional_args,
    gpointer user_data);
static void send_play (GstRtmp2Src * src);
static void play_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_rtmp2_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-flv")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmp2Src, gst_rtmp2_src, GST_TYPE_PUSH_SRC,
    do {
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
          gst_rtmp2_src_uri_handler_init);
      GST_DEBUG_CATEGORY_INIT (gst_rtmp2_src_debug_category, "rtmp2src", 0,
          "debug category for rtmp2src element");} while (0));

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
      "RTMP source element", "Source", "Source element for RTMP streams",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_rtmp2_src_set_property;
  gobject_class->get_property = gst_rtmp2_src_get_property;
  gobject_class->dispose = gst_rtmp2_src_dispose;
  gobject_class->finalize = gst_rtmp2_src_finalize;
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
  if (0)
    base_src_class->query = GST_DEBUG_FUNCPTR (gst_rtmp2_src_query);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_rtmp2_src_event);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_rtmp2_src_create);
  base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_rtmp2_src_alloc);

}

static void
gst_rtmp2_src_init (GstRtmp2Src * rtmp2src)
{
  g_mutex_init (&rtmp2src->lock);
  rtmp2src->queue = g_queue_new ();

  //gst_base_src_set_live (GST_BASE_SRC(rtmp2src), TRUE);

  rtmp2src->task = gst_task_new (gst_rtmp2_src_task, rtmp2src, NULL);
  g_rec_mutex_init (&rtmp2src->task_lock);
  gst_task_set_lock (rtmp2src->task, &rtmp2src->task_lock);
  rtmp2src->client = gst_rtmp_client_new ();
  gst_rtmp_client_set_server_address (rtmp2src->client,
      "ec2-54-185-55-241.us-west-2.compute.amazonaws.com");

}

static GstURIType
gst_rtmp2_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rtmp2_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "rtmp", NULL };

  return protocols;
}

static gchar *
gst_rtmp2_src_uri_get_uri (GstURIHandler * handler)
{
  GstRtmp2Src *src = GST_RTMP2_SRC (handler);

  /* FIXME: make thread-safe */
  return g_strdup (src->uri);
}

static gboolean
gst_rtmp2_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  return TRUE;
}

static void
gst_rtmp2_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtmp2_src_uri_get_type;
  iface->get_protocols = gst_rtmp2_src_uri_get_protocols;
  iface->get_uri = gst_rtmp2_src_uri_get_uri;
  iface->set_uri = gst_rtmp2_src_uri_set_uri;
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
  g_object_unref (rtmp2src->task);
  g_rec_mutex_clear (&rtmp2src->task_lock);
  g_object_unref (rtmp2src->client);
  g_mutex_clear (&rtmp2src->lock);
  g_queue_free_full (rtmp2src->queue, g_object_unref);

  G_OBJECT_CLASS (gst_rtmp2_src_parent_class)->finalize (object);
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_rtmp2_src_start (GstBaseSrc * src)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (rtmp2src, "start");

  gst_task_start (rtmp2src->task);

  return TRUE;
}

static void
gst_rtmp2_src_task (gpointer user_data)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  GMainLoop *main_loop;
  GMainContext *main_context;

  gst_rtmp_client_connect_async (rtmp2src->client, NULL, connect_done,
      rtmp2src);

  main_context = g_main_context_new ();
  main_loop = g_main_loop_new (main_context, TRUE);
  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);
  g_main_context_unref (main_context);
}

static void
connect_done (GObject * source, GAsyncResult * result, gpointer user_data)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  GError *error = NULL;
  gboolean ret;

  ret = gst_rtmp_client_connect_finish (rtmp2src->client, result, &error);
  if (!ret) {
    GST_ERROR ("error: %s", error->message);
    g_error_free (error);
    return;
  }

  rtmp2src->connection = gst_rtmp_client_get_connection (rtmp2src->client);
  g_signal_connect (rtmp2src->connection, "got-chunk", G_CALLBACK (got_chunk),
      rtmp2src);

  send_connect (rtmp2src);
}

static void
dump_command (GstRtmpChunk * chunk)
{
  GstAmfNode *amf;
  gsize size;
  const guint8 *data;
  gsize n_parsed;
  int offset;

  offset = 0;
  data = g_bytes_get_data (chunk->payload, &size);
  while (offset < size) {
    amf = gst_amf_node_new_parse (data + offset, size - offset, &n_parsed);
    gst_amf_node_dump (amf);
    gst_amf_node_free (amf);
    offset += n_parsed;
  }
}

static void
dump_chunk (GstRtmpChunk * chunk, gboolean dir)
{
  g_print ("%s stream_id:%-4d ts:%-8d len:%-6" G_GSIZE_FORMAT
      " type_id:%-4d info:%08x\n", dir ? ">>>" : "<<<",
      chunk->stream_id,
      chunk->timestamp,
      chunk->message_length, chunk->message_type_id, chunk->info);
  if (chunk->message_type_id == 20) {
    dump_command (chunk);
  }
  if (chunk->message_type_id == 18) {
    dump_command (chunk);
  }
  gst_rtmp_dump_data (gst_rtmp_chunk_get_payload (chunk));
}

static void
got_chunk (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    gpointer user_data)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  if (rtmp2src->dump) {
    dump_chunk (chunk, FALSE);
  }
}

static void
send_connect (GstRtmp2Src * rtmp2src)
{
  GstAmfNode *node;

  node = gst_amf_node_new (GST_AMF_TYPE_OBJECT);
  gst_amf_object_set_string (node, "app", "live");
  gst_amf_object_set_string (node, "type", "nonprivate");
  gst_amf_object_set_string (node, "tcUrl", "rtmp://localhost:1935/live");
  // "fpad": False,
  // "capabilities": 15,
  // "audioCodecs": 3191,
  // "videoCodecs": 252,
  // "videoFunction": 1,
  gst_rtmp_connection_send_command (rtmp2src->connection, 3, "connect", 1, node,
      NULL, cmd_connect_done, rtmp2src);
}

static void
cmd_connect_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  gboolean ret;

  ret = FALSE;
  if (optional_args) {
    const GstAmfNode *n;
    n = gst_amf_node_get_object (optional_args, "code");
    if (n) {
      const char *s;
      s = gst_amf_node_get_string (n);
      if (strcmp (s, "NetConnection.Connect.Success") == 0) {
        ret = TRUE;
      }
    }
  }

  if (ret) {
    GST_ERROR ("success");

    send_create_stream (rtmp2src);
  }
}

static void
send_create_stream (GstRtmp2Src * rtmp2src)
{
  GstAmfNode *node;

  node = gst_amf_node_new (GST_AMF_TYPE_NULL);
  gst_rtmp_connection_send_command (rtmp2src->connection, 3, "createStream", 2,
      node, NULL, create_stream_done, rtmp2src);

}

static void
create_stream_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  gboolean ret;
  int stream_id;

  ret = FALSE;
  if (optional_args) {
    stream_id = gst_amf_node_get_number (optional_args);
    ret = TRUE;
  }

  if (ret) {
    GST_ERROR ("createStream success, stream_id=%d", stream_id);

    send_play (rtmp2src);
  }
}

static void
send_play (GstRtmp2Src * rtmp2src)
{
  GstAmfNode *n1;
  GstAmfNode *n2;
  GstAmfNode *n3;

  n1 = gst_amf_node_new (GST_AMF_TYPE_NULL);
  n2 = gst_amf_node_new (GST_AMF_TYPE_STRING);
  gst_amf_node_set_string (n2, "myStream");
  n3 = gst_amf_node_new (GST_AMF_TYPE_NUMBER);
  gst_amf_node_set_number (n3, 0);
  gst_rtmp_connection_send_command2 (rtmp2src->connection, 8, 1, "play", 3, n1,
      n2, n3, NULL, play_done, rtmp2src);

}

static void
play_done (GstRtmpConnection * connection, GstRtmpChunk * chunk,
    const char *command_name, int transaction_id, GstAmfNode * command_object,
    GstAmfNode * optional_args, gpointer user_data)
{
  //GstRtmp2Src *rtmp2src = GST_RTMP2_SRC (user_data);
  gboolean ret;
  int stream_id;

  ret = FALSE;
  if (optional_args) {
    stream_id = gst_amf_node_get_number (optional_args);
    ret = TRUE;
  }

  if (ret) {
    GST_ERROR ("play success, stream_id=%d", stream_id);

  }
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

  return FALSE;
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
  GstRtmpChunk *chunk;
  const char *data;
  gsize payload_size;

  GST_DEBUG_OBJECT (rtmp2src, "create");

  g_mutex_lock (&rtmp2src->lock);
  chunk = g_queue_pop_head (rtmp2src->queue);
  while (!chunk) {
    if (rtmp2src->reset) {
      g_mutex_unlock (&rtmp2src->lock);
      return GST_FLOW_ERROR;
    }
    g_cond_wait (&rtmp2src->cond, &rtmp2src->lock);
    chunk = g_queue_pop_head (rtmp2src->queue);
  }
  g_mutex_unlock (&rtmp2src->lock);

  data = g_bytes_get_data (chunk->payload, &payload_size);
  *buf =
      gst_buffer_new_wrapped_full (0, (gpointer) data, payload_size, 0,
      payload_size, chunk, g_object_unref);

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
