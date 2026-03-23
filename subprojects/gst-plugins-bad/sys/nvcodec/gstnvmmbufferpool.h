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

#ifndef __GST_NVMM_BUFFER_POOL_H__
#define __GST_NVMM_BUFFER_POOL_H__

#ifdef HAVE_CUDA_NVMM

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_NVMM_BUFFER_POOL             (gst_nvmm_buffer_pool_get_type())
#define GST_NVMM_BUFFER_POOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NVMM_BUFFER_POOL,GstNvmmBufferPool))
#define GST_NVMM_BUFFER_POOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_NVMM_BUFFER_POOL,GstNvmmBufferPoolClass))
#define GST_IS_NVMM_BUFFER_POOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NVMM_BUFFER_POOL))
#define GST_IS_NVMM_BUFFER_POOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_NVMM_BUFFER_POOL))

typedef struct _GstNvmmBufferPool GstNvmmBufferPool;
typedef struct _GstNvmmBufferPoolClass GstNvmmBufferPoolClass;
typedef struct _GstNvmmBufferPoolPrivate GstNvmmBufferPoolPrivate;

struct _GstNvmmBufferPool
{
  GstBufferPool parent;

  GstNvmmBufferPoolPrivate *priv;
};

struct _GstNvmmBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType           gst_nvmm_buffer_pool_get_type     (void);

GstBufferPool * gst_nvmm_buffer_pool_new          (void);

G_END_DECLS

#endif /* HAVE_CUDA_NVMM */

#endif /* __GST_NVMM_BUFFER_POOL_H__ */
