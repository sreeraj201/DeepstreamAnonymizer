#include "pixelate.h"

#include <string.h>
#include <glib.h>

#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvdsmeta.h"

static NvBufSurface *small_surf = NULL;


gboolean
init_scratch_buffer(NvBufSurface *surface)
{
    NvBufSurfaceCreateParams params = {0};
    params.gpuId = surface->gpuId;
    params.width = 256;
    params.height = 256;
    params.colorFormat = surface->surfaceList[0].colorFormat;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.memType = surface->memType;

    if (NvBufSurfaceCreate(&small_surf, 1, &params) != 0) {
        g_printerr("Failed to create scratch buffer\n");
        return FALSE;
    }
    return TRUE;
}

static gboolean
get_valid_roi(NvBufSurfaceParams *surf, NvDsObjectMeta *obj,
              int *x, int *y, int *w, int *h)
{
    *x = MAX(0, (int)obj->rect_params.left);
    *y = MAX(0, (int)obj->rect_params.top);
    *w = MIN((int)surf->width - *x, (int)obj->rect_params.width);
    *h = MIN((int)surf->height - *y, (int)obj->rect_params.height);

    return (*w > 16 && *h > 16);
}

static void
pixelate_roi(NvBufSurface *frame_surf,
              int x, int y, int w, int h)
{
    int small_w = MAX(16, w / 12);
    int small_h = MAX(16, h / 12);

    NvBufSurfTransformParams params = {0};
    params.transform_flag =
        NVBUFSURF_TRANSFORM_FILTER |
        NVBUFSURF_TRANSFORM_CROP_SRC |
        NVBUFSURF_TRANSFORM_CROP_DST;

    params.transform_filter = NvBufSurfTransformInter_Default;

    NvBufSurfTransformRect src_rect = {y, x, w, h};
    NvBufSurfTransformRect dst_rect = {0, 0, small_w, small_h};

    params.src_rect = &src_rect;
    params.dst_rect = &dst_rect;

    // Downscale
    NvBufSurfTransform(frame_surf, small_surf, &params);

    // Upscale
    params.src_rect = &dst_rect;
    params.dst_rect = &src_rect;

    NvBufSurfTransform(small_surf, frame_surf, &params);
}

void
process_frame(NvBufSurface *surface, NvDsFrameMeta *frame_meta, int class_id)
{
    NvBufSurfaceParams *surf_params =
        &surface->surfaceList[frame_meta->batch_id];

    NvBufSurface frame_surf;
    memcpy(&frame_surf, surface, sizeof(NvBufSurface));
    frame_surf.numFilled = 1;
    frame_surf.surfaceList = surf_params;

    for (NvDsMetaList *l_obj = frame_meta->obj_meta_list;
         l_obj != NULL; l_obj = l_obj->next) {

        NvDsObjectMeta *obj = (NvDsObjectMeta *)l_obj->data;

        if (obj->class_id != class_id)
            continue;

        int x, y, w, h;
        if (!get_valid_roi(surf_params, obj, &x, &y, &w, &h))
            continue;

        pixelate_roi(&frame_surf, x, y, w, h);
    }
}