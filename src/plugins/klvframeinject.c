/**
 * @file src/plugins/klvframeinject.c
 * @brief Implementation of the GstKlvFrameInject element.
 * @ingroup gstklv_plugins
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include <gst/gst.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include "gstklv/internal/klv_ber.h"
#include "gstklv/internal/klv_checksum.h"
#include "gstklv/internal/klv_tag_defs.h"
#include "gstklv/internal/klv_ul.h"

#include "gstklv/klvframeinject.h"

/* Forward declarations */
static void gst_klv_frame_inject_class_init(GstKlvFrameInjectClass *klass);
static void gst_klv_frame_inject_init(GstKlvFrameInject *inject);

G_DEFINE_TYPE(GstKlvFrameInject, gst_klv_frame_inject, GST_TYPE_ELEMENT)

/**
 * @brief Parsed JSON tag value representation.
 */
typedef struct
{
  gboolean is_string;
  gboolean is_bytes;
  gdouble num;
  gchar *str;
  guint8 *bytes;
  gsize bytes_len;
} JsonTagVal;

static void
free_json_tag_val(gpointer data)
{
  JsonTagVal *val = (JsonTagVal *)data;
  if (!val)
    return;
  g_free(val->str);
  g_free(val->bytes);
  g_free(val);
}

/**
 * @brief Private instance data for GstKlvFrameInject.
 */
typedef struct
{
  GstPad *sinkpad;
  GstPad *video_src;
  GstPad *klv_src;
  GMutex prop_lock;
  gdouble latitude;
  gdouble longitude;
  gdouble heading;
  gdouble altitude;
  guint64 timestamp;
  gboolean use_system_time;
  gchar *tags_json;
  GHashTable *tag_defs;
} GstKlvFrameInjectPrivate;

#define GST_KLV_FRAME_INJECT_GET_PRIVATE(obj)    \
  ((GstKlvFrameInjectPrivate *)g_type_get_qdata( \
    G_OBJECT_TYPE(obj), g_quark_from_static_string("GstKlvFrameInjectPrivate")))

/**
 * @brief Resolve a byte length from a tag type string.
 * @param type Tag type string from the INI registry.
 * @return Byte length for numeric types.
 */
static gint
type_len_from_string(const gchar *type)
{
  if (!type)
    return 1;

  if (g_str_has_prefix(type, "uint8") || g_str_has_prefix(type, "int8"))
    return 1;
  if (g_str_has_prefix(type, "uint16") || g_str_has_prefix(type, "int16"))
    return 2;
  if (g_str_has_prefix(type, "uint32") || g_str_has_prefix(type, "int32"))
    return 4;
  if (g_str_has_prefix(type, "uint64") || g_str_has_prefix(type, "int64"))
    return 8;

  return 1;
}

/**
 * @brief Clamp a floating point value to a range.
 * @param v Input value.
 * @param min Minimum bound.
 * @param max Maximum bound.
 * @return Clamped value.
 */
