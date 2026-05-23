#include "utils/tensor_meta.h"

#include "utils/meta_iter.h"

NvDsInferTensorMeta *get_tensor_meta(NvDsObjectMeta *obj_meta, int unique_id) {
    if (!obj_meta)
        return nullptr;

    NvDsInferTensorMeta *result = nullptr;

    for_each_user_meta(obj_meta->obj_user_meta_list,

                       [&](NvDsUserMeta *user_meta) {
                           if (!user_meta)
                               return;

                           if (user_meta->base_meta.meta_type !=
                               NVDSINFER_TENSOR_OUTPUT_META) {
                               return;
                           }

                           auto *tensor_meta =
                               reinterpret_cast<NvDsInferTensorMeta *>(
                                   user_meta->user_meta_data);

                           if (!tensor_meta)
                               return;

                           if (user_meta->base_meta.meta_type ==
                               NVDSINFER_TENSOR_OUTPUT_META) {
                               if (tensor_meta->unique_id == unique_id) {
                                   result = tensor_meta;
                               }
                           }
                       });

    return result;
}