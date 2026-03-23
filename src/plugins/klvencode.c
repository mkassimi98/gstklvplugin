/**
 * @file src/plugins/klvencode.c
 * @brief Implementation of the GstKlvEncode element (JSON to KLV).
 * @ingroup gstklv_plugins
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "gstklv/internal/klv_ber.h"
#include "gstklv/internal/klv_checksum.h"
#include "gstklv/internal/klv_json.h"
#include "gstklv/internal/klv_scaling.h"
#include "gstklv/internal/klv_tag_defs.h"
#include "gstklv/internal/klv_ul.h"

#include "gstklv/klvencode.h"

G_DEFINE_TYPE(GstKlvEncode, gst_klv_encode, GST_TYPE_BASE_TRANSFORM)

static GHashTable *g_tag_defs = NULL;
static GMutex g_tag_defs_lock;

static GHashTable *
get_tag_defs(void)
{
  g_mutex_lock(&g_tag_defs_lock);
  if (!g_tag_defs || g_hash_table_size(g_tag_defs) == 0) {
    if (g_tag_defs) {
      g_hash_table_destroy(g_tag_defs);
      g_tag_defs = NULL;
    }
    const gchar *ini_path = g_getenv("KLV_TAGS_INI");
    if (ini_path && g_file_test(ini_path, G_FILE_TEST_EXISTS)) {
      g_tag_defs = klv_load_tag_defs_from_ini(ini_path);
#ifdef KLV_TAGS_DEFAULT_PATH
    }
    else if (g_file_test(KLV_TAGS_DEFAULT_PATH, G_FILE_TEST_EXISTS)) {
      g_tag_defs = klv_load_tag_defs_from_ini(KLV_TAGS_DEFAULT_PATH);
#endif
    }
    else if (g_file_test("data/stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
      g_tag_defs = klv_load_tag_defs_from_ini("data/stanag4609_tags.ini");
    }
    else if (g_file_test("./stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
      g_tag_defs = klv_load_tag_defs_from_ini("./stanag4609_tags.ini");
    }
    else {
      g_tag_defs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, klv_tag_def_free);
    }
  }
  g_mutex_unlock(&g_tag_defs_lock);
  return g_tag_defs;
}

/* Write @nbytes of @val in big-endian order into @p. */
static void
write_be(guint8 *p, guint64 val, gint nbytes)
{
  for (gint b = nbytes - 1; b >= 0; b--) {
    p[b] = (guint8)(val & 0xFF);
    val >>= 8;
  }
}

/* Inverse of klv_decode_with_range: physical value -> raw integer. */
static guint64
klv_encode_with_range(gdouble value, gdouble min, gdouble max, gboolean is_signed, gint bits)
{
  if (max <= min || bits <= 0)
    return 0;

  gdouble norm = (value - min) / (max - min);
  if (norm < 0.0)
    norm = 0.0;
  if (norm > 1.0)
    norm = 1.0;

  if (is_signed) {
    gint64 smax = (bits >= 64) ? G_MAXINT64 : (((gint64)1 << (bits - 1)) - 1);
    gint64 sval = (gint64)(norm * 2.0 * (gdouble)smax - (gdouble)smax + 0.5);
    if (sval < -smax)
      sval = -smax;
    if (sval > smax)
      sval = smax;
    return (guint64)(gint64)sval; /* two's complement */
  }

  guint64 umax = (bits >= 64) ? G_MAXUINT64 : (((guint64)1 << bits) - 1);
  guint64 raw = (guint64)(norm * (gdouble)umax + 0.5);
  if (raw > umax)
    raw = umax;
  return raw;
}

/**
 * @brief Return fixed payload length for hardcoded ST 0601 tags (size estimation).
 * @param tag_id Numeric tag identifier.
 * @return Length in bytes or 0 for tags handled via INI.
 */
static gsize
klv_value_len_for_tag(gint tag_id)
{
  switch (tag_id) {
    case 2: return 8;
    case 5: return 2;
    case 13: return 4;
    case 14: return 4;
    case 15: return 2;
    default: return 0;
  }
}

