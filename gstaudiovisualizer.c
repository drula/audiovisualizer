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
#include <config.h>
#endif

#include <gst/audio/audio.h>
#include <gst/video/video.h>
//! #include <gst/base/gstadapter.h>
#include "gstaudiovisualizer.h"

#include <stdio.h> /* !!! */
#include <stdlib.h>
#include <string.h>
#include <math.h>

//!
#define scope_width 256
#define scope_height 128
#define convolver_depth 8
#define convolver_small (1 << convolver_depth)
#define convolver_big (2 << convolver_depth)

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
  PROP_SILENT /*! ? */
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) " RGB_ORDER ", "
        "width = " G_STRINGIFY (scope_width) ", "
        "height = " G_STRINGIFY (scope_height) ", "
        "framerate = " GST_VIDEO_FPS_RANGE)
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 1, " "layout = (string) interleaved")
    );

#define gst_audiovisualizer_parent_class parent_class
/* Setup the GObject basics so that all functions will be called appropriately. */
G_DEFINE_TYPE (GstAudiovisualizer, gst_audiovisualizer, GST_TYPE_ELEMENT);

/*static void gst_audiovisualizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audiovisualizer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);*/

static gboolean gst_audiovisualizer_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static gboolean gst_audiovisualizer_src_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_audiovisualizer_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static gboolean gst_audiovisualizer_sink_setcaps (GstAudiovisualizer * filter, GstCaps * caps);
static gboolean gst_audiovisualizer_src_setcaps (GstAudiovisualizer * filter, GstCaps * caps);

/* GObject vmethod implementations */

/* initialize the audiovisualizer's class, registering the element details */
static void
gst_audiovisualizer_class_init (GstAudiovisualizerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  /*gobject_class->set_property = gst_audiovisualizer_set_property;
  gobject_class->get_property = gst_audiovisualizer_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));*/

  //! gobject_class->finalize = ?
  //! gstelement_class->change_state = ?

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
  //!? GST_PAD_SET_PROXY_CAPS (filter->sinkpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  /*gst_pad_set_setcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR (gst_audiovisualizer_src_setcaps));*/

  /* pad through which data goes out of the element */
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");

  /*! pads are configured here with gst_pad_set_*_function () */
  //!? GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_pad_set_event_function (filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_audiovisualizer_src_event));

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  /* properties initial value */
  //!? filter->silent = FALSE;

  //! filter->adapter = gst_adapter_new ();
  filter->next_ts = GST_CLOCK_TIME_NONE;
  filter->bps = sizeof (gint16);

  /* reset the initial video state */
  filter->width = scope_width;
  filter->height = scope_height;
  filter->fps_num = 25;      /* desired frame rate */
  filter->fps_denom = 1;
  //! filter->visstate = NULL;

  /* reset the initial audio state */
  filter->rate = GST_AUDIO_DEF_RATE;
}

static void
gst_audiovisualizer_finalize (GObject * object)
{
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (object);

  /*if (filter->visstate)
    audiovisualizer_close (filter->visstate);*/

  //! g_object_unref (filter->adapter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
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
}*/

static gboolean
gst_audiovisualizer_sink_setcaps (GstAudiovisualizer * filter, GstCaps * caps)
{
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "rate", &filter->rate);
  GST_DEBUG_OBJECT (filter, "sample rate = %d", filter->rate);
  return TRUE;
}

static gboolean
gst_audiovisualizer_src_setcaps (GstAudiovisualizer * filter, GstCaps * caps)
{
  GstStructure *structure = gst_caps_get_structure (caps, 0);
   
  gst_structure_get_int (structure, "width", &filter->width);
  gst_structure_get_int (structure, "height", &filter->height);
  gst_structure_get_fraction (structure, "framerate", &filter->fps_num,
      &filter->fps_denom);

  filter->outsize = filter->width * filter->height * 4;
  filter->frame_duration = gst_util_uint64_scale_int (GST_SECOND,
      filter->fps_denom, filter->fps_num);
  filter->spf =
      gst_util_uint64_scale_int (filter->rate, filter->fps_denom,
      filter->fps_num);

  GST_DEBUG_OBJECT (filter, "dimension %dx%d, framerate %d/%d, spf %d",
      filter->width, filter->height, filter->fps_num,
      filter->fps_denom, filter->spf);

  //! monoscope->visstate = monoscope_init (monoscope->width, monoscope->height);
  
  return gst_pad_set_caps (filter->srcpad, caps);;
}

