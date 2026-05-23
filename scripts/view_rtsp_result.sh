#!/bin/bash
gst-launch-1.0 rtspsrc location=rtsp://localhost:8555/ds-test ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvidconv ! nveglglessink sync=0
