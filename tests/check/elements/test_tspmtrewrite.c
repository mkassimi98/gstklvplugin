/**
 * @file tests/check/elements/test_tspmtrewrite.c
 * @brief gst-check coverage for the tspmtrewrite element.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the public contract of `tspmtrewrite`:
 *
 * - factory creation and pad exposure
 * - default metadata descriptor properties
 * - PMT rewriting from metadata `stream_type 0x15` to `0x06 + KLVA`
 * - custom metadata descriptor serialization
 */

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <string.h>

#include "gstklv/internal/ts_crc32.h"
#include "gstklv/internal/ts_psi.h"

/**
 * @brief Append one 188-byte TS packet to a byte array.
 * @param dst Destination byte array.
 * @param packet TS packet bytes to append.
 */
static void
append_packet(GByteArray *dst, const guint8 *packet)
{
  g_byte_array_append(dst, packet, 188);
}

/**
 * @brief Build a minimal PAT packet pointing to the PMT PID under test.
 * @param pmt_pid PMT PID to advertise in the PAT.
 * @param packet Output 188-byte TS packet buffer.
 */
static void
build_pat_packet(guint16 pmt_pid, guint8 packet[188])
{
  guint8 pat[16] = {
    0x00,
    0xB0,
    0x0D,
    0x00,
    0x01,
    0xC1,
    0x00,
    0x00,
    0x00,
    0x01,
    (guint8)(0xE0 | ((pmt_pid >> 8) & 0x1F)),
    (guint8)(pmt_pid & 0xFF),
    0x00,
    0x00,
    0x00,
    0x00,
  };
  guint32 crc = ts_crc32_mpeg2(pat, 12);
  pat[12] = (crc >> 24) & 0xFF;
  pat[13] = (crc >> 16) & 0xFF;
  pat[14] = (crc >> 8) & 0xFF;
  pat[15] = crc & 0xFF;

  memset(packet, 0xFF, 188);
  packet[0] = 0x47;
  packet[1] = 0x40;
  packet[2] = 0x00;
  packet[3] = 0x10;
  packet[4] = 0x00;
  memcpy(packet + 5, pat, sizeof(pat));
}

/**
 * @brief Build a one-program PMT section with one video and one KLVA stream.
 * @return Newly allocated PMT section byte array.
 */
static GByteArray *
build_test_pmt_section(void)
{
  TsPmtInfo pmt;
  ts_pmt_info_init(&pmt);
  pmt.program_number = 1;
  pmt.version_byte = 0xC1;
  pmt.section_number = 0;
  pmt.last_section_number = 0;
  pmt.pcr_pid = 0x0100;

  TsStreamInfo *video = g_new0(TsStreamInfo, 1);
  video->stream_type = 0x1B;
  video->pid = 0x0041;
  video->descriptors = g_ptr_array_new_with_free_func(ts_descriptor_free);
  g_ptr_array_add(pmt.streams, video);

  TsStreamInfo *klv = g_new0(TsStreamInfo, 1);
  klv->stream_type = 0x15;
  klv->pid = 0x0042;
  klv->descriptors = g_ptr_array_new_with_free_func(ts_descriptor_free);

  TsDescriptor *registration = g_new0(TsDescriptor, 1);
  registration->tag = 0x05;
  registration->payload = g_byte_array_new();
  g_byte_array_append(registration->payload, (const guint8 *)"KLVA", 4);
  g_ptr_array_add(klv->descriptors, registration);
  g_ptr_array_add(pmt.streams, klv);

  GByteArray *section = ts_build_pmt_section(&pmt);
  ts_pmt_info_clear(&pmt);
  return section;
}

/**
 * @brief Wrap a PSI section into a single TS packet.
 * @param pid Packet PID.
 * @param section PSI section payload.
 * @param section_len Number of bytes in @p section.
 * @param packet Output 188-byte TS packet buffer.
 */
static void
build_section_packet(guint16 pid, const guint8 *section, gsize section_len, guint8 packet[188])
{
  memset(packet, 0xFF, 188);
  packet[0] = 0x47;
  packet[1] = 0x40 | ((pid >> 8) & 0x1F);
  packet[2] = pid & 0xFF;
  packet[3] = 0x10;
  packet[4] = 0x00;
  fail_unless(section_len + 5 <= 188, "section does not fit in a single TS packet");
  memcpy(packet + 5, section, section_len);
}

