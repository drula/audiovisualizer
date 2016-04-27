# audiovisualizer
GStreamer plugin converting audio to video


Compilation (dynamic library):

gcc -fPIC gstaudiovisualizer.c -c \`pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-base-1.0\`

sudo gcc -shared -o /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstaudiovisualizer.so gstaudiovisualizer.o


Or just launch compile.sh.


Usage:

gst-launch-1.0 \<plugins chain with audio output\> ! audiovisualizer ! \<plugins chain with video input\>
