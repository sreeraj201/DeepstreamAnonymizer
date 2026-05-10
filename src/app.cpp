#include <string>
#include <iostream>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <glib.h>
#include <cuda_runtime_api.h>

extern "C" {
#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvdsmeta_schema.h"
#include "gstnvdsmeta.h"
#include "nvds_yml_parser.h"
#include "gst-nvmessage.h"
}

#include "config.h"
#include "source_bin.h"
#include "pixelate.h"

#define PGIE_CLASS_ID_PERSON 0

#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080
#define MUXER_BATCH_TIMEOUT_USEC 40000

#define RTSP_PORT 8555
#define UDP_PORT 5400

#define LINK(a,b) \
if (!gst_element_link(a,b)) { \
    g_printerr("FAILED LINK: %s -> %s\n", \
    GST_ELEMENT_NAME(a), GST_ELEMENT_NAME(b)); \
    return false; \
}

class ProbeHandler {
public:
    static GstPadProbeReturn buffer_probe(
        GstPad *pad,
        GstPadProbeInfo *info,
        gpointer user_data)
    {
        GstBuffer *buf = (GstBuffer *) info->data;

        NvDsBatchMeta *batch_meta =
            gst_buffer_get_nvds_batch_meta(buf);

        GstMapInfo map;

        if (!gst_buffer_map(buf, &map, GST_MAP_READWRITE))
            return GST_PAD_PROBE_OK;

        NvBufSurface *surface =
            (NvBufSurface *) map.data;

        auto *config =
            static_cast<AppConfig *>(user_data);

        for (NvDsMetaList *l_frame = batch_meta->frame_meta_list;
             l_frame != nullptr;
             l_frame = l_frame->next)
        {
            auto *frame_meta =
                (NvDsFrameMeta *) l_frame->data;

            process_frame(
                surface,
                frame_meta,
                PGIE_CLASS_ID_PERSON,
                config->roi_enabled,
                config->clear_roi
            );

            if (config->roi_enabled)
                draw_roi(batch_meta, frame_meta, config->clear_roi);
        }

        gst_buffer_unmap(buf, &map);

        return GST_PAD_PROBE_OK;
    }
private:
    static void draw_roi(NvDsBatchMeta *batch_meta, NvDsFrameMeta *frame_meta, const ROI& clear_roi)
    { 
        NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta->num_rects = 1;
        NvOSD_RectParams &rect = display_meta->rect_params[0];
        rect.left = clear_roi.x;
        rect.top = clear_roi.y;
        rect.width = clear_roi.w;
        rect.height = clear_roi.h;
        rect.border_width = 4;
        rect.border_color.red = 0.0;
        rect.border_color.green = 0.0;
        rect.border_color.blue = 1.0;
        rect.border_color.alpha = 1.0;
        rect.has_bg_color = 1;
        rect.bg_color.red = 0.0;
        rect.bg_color.green = 0.0;
        rect.bg_color.blue = 1.0;
        rect.bg_color.alpha = 0.25;
        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }
};

class DeepStreamApp {
public:
    DeepStreamApp(int argc, char **argv)
    {
        gst_init(&argc, &argv);

        loop = g_main_loop_new(nullptr, FALSE);

        ConfigParser::parse(argc, argv,
                            config,
                            yaml_path);

        init_cuda();
    }

    ~DeepStreamApp()
    {
        if (pipeline)
            gst_object_unref(pipeline);

        if (loop)
            g_main_loop_unref(loop);

        if (config.src_list)
            g_list_free(config.src_list);
    }

    bool build()
    {
        pipeline =
            gst_pipeline_new("ds-pipeline");

        streammux =
            gst_element_factory_make(
                "nvstreammux",
                "streammux");

        if (!pipeline || !streammux)
            return false;

        gst_bin_add(
            GST_BIN(pipeline),
            streammux);

        if (!add_sources())
            return false;

        if (!create_elements())
            return false;

        if (!link_elements())
            return false;

        configure();

        attach_probe();

        setup_rtsp_server();

        setup_bus();

        return true;
    }

    void run()
    {
        GstStateChangeReturn ret =
            gst_element_set_state(
                pipeline,
                GST_STATE_PLAYING);

        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Failed to start pipeline\n";
            return;
        }

        std::cout
            << "\nRTSP Stream Ready:\n"
            << "rtsp://localhost:" + std::to_string(RTSP_PORT) + "/ds-test\n\n";

        g_main_loop_run(loop);

        gst_element_set_state(
            pipeline,
            GST_STATE_NULL);
    }

