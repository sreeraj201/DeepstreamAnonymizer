#pragma once

#include <functional>

#include "nvdsmeta.h"

void for_each_frame(NvDsBatchMeta *batch_meta,
                    const std::function<void(NvDsFrameMeta *)> &callback);

void for_each_object(NvDsFrameMeta *frame_meta,
                     const std::function<void(NvDsObjectMeta *)> &callback);

void for_each_user_meta(NvDsUserMetaList *user_meta_list,
                        const std::function<void(NvDsUserMeta *)> &callback);