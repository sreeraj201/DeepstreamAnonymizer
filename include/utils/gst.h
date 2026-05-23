#pragma once

#include <gst/gst.h>

inline bool check_element(GstElement *element, const char *name) {
    if (!element) {

        g_printerr("Failed to create element: %s\n", name);

        return false;
    }

    return true;
}

inline bool link_elements(GstElement *src, GstElement *sink) {
    if (!gst_element_link(src, sink)) {

        g_printerr("Failed to link: %s -> %s\n", GST_ELEMENT_NAME(src),
                   GST_ELEMENT_NAME(sink));

        return false;
    }

    return true;
}