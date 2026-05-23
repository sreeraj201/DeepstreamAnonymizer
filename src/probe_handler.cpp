#include "probe_handler.h"

#include <iostream>

#include "gstnvdsmeta.h"
#include "nvbufsurface.h"
#include "nvdsmeta_schema.h"

#include "pixelate.h"

#include "utils/label_formatter.h"
#include "utils/meta_iter.h"
#include "utils/roi.h"
#include "utils/tensor_meta.h"

#include "visualization/object.h"
#include "visualization/overlay.h"

namespace {

constexpr int FACE_CLASS_ID = 0;

constexpr int SGIE_UNIQUE_ID = 2;

constexpr int EMBEDDING_SIZE = 512;

constexpr uint64_t LOG_EVERY_N_FRAMES = 30;

} // namespace

GstPadProbeReturn ProbeHandler::buffer_probe(GstPad *, GstPadProbeInfo *info,
                                             gpointer user_data) {
    static uint64_t frame_count = 0;

    frame_count++;

    auto *context = static_cast<Context *>(user_data);

    if (!context || !context->config || !context->face_recognition) {
        return GST_PAD_PROBE_OK;
    }

    auto *config = context->config;

    auto *face_recognition = context->face_recognition;

    auto *buffer = reinterpret_cast<GstBuffer *>(info->data);

    auto *batch_meta = gst_buffer_get_nvds_batch_meta(buffer);

    if (!batch_meta)
        return GST_PAD_PROBE_OK;

    GstMapInfo in_map_info;

    if (!gst_buffer_map(buffer, &in_map_info, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }

    auto *surface = reinterpret_cast<NvBufSurface *>(in_map_info.data);

    if (!surface) {

        gst_buffer_unmap(buffer, &in_map_info);

        return GST_PAD_PROBE_OK;
    }

    for_each_frame(batch_meta, [&](NvDsFrameMeta *frame_meta) {
        if (config->roi_enabled) {
            draw_roi_overlay(batch_meta, frame_meta, config->clear_roi);
        }

        for_each_object(frame_meta, [&](NvDsObjectMeta *obj_meta) {
            if (obj_meta->class_id != FACE_CLASS_ID) {
                return;
            }

            bool inside_roi =
                !config->roi_enabled ||
                object_intersects_roi(obj_meta, config->clear_roi);

            if (!inside_roi) {
                pixelate_object(surface, frame_meta, obj_meta);

                set_unknown_style(obj_meta);

                auto label = build_face_label(obj_meta->object_id, "UNKNOWN",
                                              "", 0.0f, false);

                set_object_label(obj_meta, label);

                return;
            }

            auto *tensor_meta = get_tensor_meta(obj_meta, SGIE_UNIQUE_ID);

            if (!tensor_meta || !tensor_meta->out_buf_ptrs_host[0]) {
                if (frame_count % LOG_EVERY_N_FRAMES == 0) {
                    std::cout << "[WARNING] Missing tensor meta" << std::endl;
                }

                return;
            }

            auto *embedding =
                reinterpret_cast<float *>(tensor_meta->out_buf_ptrs_host[0]);

            auto result =
                face_recognition->recognize(embedding, EMBEDDING_SIZE);

            if (frame_count % LOG_EVERY_N_FRAMES == 0) {
                std::cout << "Match: " << result.name
                          << " Filename: " << result.image
                          << " Similarity: " << result.similarity << std::endl;
            }

            if (result.matched)
                set_match_style(obj_meta);
            else
                set_unknown_style(obj_meta);

            auto label =
                build_face_label(obj_meta->object_id, result.name, result.image,
                                 result.similarity, result.matched);

            set_object_label(obj_meta, label);
        });
    });

    gst_buffer_unmap(buffer, &in_map_info);

    return GST_PAD_PROBE_OK;
}