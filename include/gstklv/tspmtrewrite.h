/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/tspmtrewrite.h
 * @brief GStreamer element that rewrites PMT metadata signaling.
 * @ingroup gstklv_plugins
 *
 * @section tspmtrewrite_overview Overview
 *
 * `tspmtrewrite` updates PMT entries to signal KLV metadata in the form used
 * by this repo today: `stream_type 0x06`, `registration_descriptor(KLVA)`,
 * and `metadata_descriptor (0x26)`.
 *
 * @section tspmtrewrite_caps Pads and Caps
 *
 * | Pad | Direction | Caps |
 * | --- | --- | --- |
 * | `sink` | Sink | `video/mpegts, systemstream=true, packetsize=188` |
 * | `src` | Src | `video/mpegts, systemstream=true, packetsize=188` |
 *
 * @section tspmtrewrite_properties Properties
 *
 * | Property | Default | Purpose |
 * | --- | --- | --- |
 * | `metadata-app-format` | `0xFFFF` | Metadata application format |
 * | `metadata-app-identifier` | `MISB` | 4-char app identifier |
 * | `metadata-format` | `0xFF` | Metadata format |
 * | `metadata-format-identifier` | `KLVA` | 4-char format identifier |
 * | `metadata-service-id` | `0x01` | Service ID |
 * | `metadata-flags` | `0x00` | Decoder config flags |
 *
 * @section tspmtrewrite_flow Flow Diagram
 *
 * @code
 * MPEG-TS -> tspmtrewrite -> MPEG-TS (metadata signaling updated)
 * @endcode
 *
 * @dot
 * digraph tspmtrewrite_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   ts_in [label="MPEG-TS"];
 *   rewrite [label="tspmtrewrite"];
 *   ts_out [label="MPEG-TS (stream_type 0x06 + KLVA + metadata_descriptor)"];
 *   ts_in -> rewrite -> ts_out;
 * }
 * @enddot
 *
 * @section tspmtrewrite_notes Notes
 *
 * - Only PMT sections that fit in a single TS packet are rewritten.
 * - Streams are updated only when a KLVA registration descriptor is present.
 *
 * @section tspmtrewrite_usage Usage
 *
 * @code
 * gst-launch-1.0 filesrc location=in.ts ! tspmtrewrite ! fakesink
 * @endcode
 */
#ifndef __GST_TS_PMT_REWRITE_H__
#define __GST_TS_PMT_REWRITE_H__

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/**
 * @brief GObject type for the TS PMT rewriter element.
 */
#define GST_TYPE_TS_PMT_REWRITE (gst_ts_pmt_rewrite_get_type())
G_DECLARE_FINAL_TYPE(GstTsPmtRewrite, gst_ts_pmt_rewrite, GST, TS_PMT_REWRITE, GstBaseTransform)

/**
 * @brief TS PMT rewrite element instance.
 *
 * Pads:
 * - sink/src: `video/mpegts` (systemstream, packetsize 188)
 *
 * The element rewrites PMT entries for KLVA streams to use:
 * - `stream_type 0x06`
 * - `registration_descriptor (0x05) = KLVA`
 * - `metadata_descriptor (0x26)`
 */
struct _GstTsPmtRewrite
{
  GstBaseTransform element;            /**< Base transform parent. */
  gboolean pmt_pid_known;              /**< TRUE once PAT has been parsed. */
  guint16 pmt_pid;                     /**< PMT PID discovered from PAT. */
  GByteArray *new_pmt_section;         /**< Cached rewritten PMT section. */
  guint8 pmt_cc;                       /**< Continuity counter for rewritten PMT packets (0-15). */
  guint16 metadata_app_format;         /**< metadata_application_format. */
  gchar metadata_app_identifier[5];    /**< 4-char app identifier (e.g., MISB). */
  guint8 metadata_format;              /**< metadata_format. */
  gchar metadata_format_identifier[5]; /**< 4-char format identifier (e.g., KLVA). */
  guint8 metadata_service_id;          /**< metadata_service_id. */
  guint8 metadata_flags;               /**< decoder_config_flags. */
};

G_END_DECLS

#endif /* __GST_TS_PMT_REWRITE_H__ */
