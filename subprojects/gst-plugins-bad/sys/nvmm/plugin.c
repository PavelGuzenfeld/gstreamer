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
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstnvmmallocator.h"
#include "gstnvmmconvert.h"

#ifndef PACKAGE
#define PACKAGE "gst-plugins-bad"
#endif

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = TRUE;

  /* Register allocator */
  GstAllocator *alloc = gst_nvmm_allocator_new (GST_NVMM_MEM_DEFAULT);
  gst_allocator_register (GST_NVMM_MEMORY_TYPE_NAME, alloc);

  /* Register elements */
  ret &= gst_element_register (plugin, "nvmmconvert", GST_RANK_NONE,
      GST_TYPE_NVMM_CONVERT);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    nvmm,
    "NVMM elements for Jetson NvBufSurface zero-copy video processing",
    plugin_init,
    PACKAGE_VERSION,
    "LGPL",
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
