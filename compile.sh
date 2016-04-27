#!/bin/bash

gcc -fPIC gstaudiovisualizer.c -c `pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-base-1.0`
sudo gcc -shared -o /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstaudiovisualizer.so gstaudiovisualizer.o
