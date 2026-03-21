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
 * SECTION:gstnvmmallocator
 * @title: GstNvmmAllocator
 * @short_description: GStreamer allocator for Jetson NVMM (NvBufSurface) memory
 *
 * #GstNvmmAllocator is a #GstAllocator subclass that allocates memory using
 * NVIDIA's NvBufSurface API on Jetson platforms (Xavier, Orin). This enables
 * zero-copy video processing pipelines using the Tegra VIC hardware engine.
 *
 * The allocator handles:
 * - Allocation via NvBufSurfaceCreate with configurable memory type
 * - CPU map/unmap via NvBufSurfaceMap/NvBufSurfaceUnMap
 * - DMA-buf fd export via NvBufSurfaceGetFd for V4L2 interop
 * - Proper cleanup via NvBufSurfaceDestroy
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvmmallocator.h"

#ifdef NVMM_MOCK_API
#include "nvbufsurface-stub.h"
#else
#include "nvbufsurface.h"
#endif

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_nvmm_allocator_debug);
#define GST_CAT_DEFAULT gst_nvmm_allocator_debug

/* --- Private data --- */

struct _GstNvmmMemoryPrivate
{
  NvBufSurface *surface;
  gboolean mapped;
};

struct _GstNvmmAllocatorPrivate
{
  GstNvmmMemType mem_type;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstNvmmAllocator, gst_nvmm_allocator,
    GST_TYPE_ALLOCATOR);

/* --- GstAllocator virtual methods --- */

static GstMemory *
gst_nvmm_allocator_alloc_impl (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  /* The raw alloc path is not recommended — callers should use
   * gst_nvmm_allocator_alloc() with GstVideoInfo instead.
   * This fallback creates a 1920x1080 NV12 surface. */
  GstVideoInfo info;

  (void) params;

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12,
      (size > 0) ? 1920 : 1920, (size > 0) ? 1080 : 1080);

  return gst_nvmm_allocator_alloc (GST_NVMM_ALLOCATOR (allocator), &info);
}

static void
gst_nvmm_allocator_free_impl (GstAllocator * allocator, GstMemory * memory)
{
  GstNvmmMemory *nvmem = GST_NVMM_MEMORY_CAST (memory);

  (void) allocator;

  if (nvmem->priv) {
    if (nvmem->priv->surface) {
      if (nvmem->priv->mapped)
        NvBufSurfaceUnMap (nvmem->priv->surface, 0, -1);
      NvBufSurfaceDestroy (nvmem->priv->surface);
      nvmem->priv->surface = NULL;
    }
    g_free (nvmem->priv);
    nvmem->priv = NULL;
  }

  g_free (nvmem);
}

static gpointer
gst_nvmm_allocator_mem_map (GstMemory * memory, gsize maxsize,
    GstMapFlags flags)
{
  GstNvmmMemory *nvmem = GST_NVMM_MEMORY_CAST (memory);
  NvBufSurface *surface;
  int map_type;
  int ret;

  (void) maxsize;

  if (!nvmem->priv || !nvmem->priv->surface) {
    GST_ERROR ("Cannot map: NULL surface");
    return NULL;
  }

  surface = nvmem->priv->surface;
  map_type = (flags & GST_MAP_WRITE) ? 1 : 0;    /* NVBUF_MAP_READ_WRITE : NVBUF_MAP_READ */

  ret = NvBufSurfaceMap (surface, 0, -1, map_type);
  if (ret != 0) {
    GST_ERROR ("NvBufSurfaceMap failed: %d", ret);
    return NULL;
  }

  nvmem->priv->mapped = TRUE;
  return surface->surfaceList[0].mappedAddr;
}

static void
gst_nvmm_allocator_mem_unmap (GstMemory * memory)
{
  GstNvmmMemory *nvmem = GST_NVMM_MEMORY_CAST (memory);

  if (nvmem->priv && nvmem->priv->surface && nvmem->priv->mapped) {
    NvBufSurfaceUnMap (nvmem->priv->surface, 0, -1);
    nvmem->priv->mapped = FALSE;
  }
}

/* --- GObject lifecycle --- */

static void
gst_nvmm_allocator_class_init (GstNvmmAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = gst_nvmm_allocator_alloc_impl;
  allocator_class->free = gst_nvmm_allocator_free_impl;

  GST_DEBUG_CATEGORY_INIT (gst_nvmm_allocator_debug, "nvmmallocator", 0,
      "NVMM allocator for Jetson NvBufSurface");
}