/**
 * @brief Encode a JSON tag into a KLV TLV entry.
 *
 * Tag 1 (Checksum) is silently skipped — the encoder appends it automatically
 * after all other TLVs. All numeric tags are scaled using INI definitions when
 * not explicitly hardcoded.
 *
 * @param buffer Output buffer to write the TLV.
 * @param tag Parsed JSON tag to encode.
 * @param tag_defs INI tag registry (may be NULL).
 * @return Number of bytes written into @p buffer, or 0 to skip this tag.
 */
static gsize
klv_encode_tlv(guint8 *buffer, const KLVJsonTag *tag, GHashTable *tag_defs)
{
  /* Tag 1 (Checksum) is computed and appended by the caller — skip here */
  if (tag->tag_id == 1)
    return 0;

  guint8 *p = buffer;

  *p++ = (guint8)tag->tag_id;

  guint8 *length_pos = p;
  p += 4; /* Reserve space for length encoding (worst case 4 bytes) */

  guint8 *value_start = p;

  if (tag->is_raw) {
    memcpy(p, tag->raw, tag->raw_len);
    p += tag->raw_len;
  }
  else {
    switch (tag->tag_id) {
      case 2: {
        /* Unix Time Stamp: uint64 big-endian, 8 bytes (not range-scaled) */
        guint64 ts = (guint64)tag->value;
        write_be(p, ts, 8);
        p += 8;
        break;
      }
      case 5: {
        /* Platform Heading Angle: 0..360 deg -> uint16 */
        guint64 raw = klv_encode_with_range(tag->value, 0.0, 360.0, FALSE, 16);
        write_be(p, raw, 2);
        p += 2;
        break;
      }
      case 13: {
        /* Sensor Latitude: +-90 deg -> int32 */
        guint64 raw = klv_encode_with_range(tag->value, -90.0, 90.0, TRUE, 32);
        write_be(p, raw, 4);
        p += 4;
        break;
      }
      case 14: {
        /* Sensor Longitude: +-180 deg -> int32 */
        guint64 raw = klv_encode_with_range(tag->value, -180.0, 180.0, TRUE, 32);
        write_be(p, raw, 4);
        p += 4;
        break;
      }
      case 15: {
        /* Sensor True Altitude: -900..19000 m -> uint16 */
        guint64 raw = klv_encode_with_range(tag->value, -900.0, 19000.0, FALSE, 16);
        write_be(p, raw, 2);
        p += 2;
        break;
      }
      default: {
        /* Fall back to INI tag definition for all other numeric tags */
        KLVTagDef *td = tag_defs
                          ? (KLVTagDef *)g_hash_table_lookup(tag_defs, GINT_TO_POINTER(tag->tag_id))
                          : NULL;
        if (!td || td->len <= 0 || td->len > 8) {
          GST_WARNING("Tag %d not in registry or unsupported length, skipping", tag->tag_id);
          return 0;
        }
        guint64 raw;
        if (td->has_range) {
          raw = klv_encode_with_range(
            tag->value, td->range_min, td->range_max, td->is_signed, td->len * 8);
        }
        else {
          raw = (guint64)(gint64)tag->value;
        }
        write_be(p, raw, td->len);
        p += td->len;
        break;
      }
    }
  }

  /* Encode length in-place */
  gsize value_len = p - value_start;

  /* BER with 4 reserved bytes supports up to 16,777,215 bytes (0x83 + 3 bytes).
   * Values larger than that would require 5 bytes and overflow the reserved space. */
  if (value_len > 0xFFFFFF) {
    GST_ERROR("TLV value too large (%zu bytes, max 16777215)", value_len);
    return 0;
  }

  gint len_encoded = klv_ber_encode_length(length_pos, value_len);

  /* Shift value bytes left to fill unused reserved space */
  gsize unused = 4 - (gsize)len_encoded;
  if (unused > 0) {
    memmove(length_pos + len_encoded, value_start, value_len);
    p = length_pos + len_encoded + value_len;
  }

  return p - buffer;
}

/**
 * @brief Transform JSON input into a KLV buffer.
 * @param transform Base transform instance.
 * @param inbuf Input JSON buffer.
 * @param outbuf Output KLV buffer.
 * @return GstFlowReturn status.
 */
