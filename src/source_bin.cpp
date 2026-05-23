#include "source_bin.h"

#include <cstring>
#include <iostream>

#include <glib.h>

#define GST_CAPS_FEATURES_NVMM "memory:NVMM"

static void cb_newpad(GstElement *decodebin, GstPad *decoder_src_pad,
                      gpointer data) {
    GstCaps *caps = gst_pad_get_current_caps(decoder_src_pad);

    if (!caps) {
        caps = gst_pad_query_caps(decoder_src_pad, nullptr);
    }

    const GstStructure *str = gst_caps_get_structure(caps, 0);

    const gchar *name = gst_structure_get_name(str);

    GstElement *source_bin = static_cast<GstElement *>(data);

    GstCapsFeatures *features = gst_caps_get_features(caps, 0);

    if (!std::strncmp(name, "video", 5)) {

        if (gst_caps_features_contains(features, GST_CAPS_FEATURES_NVMM)) {

            GstPad *bin_ghost_pad =
                gst_element_get_static_pad(source_bin, "src");

            if (!gst_ghost_pad_set_target(GST_GHOST_PAD(bin_ghost_pad),
                                          decoder_src_pad)) {

                std::cerr << "Failed to link decoder src "
                          << "pad to source bin ghost pad\n";
            }

            gst_object_unref(bin_ghost_pad);

        } else {

            std::cerr << "Error: Decodebin did not pick "
                      << "NVIDIA decoder plugin.\n";
        }
    }

    gst_caps_unref(caps);
}

GstElement *create_source_bin(guint index, gchar *uri, gboolean perf_mode) {
    GstElement *bin = nullptr;
    GstElement *uri_decode_bin = nullptr;

    gchar bin_name[16] = {};

    g_snprintf(bin_name, 15, "source-bin-%02d", index);

    bin = gst_bin_new(bin_name);

    if (perf_mode) {

        uri_decode_bin =
            gst_element_factory_make("nvurisrcbin", "uri-decode-bin");

        g_object_set(G_OBJECT(uri_decode_bin), "file-loop", TRUE, nullptr);

        g_object_set(G_OBJECT(uri_decode_bin), "cudadec-memtype", 0, nullptr);

    } else {

        uri_decode_bin =
            gst_element_factory_make("uridecodebin", "uri-decode-bin");
    }

    if (!bin || !uri_decode_bin) {

        std::cerr << "One element in source bin "
                  << "could not be created.\n";

        return nullptr;
    }

    g_object_set(G_OBJECT(uri_decode_bin), "uri", uri, nullptr);

    g_signal_connect(G_OBJECT(uri_decode_bin), "pad-added",
                     G_CALLBACK(cb_newpad), bin);

    gst_bin_add(GST_BIN(bin), uri_decode_bin);

    if (!gst_element_add_pad(bin,
                             gst_ghost_pad_new_no_target("src", GST_PAD_SRC))) {

        std::cerr << "Failed to add ghost pad "
                  << "in source bin\n";

        return nullptr;
    }

    return bin;
}