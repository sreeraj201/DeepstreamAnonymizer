#pragma once

#include <glib.h>

#include "nvbufsurface.h"
#include "nvdsmeta.h"

void pixelate_object(NvBufSurface *surface, NvDsFrameMeta *frame_meta,
                     NvDsObjectMeta *obj_meta);