static GstFlowReturn
gst_klv_encode_transform(GstBaseTransform *transform, GstBuffer *inbuf, GstBuffer *outbuf)
{
  /* Map input buffer */
  GstMapInfo inmap;
  if (!gst_buffer_map(inbuf, &inmap, GST_MAP_READ)) {
    return GST_FLOW_ERROR;
  }

  /* Parse JSON */
  KLVJsonTag tags[64];
  gint tag_count = klv_json_parse_flat((const gchar *)inmap.data, inmap.size, tags, 64);

  gst_buffer_unmap(inbuf, &inmap);

  if (tag_count < 0) {
    GST_ERROR_OBJECT(transform, "JSON parse error");
    return GST_FLOW_ERROR;
  }

#define FREE_PARSED_TAGS()                                \
  G_STMT_START                                            \
  {                                                       \
    for (gint free_i = 0; free_i < tag_count; free_i++) { \
      if (tags[free_i].is_raw && tags[free_i].raw)        \
        g_free(tags[free_i].raw);                         \
      if (tags[free_i].number_text)                       \
        g_free(tags[free_i].number_text);                 \
    }                                                     \
  }                                                       \
  G_STMT_END

  GHashTable *tag_defs = get_tag_defs();

  /* MISB ST 0601 requires Tag 2 (Unix Timestamp) in every packet */
  gboolean has_timestamp = FALSE;
  for (gint i = 0; i < tag_count; i++) {
    if (tags[i].tag_id == 2) {
      has_timestamp = TRUE;
      break;
    }
  }
  if (!has_timestamp) {
    GST_WARNING_OBJECT(transform,
                       "Tag 2 (Unix Timestamp) missing — "
                       "MISB ST 0601 requires a timestamp in every KLV packet");
  }

  /* Calculate KLV output buffer size */
  gsize klv_size = sizeof(KLV_MISB_ST0601_UL) + 4; /* UL + worst-case BER length */
  for (gint i = 0; i < tag_count; i++) {
    if (tags[i].tag_id == 1)
      continue; /* Checksum is appended automatically */
    gsize vlen = 0;
    if (tags[i].is_raw) {
      vlen = tags[i].raw_len;
    }
    else {
      vlen = klv_value_len_for_tag(tags[i].tag_id);
      if (vlen == 0) {
        KLVTagDef *td =
          tag_defs ? (KLVTagDef *)g_hash_table_lookup(tag_defs, GINT_TO_POINTER(tags[i].tag_id))
                   : NULL;
        if (td && td->len > 0)
          vlen = (gsize)td->len;
        else
          continue;
      }
    }
    klv_size += 1 + 4 + vlen; /* Tag + max BER length + value */
  }
  klv_size += 4; /* Tag 1 (Checksum): tag(1) + len(1) + value(2) */

  /* Allocate and map output buffer for writing */
  gst_buffer_set_size(outbuf, klv_size);
  GstMapInfo outmap;
  if (!gst_buffer_map(outbuf, &outmap, GST_MAP_WRITE)) {
    FREE_PARSED_TAGS();
    return GST_FLOW_ERROR;
  }

  guint8 *p = outmap.data;

  /* Write UL */
  memcpy(p, KLV_MISB_ST0601_UL, sizeof(KLV_MISB_ST0601_UL));
  p += sizeof(KLV_MISB_ST0601_UL);

  /* Write Local Set TLVs */
  guint8 *ls_start = p;
  p += 4; /* Reserve for BER length of Local Set */

  guint8 *ls_data_start = p;
  for (gint i = 0; i < tag_count; i++) {
    gsize tlv_size = klv_encode_tlv(p, &tags[i], tag_defs);
    p += tlv_size;
  }

  /* Append Tag 1 (Checksum) placeholder — value filled in after BER encoding */
  guint8 *cs_tlv_start = p;
  *p++ = 0x01; /* Tag */
  *p++ = 0x02; /* Length */
  *p++ = 0x00; /* Value high byte (placeholder) */
  *p++ = 0x00; /* Value low byte  (placeholder) */

  /* Encode Local Set length (includes checksum TLV) */
  gsize ls_len = p - ls_data_start;

  if (ls_len > 0xFFFFFF) {
    GST_ERROR_OBJECT(transform, "Local Set payload too large (%zu bytes, max 16777215)", ls_len);
    gst_buffer_unmap(outbuf, &outmap);
    FREE_PARSED_TAGS();
    return GST_FLOW_ERROR;
  }

  gint ls_len_encoded = klv_ber_encode_length(ls_start, ls_len);

  if (ls_len_encoded < 4) {
    /* Shift Local Set data left to remove unused BER space */
    memmove(ls_start + ls_len_encoded, ls_data_start, ls_len);
    cs_tlv_start = ls_start + ls_len_encoded + (cs_tlv_start - ls_data_start);
    p = ls_start + ls_len_encoded + ls_len;
  }

  /* Compute BCC-16 over all bytes from UL start through the checksum length byte.
   * Per MISB ST 0601 the checksum covers the entire packet excluding the 2 value bytes. */
  guint8 *cs_value = cs_tlv_start + 2; /* Points to the two placeholder bytes */
  guint16 cs = klv_bcc_16(outmap.data, (gsize)(cs_value - outmap.data));
  cs_value[0] = (guint8)((cs >> 8) & 0xFF);
  cs_value[1] = (guint8)(cs & 0xFF);

  /* Trim buffer to actual size */
  gsize actual_size = p - outmap.data;
  gst_buffer_unmap(outbuf, &outmap);
  gst_buffer_set_size(outbuf, actual_size);

  /* Copy PTS/DTS */
  GST_BUFFER_PTS(outbuf) = GST_BUFFER_PTS(inbuf);
  GST_BUFFER_DTS(outbuf) = GST_BUFFER_DTS(inbuf);

  FREE_PARSED_TAGS();

#undef FREE_PARSED_TAGS

  return GST_FLOW_OK;
}

