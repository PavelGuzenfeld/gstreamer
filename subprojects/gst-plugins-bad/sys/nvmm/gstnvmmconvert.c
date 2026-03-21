/* GStreamer
 * Copyright (C) 2026 Pavel Guzenfeld
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

/**
 * SECTION:gstnvmmconvert
 * @title: nvmmconvert
 * @short_description: NVMM video crop/scale/convert using Tegra VIC
 *
 * nvmmconvert performs video crop, scale, and color format conversion
 * on NVMM (NvBufSurface) memory using the Tegra VIC hardware engine.
 * All operations are zero-copy — no CPU involvement.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 nvv4l2decoder ! nvmmconvert crop-x=100 crop-y=100 \
 *   crop-w=800 crop-h=600 ! 'video/x-raw(memory:NVMM),width=640,height=480' \
 *   ! nvmmsink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvmmconvert.h"
#include "gstnvmmallocator.h"

#ifdef NVMM_MOCK_API
#include "nvbufsurface-stub.h"
#else
#include "nvbufsurface.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_nvmm_convert_debug);
#define GST_CAT_DEFAULT gst_nvmm_convert_debug

enum
{
  PROP_0,
  PROP_CROP_X,
  PROP_CROP_Y,
  PROP_CROP_W,
  PROP_CROP_H,
  PROP_FLIP_METHOD,
};

struct _GstNvmmConvertPrivate
{
  guint crop_x;
  guint crop_y;
  guint crop_w;
  guint crop_h;
  gint flip_method;

  GstVideoInfo sink_info;
  GstVideoInfo src_info;
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(memory:NVMM), "
        "format=(string){NV12, RGBA, I420, BGRA}, "
        "width=(int)[1, 8192], height=(int)[1, 8192], "
        "framerate=(fraction)[0/1, 240/1]")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(memory:NVMM), "
        "format=(string){NV12, RGBA, I420, BGRA}, "
        "width=(int)[1, 8192], height=(int)[1, 8192], "
        "framerate=(fraction)[0/1, 240/1]")
    );

G_DEFINE_TYPE_WITH_PRIVATE (GstNvmmConvert, gst_nvmm_convert,
    GST_TYPE_BASE_TRANSFORM);

static void
gst_nvmm_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvmmConvert *self = GST_NVMM_CONVERT (object);

  switch (prop_id) {
    case PROP_CROP_X:
      self->priv->crop_x = g_value_get_uint (value);
      break;
    case PROP_CROP_Y:
      self->priv->crop_y = g_value_get_uint (value);
      break;
    case PROP_CROP_W:
      self->priv->crop_w = g_value_get_uint (value);
      break;
    case PROP_CROP_H:
      self->priv->crop_h = g_value_get_uint (value);
      break;
    case PROP_FLIP_METHOD:
      self->priv->flip_method = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nvmm_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstNvmmConvert *self = GST_NVMM_CONVERT (object);

  switch (prop_id) {
    case PROP_CROP_X:
      g_value_set_uint (value, self->priv->crop_x);
      break;
    case PROP_CROP_Y:
      g_value_set_uint (value, self->priv->crop_y);
      break;
    case PROP_CROP_W:
      g_value_set_uint (value, self->priv->crop_w);
      break;
    case PROP_CROP_H:
      g_value_set_uint (value, self->priv->crop_h);
      break;
    case PROP_FLIP_METHOD:
      g_value_set_int (value, self->priv->flip_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_nvmm_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result;

  (void) trans;
  (void) direction;
  (void) caps;

  /* We can transform to any supported NVMM format/resolution */
  result = gst_static_pad_template_get_caps (&src_template);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect_full (result, filter,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (result);
    result = tmp;
  }

  return result;
}

static gboolean
gst_nvmm_convert_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps)
{
  GstNvmmConvert *self = GST_NVMM_CONVERT (trans);

  if (!gst_video_info_from_caps (&self->priv->sink_info, incaps)) {
    GST_ERROR_OBJECT (self, "Failed to parse sink caps");
    return FALSE;
  }
  if (!gst_video_info_from_caps (&self->priv->src_info, outcaps)) {
    GST_ERROR_OBJECT (self, "Failed to parse src caps");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Configured: %dx%d -> %dx%d",
      GST_VIDEO_INFO_WIDTH (&self->priv->sink_info),
      GST_VIDEO_INFO_HEIGHT (&self->priv->sink_info),
      GST_VIDEO_INFO_WIDTH (&self->priv->src_info),
      GST_VIDEO_INFO_HEIGHT (&self->priv->src_info));

  return TRUE;
}

