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
#include <math.h>
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

  const guint8 expected_ul[16] = {
    0x06,
    0x0E,
    0x2B,
    0x34,
    0x02,
    0x0B,
    0x01,
    0x01,
    0x0E,
    0x01,
    0x03,
    0x01,
    0x01,
    0x00,
    0x00,
    0x00,
  };
  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));
  fail_unless(map.size >= sizeof(expected_ul), "output too short");
  fail_unless(memcmp(map.data, expected_ul, sizeof(expected_ul)) == 0, "MISB ST 0601 UL incorrect");
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
 * @brief Verify numeric round-trip behaviour for the ST 0601.8 Full Corner
 * coordinate fields (Tags 82/83) and Full platform angle fields
 * (Tags 90-93), including boundary values, at a precision consistent
 * with their int32 mapping resolution (~42/84 nano degrees).
 */
GST_START_TEST(test_klvmetaenc_roundtrip_st0601_8_full_range_tags)
{
  GstHarness *enc = gst_harness_new("klvmetaenc");
  GstHarness *dec = gst_harness_new("klvmetadec");
  fail_unless(enc != NULL);
  fail_unless(dec != NULL);

  gst_harness_set_src_caps_str(enc, "application/json");
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  /* 82/84/86/88: Corner Latitude (Full), +/-90.
   * 83/85/87/89: Corner Longitude (Full), +/-180.
   * 90-93: Platform Pitch/Roll/AoA/Sideslip (Full), +/-90.
   * Includes representative and boundary (min/max) values, both signs. */
  const gchar *json = "{\"2\":1700000000000000,"
                      "\"82\":45.123456,\"83\":-120.654321,"
                      "\"84\":-90.0,\"85\":180.0,"
                      "\"86\":90.0,\"87\":-180.0,"
                      "\"90\":-90.0,\"91\":90.0,"
                      "\"92\":12.5,\"93\":-77.777}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json), strlen(json));
  GstFlowReturn ret = gst_harness_push(enc, in);
  fail_unless(ret == GST_FLOW_OK, "encoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *klv = gst_harness_pull(enc);
  fail_unless(klv != NULL, "encoder produced no KLV output");

  ret = gst_harness_push(dec, klv);
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(dec);
  fail_unless(out != NULL, "decoder produced no JSON output");

  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));

  KLVJsonTag tags[16] = {0};
  gint count = klv_json_parse_flat((const gchar *)map.data, map.size, tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 11);

  static const struct
  {
    gint id;
    gdouble expected;
  } cases[] = {
    {82, 45.123456},
    {83, -120.654321},
    {84, -90.0},
    {85, 180.0},
    {86, 90.0},
    {87, -180.0},
    {90, -90.0},
    {91, 90.0},
    {92, 12.5},
    {93, -77.777},
  };
  for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
    KLVJsonTag *t = find_tag(tags, count, cases[i].id);
    fail_unless(t != NULL, "missing Tag %d in decoded output", cases[i].id);
    fail_unless(!t->is_raw, "Tag %d should be numeric", cases[i].id);
    gdouble decoded = g_ascii_strtod(t->number_text, NULL);
    /* Tolerance well above the ~1e-7 deg (int32/+-90..180) mapping
     * resolution, but tight enough to catch a wrong range/scale. */
    fail_unless(fabs(decoded - cases[i].expected) < 1e-3,
                "Tag %d: expected %f, got %f",
                cases[i].id,
                cases[i].expected,
                decoded);
  }

  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
  gst_harness_teardown(enc);
  gst_harness_teardown(dec);
}
GST_END_TEST

/**
 * @brief Verify byte-exact round-trip behaviour for the opaque/raw ST
 * 0601.8 payload tags: Tag 81 (Image Horizon Pixel Pack), Tag 94 (MIIS
 * Core Identifier, ST 1204 Binary Value), and Tag 95 (SAR Motion Imagery
 * Metadata, nested ST 1206 Local Set). None of these are semantically
 * decoded by gstklvplugin; their payloads must survive encode/decode
 * unchanged.
 */
GST_START_TEST(test_klvmetaenc_roundtrip_opaque_payload_tags)
{
  GstHarness *enc = gst_harness_new("klvmetaenc");
  GstHarness *dec = gst_harness_new("klvmetadec");
  fail_unless(enc != NULL);
  fail_unless(dec != NULL);

  gst_harness_set_src_caps_str(enc, "application/json");
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  /* Tag 81: 2-point Image Horizon Pixel Pack (x0,y0,x1,y1), no geo-coords. */
  const gchar *json = "{\"2\":1700000000000000,"
                      "\"81\":\"hex:0a141e28\","
                      "\"94\":\"hex:0102030405060708090a0b0c0d0e0f10\","
                      "\"95\":\"hex:deadbeefcafef00d\"}";
  GstBuffer *in = gst_buffer_new_wrapped(g_strdup(json), strlen(json));
  GstFlowReturn ret = gst_harness_push(enc, in);
  fail_unless(ret == GST_FLOW_OK, "encoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *klv = gst_harness_pull(enc);
  fail_unless(klv != NULL, "encoder produced no KLV output");

  ret = gst_harness_push(dec, klv);
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(dec);
  fail_unless(out != NULL, "decoder produced no JSON output");

  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));

  /* Tags declared as `bytes` are decoded back as base64 JSON strings;
   * decode base64 ourselves and compare raw bytes for exactness. */
  gchar *body = g_strndup((const gchar *)map.data, map.size);

  const guint8 expected_81[] = {0x0a, 0x14, 0x1e, 0x28};
  const guint8 expected_94[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
  const guint8 expected_95[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xf0, 0x0d};

  struct
  {
    gint id;
    const guint8 *expected;
    gsize expected_len;
  } cases[] = {
    {81, expected_81, sizeof(expected_81)},
    {94, expected_94, sizeof(expected_94)},
    {95, expected_95, sizeof(expected_95)},
  };

  for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
    gchar needle[16];
    g_snprintf(needle, sizeof(needle), "\"%d\":\"base64:", cases[i].id);
    gchar *pos = strstr(body, needle);
    fail_unless(pos != NULL, "Tag %d missing or not base64-encoded in decoded output", cases[i].id);
    pos += strlen(needle);
    gchar *end = strchr(pos, '"');
    fail_unless(end != NULL);
    gchar *b64 = g_strndup(pos, (gsize)(end - pos));
    gsize decoded_len = 0;
    guint8 *decoded = g_base64_decode(b64, &decoded_len);
    g_free(b64);
    fail_unless(decoded_len == cases[i].expected_len,
                "Tag %d: expected %zu bytes, got %zu",
                cases[i].id,
                cases[i].expected_len,
                decoded_len);
    fail_unless(memcmp(decoded, cases[i].expected, decoded_len) == 0,
                "Tag %d: byte payload mismatch after round trip",
                cases[i].id);
    g_free(decoded);
  }

  g_free(body);
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
  tcase_add_test(tc, test_klvmetaenc_roundtrip_st0601_8_full_range_tags);
  tcase_add_test(tc, test_klvmetaenc_roundtrip_opaque_payload_tags);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(klvmetaenc)
