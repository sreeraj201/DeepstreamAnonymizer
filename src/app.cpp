#include <string>
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#include <gst/gst.h>
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

#define OSD_PROCESS_MODE 1
#define OSD_DISPLAY_TEXT 0

#define PGIE_CLASS_ID_PERSON 0

#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080
#define MUXER_BATCH_TIMEOUT_USEC 40000

static gboolean PERF_MODE = FALSE;


class ProbeHandler {
public:
    static GstPadProbeReturn buffer_probe(GstPad *pad,
                                          GstPadProbeInfo *info,
                                          gpointer user_data)
    {
        GstBuffer *buf = (GstBuffer *)info->data;
        NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);

        GstMapInfo map;
        if (!gst_buffer_map(buf, &map, GST_MAP_READWRITE))
            return GST_PAD_PROBE_OK;

        NvBufSurface *surface = (NvBufSurface *)map.data;

        for (NvDsMetaList *l_frame = batch_meta->frame_meta_list;
             l_frame != nullptr; l_frame = l_frame->next) {


            auto *config = static_cast<AppConfig *>(user_data);

            const ROI& clear_roi = config->clear_roi;

            auto *frame_meta = (NvDsFrameMeta *)l_frame->data;
            process_frame(surface, frame_meta, PGIE_CLASS_ID_PERSON, config->roi_enabled, clear_roi);

            if (config->roi_enabled)
                draw_roi(batch_meta, frame_meta, clear_roi);
        }

        gst_buffer_unmap(buf, &map);
        return GST_PAD_PROBE_OK;
    }

private:
    static void draw_roi(NvDsBatchMeta *batch_meta,
                         NvDsFrameMeta *frame_meta, const ROI& clear_roi)
    {

        NvDsDisplayMeta *display_meta =
            nvds_acquire_display_meta_from_pool(batch_meta);

        display_meta->num_rects = 1;

        NvOSD_RectParams &rect =
            display_meta->rect_params[0];

        rect.left   = clear_roi.x;
        rect.top    = clear_roi.y;
        rect.width  = clear_roi.w;
        rect.height = clear_roi.h;

        // Border width
        rect.border_width = 4;

        // Red border
        rect.border_color.red   = 0.0;
        rect.border_color.green = 0.0;
        rect.border_color.blue  = 1.0;
        rect.border_color.alpha = 1.0;

        // Semi-transparent fill
        rect.has_bg_color = 1;

        rect.bg_color.red   = 0.0;
        rect.bg_color.green = 0.0;
        rect.bg_color.blue  = 1.0;
        rect.bg_color.alpha = 0.25;

        nvds_add_display_meta_to_frame(frame_meta,
                                       display_meta);
    }
};

class DeepStreamApp {
public:
    DeepStreamApp(int argc, char** argv) {
        gst_init(&argc, &argv);
        loop = g_main_loop_new(nullptr, FALSE);

        config.perf_mode =
            g_getenv("NVDS_PERF_MODE") &&
            !g_strcmp0(g_getenv("NVDS_PERF_MODE"), "1");

        ConfigParser::parse(argc, argv, config, yaml_path);
        init_cuda();
    }

    ~DeepStreamApp() {
        if (pipeline) gst_object_unref(pipeline);
        if (loop) g_main_loop_unref(loop);
        if (config.src_list) g_list_free(config.src_list);
    }

    bool build() {
        pipeline = gst_pipeline_new("ds-pipeline");
        streammux = gst_element_factory_make("nvstreammux", "streammux");

        if (!pipeline || !streammux) return false;

        gst_bin_add(GST_BIN(pipeline), streammux);

        if (!add_sources()) return false;
        if (!create_elements()) return false;
        if (!link_elements()) return false;

        configure();
        attach_probe();
        setup_bus();

        return true;
    }

    void run() {
        GstStateChangeReturn ret =
            gst_element_set_state(pipeline, GST_STATE_PLAYING);

        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Failed to start pipeline\n";
            return;
        }

        std::cout << "Running...\n";
        g_main_loop_run(loop);

        gst_element_set_state(pipeline, GST_STATE_NULL);
    }