static void
gst_nvmm_allocator_init (GstNvmmAllocator * self)
{
  GstAllocator *alloc = GST_ALLOCATOR (self);

  self->priv = gst_nvmm_allocator_get_instance_private (self);
  self->priv->mem_type = GST_NVMM_MEM_SURFACE_ARRAY;

  alloc->mem_type = GST_NVMM_MEMORY_TYPE_NAME;
  alloc->mem_map = gst_nvmm_allocator_mem_map;
  alloc->mem_unmap = gst_nvmm_allocator_mem_unmap;

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

/* --- Public API --- */

/**
 * gst_nvmm_allocator_new:
 * @mem_type: the NVMM memory type to use for allocations
 *
 * Creates a new #GstNvmmAllocator.
 *
 * Returns: (transfer full): a new #GstAllocator
 */
GstAllocator *
gst_nvmm_allocator_new (GstNvmmMemType mem_type)
{
  GstNvmmAllocator *alloc;

  alloc = g_object_new (GST_TYPE_NVMM_ALLOCATOR, NULL);
  alloc->priv->mem_type = mem_type;

  return GST_ALLOCATOR (alloc);
}

static NvBufSurfaceColorFormat
gst_video_format_to_nvmm (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      return NVBUF_COLOR_FORMAT_NV12;
    case GST_VIDEO_FORMAT_RGBA:
      return NVBUF_COLOR_FORMAT_RGBA;
    case GST_VIDEO_FORMAT_BGRA:
      return NVBUF_COLOR_FORMAT_BGRA;
    case GST_VIDEO_FORMAT_I420:
      return NVBUF_COLOR_FORMAT_I420;
    case GST_VIDEO_FORMAT_NV21:
      return NVBUF_COLOR_FORMAT_NV21;
    case GST_VIDEO_FORMAT_GRAY8:
      return NVBUF_COLOR_FORMAT_GRAY8;
    default:
      GST_WARNING ("Unsupported video format %s, falling back to NV12",
          gst_video_format_to_string (format));
      return NVBUF_COLOR_FORMAT_NV12;
  }
}

/**
 * gst_nvmm_allocator_alloc:
 * @allocator: a #GstNvmmAllocator
 * @info: the #GstVideoInfo describing the desired buffer
 *
 * Allocates an NVMM buffer matching the video format in @info.
 *
 * Returns: (transfer full) (nullable): a new #GstMemory or %NULL on failure
 */
GstMemory *
gst_nvmm_allocator_alloc (GstNvmmAllocator * allocator,
    const GstVideoInfo * info)
{
  NvBufSurfaceCreateParams create_params = { 0 };
  NvBufSurface *surface = NULL;
  GstNvmmMemory *nvmem;
  gsize total_size;
  int ret;

  g_return_val_if_fail (GST_IS_NVMM_ALLOCATOR (allocator), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  create_params.width = GST_VIDEO_INFO_WIDTH (info);
  create_params.height = GST_VIDEO_INFO_HEIGHT (info);
  create_params.colorFormat = gst_video_format_to_nvmm (
      GST_VIDEO_INFO_FORMAT (info));
  create_params.memType = (NvBufSurfaceMemType) allocator->priv->mem_type;
  create_params.layout = 0;        /* pitch linear */
  create_params.isContiguous = 1;

  ret = NvBufSurfaceCreate (&surface, 1, &create_params);
  if (ret != 0 || !surface) {
    GST_ERROR_OBJECT (allocator, "NvBufSurfaceCreate failed: %d", ret);
    return NULL;
  }

  total_size = surface->surfaceList[0].dataSize;

  nvmem = g_new0 (GstNvmmMemory, 1);
  nvmem->priv = g_new0 (GstNvmmMemoryPrivate, 1);
  nvmem->priv->surface = surface;
  nvmem->priv->mapped = FALSE;
  nvmem->info = *info;

  gst_memory_init (GST_MEMORY_CAST (nvmem), GST_MEMORY_FLAG_NO_SHARE,
      GST_ALLOCATOR (allocator), NULL, total_size, 0, 0, total_size);

  GST_DEBUG_OBJECT (allocator,
      "Allocated NVMM buffer %ux%u format=%d size=%" G_GSIZE_FORMAT,
      create_params.width, create_params.height,
      create_params.colorFormat, total_size);

  return GST_MEMORY_CAST (nvmem);
}

/**
 * gst_is_nvmm_memory:
 * @mem: a #GstMemory
 *
 * Check if @mem was allocated by a #GstNvmmAllocator.
 *
 * Returns: %TRUE if @mem is NVMM memory
 */
gboolean
gst_is_nvmm_memory (GstMemory * mem)
{
  return mem && mem->allocator
      && g_strcmp0 (mem->allocator->mem_type, GST_NVMM_MEMORY_TYPE_NAME) == 0;
}

/**
 * gst_nvmm_memory_get_surface:
 * @mem: a #GstMemory allocated by #GstNvmmAllocator
 *
 * Returns: (transfer none) (nullable): the NvBufSurface pointer
 */
gpointer
gst_nvmm_memory_get_surface (GstMemory * mem)
{
  GstNvmmMemory *nvmem;

  if (!gst_is_nvmm_memory (mem))
    return NULL;

  nvmem = GST_NVMM_MEMORY_CAST (mem);
  return nvmem->priv ? nvmem->priv->surface : NULL;
}

/**
 * gst_nvmm_memory_get_fd:
 * @mem: a #GstMemory allocated by #GstNvmmAllocator
 * @fd: (out): the DMA-buf fd
 *
 * Returns: %TRUE on success
 */
gboolean
gst_nvmm_memory_get_fd (GstMemory * mem, gint * fd)
{
  GstNvmmMemory *nvmem;
  int ret;

  g_return_val_if_fail (fd != NULL, FALSE);

  if (!gst_is_nvmm_memory (mem))
    return FALSE;

  nvmem = GST_NVMM_MEMORY_CAST (mem);
  if (!nvmem->priv || !nvmem->priv->surface)
    return FALSE;

  ret = NvBufSurfaceGetFd (nvmem->priv->surface, 0, fd);
  if (ret != 0) {
    GST_WARNING ("NvBufSurfaceGetFd failed: %d", ret);
    return FALSE;
  }

  return TRUE;
}
