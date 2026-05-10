#include "probe_handler.h"

extern "C" {
#include "nvbufsurface.h"
#include "nvdsmeta_schema.h"
#include "gstnvdsmeta.h"
}

#include "config.h"
#include "pixelate.h"

namespace {

constexpr guint PGIE_CLASS_ID_PERSON = 0;

void draw_roi(
    NvDsBatchMeta* batch_meta,
    NvDsFrameMeta* frame_meta,
    const ROI& roi)
{
    auto* display_meta =
        nvds_acquire_display_meta_from_pool(batch_meta);

    display_meta->num_rects = 1;

    auto& rect =
        display_meta->rect_params[0];

    rect.left   = roi.x;
    rect.top    = roi.y;
    rect.width  = roi.w;
    rect.height = roi.h;

    rect.border_width = 4;

    rect.border_color.red   = 0.0;
    rect.border_color.green = 0.0;
    rect.border_color.blue  = 1.0;
    rect.border_color.alpha = 1.0;

    rect.has_bg_color = 1;

    rect.bg_color.red   = 0.0;
    rect.bg_color.green = 0.0;
    rect.bg_color.blue  = 1.0;
    rect.bg_color.alpha = 0.25;

    nvds_add_display_meta_to_frame(
        frame_meta,
        display_meta);
}

}

GstPadProbeReturn ProbeHandler::buffer_probe(
    GstPad*,
    GstPadProbeInfo* info,
    gpointer user_data)
{
    auto* config =
        static_cast<AppConfig*>(user_data);

    auto* buffer =
        reinterpret_cast<GstBuffer*>(info->data);

    auto* batch_meta =
        gst_buffer_get_nvds_batch_meta(buffer);

    if (!batch_meta)
        return GST_PAD_PROBE_OK;

    GstMapInfo map {};

    if (!gst_buffer_map(buffer, &map, GST_MAP_READWRITE))
        return GST_PAD_PROBE_OK;

    auto* surface =
        reinterpret_cast<NvBufSurface*>(map.data);

    for (NvDsMetaList* frame_list = batch_meta->frame_meta_list;
         frame_list != nullptr;
         frame_list = frame_list->next)
    {
        auto* frame_meta =
            reinterpret_cast<NvDsFrameMeta*>(frame_list->data);

        process_frame(
            surface,
            frame_meta,
            PGIE_CLASS_ID_PERSON,
            config->roi_enabled,
            config->clear_roi);

        if (config->roi_enabled)
            draw_roi(
                batch_meta,
                frame_meta,
                config->clear_roi);
    }

    gst_buffer_unmap(buffer, &map);

    return GST_PAD_PROBE_OK;
}