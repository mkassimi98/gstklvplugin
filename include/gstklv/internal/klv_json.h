/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_json.h
 * @brief Flat JSON parsing and formatting helpers.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_json_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `klv_json_parse_flat` | Parse flat JSON into numeric tags |
 * | `klv_parse_hex_string` | Decode hex strings into bytes |
 * | `klv_parse_string_value` | Parse `hex:` or `base64:` values |
 * | `klv_json_format_number_*` | Format numeric values for JSON |
 * | `klv_json_append_escaped` | Append escaped JSON strings |
 *
 * @section klv_json_format JSON Format
 *
 * - Keys are numeric strings.
 * - Values can be numeric or string.
 * - Raw bytes should use `hex:` or `base64:` prefixes.
 */
#ifndef GSTKLV_KLV_JSON_H
#define GSTKLV_KLV_JSON_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Parsed JSON tag entry.
 */
  typedef struct
  {
    gint tag_id;        /**< Tag numeric ID. */
    gdouble value;      /**< Numeric value (if not raw). */
    gchar *number_text; /**< Original numeric text, preserved for exact integer printing. */
    gboolean is_raw;    /**< TRUE when raw bytes are provided. */
    guint8 *raw;        /**< Raw bytes buffer. */
    gsize raw_len;      /**< Raw buffer length. */
  } KLVJsonTag;

  /**
 * @brief Parse a hex string (no prefix) into bytes.
 */
  gboolean klv_parse_hex_string(const gchar *str, guint8 **out, gsize *out_len);

  /**
 * @brief Parse a string value with optional `hex:` or `base64:` prefixes.
 */
  gboolean klv_parse_string_value(const gchar *str, guint8 **out, gsize *out_len);

  /**
 * @brief Parse a flat JSON object into tag entries.
 * @return Number of parsed tags or -1 on error.
 */
  gint klv_json_parse_flat(const gchar *json_str, gsize json_len, KLVJsonTag *tags, gint max_tags);

  /**
 * @brief Append an escaped string into a GString.
 */
  void klv_json_append_escaped(GString *out, const gchar *str, gsize len);

  /** Format helpers used by the decoder. */
  gchar *klv_json_format_number_gint64(gint64 value);
  gchar *klv_json_format_number_guint64(guint64 value);
  gchar *klv_json_format_number_double(gdouble value);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_JSON_H */