static gdouble
clamp_double(gdouble v, gdouble min, gdouble max)
{
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

/**
 * @brief Encode a value into an integer range with optional sign.
 * @param value Physical value to encode.
 * @param min Minimum physical range.
 * @param max Maximum physical range.
 * @param is_signed Whether the field is signed.
 * @param bits Bit width of the target field.
 * @return Encoded integer value.
 */
static guint64
encode_with_range(gdouble value, gdouble min, gdouble max, gboolean is_signed, gint bits)
{
  if (max <= min)
    return 0;

  if (is_signed) {
    gint64 smax = (bits == 64) ? G_MAXINT64 : (((gint64)1 << (bits - 1)) - 1);
    gdouble norm = (value - min) / (max - min);
    gdouble scaled = (norm * (2.0 * smax)) - smax;
    gint64 sval = (gint64)llround(clamp_double(scaled, (gdouble)(-smax), (gdouble)smax));
    if (bits == 64)
      return (guint64)sval;
    guint64 mask = ((guint64)1 << bits) - 1;
    return ((guint64)sval) & mask;
  }

  guint64 umax = (bits == 64) ? G_MAXUINT64 : (((guint64)1 << bits) - 1);
  gdouble norm = (value - min) / (max - min);
  gdouble scaled = norm * (gdouble)umax;
  guint64 uval = (guint64)llround(clamp_double(scaled, 0.0, (gdouble)umax));
  return uval;
}

static gint
compare_tag_id(gconstpointer a, gconstpointer b)
{
  return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

static void
append_tag_tlv(GByteArray *tlv, guint8 tag_id, const guint8 *value, gsize value_len)
{
  if (!tlv)
    return;
  g_byte_array_append(tlv, &tag_id, 1);
  klv_ber_append_length(tlv, value_len);
  if (value && value_len > 0)
    g_byte_array_append(tlv, value, value_len);
}

static guint8 *
parse_hex_string(const gchar *str, gsize *out_len)
{
  if (!str || !out_len)
    return NULL;

  gchar *tmp = g_strdup(str);
  g_strstrip(tmp);

  GString *hex = g_string_sized_new(strlen(tmp));
  for (gchar *p = tmp; *p; p++) {
    if (g_ascii_isxdigit(*p)) {
      g_string_append_c(hex, *p);
    }
  }

  if ((hex->len % 2) != 0) {
    g_string_free(hex, TRUE);
    g_free(tmp);
    return NULL;
  }

  gsize bytes_len = hex->len / 2;
  guint8 *bytes = (guint8 *)g_malloc0(bytes_len);
  for (gsize i = 0; i < bytes_len; i++) {
    gchar byte_str[3] = {hex->str[i * 2], hex->str[i * 2 + 1], '\0'};
    bytes[i] = (guint8)g_ascii_strtoll(byte_str, NULL, 16);
  }

  g_string_free(hex, TRUE);
  g_free(tmp);
  *out_len = bytes_len;
  return bytes;
}

/**
 * @brief Parse a flat JSON object into tag values.
 * @param json_str JSON string containing numeric keys.
 * @return Hash table mapping tag id to JsonTagVal.
 */
/* Module-level regex cache -- compiled once, reused across all frames and instances.
 * GRegex match operations are thread-safe; g_once guarantees single initialisation. */
static GRegex *s_re_num = NULL;
static GRegex *s_re_str = NULL;
static GOnce s_regex_once = G_ONCE_INIT;

static gpointer
init_json_regex(gpointer unused)
{
  s_re_num = g_regex_new("\"([0-9]+)\"\\s*:\\s*([-+]?(?:[0-9]*\\.?[0-9]+)(?:[eE][-+]?[0-9]+)?)",
                         (GRegexCompileFlags)0,
                         (GRegexMatchFlags)0,
                         NULL);
  s_re_str = g_regex_new("\"([0-9]+)\"\\s*:\\s*\"([^\"\\\\]*(?:\\\\.[^\"\\\\]*)*)\"",
                         (GRegexCompileFlags)0,
                         (GRegexMatchFlags)0,
                         NULL);
  (void)unused;
  return NULL;
}

static GHashTable *
parse_json_tags(const gchar *json_str)
{
  GHashTable *tags = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_json_tag_val);
  if (!json_str || strlen(json_str) < 2)
    return tags;

  g_once(&s_regex_once, init_json_regex, NULL);
  GRegex *re_num = s_re_num;
  GRegex *re_str = s_re_str;

  if (!re_num || !re_str)
    return tags;

  gint parsed_count = 0;

  /* Parse numeric tags first */
  GMatchInfo *match_info = NULL;
  g_regex_match(re_num, json_str, (GRegexMatchFlags)0, &match_info);
  while (match_info && g_match_info_matches(match_info)) {
    gchar *key = g_match_info_fetch(match_info, 1);
    gchar *val = g_match_info_fetch(match_info, 2);

    if (key && val) {
      gint tag_id = atoi(key);
      JsonTagVal *pval = g_new0(JsonTagVal, 1);
      pval->is_string = FALSE;
      pval->num = g_strtod(val, NULL);
      g_hash_table_insert(tags, GINT_TO_POINTER(tag_id), pval);
      parsed_count++;
    }

    g_free(key);
    g_free(val);
    if (!g_match_info_next(match_info, NULL))
      break;
  }

  if (match_info)
    g_match_info_free(match_info);

  /* Parse string tags */
  g_regex_match(re_str, json_str, (GRegexMatchFlags)0, &match_info);
  while (match_info && g_match_info_matches(match_info)) {
    gchar *key = g_match_info_fetch(match_info, 1);
    gchar *val = g_match_info_fetch(match_info, 2);

    if (key && val) {
      gint tag_id = atoi(key);
      JsonTagVal *pval = g_new0(JsonTagVal, 1);
      if (g_ascii_strncasecmp(val, "base64:", 7) == 0) {
        gsize out_len = 0;
        pval->bytes = g_base64_decode(val + 7, &out_len);
        pval->bytes_len = out_len;
        pval->is_bytes = TRUE;
      }
      else if (g_ascii_strncasecmp(val, "hex:", 4) == 0) {
        gsize out_len = 0;
        pval->bytes = parse_hex_string(val + 4, &out_len);
        pval->bytes_len = out_len;
        pval->is_bytes = (pval->bytes != NULL);
      }
      else {
        pval->is_string = TRUE;
        pval->str = g_strdup(val);
      }
      g_hash_table_insert(tags, GINT_TO_POINTER(tag_id), pval);
      parsed_count++;
    }

    g_free(key);
    g_free(val);
    if (!g_match_info_next(match_info, NULL))
      break;
  }

  if (match_info)
    g_match_info_free(match_info);

  /* re_num and re_str are module-level cached patterns -- do not unref here */

  if (g_getenv("KLV_DEBUG")) {
    g_print("parse_json_tags: parsed %d tags from JSON (len=%lu)\n",
            parsed_count,
            (gulong)(json_str ? strlen(json_str) : 0));
  }

  return tags;
}

/**
 * @brief Build a ST 0601 KLV packet from JSON tags or element properties.
 * @param priv Element private state.
 * @param pts Video PTS (used for metadata correlation).
 * @param dts Video DTS (reserved for future use).
 * @return Newly allocated KLV buffer.
 */
static GstBuffer *
create_klv_buffer(GstKlvFrameInjectPrivate *priv, GstClockTime pts, GstClockTime dts)
{
  static gboolean logged_once = FALSE;
  if (!logged_once && g_getenv("KLV_DEBUG")) {
    KLVTagDef *t2 =
      priv->tag_defs ? (KLVTagDef *)g_hash_table_lookup(priv->tag_defs, GINT_TO_POINTER(2)) : NULL;
    g_print("klvframeinject: tag_defs=%u, tag2_len=%d\n",
            priv->tag_defs ? g_hash_table_size(priv->tag_defs) : 0,
            t2 ? t2->len : -1);
    logged_once = TRUE;
  }

  if (!priv->tag_defs || g_hash_table_size(priv->tag_defs) == 0) {
    if (!priv->tag_defs) {
      priv->tag_defs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, klv_tag_def_free);
    }
    const gchar *ini_path = g_getenv("KLV_TAGS_INI");
    if (ini_path && g_file_test(ini_path, G_FILE_TEST_EXISTS)) {
      if (priv->tag_defs)
        g_hash_table_destroy(priv->tag_defs);
      priv->tag_defs = klv_load_tag_defs_from_ini(ini_path);
#ifdef KLV_TAGS_DEFAULT_PATH
    }
    else if (g_file_test(KLV_TAGS_DEFAULT_PATH, G_FILE_TEST_EXISTS)) {
      if (priv->tag_defs)
        g_hash_table_destroy(priv->tag_defs);
      priv->tag_defs = klv_load_tag_defs_from_ini(KLV_TAGS_DEFAULT_PATH);
#endif
    }

    if (priv->tag_defs && g_hash_table_size(priv->tag_defs) == 0) {
      if (g_file_test("data/stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
        g_hash_table_destroy(priv->tag_defs);
        priv->tag_defs = klv_load_tag_defs_from_ini("data/stanag4609_tags.ini");
      }
      else if (g_file_test("./stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
        g_hash_table_destroy(priv->tag_defs);
        priv->tag_defs = klv_load_tag_defs_from_ini("./stanag4609_tags.ini");
      }
    }
  }

  g_mutex_lock(&priv->prop_lock);
  gchar *json_str = g_strdup(priv->tags_json ? priv->tags_json : "{}");
  gdouble lat = priv->latitude;
  gdouble lon = priv->longitude;
  gdouble hdg = priv->heading;
  gdouble alt = priv->altitude;
  guint64 ts = priv->use_system_time ? (guint64)time(NULL) : priv->timestamp;
  g_mutex_unlock(&priv->prop_lock);

  GHashTable *json_tags = parse_json_tags(json_str);

  if (g_getenv("KLV_DEBUG")) {
    g_print("create_klv_buffer: json_str length=%lu, parsed %d tags\n",
            (gulong)(json_str ? strlen(json_str) : 0),
            g_hash_table_size(json_tags));
  }

  GByteArray *tlv = g_byte_array_sized_new(2048);
  gsize checksum_tag_offset_in_tlv = 0;
  gsize checksum_value_offset_in_tlv = 0;
  gint encoded_count = 0;

  if (g_hash_table_size(json_tags) > 0) {
    GList *keys = g_hash_table_get_keys(json_tags);
    keys = g_list_sort(keys, compare_tag_id);

    for (GList *l = keys; l != NULL; l = l->next) {
      gint tag_id = GPOINTER_TO_INT(l->data);
      JsonTagVal *jval = (JsonTagVal *)g_hash_table_lookup(json_tags, GINT_TO_POINTER(tag_id));
      KLVTagDef *td = priv->tag_defs
                        ? (KLVTagDef *)g_hash_table_lookup(priv->tag_defs, GINT_TO_POINTER(tag_id))
                        : NULL;

      if (tag_id <= 0 || tag_id > 95)
        continue;
      if (tag_id == 1)
        continue; /* checksum is computed and appended last */
      if (!td)
        continue;
      if (!td->type || td->type[0] == '\0')
        continue;

      gboolean is_string_tag = td->type && g_str_has_prefix(td->type, "String");
      gboolean is_bytes_tag = td->type && g_str_has_prefix(td->type, "bytes");

      if (is_bytes_tag) {
        if (!jval || !jval->is_bytes || !jval->bytes || jval->bytes_len == 0)
          continue;
        append_tag_tlv(tlv, (guint8)tag_id, jval->bytes, jval->bytes_len);
        encoded_count++;
        continue;
      }

      if (is_string_tag) {
        const gchar *payload = (jval && jval->is_string && jval->str) ? jval->str : NULL;
        if (!payload)
          continue;
        gsize str_len = strlen(payload);
        if (str_len > 127)
          str_len = 127;
        append_tag_tlv(tlv, (guint8)tag_id, (const guint8 *)payload, str_len);
        encoded_count++;
        continue;
      }

      if (!jval)
        continue;

      gint tag_len = td->len > 0 ? td->len : type_len_from_string(td->type);
      if (tag_len <= 0)
        continue;
      if (tag_len > 8)
        tag_len = 8;

      guint8 val_bytes[8];
      memset(val_bytes, 0, sizeof(val_bytes));
      gdouble val = jval->num;

      if ((tag_id == 2 || tag_id == 72) && tag_len == 8) {
        guint64 tsv = (guint64)val;
        for (gint i = tag_len - 1; i >= 0; i--) {
          val_bytes[tag_len - 1 - i] = (guint8)((tsv >> (i * 8)) & 0xFF);
        }
      }
      else if (td->has_range) {
        guint64 raw =
          encode_with_range(val, td->range_min, td->range_max, td->is_signed, tag_len * 8);
        for (gint i = tag_len - 1; i >= 0; i--) {
          val_bytes[tag_len - 1 - i] = (guint8)((raw >> (i * 8)) & 0xFF);
        }
      }
      else {
        guint64 raw = 0;
        if (td->is_signed) {
          gint64 sval = (gint64)llround(val / (td ? td->scale : 1.0));
          raw = (guint64)sval;
        }
        else {
          raw = (guint64)llround(val / (td ? td->scale : 1.0));
        }
        for (gint i = tag_len - 1; i >= 0; i--) {
          val_bytes[tag_len - 1 - i] = (guint8)((raw >> (i * 8)) & 0xFF);
        }
      }

      append_tag_tlv(tlv, (guint8)tag_id, val_bytes, tag_len);
      encoded_count++;
    }

    g_list_free(keys);
  }
  else {
    /* Fallback: use hardcoded tags 2, 5, 13, 14, 15 from properties */
    struct
    {
      gint tag_id;
      gdouble val;
    } fallback_tags[] = {{2, (gdouble)ts}, {5, hdg}, {13, lat}, {14, lon}, {15, alt}};
    for (guint i = 0; i < G_N_ELEMENTS(fallback_tags); i++) {
      gint tag_id = fallback_tags[i].tag_id;
      KLVTagDef *td = priv->tag_defs
                        ? (KLVTagDef *)g_hash_table_lookup(priv->tag_defs, GINT_TO_POINTER(tag_id))
                        : NULL;
      if (!td)
        continue;
      gint tag_len = td->len > 0 ? td->len : type_len_from_string(td->type);
      if (tag_len <= 0)
        continue;
      if (tag_len > 8)
        tag_len = 8;

      guint8 val_bytes[8];
      memset(val_bytes, 0, sizeof(val_bytes));

      if ((tag_id == 2 || tag_id == 72) && tag_len == 8) {
        guint64 tsv = (guint64)fallback_tags[i].val;
        for (gint j = tag_len - 1; j >= 0; j--) {
          val_bytes[tag_len - 1 - j] = (guint8)((tsv >> (j * 8)) & 0xFF);
        }
      }
      else if (td->has_range) {
        guint64 raw = encode_with_range(
          fallback_tags[i].val, td->range_min, td->range_max, td->is_signed, tag_len * 8);
        for (gint j = tag_len - 1; j >= 0; j--) {
          val_bytes[tag_len - 1 - j] = (guint8)((raw >> (j * 8)) & 0xFF);
        }
      }
      else {
        guint64 raw = (guint64)llround(fallback_tags[i].val / (td ? td->scale : 1.0));
        for (gint j = tag_len - 1; j >= 0; j--) {
          val_bytes[tag_len - 1 - j] = (guint8)((raw >> (j * 8)) & 0xFF);
        }
      }

      append_tag_tlv(tlv, (guint8)tag_id, val_bytes, tag_len);
      encoded_count++;
    }
  }

  /* Append checksum tag last (Tag 1, length 2) */
  {
    guint8 tag_id_byte = 1;
    checksum_tag_offset_in_tlv = tlv->len;
    g_byte_array_append(tlv, &tag_id_byte, 1);
    klv_ber_append_length(tlv, 2);
    checksum_value_offset_in_tlv = tlv->len;
    guint8 zero_bytes[2] = {0, 0};
    g_byte_array_append(tlv, zero_bytes, sizeof(zero_bytes));
  }

  g_free(json_str);
  g_hash_table_destroy(json_tags);

  /* Compute checksum over Local Set (excluding checksum tag itself) */
  if (checksum_tag_offset_in_tlv > 0 && checksum_value_offset_in_tlv + 1 < tlv->len) {
    guint16 checksum = klv_bcc_16(tlv->data, checksum_tag_offset_in_tlv);
    tlv->data[checksum_value_offset_in_tlv] = (guint8)((checksum >> 8) & 0xFF);
    tlv->data[checksum_value_offset_in_tlv + 1] = (guint8)(checksum & 0xFF);
  }

  /* Build final KLV: UL (16) + BER length + TLV data */
  GByteArray *klv = g_byte_array_sized_new(16 + 5 + tlv->len);
  g_byte_array_append(klv, KLV_MISB_ST0601_UL, sizeof(KLV_MISB_ST0601_UL));

  guint8 ls_len_buf[8];
  gsize ls_len = tlv->len;
  if (ls_len < 0x80) {
    ls_len_buf[0] = (guint8)ls_len;
    g_byte_array_append(klv, ls_len_buf, 1);
  }
  else {
    guint8 len_bytes[8];
    guint n = 0;
    gsize tmp = ls_len;
    while (tmp > 0 && n < sizeof(len_bytes)) {
      len_bytes[n++] = (guint8)(tmp & 0xFF);
      tmp >>= 8;
    }
    ls_len_buf[0] = (guint8)(0x80 | n);
    g_byte_array_append(klv, ls_len_buf, 1);
    for (gint i = (gint)n - 1; i >= 0; i--) {
      g_byte_array_append(klv, &len_bytes[i], 1);
    }
  }

  g_byte_array_append(klv, tlv->data, tlv->len);

  if (g_getenv("KLV_DEBUG")) {
    g_print(
      "create_klv_buffer: encoded %d tags, TLV size=%lu bytes\n", encoded_count, (gulong)tlv->len);
  }

  GstBuffer *klv_buf = gst_buffer_new_allocate(NULL, klv->len, NULL);
  GstMapInfo map;
  gst_buffer_map(klv_buf, &map, GST_MAP_WRITE);
  memcpy(map.data, klv->data, klv->len);
  gst_buffer_unmap(klv_buf, &map);

  GST_BUFFER_PTS(klv_buf) = pts;
  GST_BUFFER_DTS(klv_buf) = dts;

  g_byte_array_free(tlv, TRUE);
  g_byte_array_free(klv, TRUE);

  return klv_buf;
}

/**
 * @brief Sink pad chain function that emits video and KLV buffers.
 * @param pad Sink pad.
 * @param parent Parent element.
 * @param buffer Incoming video buffer.
 * @return GstFlowReturn status.
 */
static GstFlowReturn
gst_klv_frame_inject_chain(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  (void)pad;
  GstKlvFrameInject *inject = GST_KLV_FRAME_INJECT(parent);
  GstKlvFrameInjectPrivate *priv = GST_KLV_FRAME_INJECT_GET_PRIVATE(inject);

  /* Push video buffer unchanged to video_src */
  GstFlowReturn ret = gst_pad_push(priv->video_src, gst_buffer_copy(buffer));
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref(buffer);
    return ret;
  }

  /* Create and push KLV buffer to klv_src with current properties */
  GstBuffer *klv_buf = create_klv_buffer(priv, GST_BUFFER_PTS(buffer), GST_BUFFER_DTS(buffer));

  if (g_getenv("KLV_DEBUG")) {
    g_print("klvframeinject: pushing KLV buffer, size=%zu bytes, PTS=%" GST_TIME_FORMAT "\n",
            gst_buffer_get_size(klv_buf),
            GST_TIME_ARGS(GST_BUFFER_PTS(klv_buf)));
  }

  ret = gst_pad_push(priv->klv_src, klv_buf);

  gst_buffer_unref(buffer);
  return ret;
}

/* Event function for sink pad */
static gboolean
gst_klv_frame_inject_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
  (void)pad;
  GstKlvFrameInject *inject = GST_KLV_FRAME_INJECT(parent);
  GstKlvFrameInjectPrivate *priv = GST_KLV_FRAME_INJECT_GET_PRIVATE(inject);

  GST_DEBUG_OBJECT(inject, "Sink event: %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
      GstCaps *caps;
      gst_event_parse_caps(event, &caps);
      /* Push same caps to both source pads */
      gst_pad_set_caps(priv->video_src, caps);
      gst_pad_push_event(priv->video_src, gst_event_new_caps(gst_caps_copy(caps)));

      /* Use private data caps so mpegtsmux accepts it */
      GstCaps *klv_caps = gst_caps_new_simple("meta/x-klv",
                                              "parsed",
                                              G_TYPE_BOOLEAN,
                                              TRUE,
                                              "stream-format",
                                              G_TYPE_STRING,
                                              "klv",
                                              "stream-type",
                                              G_TYPE_INT,
                                              21,
                                              NULL);
      gst_pad_set_caps(priv->klv_src, klv_caps);
      gst_pad_push_event(priv->klv_src, gst_event_new_caps(klv_caps));
      gst_event_unref(event);
      return TRUE;
    }
    default:
      /* Forward events to both source pads */
      gst_pad_push_event(priv->video_src, gst_event_copy(event));
      gst_pad_push_event(priv->klv_src, event);
      return TRUE;
  }
}

