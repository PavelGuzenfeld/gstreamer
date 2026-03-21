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
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_NVMM_CONVERT             (gst_nvmm_convert_get_type())
#define GST_NVMM_CONVERT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NVMM_CONVERT,GstNvmmConvert))
#define GST_NVMM_CONVERT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NVMM_CONVERT,GstNvmmConvertClass))
#define GST_IS_NVMM_CONVERT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NVMM_CONVERT))
#define GST_IS_NVMM_CONVERT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NVMM_CONVERT))

typedef struct _GstNvmmConvert GstNvmmConvert;
typedef struct _GstNvmmConvertClass GstNvmmConvertClass;
typedef struct _GstNvmmConvertPrivate GstNvmmConvertPrivate;

struct _GstNvmmConvert
{
  GstBaseTransform parent;

  /*< private >*/
  GstNvmmConvertPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

struct _GstNvmmConvertClass
{
  GstBaseTransformClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_nvmm_convert_get_type (void);

G_END_DECLS
