/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/klvframeinject.h
 * @brief GStreamer element that injects per-frame KLV metadata.
 * @ingroup gstklv_plugins
 *
 * @section klvframeinject_overview Overview
 *
 * `klvframeinject` is a dual-output element that forwards video buffers and
 * emits a matching KLV buffer per frame. It can generate tags from JSON or
 * from element properties (fallback tags 2, 5, 13, 14, 15).
 *
 * @section klvframeinject_caps Pads and Caps
 *
 * | Pad | Direction | Caps |
 * | --- | --- | --- |
 * | `sink` | Sink | `video/x-h264` or `video/x-h265` (byte-stream, AU) |
 * | `video_src` | Src | Same as sink |
 * | `klv_src` | Src | `meta/x-klv` |
 *
 * @section klvframeinject_properties Properties
 *
 * | Property | Type | Default | Description |
 * | --- | --- | --- | --- |
 * | `latitude` | double | 0.0 | Sensor latitude |
 * | `longitude` | double | 0.0 | Sensor longitude |
 * | `heading` | double | 0.0 | Platform heading |
 * | `altitude` | double | 1000.0 | Sensor altitude |
 * | `timestamp` | uint64 | 0 | Unix timestamp (seconds) |
 * | `use-system-time` | bool | true | Use system time |
 * | `tags-json` | string | "" | Flat JSON with numeric keys |
 *
 * @section klvframeinject_flow Flow Diagram
 *
 * @code
 * video/x-h264 or video/x-h265
 *         |
 *         v
 *   klvframeinject
 *     |        |
 *     v        v
 * video_src  klv_src (meta/x-klv)
 * @endcode
 *
 * @dot
 * digraph klvframeinject_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   video [label="video/x-h264 or video/x-h265"];
 *   inj [label="klvframeinject"];
 *   vout [label="video_src"];
 *   kout [label="klv_src (meta/x-klv)"];
 *   video -> inj;
 *   inj -> vout;
 *   inj -> kout;
 * }
 * @enddot
 *
 * @section klvframeinject_notes Notes
 *
 * - Tags are sorted by ID before encoding.
 * - Tag 1 (checksum) is appended last.
 * - Byte tags require `hex:` or `base64:` payloads in JSON.
 *
 * @section klvframeinject_usage Usage
 *
 * @code
 * gst-launch-1.0 videotestsrc ! x264enc ! klvframeinject name=inj \
 *   inj.video_src ! fakesink \
 *   inj.klv_src ! fakesink
 * @endcode
 */
#ifndef __GST_KLV_FRAME_INJECT_H__
#define __GST_KLV_FRAME_INJECT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/**
 * @brief GObject type for the KLV frame injector element.
 */
#define GST_TYPE_KLV_FRAME_INJECT (gst_klv_frame_inject_get_type())
G_DECLARE_FINAL_TYPE(GstKlvFrameInject, gst_klv_frame_inject, GST, KLV_FRAME_INJECT, GstElement)

/**
 * @brief KLV frame injector element instance.
 *
 * Pads:
 * - sink: `video/x-h264` or `video/x-h265`
 * - video_src: passthrough video
 * - klv_src: `meta/x-klv` (per-frame metadata)
 *
 * Properties:
 * - `latitude`, `longitude`, `heading`, `altitude`
 * - `timestamp`, `use-system-time`
 * - `tags-json` (full tag override)
 */
struct _GstKlvFrameInject
{
  GstElement element; /**< Element parent. */
  /* Private data managed by G_DEFINE_TYPE */
};

G_END_DECLS

#endif /* __GST_KLV_FRAME_INJECT_H__ */
