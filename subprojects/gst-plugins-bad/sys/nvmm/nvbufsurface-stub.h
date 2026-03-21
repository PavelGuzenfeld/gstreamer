/* Mock NvBufSurface API for host-side compilation and testing.
 * On Jetson, the real headers from jetson_multimedia_api are used instead.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NVBUF_MAX_PLANES 4

typedef enum {
  NVBUF_MEM_DEFAULT = 0,
  NVBUF_MEM_CUDA_DEVICE,
  NVBUF_MEM_CUDA_PINNED,
  NVBUF_MEM_CUDA_UNIFIED,
  NVBUF_MEM_SURFACE_ARRAY,
  NVBUF_MEM_HANDLE,
  NVBUF_MEM_SYSTEM,
} NvBufSurfaceMemType;

typedef enum {
  NVBUF_COLOR_FORMAT_NV12 = 0,
  NVBUF_COLOR_FORMAT_RGBA,
  NVBUF_COLOR_FORMAT_BGRA,
  NVBUF_COLOR_FORMAT_I420,
  NVBUF_COLOR_FORMAT_NV21,
  NVBUF_COLOR_FORMAT_GRAY8,
} NvBufSurfaceColorFormat;

typedef enum {
  NvBufSurfTransform_None = 0,
  NvBufSurfTransform_Rotate90,
  NvBufSurfTransform_Rotate180,
  NvBufSurfTransform_Rotate270,
  NvBufSurfTransform_FlipX,
  NvBufSurfTransform_Transpose,
  NvBufSurfTransform_FlipY,
  NvBufSurfTransform_InvTranspose,
} NvBufSurfTransform_Flip;

typedef struct {
  uint32_t top;
  uint32_t left;
  uint32_t width;
  uint32_t height;
} NvBufSurfTransformRect;

typedef struct {
  NvBufSurfTransform_Flip flip;
  NvBufSurfTransformRect *src_rect;
  NvBufSurfTransformRect *dst_rect;
  uint32_t transform_flag;
} NvBufSurfTransformParams;

typedef struct {
  uint32_t num_planes;
  struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t offset;
    uint32_t psize;
    uint32_t bytesPerPix;
  } planeParams[NVBUF_MAX_PLANES];
} NvBufSurfacePlaneParamsEx;

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  NvBufSurfaceColorFormat colorFormat;
  uint32_t layout;
  uint64_t bufferDesc;
  uint32_t dataSize;
  void *dataPtr;
  void *mappedAddr;
  int dmabuf_fd;
  NvBufSurfacePlaneParamsEx planeParams;
} NvBufSurfaceParams;

typedef struct NvBufSurface {
  uint32_t batchSize;
  uint32_t numFilled;
  uint32_t isContiguous;
  NvBufSurfaceMemType memType;
  NvBufSurfaceParams *surfaceList;
} NvBufSurface;

typedef struct {
  uint32_t width;
  uint32_t height;
  NvBufSurfaceColorFormat colorFormat;
  NvBufSurfaceMemType memType;
  uint32_t size;
  uint32_t layout;
  uint32_t isContiguous;
} NvBufSurfaceCreateParams;

/* --- Helper functions --- */

static inline uint32_t
_nvmm_stub_plane_count (NvBufSurfaceColorFormat fmt)
{
  switch (fmt) {
    case NVBUF_COLOR_FORMAT_NV12:
    case NVBUF_COLOR_FORMAT_NV21:
      return 2;
    case NVBUF_COLOR_FORMAT_I420:
      return 3;
    default:
      return 1;
  }
}

static inline uint32_t
_nvmm_stub_bpp (NvBufSurfaceColorFormat fmt)
{
  switch (fmt) {
    case NVBUF_COLOR_FORMAT_RGBA:
    case NVBUF_COLOR_FORMAT_BGRA:
      return 4;
    default:
      return 1;
  }
}

/* --- Mock API implementations --- */

