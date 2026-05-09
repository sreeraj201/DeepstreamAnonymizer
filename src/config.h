#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <string>

#include <glib.h>

struct ROI {
    int x;
    int y;
    int w;
    int h;
};

struct AppConfig {
    bool perf_mode = false;
    bool yaml_config = false;

    guint num_sources = 0;

    GList* src_list = nullptr;

    bool roi_enabled = false;
    ROI clear_roi;
};

class ConfigParser {
public:
    static void parse(int argc,
                      char **argv,
                      AppConfig& config,
                      std::string& yaml_path);

private:
    static void parse_roi_from_yaml(
        const std::string& path,
        ROI& roi);
};

#endif