static GstFlowReturn
gst_nvmm_convert_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstNvmmConvert *self = GST_NVMM_CONVERT (trans);
  GstMemory *in_mem, *out_mem;
  NvBufSurface *src_surface, *dst_surface;
  NvBufSurfTransformParams xform_params = { 0 };
  NvBufSurfTransformRect src_rect = { 0 };
  NvBufSurfTransformRect dst_rect = { 0 };
  int ret;

  in_mem = gst_buffer_peek_memory (inbuf, 0);
  out_mem = gst_buffer_peek_memory (outbuf, 0);

  if (!gst_is_nvmm_memory (in_mem) || !gst_is_nvmm_memory (out_mem)) {
    GST_ERROR_OBJECT (self, "Input/output must be NVMM memory");
    return GST_FLOW_ERROR;
  }

  src_surface = (NvBufSurface *) gst_nvmm_memory_get_surface (in_mem);
  dst_surface = (NvBufSurface *) gst_nvmm_memory_get_surface (out_mem);

  if (!src_surface || !dst_surface) {
    GST_ERROR_OBJECT (self, "Failed to get NvBufSurface from memory");
    return GST_FLOW_ERROR;
  }

  xform_params.transform_flag = 0;
  xform_params.flip = (NvBufSurfTransform_Flip) self->priv->flip_method;

  if (self->priv->crop_w > 0 && self->priv->crop_h > 0) {
    src_rect.top = self->priv->crop_y;
    src_rect.left = self->priv->crop_x;
    src_rect.width = self->priv->crop_w;
    src_rect.height = self->priv->crop_h;
    xform_params.src_rect = &src_rect;
    xform_params.transform_flag |= 1;  /* NVBUFSURF_TRANSFORM_CROP_SRC */
  }

  if (self->priv->flip_method != 0) {
    xform_params.transform_flag |= 4;  /* NVBUFSURF_TRANSFORM_FLIP */
  }

  ret = NvBufSurfTransform (src_surface, dst_surface, &xform_params);
  if (ret != 0) {
    GST_ERROR_OBJECT (self, "NvBufSurfTransform failed: %d", ret);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
gst_nvmm_convert_class_init (GstNvmmConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_nvmm_convert_set_property;
  gobject_class->get_property = gst_nvmm_convert_get_property;

  g_object_class_install_property (gobject_class, PROP_CROP_X,
      g_param_spec_uint ("crop-x", "Crop X", "Source crop X offset",
          0, 8192, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_Y,
      g_param_spec_uint ("crop-y", "Crop Y", "Source crop Y offset",
          0, 8192, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_W,
      g_param_spec_uint ("crop-w", "Crop Width",
          "Source crop width (0 = full width)",
          0, 8192, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CROP_H,
      g_param_spec_uint ("crop-h", "Crop Height",
          "Source crop height (0 = full height)",
          0, 8192, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FLIP_METHOD,
      g_param_spec_int ("flip-method", "Flip Method",
          "Video flip method (0=none, 1=90CW, 2=180, 3=90CCW, "
          "4=flipH, 5=transpose, 6=flipV, 7=inv-transpose)",
          0, 7, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "NVMM Video Converter",
      "Filter/Converter/Video",
      "Crop, scale, and convert video using Tegra VIC (NvBufSurfTransform)",
      "Pavel Guzenfeld");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_nvmm_convert_transform_caps);
  transform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_nvmm_convert_set_caps);
  transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_nvmm_convert_transform);

  GST_DEBUG_CATEGORY_INIT (gst_nvmm_convert_debug, "nvmmconvert", 0,
      "NVMM video converter");
}

static void
gst_nvmm_convert_init (GstNvmmConvert * self)
{
  self->priv = gst_nvmm_convert_get_instance_private (self);
  self->priv->crop_x = 0;
  self->priv->crop_y = 0;
  self->priv->crop_w = 0;
  self->priv->crop_h = 0;
  self->priv->flip_method = 0;
}