static inline int
NvBufSurfaceCreate (NvBufSurface ** surf, uint32_t batch_size,
    NvBufSurfaceCreateParams * params)
{
  NvBufSurface *s;
  uint32_t i;

  s = (NvBufSurface *) calloc (1, sizeof (NvBufSurface));
  if (!s)
    return -1;

  s->batchSize = batch_size;
  s->numFilled = batch_size;
  s->memType = params->memType;

  s->surfaceList =
      (NvBufSurfaceParams *) calloc (batch_size, sizeof (NvBufSurfaceParams));
  if (!s->surfaceList) {
    free (s);
    return -1;
  }

  for (i = 0; i < batch_size; i++) {
    NvBufSurfaceParams *p = &s->surfaceList[i];
    uint32_t nplanes, pl, total_size = 0;

    p->width = params->width;
    p->height = params->height;
    p->colorFormat = params->colorFormat;
    p->pitch = params->width * _nvmm_stub_bpp (params->colorFormat);
    p->dmabuf_fd = -1;

    nplanes = _nvmm_stub_plane_count (params->colorFormat);
    p->planeParams.num_planes = nplanes;

    for (pl = 0; pl < nplanes; pl++) {
      uint32_t pw = params->width;
      uint32_t ph = params->height;
      uint32_t bpp = _nvmm_stub_bpp (params->colorFormat);

      if (pl > 0
          && (params->colorFormat == NVBUF_COLOR_FORMAT_NV12
              || params->colorFormat == NVBUF_COLOR_FORMAT_NV21)) {
        ph /= 2;
      } else if (pl > 0 && params->colorFormat == NVBUF_COLOR_FORMAT_I420) {
        pw /= 2;
        ph /= 2;
      }

      p->planeParams.planeParams[pl].width = pw;
      p->planeParams.planeParams[pl].height = ph;
      p->planeParams.planeParams[pl].pitch = pw * bpp;
      p->planeParams.planeParams[pl].offset = total_size;
      p->planeParams.planeParams[pl].psize = pw * ph * bpp;
      p->planeParams.planeParams[pl].bytesPerPix = bpp;
      total_size += pw * ph * bpp;
    }

    p->dataSize = total_size;
    p->dataPtr = calloc (1, total_size);
    if (!p->dataPtr) {
      free (s->surfaceList);
      free (s);
      return -1;
    }
  }

  *surf = s;
  return 0;
}

static inline int
NvBufSurfaceDestroy (NvBufSurface * surf)
{
  uint32_t i;

  if (!surf)
    return -1;

  if (surf->surfaceList) {
    for (i = 0; i < surf->batchSize; i++) {
      free (surf->surfaceList[i].dataPtr);
      free (surf->surfaceList[i].mappedAddr);
    }
    free (surf->surfaceList);
  }
  free (surf);
  return 0;
}

static inline int
NvBufSurfaceMap (NvBufSurface * surf, int index, int plane, int type)
{
  NvBufSurfaceParams *p;
  (void) type;
  (void) plane;

  if (!surf || index < 0 || (uint32_t) index >= surf->batchSize)
    return -1;

  p = &surf->surfaceList[index];
  if (!p->mappedAddr) {
    p->mappedAddr = malloc (p->dataSize);
    if (!p->mappedAddr)
      return -1;
    memcpy (p->mappedAddr, p->dataPtr, p->dataSize);
  }
  return 0;
}

static inline int
NvBufSurfaceUnMap (NvBufSurface * surf, int index, int plane)
{
  NvBufSurfaceParams *p;
  (void) plane;

  if (!surf || index < 0 || (uint32_t) index >= surf->batchSize)
    return -1;

  p = &surf->surfaceList[index];
  if (p->mappedAddr) {
    memcpy (p->dataPtr, p->mappedAddr, p->dataSize);
    free (p->mappedAddr);
    p->mappedAddr = NULL;
  }
  return 0;
}

static inline int
NvBufSurfaceGetFd (NvBufSurface * surf, int index, int *fd)
{
  if (!surf || index < 0 || (uint32_t) index >= surf->batchSize || !fd)
    return -1;
  *fd = 42 + index;
  surf->surfaceList[index].dmabuf_fd = *fd;
  return 0;
}

static inline int
NvBufSurfTransform (NvBufSurface * src, NvBufSurface * dst,
    NvBufSurfTransformParams * params)
{
  uint32_t i, copy_size;

  if (!src || !dst || !params)
    return -1;

  for (i = 0; i < src->numFilled && i < dst->batchSize; i++) {
    copy_size = src->surfaceList[i].dataSize;
    if (dst->surfaceList[i].dataSize < copy_size)
      copy_size = dst->surfaceList[i].dataSize;
    memcpy (dst->surfaceList[i].dataPtr, src->surfaceList[i].dataPtr,
        copy_size);
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
