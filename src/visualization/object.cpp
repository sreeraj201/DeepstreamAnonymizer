#include "visualization/object.h"

#include <gst/gst.h>

void set_object_label(NvDsObjectMeta *obj_meta, const std::string &label) {
    if (!obj_meta)
        return;

    if (obj_meta->text_params.display_text)
        g_free(obj_meta->text_params.display_text);

    obj_meta->text_params.display_text = g_strdup(label.c_str());

    obj_meta->text_params.x_offset = obj_meta->rect_params.left;

    obj_meta->text_params.y_offset = obj_meta->rect_params.top - 10;

    obj_meta->text_params.font_params.font_name = (gchar *)"Serif";

    obj_meta->text_params.font_params.font_size = 16;

    obj_meta->text_params.font_params.font_color = {1.0, 1.0, 1.0, 1.0};

    obj_meta->text_params.set_bg_clr = 1;

    obj_meta->text_params.text_bg_clr = {0.0, 0.0, 0.0, 1.0};
}

void set_match_style(NvDsObjectMeta *obj_meta) {
    if (!obj_meta)
        return;

    obj_meta->rect_params.border_width = 4;

    obj_meta->rect_params.border_color = {0.0, 1.0, 0.0, 1.0};
}

void set_unknown_style(NvDsObjectMeta *obj_meta) {
    if (!obj_meta)
        return;

    obj_meta->rect_params.border_width = 4;

    obj_meta->rect_params.border_color = {1.0, 0.0, 0.0, 1.0};
}