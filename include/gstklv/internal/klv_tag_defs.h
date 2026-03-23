/*
Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
*/

/**
 * @file gstklv/internal/klv_tag_defs.h
 * @brief Tag registry parsing and metadata structures.
 * @ingroup gstklv_internal_klv
 *
 * @section klv_tag_defs_api API Summary
 *
 * | Function | Purpose |
 * | --- | --- |
 * | `klv_load_tag_defs_from_ini` | Load tag definitions from INI |
 * | `klv_parse_range_string` | Parse `range` strings into min/max |
 * | `klv_tag_def_free` | Free tag metadata entries |
 *
 * @section klv_tag_defs_fields Tag Metadata
 *
 * `KLVTagDef` captures:
 *
 * - `name`, `type`, `units`
 * - `len`, `scale`
 * - `range_min`, `range_max`
 * - `is_signed`, `has_range`
 *
 * @section klv_tag_defs_flow Data Flow
 *
 * @dot
 * digraph klv_tag_defs_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *   ini [label="stanag4609_tags.ini"];
 *   defs [label="KLVTagDef table"];
 *   enc [label="klvmetaenc / klvframeinject"];
 *   dec [label="klvmetadec"];
 *   ini -> defs;
 *   defs -> enc;
 *   defs -> dec;
 * }
 * @enddot
 */
#ifndef GSTKLV_KLV_TAG_DEFS_H
#define GSTKLV_KLV_TAG_DEFS_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief Tag metadata definition parsed from the INI registry.
 */
  typedef struct
  {
    gint tag_id;        /**< Tag numeric ID. */
    gchar *name;        /**< Human-readable tag name. */
    gchar *type;        /**< Type string (uint16, int32, String, bytes). */
    gchar *unit;        /**< Unit name (if any). */
    gint len;           /**< Length in bytes. */
    gdouble scale;      /**< Scale factor (if numeric). */
    gdouble offset;     /**< Offset (if numeric). */
    gboolean is_signed; /**< TRUE for signed numeric types. */
    gboolean has_range; /**< TRUE when range is defined. */
    gdouble range_min;  /**< Range minimum. */
    gdouble range_max;  /**< Range maximum. */
  } KLVTagDef;

  /** Free a KLVTagDef instance. */
  void klv_tag_def_free(gpointer data);

  /** Parse a numeric string into a gdouble (fallback on failure). */
  gdouble klv_parse_numeric_value(const gchar *str, gdouble fallback);

  /** Parse a numeric token or power expression like 2^16. */
  gboolean klv_parse_range_value(const gchar *str, gdouble *out);

  /** Parse range strings like "-900..19000" or "+/- 20". */
  gboolean klv_parse_range_string(const gchar *str, gdouble *out_min, gdouble *out_max);

  /** Load tag definitions from an INI file. */
  GHashTable *klv_load_tag_defs_from_ini(const gchar *ini_file);

#ifdef __cplusplus
}
#endif

#endif /* GSTKLV_KLV_TAG_DEFS_H */
