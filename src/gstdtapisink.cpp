/*
 * GStreamer
 * Copyright (C) 2012 YouView TV Ltd. <william.manley@youview.com>
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
 * SECTION:element-dtapisink
 *
 * Provides a sink for DekTec modulator hardware using the proprietary DecTek
 * DTAPI C++ API.  Tested using a DekTec DTU-215 USB modulator.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 filesrc location=Mux1.ts ! dtapisink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/*#define DTAPI_DEBUG 1*/

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include "DTAPI.h"
#include <stdio.h>
#include <assert.h>

/* FIXME: what is a sane default?: */
#define DEFAULT_BITRATE 24128342
#define DEFAULT_FREQUENCY 474000000
#define DEFAULT_OUTPUT_POWER -495 /* /0.1dBm */
#define DEFAULT_CODE_RATE DTAPI_MOD_2_3
#define DEFAULT_BANDWIDTH DTAPI_MOD_DVBT_8MHZ
#define DEFAULT_MODULATION DTAPI_MOD_DVBT_QAM64
#define DEFAULT_GUARD DTAPI_MOD_DVBT_G_1_32
#define DEFAULT_INTERLEAVING DTAPI_MOD_DVBT_NATIVE
#define DEFAULT_TRANSMISSION_MODE DTAPI_MOD_DVBT_8K
#define DEFAULT_INVERSION 0
#define DEFAULT_TXMODE DTAPI_TXMODE_188
#define DEFAULT_STUFFING 1

#define GST_TYPE_DTAPISINK_CODE_RATE (gst_dtapisink_code_rate_get_type ())
static GType
gst_dtapisink_code_rate_get_type (void)
{
  static GType dtapisink_code_rate_type = 0;
  static GEnumValue code_rate_types[] = {
    {DTAPI_MOD_1_2,  "1/2",  "1/2"},
    {DTAPI_MOD_2_3,  "2/3",  "2/3"},
    {DTAPI_MOD_3_4,  "3/4",  "3/4"},
    {DTAPI_MOD_4_5,  "4/5",  "4/5"},
    {DTAPI_MOD_5_6,  "5/6",  "5/6"},
    {DTAPI_MOD_6_7,  "6/7",  "6/7"},
    {DTAPI_MOD_7_8,  "7/8",  "7/8"},
    {DTAPI_MOD_1_4,  "1/4",  "1/4"},
    {DTAPI_MOD_1_3,  "1/3",  "1/3"},
    {DTAPI_MOD_2_5,  "2/5",  "2/5"},
    {DTAPI_MOD_3_5,  "3/5",  "3/5"},
    {DTAPI_MOD_8_9,  "8/9",  "8/9"},
    {DTAPI_MOD_9_10, "9/10", "9/10"},
    {0, NULL, NULL},
  };

  if (!dtapisink_code_rate_type) {
    dtapisink_code_rate_type =
        g_enum_register_static ("GstDTAPISinkCode_Rate", code_rate_types);
  }
  return dtapisink_code_rate_type;
}

#define GST_TYPE_DTAPISINK_MODULATION (gst_dtapisink_modulation_get_type ())
static GType
gst_dtapisink_modulation_get_type (void)
{
  static GType dtapisink_modulation_type = 0;
  static GEnumValue modulation_types[] = {
    {DTAPI_MOD_DVBT_QPSK,   "QPSK",    "qpsk"},
    {DTAPI_MOD_DVBT_QAM16,  "QAM 16",  "qam-16"},
    {DTAPI_MOD_DVBT_QAM64,  "QAM 64",  "qam-64"},
    {0, NULL, NULL},
  };

  if (!dtapisink_modulation_type) {
    dtapisink_modulation_type =
        g_enum_register_static ("GstDTAPISinkModulation", modulation_types);
  }
  return dtapisink_modulation_type;
}

#define GST_TYPE_DTAPISINK_TRANSMISSION_MODE (gst_dtapisink_transmission_mode_get_type ())
static GType
gst_dtapisink_transmission_mode_get_type (void)
{
  static GType dtapisink_transmission_mode_type = 0;
  static GEnumValue transmission_mode_types[] = {
    {DTAPI_MOD_DVBT_2K, "2K", "2k"},
    {DTAPI_MOD_DVBT_4K, "4K", "4k"},
    {DTAPI_MOD_DVBT_8K, "8K", "8k"},
    {0, NULL, NULL},
  };

  if (!dtapisink_transmission_mode_type) {
    dtapisink_transmission_mode_type =
        g_enum_register_static ("GstDTAPISinkTransmission_Mode",
        transmission_mode_types);
  }
  return dtapisink_transmission_mode_type;
}

