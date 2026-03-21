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

#pragma once

#include <gst/gst.h>
#include <gst/gstallocator.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_NVMM_ALLOCATOR             (gst_nvmm_allocator_get_type())
#define GST_NVMM_ALLOCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NVMM_ALLOCATOR,GstNvmmAllocator))
#define GST_NVMM_ALLOCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NVMM_ALLOCATOR,GstNvmmAllocatorClass))
#define GST_NVMM_ALLOCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_NVMM_ALLOCATOR,GstNvmmAllocatorClass))
#define GST_IS_NVMM_ALLOCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NVMM_ALLOCATOR))
#define GST_IS_NVMM_ALLOCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NVMM_ALLOCATOR))

#define GST_NVMM_ALLOCATOR_CAST(obj)        ((GstNvmmAllocator *)(obj))
#define GST_NVMM_MEMORY_CAST(mem)           ((GstNvmmMemory *)(mem))

/**
 * GST_NVMM_MEMORY_TYPE_NAME:
 *
 * Name of the NVMM memory type
 */
#define GST_NVMM_MEMORY_TYPE_NAME "gst.nvmm.memory"

/**
 * GST_CAPS_FEATURE_MEMORY_NVMM:
 *
 * Name of the caps feature for indicating the use of #GstNvmmMemory
 */
#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

typedef struct _GstNvmmMemory GstNvmmMemory;
typedef struct _GstNvmmMemoryPrivate GstNvmmMemoryPrivate;

typedef struct _GstNvmmAllocator GstNvmmAllocator;
typedef struct _GstNvmmAllocatorClass GstNvmmAllocatorClass;
typedef struct _GstNvmmAllocatorPrivate GstNvmmAllocatorPrivate;

/**
 * GstNvmmMemType:
 *
 * NVMM buffer memory types matching NvBufSurfaceMemType
 */
typedef enum
{
  GST_NVMM_MEM_DEFAULT = 0,
  GST_NVMM_MEM_CUDA_DEVICE,
  GST_NVMM_MEM_CUDA_PINNED,
  GST_NVMM_MEM_CUDA_UNIFIED,
  GST_NVMM_MEM_SURFACE_ARRAY,
  GST_NVMM_MEM_HANDLE,
  GST_NVMM_MEM_SYSTEM,
} GstNvmmMemType;

/**
 * GstNvmmMemory:
 *
 * GstMemory subclass backed by NvBufSurface
 */
struct _GstNvmmMemory
{
  GstMemory mem;

  /*< public >*/
  GstVideoInfo info;

  /*< private >*/
  GstNvmmMemoryPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstNvmmAllocator:
 *
 * A #GstAllocator subclass for Jetson NVMM (NvBufSurface) memory
 */
struct _GstNvmmAllocator
{
  GstAllocator parent;

  /*< private >*/
  GstNvmmAllocatorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

struct _GstNvmmAllocatorClass
{
  GstAllocatorClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GType           gst_nvmm_allocator_get_type (void);

GstAllocator *  gst_nvmm_allocator_new      (GstNvmmMemType mem_type);

GstMemory *     gst_nvmm_allocator_alloc     (GstNvmmAllocator * allocator,
                                               const GstVideoInfo * info);

gboolean        gst_is_nvmm_memory          (GstMemory * mem);

/**
 * gst_nvmm_memory_get_surface:
 * @mem: a #GstNvmmMemory
 *
 * Returns the underlying NvBufSurface pointer.
 * The caller does not own the returned pointer.
 *
 * Returns: (transfer none) (nullable): the NvBufSurface pointer
 */
gpointer        gst_nvmm_memory_get_surface (GstMemory * mem);

/**
 * gst_nvmm_memory_get_fd:
 * @mem: a #GstNvmmMemory
 * @fd: (out): the DMA-buf file descriptor
 *
 * Export the NVMM buffer as a DMA-buf fd for V4L2/display interop.
 *
 * Returns: %TRUE on success
 */
gboolean        gst_nvmm_memory_get_fd      (GstMemory * mem,
                                               gint * fd);

G_END_DECLS
