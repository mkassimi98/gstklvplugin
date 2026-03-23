/**
 * @file src/plugins/tspmtrewrite.c
 * @brief Implementation of the GstTsPmtRewrite element (PMT metadata rewrite).
 * @ingroup gstklv_plugins
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>

#include "gstklv/internal/ts_psi.h"

#include "gstklv/tspmtrewrite.h"

G_DEFINE_TYPE(GstTsPmtRewrite, gst_ts_pmt_rewrite, GST_TYPE_BASE_TRANSFORM)

static const guint TS_PACKET_SIZE = 188;

enum
{
  PROP_0,
  PROP_METADATA_APP_FORMAT,
  PROP_METADATA_APP_IDENTIFIER,
  PROP_METADATA_FORMAT,
  PROP_METADATA_FORMAT_IDENTIFIER,
  PROP_METADATA_SERVICE_ID,
  PROP_METADATA_FLAGS,
};

/* Forward declarations */
static void gst_ts_pmt_rewrite_set_property(GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void
gst_ts_pmt_rewrite_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

/**
 * @brief Check whether descriptors include a KLVA registration descriptor.
 * @param descriptors GPtrArray of TsDescriptor*.
 * @return TRUE if KLVA registration is found.
 */
static gboolean
has_registration_klva(GPtrArray *descriptors)
{
  if (!descriptors)
    return FALSE;
  for (guint i = 0; i < descriptors->len; i++) {
    TsDescriptor *d = (TsDescriptor *)g_ptr_array_index(descriptors, i);
    if (d->tag == 0x05 && d->payload && d->payload->len >= 4) {
      if (d->payload->data[0] == 'K' && d->payload->data[1] == 'L' && d->payload->data[2] == 'V' &&
          d->payload->data[3] == 'A') {
        return TRUE;
      }
    }
  }
  return FALSE;
}

/**
 * @brief Check whether descriptors include a metadata descriptor (0x26).
 * @param descriptors GPtrArray of TsDescriptor*.
 * @return TRUE if a metadata descriptor is present.
 */
static gboolean
has_metadata_descriptor(GPtrArray *descriptors)
{
  if (!descriptors)
    return FALSE;
  for (guint i = 0; i < descriptors->len; i++) {
    TsDescriptor *d = (TsDescriptor *)g_ptr_array_index(descriptors, i);
    if (d->tag == 0x26)
      return TRUE;
  }
  return FALSE;
}

/**
 * @brief Coerce a 4-character identifier into a fixed buffer.
 * @param input Optional user input string.
 * @param fallback Fallback identifier when input is invalid.
 * @param out Output buffer (5 bytes including NUL).
 */
static void
coerce_identifier(const gchar *input, const gchar *fallback, gchar out[5])
{
  if (input && strlen(input) == 4) {
    memcpy(out, input, 4);
    out[4] = '\0';
  }
  else {
    memcpy(out, fallback, 4);
    out[4] = '\0';
  }
}

/**
 * @brief Build a metadata descriptor (tag 0x26) from element properties.
 * @param self Element instance.
 * @return Newly allocated TsDescriptor* (caller owns, free with ts_descriptor_free).
 */
static TsDescriptor *
build_metadata_descriptor(GstTsPmtRewrite *self)
{
  TsDescriptor *d = g_new0(TsDescriptor, 1);
  d->tag = 0x26;
  d->payload = g_byte_array_new();

  guint16 app_format = self->metadata_app_format;
  guint8 metadata_format = self->metadata_format;
  guint8 service_id = self->metadata_service_id;
  guint8 flags = self->metadata_flags;
  guint8 b;

  b = (guint8)((app_format >> 8) & 0xFF);
  g_byte_array_append(d->payload, &b, 1);
  b = (guint8)(app_format & 0xFF);
  g_byte_array_append(d->payload, &b, 1);

  if (app_format == 0xFFFF) {
    g_byte_array_append(d->payload, (const guint8 *)self->metadata_app_identifier, 4);
  }

  g_byte_array_append(d->payload, &metadata_format, 1);
  if (metadata_format == 0xFF) {
    g_byte_array_append(d->payload, (const guint8 *)self->metadata_format_identifier, 4);
  }

  g_byte_array_append(d->payload, &service_id, 1);
  g_byte_array_append(d->payload, &flags, 1);

  return d;
}

/**
 * @brief Rewrite PMT entries to signal KLV metadata as `0x06 + KLVA` with
 * `metadata_descriptor (0x26)` in the current implementation.
 * @param self Element instance.
 * @param pmt Parsed PMT info (will be modified in place).
 * @return Rebuilt PMT section as GByteArray* (caller must g_byte_array_unref).
 */
static GByteArray *
rewrite_pmt_section(GstTsPmtRewrite *self, TsPmtInfo *pmt)
{
  /* Increment PMT version number (bits 5:1 of version_byte, modulo 32).
   * Per MISB ST 1402 / ISO 13818-1, version must change when section content changes. */
  guint8 old_ver = (pmt->version_byte >> 1) & 0x1F;
  pmt->version_byte = (pmt->version_byte & 0xC1) | (((old_ver + 1) & 0x1F) << 1);

  TsDescriptor *metadata_desc = build_metadata_descriptor(self);

  guint num_streams = pmt->streams ? pmt->streams->len : 0;
  for (guint si = 0; si < num_streams; si++) {
    TsStreamInfo *s = (TsStreamInfo *)g_ptr_array_index(pmt->streams, si);
    if (!has_registration_klva(s->descriptors))
      continue;

    /* Use stream_type 0x06 (private PES data) instead of 0x15 (metadata in PES).
     * GStreamer mpegtsmux does not wrap KLV in MPEG-TS Metadata Access Unit
     * cell format, so tsdemux rejects 0x15 streams with
     * "Failed to parse PES metadata access units". With 0x06 + KLVA
     * registration_descriptor, tsdemux routes raw KLV bytes to the
     * meta/x-klv pad without metadata AU cell parsing. */
    s->stream_type = 0x06;
    if (!has_metadata_descriptor(s->descriptors)) {
      gboolean inserted = FALSE;
      guint nd = s->descriptors ? s->descriptors->len : 0;
      for (guint di = 0; di < nd; di++) {
        TsDescriptor *d = (TsDescriptor *)g_ptr_array_index(s->descriptors, di);
        if (d->tag == 0x05 && d->payload && d->payload->len >= 4 && d->payload->data[0] == 'K' &&
            d->payload->data[1] == 'L' && d->payload->data[2] == 'V' &&
            d->payload->data[3] == 'A') {
          /* Insert metadata_desc right after this registration descriptor.
           * g_ptr_array_insert requires GLib >= 2.40. */
          g_ptr_array_add(s->descriptors, NULL); /* grow by 1 */
          /* Shift elements from di+1 onward to the right */
          for (guint k = s->descriptors->len - 1; k > di + 1; k--) {
            s->descriptors->pdata[k] = s->descriptors->pdata[k - 1];
          }
          s->descriptors->pdata[di + 1] = metadata_desc;
          metadata_desc = NULL; /* ownership transferred */
          inserted = TRUE;
          break;
        }
      }
      if (!inserted) {
        g_ptr_array_add(s->descriptors, metadata_desc);
        metadata_desc = NULL; /* ownership transferred */
      }
    }
  }

  if (metadata_desc)
    ts_descriptor_free(metadata_desc);

  return ts_build_pmt_section(pmt);
}

static void
rewrite_pmt_packet(guint8 *pkt, const guint8 *section, gsize section_len, guint8 cc)
{
  pkt[1] |= 0x40;
  /* AFC = 0x01 (payload only, no adaptation field), CC from our tracked counter */
  pkt[3] = (pkt[3] & 0xC0) | 0x10 | (cc & 0x0F);

  guint payload_len = TS_PACKET_SIZE - 4;
  pkt[4] = 0x00;
  guint max_section = payload_len - 1;
  guint to_copy = section_len < max_section ? (guint)section_len : max_section;
  memcpy(pkt + 5, section, to_copy);
  if (to_copy < max_section) {
    memset(pkt + 5 + to_copy, 0xFF, max_section - to_copy);
  }
}

/**
 * @brief In-place PMT rewrite that updates KLV signaling.
 * @param trans Base transform instance.
 * @param buf MPEG-TS buffer to inspect and rewrite.
 * @return GstFlowReturn status.
 */
static GstFlowReturn
gst_ts_pmt_rewrite_transform_ip(GstBaseTransform *trans, GstBuffer *buf)
{
  GstTsPmtRewrite *self = GST_TS_PMT_REWRITE(trans);
  GstMapInfo map;

  if (!gst_buffer_map(buf, &map, GST_MAP_READWRITE))
    return GST_FLOW_ERROR;

  gsize size = map.size - (map.size % TS_PACKET_SIZE);
  for (gsize i = 0; i + TS_PACKET_SIZE <= size; i += TS_PACKET_SIZE) {
    guint8 *pkt = map.data + i;
    if (pkt[0] != 0x47)
      continue;

    guint16 pid = ((pkt[1] & 0x1F) << 8) | pkt[2];

    if (!self->pmt_pid_known && pid == 0) {
      const guint8 *section = NULL;
      gsize section_len = 0;
      if (ts_extract_section_from_packet(pkt, TS_PACKET_SIZE, 0x00, &section, &section_len)) {
        guint16 pmt_pid = 0;
        if (ts_parse_pat_section(section, section_len, &pmt_pid)) {
          self->pmt_pid = pmt_pid;
          self->pmt_pid_known = TRUE;
          GST_INFO_OBJECT(self, "Discovered PMT PID 0x%04X", self->pmt_pid);
        }
      }
    }

    if (self->pmt_pid_known && pid == self->pmt_pid && self->new_pmt_section == NULL) {
      const guint8 *section = NULL;
      gsize section_len = 0;
      if (ts_extract_section_from_packet(pkt, TS_PACKET_SIZE, 0x02, &section, &section_len)) {
        TsPmtInfo pmt;
        ts_pmt_info_init(&pmt);
        if (ts_parse_pmt_section(section, section_len, &pmt)) {
          GByteArray *new_section = rewrite_pmt_section(self, &pmt);
          if (new_section && new_section->len + 1 <= TS_PACKET_SIZE - 4) {
            self->new_pmt_section = new_section;
            /* Seed our CC from the original packet so the counter stays in sync */
            self->pmt_cc = pkt[3] & 0x0F;
            GST_INFO_OBJECT(self,
                            "PMT rewrite ready (section size %u, seed CC %u)",
                            self->new_pmt_section->len,
                            self->pmt_cc);
          }
          else {
            if (new_section)
              g_byte_array_unref(new_section);
            GST_WARNING_OBJECT(self, "PMT section too large to fit in single packet");
          }
        }
        ts_pmt_info_clear(&pmt);
      }
    }

    if (self->pmt_pid_known && pid == self->pmt_pid && self->new_pmt_section != NULL) {
      rewrite_pmt_packet(
        pkt, self->new_pmt_section->data, self->new_pmt_section->len, self->pmt_cc);
      self->pmt_cc = (self->pmt_cc + 1) & 0x0F;
    }
  }

  gst_buffer_unmap(buf, &map);
  return GST_FLOW_OK;
}

static gboolean
gst_ts_pmt_rewrite_start(GstBaseTransform *trans)
{
  GstTsPmtRewrite *self = GST_TS_PMT_REWRITE(trans);
  self->pmt_pid_known = FALSE;
  self->pmt_pid = 0;
  self->pmt_cc = 0;
  if (self->new_pmt_section) {
    g_byte_array_unref(self->new_pmt_section);
    self->new_pmt_section = NULL;
  }
  return TRUE;
}

static gboolean
gst_ts_pmt_rewrite_stop(GstBaseTransform *trans)
{
  GstTsPmtRewrite *self = GST_TS_PMT_REWRITE(trans);
  if (self->new_pmt_section) {
    g_byte_array_unref(self->new_pmt_section);
    self->new_pmt_section = NULL;
  }
  return TRUE;
}

static void
gst_ts_pmt_rewrite_set_property(GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  GstTsPmtRewrite *self = GST_TS_PMT_REWRITE(object);
  switch (prop_id) {
    case PROP_METADATA_APP_FORMAT:
      self->metadata_app_format = (guint16)g_value_get_uint(value);
      break;
    case PROP_METADATA_APP_IDENTIFIER:
      coerce_identifier(g_value_get_string(value), "MISB", self->metadata_app_identifier);
      break;
    case PROP_METADATA_FORMAT: self->metadata_format = (guint8)g_value_get_uint(value); break;
    case PROP_METADATA_FORMAT_IDENTIFIER:
      coerce_identifier(g_value_get_string(value), "KLVA", self->metadata_format_identifier);
      break;
    case PROP_METADATA_SERVICE_ID:
      self->metadata_service_id = (guint8)g_value_get_uint(value);
      break;
    case PROP_METADATA_FLAGS: self->metadata_flags = (guint8)g_value_get_uint(value); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
  if (self->new_pmt_section) {
    g_byte_array_unref(self->new_pmt_section);
    self->new_pmt_section = NULL;
  }
}

static void
gst_ts_pmt_rewrite_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstTsPmtRewrite *self = GST_TS_PMT_REWRITE(object);
  switch (prop_id) {
    case PROP_METADATA_APP_FORMAT: g_value_set_uint(value, self->metadata_app_format); break;
    case PROP_METADATA_APP_IDENTIFIER:
      g_value_set_string(value, self->metadata_app_identifier);
      break;
    case PROP_METADATA_FORMAT: g_value_set_uint(value, self->metadata_format); break;
    case PROP_METADATA_FORMAT_IDENTIFIER:
      g_value_set_string(value, self->metadata_format_identifier);
      break;
    case PROP_METADATA_SERVICE_ID: g_value_set_uint(value, self->metadata_service_id); break;
    case PROP_METADATA_FLAGS: g_value_set_uint(value, self->metadata_flags); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void
gst_ts_pmt_rewrite_class_init(GstTsPmtRewriteClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gst_element_class_set_static_metadata(element_class,
                                        "TS PMT Rewriter",
                                        "Filter/Metadata",
                                        "Rewrite PMT entries for KLV metadata streams",
                                        "Mouhsine Kassimi Farhaoui <mouhsine98@gmail.com>");

  GstCaps *caps =
    gst_caps_from_string("video/mpegts, systemstream=(boolean)true, packetsize=(int)188");
  GstPadTemplate *sink_templ = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
  GstPadTemplate *src_templ = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template(element_class, sink_templ);
  gst_element_class_add_pad_template(element_class, src_templ);
  gst_caps_unref(caps);

  trans_class->transform_ip = GST_DEBUG_FUNCPTR(gst_ts_pmt_rewrite_transform_ip);
  trans_class->start = GST_DEBUG_FUNCPTR(gst_ts_pmt_rewrite_start);
  trans_class->stop = GST_DEBUG_FUNCPTR(gst_ts_pmt_rewrite_stop);

  gobject_class->set_property = gst_ts_pmt_rewrite_set_property;
  gobject_class->get_property = gst_ts_pmt_rewrite_get_property;

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_APP_FORMAT,
    g_param_spec_uint("metadata-app-format",
                      "Metadata Application Format",
                      "metadata_application_format (0xFFFF uses identifier)",
                      0,
                      0xFFFF,
                      0xFFFF,
                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_APP_IDENTIFIER,
    g_param_spec_string("metadata-app-identifier",
                        "Metadata Application Identifier",
                        "4-character metadata_application_format_identifier",
                        "MISB",
                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_FORMAT,
    g_param_spec_uint("metadata-format",
                      "Metadata Format",
                      "metadata_format (0xFF uses identifier)",
                      0,
                      0xFF,
                      0xFF,
                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_FORMAT_IDENTIFIER,
    g_param_spec_string("metadata-format-identifier",
                        "Metadata Format Identifier",
                        "4-character metadata_format_identifier",
                        "KLVA",
                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_SERVICE_ID,
    g_param_spec_uint("metadata-service-id",
                      "Metadata Service ID",
                      "metadata_service_id",
                      0,
                      0xFF,
                      0x01,
                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
    gobject_class,
    PROP_METADATA_FLAGS,
    g_param_spec_uint("metadata-flags",
                      "Metadata Flags",
                      "decoder_config_flags",
                      0,
                      0xFF,
                      0x00,
                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_ts_pmt_rewrite_init(GstTsPmtRewrite *self)
{
  self->pmt_pid_known = FALSE;
  self->pmt_pid = 0;
  self->new_pmt_section = NULL;
  self->pmt_cc = 0;
  self->metadata_app_format = 0xFFFF;
  self->metadata_format = 0xFF;
  self->metadata_service_id = 0x01;
  self->metadata_flags = 0x00;
  coerce_identifier("MISB", "MISB", self->metadata_app_identifier);
  coerce_identifier("KLVA", "KLVA", self->metadata_format_identifier);
  gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
  gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
}
