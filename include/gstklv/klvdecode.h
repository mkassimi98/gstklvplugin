/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/klvdecode.h
 * @brief GStreamer element that decodes KLV to JSON.
 * @ingroup gstklv_plugins
 *
 * @section klvdecode_overview Overview
 *
 * `klvmetadec` parses MISB ST 0601 Local Sets and emits a flat JSON object.
 * Tag ranges and types are taken from `stanag4609_tags.ini`.
 *
 * @section klvdecode_caps Pads and Caps
 *
 * | Pad | Direction | Caps |
 * | --- | --- | --- |
 * | `sink` | Sink | `meta/x-klv` |
 * | `src` | Src | `application/json` |
 *
 * @section klvdecode_output Output Rules
 *
 * - Numeric tags are scaled to physical units when a range is defined.
 * - String tags are emitted as JSON strings.
 * - Byte tags are emitted as `base64:` strings.
 *
 * @section klvdecode_flow Flow Diagram
 *
 * @code
 * meta/x-klv -> klvmetadec -> application/json
 * @endcode
 *
 * @dot
 * digraph klvdecode_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   klv [label="meta/x-klv"];
 *   dec [label="klvmetadec"];
 *   json [label="application/json"];
 *   klv -> dec -> json;
 * }
 * @enddot
 *
 * @section klvdecode_usage Usage
 *
 * @code
 * gst-launch-1.0 filesrc location=klv.bin ! klvmetadec ! fakesink
 * @endcode
 */
#ifndef __GST_KLV_DECODE_H__
#define __GST_KLV_DECODE_H__

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/**
 * @brief GObject type for the KLV decoder element.
 */
#define GST_TYPE_KLV_DECODE (gst_klv_decode_get_type())
G_DECLARE_FINAL_TYPE(GstKlvDecode, gst_klv_decode, GST, KLV_DECODE, GstBaseTransform)

/**
 * @brief KLV decoder element instance.
 *
 * Pads:
 * - sink: `meta/x-klv`
 * - src: `application/json`
 *
 * Behavior:
 * - Verifies MISB ST 0601 UL.
 * - Decodes BER lengths and emits flat JSON with numeric keys.
 */
struct _GstKlvDecode
{
  GstBaseTransform element; /**< Base transform parent. */
  /* Private data managed by G_DEFINE_TYPE */
};

G_END_DECLS

#endif /* __GST_KLV_DECODE_H__ */