static gboolean
gst_klv_frame_inject_sink_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
  switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_CAPS: {
      GstCaps *caps = gst_caps_new_simple("video/x-h264",
                                          "stream-format",
                                          G_TYPE_STRING,
                                          "byte-stream",
                                          "alignment",
                                          G_TYPE_STRING,
                                          "au",
                                          NULL);
      GstCaps *h265_caps = gst_caps_new_simple("video/x-h265",
                                               "stream-format",
                                               G_TYPE_STRING,
                                               "byte-stream",
                                               "alignment",
                                               G_TYPE_STRING,
                                               "au",
                                               NULL);
      gst_caps_append(caps, h265_caps);
      gst_query_set_caps_result(query, caps);
      gst_caps_unref(caps);
      return TRUE;
    }
    default: return gst_pad_query_default(pad, parent, query);
  }
}

static void
gst_klv_frame_inject_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  GstKlvFrameInject *inject = GST_KLV_FRAME_INJECT(object);
  GstKlvFrameInjectPrivate *priv = GST_KLV_FRAME_INJECT_GET_PRIVATE(inject);

  g_mutex_lock(&priv->prop_lock);

  switch (prop_id) {
    case 1: priv->latitude = g_value_get_double(value); break;
    case 2: priv->longitude = g_value_get_double(value); break;
    case 3: priv->heading = g_value_get_double(value); break;
    case 4: priv->altitude = g_value_get_double(value); break;
    case 5: priv->timestamp = g_value_get_uint64(value); break;
    case 6: priv->use_system_time = g_value_get_boolean(value); break;
    case 7:
      g_free(priv->tags_json);
      priv->tags_json = g_strdup(g_value_get_string(value));
      break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }

  g_mutex_unlock(&priv->prop_lock);
}