private:
    GMainLoop *loop = nullptr;
    GstElement *pipeline = nullptr, *streammux = nullptr;
    GstElement *pgie = nullptr, *tiler = nullptr;
    GstElement *nvvidconv = nullptr, *nvosd = nullptr;
    GstElement *sink = nullptr, *nvdslogger = nullptr;

    GstElement *queue1, *queue2, *queue3;

    std::string yaml_path;

    AppConfig config;
    struct cudaDeviceProp prop;

    void init_cuda() {
        int dev = 0;
        cudaGetDevice(&dev);
        cudaGetDeviceProperties(&prop, dev);
    }

    bool add_sources() {
        GList *cur = config.src_list;

        for (guint i = 0; i < config.num_sources; i++) {

            gchar *uri = (gchar*)cur->data;

            GstElement *source_bin =
                create_source_bin(i, uri, config.perf_mode);

            if (!source_bin) return false;

            gst_bin_add(GST_BIN(pipeline), source_bin);

            gchar pad_name[16];
            g_snprintf(pad_name, sizeof(pad_name), "sink_%u", i);

            GstPad *sinkpad =
                gst_element_request_pad_simple(streammux, pad_name);
            GstPad *srcpad =
                gst_element_get_static_pad(source_bin, "src");

            if (!sinkpad || !srcpad ||
                gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK) {
                g_printerr("Failed to link source %d\n", i);
                return false;
            }

            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            cur = cur->next;
        }

        return true;
    }

    bool create_elements() {
        pgie = gst_element_factory_make("nvinfer", "pgie");
        tiler = gst_element_factory_make("nvmultistreamtiler", "tiler");
        nvvidconv = gst_element_factory_make("nvvideoconvert", "conv");
        nvosd = gst_element_factory_make("nvdsosd", "osd");
        nvdslogger = gst_element_factory_make("nvdslogger", "logger");

        sink = config.perf_mode ?
            gst_element_factory_make("fakesink", "sink") :
            (prop.integrated ?
             gst_element_factory_make("nv3dsink", "sink") :
             gst_element_factory_make("nveglglessink", "sink"));

        queue1 = gst_element_factory_make("queue", "q1");
        queue2 = gst_element_factory_make("queue", "q2");
        queue3 = gst_element_factory_make("queue", "q3");

        return (pgie && tiler && nvvidconv && nvosd &&
                sink && queue1 && queue2 && queue3);
    }

    bool link_elements() {
        gst_bin_add_many(GST_BIN(pipeline),
            queue1, pgie, queue2, nvdslogger,
            tiler, queue3, nvvidconv, nvosd,
            sink, nullptr);

        return gst_element_link_many(streammux, queue1, pgie,
            queue2, nvdslogger, tiler, queue3,
            nvvidconv, nvosd, sink, nullptr);
    }

    void configure() {
        g_object_set(streammux,
            "batch-size", config.num_sources,
            "width", MUXER_OUTPUT_WIDTH,
            "height", MUXER_OUTPUT_HEIGHT,
            "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC,
            NULL);

        if (nvds_parse_gie(pgie,
            const_cast<gchar*>(yaml_path.c_str()),
            (gchar*)"primary-gie")){
            throw std::runtime_error("Failed to parse primary-gie config");
        }

        guint pgie_batch_size = 0;
        g_object_get(G_OBJECT(pgie), "batch-size", &pgie_batch_size, NULL);

        if (pgie_batch_size != config.num_sources) {
            g_printerr("WARNING: Overriding pgie batch-size %d → %d\n",
                    pgie_batch_size, config.num_sources);

            g_object_set(G_OBJECT(pgie),
                "batch-size", config.num_sources,
                NULL);
        }
    }

    void attach_probe() {
        GstPad *pad = gst_element_get_static_pad(pgie, "src");
        gst_pad_add_probe(pad,
            GST_PAD_PROBE_TYPE_BUFFER,
            ProbeHandler::buffer_probe,
            &config, nullptr);
        gst_object_unref(pad);
    }

    static gboolean bus_callback(GstBus*, GstMessage *msg, gpointer data) {
        auto *app = static_cast<DeepStreamApp*>(data);

        switch (GST_MESSAGE_TYPE(msg)) {

            case GST_MESSAGE_EOS:
                g_print("End of stream\n");
                g_main_loop_quit(app->loop);
                break;

            case GST_MESSAGE_ERROR: {
                GError *err = nullptr;
                gchar *debug = nullptr;

                gst_message_parse_error(msg, &err, &debug);

                g_printerr("ERROR from %s: %s\n",
                    GST_OBJECT_NAME(msg->src),
                    err->message);

                if (debug)
                    g_printerr("Debug details: %s\n", debug);

                g_error_free(err);
                g_free(debug);

                g_main_loop_quit(app->loop);
                break;
            }

            default:
                break;
        }
        return TRUE;
    }

    void setup_bus() {
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
        gst_bus_add_watch(bus, bus_callback, this);
        gst_object_unref(bus);
    }
};

int main(int argc, char *argv[]) {
    try {
        DeepStreamApp app(argc, argv);

        if (!app.build()) {
            std::cerr << "Failed to build pipeline\n";
            return -1;
        }

        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return -1;
    }

    return 0;
}
