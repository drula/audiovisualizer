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
 * gst-launch-1.0 -v audiotestsrc ! audioconvert ! audiovisualizer ! videoconvert ! ximagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include "gstaudiovisualizer.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

//!
#define SCOPE_WIDTH 400
#define SCOPE_HEIGHT 200

GST_DEBUG_CATEGORY_STATIC (gst_audiovisualizer_debug);
#define GST_CAT_DEFAULT gst_audiovisualizer_debug

/* Filter signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

/* properties */
enum {
  PROP_0,
  //! PROP_SILENT
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_VIDEOPATTERN,
  PROP_COLOR,
  PROP_BGCOLOR,
};

#define GST_TYPE_AUDIOVISUALIZER_VIDEOPATTERN (gst_audiovisualizer_videopattern_get_type ())

static GType
gst_audiovisualizer_videopattern_get_type (void)
{
  static GType videopattern_type = 0;

  if (!videopattern_type) {
    static GEnumValue pattern_types[] = {
      { VIDEOPATTERN_WAVE, "Simple wave", "wave" },
      { VIDEOPATTERN_CIRCLE, "Amplitude circle", "circle"},
      { 0, NULL, NULL },
    };

    videopattern_type = g_enum_register_static ("VideoPattern",
        pattern_types);
  }

  return videopattern_type;
}


/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", " // native endianness
        "rate = (int) [ 8000, 96000 ], " /*!?*/
        "channels = (int) 1, " /*! {1, 2}*/
        "layout = (string) interleaved")
    );
/* {x1, x2} - list, [x1, x2] - range */
/* xxx ; yyy - several multimedia types */

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) " RGB_ORDER ", "
        "width = " G_STRINGIFY (SCOPE_WIDTH) ", "
        "height = " G_STRINGIFY (SCOPE_HEIGHT) ", "
        "framerate = " GST_VIDEO_FPS_RANGE) // (fraction) [ 0, max ]
    );

#define gst_audiovisualizer_parent_class parent_class
/* Setup the GObject basics so that all functions will be called appropriately. */
G_DEFINE_TYPE (GstAudiovisualizer, gst_audiovisualizer, GST_TYPE_ELEMENT);

