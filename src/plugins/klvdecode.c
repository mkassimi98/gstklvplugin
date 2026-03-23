/**
 * @file src/plugins/klvdecode.c
 * @brief Implementation of the GstKlvDecode element (KLV to JSON).
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

#include "gstklv/klvdecode.h"

G_DEFINE_TYPE(GstKlvDecode, gst_klv_decode, GST_TYPE_BASE_TRANSFORM)

static GHashTable *g_tag_defs = NULL;
static GMutex g_tag_defs_lock;

/**
 * @brief Resolve tag metadata definitions from the INI registry.
 * @return Shared tag definition hash table.
 */
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

/**
 * @brief Build JSON from a MISB ST 0601 local set.
 * @param data Input KLV payload (including UL).
 * @param data_len Length of @p data in bytes.
 * @param out Output JSON string builder.
 * @return TRUE on success, FALSE on parse error.
 */
static gboolean
klv_build_json(const guint8 *data, gsize data_len, GString *out)
{
  gsize pos = 0;

  if (data_len < 16) {
    GST_ERROR("KLV packet too short");
    return FALSE;
  }

  if (memcmp(data, KLV_MISB_ST0601_UL, 16) != 0) {
    GST_ERROR("Invalid MISB ST 0601 UL");
    return FALSE;
  }
  pos = 16;

  gsize ls_len;
  if (!klv_ber_decode_length(data, data_len, &pos, &ls_len)) {
    GST_ERROR("Failed to decode Local Set length");
    return FALSE;
  }

  if (pos + ls_len > data_len) {
    GST_ERROR("Local Set extends beyond buffer");
    return FALSE;
  }

  gsize ls_end = pos + ls_len;
  gboolean first = TRUE;
  GHashTable *tag_defs = get_tag_defs();

  g_string_append(out, "{");

  while (pos < ls_end) {
    if (pos >= data_len)
      break;

    gint tag_id = data[pos];
    pos++;

    gsize tlv_len;
    if (!klv_ber_decode_length(data, data_len, &pos, &tlv_len))
      break;

    if (pos + tlv_len > data_len)
      break;

    const guint8 *tlv_value = data + pos;
    pos += tlv_len;

    KLVTagDef *td =
      tag_defs ? (KLVTagDef *)g_hash_table_lookup(tag_defs, GINT_TO_POINTER(tag_id)) : NULL;

    /* Tag 1 (Checksum): validate BCC-16 and skip from JSON output.
     * Per MISB ST 0601 the checksum covers all bytes from UL start through
     * the checksum length byte (i.e., the full packet minus the 2 value bytes). */
    if (tag_id == 1 && tlv_len == 2) {
      guint16 received_cs = ((guint16)tlv_value[0] << 8) | tlv_value[1];
      /* pos now points past the value bytes; BCC-16 covers all bytes up to
       * and including the checksum length byte (pos - 2). */
      guint16 computed_cs = klv_bcc_16(data, pos - 2);
      if (computed_cs != received_cs) {
        GST_WARNING("KLV checksum mismatch: expected 0x%04X, got 0x%04X "
                    "(packet may be corrupted)",
                    computed_cs,
                    received_cs);
      }
      /* MISB ST 0601 requires Tag 1 to be the last tag in the Local Set. */
      if (pos < ls_end) {
        GST_WARNING("Tag 1 (Checksum) is not the last tag in the Local Set "
                    "-- MISB ST 0601 violation, checksum may be invalid");
      }
      continue; /* Checksum is internal -- not emitted in JSON */
    }

    gchar *value_str = NULL;
    gboolean value_is_string = FALSE;

    /* Special-case known tags for correct scaling */
    if (tag_id == 2 && tlv_len == 8) {
      guint64 ts = 0;
      for (gsize i = 0; i < 8; i++)
        ts = (ts << 8) | tlv_value[i];
      value_str = klv_json_format_number_guint64(ts);
    }
    else if (tag_id == 72 && tlv_len == 8) {
      guint64 ts = 0;
      for (gsize i = 0; i < 8; i++)
        ts = (ts << 8) | tlv_value[i];
      value_str = klv_json_format_number_guint64(ts);
    }
    else if (td && td->type &&
             (g_str_has_prefix(td->type, "String") || g_str_has_prefix(td->type, "string"))) {
      gchar *s = g_strndup((const gchar *)tlv_value, tlv_len);
      if (g_utf8_validate(s, -1, NULL)) {
        value_str = s;
      }
      else {
        gchar *b64 = g_base64_encode(tlv_value, tlv_len);
        value_str = g_strconcat("base64:", b64, NULL);
        g_free(b64);
        g_free(s);
      }
      value_is_string = TRUE;
    }
    else if (td && td->type && g_str_has_prefix(td->type, "bytes")) {
      gchar *b64 = g_base64_encode(tlv_value, tlv_len);
      value_str = g_strconcat("base64:", b64, NULL);
      g_free(b64);
      value_is_string = TRUE;
    }
    else {
      /* Numeric decode */
      guint64 raw = 0;
      for (gsize i = 0; i < tlv_len; i++)
        raw = (raw << 8) | tlv_value[i];

      gboolean is_signed = td ? td->is_signed : FALSE;
      if (td && td->has_range) {
        gdouble decoded =
          klv_decode_with_range(raw, td->range_min, td->range_max, is_signed, (gint)(tlv_len * 8));
        value_str = klv_json_format_number_double(decoded);
      }
      else if (is_signed) {
        gint64 sval = klv_sign_extend(raw, (gint)(tlv_len * 8));
        value_str = klv_json_format_number_gint64(sval);
      }
      else {
        value_str = klv_json_format_number_guint64(raw);
      }
    }

    if (!value_str)
      continue;

    if (!first)
      g_string_append(out, ",");
    first = FALSE;

    g_string_append_printf(out, "\"%d\":", tag_id);
    if (value_is_string) {
      g_string_append_c(out, '"');
      klv_json_append_escaped(out, value_str, strlen(value_str));
      g_string_append_c(out, '"');
    }
    else {
      g_string_append(out, value_str);
    }

    g_free(value_str);
  }

  g_string_append(out, "}");
  return TRUE;
}

