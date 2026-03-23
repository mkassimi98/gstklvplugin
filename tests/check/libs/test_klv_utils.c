/**
 * @file tests/check/libs/test_klv_utils.c
 * @brief gst-check coverage for internal KLV helper utilities.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the deterministic helper layer used by the plugin:
 *
 * - BER length encoding and decoding
 * - checksum helpers
 * - scaling helpers
 * - INI tag registry parsing
 * - flat JSON parsing and formatting
 */

#include <gst/check/gstcheck.h>
#include <math.h>
#include <string.h>

#include "gstklv/internal/klv_ber.h"
#include "gstklv/internal/klv_checksum.h"
#include "gstklv/internal/klv_json.h"
#include "gstklv/internal/klv_scaling.h"
#include "gstklv/internal/klv_tag_defs.h"

/**
 * @brief Release heap-owned fields created by `klv_json_parse_flat()`.
 * @param tags Parsed tag array.
 * @param count Number of valid entries in @p tags.
 */
static void
free_json_tags(KLVJsonTag *tags, gint count)
{
  for (gint i = 0; i < count; i++) {
    g_free(tags[i].raw);
    g_free(tags[i].number_text);
  }
}

/**
 * @brief Locate a parsed JSON tag by numeric identifier.
 * @param tags Parsed tag array.
 * @param count Number of valid entries in @p tags.
 * @param tag_id Numeric tag identifier to locate.
 * @return Matching tag or `NULL`.
 */
static KLVJsonTag *
find_tag(KLVJsonTag *tags, gint count, gint tag_id)
{
  for (gint i = 0; i < count; i++) {
    if (tags[i].tag_id == tag_id)
      return &tags[i];
  }
  return NULL;
}

/**
 * @brief Verify short-form BER length encoding and decoding.
 */
GST_START_TEST(test_klv_ber_encode_decode_short)
{
  guint8 buf[8] = {0};
  gint n = klv_ber_encode_length(buf, 127);
  fail_unless_equals_int(n, 1);
  fail_unless_equals_int(buf[0], 127);

  gsize pos = 0;
  gsize len = 0;
  fail_unless(klv_ber_decode_length(buf, n, &pos, &len));
  fail_unless_equals_int((gint)len, 127);
  fail_unless_equals_int((gint)pos, 1);
}
GST_END_TEST

/**
 * @brief Verify long-form BER length encoding and decoding.
 */
GST_START_TEST(test_klv_ber_encode_decode_long)
{
  guint8 buf[8] = {0};
  gint n = klv_ber_encode_length(buf, 300);
  fail_unless_equals_int(n, 3);
  fail_unless_equals_int(buf[0], 0x82);
  fail_unless_equals_int(buf[1], 0x01);
  fail_unless_equals_int(buf[2], 0x2C);

  gsize pos = 0;
  gsize len = 0;
  fail_unless(klv_ber_decode_length(buf, n, &pos, &len));
  fail_unless_equals_int((gint)len, 300);
  fail_unless_equals_int((gint)pos, 3);
}
GST_END_TEST

/**
 * @brief Verify sign extension and range-based scaling helpers.
 */
GST_START_TEST(test_klv_scaling_helpers)
{
  fail_unless_equals_int(klv_sign_extend(0x7F, 8), 127);
  fail_unless_equals_int(klv_sign_extend(0x80, 8), -128);
  fail_unless(fabs(klv_decode_with_range(0, -90.0, 90.0, FALSE, 16) + 90.0) < 1e-6);
  fail_unless(fabs(klv_decode_with_range(65535, -90.0, 90.0, FALSE, 16) - 90.0) < 1e-6);
}
GST_END_TEST

/**
 * @brief Verify the BCC-16 checksum helper against a known vector.
 */
GST_START_TEST(test_klv_checksum_bcc16)
{
  const guint8 data[] = {0x00, 0x01, 0x00, 0x02};
  fail_unless_equals_int(klv_bcc_16(data, sizeof(data)), 0x0003);
}
GST_END_TEST

/**
 * @brief Verify that the ST 0601 tag registry is loaded from the INI file.
 */
GST_START_TEST(test_klv_tag_defs_load_ini)
{
  gchar *ini = g_build_filename(GSTKLVPLUGIN_SOURCE_DIR, "data", "stanag4609_tags.ini", NULL);
  GHashTable *defs = klv_load_tag_defs_from_ini(ini);
  g_free(ini);

  fail_unless(defs != NULL);

  KLVTagDef *tag13 = g_hash_table_lookup(defs, GINT_TO_POINTER(13));
  fail_unless(tag13 != NULL, "Tag 13 definition missing");
  fail_unless(g_strcmp0(tag13->name, "Sensor Latitude") == 0,
              "unexpected Tag 13 name: %s",
              GST_STR_NULL(tag13->name));
  fail_unless(tag13->has_range, "Tag 13 should have range information");
  fail_unless(fabs(tag13->range_min + 90.0) < 1e-6);
  fail_unless(fabs(tag13->range_max - 90.0) < 1e-6);

  g_hash_table_destroy(defs);
}
GST_END_TEST

/**
 * @brief Verify parsing of representative range expressions from the INI file.
 */