#define GST_TYPE_DTAPISINK_BANDWIDTH (gst_dtapisink_bandwidth_get_type ())
static GType
gst_dtapisink_bandwidth_get_type (void)
{
  static GType dtapisink_bandwidth_type = 0;
  static GEnumValue bandwidth_types[] = {
    {DTAPI_MOD_DVBT_5MHZ, "5", "5"},
    {DTAPI_MOD_DVBT_6MHZ, "6", "6"},
    {DTAPI_MOD_DVBT_7MHZ, "7", "7"},
    {DTAPI_MOD_DVBT_8MHZ, "8", "8"},
    {0, NULL, NULL},
  };

  if (!dtapisink_bandwidth_type) {
    dtapisink_bandwidth_type =
        g_enum_register_static ("GstDTAPISinkBandwidth", bandwidth_types);
  }
  return dtapisink_bandwidth_type;
}

#define GST_TYPE_DTAPISINK_GUARD (gst_dtapisink_guard_get_type ())
static GType
gst_dtapisink_guard_get_type (void)
{
  static GType dtapisink_guard_type = 0;
  static GEnumValue guard_types[] = {
    {DTAPI_MOD_DVBT_G_1_32, "32", "32"},
    {DTAPI_MOD_DVBT_G_1_16, "16", "16"},
    {DTAPI_MOD_DVBT_G_1_8,  "8",  "8"},
    {DTAPI_MOD_DVBT_G_1_4,  "4",  "4"},
    {0, NULL, NULL},
  };

  if (!dtapisink_guard_type) {
    dtapisink_guard_type = g_enum_register_static ("GstDTAPISinkGuard", guard_types);
  }
  return dtapisink_guard_type;
}

#define GST_TYPE_DTAPISINK_INVERSION (gst_dtapisink_inversion_get_type ())
static GType
gst_dtapisink_inversion_get_type (void)
{
  static GType dtapisink_inversion_type = 0;
  static GEnumValue inversion_types[] = {
    {0, "OFF", "off"},
    {DTAPI_UPCONV_SPECINV, "ON", "on"},
    {0, NULL, NULL},
  };

  if (!dtapisink_inversion_type) {
    dtapisink_inversion_type =
        g_enum_register_static ("GstDTAPISinkInversion", inversion_types);
  }
  return dtapisink_inversion_type;
}

#define GST_TYPE_DTAPISINK_TXMODE (gst_dtapisink_txmode_get_type ())
static GType
gst_dtapisink_txmode_get_type (void)
{
  static GType dtapisink_txmode_type = 0;
  static GEnumValue txmode_types[] = {
    {DTAPI_TXMODE_188, "188", "188"},
    {DTAPI_TXMODE_192, "192", "192"},
    {DTAPI_TXMODE_204, "204", "204"},
    {DTAPI_TXMODE_ADD16, "ADD16", "ADD16"},
    {DTAPI_TXMODE_MIN16, "MIN16", "MIN16"},
    {DTAPI_TXMODE_RAW, "RAW", "RAW"},
    {0, NULL, NULL},
  };

  if (!dtapisink_txmode_type) {
    dtapisink_txmode_type =
        g_enum_register_static ("GstDTAPISinkTxMode", txmode_types);
  }
  return dtapisink_txmode_type;
}

#define GST_TYPE_DTAPISINK_INTERLEAVING (gst_dtapisink_interleaving_get_type ())
static GType
gst_dtapisink_interleaving_get_type (void)
{
  static GType dtapisink_interleaving_type = 0;
  static GEnumValue interleaving_types[] = {
    {DTAPI_MOD_DVBT_INDEPTH, "INDEPTH", "INDEPTH"},
    {DTAPI_MOD_DVBT_NATIVE,  "NATIVE",  "NATIVE"},
    {0, NULL, NULL},
  };

  if (!dtapisink_interleaving_type) {
    dtapisink_interleaving_type =
        g_enum_register_static ("GstDTAPISinkInterleaving", interleaving_types);
  }
  return dtapisink_interleaving_type;
}

#define GST_TYPE_DTAPISINK_STUFFING (gst_dtapisink_stuffing_get_type ())
static GType
gst_dtapisink_stuffing_get_type (void)
{
  static GType dtapisink_stuffing_type = 0;
  static GEnumValue stuffing_types[] = {
    {0, "none",   "None"},
    {1, "nulls",  "NULL Packets"},
    {0, NULL, NULL},
  };

  if (!dtapisink_stuffing_type) {
    dtapisink_stuffing_type =
        g_enum_register_static ("GstDTAPISinkStuffing", stuffing_types);
  }
  return dtapisink_stuffing_type;
}