/**
 * @brief Transform KLV input into JSON output.
 * @param transform Base transform instance.
 * @param inbuf Input KLV buffer.
 * @param outbuf Output JSON buffer.
 * @return GstFlowReturn status.
 */
static GstFlowReturn
gst_klv_decode_transform(GstBaseTransform *transform, GstBuffer *inbuf, GstBuffer *outbuf)
{
  GstMapInfo inmap;
  if (!gst_buffer_map(inbuf, &inmap, GST_MAP_READ)) {
    return GST_FLOW_ERROR;
  }

  GString *json = g_string_sized_new(inmap.size * 4 + 256);
  gboolean parse_ok = klv_build_json(inmap.data, inmap.size, json);
  gst_buffer_unmap(inbuf, &inmap);

  if (!parse_ok) {
    g_string_free(json, TRUE);
    GST_ERROR_OBJECT(transform, "KLV parse error");
    return GST_FLOW_ERROR;
  }

  gst_buffer_set_size(outbuf, json->len);
  GstMapInfo outmap;
  if (!gst_buffer_map(outbuf, &outmap, GST_MAP_WRITE)) {
    g_string_free(json, TRUE);
    return GST_FLOW_ERROR;
  }

  memcpy(outmap.data, json->str, json->len);
  gst_buffer_unmap(outbuf, &outmap);
  g_string_free(json, TRUE);

  GST_BUFFER_PTS(outbuf) = GST_BUFFER_PTS(inbuf);
  GST_BUFFER_DTS(outbuf) = GST_BUFFER_DTS(inbuf);

  return GST_FLOW_OK;
}

/**
 * @brief Set caps on the base transform.
 * @param transform Base transform instance.
 * @param incaps Input caps.
 * @param outcaps Output caps.
 * @return TRUE on success.
 */
static gboolean
gst_klv_decode_set_caps(GstBaseTransform *transform, GstCaps *incaps, GstCaps *outcaps)
{
  (void)transform;
  (void)incaps;
  (void)outcaps;
  return TRUE;
}

/**
 * @brief Convert caps between sink and source directions.
 * @param transform Base transform instance.
 * @param direction Pad direction.
 * @param caps Input caps.
 * @param filter Optional filter caps.
 * @return Caps describing the conversion result.
 */
static GstCaps *
gst_klv_decode_transform_caps(GstBaseTransform *transform,
                              GstPadDirection direction,
                              GstCaps *caps,
                              GstCaps *filter)
{
  (void)transform;
  (void)caps;
  GstCaps *result = NULL;

  if (direction == GST_PAD_SINK) {
    /* Input: meta/x-klv, output: application/json */
    result = gst_caps_new_empty_simple("application/json");
  }
  else {
    /* Input: application/json, output: meta/x-klv */
    result = gst_caps_new_empty_simple("meta/x-klv");
  }

  if (filter) {
    GstCaps *filtered = gst_caps_intersect(result, filter);
    gst_caps_unref(result);
    return filtered;
  }

  return result;
}

/**
 * @brief Compute output size estimate for a given input buffer.
 * @param transform Base transform instance.
 * @param direction Pad direction.
 * @param caps Input caps.
 * @param size Input size.
 * @param othercaps Output caps.
 * @param othersize Output size estimate.
 * @return TRUE on success.
 */
static gboolean
gst_klv_decode_transform_size(GstBaseTransform *transform,
                              GstPadDirection direction,
                              GstCaps *caps,
                              gsize size,
                              GstCaps *othercaps,
                              gsize *othersize)
{
  (void)transform;
  (void)caps;
  (void)othercaps;
  /* JSON output can be much larger than KLV input */
  if (direction == GST_PAD_SINK) {
    *othersize = size * 12 + 4096;
  }
  else {
    *othersize = size * 2;
  }
  return TRUE;
}

static void
gst_klv_decode_class_init(GstKlvDecodeClass *klass)
{
  GstBaseTransformClass *btrans_class = GST_BASE_TRANSFORM_CLASS(klass);

  gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                        "KLV Decoder",
                                        "Transform/Metadata",
                                        "Decode MISB ST 0601 KLV to JSON",
                                        "Mouhsine Kassimi Farhaoui <mouhsine98@gmail.com>");

  btrans_class->set_caps = GST_DEBUG_FUNCPTR(gst_klv_decode_set_caps);
  btrans_class->transform_caps = GST_DEBUG_FUNCPTR(gst_klv_decode_transform_caps);
  btrans_class->transform_size = GST_DEBUG_FUNCPTR(gst_klv_decode_transform_size);
  btrans_class->transform = GST_DEBUG_FUNCPTR(gst_klv_decode_transform);

  GstCaps *klv_caps = gst_caps_new_empty_simple("meta/x-klv");
  gst_element_class_add_pad_template(
    GST_ELEMENT_CLASS(klass), gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, klv_caps));
  gst_caps_unref(klv_caps);

  GstCaps *json_caps = gst_caps_new_empty_simple("application/json");
  gst_element_class_add_pad_template(
    GST_ELEMENT_CLASS(klass), gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, json_caps));
  gst_caps_unref(json_caps);
}

static void
gst_klv_decode_init(GstKlvDecode *klvmetadec)
{
  (void)klvmetadec;
}