static void
gst_klv_frame_inject_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstKlvFrameInject *inject = GST_KLV_FRAME_INJECT(object);
  GstKlvFrameInjectPrivate *priv = GST_KLV_FRAME_INJECT_GET_PRIVATE(inject);

  g_mutex_lock(&priv->prop_lock);

  switch (prop_id) {
    case 1: g_value_set_double(value, priv->latitude); break;
    case 2: g_value_set_double(value, priv->longitude); break;
    case 3: g_value_set_double(value, priv->heading); break;
    case 4: g_value_set_double(value, priv->altitude); break;
    case 5: g_value_set_uint64(value, priv->timestamp); break;
    case 6: g_value_set_boolean(value, priv->use_system_time); break;
    case 7: g_value_set_string(value, priv->tags_json ? priv->tags_json : ""); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }

  g_mutex_unlock(&priv->prop_lock);
}

static void
gst_klv_frame_inject_class_init(GstKlvFrameInjectClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gst_element_class_set_static_metadata(
    element_class,
    "KLV Frame Injector",
    "Transform/Metadata",
    "Inject KLV metadata per video frame with dynamic properties",
    "Mouhsine Kassimi Farhaoui <mouhsine98@gmail.com>");

  /* Set property and get property handlers FIRST */
  gobject_class->set_property = gst_klv_frame_inject_set_property;
  gobject_class->get_property = gst_klv_frame_inject_get_property;

  /* Install properties */
  g_object_class_install_property(gobject_class,
                                  1,
                                  g_param_spec_double("latitude",
                                                      "Latitude",
                                                      "Sensor latitude (-90 to +90 degrees)",
                                                      -90.0,
                                                      90.0,
                                                      0.0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  2,
                                  g_param_spec_double("longitude",
                                                      "Longitude",
                                                      "Sensor longitude (-180 to +180 degrees)",
                                                      -180.0,
                                                      180.0,
                                                      0.0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  3,
                                  g_param_spec_double("heading",
                                                      "Heading",
                                                      "Platform heading angle (0 to 360 degrees)",
                                                      0.0,
                                                      360.0,
                                                      0.0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  4,
                                  g_param_spec_double("altitude",
                                                      "Altitude",
                                                      "Sensor altitude (meters)",
                                                      -900.0,
                                                      20000.0,
                                                      1000.0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(
    gobject_class,
    5,
    g_param_spec_uint64(
      "timestamp", "Timestamp", "Unix timestamp (seconds)", 0, G_MAXUINT64, 0, G_PARAM_READWRITE));

  g_object_class_install_property(
    gobject_class,
    6,
    g_param_spec_boolean("use-system-time",
                         "Use System Time",
                         "Use system time instead of timestamp property",
                         TRUE,
                         G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  7,
                                  g_param_spec_string("tags-json",
                                                      "Tags JSON",
                                                      "JSON-encoded tag data {tag_id: value, ...}",
                                                      "",
                                                      G_PARAM_READWRITE));

  /* Sink pad template */
  GstCaps *video_caps = gst_caps_new_simple("video/x-h264",
                                            "stream-format",
                                            G_TYPE_STRING,
                                            "byte-stream",
                                            "alignment",
                                            G_TYPE_STRING,
                                            "au",
                                            NULL);
  GstCaps *h265_caps = gst_caps_new_simple("video/x-h265",
                                           "stream-format",
                                           G_TYPE_STRING,
                                           "byte-stream",
                                           "alignment",
                                           G_TYPE_STRING,
                                           "au",
                                           NULL);
  gst_caps_append(video_caps, h265_caps);
  gst_element_class_add_pad_template(
    element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, video_caps));
  gst_caps_unref(video_caps);

  /* Source pad templates */
  GstCaps *video_src_caps = gst_caps_new_simple("video/x-h264",
                                                "stream-format",
                                                G_TYPE_STRING,
                                                "byte-stream",
                                                "alignment",
                                                G_TYPE_STRING,
                                                "au",
                                                NULL);
  h265_caps = gst_caps_new_simple("video/x-h265",
                                  "stream-format",
                                  G_TYPE_STRING,
                                  "byte-stream",
                                  "alignment",
                                  G_TYPE_STRING,
                                  "au",
                                  NULL);
  gst_caps_append(video_src_caps, h265_caps);
  gst_element_class_add_pad_template(
    element_class, gst_pad_template_new("video_src", GST_PAD_SRC, GST_PAD_ALWAYS, video_src_caps));
  gst_caps_unref(video_src_caps);

  GstCaps *klv_src_caps = gst_caps_new_simple(
    "meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "stream-format", G_TYPE_STRING, "klv", NULL);
  gst_element_class_add_pad_template(
    element_class, gst_pad_template_new("klv_src", GST_PAD_SRC, GST_PAD_ALWAYS, klv_src_caps));
  gst_caps_unref(klv_src_caps);
}

static void
gst_klv_frame_inject_init(GstKlvFrameInject *inject)
{
  GstKlvFrameInjectPrivate *priv;
  priv = g_new0(GstKlvFrameInjectPrivate, 1);

  /* Initialize mutex */
  g_mutex_init(&priv->prop_lock);

  /* Initialize properties with default values */
  priv->latitude = 0.0;
  priv->longitude = 0.0;
  priv->heading = 0.0;
  priv->altitude = 1000.0;
  priv->timestamp = 0;
  priv->use_system_time = TRUE;
  priv->tags_json = g_strdup("");

  /* Load tag definitions from INI file if available */
  const gchar *ini_path = g_getenv("KLV_TAGS_INI");
  if (ini_path && g_file_test(ini_path, G_FILE_TEST_EXISTS)) {
    GST_INFO("Loading tag definitions from %s", ini_path);
    priv->tag_defs = klv_load_tag_defs_from_ini(ini_path);
  }

  if (!priv->tag_defs || g_hash_table_size(priv->tag_defs) == 0) {
    if (priv->tag_defs) {
      g_hash_table_destroy(priv->tag_defs);
    }

    if (g_file_test("data/stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
      GST_INFO("Loading tag definitions from data/stanag4609_tags.ini");
      priv->tag_defs = klv_load_tag_defs_from_ini("data/stanag4609_tags.ini");
    }
    else if (g_file_test("./stanag4609_tags.ini", G_FILE_TEST_EXISTS)) {
      GST_INFO("Loading tag definitions from ./stanag4609_tags.ini");
      priv->tag_defs = klv_load_tag_defs_from_ini("./stanag4609_tags.ini");
    }
  }

  if (!priv->tag_defs) {
    priv->tag_defs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, klv_tag_def_free);
  }

  GST_INFO("Tag definitions loaded: %u", g_hash_table_size(priv->tag_defs));

  /* Create sink pad */
  GstPadTemplate *sink_template =
    gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(inject), "sink");
  priv->sinkpad = gst_pad_new_from_template(sink_template, "sink");
  gst_pad_set_chain_function(priv->sinkpad, gst_klv_frame_inject_chain);
  gst_pad_set_event_function(priv->sinkpad, gst_klv_frame_inject_sink_event);
  gst_pad_set_query_function(priv->sinkpad, gst_klv_frame_inject_sink_query);
  gst_element_add_pad(GST_ELEMENT(inject), priv->sinkpad);

  /* Create video source pad */
  GstPadTemplate *video_src_template =
    gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(inject), "video_src");
  priv->video_src = gst_pad_new_from_template(video_src_template, "video_src");
  gst_element_add_pad(GST_ELEMENT(inject), priv->video_src);

  /* Create KLV source pad */
  GstPadTemplate *klv_src_template =
    gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(inject), "klv_src");
  priv->klv_src = gst_pad_new_from_template(klv_src_template, "klv_src");
  gst_element_add_pad(GST_ELEMENT(inject), priv->klv_src);

  /* Store private data */
  g_type_set_qdata(
    G_OBJECT_TYPE(inject), g_quark_from_static_string("GstKlvFrameInjectPrivate"), priv);
}
