#include "pixelate.h"

#include <iostream>

#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvdsmeta.h"
#include <glib.h>

#include "utils/roi.h"

namespace {

constexpr int MIN_SIZE = 32;

constexpr int SCALE_FACTOR = 32;

constexpr int MIN_PIXELATED_SIZE = 2;

constexpr int PIXEL_ALIGNMENT = 2;

bool session_initialized = false;

}

static void ensure_transform_session(guint gpu_id) {
    if (session_initialized)
        return;

    NvBufSurfTransformConfigParams config = {};

    config.compute_mode = NvBufSurfTransformCompute_GPU;

    config.gpu_id = gpu_id;

    if (NvBufSurfTransformSetSessionParams(&config) != 0) {
        std::cerr << "Failed to set NvBufSurfTransform session\n";
    }

    session_initialized = true;
}

static bool get_valid_roi(NvBufSurfaceParams *surf, NvDsObjectMeta *obj, int *x,
                          int *y, int *w, int *h) {
    *x = static_cast<int>(MAX(0, obj->rect_params.left));

    *y = static_cast<int>(MAX(0, obj->rect_params.top));

    *w = static_cast<int>(MIN(surf->width - *x, obj->rect_params.width));

    *h = static_cast<int>(MIN(surf->height - *y, obj->rect_params.height));

    return (*w >= MIN_SIZE && *h >= MIN_SIZE);
}

static void pixelate_roi(NvBufSurface *surface, int batch_id, int x, int y,
                         int w, int h) {
    int small_w = w / SCALE_FACTOR;

    int small_h = h / SCALE_FACTOR;

    small_w =
        MAX(MIN_PIXELATED_SIZE, (small_w / PIXEL_ALIGNMENT) * PIXEL_ALIGNMENT);

    small_h =
        MAX(MIN_PIXELATED_SIZE, (small_h / PIXEL_ALIGNMENT) * PIXEL_ALIGNMENT);

    auto *in_params = &surface->surfaceList[batch_id];

    NvBufSurfaceCreateParams cparams = {};

    cparams.width = small_w;
    cparams.height = small_h;
    cparams.memType = surface->memType;
    cparams.gpuId = surface->gpuId;
    cparams.colorFormat = in_params->colorFormat;

    cparams.layout = in_params->layout;

    NvBufSurface *tmp_surf = nullptr;

    if (NvBufSurfaceCreate(&tmp_surf, 1, &cparams) != 0) {
        std::cerr << "Failed to create temp surface\n";

        return;
    }

    NvBufSurface ip_surf = *surface;

    ip_surf.numFilled = 1;

    ip_surf.surfaceList = &surface->surfaceList[batch_id];

    NvBufSurfTransformParams params = {};

    params.transform_flag = NVBUFSURF_TRANSFORM_FILTER |
                            NVBUFSURF_TRANSFORM_CROP_SRC |
                            NVBUFSURF_TRANSFORM_CROP_DST;

    params.transform_filter = NvBufSurfTransformInter_Nearest;

    NvBufSurfTransformRect src_rect = {
        static_cast<uint32_t>(y), static_cast<uint32_t>(x),
        static_cast<uint32_t>(w), static_cast<uint32_t>(h)};

    NvBufSurfTransformRect dst_rect = {0, 0, static_cast<uint32_t>(small_w),
                                       static_cast<uint32_t>(small_h)};

    params.src_rect = &src_rect;
    params.dst_rect = &dst_rect;

    auto err = NvBufSurfTransform(&ip_surf, tmp_surf, &params);

    if (err != NvBufSurfTransformError_Success) {
        std::cerr << "Downscale failed: " << err << "\n";

        NvBufSurfaceDestroy(tmp_surf);

        return;
    }

    params.src_rect = &dst_rect;
    params.dst_rect = &src_rect;

    err = NvBufSurfTransform(tmp_surf, &ip_surf, &params);

    if (err != NvBufSurfTransformError_Success) {
        std::cerr << "Upscale failed: " << err << "\n";
    }

    NvBufSurfaceDestroy(tmp_surf);
}

void pixelate_object(NvBufSurface *surface, NvDsFrameMeta *frame_meta,
                     NvDsObjectMeta *obj_meta) {
    int batch_id = frame_meta->batch_id;

    auto *surf_params = &surface->surfaceList[batch_id];

    ensure_transform_session(surface->gpuId);

    int x, y, w, h;

    if (!get_valid_roi(surf_params, obj_meta, &x, &y, &w, &h)) {
        return;
    }

    pixelate_roi(surface, batch_id, x, y, w, h);
}