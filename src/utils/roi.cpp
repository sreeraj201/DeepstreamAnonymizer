#include "utils/roi.h"

bool object_intersects_roi(NvDsObjectMeta *obj, const ROI &roi) {
    int obj_left = static_cast<int>(obj->rect_params.left);
    int obj_top = static_cast<int>(obj->rect_params.top);
    int obj_right = obj_left + static_cast<int>(obj->rect_params.width);
    int obj_bottom = obj_top + static_cast<int>(obj->rect_params.height);

    int roi_left = roi.x;
    int roi_top = roi.y;
    int roi_right = roi.x + roi.w;
    int roi_bottom = roi.y + roi.h;

    return !(obj_right < roi_left || obj_left > roi_right ||
             obj_bottom < roi_top || obj_top > roi_bottom);
}