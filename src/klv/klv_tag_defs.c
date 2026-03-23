/**
 * @file src/klv/klv_tag_defs.c
 * @brief INI registry parsing for MISB ST 0601 tag definitions.
 * @ingroup gstklv_internal_klv
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 */

#include "gstklv/internal/klv_tag_defs.h"

#include <gst/gst.h>
#include <math.h>
#include <string.h>

void
klv_tag_def_free(gpointer data)
{
  KLVTagDef *def = (KLVTagDef *)data;
  if (!def)
    return;
  g_free(def->name);
  g_free(def->type);
  g_free(def->unit);
  g_free(def);
}

gdouble
klv_parse_numeric_value(const gchar *str, gdouble fallback)
{
  if (!str)
    return fallback;

  gchar *endptr = NULL;
  gdouble val = g_strtod(str, &endptr);
  if (endptr == str || (endptr && *endptr != '\0')) {
    return fallback;
  }
  return val;
}

gboolean
klv_parse_range_value(const gchar *str, gdouble *out)
{
  if (!str || !out)
    return FALSE;

  gchar *tmp = g_strdup(str);
  g_strstrip(tmp);
  gsize len = strlen(tmp);
  if (len >= 2 && tmp[0] == '(' && tmp[len - 1] == ')') {
    tmp[len - 1] = '\0';
    memmove(tmp, tmp + 1, len - 1);
  }

  gchar *w = tmp;
  for (gchar *p = tmp; *p; p++) {
    if (*p != ' ')
      *w++ = *p;
  }
  *w = '\0';

  gchar *powpos = g_strstr_len(tmp, -1, "2^");
  if (powpos) {
    gchar *exp_start = powpos + 2;
    gchar *exp_end = exp_start;
    while (g_ascii_isdigit(*exp_end))
      exp_end++;
    if (exp_end == exp_start) {
      g_free(tmp);
      return FALSE;
    }
    gint exp = (gint)g_ascii_strtoll(exp_start, NULL, 10);
    gdouble value = pow(2.0, (gdouble)exp);
    gchar *rest = exp_end;
    if (*rest == '+' || *rest == '-') {
      gdouble delta = g_ascii_strtod(rest, NULL);
      value += delta;
    }
    else if (*rest != '\0') {
      g_free(tmp);
      return FALSE;
    }
    *out = value;
    g_free(tmp);
    return TRUE;
  }

  gchar *endptr = NULL;
  gdouble val = g_ascii_strtod(tmp, &endptr);
  if (endptr == tmp || (endptr && *endptr != '\0')) {
    g_free(tmp);
    return FALSE;
  }
  *out = val;
  g_free(tmp);
  return TRUE;
}

gboolean
klv_parse_range_string(const gchar *str, gdouble *out_min, gdouble *out_max)
{
  if (!str || !out_min || !out_max)
    return FALSE;

  gchar *tmp = g_strdup(str);
  g_strstrip(tmp);

  gchar *pm = g_strstr_len(tmp, -1, "+/-");
  if (pm) {
    gchar *valstr = pm + 3;
    g_strstrip(valstr);
    gdouble v = 0.0;
    if (!klv_parse_range_value(valstr, &v)) {
      g_free(tmp);
      return FALSE;
    }
    *out_min = -v;
    *out_max = v;
    g_free(tmp);
    return TRUE;
  }

  gchar *dots = g_strstr_len(tmp, -1, "..");
  if (dots) {
    *dots = '\0';
    gchar *minstr = tmp;
    gchar *maxstr = dots + 2;
    g_strstrip(minstr);
    g_strstrip(maxstr);
    gdouble vmin = 0.0, vmax = 0.0;
    if (!klv_parse_range_value(minstr, &vmin) || !klv_parse_range_value(maxstr, &vmax)) {
      g_free(tmp);
      return FALSE;
    }
    *out_min = vmin;
    *out_max = vmax;
    g_free(tmp);
    return TRUE;
  }

  g_free(tmp);
  return FALSE;
}

GHashTable *
klv_load_tag_defs_from_ini(const gchar *ini_file)
{
  GHashTable *tags = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, klv_tag_def_free);
  GKeyFile *kf = g_key_file_new();
  GError *err = NULL;

  gchar *contents = NULL;
  gsize len = 0;
  if (!g_file_get_contents(ini_file, &contents, &len, &err)) {
    GST_WARNING("Failed to load INI: %s", err ? err->message : "unknown error");
    if (err)
      g_error_free(err);
    g_key_file_free(kf);
    return tags;
  }

  gboolean at_line_start = TRUE;
  for (gsize i = 0; i < len; i++) {
    if (at_line_start) {
      if (contents[i] == ';')
        contents[i] = '#';
      if (contents[i] != ' ' && contents[i] != '\t' && contents[i] != '\r')
        at_line_start = FALSE;
    }
    if (contents[i] == '\n')
      at_line_start = TRUE;
  }

  if (!g_key_file_load_from_data(kf, contents, len, G_KEY_FILE_NONE, &err)) {
    GST_WARNING("Failed to parse INI: %s", err ? err->message : "unknown error");
    if (err)
      g_error_free(err);
    g_free(contents);
    g_key_file_free(kf);
    return tags;
  }
  g_free(contents);

  gchar **groups = g_key_file_get_groups(kf, NULL);
  for (int i = 0; groups && groups[i]; i++) {
    gchar *group = groups[i];

    if (!g_key_file_has_key(kf, group, "id", NULL))
      continue;

    KLVTagDef *td = g_new0(KLVTagDef, 1);
    td->tag_id = (gint)g_key_file_get_integer(kf, group, "id", NULL);
    td->name = g_key_file_get_string(kf, group, "name", NULL);
    td->type = g_key_file_get_string(kf, group, "type", NULL);
    td->unit = g_key_file_get_string(kf, group, "units", NULL);
    td->len = (gint)g_key_file_get_integer(kf, group, "length", NULL);

    gchar *scale_str = g_key_file_get_string(kf, group, "scale", NULL);
    td->scale = klv_parse_numeric_value(scale_str, 1.0);
    g_free(scale_str);

    gchar *offset_str = g_key_file_get_string(kf, group, "offset", NULL);
    td->offset = klv_parse_numeric_value(offset_str, 0.0);
    g_free(offset_str);

    if (td->type && (g_str_has_prefix(td->type, "int") || g_str_has_prefix(td->type, "Int")))
      td->is_signed = TRUE;
    else
      td->is_signed = FALSE;

    gchar *range_str = g_key_file_get_string(kf, group, "range", NULL);
    td->has_range = FALSE;
    if (range_str) {
      gdouble rmin = 0.0, rmax = 0.0;
      if (klv_parse_range_string(range_str, &rmin, &rmax)) {
        td->has_range = TRUE;
        td->range_min = rmin;
        td->range_max = rmax;
      }
    }
    g_free(range_str);

    g_hash_table_insert(tags, GINT_TO_POINTER(td->tag_id), td);
  }

  g_strfreev(groups);
  g_key_file_free(kf);
  return tags;
}