/**
 * @brief Locate a stream entry inside a parsed PMT by PID.
 * @param pmt Parsed PMT object.
 * @param pid Elementary PID to search for.
 * @return Matching stream entry or `NULL`.
 */
static TsStreamInfo *
find_stream_by_pid(TsPmtInfo *pmt, guint16 pid)
{
  for (guint i = 0; i < pmt->streams->len; i++) {
    TsStreamInfo *stream = g_ptr_array_index(pmt->streams, i);
    if (stream->pid == pid)
      return stream;
  }
  return NULL;
}

/**
 * @brief Locate a descriptor by tag inside a descriptor list.
 * @param descriptors Descriptor array to inspect.
 * @param tag Descriptor tag to search for.
 * @return Matching descriptor or `NULL`.
 */
static TsDescriptor *
find_descriptor(GPtrArray *descriptors, guint8 tag)
{
  for (guint i = 0; descriptors != NULL && i < descriptors->len; i++) {
    TsDescriptor *descriptor = g_ptr_array_index(descriptors, i);
    if (descriptor->tag == tag)
      return descriptor;
  }
  return NULL;
}

/**
 * @brief Push a PAT+PMT buffer through the element and return the output.
 * @param element Rewriter instance under test.
 * @return Output buffer produced by the harness.
 */
static GstBuffer *
rewrite_buffer_with_element(GstElement *element)
{
  guint8 pat_packet[188];
  guint8 pmt_packet[188];
  GByteArray *input_bytes = g_byte_array_new();
  GByteArray *pmt_section = build_test_pmt_section();

  GstHarness *h = gst_harness_new_with_element(element, "sink", "src");
  fail_unless(h != NULL);
  gst_harness_play(h);
  gst_harness_set_src_caps_str(h, "video/mpegts, systemstream=(boolean)true, packetsize=(int)188");

  build_pat_packet(0x0042, pat_packet);
  build_section_packet(0x0042, pmt_section->data, pmt_section->len, pmt_packet);
  append_packet(input_bytes, pat_packet);
  append_packet(input_bytes, pmt_packet);

  GstBuffer *in = gst_buffer_new_allocate(NULL, input_bytes->len, NULL);
  gst_buffer_fill(in, 0, input_bytes->data, input_bytes->len);

  GstFlowReturn ret = gst_harness_push(h, in);
  fail_unless(ret == GST_FLOW_OK, "push failed: %s", gst_flow_get_name(ret));

  GstBuffer *out = gst_harness_pull(h);
  fail_unless(out != NULL, "missing rewritten output");

  g_byte_array_unref(input_bytes);
  g_byte_array_unref(pmt_section);
  gst_harness_teardown(h);
  return out;
}

/**
 * @brief Verify that the element factory instantiates `tspmtrewrite`.
 */