static const char* result_to_string(DTAPI_RESULT result)
{
  switch (result) {
  case DTAPI_OK:
    return "Success";
  case DTAPI_E_DEV_DRIVER:
    return "Unclassified failure in device driver";
  case DTAPI_E_INSUF_LOAD:
    return "For modulators: FIFO load is insufficient to start modulation";
  case DTAPI_E_INVALID_LEVEL:
    return "The output level specified in SetOutputLevel is invalid for the "
           "attached hardware function";
  case DTAPI_E_INVALID_MODE:
    return "The specified transmit-control state is invalid or incompatible "
           "with the attached hardware function";
    // FIXME: This is for SetModControl:
    return "Modulation type is incompatible with modulator";
  case DTAPI_E_MODPARS_NOT_SET:
    return "For modulators: modulation parameters have not been set but are "
           "required for starting modulation";
  case DTAPI_E_MODTYPE_UNSUP:
    return "For modulators: modulation type is not supported";
  case DTAPI_E_NO_IPPARS:
    return "For TS-over-IP channels: cannot set transmission state because "
           "IP parameters have not been specified yet";
  case DTAPI_E_NO_TSRATE:
    // FIXME: This is for SetTxControl:
    return "For modulators: TS rate has not been set but is required for "
           "starting modulation";
    // FIXME: And this is for Write:
    return "For TS-over-IP channels: cannot write data because Transport-"
           "Stream rate has not been specified, or is too low";
  case DTAPI_E_NOT_ATTACHED:
    return "Channel object is not attached to a hardware function";
  case DTAPI_E_INVALID_BUF:
    return "The buffer is not aligned to a 32-bit word boundary";
  case DTAPI_E_INVALID_SIZE:
    return "The specified transfer size is negative or not a multiple of four";
  case DTAPI_E_IDLE:
    // FIXME: This is for write
    return "Cannot write data because transmission-control state is"
           "DTAPI_TXCTRL_IDLE";
    // FIXME: And this is for SetModControl:
    return "Transmit-control state is not DTAPI_TXCTRL_IDLE; The requested "
           "modulation parameters can only be set in idle state";
  case DTAPI_E_INVALID_BANDWIDTH:
      return "Invalid value for bandwidth field";
  case DTAPI_E_INVALID_CONSTEL:
      return "Invalid value for constellation field";
  case DTAPI_E_INVALID_FHMODE:
      return "Invalid value for frame-header mode field";
  case DTAPI_E_INVALID_GUARD:
      return "Invalid value for guard-interval field";
  case DTAPI_E_INVALID_INTERLVNG:
      return "Invalid value for interleaving field";
  case DTAPI_E_INVALID_J83ANNEX:
  case DTAPI_E_INVALID_ROLLOFF:
      return "Invalid value for J.83 annex";
  case DTAPI_E_INVALID_PILOTS:
      return "Pilots cannot be specified in C=1 mode";
  case DTAPI_E_INVALID_RATE:
      return "Invalid value for convolutional rate or FEC code rate";
  case DTAPI_E_INVALID_TRANSMODE:
      return "Invalid value for transmission-mode field";
  case DTAPI_E_INVALID_USEFRAMENO:
      return "Invalid value for use-frame-numbering field";
  case DTAPI_E_NOT_SUPPORTED:
      return "The device does not include a modulator";
  default:
    return "Unknown error";
  }
}

/* FIXME: RESOURCE, OPEN_WRITE is inappropriate for the vast majority of errors
   here.  Need to go though and apply the right codes in the right places */
#define CHECK(expr, desc) \
  if ((result = expr) != DTAPI_OK) { \
    GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_WRITE, (NULL), \
                      (desc, result_to_string(result))); \
  }

#define BUFSIZE (512 * 1024)

typedef struct _GstDTAPISink
{
  GstBaseSink base_class;

  GstPad *sinkpad;

  DtDevice* Dvc;
  DtOutpChannel* TsOut;

  /* Each of these mirrors a parameter that must be passed to DTAPI.  We do this
     so we can assign to them before we have even set-up the relevant DTAPI
     objects. */
  int ts_rate_bps;
  int64_t frequency;
  int code_rate;
  int mod_param;
  int rf_mode;
  int tx_mode;
  int stuff_mode;
  int output_power;
} GstDTAPISink;

typedef struct _GstDTAPISinkClass {
  GstBaseSinkClass parent_class;
} GstDTAPISinkClass;

static void
_do_init (GType gst_dtapi_sink_type)
{
}

GST_BOILERPLATE_FULL (GstDTAPISink, gst_dtapi_sink, GstBaseSink,
                      GST_TYPE_BASE_SINK, _do_init);

#define GST_TYPE_DTAPI_SINK \
  (gst_dtapi_sink_get_type())
#define GST_DTAPI_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTAPI_SINK,GstDTAPISink))
#define GST_DTAPI_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DTAPI_SINK,GstDTAPISinkClass))
#define GST_IS_DTAPI_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTAPI_SINK))
#define GST_IS_DTAPI_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DTAPI_SINK))

