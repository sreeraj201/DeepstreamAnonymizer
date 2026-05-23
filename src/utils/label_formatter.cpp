#include "utils/label_formatter.h"

#include <iomanip>
#include <sstream>

std::string build_face_label(uint64_t object_id, const std::string &name,
                             const std::string &image, float similarity,
                             bool matched) {
    std::stringstream label;

    if (matched) {

        label << "ID:" << object_id << " " << name << " " << image << " "
              << std::fixed << std::setprecision(2) << similarity;
    } else {

        label << "ID:" << object_id << " UNKNOWN";
    }

    return label.str();
}