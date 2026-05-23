#include "rtsp_server.h"

#include <string>

GstRTSPServer *RtspServer::create(int rtsp_port, int udp_port,
                                  const char *mount_point) {
    GstRTSPServer *server = gst_rtsp_server_new();

    g_object_set(server, "service", std::to_string(rtsp_port).c_str(), nullptr);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_shared(factory, TRUE);

    std::string launch_string = "( "
                                "udpsrc name=pay0 "
                                "port=" +
                                std::to_string(udp_port) +
                                " "
                                "buffer-size=524288 "
                                "caps=\"application/x-rtp, "
                                "media=video, "
                                "clock-rate=90000, "
                                "encoding-name=H264, "
                                "payload=96\" "
                                ")";

    gst_rtsp_media_factory_set_launch(factory, launch_string.c_str());

    gst_rtsp_mount_points_add_factory(mounts, mount_point, factory);

    g_object_unref(mounts);

    gst_rtsp_server_attach(server, nullptr);

    return server;
}