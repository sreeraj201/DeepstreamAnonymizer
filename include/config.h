#pragma once

#include <string>

#include <glib.h>
#include <yaml-cpp/yaml.h>

struct ROI {

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct SinkConfig {

    /*
     * 0 -> Default 
     * 1 -> RTSP output
     */
    int type = 0;
    int rtsp_port = 8555;
    int udp_port = 5400;
    std::string mount = "/ds-test";
};

struct AppConfig {

    GList *src_list = nullptr;
    guint num_sources = 0;
    bool perf_mode = false;
    bool roi_enabled = false;
    ROI clear_roi;
    SinkConfig sink;
};

class ConfigParser {

  public:
    static void parse(int argc, char **argv, AppConfig &config,
                      std::string &yaml_path);

  private:
    static void parse_sources(YAML::Node &root, AppConfig &config);

    static void parse_roi(YAML::Node &root, AppConfig &config);

    static void parse_sink(YAML::Node &root, AppConfig &config);
};