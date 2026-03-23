/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/klvencode.h
 * @brief GStreamer element that encodes JSON to KLV.
 * @ingroup gstklv_plugins
 *
 * @section klvencode_overview Overview
 *
 * `klvmetaenc` converts a flat JSON object into a MISB ST 0601 Local Set
 * wrapped with the 16-byte UL and BER length per SMPTE ST 336.
 *
 * @section klvencode_caps Pads and Caps
 *
 * | Pad | Direction | Caps |
 * | --- | --- | --- |
 * | `sink` | Sink | `application/json` |
 * | `src` | Src | `meta/x-klv` |
 *
 * @section klvencode_json JSON Input Format
 *
 * Keys are numeric strings and values are numbers or strings. Byte payloads
 * can be passed with `hex:` or `base64:` prefixes.
 *
 * @code{.json}
 * { "2": 1700000000, "13": 40.12, "48": "base64:AQID" }
 * @endcode
 *
 * @section klvencode_behavior Behavior
 *
 * - Numeric scaling is implemented for tags 2, 5, 13, 14, and 15.
 * - Other numeric tags must be supplied as raw bytes or strings.
 * - The encoder emits one-byte tag IDs only.
 *
 * @section klvencode_flow Flow Diagram
 *
 * @code
 * application/json -> klvmetaenc -> meta/x-klv
 * @endcode
 *
 * @dot
 * digraph klvencode_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   json [label="application/json"];
 *   enc [label="klvmetaenc"];
 *   klv [label="meta/x-klv"];
 *   json -> enc -> klv;
 * }
 * @enddot
 *
 * @section klvencode_usage Usage
 *
 * @code
 * gst-launch-1.0 appsrc caps=application/json ! klvmetaenc ! fakesink
 * @endcode
 */
#ifndef __GST_KLV_ENCODE_H__
#define __GST_KLV_ENCODE_H__

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/**
 * @brief GObject type for the KLV encoder element.
 */
#define GST_TYPE_KLV_ENCODE (gst_klv_encode_get_type())
G_DECLARE_FINAL_TYPE(GstKlvEncode, gst_klv_encode, GST, KLV_ENCODE, GstBaseTransform)

/**
 * @brief KLV encoder element instance.
 *
 * Pads:
 * - sink: `application/json`
 * - src: `meta/x-klv`
 *
 * Behavior:
 * - Parses flat JSON with numeric keys.
 * - Encodes tags into ST 0601 local set with BER lengths.
 */
struct _GstKlvEncode
{
  GstBaseTransform element; /**< Base transform parent. */
  /* Private data managed by G_DEFINE_TYPE */
};

G_END_DECLS

#endif /* __GST_KLV_ENCODE_H__ */
