/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2024 Nishant <<user@hostname.org>>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-myfilter
 *
 * FIXME:Describe myfilter here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! myfilter ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstmyfilter.h"

GST_DEBUG_CATEGORY_STATIC(gst_myfilter_debug);
#define GST_CAT_DEFAULT gst_myfilter_debug

#define MY_MIN(a, b) ((a) < (b) ? (a) : (b))

#define GST_VIDEO_SIZE_RANGE "(int) [ 1, max ]"
#define GST_VIDEO_FPS_RANGE "(fraction) [ 0, 60 ]"

#define GST_VIDEO_CAPS_MAKE(format)     \
  "video/x-raw, "                       \
  "format = (string) " format ", "      \
  "width = " GST_VIDEO_SIZE_RANGE ", "  \
  "height = " GST_VIDEO_SIZE_RANGE ", " \
  "framerate = " GST_VIDEO_FPS_RANGE
#define GST_VIDEO_FORMATS_ALL "{ NV12, BGR, I420}"
#define SW_VIDEO_FORMATS_ALL "{RGB}" // Add more formats here
#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)
#define SW_CAPS GST_VIDEO_CAPS_MAKE(SW_VIDEO_FORMATS_ALL)

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
                                                                    GST_PAD_SINK,
                                                                    GST_PAD_ALWAYS,
                                                                    GST_STATIC_CAPS(VIDEO_CAPS));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
                                                                   GST_PAD_SRC,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS(VIDEO_CAPS));

