#include "nvdsinfer_custom_impl.h"

extern "C" bool NvDsInferParseCustomYoloFace(
    std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
    NvDsInferNetworkInfo const &networkInfo,
    NvDsInferParseDetectionParams const &detectionParams,
    std::vector<NvDsInferObjectDetectionInfo> &objectList) {

    const float *data =
        reinterpret_cast<const float *>(outputLayersInfo[0].buffer);

    const int num_boxes = 8400;

    const float threshold = detectionParams.perClassThreshold[0];

    for (int i = 0; i < num_boxes; i++) {
        float x = data[i];

        float y = data[num_boxes + i];

        float w = data[(num_boxes * 2) + i];

        float h = data[(num_boxes * 3) + i];

        float conf = data[(num_boxes * 4) + i];

        if (conf < threshold)
            continue;

        NvDsInferObjectDetectionInfo obj;

        obj.classId = 0;

        obj.detectionConfidence = conf;

        obj.left = x - (w / 2.0f);

        obj.top = y - (h / 2.0f);

        obj.width = w;
        obj.height = h;

        objectList.push_back(obj);
    }

    return true;
}