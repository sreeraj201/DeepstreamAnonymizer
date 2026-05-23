#pragma once

#include <gst/gst.h>

#include "config.h"
#include "face_recognition.h"

class ProbeHandler {
  public:
    struct Context {
        AppConfig *config = nullptr;
        FaceRecognition *face_recognition = nullptr;
    };

    static GstPadProbeReturn buffer_probe(GstPad *pad, GstPadProbeInfo *info,
                                          gpointer user_data);
};