#include "deepstream_app.h"

#include "nvds_yml_parser.h"

#include "probe_handler.h"
#include "rtsp_server.h"
#include "source_bin.h"
#include "utils/gst.h"

namespace {

constexpr guint MUXER_OUTPUT_WIDTH = 1920;
constexpr guint MUXER_OUTPUT_HEIGHT = 1080;
constexpr guint MUXER_BATCH_TIMEOUT_USEC = 40000;

}

bool DeepStreamApp::is_rtsp_sink() const { return config_.sink.type == 1; }

DeepStreamApp::DeepStreamApp(int argc, char **argv) {
    gst_init(&argc, &argv);

    loop_ = g_main_loop_new(nullptr, FALSE);

    ConfigParser::parse(argc, argv, config_, yaml_path_);

    init_cuda();
}

DeepStreamApp::~DeepStreamApp() {
    if (pipeline_)
        gst_object_unref(pipeline_);

    if (loop_)
        g_main_loop_unref(loop_);

    if (config_.src_list)
        g_list_free(config_.src_list);
}

void DeepStreamApp::init_cuda() {
    int device = 0;
    cudaGetDevice(&device);
    cudaGetDeviceProperties(&cuda_prop_, device);
}

bool DeepStreamApp::load_face_embeddings() {
    return face_recognition_.load_embeddings("embeddings/metadata.json",
                                             "embeddings/embeddings.bin");
}

bool DeepStreamApp::build() {
    pipeline_ = gst_pipeline_new("deepstream-pipeline");

    streammux_ = gst_element_factory_make("nvstreammux", "streammux");

    check_element(pipeline_, "pipeline");
    check_element(streammux_, "streammux");

    gst_bin_add(GST_BIN(pipeline_), streammux_);

    if (!load_face_embeddings()) {
        std::cerr << "Failed to load face embeddings" << std::endl;

        return false;
    }

    probe_context_.config = &config_;

    probe_context_.face_recognition = &face_recognition_;

    return add_sources() && create_elements() && add_elements_to_pipeline() &&
           configure_elements() && link_pipeline() && attach_probe() &&
           setup_rtsp_server() && setup_bus();
}

void DeepStreamApp::run() {
    auto ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline\n";
        return;
    }

    if (is_rtsp_sink()) {
        std::cout << "\nRTSP Stream Ready\n"
                  << "rtsp://localhost:" << config_.sink.rtsp_port
                  << config_.sink.mount << "\n\n";
    }

    g_main_loop_run(loop_);

    gst_element_set_state(pipeline_, GST_STATE_NULL);
}

bool DeepStreamApp::add_sources() {
    GList *current = config_.src_list;

    for (guint i = 0; i < config_.num_sources; ++i) {
        gchar *uri = static_cast<gchar *>(current->data);

        GstElement *source_bin = create_source_bin(i, uri, FALSE);

        if (!source_bin)
            return false;

        gst_bin_add(GST_BIN(pipeline_), source_bin);

        gchar sink_pad_name[16];

        g_snprintf(sink_pad_name, sizeof(sink_pad_name), "sink_%u", i);

        GstPad *sink_pad =
            gst_element_request_pad_simple(streammux_, sink_pad_name);

        GstPad *src_pad = gst_element_get_static_pad(source_bin, "src");

        if (!sink_pad || !src_pad ||
            gst_pad_link(src_pad, sink_pad) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link source %u\n", i);

            return false;
        }

        gst_object_unref(src_pad);
        gst_object_unref(sink_pad);

        current = current->next;
    }

    return true;
}

bool DeepStreamApp::create_elements() {
    pgie_ = gst_element_factory_make("nvinfer", "pgie");

    sgie_ = gst_element_factory_make("nvinfer", "sgie");

    tiler_ = gst_element_factory_make("nvmultistreamtiler", "tiler");

    nvvidconv_ = gst_element_factory_make("nvvideoconvert", "conv");

    nvosd_ = gst_element_factory_make("nvdsosd", "osd");

    nvdslogger_ = gst_element_factory_make("nvdslogger", "logger");

    tracker_ = gst_element_factory_make("nvtracker", "tracker");

    queue1_ = gst_element_factory_make("queue", "q1");

    queue_sgie_ = gst_element_factory_make("queue", "q_sgie");

    queue_tracker_ = gst_element_factory_make("queue", "q_tracker");

    queue2_ = gst_element_factory_make("queue", "q2");

    queue3_ = gst_element_factory_make("queue", "q3");

    queue4_ = gst_element_factory_make("queue", "q4");

    check_element(pgie_, "pgie");
    check_element(sgie_, "sgie");

    check_element(tiler_, "tiler");
    check_element(nvvidconv_, "nvvidconv");
    check_element(nvosd_, "nvosd");
    check_element(nvdslogger_, "nvdslogger");

    check_element(tracker_, "tracker");

    check_element(queue1_, "queue1");
    check_element(queue_sgie_, "queue_sgie");
    check_element(queue_tracker_, "queue_tracker");
    check_element(queue2_, "queue2");
    check_element(queue3_, "queue3");
    check_element(queue4_, "queue4");

    if (is_rtsp_sink()) {

        rtsp_conv_ = gst_element_factory_make("nvvideoconvert", "rtsp-conv");

        capsfilter_ = gst_element_factory_make("capsfilter", "caps");

        encoder_ = gst_element_factory_make("x264enc", "encoder");

        h264parse_ = gst_element_factory_make("h264parse", "h264parse");

        rtppay_ = gst_element_factory_make("rtph264pay", "rtppay");

        udpsink_ = gst_element_factory_make("udpsink", "udpsink");

        check_element(rtsp_conv_, "rtsp_conv");
        check_element(capsfilter_, "capsfilter");
        check_element(encoder_, "encoder");
        check_element(h264parse_, "h264parse");
        check_element(rtppay_, "rtppay");
        check_element(udpsink_, "udpsink");
    } else {

        sink_ = gst_element_factory_make("nv3dsink", "sink");

        check_element(sink_, "sink");
    }

    return true;
}