static void gst_dtapi_sink_set_property (GObject * object, guint prop_id,
                                         const GValue * value,
                                         GParamSpec * pspec);
static void gst_dtapi_sink_get_property (GObject * object, guint prop_id,
                                         GValue * value, GParamSpec * pspec);
static gboolean      gst_dtapi_sink_start       (GstBaseSink *sink);
static GstFlowReturn gst_dtapi_sink_render      (GstBaseSink *sink,
                                                 GstBuffer *buffer);
static gboolean      gst_dtapi_sink_unlock      (GstBaseSink *sink);
static gboolean      gst_dtapi_sink_stop_unlock (GstBaseSink *sink);
static gboolean      gst_dtapi_sink_stop        (GstBaseSink *sink);

enum
{
  PROP_0,
  PROP_BITRATE,
  
  /* SetRfControl: */
  PROP_DTAPISINK_FREQUENCY,

  /* SetOutputLevel */
  PROP_DTAPISINK_OUTPUT_POWER, /* <- Not in dvbsrc */
  
  /* SetModControl*/
  /* ParXtra0 */
  PROP_DTAPISINK_CODE_RATE,

  /* ParXtra1 */
  PROP_DTAPISINK_BANDWIDTH,
  PROP_DTAPISINK_MODULATION,
  PROP_DTAPISINK_GUARD,
  PROP_DTAPISINK_INTERLEAVING, /* <- Not in dvbsrc */
  PROP_DTAPISINK_TRANSMISSION_MODE,

  /* SetRfMode */
  PROP_DTAPISINK_INVERSION,

  /* SetTxMode */
  /* FIXME: rename TXMODE to avoid confusion with TRANSMISSION_MODE.  Perhaps
     to something like PACKET_SIZE or PACKET_MODE or something */
  PROP_DTAPISINK_TXMODE,
  PROP_DTAPISINK_STUFFING,

#if 0
  /* GetFifoLoad */
  PROP_FIFO_LOAD,

  /* GetFifoSize */
  PROP_FIFO_SIZE,

  /* GetFifoSizeMax */
  PROP_FIFO_SIZE_MAX,

  /* GetFifoSizeTyp */
  PROP_FIFO_SIZE_TYPICAL,

  /* UNKNOWN from dvbsrc */
  PROP_DTAPISINK_POLARITY,
  PROP_DTAPISINK_SYM_RATE,
  PROP_DTAPISINK_TUNE,
  PROP_DTAPISINK_DISEQC_SRC,

  /* TODO: Work out how hierarchy fits into all of this: */
  PROP_DTAPISINK_HIERARCHY_INF,
  PROP_DTAPISINK_CODE_RATE_HP,
  PROP_DTAPISINK_CODE_RATE_LP,

#endif

  PROP_LAST
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/mpegts, mpegversion = (int) 2"));

static void
gst_dtapi_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "DekTec DTAPI Sink",
      "Sink/Modulator",
      "Write data to a DekTec modulator",
      "William Manley <william.manley@youview.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
}

