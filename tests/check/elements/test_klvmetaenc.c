/**
 * @file tests/check/elements/test_klvmetaenc.c
 * @brief gst-check coverage for the klvmetaenc element.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the public contract of `klvmetaenc`:
 *
 * - factory creation and pad exposure
 * - MISB ST 0601 Universal Label emission
 * - checksum tag generation
 * - round-trip behaviour for large integers and raw byte tags
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
 * @return Pointer to the matching tag or `NULL` when not present.
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
 * @brief Verify that the element factory instantiates `klvmetaenc`.
 */
GST_START_TEST(test_klvmetaenc_create)
{
  GstElement *enc = gst_element_factory_make("klvmetaenc", NULL);
  fail_unless(enc != NULL, "klvmetaenc element could not be created");
  gst_object_unref(enc);
}
GST_END_TEST

/**
 * @brief Verify that the encoder exposes the expected static pads.
 */
GST_START_TEST(test_klvmetaenc_pads)
{
  GstElement *enc = gst_element_factory_make("klvmetaenc", NULL);
  fail_unless(enc != NULL);

  GstPad *sink = gst_element_get_static_pad(enc, "sink");
  GstPad *src = gst_element_get_static_pad(enc, "src");
  fail_unless(sink != NULL, "klvmetaenc has no sink pad");
  fail_unless(src != NULL, "klvmetaenc has no src pad");

  gst_object_unref(sink);
  gst_object_unref(src);
  gst_object_unref(enc);
}
GST_END_TEST

/**
 * @brief Verify that encoded output starts with the MISB ST 0601 UL.
 */
GST_START_TEST(test_klvmetaenc_ul_header)
{
  GstHarness *h = gst_harness_new("klvmetaenc");
  fail_unless(h != NULL);

  gst_harness_set_src_caps_str(h, "application/json");

  /* JSON with Tag 2 (Unix Timestamp) */
  const gchar *json = "{\"2\": 1700000000000000}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json), strlen(json));
  GST_BUFFER_PTS(in) = 0;

  GstFlowReturn ret = gst_harness_push(h, in);
  fail_unless(ret == GST_FLOW_OK, "push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(h);
  fail_unless(out != NULL, "no output buffer");

  /* Check MISB ST 0601 UL: 06 0E 2B 34 02 0B 01 01 0E 01 03 01 00 00 00 00 */
  GstMapInfo map;
  gst_buffer_map(out, &map, GST_MAP_READ);
  fail_unless(map.size >= 16, "output too short");
  fail_unless(map.data[0] == 0x06 && map.data[1] == 0x0E && map.data[2] == 0x2B &&
                map.data[3] == 0x34,
              "MISB ST 0601 UL prefix incorrect");
  gst_buffer_unmap(out, &map);

  gst_buffer_unref(out);
  gst_harness_teardown(h);
}
GST_END_TEST

/**
 * @brief Verify that the encoder appends Tag 1 as the trailing checksum TLV.
 */
GST_START_TEST(test_klvmetaenc_checksum_tag)
{
  GstHarness *h = gst_harness_new("klvmetaenc");
  fail_unless(h != NULL);
  gst_harness_set_src_caps_str(h, "application/json");

  const gchar *json = "{\"2\": 1700000000000000}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json), strlen(json));
  gst_harness_push(h, in);

  GstBuffer *out = gst_harness_pull(h);
  fail_unless(out != NULL);

  GstMapInfo map;
  gst_buffer_map(out, &map, GST_MAP_READ);

  /* Last 4 bytes should be: 0x01 0x02 <cs_hi> <cs_lo> */
  fail_unless(map.size >= 4, "output too short for checksum tag");
  fail_unless(map.data[map.size - 4] == 0x01, "Tag 1 ID missing at end");
  fail_unless(map.data[map.size - 3] == 0x02, "Tag 1 length != 2");

  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
  gst_harness_teardown(h);
}
GST_END_TEST

/**
 * @brief Verify round-trip behaviour for large integers and raw byte tags.
 */
GST_START_TEST(test_klvmetaenc_roundtrip_complex_payload)
{
  GstHarness *enc = gst_harness_new("klvmetaenc");
  GstHarness *dec = gst_harness_new("klvmetadec");
  fail_unless(enc != NULL);
  fail_unless(dec != NULL);

  gst_harness_set_src_caps_str(enc, "application/json");
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  const gchar *json = "{\"2\":1774033378153350,\"48\":\"hex:0102\",\"73\":\"base64:AQID\"}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json), strlen(json));
  GST_BUFFER_PTS(in) = 5 * GST_SECOND;
  GST_BUFFER_DTS(in) = 4 * GST_SECOND;

  GstFlowReturn ret = gst_harness_push(enc, in);
  fail_unless(ret == GST_FLOW_OK, "encoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *klv = gst_harness_pull(enc);
  fail_unless(klv != NULL, "encoder produced no KLV output");

  ret = gst_harness_push(dec, klv);
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(dec);
  fail_unless(out != NULL, "decoder produced no JSON output");
  fail_unless(GST_BUFFER_PTS(out) == 5 * GST_SECOND, "unexpected PTS on decoded buffer");
  fail_unless(GST_BUFFER_DTS(out) == 4 * GST_SECOND, "unexpected DTS on decoded buffer");

  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));

  KLVJsonTag tags[8] = {0};
  gint count = klv_json_parse_flat((const gchar *)map.data, map.size, tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 3);

  KLVJsonTag *tag2 = find_tag(tags, count, 2);
  fail_unless(tag2 != NULL, "missing Tag 2");
  fail_unless(tag2->number_text != NULL);
  fail_unless(g_strcmp0(tag2->number_text, "1774033378153350") == 0,
              "unexpected Tag 2 text: %s",
              GST_STR_NULL(tag2->number_text));

  KLVJsonTag *tag48 = find_tag(tags, count, 48);
  fail_unless(tag48 != NULL && tag48->is_raw, "missing raw Tag 48");
  fail_unless_equals_int((gint)tag48->raw_len, 2);
  fail_unless(tag48->raw[0] == 0x01 && tag48->raw[1] == 0x02, "unexpected Tag 48 payload");

  KLVJsonTag *tag73 = find_tag(tags, count, 73);
  fail_unless(tag73 != NULL && tag73->is_raw, "missing raw Tag 73");
  fail_unless_equals_int((gint)tag73->raw_len, 3);
  fail_unless(tag73->raw[0] == 0x01 && tag73->raw[1] == 0x02 && tag73->raw[2] == 0x03,
              "unexpected Tag 73 payload");

  fail_unless(find_tag(tags, count, 1) == NULL, "checksum tag must not be exposed in JSON");

  free_json_tags(tags, count);
  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
  gst_harness_teardown(enc);
  gst_harness_teardown(dec);
}
GST_END_TEST

/**
 * @brief Build the `klvmetaenc` suite definition.
 * @return Populated Check suite.
 */
static Suite *
klvmetaenc_suite(void)
{
  Suite *s = suite_create("klvmetaenc");
  TCase *tc = tcase_create("general");

  tcase_add_test(tc, test_klvmetaenc_create);
  tcase_add_test(tc, test_klvmetaenc_pads);
  tcase_add_test(tc, test_klvmetaenc_ul_header);
  tcase_add_test(tc, test_klvmetaenc_checksum_tag);
  tcase_add_test(tc, test_klvmetaenc_roundtrip_complex_payload);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(klvmetaenc)
