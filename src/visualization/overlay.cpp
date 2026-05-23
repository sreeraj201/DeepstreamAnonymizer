#include "visualization/overlay.h"

void draw_roi_overlay(NvDsBatchMeta *batch_meta, NvDsFrameMeta *frame_meta,
                      const ROI &roi) {
    NvDsDisplayMeta *display_meta =
        nvds_acquire_display_meta_from_pool(batch_meta);

    display_meta->num_rects = 1;

    NvOSD_RectParams &rect = display_meta->rect_params[0];

    rect.left = roi.x;
    rect.top = roi.y;
    rect.width = roi.w;
    rect.height = roi.h;

    rect.border_width = 4;

    rect.border_color.red = 0.0;
    rect.border_color.green = 0.0;
    rect.border_color.blue = 1.0;
    rect.border_color.alpha = 1.0;

    rect.has_bg_color = 0;

    nvds_add_display_meta_to_frame(frame_meta, display_meta);
}