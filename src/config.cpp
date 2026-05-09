#include "config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

extern "C" {
#include "nvds_yml_parser.h"
}

void ConfigParser::parse(int argc,
                         char **argv,
                         AppConfig& config,
                         std::string& yaml_path)
{
    if (argc < 2) {
        throw std::runtime_error(
            "Usage: <yml>");
    }

    config.yaml_config =
        g_str_has_suffix(argv[1], ".yml") ||
        g_str_has_suffix(argv[1], ".yaml");

    if (!config.yaml_config) {
        throw std::runtime_error(
            "Missing <yml> file");
    }

    yaml_path = argv[1];

    nvds_parse_source_list(
        &config.src_list,
        argv[1],
        "source-list");

    for (GList *tmp = config.src_list;
         tmp;
         tmp = tmp->next)
    {
        config.num_sources++;
    }

    parse_roi_from_yaml(
        yaml_path,
        config.clear_roi);

    config.roi_enabled =
        (config.clear_roi.w > 0 &&
        config.clear_roi.h > 0);

}

void ConfigParser::parse_roi_from_yaml(
    const std::string& path,
    ROI& roi)
{
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open config file");
    }

    std::string line;

    roi = {0, 0, 0, 0};

    bool inside_roi = false;

    while (std::getline(file, line)) {

        line.erase(
            0,
            line.find_first_not_of(" \t"));

        if (line == "roi:") {
            inside_roi = true;
            continue;
        }

        if (!inside_roi)
            continue;

        if (!line.empty() &&
            line.back() == ':' &&
            line != "roi:")
        {
            break;
        }

        auto parse_value =
            [&](const std::string& key,
                int& value)
        {
            if (line.find(key) == 0) {

                size_t pos = line.find(':');

                if (pos != std::string::npos) {

                    value = std::stoi(
                        line.substr(pos + 1));
                }
            }
        };

        parse_value("x", roi.x);
        parse_value("y", roi.y);
        parse_value("w", roi.w);
        parse_value("h", roi.h);
    }
}