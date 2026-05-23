#pragma once

#include <string>

std::string build_face_label(uint64_t object_id, const std::string &name,
                             const std::string &image, float similarity,
                             bool matched);