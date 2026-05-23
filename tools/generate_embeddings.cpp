// generate_embeddings.cpp

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

namespace fs = std::filesystem;

namespace {

constexpr int FACE_SIZE = 112;

constexpr int EMBEDDING_SIZE = 512;

struct MetadataEntry {

    int id;

    std::string name;

    std::string image;
};

}

static std::vector<float> preprocess_face(
    const cv::Mat& image)
{
    cv::Mat rgb;

    cv::cvtColor(
        image,
        rgb,
        cv::COLOR_BGR2RGB);

    cv::Mat resized;

    cv::resize(
        rgb,
        resized,
        cv::Size(FACE_SIZE, FACE_SIZE));

    resized.convertTo(
        resized,
        CV_32F);

    resized =
        (resized - 127.5f) / 128.0f;

    std::vector<float> tensor(
        3 * FACE_SIZE * FACE_SIZE);

    for (int y = 0;
         y < FACE_SIZE;
         ++y)
    {
        for (int x = 0;
             x < FACE_SIZE;
             ++x)
        {
            cv::Vec3f pixel =
                resized.at<cv::Vec3f>(y, x);

            tensor[
                0 * FACE_SIZE * FACE_SIZE +
                y * FACE_SIZE + x] =
                    pixel[0];

            tensor[
                1 * FACE_SIZE * FACE_SIZE +
                y * FACE_SIZE + x] =
                    pixel[1];

            tensor[
                2 * FACE_SIZE * FACE_SIZE +
                y * FACE_SIZE + x] =
                    pixel[2];
        }
    }

    return tensor;
}

static void normalize_embedding(
    std::vector<float>& embedding)
{
    float norm = 0.0f;

    for (float value : embedding) {
        norm += value * value;
    }

    norm = std::sqrt(norm) + 1e-5f;

    for (float& value : embedding) {
        value /= norm;
    }
}

static bool is_image_file(
    const fs::path& path)
{
    std::string ext =
        path.extension().string();

    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        ::tolower);

    return ext == ".jpg"  ||
           ext == ".jpeg" ||
           ext == ".png";
}

static void write_metadata_json(
    const std::string& path,
    const std::vector<MetadataEntry>& entries)
{
    std::ofstream file(path);

    if (!file.is_open()) {

        throw std::runtime_error(
            "Failed to open metadata file");
    }

    file << "{\n";

    file << "  \"version\": 1,\n";

    file << "  \"embedding_size\": "
         << EMBEDDING_SIZE
         << ",\n";

    file << "  \"dtype\": \"float32\",\n";

    file << "  \"normalized\": true,\n";

    file << "  \"count\": "
         << entries.size()
         << ",\n\n";

    file << "  \"faces\": [\n";

    for (size_t i = 0;
         i < entries.size();
         ++i)
    {
        const auto& entry =
            entries[i];

        file << "    {\n";

        file << "      \"id\": "
             << entry.id
             << ",\n";

        file << "      \"name\": \""
             << entry.name
             << "\",\n";

        file << "      \"image\": \""
             << entry.image
             << "\"\n";

        file << "    }";

        if (i + 1 < entries.size()) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ]\n";

    file << "}\n";
}

int main(
    int argc,
    char** argv)
{
    if (argc < 4) {

        std::cerr
            << "Usage:\n"
            << argv[0]
            << " <faces_folder> <arcface.onnx> <output_folder>\n";

        return 1;
    }

    std::string dataset_folder =
        argv[1];

    std::string model_path =
        argv[2];

    std::string output_folder =
        argv[3];

    fs::create_directories(
        output_folder);

    std::string embeddings_path =
        output_folder + "/embeddings.bin";

    std::string metadata_path =
        output_folder + "/metadata.json";

    std::ofstream bin_file(
        embeddings_path,
        std::ios::binary);

    if (!bin_file.is_open()) {

        std::cerr
            << "Failed to create embeddings file"
            << std::endl;

        return 1;
    }

    std::vector<MetadataEntry>
        metadata_entries;

    Ort::Env env(
        ORT_LOGGING_LEVEL_WARNING,
        "arcface");

    Ort::SessionOptions session_options;

    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    Ort::Session session(
        env,
        model_path.c_str(),
        session_options);

    Ort::AllocatorWithDefaultOptions allocator;

    auto input_names =
        session.GetInputNames();

    auto output_names =
        session.GetOutputNames();

    const char* input_name =
        input_names[0].c_str();

    const char* output_name =
        output_names[0].c_str();

    int current_id = 1;

    for (const auto& person_dir :
         fs::directory_iterator(dataset_folder))
    {
        if (!person_dir.is_directory()) {
            continue;
        }

        std::string person_name =
            person_dir.path()
                .filename()
                .string();

        std::cout
            << "\nProcessing: "
            << person_name
            << std::endl;

        for (const auto& image_entry :
             fs::directory_iterator(
                 person_dir.path()))
        {
            if (!image_entry.is_regular_file())
                continue;

            if (!is_image_file(
                    image_entry.path()))
            {
                continue;
            }

            std::string image_path =
                image_entry.path()
                    .string();

            std::string image_name =
                image_entry.path()
                    .filename()
                    .string();

            cv::Mat image =
                cv::imread(image_path);

            if (image.empty()) {

                std::cerr
                    << "Failed to load image: "
                    << image_path
                    << std::endl;

                continue;
            }

            auto input_tensor_values =
                preprocess_face(image);

            std::vector<int64_t> input_shape = {
                1,
                3,
                FACE_SIZE,
                FACE_SIZE
            };

            Ort::MemoryInfo memory_info =
                Ort::MemoryInfo::CreateCpu(
                    OrtArenaAllocator,
                    OrtMemTypeDefault);

            Ort::Value input_tensor =
                Ort::Value::CreateTensor<float>(
                    memory_info,
                    input_tensor_values.data(),
                    input_tensor_values.size(),
                    input_shape.data(),
                    input_shape.size());

            auto output_tensors =
                session.Run(
                    Ort::RunOptions{nullptr},
                    &input_name,
                    &input_tensor,
                    1,
                    &output_name,
                    1);

            float* output_data =
                output_tensors[0]
                    .GetTensorMutableData<float>();

            std::vector<float> embedding(
                output_data,
                output_data + EMBEDDING_SIZE);

            normalize_embedding(
                embedding);

            bin_file.write(
                reinterpret_cast<char*>(
                    embedding.data()),
                EMBEDDING_SIZE *
                    sizeof(float));

            metadata_entries.push_back({
                current_id,
                person_name,
                image_name
            });

            std::cout
                << "Saved embedding for: "
                << image_name
                << std::endl;

            current_id++;
        }
    }

    bin_file.close();

    write_metadata_json(
        metadata_path,
        metadata_entries);

    std::cout
        << "\nDone\n"
        << "Generated:\n"
        << embeddings_path
        << "\n"
        << metadata_path
        << std::endl;

    return 0;
}