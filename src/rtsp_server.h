#pragma once

#include <gst/rtsp-server/rtsp-server.h>

class RtspServer {
public:
    static GstRTSPServer* create(
        int rtsp_port,
        int udp_port,
        const char* mount_point);
};