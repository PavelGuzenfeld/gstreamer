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

#ifndef __GST_NVMM_ALLOCATOR_H__
#define __GST_NVMM_ALLOCATOR_H__

#ifdef HAVE_CUDA_NVMM

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_NVMM_ALLOCATOR             (gst_nvmm_allocator_get_type())
#define GST_NVMM_ALLOCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NVMM_ALLOCATOR,GstNvmmAllocator))
#define GST_NVMM_ALLOCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_NVMM_ALLOCATOR,GstNvmmAllocatorClass))
#define GST_IS_NVMM_ALLOCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NVMM_ALLOCATOR))
#define GST_IS_NVMM_ALLOCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_NVMM_ALLOCATOR))

#define GST_NVMM_MEMORY_TYPE "nvmm"

typedef struct _GstNvmmAllocator GstNvmmAllocator;
typedef struct _GstNvmmAllocatorClass GstNvmmAllocatorClass;
typedef struct _GstNvmmAllocatorPrivate GstNvmmAllocatorPrivate;

struct _GstNvmmAllocator
{
  GstAllocator parent;

  GstNvmmAllocatorPrivate *priv;
};

struct _GstNvmmAllocatorClass
{
  GstAllocatorClass parent_class;
};

GType           gst_nvmm_allocator_get_type       (void);

GstAllocator *  gst_nvmm_allocator_new            (void);

GstMemory *     gst_nvmm_allocator_alloc_video    (GstAllocator * allocator,
                                                    GstVideoFormat format,
                                                    guint width,
                                                    guint height);

gboolean        gst_is_nvmm_memory                (GstMemory * mem);

void *          gst_nvmm_memory_get_surface        (GstMemory * mem);

G_END_DECLS

#endif /* HAVE_CUDA_NVMM */

#endif /* __GST_NVMM_ALLOCATOR_H__ */
