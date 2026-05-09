#ifndef SOURCE_BIN_H
#define SOURCE_BIN_H

#include <gst/gst.h>

GstElement* create_source_bin(guint index,
                              gchar *uri,
                              gboolean perf_mode);

#endif