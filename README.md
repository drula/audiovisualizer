# audiovisualizer
GStreamer plugin converting audio to video


Installation:
sudo make install


Uninstallation:
sudo make unistall


Usage:

gst-launch-1.0 \<plugins chain with audio output\> ! audiovisualizer ! \<plugins chain with video input\>

Example:
gst-launch-1.0 autoaudiosrc ! audiovisualizer ! videoconvert ! autovideosink
