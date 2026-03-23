/**
 * @file src/klv/klv_json.c
 * @brief JSON parsing and formatting helpers for KLV tags.
 * @ingroup gstklv_internal_klv
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/klv_json.h"

#include <gst/gst.h>
#include <string.h>
#include <stdio.h>

gboolean
klv_parse_hex_string(const gchar *str, guint8 **out, gsize *out_len)
{
  gsize len = strlen(str);
  if (len % 2 != 0)
    return FALSE;

  gsize bytes = len / 2;
  guint8 *buf = (guint8 *)g_malloc(bytes);
  for (gsize i = 0; i < bytes; i++) {
    gint hi = g_ascii_xdigit_value(str[i * 2]);
    gint lo = g_ascii_xdigit_value(str[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      g_free(buf);
      return FALSE;
    }
    buf[i] = (guint8)((hi << 4) | lo);
  }

  *out = buf;
  *out_len = bytes;
  return TRUE;
}

gboolean
klv_parse_string_value(const gchar *str, guint8 **out, gsize *out_len)
{
  if (g_str_has_prefix(str, "hex:")) {
    return klv_parse_hex_string(str + 4, out, out_len);
  }
  if (g_str_has_prefix(str, "base64:")) {
    gsize decoded_len = 0;
    guint8 *decoded = g_base64_decode(str + 7, &decoded_len);
    if (!decoded)
      return FALSE;
    *out = decoded;
    *out_len = decoded_len;
    return TRUE;
  }
  if (g_str_has_prefix(str, "ascii:")) {
    const gchar *payload = str + 6;
    gsize len = strlen(payload);
    guint8 *buf = (guint8 *)g_malloc(len);
    memcpy(buf, payload, len);
    *out = buf;
    *out_len = len;
    return TRUE;
  }

  gsize len = strlen(str);
  guint8 *buf = (guint8 *)g_malloc(len);
  memcpy(buf, str, len);
  *out = buf;
  *out_len = len;
  return TRUE;
}

gint
klv_json_parse_flat(const gchar *json_str, gsize json_len, KLVJsonTag *tags, gint max_tags)
{
  gint tag_count = 0;
  const gchar *p = json_str;
  const gchar *end = json_str + json_len;

  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '{'))
    p++;

  while (p < end && tag_count < max_tags) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;

    if (p >= end || *p == '}')
      break;

    if (*p != '"') {
      GST_ERROR("Expected '\"' at position %ld", p - json_str);
      return -1;
    }
    p++;

    gchar tag_str[32];
    gint tag_idx = 0;
    while (p < end && *p >= '0' && *p <= '9' && tag_idx < 31) {
      tag_str[tag_idx++] = *p;
      p++;
    }
    tag_str[tag_idx] = '\0';

    if (tag_idx == 0) {
      GST_ERROR("Invalid tag ID");
      return -1;
    }

    gint tag_id = atoi(tag_str);

    if (p >= end || *p != '"') {
      GST_ERROR("Expected closing '\"' for tag");
      return -1;
    }
    p++;

    while (p < end && (*p == ' ' || *p == '\t'))
      p++;
    if (p >= end || *p != ':') {
      GST_ERROR("Expected ':'");
      return -1;
    }
    p++;

    while (p < end && (*p == ' ' || *p == '\t'))
      p++;

    tags[tag_count].tag_id = tag_id;
    tags[tag_count].number_text = NULL;
    tags[tag_count].is_raw = FALSE;
    tags[tag_count].raw = NULL;
    tags[tag_count].raw_len = 0;

    if (p < end && *p == '"') {
      p++;
      const gchar *s = p;
      /* Find the true closing quote, skipping over escape sequences */
      while (p < end && *p != '"') {
        if (*p == '\\' && (p + 1) < end)
          p++; /* skip the escaped character */
        p++;
      }
      if (p >= end) {
        GST_ERROR("Unterminated string value");
        return -1;
      }
      gsize slen = p - s;
      /* Unescape JSON escape sequences while copying */
      gchar *tmp = (gchar *)g_malloc(slen + 1);
      gsize out_idx = 0;
      for (gsize i = 0; i < slen; i++) {
        if (s[i] == '\\' && i + 1 < slen) {
          i++;
          switch (s[i]) {
            case '"': tmp[out_idx++] = '"'; break;
            case '\\': tmp[out_idx++] = '\\'; break;
            case '/': tmp[out_idx++] = '/'; break;
            case 'n': tmp[out_idx++] = '\n'; break;
            case 'r': tmp[out_idx++] = '\r'; break;
            case 't': tmp[out_idx++] = '\t'; break;
            case 'b': tmp[out_idx++] = '\b'; break;
            case 'f': tmp[out_idx++] = '\f'; break;
            default: tmp[out_idx++] = s[i]; break;
          }
        }
        else {
          tmp[out_idx++] = s[i];
        }
      }
      tmp[out_idx] = '\0';

      guint8 *raw = NULL;
      gsize raw_len = 0;
      if (!klv_parse_string_value(tmp, &raw, &raw_len)) {
        GST_ERROR("Failed to parse string value for tag %d", tag_id);
        g_free(tmp);
        return -1;
      }
      g_free(tmp);
      tags[tag_count].is_raw = TRUE;
      tags[tag_count].raw = raw;
      tags[tag_count].raw_len = raw_len;

      p++;
    }
    else {
      gchar num_str[64];
      gint num_idx = 0;
      while (p < end && num_idx < 63 &&
             ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == '+' || *p == 'e' ||
              *p == 'E')) {
        num_str[num_idx++] = *p;
        p++;
      }
      num_str[num_idx] = '\0';
      tags[tag_count].number_text = g_strdup(num_str);
      gdouble val = g_strtod(num_str, NULL);
      tags[tag_count].value = val;
    }

    tag_count++;

    while (p < end && *p != ',' && *p != '}')
      p++;
    if (*p == ',')
      p++;
  }

  return tag_count;
}

void
klv_json_append_escaped(GString *out, const gchar *str, gsize len)
{
  gchar *tmp = g_strndup(str, len);
  gchar *escaped = g_strescape(tmp, NULL);
  if (escaped) {
    g_string_append(out, escaped);
    g_free(escaped);
  }
  g_free(tmp);
}

gchar *
klv_json_format_number_gint64(gint64 value)
{
  gchar buf[64];
  g_snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return g_strdup(buf);
}

gchar *
klv_json_format_number_guint64(guint64 value)
{
  gchar buf[64];
  g_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return g_strdup(buf);
}

gchar *
klv_json_format_number_double(gdouble value)
{
  gchar buf[128];
  g_snprintf(buf, sizeof(buf), "%.15g", value);
  return g_strdup(buf);
}