private:
    GMainLoop *loop = nullptr;

    GstElement *pipeline = nullptr;

    GstElement *streammux = nullptr;
    GstElement *pgie = nullptr;
    GstElement *tiler = nullptr;
    GstElement *nvvidconv = nullptr;
    GstElement *nvosd = nullptr;
    GstElement *nvdslogger = nullptr;

    GstElement *queue1 = nullptr;
    GstElement *queue2 = nullptr;
    GstElement *queue3 = nullptr;
    GstElement *queue4 = nullptr;

    GstElement *conv_rtsp = nullptr;
    GstElement *capsfilter = nullptr;
    GstElement *encoder = nullptr;
    GstElement *h264parse = nullptr;
    GstElement *rtppay = nullptr;
    GstElement *udpsink = nullptr;

    GstRTSPServer *server = nullptr;

    std::string yaml_path;

    AppConfig config;

    struct cudaDeviceProp prop;

    void init_cuda()
    {
        int dev = 0;

        cudaGetDevice(&dev);

        cudaGetDeviceProperties(
            &prop,
            dev);
    }

    bool add_sources()
    {
        GList *cur = config.src_list;

        for (guint i = 0;
             i < config.num_sources;
             i++)
        {
            gchar *uri =
                (gchar *) cur->data;

            GstElement *source_bin =
                create_source_bin(
                    i,
                    uri,
                    FALSE);

            if (!source_bin)
                return false;

            gst_bin_add(
                GST_BIN(pipeline),
                source_bin);

            gchar pad_name[16];

            g_snprintf(
                pad_name,
                sizeof(pad_name),
                "sink_%u",
                i);

            GstPad *sinkpad =
                gst_element_request_pad_simple(
                    streammux,
                    pad_name);

            GstPad *srcpad =
                gst_element_get_static_pad(
                    source_bin,
                    "src");

            if (!sinkpad ||
                !srcpad ||
                gst_pad_link(srcpad, sinkpad)
                    != GST_PAD_LINK_OK)
            {
                g_printerr(
                    "Failed to link source %d\n",
                    i);

                return false;
            }

            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            cur = cur->next;
        }

        return true;
    }

    bool create_elements()
    {
        pgie =
            gst_element_factory_make(
                "nvinfer",
                "pgie");

        tiler =
            gst_element_factory_make(
                "nvmultistreamtiler",
                "tiler");

        nvvidconv =
            gst_element_factory_make(
                "nvvideoconvert",
                "conv");

        nvosd =
            gst_element_factory_make(
                "nvdsosd",
                "osd");

        nvdslogger =
            gst_element_factory_make(
                "nvdslogger",
                "logger");

        queue1 =
            gst_element_factory_make(
                "queue",
                "q1");

        queue2 =
            gst_element_factory_make(
                "queue",
                "q2");

        queue3 =
            gst_element_factory_make(
                "queue",
                "q3");

        queue4 =
            gst_element_factory_make(
                "queue",
                "q4");

        conv_rtsp =
            gst_element_factory_make(
                "nvvideoconvert",
                "conv-rtsp");

        capsfilter =
            gst_element_factory_make(
                "capsfilter",
                "caps");

        encoder =
            gst_element_factory_make(
                "x264enc",
                "h264-enc");

        h264parse =
            gst_element_factory_make(
                "h264parse",
                "h264-parse");

        rtppay =
            gst_element_factory_make(
                "rtph264pay",
                "rtppay");

        udpsink =
            gst_element_factory_make(
                "udpsink",
                "udp-sink");

        if (!pgie ||
            !tiler ||
            !nvvidconv ||
            !nvosd ||
            !nvdslogger ||
            !queue1 ||
            !queue2 ||
            !queue3 ||
            !queue4 ||
            !conv_rtsp ||
            !capsfilter ||
            !encoder ||
            !h264parse ||
            !rtppay ||
            !udpsink)
        {
            g_printerr(
                "Failed creating elements\n");

            return false;
        }

        return true;
    }

    bool link_elements()
    {
        gst_bin_add_many(
            GST_BIN(pipeline),
            queue1,
            pgie,
            queue2,
            nvdslogger,
            tiler,
            queue3,
            nvvidconv,
            nvosd,
            queue4,
            conv_rtsp,
            capsfilter,
            encoder,
            h264parse,
            rtppay,
            udpsink,
            nullptr);

        GstCaps *caps =
            gst_caps_from_string(
                "video/x-raw, format=I420");

        g_object_set(
            G_OBJECT(capsfilter),
            "caps",
            caps,
            NULL);

        gst_caps_unref(caps);

        LINK(streammux, queue1);
        LINK(queue1, pgie);
        LINK(pgie, queue2);
        LINK(queue2, nvdslogger);
        LINK(nvdslogger, tiler);
        LINK(tiler, queue3);
        LINK(queue3, nvvidconv);
        LINK(nvvidconv, nvosd);
        LINK(nvosd, queue4);
        LINK(queue4, conv_rtsp);
        LINK(conv_rtsp, capsfilter);
        LINK(capsfilter, encoder);
        LINK(encoder, h264parse);
        LINK(h264parse, rtppay);
        LINK(rtppay, udpsink);

        return true;
    }

    void configure()
    {
        g_object_set(
            G_OBJECT(streammux),
            "batch-size",
            config.num_sources,
            "width",
            MUXER_OUTPUT_WIDTH,
            "height",
            MUXER_OUTPUT_HEIGHT,
            "batched-push-timeout",
            MUXER_BATCH_TIMEOUT_USEC,
            NULL);

        if (nvds_parse_gie(
                pgie,
                const_cast<gchar *>(
                    yaml_path.c_str()),
                (gchar *) "primary-gie"))
        {
            throw std::runtime_error(
                "Failed to parse primary-gie");
        }

        g_object_set(
            G_OBJECT(encoder),
            "bitrate", 4000,
            "speed-preset", 1,
            "tune", 0x00000004,
            "key-int-max", 15,
            "threads", 4,
            NULL);

        g_object_set(
            G_OBJECT(rtppay),
            "config-interval",
            1,
            NULL);

        g_object_set(
            G_OBJECT(udpsink),
            "host", "127.0.0.1",
            "port", UDP_PORT,
            "async", FALSE,
            "sync", 0,
            NULL);
    }

    void setup_rtsp_server()
    {
        server = gst_rtsp_server_new();

        g_object_set(
            server,
            "service",
            std::to_string(RTSP_PORT).c_str(),
            NULL);

        GstRTSPMountPoints *mounts =
            gst_rtsp_server_get_mount_points(
                server);

        GstRTSPMediaFactory *factory =
            gst_rtsp_media_factory_new();

        gst_rtsp_media_factory_set_shared(
            factory,
            TRUE);

        gst_rtsp_media_factory_set_launch(
            factory,
            "( "
            "udpsrc name=pay0 "
            "port=" + std::string(UDP_PORT).c_str() + " " 
            "buffer-size=524288 "
            "caps=\"application/x-rtp, "
            "media=video, "
            "clock-rate=90000, "
            "encoding-name=H264, "
            "payload=96\" "
            ")");

        gst_rtsp_mount_points_add_factory(
            mounts,
            "/ds-test",
            factory);

        g_object_unref(mounts);

        gst_rtsp_server_attach(
            server,
            NULL);
    }

    void attach_probe()
    {
        GstPad *pad =
            gst_element_get_static_pad(
                pgie,
                "src");

        gst_pad_add_probe(
            pad,
            GST_PAD_PROBE_TYPE_BUFFER,
            ProbeHandler::buffer_probe,
            &config,
            nullptr);

        gst_object_unref(pad);
    }

    static gboolean bus_callback(
        GstBus *,
        GstMessage *msg,
        gpointer data)
    {
        auto *app =
            static_cast<DeepStreamApp *>(data);

        switch (GST_MESSAGE_TYPE(msg)) {

        case GST_MESSAGE_EOS:

            g_print("End of stream\n");

            g_main_loop_quit(
                app->loop);

            break;

        case GST_MESSAGE_ERROR: {

            GError *err = nullptr;
            gchar *debug = nullptr;

            gst_message_parse_error(
                msg,
                &err,
                &debug);

            g_printerr(
                "ERROR from %s: %s\n",
                GST_OBJECT_NAME(msg->src),
                err->message);

            if (debug)
                g_printerr(
                    "Debug details: %s\n",
                    debug);

            g_error_free(err);
            g_free(debug);

            g_main_loop_quit(
                app->loop);

            break;
        }

        default:
            break;
        }

        return TRUE;
    }

    void setup_bus()
    {
        GstBus *bus =
            gst_pipeline_get_bus(
                GST_PIPELINE(pipeline));

        gst_bus_add_watch(
            bus,
            bus_callback,
            this);

        gst_object_unref(bus);
    }
};

int main(int argc, char *argv[])
{
    try {

        DeepStreamApp app(argc, argv);

        if (!app.build()) {

            std::cerr
                << "Failed to build pipeline\n";

            return -1;
        }

        app.run();

    } catch (const std::exception &e) {

        std::cerr
            << e.what()
            << "\n";

        return -1;
    }

    return 0;
}