static void
gst_dtapi_sink_class_init (GstDTAPISinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = gst_dtapi_sink_set_property;
  gobject_class->get_property = gst_dtapi_sink_get_property;

  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_dtapi_sink_render);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_dtapi_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_dtapi_sink_stop);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_dtapi_sink_unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_dtapi_sink_stop_unlock);

  // TODO: Set this with caps rather than as a property:
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "bitrate", "Bitrate",
          0, G_MAXINT, DEFAULT_BITRATE, (GParamFlags) G_PARAM_READWRITE));

  /* SetRfControl: */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_FREQUENCY,
      g_param_spec_int64 ("frequency", "frequency", "Frequency in Hz",
          0, G_MAXINT64, DEFAULT_FREQUENCY, (GParamFlags) G_PARAM_READWRITE));

  /* SetOutputLevel */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_OUTPUT_POWER,
      g_param_spec_double ("output-power", "output-power",
          "Output level in dBm", -35.0, 0.0, 0.0,
          (GParamFlags) G_PARAM_READWRITE));

  /* SetModControl*/
  /* ParXtra0 */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_CODE_RATE,
    g_param_spec_enum ("code-rate",
        "code-rate",
        "Code Rate",
        GST_TYPE_DTAPISINK_CODE_RATE, DEFAULT_CODE_RATE,
        (GParamFlags) G_PARAM_READWRITE));

  /* ParXtra1 */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_BANDWIDTH,
      g_param_spec_enum ("bandwidth",
          "bandwidth",
          "Bandwidth (DVB-T)", GST_TYPE_DTAPISINK_BANDWIDTH, DEFAULT_BANDWIDTH,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DTAPISINK_MODULATION,
      g_param_spec_enum ("modulation", "modulation",
          "Modulation (DVB-T and DVB-C)",
          GST_TYPE_DTAPISINK_MODULATION, DEFAULT_MODULATION,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DTAPISINK_GUARD,
      g_param_spec_enum ("guard",
          "guard",
          "Guard Interval (DVB-T)",
          GST_TYPE_DTAPISINK_GUARD, DEFAULT_GUARD,
          (GParamFlags) G_PARAM_READWRITE));

  /* TODO: What is interleaving?  Does it have something to do with
           hierarchy? */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_INTERLEAVING,
    g_param_spec_enum ("interleaving",
        "interleaving",
        "Interleaving",
        GST_TYPE_DTAPISINK_INTERLEAVING, DEFAULT_INTERLEAVING,
        (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      PROP_DTAPISINK_TRANSMISSION_MODE,
      g_param_spec_enum ("trans-mode", "trans-mode",
          "Transmission Mode (DVB-T)", GST_TYPE_DTAPISINK_TRANSMISSION_MODE,
          DEFAULT_TRANSMISSION_MODE, (GParamFlags) G_PARAM_READWRITE));

  /* SetRfMode */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_INVERSION,
      g_param_spec_enum ("inversion", "inversion",
          "Inversion Information (DVB-T and DVB-C)",
          GST_TYPE_DTAPISINK_INVERSION, DEFAULT_INVERSION,
          (GParamFlags) G_PARAM_READWRITE));

  /* SetTxMode */
  g_object_class_install_property (gobject_class, PROP_DTAPISINK_TXMODE,
    g_param_spec_enum ("transmit-mode",
        "transmit-mode",
        "Transmit Mode (188, 192, 204, ADD16, MIN16 or RAW)",
        GST_TYPE_DTAPISINK_TXMODE, DEFAULT_TXMODE,
        (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DTAPISINK_STUFFING,
    g_param_spec_enum ("stuffing",
        "stuffing",
        "Determines how the modulator should behave when there is no packet "
        "data available (none or nulls",
        GST_TYPE_DTAPISINK_STUFFING, DEFAULT_STUFFING,
        (GParamFlags) G_PARAM_READWRITE));
}

static void
gst_dtapi_sink_init (GstDTAPISink * sink, GstDTAPISinkClass * gclass)
{
  /* TODO: Is this appropriate???: */
  gst_base_sink_set_sync (GST_BASE_SINK (sink), FALSE);
  /* NOTE: not sure what effect this has.  Setting it doesn't seem to make
     filesrc send us buffers > 4KB as you might expect: */
  gst_base_sink_set_blocksize (GST_BASE_SINK (sink), BUFSIZE / 2);
  /* FIXME: We don't want to preroll a certain number of buffers, we want to
     pre-roll until there is enough data in the FIFO such that we can enter
     state TXCTRL_SEND as soon as we enter state PLAYING */
  g_object_set(G_OBJECT(sink), "preroll-queue-len", 100, NULL);

  sink->ts_rate_bps = DEFAULT_BITRATE;
  sink->frequency = DEFAULT_FREQUENCY;
  sink->code_rate = DEFAULT_CODE_RATE;
  sink->mod_param =   DEFAULT_BANDWIDTH | DEFAULT_MODULATION
                    | DEFAULT_INTERLEAVING | DEFAULT_GUARD
                    | DEFAULT_TRANSMISSION_MODE;
  sink->rf_mode = DTAPI_UPCONV_NORMAL | DEFAULT_INVERSION;
  sink->tx_mode = DEFAULT_TXMODE;
  sink->stuff_mode = DEFAULT_STUFFING;
  sink->output_power = DEFAULT_OUTPUT_POWER;
}

static void
gst_dtapi_sink_update_prop_cache (GstDTAPISink * sink)
{
  /* TODO: Deal with errors */
  if (sink->TsOut) {
    int result, mod_type, par_xtra_2, lock_status;
    void* p_xtra_pars;
    CHECK(sink->TsOut->GetTsRateBps(sink->ts_rate_bps),
          "Failed to get TS rate: %s");
    CHECK(sink->TsOut->GetTxMode(sink->tx_mode, sink->stuff_mode),
          "Failed to get TxMode: %s");
    CHECK(sink->TsOut->GetRfControl(sink->frequency, lock_status),
          "Failed to get frequency: %s");
    CHECK(sink->TsOut->GetOutputLevel(sink->output_power),
          "Failed to get output power: %s");
    CHECK(sink->TsOut->GetModControl(mod_type, sink->code_rate,
                                     sink->mod_param, par_xtra_2, p_xtra_pars),
          "Failed to get modulation parameters: %s");
  }
}

static void assign_bits(int* out, int mask, int value)
{
  assert((~mask & value) == 0);
  assert(out);
  *out &= ~mask;
  *out |= value;
}

static void
gst_dtapi_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  int result;
  GstDTAPISink *sink;

  sink = GST_DTAPI_SINK (object);
  gst_dtapi_sink_update_prop_cache(sink);

  switch (prop_id) {
    case PROP_BITRATE:
      /* Only takes effect once start() is called */
      sink->ts_rate_bps = g_value_get_int(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetTsRateBps(sink->ts_rate_bps),
          "Failed to set TS rate: %s");
      }
      break;
    /* SetRfControl: */
    case PROP_DTAPISINK_FREQUENCY:
      sink->frequency = g_value_get_int64(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetRfControl(sink->frequency),
              "Failed to set frequency: %s");
      }
      break;
    /* SetOutputLevel */
    case PROP_DTAPISINK_OUTPUT_POWER:
      /* SetOutputLevel expects a value expressed in 0.1 dBm, we expect one in
         dBm so need to do a conversion here: */
      /* FIXME: do rounding */
      sink->output_power = g_value_get_double(value) * 10;
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetOutputLevel(sink->output_power),
              "Failed to set output power: %s");
      }
      break;
    /* SetModControl*/
    /* ParXtra0 */
    case PROP_DTAPISINK_CODE_RATE:
      sink->code_rate = g_value_get_enum(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set code rate: %s");
      }
      break;
    /* ParXtra1 */
    case PROP_DTAPISINK_BANDWIDTH:
      assign_bits(&sink->mod_param, DTAPI_MOD_DVBT_BW_MSK,
                  g_value_get_enum(value));
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set bandwidth: %s");
      }
      break;
    case PROP_DTAPISINK_MODULATION:
      assign_bits(&sink->mod_param, DTAPI_MOD_DVBT_CO_MSK,
                  g_value_get_enum(value));
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set modulation: %s");
      }
      break;
    case PROP_DTAPISINK_GUARD:
      assign_bits(&sink->mod_param, DTAPI_MOD_DVBT_GU_MSK,
                  g_value_get_enum(value));
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set guard: %s");
      }
      break;
    case PROP_DTAPISINK_INTERLEAVING:
      assign_bits(&sink->mod_param, DTAPI_MOD_DVBT_IL_MSK,
                  g_value_get_enum(value));
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set interleaving: %s");
      }
      break;
    case PROP_DTAPISINK_TRANSMISSION_MODE:
      assign_bits(&sink->mod_param, DTAPI_MOD_DVBT_MD_MSK,
                  g_value_get_enum(value));
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                         sink->mod_param, -1),
              "Failed to set transmission mode: %s");
      }
      break;
    /* SetRfMode */
    case PROP_DTAPISINK_INVERSION:
      sink->rf_mode = DTAPI_UPCONV_NORMAL | g_value_get_enum(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetRfMode(sink->rf_mode, sink->stuff_mode),
              "Failed to set inversion: %s");
      }
      break;
    /* SetTxMode */
    case PROP_DTAPISINK_TXMODE:
      sink->tx_mode = g_value_get_enum(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetTxMode(sink->tx_mode, sink->stuff_mode),
              "Failed to set txmode: %s");
      }
      break;
    case PROP_DTAPISINK_STUFFING:
      sink->stuff_mode = g_value_get_enum(value);
      if (sink->TsOut) {
        CHECK(sink->TsOut->SetTxMode(sink->tx_mode, sink->stuff_mode),
              "Failed to set stuffing: %s");
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dtapi_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDTAPISink *sink;

  sink = GST_DTAPI_SINK (object);

  gst_dtapi_sink_update_prop_cache (sink);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_int (value, sink->ts_rate_bps);
      break;
    /* SetRfControl: */
    case PROP_DTAPISINK_FREQUENCY:
      g_value_set_int64 (value, sink->frequency);
      break;
    /* SetOutputLevel */
    case PROP_DTAPISINK_OUTPUT_POWER:
      g_value_set_double (value, sink->output_power / 10.0);
      break;
    /* SetModControl*/
    /* ParXtra0 */
    case PROP_DTAPISINK_CODE_RATE:
      g_value_set_enum (value, sink->code_rate);
      break;
    /* ParXtra1 */
    case PROP_DTAPISINK_BANDWIDTH:
      g_value_set_enum (value, sink->mod_param & DTAPI_MOD_DVBT_BW_MSK);
      break;
    case PROP_DTAPISINK_MODULATION:
      g_value_set_enum (value, sink->mod_param & DTAPI_MOD_DVBT_CO_MSK);
      break;
    case PROP_DTAPISINK_GUARD:
      g_value_set_enum (value, sink->mod_param & DTAPI_MOD_DVBT_GU_MSK);
      break;
    case PROP_DTAPISINK_INTERLEAVING:
      g_value_set_enum (value, sink->mod_param & DTAPI_MOD_DVBT_IL_MSK);
      break;
    case PROP_DTAPISINK_TRANSMISSION_MODE:
      g_value_set_enum (value, sink->mod_param & DTAPI_MOD_DVBT_MD_MSK);
      break;
    /* SetRfMode */
    case PROP_DTAPISINK_INVERSION:
      g_value_set_enum(value, sink->rf_mode & DTAPI_UPCONV_SPECINV);
      break;
    /* SetTxMode */
    case PROP_DTAPISINK_TXMODE:
      g_value_set_enum(value, sink->tx_mode);
      break;
    case PROP_DTAPISINK_STUFFING:
      g_value_set_enum(value, sink->stuff_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#ifdef DTAPI_DEBUG
/* Just for debugging: Print all the properties according to DTAPI */
static void
gst_dtapi_sink_print_props (GstDTAPISink * sink)
{
  int result, ts_rate_bps, tx_mode, stuff_mode, lock_status,
      output_power, mod_type, code_rate, mod_param, par_xtra_2;
  int64_t frequency;
  void* p_xtra_pars;
  CHECK(sink->TsOut->GetTsRateBps(ts_rate_bps),
        "Failed to get TS rate: %s");
  CHECK(sink->TsOut->GetTxMode(tx_mode, stuff_mode),
        "Failed to get TxMode: %s");
  CHECK(sink->TsOut->GetRfControl(frequency, lock_status),
        "Failed to get frequency: %s");
  CHECK(sink->TsOut->GetOutputLevel(output_power),
        "Failed to get output power: %s");
  CHECK(sink->TsOut->GetModControl(mod_type, code_rate,
                                   mod_param, par_xtra_2, p_xtra_pars),
        "Failed to get modulation parameters: %s");
  printf("ts_rate_bps: %i\ntx_mode: %i\nstuff_mode: %i\nlock_status: %i\n"
         "output_power: %i\nmod_type: %i\ncode_rate: %i\nmod_param: %i\n"
         "par_xtra_2: %i\nfrequency: %lld\n", ts_rate_bps, tx_mode, stuff_mode,
         lock_status, output_power, mod_type, code_rate, mod_param, par_xtra_2,
         frequency);
}
#endif /* DTAPI_DEBUG */

static gboolean
gst_dtapi_sink_start (GstBaseSink * base_sink)
{
  DTAPI_RESULT result;
  GstDTAPISink *sink = GST_DTAPI_SINK (base_sink);

  sink->Dvc = new DtDevice();
  sink->TsOut = new DtOutpChannel();

  /* Attach device and output channel objects to hardware */
  if (sink->Dvc->AttachToType(215) != DTAPI_OK) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
      ("No DTU-215 in system"));
    return FALSE;
  }
  if (sink->TsOut->AttachToPort(sink->Dvc, 1) != DTAPI_OK) {
    /* TODO: Decide what the right type of error code to use here: */
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
      ("Can't attach output channel."));
    return FALSE;
  }

  /* Initialise bit rate and packet mode */
  CHECK(sink->TsOut->SetTxMode(sink->tx_mode, sink->stuff_mode),
        "Failed to set TxMode: %s");
  /* FIXME: Setting the TS rate has no effect, it seems to be purely detemined
     by the other parameters and I can't seem to work out how to apply
     stuffing to e.g. bulk out a 18Mb/s stream into a 24Mb/s one. */
  CHECK(sink->TsOut->SetTsRateBps(sink->ts_rate_bps),
        "Failed to set TS rate: %s");
  CHECK(sink->TsOut->SetRfMode(sink->rf_mode),
        "Failed to set RF mode");
  CHECK(sink->TsOut->SetRfControl(sink->frequency),
        "Failed to set frequency: %s");
  CHECK(sink->TsOut->SetOutputLevel(sink->output_power * 10),
        "Failed to set output power: %s");
  CHECK(sink->TsOut->SetModControl(DTAPI_MOD_DVBT, sink->code_rate,
                                   sink->mod_param, -1),
        "Failed to set modulation parameters: %s");

  CHECK(sink->TsOut->SetTxControl(DTAPI_TXCTRL_HOLD),
        "Entering state HOLD failed: %s");

  gst_dtapi_sink_update_prop_cache(sink);

  return TRUE;
}