#define gst_myfilter_parent_class parent_class
G_DEFINE_TYPE(Gstmyfilter, gst_myfilter, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE(myfilter, "myfilter", GST_RANK_NONE,
                            GST_TYPE_MYFILTER);

static void gst_myfilter_set_property(GObject *object,
                                      guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_myfilter_get_property(GObject *object,
                                      guint prop_id, GValue *value, GParamSpec *pspec);

static GstFlowReturn gst_myfilter_transform_ip(GstBaseTransform *
                                                   base,
                                               GstBuffer *outbuf);
static gboolean
gst_myfilter_setcaps(GstBaseTransform *base,
                     GstCaps *incaps, GstCaps *outcaps);
static GstCaps *
gst_myfilter_transform_caps(GstBaseTransform *trans,
                            GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static GstFlowReturn
gst_myfilter_transform(GstBaseTransform *base, GstBuffer *inbuf,
                       GstBuffer *outbuf);
static GstCaps *
gst_myfilter_fixate_caps(GstBaseTransform *base,
                         GstPadDirection direction, GstCaps *caps, GstCaps *othercaps);
/* GObject vmethod implementations */

/* initialize the myfilter's class */
static void gst_myfilter_class_init(GstmyfilterClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_myfilter_set_property;
  gobject_class->get_property = gst_myfilter_get_property;

  g_object_class_install_property(gobject_class, PROP_SILENT,
                                  g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
                                                       FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  gst_element_class_set_details_simple(gstelement_class,
                                       "myfilter",
                                       "Generic/Filter",
                                       "FIXME:Generic Template Filter", "Nishant <<user@hostname.org>>");

  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&sink_template));

  GST_BASE_TRANSFORM_CLASS(klass)->transform_ip =
      GST_DEBUG_FUNCPTR(gst_myfilter_transform_ip);
  GST_BASE_TRANSFORM_CLASS(klass)->transform_caps = GST_DEBUG_FUNCPTR(gst_myfilter_transform_caps);
  GST_BASE_TRANSFORM_CLASS(klass)->transform = GST_DEBUG_FUNCPTR(gst_myfilter_transform);
  GST_BASE_TRANSFORM_CLASS(klass)->set_caps = GST_DEBUG_FUNCPTR(gst_myfilter_setcaps);
  GST_BASE_TRANSFORM_CLASS(klass)->fixate_caps =
      GST_DEBUG_FUNCPTR(gst_myfilter_fixate_caps);
  /* debug category for fltering log messages
   *
   * FIXME:exchange the string 'Template myfilter' with your description
   */
  GST_DEBUG_CATEGORY_INIT(gst_myfilter_debug, "myfilter", 0,
                          "Template myfilter");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_myfilter_init(Gstmyfilter *filter)
{
  filter->silent = FALSE;
}

static void
gst_myfilter_set_property(GObject *object, guint prop_id,
                          const GValue *value, GParamSpec *pspec)
{
  Gstmyfilter *filter = GST_MYFILTER(object);

  switch (prop_id)
  {
  case PROP_SILENT:
    filter->silent = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
gst_myfilter_get_property(GObject *object, guint prop_id,
                          GValue *value, GParamSpec *pspec)
{
  Gstmyfilter *filter = GST_MYFILTER(object);

  switch (prop_id)
  {
  case PROP_SILENT:
    g_value_set_boolean(value, filter->silent);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

/* GstBaseTransform vmethod implementations */
void rgb_to_yuv(guint8 r, guint8 g, guint8 b, guint8 *y, guint8 *u, guint8 *v)
{
  // RGB to YUV conversion
  *y = (guchar)CLAMP(0.299 * r + 0.587 * g + 0.114 * b, 0, 255);
  *u = (guchar)CLAMP(128 - 0.147 * r - 0.289 * g + 0.436 * b, 0, 255);
  *v = (guchar)CLAMP(128 + 0.615 * r - 0.515 * g - 0.100 * b, 0, 255);
}
void bgr2nv12(Gstmyfilter *filter, guint8 *inbuf, guint in_size, guint8 *outbuf, guint out_size)
{
  gsize y_plane_size = filter->width * filter->height;

  guint8 *y_plane = outbuf;
  guint8 *uv_plane = y_plane + y_plane_size;

  for (int y = 0; y < filter->height; y++)
  {
    for (int x = 0; x < filter->width; x++)
    {
      // Get pixel values from overlay
      guchar *overlay_pixel = inbuf + (y * filter->width + x) * 3; // RGB
      guchar b = overlay_pixel[0];
      guchar g = overlay_pixel[1];
      guchar r = overlay_pixel[2];

      guint y_index = (y)*filter->width + (x);
      guint uv_index = ((y) / 2) * (filter->width / 2) + ((x) / 2);

      guint8 y_ol, u_ol, v_ol;
      rgb_to_yuv(r, g, b, &y_ol, &u_ol, &v_ol);

      y_plane[y_index] = y_ol;
      uv_plane[uv_index * 2] = u_ol;
      uv_plane[uv_index * 2 + 1] = v_ol;
    }
  }
}

static GstFlowReturn
gst_myfilter_transform(GstBaseTransform *base, GstBuffer *inbuf,
                       GstBuffer *outbuf)
{
  Gstmyfilter *self = GST_MYFILTER(base);
  GstMapInfo in_map, out_map;

  guint in_size = gst_buffer_get_size(inbuf);
  guint out_size = gst_buffer_get_size(outbuf);

  if (!gst_buffer_map(inbuf, &in_map, GST_MAP_READ))
  {
    return GST_FLOW_ERROR;
  }

  if (!gst_buffer_map(outbuf, &out_map, GST_MAP_WRITE))
  {
    gst_buffer_unmap(inbuf, &in_map);
    return GST_FLOW_ERROR;
  }

  bgr2nv12(self, in_map.data, in_size, out_map.data, out_size);

  gst_buffer_unmap(inbuf, &in_map);
  gst_buffer_unmap(outbuf, &out_map);

  return GST_FLOW_OK;
}

static GstCaps *
gst_myfilter_fixate_caps(GstBaseTransform *base,
                         GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
  GstStructure *structure;
  gint rate, width, height;

  structure = gst_caps_get_structure(caps, 0);
  gst_structure_get_int(structure, "width", &width);
  gst_structure_get_int(structure, "height", &height);
  if (G_UNLIKELY(!gst_structure_get_int(structure, "framerate", &rate)))
  {

    g_print("@@@ %s width: %d height: %d framerate: %d \n", __func__, width, height, rate);
    return othercaps;
  }

  othercaps = gst_caps_truncate(othercaps);
  othercaps = gst_caps_make_writable(othercaps);
  structure = gst_caps_get_structure(othercaps, 0);

  gst_structure_fixate_field_nearest_int(structure, "rate", rate);

  return othercaps;
}

static GstCaps *
gst_myfilter_transform_caps(GstBaseTransform *trans,
                            GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
  GstCaps *new_caps = NULL;
  GstStructure *structure;
  const gchar *pixel_format;
  structure = gst_caps_get_structure(filter, 0);

  GstCaps *res_caps;

  res_caps = gst_caps_copy(caps);
  structure = gst_caps_get_structure(res_caps, 0);

  pixel_format = gst_structure_get_string(structure, "format");
  g_print("pixel_format: %s, =>  ", pixel_format);

  if (direction == GST_PAD_SRC)
  {
    g_print("pad dire: SRC\n");
    new_caps = gst_caps_from_string("video/x-raw, format=BGR");
    // new_caps = gst_caps_from_string(VIDEO_CAPS);
  }
  else if (direction == GST_PAD_SINK)
  {
    g_print("pad dire: SINK\n");
    new_caps = gst_caps_from_string("video/x-raw, format=NV12");
    // new_caps = gst_caps_from_string(VIDEO_CAPS);
  }

  return new_caps;
}

static gboolean
gst_myfilter_setcaps(GstBaseTransform *base,
                     GstCaps *incaps, GstCaps *outcaps)
{
  Gstmyfilter *filter = GST_MYFILTER(base);
  GstStructure *structure;
  const gchar *pixel_format;
  // GstCaps *caps;
  // g_print("@@@ %s\n", __func__);
  structure = gst_caps_get_structure(incaps, 0);

  gst_structure_get_int(structure, "width", &filter->width);
  gst_structure_get_int(structure, "height", &filter->height);

  pixel_format = gst_structure_get_string(structure, "format");
  gint rate;
  gst_structure_get_int(structure, "framerate", &rate);
  g_print("form_%s pixel_format: %s, framerate: %d\n", __func__, pixel_format, rate);
  if (gst_structure_has_name(structure, "video/x-raw"))
  {
    g_print("cpas have video/x-raw\n");
    return TRUE;
  }
  return FALSE;
}

static GstFlowReturn
gst_myfilter_transform_ip(GstBaseTransform *base, GstBuffer *outbuf)
{
  // Gstmyfilter *filter = GST_MYFILTER(base);

  g_print("@@@ %s\n", __func__);

  return GST_FLOW_OK;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
myfilter_init(GstPlugin *myfilter)
{
  return GST_ELEMENT_REGISTER(myfilter, myfilter);
}

/* gstreamer looks for this structure to register myfilters
 *
 * FIXME:exchange the string 'Template myfilter' with you myfilter description
 */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  myfilter,
                  "myfilter",
                  myfilter_init,
                  PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
