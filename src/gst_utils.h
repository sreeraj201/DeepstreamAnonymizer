#pragma once

#include <gst/gst.h>

#define CHECK_ELEMENT(elem)                                      \
    if (!(elem)) {                                               \
        g_printerr("Failed to create: %s\n", #elem);             \
        return false;                                            \
    }

#define LINK_ELEMENTS(src, sink)                                 \
    if (!gst_element_link((src), (sink))) {                      \
        g_printerr("Failed link: %s -> %s\n",                    \
            GST_ELEMENT_NAME(src),                               \
            GST_ELEMENT_NAME(sink));                             \
        return false;                                            \
    }