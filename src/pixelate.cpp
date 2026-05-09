#include <cstring>
#include <iostream>

extern "C" {
#include <glib.h>

#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvdsmeta.h"
}

#include "pixelate.h"

static bool session_initialized = false;

static void
ensure_transform_session(guint gpu_id)
{
    if (session_initialized)
        return;

    NvBufSurfTransformConfigParams config = {};
    config.compute_mode = NvBufSurfTransformCompute_Default;
    config.gpu_id = gpu_id;

    if (NvBufSurfTransformSetSessionParams(&config) != 0) {
        std::cerr << "Failed to set NvBufSurfTransform session\n";
    }

    session_initialized = true;
}

static bool
get_valid_roi(NvBufSurfaceParams *surf,
              NvDsObjectMeta *obj,
              int *x,
              int *y,
              int *w,
              int *h)
{
    *x = static_cast<int>(MAX(0, obj->rect_params.left));
    *y = static_cast<int>(MAX(0, obj->rect_params.top));

    *w = static_cast<int>(
        MIN(surf->width - *x,
            obj->rect_params.width));

    *h = static_cast<int>(
        MIN(surf->height - *y,
            obj->rect_params.height));

    return (*w >= 32 && *h >= 32);
}

static void
pixelate_roi(NvBufSurface *surface,
             int batch_id,
             int x,
             int y,
             int w,
             int h)
{
    constexpr int scale_factor = 8;

    int small_w = (w / scale_factor);
    int small_h = (h / scale_factor);

    small_w = MAX(16, (small_w / 4) * 4);
    small_h = MAX(16, (small_h / 4) * 4);

    NvBufSurfaceParams *in_params =
        &surface->surfaceList[batch_id];

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
    ip_surf.surfaceList =
        &surface->surfaceList[batch_id];

    NvBufSurfTransformParams params = {};

    params.transform_flag =
        NVBUFSURF_TRANSFORM_FILTER |
        NVBUFSURF_TRANSFORM_CROP_SRC |
        NVBUFSURF_TRANSFORM_CROP_DST;

    params.transform_filter =
        NvBufSurfTransformInter_Nearest;

    NvBufSurfTransformRect src_rect = {
        static_cast<uint32_t>(y),
        static_cast<uint32_t>(x),
        static_cast<uint32_t>(w),
        static_cast<uint32_t>(h)
    };

    NvBufSurfTransformRect dst_rect = {
        0,
        0,
        static_cast<uint32_t>(small_w),
        static_cast<uint32_t>(small_h)
    };

    // Downscale
    params.src_rect = &src_rect;
    params.dst_rect = &dst_rect;

    NvBufSurfTransform_Error err =
        NvBufSurfTransform(&ip_surf,
                           tmp_surf,
                           &params);

    if (err != NvBufSurfTransformError_Success) {
        std::cerr << "Downscale failed: "
                  << err << "\n";

        NvBufSurfaceDestroy(tmp_surf);
        return;
    }

    // Upscale
    params.src_rect = &dst_rect;
    params.dst_rect = &src_rect;

    err = NvBufSurfTransform(tmp_surf,
                             &ip_surf,
                             &params);

    if (err != NvBufSurfTransformError_Success) {
        std::cerr << "Upscale failed: "
                  << err << "\n";
    }

    NvBufSurfaceDestroy(tmp_surf);
}

void
process_frame(NvBufSurface *surface,
              NvDsFrameMeta *frame_meta,
              int class_id,
              bool roi_enabled,
              const ROI& clear_roi)
{
    int batch_id = frame_meta->batch_id;

    NvBufSurfaceParams *surf_params =
        &surface->surfaceList[batch_id];

    ensure_transform_session(surface->gpuId);

    for (NvDsMetaList *l_obj =
            frame_meta->obj_meta_list;
         l_obj != nullptr;
         l_obj = l_obj->next) {

        auto *obj =
            static_cast<NvDsObjectMeta *>(l_obj->data);

        if (obj->class_id != class_id)
            continue;

        int x, y, w, h;

        if (!get_valid_roi(surf_params,
                           obj,
                           &x, &y, &w, &h))
            continue;

        // Object bounds
        int obj_left   = x;
        int obj_top    = y;
        int obj_right  = x + w;
        int obj_bottom = y + h;

        if (roi_enabled) {
            // ROI bounds
            int roi_left   = clear_roi.x;
            int roi_top    = clear_roi.y;
            int roi_right  = clear_roi.x + clear_roi.w;
            int roi_bottom = clear_roi.y + clear_roi.h;

            // Check intersection
            bool intersects =
                !(obj_right  < roi_left  ||
                obj_left   > roi_right ||
                obj_bottom < roi_top   ||
                obj_top    > roi_bottom);

            // Inside ROI -> don't blur
            if (intersects)
                continue;
        }

        pixelate_roi(surface,
                     batch_id,
                     x, y, w, h);
    }
}