GST_START_TEST(test_klv_tag_defs_parse_ranges)
{
  gdouble min = 0.0;
  gdouble max = 0.0;

  fail_unless(klv_parse_range_string("+/- 20", &min, &max));
  fail_unless(fabs(min + 20.0) < 1e-6);
  fail_unless(fabs(max - 20.0) < 1e-6);

  fail_unless(klv_parse_range_string("-900..19000", &min, &max));
  fail_unless(fabs(min + 900.0) < 1e-6);
  fail_unless(fabs(max - 19000.0) < 1e-6);

  fail_unless(klv_parse_range_value("2^16-1", &max));
  fail_unless(fabs(max - 65535.0) < 1e-6);
}
GST_END_TEST

/**
 * @brief Verify flat JSON parsing for numeric and raw byte tags.
 */
GST_START_TEST(test_klv_json_parse_flat_numeric_and_raw)
{
  const gchar *json = "{ \"2\": 123, \"48\": \"hex:0102\", \"60\": 17, \"73\": \"base64:AQID\" }";
  KLVJsonTag tags[8] = {0};
  gint count = klv_json_parse_flat(json, strlen(json), tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 4);

  KLVJsonTag *tag2 = find_tag(tags, count, 2);
  KLVJsonTag *tag48 = find_tag(tags, count, 48);
  KLVJsonTag *tag60 = find_tag(tags, count, 60);
  KLVJsonTag *tag73 = find_tag(tags, count, 73);

  fail_unless(tag2 != NULL && !tag2->is_raw);
  fail_unless(tag2->number_text != NULL);
  fail_unless(g_strcmp0(tag2->number_text, "123") == 0);

  fail_unless(tag48 != NULL && tag48->is_raw);
  fail_unless_equals_int((gint)tag48->raw_len, 2);
  fail_unless(tag48->raw[0] == 0x01 && tag48->raw[1] == 0x02);

  fail_unless(tag60 != NULL && !tag60->is_raw);
  fail_unless(tag60->number_text != NULL);
  fail_unless(g_strcmp0(tag60->number_text, "17") == 0);

  fail_unless(tag73 != NULL && tag73->is_raw);
  fail_unless_equals_int((gint)tag73->raw_len, 3);
  fail_unless(tag73->raw[0] == 0x01 && tag73->raw[1] == 0x02 && tag73->raw[2] == 0x03);

  free_json_tags(tags, count);
}
GST_END_TEST

/**
 * @brief Verify exact preservation of large integer text in parsed JSON tags.
 */
GST_START_TEST(test_klv_json_preserves_large_integer_text)
{
  const gchar *json = "{ \"2\": 1774033378153350 }";
  KLVJsonTag tags[2] = {0};
  gint count = klv_json_parse_flat(json, strlen(json), tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 1);

  fail_unless(tags[0].number_text != NULL);
  fail_unless(g_strcmp0(tags[0].number_text, "1774033378153350") == 0);
  fail_unless(tags[0].value > 1.0e15);

  free_json_tags(tags, count);
}
GST_END_TEST

/**
 * @brief Verify rejection of malformed flat JSON payloads.
 */
GST_START_TEST(test_klv_json_rejects_invalid_json)
{
  const gchar *json = "{ 2: 123 }";
  KLVJsonTag tags[4] = {0};
  fail_unless_equals_int(klv_json_parse_flat(json, strlen(json), tags, G_N_ELEMENTS(tags)), -1);
}
GST_END_TEST

/**
 * @brief Verify numeric formatting helpers used by the decoder.
 */
GST_START_TEST(test_klv_json_formatters)
{
  gchar *s1 = klv_json_format_number_gint64(-42);
  gchar *s2 = klv_json_format_number_guint64(42);
  gchar *s3 = klv_json_format_number_double(1.2345);

  fail_unless(s1 != NULL);
  fail_unless(s2 != NULL);
  fail_unless(s3 != NULL);
  fail_unless(g_strcmp0(s1, "-42") == 0);
  fail_unless(g_strcmp0(s2, "42") == 0);
  fail_unless(fabs(g_ascii_strtod(s3, NULL) - 1.2345) < 1e-6);

  g_free(s1);
  g_free(s2);
  g_free(s3);
}
GST_END_TEST

/**
 * @brief Build the `klv-utils` suite definition.
 * @return Populated Check suite.
 */
static Suite *
klv_utils_suite(void)
{
  Suite *s = suite_create("klv-utils");
  TCase *tc = tcase_create("general");

  tcase_add_test(tc, test_klv_ber_encode_decode_short);
  tcase_add_test(tc, test_klv_ber_encode_decode_long);
  tcase_add_test(tc, test_klv_scaling_helpers);
  tcase_add_test(tc, test_klv_checksum_bcc16);
  tcase_add_test(tc, test_klv_tag_defs_load_ini);
  tcase_add_test(tc, test_klv_tag_defs_parse_ranges);
  tcase_add_test(tc, test_klv_json_parse_flat_numeric_and_raw);
  tcase_add_test(tc, test_klv_json_preserves_large_integer_text);
  tcase_add_test(tc, test_klv_json_rejects_invalid_json);
  tcase_add_test(tc, test_klv_json_formatters);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(klv_utils)
