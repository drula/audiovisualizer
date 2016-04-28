/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2016 Andrey Dudinov <adudinov@yandex.by>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-audiovisualizer
 *
 * FIXME:Describe audiovisualizer here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! audiovisualizer ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include "gstaudiovisualizer.h"

GST_DEBUG_CATEGORY_STATIC (gst_audiovisualizer_debug);
#define GST_CAT_DEFAULT gst_audiovisualizer_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      "audio/x-raw, "  /* media type */
      "format = (string) "
        GST_AUDIO_NE (S16) ", "  /*! 16 bit, stereo (?) */
      "channels = (int) {1, 2}, "  /* list of all possible values */
      "rate = (int) [8000, 96000]"  /* range of all possible values */
      )
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      "video/x-raw, "
      "width = 640, "
      "height = 480"
      /* "format = " */
      )
    );

#define gst_audiovisualizer_parent_class parent_class
/* Setup the GObject basics so that all functions will be called appropriately. */
G_DEFINE_TYPE (GstAudiovisualizer, gst_audiovisualizer, GST_TYPE_ELEMENT);

static void gst_audiovisualizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audiovisualizer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_audiovisualizer_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_audiovisualizer_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the audiovisualizer's class, registering the element details */
static void
gst_audiovisualizer_class_init (GstAudiovisualizerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_audiovisualizer_set_property;
  gobject_class->get_property = gst_audiovisualizer_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  /* Metadata */
  gst_element_class_set_details_simple(gstelement_class,
    "Audiovisualizer plugin",
    "FIXME:Generic",
    "Converts audio streaming data to video stream",
    "Andrey Dudinov <adudinov@yandex.by>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_audiovisualizer_init (GstAudiovisualizer * filter)
{
  /* pad through which data comes in to the element */
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");

  /* pads are configured here with gst_pad_set_*_function () */
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_audiovisualizer_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_audiovisualizer_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  /* pad through which data goes out of the element */
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");

  /*! pads are configured here with gst_pad_set_*_function () */
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  /* properties initial value */
  filter->silent = FALSE;
}

static void
gst_audiovisualizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audiovisualizer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_audiovisualizer_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAudiovisualizer *filter;
  gboolean ret;

  filter = GST_AUDIOVISUALIZER (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* converting audiodata to video */
static GstBuffer *
gst_audiovisualizer_process_data (GstAudiovisualizer * filter, GstBuffer * buf)
{
  return NULL;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_audiovisualizer_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstAudiovisualizer *filter;
  GstBuffer *outbuf;

  filter = GST_AUDIOVISUALIZER (parent);
  outbuf = gst_audiovisualizer_process_data (filter, buf);
  gst_buffer_unref (buf);

  if (!outbuf) {
    GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, FAILED, (NULL), (NULL));
    return GST_FLOW_ERROR;
  }

  return gst_pad_push (filter->srcpad, outbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
audiovisualizer_init (GstPlugin * audiovisualizer)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template audiovisualizer' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_audiovisualizer_debug, "audiovisualizer",
      0, "Template audiovisualizer");

  return gst_element_register (audiovisualizer, "audiovisualizer", GST_RANK_NONE,
      GST_TYPE_AUDIOVISUALIZER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstaudiovisualizer"
#endif

/* gstreamer looks for this structure to register audiovisualizers
 *
 * exchange the string 'Template audiovisualizer' with your audiovisualizer description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    audiovisualizer,
    "Audiovisualizer plugin",
    audiovisualizer_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
