#pragma once

#include "nvdsmeta.h"

#include <string>

void set_object_label(NvDsObjectMeta *obj_meta, const std::string &label);

void set_match_style(NvDsObjectMeta *obj_meta);

void set_unknown_style(NvDsObjectMeta *obj_meta);