/**
 * @file tests/check/elements/test_klvmetadec.c
 * @brief gst-check coverage for the klvmetadec element.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the public contract of `klvmetadec`:
 *
 * - factory creation and static pads
 * - JSON reconstruction from encoded KLV buffers
 * - preservation of `PTS` / `DTS`
 * - recovery of raw byte tags and large integer text
 */

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <string.h>

#include "gstklv/internal/klv_json.h"

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
 * @return Pointer to the matching tag or `NULL`.
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
 * @brief Verify that the element factory instantiates `klvmetadec`.
 */
GST_START_TEST(test_klvmetadec_create)
{
  GstElement *dec = gst_element_factory_make("klvmetadec", NULL);
  fail_unless(dec != NULL, "klvmetadec element could not be created");
  gst_object_unref(dec);
}
GST_END_TEST

/**
 * @brief Verify that the decoder exposes the expected static pads.
 */
GST_START_TEST(test_klvmetadec_pads)
{
  GstElement *dec = gst_element_factory_make("klvmetadec", NULL);
  fail_unless(dec != NULL);
  GstPad *sink = gst_element_get_static_pad(dec, "sink");
  GstPad *src = gst_element_get_static_pad(dec, "src");
  fail_unless(sink != NULL);
  fail_unless(src != NULL);
  gst_object_unref(sink);
  gst_object_unref(src);
  gst_object_unref(dec);
}
GST_END_TEST

/**
 * @brief Verify that a simple encode/decode round-trip preserves Tag 2.
 */
GST_START_TEST(test_klvmetadec_roundtrip_timestamp)
{
  /* Encode JSON -> KLV */
  GstHarness *enc = gst_harness_new("klvmetaenc");
  fail_unless(enc != NULL);
  gst_harness_set_src_caps_str(enc, "application/json");

  const gchar *json_in = "{\"2\": 1700000000000000}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json_in), strlen(json_in));
  GST_BUFFER_PTS(in) = 0;
  GstFlowReturn ret = gst_harness_push(enc, in);
  fail_unless(ret == GST_FLOW_OK, "encoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *klv = gst_harness_pull(enc);
  fail_unless(klv != NULL, "encoder produced no output");

  /* Decode KLV -> JSON */
  GstHarness *dec = gst_harness_new("klvmetadec");
  fail_unless(dec != NULL);
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  ret = gst_harness_push(dec, klv);
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(dec);
  fail_unless(out != NULL, "decoder produced no output");

  GstMapInfo map;
  gst_buffer_map(out, &map, GST_MAP_READ);
  gchar *json_out = g_strndup((const gchar *)map.data, map.size);
  fail_unless(strstr(json_out, "\"2\"") != NULL, "Tag 2 missing from decoded JSON: %s", json_out);
  g_free(json_out);
  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);

  gst_harness_teardown(enc);
  gst_harness_teardown(dec);
}
GST_END_TEST

/**
 * @brief Verify raw-tag recovery and `PTS` / `DTS` preservation.
 */
GST_START_TEST(test_klvmetadec_preserves_timestamps_and_raw_bytes)
{
  GstHarness *enc = gst_harness_new("klvmetaenc");
  GstHarness *dec = gst_harness_new("klvmetadec");
  fail_unless(enc != NULL);
  fail_unless(dec != NULL);

  gst_harness_set_src_caps_str(enc, "application/json");
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  const gchar *json_in = "{\"2\":1774033378153350,\"48\":\"hex:0102\"}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json_in), strlen(json_in));
  GST_BUFFER_PTS(in) = 11 * GST_SECOND;
  GST_BUFFER_DTS(in) = 10 * GST_SECOND;

  GstFlowReturn ret = gst_harness_push(enc, in);
  fail_unless(ret == GST_FLOW_OK, "encoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *klv = gst_harness_pull(enc);
  fail_unless(klv != NULL);
  fail_unless(GST_BUFFER_PTS(klv) == 11 * GST_SECOND);
  fail_unless(GST_BUFFER_DTS(klv) == 10 * GST_SECOND);

  ret = gst_harness_push(dec, klv);
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(dec);
  fail_unless(out != NULL);
  fail_unless(GST_BUFFER_PTS(out) == 11 * GST_SECOND);
  fail_unless(GST_BUFFER_DTS(out) == 10 * GST_SECOND);

  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));

  KLVJsonTag tags[4] = {0};
  gint count = klv_json_parse_flat((const gchar *)map.data, map.size, tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 2);

  KLVJsonTag *tag2 = find_tag(tags, count, 2);
  fail_unless(tag2 != NULL && tag2->number_text != NULL, "missing Tag 2");
  fail_unless(g_strcmp0(tag2->number_text, "1774033378153350") == 0,
              "unexpected Tag 2 text: %s",
              GST_STR_NULL(tag2->number_text));

  KLVJsonTag *tag48 = find_tag(tags, count, 48);
  fail_unless(tag48 != NULL && tag48->is_raw, "missing raw Tag 48");
  fail_unless_equals_int((gint)tag48->raw_len, 2);
  fail_unless(tag48->raw[0] == 0x01 && tag48->raw[1] == 0x02, "unexpected Tag 48 payload");

  free_json_tags(tags, count);
  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
  gst_harness_teardown(enc);
  gst_harness_teardown(dec);
}
GST_END_TEST

/**
 * @brief Build the `klvmetadec` suite definition.
 * @return Populated Check suite.
 */
static Suite *
klvmetadec_suite(void)
{
  Suite *s = suite_create("klvmetadec");
  TCase *tc = tcase_create("general");
  tcase_add_test(tc, test_klvmetadec_create);
  tcase_add_test(tc, test_klvmetadec_pads);
  tcase_add_test(tc, test_klvmetadec_roundtrip_timestamp);
  tcase_add_test(tc, test_klvmetadec_preserves_timestamps_and_raw_bytes);
  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(klvmetadec)
