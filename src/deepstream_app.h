#pragma once

#include <iostream>
#include <string>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <glib.h>
#include <cuda_runtime_api.h>

#include "config.h"


class DeepStreamApp {
public:
    DeepStreamApp(int argc, char** argv);
    ~DeepStreamApp();

    bool build();
    void run();
    bool is_rtsp_sink() const;

private:
    bool add_sources();
    bool create_elements();
    bool add_elements_to_pipeline();
    bool configure_elements();
    bool link_pipeline();

    bool setup_rtsp_server();
    bool attach_probe();
    bool setup_bus();

    void init_cuda();

    static gboolean bus_callback(
        GstBus*,
        GstMessage*,
        gpointer);

private:
    GMainLoop* loop_ = nullptr;

    GstElement* pipeline_   = nullptr;
    GstElement* streammux_  = nullptr;

    GstElement* pgie_       = nullptr;
    GstElement* tiler_      = nullptr;
    GstElement* nvvidconv_  = nullptr;
    GstElement* nvosd_      = nullptr;
    GstElement* nvdslogger_ = nullptr;

    GstElement* queue1_ = nullptr;
    GstElement* queue2_ = nullptr;
    GstElement* queue3_ = nullptr;
    GstElement* queue4_ = nullptr;

    GstElement* rtsp_conv_  = nullptr;
    GstElement* capsfilter_ = nullptr;
    GstElement* encoder_    = nullptr;
    GstElement* h264parse_  = nullptr;
    GstElement* rtppay_     = nullptr;
    GstElement* udpsink_    = nullptr;
    GstElement* sink_    = nullptr;

    GstRTSPServer* rtsp_server_ = nullptr;

    AppConfig config_;

    std::string yaml_path_;

    cudaDeviceProp cuda_prop_ {};
};