#ifdef DTAPI_DEBUG
/* Just used for debugging in conjunction with DtOutpChannel::GetFlags */
static void
print_flags(int flags)
{
  if (flags & DTAPI_TX_FIFO_UFL)
    printf("DTAPI_TX_FIFO_UFL, ");
  if (flags & DTAPI_TX_MUX_OVF)
    printf("DTAPI_TX_MUX_OVF, ");
  if (flags & DTAPI_TX_READBACK_ERR)
    printf("DTAPI_TX_READBACK_ERR, ");
  if (flags & DTAPI_TX_SYNC_ERR)
    printf("DTAPI_TX_SYNC_ERR, ");
  if (flags & DTAPI_TX_TARGET_ERR)
    printf("DTAPI_TX_TARGET_ERR, ");
  if (flags & DTAPI_TX_LINK_ERR)
    printf("DTAPI_TX_LINK_ERR, ");
  if (flags & DTAPI_TX_DATA_ERR)
    printf("DTAPI_TX_DATA_ERR, ");
  
  /* Check that we've printed everything: */
  printf("%i\n", flags & ~(  DTAPI_TX_FIFO_UFL
                           | DTAPI_TX_MUX_OVF | DTAPI_TX_READBACK_ERR
                           | DTAPI_TX_SYNC_ERR | DTAPI_TX_TARGET_ERR
                           | DTAPI_TX_LINK_ERR | DTAPI_TX_DATA_ERR ));
}
#endif /* DTAPI_DEBUG */

