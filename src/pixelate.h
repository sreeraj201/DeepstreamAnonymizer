#ifndef PIXELATE_H
#define PIXELATE_H

extern "C" {
#include <glib.h>

#include "nvbufsurface.h"
#include "nvdsmeta.h"
}

#include "config.h"


void process_frame(NvBufSurface *surface,
                   NvDsFrameMeta *frame_meta,
                   int class_id,
                   bool roi_enabled,
                   const ROI& clear_roi);

#endif