#include "utils/face_metadata_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <cjson/cJSON.h>

bool FaceMetadataParser::load(const std::string &metadata_path,
                              const std::string &embeddings_path,
                              FaceDatabase &database) {
    database = {};

    return load_metadata(metadata_path, database) &&
           load_embeddings(embeddings_path, database);
}

bool FaceMetadataParser::load_metadata(const std::string &metadata_path,
                                       FaceDatabase &database) {
    std::ifstream file(metadata_path);

    if (!file.is_open()) {

        std::cerr << "Failed to open metadata file: " << metadata_path
                  << std::endl;

        return false;
    }

    std::stringstream buffer;

    buffer << file.rdbuf();

    std::string json_str = buffer.str();

    cJSON *root = cJSON_Parse(json_str.c_str());

    if (!root) {

        std::cerr << "Failed to parse metadata JSON" << std::endl;

        return false;
    }

    cJSON *embedding_size_json = cJSON_GetObjectItem(root, "embedding_size");

    cJSON *count_json = cJSON_GetObjectItem(root, "count");

    cJSON *faces_json = cJSON_GetObjectItem(root, "faces");

    if (!cJSON_IsNumber(embedding_size_json) ||

        !cJSON_IsNumber(count_json) ||

        !cJSON_IsArray(faces_json)) {
        std::cerr << "Invalid metadata format" << std::endl;

        cJSON_Delete(root);

        return false;
    }

    database.embedding_size = embedding_size_json->valueint;

    database.num_faces = count_json->valueint;

    cJSON *face_json = nullptr;

    cJSON_ArrayForEach(face_json, faces_json) {
        cJSON *id_json = cJSON_GetObjectItem(face_json, "id");

        cJSON *name_json = cJSON_GetObjectItem(face_json, "name");

        cJSON *image_json = cJSON_GetObjectItem(face_json, "image");

        if (!cJSON_IsNumber(id_json) || !cJSON_IsString(name_json) ||
            !cJSON_IsString(image_json)) {
            continue;
        }

        FaceEntry entry;

        entry.id = id_json->valueint;

        entry.name = name_json->valuestring;

        entry.image = image_json->valuestring;

        database.faces.push_back(entry);
    }

    cJSON_Delete(root);

    return !database.faces.empty();
}

bool FaceMetadataParser::load_embeddings(const std::string &embeddings_path,
                                         FaceDatabase &database) {
    std::ifstream file(embeddings_path, std::ios::binary);

    if (!file.is_open()) {

        std::cerr << "Failed to open embeddings file: " << embeddings_path
                  << std::endl;

        return false;
    }

    size_t total_floats =
        static_cast<size_t>(database.embedding_size) * database.num_faces;

    database.embeddings.resize(total_floats);

    file.read(reinterpret_cast<char *>(database.embeddings.data()),
              total_floats * sizeof(float));

    size_t bytes_read = static_cast<size_t>(file.gcount());

    size_t expected_bytes = total_floats * sizeof(float);

    if (bytes_read != expected_bytes) {

        std::cerr << "Embeddings size mismatch" << std::endl;

        return false;
    }

    return true;
}