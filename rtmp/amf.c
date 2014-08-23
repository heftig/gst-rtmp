/* GStreamer RTMP Library
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "amf.h"

typedef struct _AmfObjectField AmfObjectField;
struct _AmfObjectField
{
  char *name;
  GstAmfNode *value;
};

typedef struct _AmfParser AmfParser;
struct _AmfParser
{
  const char *data;
  gsize size;
  int offset;
  gboolean error;
};

static char *_parse_utf8_string (AmfParser * parser);
static void _parse_object (AmfParser * parser, GstAmfNode * node);
static GstAmfNode *_parse_value (AmfParser * parser);
static void amf_object_field_free (AmfObjectField * field);


GstAmfNode *
gst_amf_node_new (GstAmfType type)
{
  GstAmfNode *node;

  node = g_malloc0 (sizeof (GstAmfNode));
  node->type = type;
  if (node->type == GST_AMF_TYPE_OBJECT) {
    node->array_val = g_ptr_array_new ();
  }

  return node;
}

void
gst_amf_node_free (GstAmfNode * node)
{
  if (node->type == GST_AMF_TYPE_STRING) {
    g_free (node->string_val);
  } else if (node->type == GST_AMF_TYPE_OBJECT) {
    g_ptr_array_foreach (node->array_val, (GFunc) amf_object_field_free, NULL);
    g_ptr_array_free (node->array_val, TRUE);
  }

  g_free (node);
}

static int
_parse_u8 (AmfParser * parser)
{
  int x;
  x = parser->data[parser->offset];
  parser->offset++;
  return x;
}

static int
_parse_u16 (AmfParser * parser)
{
  int x;
  x = (parser->data[parser->offset] << 8) | parser->data[parser->offset + 1];
  parser->offset += 2;
  return x;
}

#if 0
static int
_parse_u24 (AmfParser * parser)
{
  int x;
  x = (parser->data[parser->offset] << 16) |
      (parser->data[parser->offset + 1] << 8) | parser->data[parser->offset +
      2];
  parser->offset += 3;
  return x;
}

static int
_parse_u32 (AmfParser * parser)
{
  int x;
  x = (parser->data[parser->offset] << 24) |
      (parser->data[parser->offset + 1] << 16) |
      (parser->data[parser->offset + 2] << 8) | parser->data[parser->offset +
      3];
  parser->offset += 4;
  return x;
}
#endif

static int
_parse_double (AmfParser * parser)
{
  double d;
  int i;
  guint8 *d_ptr = (guint8 *) & d;
  for (i = 0; i < 8; i++) {
    d_ptr[i] = parser->data[parser->offset + (7 - i)];
  }
  parser->offset += 8;
  return d;
}

static char *
_parse_utf8_string (AmfParser * parser)
{
  int size;
  char *s;

  size = _parse_u16 (parser);
  if (parser->offset + size >= parser->size) {
    parser->error = TRUE;
    return NULL;
  }
  s = g_strndup (parser->data + parser->offset, size);
  parser->offset += size;

  return s;
}

static void
_parse_object (AmfParser * parser, GstAmfNode * node)
{
  while (1) {
    char *s;
    GstAmfNode *child_node;
    s = _parse_utf8_string (parser);
    child_node = _parse_value (parser);
    if (child_node->type == GST_AMF_TYPE_OBJECT_END) {
      gst_amf_node_free (child_node);
      break;
    }
    gst_amf_object_append_take (node, s, child_node);
  }
}

static GstAmfNode *
_parse_value (AmfParser * parser)
{
  GstAmfNode *node = NULL;
  GstAmfType type;

  type = _parse_u8 (parser);
  node = gst_amf_node_new (type);

  GST_DEBUG ("parsing type %d", type);

  switch (type) {
    case GST_AMF_TYPE_NUMBER:
      gst_amf_node_set_double (node, _parse_double (parser));
      break;
    case GST_AMF_TYPE_BOOLEAN:
      gst_amf_node_set_boolean (node, _parse_u8 (parser));
      break;
    case GST_AMF_TYPE_STRING:
      gst_amf_node_set_string_take (node, _parse_utf8_string (parser));
      break;
    case GST_AMF_TYPE_OBJECT:
      _parse_object (parser, node);
      break;
    case GST_AMF_TYPE_MOVIECLIP:
      GST_ERROR ("unimplemented");
      break;
    case GST_AMF_TYPE_NULL:
      break;
    case GST_AMF_TYPE_OBJECT_END:
      break;
    default:
      GST_ERROR ("unimplemented");
      break;
  }

  return node;
}

GstAmfNode *
gst_amf_node_new_parse (const char *data, int size, int *n_bytes)
{
  AmfParser _p = { 0 }, *parser = &_p;
  GstAmfNode *node;

  parser->data = data;
  parser->size = size;
  node = _parse_value (parser);

  if (n_bytes)
    *n_bytes = parser->offset;
  return node;
}

void
gst_amf_node_set_boolean (GstAmfNode * node, gboolean val)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_BOOLEAN);
  node->int_val = val;
}

void
gst_amf_node_set_double (GstAmfNode * node, double val)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_NUMBER);
  node->double_val = val;
}

void
gst_amf_node_set_string (GstAmfNode * node, const char *s)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_STRING);
  node->string_val = g_strdup (s);
}

void
gst_amf_node_set_string_take (GstAmfNode * node, char *s)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_STRING);
  node->string_val = s;
}

void
gst_amf_object_append_take (GstAmfNode * node, char *s, GstAmfNode * child_node)
{
  AmfObjectField *field;

  g_return_if_fail (node->type == GST_AMF_TYPE_OBJECT);

  field = g_malloc0 (sizeof (AmfObjectField));
  field->name = s;
  field->value = child_node;
  g_ptr_array_add (node->array_val, field);
}

static void
amf_object_field_free (AmfObjectField * field)
{
  g_free (field->name);
  gst_amf_node_free (field->value);
  g_free (field);
}
