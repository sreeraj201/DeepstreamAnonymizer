#pragma once

#include <string>
#include <vector>

struct FaceEntry {

    int id;

    std::string name;

    std::string image;
};

struct FaceDatabase {

    int embedding_size = 0;

    int num_faces = 0;

    std::vector<FaceEntry> faces;

    std::vector<float> embeddings;
};

class FaceMetadataParser {
  public:
    static bool load(const std::string &metadata_path,
                     const std::string &embeddings_path,
                     FaceDatabase &database);

  private:
    static bool load_metadata(const std::string &metadata_path,
                              FaceDatabase &database);

    static bool load_embeddings(const std::string &embeddings_path,
                                FaceDatabase &database);
};