GST_START_TEST(test_tspmtrewrite_create)
{
  GstElement *el = gst_element_factory_make("tspmtrewrite", NULL);
  fail_unless(el != NULL, "tspmtrewrite element could not be created");
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify that the rewriter exposes the expected static pads.
 */
GST_START_TEST(test_tspmtrewrite_pads)
{
  GstElement *el = gst_element_factory_make("tspmtrewrite", NULL);
  fail_unless(el != NULL);

  GstPad *sink = gst_element_get_static_pad(el, "sink");
  GstPad *src = gst_element_get_static_pad(el, "src");
  fail_unless(sink != NULL, "missing sink pad");
  fail_unless(src != NULL, "missing src pad");

  gst_object_unref(sink);
  gst_object_unref(src);
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify the default metadata descriptor property values.
 */
GST_START_TEST(test_tspmtrewrite_default_properties)
{
  GstElement *el = gst_element_factory_make("tspmtrewrite", NULL);
  fail_unless(el != NULL);

  guint app_format = 0;
  guint format = 0;
  guint service_id = 0;
  guint flags = 0;
  gchar *app_id = NULL;
  gchar *format_id = NULL;

  g_object_get(el,
               "metadata-app-format",
               &app_format,
               "metadata-app-identifier",
               &app_id,
               "metadata-format",
               &format,
               "metadata-format-identifier",
               &format_id,
               "metadata-service-id",
               &service_id,
               "metadata-flags",
               &flags,
               NULL);

  fail_unless_equals_int(app_format, 0xFFFF);
  fail_unless_equals_int(format, 0xFF);
  fail_unless_equals_int(service_id, 0x01);
  fail_unless_equals_int(flags, 0x00);
  fail_unless(g_strcmp0(app_id, "MISB") == 0, "unexpected app identifier");
  fail_unless(g_strcmp0(format_id, "KLVA") == 0, "unexpected format identifier");

  g_free(app_id);
  g_free(format_id);
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify PMT rewriting to `0x06 + KLVA` signalling.
 */
GST_START_TEST(test_tspmtrewrite_rewrites_klva_stream_to_private_pes)
{
  GstElement *element = gst_element_factory_make("tspmtrewrite", "rewriter");
  fail_unless(element != NULL);

  GstBuffer *out = rewrite_buffer_with_element(element);
  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));
  fail_unless(map.size >= 376, "unexpected output size");

  const guint8 *section = NULL;
  gsize section_len = 0;
  fail_unless(ts_extract_section_from_packet(map.data + 188, 188, 0x02, &section, &section_len));

  TsPmtInfo pmt;
  ts_pmt_info_init(&pmt);
  fail_unless(ts_parse_pmt_section(section, section_len, &pmt));

  TsStreamInfo *klv = find_stream_by_pid(&pmt, 0x0042);
  fail_unless(klv != NULL, "missing KLV stream after rewrite");
  fail_unless_equals_int(klv->stream_type, 0x06);
  fail_unless(find_descriptor(klv->descriptors, 0x05) != NULL, "registration descriptor missing");
  fail_unless(find_descriptor(klv->descriptors, 0x26) != NULL, "metadata descriptor missing");
  fail_unless(((pmt.version_byte >> 1) & 0x1F) == 1, "PMT version should be incremented");

  ts_pmt_info_clear(&pmt);
  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
}
GST_END_TEST

/**
 * @brief Verify serialization of custom metadata descriptor values.
 */
GST_START_TEST(test_tspmtrewrite_serializes_custom_metadata_descriptor)
{
  GstElement *element = gst_element_factory_make("tspmtrewrite", "rewriter");
  fail_unless(element != NULL);

  g_object_set(element,
               "metadata-app-format",
               0xFFFF,
               "metadata-app-identifier",
               "TEST",
               "metadata-format",
               0xFF,
               "metadata-format-identifier",
               "ABCD",
               "metadata-service-id",
               0x21,
               "metadata-flags",
               0x80,
               NULL);

  GstBuffer *out = rewrite_buffer_with_element(element);
  GstMapInfo map;
  fail_unless(gst_buffer_map(out, &map, GST_MAP_READ));

  const guint8 *section = NULL;
  gsize section_len = 0;
  fail_unless(ts_extract_section_from_packet(map.data + 188, 188, 0x02, &section, &section_len));

  TsPmtInfo pmt;
  ts_pmt_info_init(&pmt);
  fail_unless(ts_parse_pmt_section(section, section_len, &pmt));

  TsStreamInfo *klv = find_stream_by_pid(&pmt, 0x0042);
  fail_unless(klv != NULL);
  TsDescriptor *metadata = find_descriptor(klv->descriptors, 0x26);
  fail_unless(metadata != NULL, "metadata descriptor missing");
  fail_unless_equals_int((gint)metadata->payload->len, 13);

  const guint8 expected[] = {
    0xFF,
    0xFF,
    'T',
    'E',
    'S',
    'T',
    0xFF,
    'A',
    'B',
    'C',
    'D',
    0x21,
    0x80,
  };
  fail_unless(memcmp(metadata->payload->data, expected, sizeof(expected)) == 0,
              "metadata descriptor payload did not match custom properties");

  ts_pmt_info_clear(&pmt);
  gst_buffer_unmap(out, &map);
  gst_buffer_unref(out);
}
GST_END_TEST

/**
 * @brief Build the `tspmtrewrite` suite definition.
 * @return Populated Check suite.
 */
static Suite *
tspmtrewrite_suite(void)
{
  Suite *s = suite_create("tspmtrewrite");
  TCase *tc = tcase_create("general");

  tcase_add_test(tc, test_tspmtrewrite_create);
  tcase_add_test(tc, test_tspmtrewrite_pads);
  tcase_add_test(tc, test_tspmtrewrite_default_properties);
  tcase_add_test(tc, test_tspmtrewrite_rewrites_klva_stream_to_private_pes);
  tcase_add_test(tc, test_tspmtrewrite_serializes_custom_metadata_descriptor);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(tspmtrewrite)