static gboolean //!
gst_audiovisualizer_src_negotiate (GstAudiovisualizer * filter)
{
  GstCaps *othercaps, *target;
  GstStructure *structure;
  GstCaps *templ;
  GstQuery *query;
  GstBufferPool *pool;  
  GstStructure *config;
  guint size, min, max;

  templ = gst_pad_get_pad_template_caps (filter->srcpad);

  GST_DEBUG_OBJECT (filter, "performing negotiation");

  /* see what the peer can do */
  othercaps = gst_pad_peer_query_caps (filter->srcpad, NULL);
  if (othercaps) {
    target = gst_caps_intersect (othercaps, templ);
    gst_caps_unref (othercaps);
    gst_caps_unref (templ);

    if (gst_caps_is_empty (target))
      goto no_format;

    target = gst_caps_truncate (target);
  } else {
    target = templ;
  }

  target = gst_caps_make_writable (target);
  structure = gst_caps_get_structure (target, 0);
  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 25, 1);

  gst_audiovisualizer_src_setcaps (filter, target);

  /* try to get a bufferpool now */
  /* find a pool for the negotiated caps now */
  query = gst_query_new_allocation (target, TRUE);

  if (!gst_pad_peer_query (filter->srcpad, query)) {
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    /* we got configuration from our peer, parse them */
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
  } else {
    pool = NULL;
    size = filter->outsize;
    min = max = 0;
  }

  if (pool == NULL) {
    /* we did not get a pool, make one ourselves then */
    pool = gst_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, target, size, min, max);
  gst_buffer_pool_set_config (pool, config);

  if (filter->pool) {
    gst_buffer_pool_set_active (filter->pool, TRUE);
    gst_object_unref (filter->pool);
  }
  filter->pool = pool;

  /* and activate */
  gst_buffer_pool_set_active (pool, TRUE);

  gst_caps_unref (target);

  return TRUE;

no_format:
  {
    gst_caps_unref (target);
    return FALSE;
  }
}

/* make sure we are negotiated */
static GstFlowReturn
ensure_negotiated (GstAudiovisualizer * filter)
{
  gboolean reconfigure;

  reconfigure = gst_pad_check_reconfigure (filter->srcpad);

  /* we don't know an output format yet, pick one */
  if (reconfigure || !gst_pad_has_current_caps (filter->srcpad)) {
    if (!gst_audiovisualizer_src_negotiate (filter))
      return GST_FLOW_NOT_NEGOTIATED;
  }
  return GST_FLOW_OK;
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
      //! ret = gst_audiovisualizer_src_setcaps(filter, caps);
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

static gboolean
gst_audiovisualizer_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return TRUE;
}

/* converting audiodata to video */
static GstBuffer *
gst_audiovisualizer_process_data (GstAudiovisualizer * filter, GstBuffer * inbuf)
{
  GstMapInfo inbuf_info;
  gst_buffer_map (inbuf, &inbuf_info, GST_MAP_READ /*!?*/);

  GstBuffer *outbuf = gst_buffer_new ();

  GstMemory *mem = gst_allocator_alloc (NULL, inbuf_info.size, NULL);
  gst_buffer_append_memory (outbuf, mem);

  GstMapInfo outbuf_info;
  gst_buffer_map (outbuf, &outbuf_info, GST_MAP_WRITE);

  memcpy (outbuf_info.data, inbuf_info.data, inbuf_info.size);

  int i;
  for (i = 0; i < inbuf_info.size; ++i)
    outbuf_info.data[i] = inbuf_info.data[i] * 2;

  gst_buffer_unmap (inbuf, &inbuf_info);
  gst_buffer_unmap (outbuf, &outbuf_info);

  return outbuf;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_audiovisualizer_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (parent);
  GstFlowReturn flow_ret = GST_FLOW_OK;

  if (filter->rate == 0) {
    gst_buffer_unref (inbuf);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  flow_ret = ensure_negotiated (filter);
  if (flow_ret != GST_FLOW_OK) {
    gst_buffer_unref (inbuf);
    return flow_ret;
  }

  /* don't try to combine samples from discont buffer */
  if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DISCONT)) {
    //! gst_adapter_clear (filter->adapter);
    filter->next_ts = GST_CLOCK_TIME_NONE;
  }

  if (GST_BUFFER_TIMESTAMP (inbuf) != GST_CLOCK_TIME_NONE)
    filter->next_ts = GST_BUFFER_TIMESTAMP (inbuf);

  GstMapInfo inbuf_info;
  gst_buffer_map (inbuf, &inbuf_info, GST_MAP_READ /*!?*/);
  guint32 *pixels = calloc(scope_width * scope_height, sizeof (guint32));

  gint16 *data = (gint16 *) inbuf_info.data;
  gsize size = inbuf_info.size / 2; //! (sizeof (guint16) / sizeof (guint8));
  gdouble samples_per_pixel = (gdouble) size / scope_width;

  double start = 0;
  double finish;
  long istart, ifinish;
  istart = 0;
  
  int ip = 0;
  while (start < size) {
    finish = start + samples_per_pixel;  
    ifinish = lround (finish);

    int i;
    gdouble sum = 0;
    for (i = istart; i < ifinish; ++i)
      sum += (gdouble) data[i];
    sum /= ifinish - istart;

    // long x = ip;
    long y = scope_height / 2 + (sum / G_MAXUINT16) * scope_height / 2;
    pixels[y * scope_width + ip] = G_MAXUINT32;

    start = finish;
    istart = ifinish;
    ++ip;
  }
 
  GstBuffer *outbuf = NULL;
  GST_LOG_OBJECT (filter, "allocating output buffer");
  flow_ret = gst_buffer_pool_acquire_buffer (filter->pool, &outbuf, NULL);
  if (flow_ret != GST_FLOW_OK) {
    //! gst_adapter_unmap (filter->adapter);
    return flow_ret;
  }

  gst_buffer_fill (outbuf, 0, pixels, filter->outsize);

  GST_BUFFER_TIMESTAMP (outbuf) = filter->next_ts;
  GST_BUFFER_DURATION (outbuf) = filter->frame_duration;

  flow_ret = gst_pad_push (filter->srcpad, outbuf);
  return flow_ret;
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
