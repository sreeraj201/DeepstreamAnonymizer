#pragma once

#include <gst/gst.h>

GstElement *create_source_bin(guint index, gchar *uri, gboolean perf_mode);