static GstFlowReturn
gst_dtapi_sink_render (GstBaseSink *base_sink, GstBuffer *buffer)
{
  GstDTAPISink *sink = GST_DTAPI_SINK (base_sink);
  DTAPI_RESULT result;

  static size_t total_bytes_rendered = 0;
  total_bytes_rendered += GST_BUFFER_SIZE(buffer);

  if ((result = sink->TsOut->Write((char*) GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer))) != DTAPI_OK) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
      ("Writing data failed: %s", result_to_string(result)));
    return GST_FLOW_ERROR;
  }

#ifdef DTAPI_DEBUG
  int out;
  CHECK(sink->TsOut->GetTxControl(out), "GetTxControl failed: %s");
  switch (out) {
  case DTAPI_TXCTRL_IDLE:
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
      ("Error: in state IDLE"));
    return GST_FLOW_ERROR;
  case DTAPI_TXCTRL_HOLD:
    printf("Prebuffering... ");
    break;
  case DTAPI_TXCTRL_SEND:
    printf("Sending... ");
    break;
  };
  printf("Writing %iB, total %uB\n", GST_BUFFER_SIZE(buffer), total_bytes_rendered);
#endif /* DTAPI_DEBUG */
  /* Start transmission (if not already started) */
  result = sink->TsOut->SetTxControl(DTAPI_TXCTRL_SEND);
  /* If we haven't loaded enough it's not an error.
     TODO: work out how to load up in preroll */
  if (result != DTAPI_OK && result != DTAPI_E_INSUF_LOAD) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
      ("Enabling outputs failed: %s", result_to_string(result)));
    return GST_FLOW_ERROR;
  };

  int status, latched, fifosize, fifoload;
  CHECK(sink->TsOut->GetFlags(status, latched), "Getting flags failed: %s");
  CHECK(sink->TsOut->GetFifoLoad(fifoload), "Getting fifo load failed: %s");
  CHECK(sink->TsOut->GetFifoSize(fifosize), "Getting fifo size failed: %s");