static void gst_audiovisualizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audiovisualizer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_audiovisualizer_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
//! static gboolean gst_audiovisualizer_src_event (GstPad * pad, GstObject * parent, GstEvent * event);
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

  gobject_class->set_property = gst_audiovisualizer_set_property;
  gobject_class->get_property = gst_audiovisualizer_get_property;

  g_object_class_install_property(gobject_class, PROP_WIDTH,
    g_param_spec_int ("width", "Width", "Video frame width",
      32, 512, SCOPE_WIDTH, G_PARAM_READWRITE /*G_PARAM_CONSTRUCT_ONLY ?*/));

  g_object_class_install_property(gobject_class, PROP_HEIGHT,
    g_param_spec_int ("height", "Height", "Video frame height",
      32, 512, SCOPE_HEIGHT, G_PARAM_READWRITE /*G_PARAM_CONSTRUCT_ONLY ?*/));

  g_object_class_install_property (gobject_class, PROP_VIDEOPATTERN,
    g_param_spec_enum ("pattern", "Pattern", "Video pattern",
      GST_TYPE_AUDIOVISUALIZER_VIDEOPATTERN, VIDEOPATTERN_WAVE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS /*!?*/));

  g_object_class_install_property (gobject_class, PROP_COLOR,
    g_param_spec_uint ("color", "Color", "Drawing color",
      0, G_MAXUINT32, G_MAXUINT32, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_BGCOLOR,
    g_param_spec_uint ("bgcolor", "BgColor", "Background color",
      0, G_MAXUINT32, 0, G_PARAM_READWRITE));

  /*g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));*/

  //! gobject_class->finalize = ?
  //! gstelement_class->change_state = ?

  gst_element_class_set_static_metadata(gstelement_class,
    "Audiovisualizer",
    "Visualization",
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
  /*gst_pad_set_event_function (filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_audiovisualizer_src_event));*/

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  /* properties initial value */
  //!? filter->silent = FALSE;

  //! filter->adapter = gst_adapter_new ();
  filter->next_ts = GST_CLOCK_TIME_NONE;
  filter->bps = sizeof (gint16);

  /* reset the initial video state */
  filter->width = SCOPE_WIDTH;
  filter->height = SCOPE_HEIGHT;
  filter->fps_num = 25;      /* desired frame rate */
  filter->fps_denom = 1;
  filter->pattern = VIDEOPATTERN_WAVE;
  filter->color = G_MAXUINT32;
  filter->bgcolor = 0;
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


static void
gst_audiovisualizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (object);

  switch (prop_id) {
    case PROP_WIDTH:
      filter->width = g_value_get_int (value);
      break;

    case PROP_HEIGHT:
      filter->height = g_value_get_int (value);
      break;

    case PROP_VIDEOPATTERN:
      filter->pattern = g_value_get_enum (value);
      break;

    case PROP_COLOR:
      filter->color = g_value_get_uint (value);
      break;

    case PROP_BGCOLOR:
      filter->bgcolor = g_value_get_uint (value);
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
    case PROP_WIDTH:
      g_value_set_int (value, filter->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, filter->height);
      break;

    case PROP_VIDEOPATTERN:
      g_value_set_enum (value, filter->pattern);
      break;

    case PROP_COLOR:
      g_value_set_uint (value, filter->color);
      break;

    case PROP_BGCOLOR:
      g_value_set_uint (value, filter->bgcolor);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

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
  GstAudiovisualizer *filter = GST_AUDIOVISUALIZER (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;
      gst_event_parse_caps (event, &caps);
      gst_event_unref (event); //!?
      return gst_audiovisualizer_src_setcaps(filter, caps);
      //! The event should be unreffed with gst_event_unref() if it has not been sent.
    }

    /*case GST_EVENT_FLUSH_START: //!?
      return gst_pad_push_event (filter->srcpad, event);

    case GST_EVENT_FLUSH_STOP: //!?
      return gst_pad_push_event (filter->srcpad, event);

    case GST_EVENT_SEGMENT: //!?
      / * the newsegment values are used to clip the input samples
       * and to convert the incomming timestamps to running time so
       * we can do QoS * /
      gst_event_copy_segment (event, &filter->segment);
      return gst_pad_push_event (filter->srcpad, event);*/

    case GST_EVENT_EOS:
      //! gst_audiovisualizer_stop_processing (filter);
      return gst_pad_event_default (pad, parent, event);
    
    default:
      return gst_pad_event_default (pad, parent, event);
  }
}

/*static gboolean
gst_audiovisualizer_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return TRUE;
}*/

/* converting audiodata to video */
static guint32 *
gst_audiovisualizer_process_data (GstAudiovisualizer * filter, GstBuffer * inbuf)
{
  GstMapInfo inbuf_info;
  gst_buffer_map (inbuf, &inbuf_info, GST_MAP_READ /*!?*/);
  
  static guint32 pixels[SCOPE_WIDTH * SCOPE_HEIGHT];

  gint16 *data = (gint16 *) inbuf_info.data;
  gsize size = inbuf_info.size / (sizeof (guint16) / sizeof (guint8));

  switch (filter->pattern) {
  case VIDEOPATTERN_WAVE: {
    int i;
    for (i = 0; i < SCOPE_WIDTH * SCOPE_HEIGHT; ++i)
      pixels[i] = filter->bgcolor;

    gdouble samples_per_pixel = (gdouble) size / SCOPE_WIDTH;

    double start = 0;
    double finish;
    long istart, ifinish;
    istart = 0;
    
    int x = 0;
    while (start < size) {
      finish = start + samples_per_pixel;  
      ifinish = lround (finish);

      int i;
      gdouble sum = 0;
      for (i = istart; i < ifinish; ++i)
        sum += (gdouble) data[i];
      sum /= ifinish - istart;

      long y = SCOPE_HEIGHT / 2 + (sum / G_MAXUINT16) * SCOPE_HEIGHT / 2;
      pixels[y * SCOPE_WIDTH + x] = filter->color;

      start = finish;
      istart = ifinish;
      ++x;
    }

    break;
  }

  case VIDEOPATTERN_CIRCLE: {
    int xcenter = SCOPE_WIDTH / 2;
    int ycenter = SCOPE_HEIGHT / 2;

    int i;
    guint16 max = 0;
    for (i = 0; i < size; ++i)
      if (max < data[i])
        max = data[i];

    int radius = (int) round(((gdouble) max / G_MAXUINT16) * (SCOPE_HEIGHT / 2));

    int x, y;
    for (x = 0; x < SCOPE_WIDTH; ++x) {
      for (y = 0; y < SCOPE_HEIGHT; ++y) {
        int x_ = abs(x - xcenter);
        int y_ = abs(y - ycenter);
        pixels[y * SCOPE_WIDTH + x] = x_ * x_ + y_ * y_ <= radius * radius ? filter->color : filter->bgcolor;
      }
    }

    break;
  }

  default:
    break;
  }

  gst_buffer_unmap (inbuf, &inbuf_info);

  return pixels;
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
 
  GstBuffer *outbuf = NULL;
  GST_LOG_OBJECT (filter, "allocating output buffer");
  flow_ret = gst_buffer_pool_acquire_buffer (filter->pool, &outbuf, NULL);
  if (flow_ret != GST_FLOW_OK) {
    //! gst_adapter_unmap (filter->adapter);
    return flow_ret;
  }

  guint32 *pixels = gst_audiovisualizer_process_data(filter, inbuf);
  gst_buffer_fill (outbuf, 0, pixels, filter->outsize);
  gst_buffer_unref (inbuf); //!

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
plugin_init (GstPlugin * audiovisualizer)
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
#define PACKAGE "audiovisualizer"
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
    plugin_init,
    "1.0",
    "LGPL",
    PACKAGE,
    "https://github.com/drula/audiovisualizer"
)
