#pragma once

#include "nvdsmeta.h"

#include "config.h"

void draw_roi_overlay(NvDsBatchMeta *batch_meta, NvDsFrameMeta *frame_meta,
                      const ROI &roi);