#ifdef DTAPI_DEBUG
  printf("State: ");
  print_flags(status);
  printf("Latched: ");
  print_flags(status);
  printf("Fifo state: %d/%d\n", fifoload, fifosize);
  gst_dtapi_sink_update_prop_cache(sink);
  gst_dtapi_sink_print_props(sink);
#endif /* DTAPI_DEBUG */

  return GST_FLOW_OK;
}

static gboolean
gst_dtapi_sink_unlock (GstBaseSink *base_sink)
{
  GstDTAPISink *sink = GST_DTAPI_SINK (base_sink);
  return sink->TsOut->Reset(DTAPI_FIFO_RESET) == DTAPI_OK;
}

static gboolean
gst_dtapi_sink_stop_unlock (GstBaseSink *base_sink)
{
  GstDTAPISink *sink = GST_DTAPI_SINK (base_sink);
  return sink->TsOut->SetTxControl(DTAPI_TXCTRL_HOLD) == DTAPI_OK;
}

static gboolean
gst_dtapi_sink_stop (GstBaseSink *base_sink)
{
  GstDTAPISink *sink = GST_DTAPI_SINK (base_sink);

  sink->TsOut->Detach (DTAPI_INSTANT_DETACH);
  sink->Dvc->Detach ();

  delete sink->Dvc;
  sink->Dvc = NULL;
  delete sink->TsOut;
  sink->TsOut = NULL;

  return TRUE;
}

extern "C" {
gboolean
gst_dtapisink_plugin_init (GstPlugin * dtapisink)
{
  return gst_element_register (dtapisink, "dtapisink", GST_RANK_NONE,
      GST_TYPE_DTAPI_SINK);
}
}
