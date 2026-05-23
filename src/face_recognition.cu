#include "face_recognition.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <thrust/count.h>
#include <thrust/device_vector.h>
#include <thrust/extrema.h>
#include <thrust/functional.h>
#include <thrust/transform.h>
#include <thrust/transform_reduce.h>

#include "utils/face_metadata_parser.h"

namespace {

constexpr float MATCH_THRESHOLD = 0.50f;

constexpr float EPSILON = 1e-5f;

} // namespace

bool FaceRecognition::load_embeddings(const std::string &metadata_path,
                                      const std::string &embeddings_path) {
    FaceDatabase database;

    if (!FaceMetadataParser::load(metadata_path, embeddings_path, database)) {
        return false;
    }

    known_faces_ = database.faces;

    embedding_size_ = database.embedding_size;

    num_faces_ = database.num_faces;

    d_known_embeddings_ = database.embeddings;

    std::cout << "Loaded " << num_faces_ << " embeddings to GPU" << std::endl;

    return true;
}

FaceRecognition::MatchResult FaceRecognition::recognize(float *embedding,
                                                        int embedding_size) {
    MatchResult result{

        "UNKNOWN",
        "",
        -1.0f,
        false,
    };

    if (known_faces_.empty())
        return result;

    if (embedding_size != embedding_size_) {

        std::cerr << "Embedding size mismatch" << std::endl;

        return result;
    }

    thrust::device_vector<float> d_query(embedding,
                                         embedding + embedding_size_);

    float sum_squares = thrust::transform_reduce(
        d_query.begin(), d_query.end(),

        [] __device__(float x) -> float { return x * x; },

        0.0f, thrust::plus<float>());

    float norm = std::sqrt(sum_squares) + EPSILON;

    thrust::transform(d_query.begin(), d_query.end(), d_query.begin(),

                      [norm] __device__(float x) -> float { return x / norm; });

    thrust::device_vector<float> d_scores(num_faces_);

    thrust::transform(
        thrust::counting_iterator<int>(0),
        thrust::counting_iterator<int>(num_faces_),

        d_scores.begin(),

        [query_ptr = thrust::raw_pointer_cast(d_query.data()),

         known_ptr = thrust::raw_pointer_cast(d_known_embeddings_.data()),

         embedding_size = embedding_size_

    ]

        __device__(int face_idx) -> float {
            float similarity = 0.0f;

            int offset = face_idx * embedding_size;

            for (int i = 0; i < embedding_size; ++i) {
                similarity += query_ptr[i] * known_ptr[offset + i];
            }

            return similarity;
        });

    // FIND BEST MATCH

    auto max_iter = thrust::max_element(d_scores.begin(), d_scores.end());

    int best_index = static_cast<int>(max_iter - d_scores.begin());

    float best_similarity = *max_iter;

    result.similarity = best_similarity;

    result.matched = (best_similarity > MATCH_THRESHOLD);

    if (result.matched) {

        result.name = known_faces_[best_index].name;
        result.image = known_faces_[best_index].image;
    }

    return result;
}