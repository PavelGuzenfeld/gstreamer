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

#include "gstnvmmallocator.h"

#include <gst/video/video.h>
#include <nvbufsurface.h>

#include <string>

GST_DEBUG_CATEGORY_STATIC (gst_nvmm_allocator_debug);
#define GST_CAT_DEFAULT gst_nvmm_allocator_debug

/* --- NvBufSurface color format conversions --- */

static NvBufSurfaceColorFormat
gst_video_format_to_nvbuf_color (GstVideoFormat fmt)
{
  switch (fmt) {
    case GST_VIDEO_FORMAT_NV12:
      return NVBUF_COLOR_FORMAT_NV12;
    case GST_VIDEO_FORMAT_RGBA:
      return NVBUF_COLOR_FORMAT_RGBA;
    case GST_VIDEO_FORMAT_BGRA:
      return NVBUF_COLOR_FORMAT_BGRA;
    case GST_VIDEO_FORMAT_I420:
      return NVBUF_COLOR_FORMAT_YUV420;
    case GST_VIDEO_FORMAT_NV21:
      return NVBUF_COLOR_FORMAT_NV21;
    default:
      return NVBUF_COLOR_FORMAT_NV12;
  }
}

/* --- Internal NVMM GstMemory subclass --- */

typedef struct _GstNvmmMemory GstNvmmMemory;

struct _GstNvmmMemory
{
  GstMemory parent;
  NvBufSurface *surface;
};

struct _GstNvmmAllocatorPrivate
{
  gint placeholder;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstNvmmAllocator, gst_nvmm_allocator,
    GST_TYPE_ALLOCATOR);

static GstMemory *
gst_nvmm_allocator_do_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  (void) params;

  /* Callers should prefer gst_nvmm_allocator_alloc_video() or the
   * buffer pool. This fallback guesses NV12 1920x1080. */
  GST_WARNING_OBJECT (allocator,
      "Allocating NVMM by size (%" G_GSIZE_FORMAT
      ") -- use gst_nvmm_allocator_alloc_video() instead", size);

  NvBufSurfaceCreateParams create_params = {};
  create_params.width = 1920;
  create_params.height = 1080;
  create_params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
  create_params.memType = NVBUF_MEM_SURFACE_ARRAY;
  create_params.layout = NVBUF_LAYOUT_PITCH;
  create_params.isContiguous = true;

  NvBufSurface *surface = nullptr;
  int ret = NvBufSurfaceCreate (&surface, 1, &create_params);
  if (ret != 0 || !surface) {
    GST_ERROR_OBJECT (allocator, "NvBufSurfaceCreate failed (%d)", ret);
    return nullptr;
  }

  surface->numFilled = surface->batchSize;

  GstNvmmMemory *mem = g_new0 (GstNvmmMemory, 1);
  gsize actual_size = static_cast < gsize > (surface->surfaceList[0].dataSize);

  gst_memory_init (GST_MEMORY_CAST (mem), GST_MEMORY_FLAG_NO_SHARE,
      allocator, nullptr, actual_size, 0, 0, actual_size);

  mem->surface = surface;

  return GST_MEMORY_CAST (mem);
}

static void
gst_nvmm_allocator_do_free (GstAllocator * allocator, GstMemory * memory)
{
  (void) allocator;
  GstNvmmMemory *mem = reinterpret_cast < GstNvmmMemory * >(memory);

  if (mem->surface) {
    NvBufSurfaceDestroy (mem->surface);
    mem->surface = nullptr;
  }

  g_free (mem);
}

static gpointer
gst_nvmm_allocator_mem_map (GstMemory * memory, gsize maxsize,
    GstMapFlags flags)
{
  (void) maxsize;
  (void) flags;

  /* NVIDIA convention: mapped data pointer IS the NvBufSurface*.
   * This is what NVIDIA elements (nvvidconv, nvv4l2decoder, etc.)
   * expect when they map NVMM memory. */
  GstNvmmMemory *mem = reinterpret_cast < GstNvmmMemory * >(memory);
  return mem->surface;
}

static void
gst_nvmm_allocator_mem_unmap (GstMemory * memory)
{
  (void) memory;
}

