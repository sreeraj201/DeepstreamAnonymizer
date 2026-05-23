// face_recognition.h

#pragma once

#include <string>
#include <vector>

#include <thrust/device_vector.h>

#include "utils/face_metadata_parser.h"

class FaceRecognition {
  public:
    struct MatchResult {

        std::string name;

        std::string image;

        float similarity;

        bool matched;
    };

    bool load_embeddings(const std::string &metadata_path,
                         const std::string &embeddings_path);

    MatchResult recognize(float *embedding, int embedding_size);

  private:
    std::vector<FaceEntry> known_faces_;

    thrust::device_vector<float> d_known_embeddings_;

    int embedding_size_ = 0;

    int num_faces_ = 0;
};