bool DeepStreamApp::add_elements_to_pipeline() {

    gst_bin_add_many(GST_BIN(pipeline_), queue1_, pgie_, queue_tracker_,
                     tracker_, queue_sgie_, sgie_, queue2_, nvdslogger_, tiler_,
                     queue3_, nvvidconv_, nvosd_, queue4_, nullptr);

    if (is_rtsp_sink()) {
        gst_bin_add_many(GST_BIN(pipeline_), rtsp_conv_, capsfilter_, encoder_,
                         h264parse_, rtppay_, udpsink_, nullptr);
    } else {
        gst_bin_add(GST_BIN(pipeline_), sink_);
    }

    return true;
}

bool DeepStreamApp::configure_elements() {
    g_object_set(G_OBJECT(streammux_), "batch-size", config_.num_sources,
                 "width", MUXER_OUTPUT_WIDTH, "height", MUXER_OUTPUT_HEIGHT,
                 "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, nullptr);

    // Parse PGIE and SGIE from Master YAML config
    if (nvds_parse_gie(pgie_, const_cast<gchar *>(yaml_path_.c_str()),
                       (gchar *)"primary-gie")) {
        throw std::runtime_error("Failed to parse primary-gie");
    }

    if (nvds_parse_gie(sgie_, const_cast<gchar *>(yaml_path_.c_str()),
                       (gchar *)"secondary-gie")) {
        throw std::runtime_error("Failed to parse secondary-gie");
    }

    g_object_set(
        G_OBJECT(tracker_), "tracker-width", 640, "tracker-height", 384,
        "gpu-id", 0, "ll-lib-file",
        "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file",
        "/opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/"
        "config_tracker_NvDCF_perf.yml",
        "enable-batch-process", 1, nullptr);

    if (is_rtsp_sink()) {
        GstCaps *caps = gst_caps_from_string("video/x-raw,format=I420");
        g_object_set(G_OBJECT(capsfilter_), "caps", caps, nullptr);
        gst_caps_unref(caps);

        g_object_set(G_OBJECT(encoder_), "bitrate", 4000, "speed-preset", 1,
                     "tune", 0x00000004, "key-int-max", 15, "bframes", 0,
                     "threads", 4, nullptr);

        g_object_set(G_OBJECT(rtppay_), "config-interval", 1, "pt", 96,
                     nullptr);
        g_object_set(G_OBJECT(udpsink_), "host", "127.0.0.1", "port",
                     config_.sink.udp_port, "sync", FALSE, "async", FALSE,
                     nullptr);
    }

    return true;
}

bool DeepStreamApp::link_pipeline() {
    link_elements(streammux_, queue1_);
    link_elements(queue1_, pgie_);
    link_elements(pgie_, queue_tracker_);
    link_elements(queue_tracker_, tracker_);
    link_elements(tracker_, queue_sgie_);
    link_elements(queue_sgie_, sgie_);
    link_elements(sgie_, queue2_);
    link_elements(queue2_, nvdslogger_);
    link_elements(nvdslogger_, tiler_);
    link_elements(tiler_, queue3_);
    link_elements(queue3_, nvvidconv_);
    link_elements(nvvidconv_, nvosd_);
    link_elements(nvosd_, queue4_);

    if (is_rtsp_sink()) {

        link_elements(queue4_, rtsp_conv_);
        link_elements(rtsp_conv_, capsfilter_);
        link_elements(capsfilter_, encoder_);
        link_elements(encoder_, h264parse_);
        link_elements(h264parse_, rtppay_);
        link_elements(rtppay_, udpsink_);
    } else {

        link_elements(queue4_, sink_);
    }

    return true;
}

bool DeepStreamApp::setup_rtsp_server() {
    if (!is_rtsp_sink())
        return true;

    rtsp_server_ =
        RtspServer::create(config_.sink.rtsp_port, config_.sink.udp_port,
                           config_.sink.mount.c_str());

    return rtsp_server_ != nullptr;
}

bool DeepStreamApp::attach_probe() {
    GstPad *pad = gst_element_get_static_pad(sgie_, "src");

    if (!pad)
        return false;

    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                      ProbeHandler::buffer_probe, &probe_context_, nullptr);

    gst_object_unref(pad);

    return true;
}

bool DeepStreamApp::setup_bus() {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));

    gst_bus_add_watch(bus, bus_callback, this);

    gst_object_unref(bus);

    return true;
}

gboolean DeepStreamApp::bus_callback(GstBus *, GstMessage *msg, gpointer data) {
    auto *app = static_cast<DeepStreamApp *>(data);

    switch (GST_MESSAGE_TYPE(msg)) {

    case GST_MESSAGE_EOS:

        g_print("End of stream\n");
        g_main_loop_quit(app->loop_);
        break;

    case GST_MESSAGE_ERROR: {

        GError *err = nullptr;
        gchar *debug = nullptr;

        gst_message_parse_error(msg, &err, &debug);

        g_printerr("ERROR from %s: %s\n", GST_OBJECT_NAME(msg->src),
                   err->message);

        if (debug)
            g_printerr("Debug details: %s\n", debug);

        g_error_free(err);
        g_free(debug);

        g_main_loop_quit(app->loop_);

        break;
    }

    default:
        break;
    }

    return TRUE;
}