static void
gst_nvmm_allocator_class_init (GstNvmmAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = gst_nvmm_allocator_do_alloc;
  allocator_class->free = gst_nvmm_allocator_do_free;

  GST_DEBUG_CATEGORY_INIT (gst_nvmm_allocator_debug, "nvmmallocator", 0,
      "NVMM Memory Allocator");
}

static void
gst_nvmm_allocator_init (GstNvmmAllocator * self)
{
  GstAllocator *alloc = GST_ALLOCATOR (self);

  self->priv = static_cast < GstNvmmAllocatorPrivate * >
      (gst_nvmm_allocator_get_instance_private (self));

  alloc->mem_type = GST_NVMM_MEMORY_TYPE;
  alloc->mem_map = gst_nvmm_allocator_mem_map;
  alloc->mem_unmap = gst_nvmm_allocator_mem_unmap;

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

/* --- Public API --- */

/**
 * gst_nvmm_allocator_new:
 *
 * Creates a new NVMM allocator that allocates NvBufSurface memory
 * using NVBUF_MEM_SURFACE_ARRAY (Jetson hardware surfaces).
 *
 * Returns: (transfer full): a new #GstAllocator
 */
GstAllocator *
gst_nvmm_allocator_new (void)
{
  return GST_ALLOCATOR (g_object_new (GST_TYPE_NVMM_ALLOCATOR, nullptr));
}

/**
 * gst_nvmm_allocator_alloc_video:
 * @allocator: a #GstNvmmAllocator
 * @format: GStreamer video format
 * @width: frame width in pixels
 * @height: frame height in pixels
 *
 * Allocates NVMM memory with explicit video format and dimensions.
 * Preferred over gst_allocator_alloc() which has to guess dimensions.
 *
 * Returns: (transfer full) (nullable): a new #GstMemory or %NULL on failure
 */
GstMemory *
gst_nvmm_allocator_alloc_video (GstAllocator * allocator,
    GstVideoFormat format, guint width, guint height)
{
  g_return_val_if_fail (GST_IS_NVMM_ALLOCATOR (allocator), nullptr);
  g_return_val_if_fail (width > 0 && height > 0, nullptr);

  NvBufSurfaceCreateParams create_params = {};
  create_params.width = width;
  create_params.height = height;
  create_params.colorFormat = gst_video_format_to_nvbuf_color (format);
  create_params.memType = NVBUF_MEM_SURFACE_ARRAY;
  create_params.layout = NVBUF_LAYOUT_PITCH;
  create_params.isContiguous = true;

  NvBufSurface *surface = nullptr;
  int ret = NvBufSurfaceCreate (&surface, 1, &create_params);
  if (ret != 0 || !surface) {
    GST_ERROR_OBJECT (allocator,
        "NvBufSurfaceCreate failed for %ux%u %s (%d)",
        width, height, gst_video_format_to_string (format), ret);
    return nullptr;
  }

  surface->numFilled = surface->batchSize;

  GstNvmmMemory *mem = g_new0 (GstNvmmMemory, 1);
  gsize actual_size = static_cast < gsize > (surface->surfaceList[0].dataSize);

  gst_memory_init (GST_MEMORY_CAST (mem), GST_MEMORY_FLAG_NO_SHARE,
      allocator, nullptr, actual_size, 0, 0, actual_size);

  mem->surface = surface;

  return GST_MEMORY_CAST (mem);
}

/**
 * gst_is_nvmm_memory:
 * @mem: a #GstMemory
 *
 * Checks whether @mem was allocated by an NVMM allocator.
 *
 * Returns: %TRUE if the memory is NVMM memory
 */
gboolean
gst_is_nvmm_memory (GstMemory * mem)
{
  return mem && mem->allocator
      && g_strcmp0 (mem->allocator->mem_type, GST_NVMM_MEMORY_TYPE) == 0;
}

/**
 * gst_nvmm_memory_get_surface:
 * @mem: a #GstMemory allocated by a #GstNvmmAllocator
 *
 * Gets the NvBufSurface pointer from NVMM memory.
 *
 * Returns: (transfer none) (nullable): the NvBufSurface* or %NULL
 */
void *
gst_nvmm_memory_get_surface (GstMemory * mem)
{
  if (!gst_is_nvmm_memory (mem))
    return nullptr;

  GstNvmmMemory *nvmm_mem = reinterpret_cast < GstNvmmMemory * >(mem);
  return nvmm_mem->surface;
}

#endif /* HAVE_CUDA_NVMM */