static gboolean
gst_klv_encode_set_caps(GstBaseTransform *transform, GstCaps *incaps, GstCaps *outcaps)
{
  (void)transform;
  (void)incaps;
  (void)outcaps;
  return TRUE;
}

static GstCaps *
gst_klv_encode_transform_caps(GstBaseTransform *transform,
                              GstPadDirection direction,
                              GstCaps *caps,
                              GstCaps *filter)
{
  (void)transform;
  (void)caps;
  GstCaps *result = NULL;

  if (direction == GST_PAD_SINK) {
    /* Input: application/json, output: meta/x-klv */
    result = gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
  }
  else {
    /* Input: meta/x-klv, output: application/json */
    result = gst_caps_new_empty_simple("application/json");
  }

  if (filter) {
    GstCaps *filtered = gst_caps_intersect(result, filter);
    gst_caps_unref(result);
    return filtered;
  }

  return result;
}

static gboolean
gst_klv_encode_transform_size(GstBaseTransform *transform,
                              GstPadDirection direction,
                              GstCaps *caps,
                              gsize size,
                              GstCaps *othercaps,
                              gsize *othersize)
{
  (void)transform;
  (void)caps;
  (void)othercaps;
  /* KLV output size is variable, but estimate conservatively */
  if (direction == GST_PAD_SINK) {
    *othersize = size * 2 + 256; /* Conservative estimate */
  }
  else {
    *othersize = size * 2;
  }
  return TRUE;
}

static void
gst_klv_encode_class_init(GstKlvEncodeClass *klass)
{
  GstBaseTransformClass *btrans_class = GST_BASE_TRANSFORM_CLASS(klass);

  gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                        "KLV Encoder",
                                        "Transform/Metadata",
                                        "Encode JSON to MISB ST 0601 KLV",
                                        "Mouhsine Kassimi Farhaoui <mouhsine98@gmail.com>");

  btrans_class->set_caps = GST_DEBUG_FUNCPTR(gst_klv_encode_set_caps);
  btrans_class->transform_caps = GST_DEBUG_FUNCPTR(gst_klv_encode_transform_caps);
  btrans_class->transform_size = GST_DEBUG_FUNCPTR(gst_klv_encode_transform_size);
  btrans_class->transform = GST_DEBUG_FUNCPTR(gst_klv_encode_transform);

  /* Add pad templates */
  GstCaps *json_caps = gst_caps_new_empty_simple("application/json");
  gst_element_class_add_pad_template(
    GST_ELEMENT_CLASS(klass),
    gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, json_caps));
  gst_caps_unref(json_caps);

  GstCaps *klv_caps = gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
  gst_element_class_add_pad_template(
    GST_ELEMENT_CLASS(klass), gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, klv_caps));
  gst_caps_unref(klv_caps);
}

static void
gst_klv_encode_init(GstKlvEncode *klvmetaenc)
{
  (void)klvmetaenc;
}
