#ifdef __cplusplus
extern "C" {
#endif

#ifndef PIXELATE_H
#define PIXELATE_H

#include <glib.h>
#include "nvbufsurface.h"
#include "nvdsmeta.h"

gboolean init_scratch_buffer(NvBufSurface *surface);
void process_frame(NvBufSurface *surface, NvDsFrameMeta *frame_meta, int class_id);

#endif

#ifdef __cplusplus
}
#endif