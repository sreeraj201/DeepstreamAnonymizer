#include "utils/meta_iter.h"

void for_each_frame(NvDsBatchMeta *batch_meta,
                    const std::function<void(NvDsFrameMeta *)> &callback) {
    for (NvDsMetaList *frame_list = batch_meta->frame_meta_list;
         frame_list != nullptr; frame_list = frame_list->next) {
        auto *frame_meta = reinterpret_cast<NvDsFrameMeta *>(frame_list->data);

        callback(frame_meta);
    }
}

void for_each_object(NvDsFrameMeta *frame_meta,
                     const std::function<void(NvDsObjectMeta *)> &callback) {
    for (NvDsMetaList *obj_list = frame_meta->obj_meta_list;
         obj_list != nullptr; obj_list = obj_list->next) {
        auto *obj_meta = reinterpret_cast<NvDsObjectMeta *>(obj_list->data);

        callback(obj_meta);
    }
}

void for_each_user_meta(NvDsUserMetaList *user_meta_list,
                        const std::function<void(NvDsUserMeta *)> &callback) {
    for (NvDsMetaList *user_list = user_meta_list;

         user_list != nullptr;

         user_list = user_list->next) {
        auto *user_meta = reinterpret_cast<NvDsUserMeta *>(user_list->data);

        callback(user_meta);
    }
}