#include "config.h"

#include <stdexcept>

void ConfigParser::parse(
    int argc,
    char** argv,
    AppConfig& config,
    std::string& yaml_path)
{
    if (argc < 2)
        throw std::runtime_error("Missing config");

    yaml_path = argv[1];

    YAML::Node root =
        YAML::LoadFile(yaml_path);

    parse_sources(root, config);
    parse_roi(root, config);
    parse_sink(root, config);
}

void ConfigParser::parse_sources(
    YAML::Node& root,
    AppConfig& config)
{
    if (!root["source-list"])
        throw std::runtime_error(
            "Missing source-list");

    auto sources = root["source-list"];

    for (auto source : sources) {

        config.src_list =
            g_list_append(
                config.src_list,
                g_strdup(
                    source.as<std::string>().c_str()));

        config.num_sources++;
    }
}

void ConfigParser::parse_roi(
    YAML::Node& root,
    AppConfig& config)
{
    if (!root["roi"])
        return;

    auto roi = root["roi"];

    config.roi_enabled =
        roi["enabled"]
            ? roi["enabled"].as<bool>()
            : false;

    config.clear_roi.x =
        roi["x"]
            ? roi["x"].as<int>()
            : 0;

    config.clear_roi.y =
        roi["y"]
            ? roi["y"].as<int>()
            : 0;

    config.clear_roi.w =
        roi["w"]
            ? roi["w"].as<int>()
            : 0;

    config.clear_roi.h =
        roi["h"]
            ? roi["h"].as<int>()
            : 0;
}

void ConfigParser::parse_sink(
    YAML::Node& root,
    AppConfig& config)
{
    if (!root["sink"])
        return;

    auto sink = root["sink"];

    config.sink.type =
        sink["type"]
            ? sink["type"].as<int>()
            : 0;

    config.sink.rtsp_port =
        sink["rtsp_port"]
            ? sink["rtsp_port"].as<int>()
            : 8555;

    config.sink.udp_port =
        sink["udp_port"]
            ? sink["udp_port"].as<int>()
            : 5400;

    config.sink.mount =
        sink["mount"]
            ? sink["mount"].as<std::string>()
            : "/ds-test";
}