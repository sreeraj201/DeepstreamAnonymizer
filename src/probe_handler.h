#pragma once

#include <gst/gst.h>

class ProbeHandler {
public:
    static GstPadProbeReturn buffer_probe(
        GstPad* pad,
        GstPadProbeInfo* info,
        gpointer user_data);
};