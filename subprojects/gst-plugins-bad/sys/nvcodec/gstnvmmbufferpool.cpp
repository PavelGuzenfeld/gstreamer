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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_CUDA_NVMM

#include "gstnvmmbufferpool.h"
#include "gstnvmmallocator.h"

#include <nvbufsurface.h>

GST_DEBUG_CATEGORY_STATIC (gst_nvmm_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_nvmm_buffer_pool_debug

struct _GstNvmmBufferPoolPrivate
{
  GstVideoInfo video_info;
  GstAllocator *allocator;
  gboolean configured;
};

#define gst_nvmm_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstNvmmBufferPool, gst_nvmm_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar **
gst_nvmm_buffer_pool_get_options (GstBufferPool * pool)
{
  (void) pool;
  static const gchar *options[] = {
    GST_BUFFER_POOL_OPTION_VIDEO_META,
    NULL
  };
  return options;
}

static gboolean
gst_nvmm_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstNvmmBufferPool *self = GST_NVMM_BUFFER_POOL (pool);
  GstCaps *caps = NULL;
  guint size, min_buffers, max_buffers;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size,
          &min_buffers, &max_buffers)) {
    GST_ERROR_OBJECT (pool, "Failed to get pool config params");
    return FALSE;
  }

  if (!caps) {
    GST_ERROR_OBJECT (pool, "No caps in pool config");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&self->priv->video_info, caps)) {
    GST_ERROR_OBJECT (pool, "Failed to parse caps");
    return FALSE;
  }

  /* Create our NVMM allocator */
  if (self->priv->allocator) {
    gst_object_unref (self->priv->allocator);
  }
  self->priv->allocator = gst_nvmm_allocator_new ();

  /* Update config with actual NVMM buffer size */
  gsize nvmm_size = GST_VIDEO_INFO_SIZE (&self->priv->video_info);
  gst_buffer_pool_config_set_params (config, caps, nvmm_size,
      min_buffers, max_buffers);

  self->priv->configured = TRUE;

  GST_INFO_OBJECT (pool, "Configured: %dx%d %s, %u-%u buffers",
      GST_VIDEO_INFO_WIDTH (&self->priv->video_info),
      GST_VIDEO_INFO_HEIGHT (&self->priv->video_info),
      gst_video_format_to_string (
          GST_VIDEO_INFO_FORMAT (&self->priv->video_info)),
      min_buffers, max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static GstFlowReturn
gst_nvmm_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  (void) params;
  GstNvmmBufferPool *self = GST_NVMM_BUFFER_POOL (pool);

  if (!self->priv->configured) {
    GST_ERROR_OBJECT (pool, "Pool not configured");
    return GST_FLOW_ERROR;
  }

  gint w = GST_VIDEO_INFO_WIDTH (&self->priv->video_info);
  gint h = GST_VIDEO_INFO_HEIGHT (&self->priv->video_info);
  GstVideoFormat fmt = GST_VIDEO_INFO_FORMAT (&self->priv->video_info);

  /* Allocate with exact format/dimensions */
  GstMemory *mem = gst_nvmm_allocator_alloc_video (self->priv->allocator,
      fmt, static_cast < guint > (w), static_cast < guint > (h));
  if (!mem) {
    GST_ERROR_OBJECT (pool, "Failed to allocate NVMM GstMemory");
    return GST_FLOW_ERROR;
  }

  /* Build the GstBuffer */
  *buffer = gst_buffer_new ();
  gst_buffer_append_memory (*buffer, mem);

  /* Read actual strides/offsets from the NVMM surface -- these may
   * differ from GstVideoInfo due to hardware alignment requirements */
  guint n_planes = GST_VIDEO_INFO_N_PLANES (&self->priv->video_info);
  gsize offsets[GST_VIDEO_MAX_PLANES] = {};
  gint strides[GST_VIDEO_MAX_PLANES] = {};

  void *surface = gst_nvmm_memory_get_surface (mem);
  if (surface) {
    NvBufSurface *nvsurf = static_cast < NvBufSurface * >(surface);
    NvBufSurfacePlaneParams *pp = &nvsurf->surfaceList[0].planeParams;
    for (guint i = 0; i < n_planes && i < pp->num_planes; i++) {
      offsets[i] = pp->offset[i];
      strides[i] = static_cast < gint > (pp->pitch[i]);
    }
  } else {
    /* Fallback to GstVideoInfo values */
    for (guint i = 0; i < n_planes; i++) {
      offsets[i] = self->priv->video_info.offset[i];
      strides[i] = self->priv->video_info.stride[i];
    }
  }

  gst_buffer_add_video_meta_full (*buffer, GST_VIDEO_FRAME_FLAG_NONE,
      fmt, w, h, n_planes, offsets, strides);

  return GST_FLOW_OK;
}

static void
gst_nvmm_buffer_pool_dispose (GObject * object)
{
  GstNvmmBufferPool *self = GST_NVMM_BUFFER_POOL (object);

  if (self->priv->allocator) {
    gst_object_unref (self->priv->allocator);
    self->priv->allocator = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_nvmm_buffer_pool_class_init (GstNvmmBufferPoolClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *pool_class = GST_BUFFER_POOL_CLASS (klass);

  gobject_class->dispose = gst_nvmm_buffer_pool_dispose;

  pool_class->get_options = gst_nvmm_buffer_pool_get_options;
  pool_class->set_config = gst_nvmm_buffer_pool_set_config;
  pool_class->alloc_buffer = gst_nvmm_buffer_pool_alloc_buffer;

  GST_DEBUG_CATEGORY_INIT (gst_nvmm_buffer_pool_debug, "nvmmbufferpool", 0,
      "NVMM Buffer Pool");
}

static void
gst_nvmm_buffer_pool_init (GstNvmmBufferPool * self)
{
  self->priv = static_cast < GstNvmmBufferPoolPrivate * >
      (gst_nvmm_buffer_pool_get_instance_private (self));
  self->priv->allocator = NULL;
  self->priv->configured = FALSE;
}

/* --- Public API --- */

/**
 * gst_nvmm_buffer_pool_new:
 *
 * Creates a new buffer pool that allocates NVMM (NvBufSurface) buffers.
 *
 * Returns: (transfer full): a new #GstBufferPool
 */
GstBufferPool *
gst_nvmm_buffer_pool_new (void)
{
  return GST_BUFFER_POOL (g_object_new (GST_TYPE_NVMM_BUFFER_POOL, NULL));
}

#endif /* HAVE_CUDA_NVMM */
