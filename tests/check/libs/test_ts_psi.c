/**
 * @file tests/check/libs/test_ts_psi.c
 * @brief gst-check coverage for internal MPEG-TS PSI helpers.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the PSI utility layer used by the TS metadata path:
 *
 * - CRC-32/MPEG-2 computation
 * - PAT parsing
 * - PMT build / parse round-trips
 * - PSI section extraction from TS packets
 */

#include <gst/check/gstcheck.h>
#include <string.h>

#include "gstklv/internal/ts_crc32.h"
#include "gstklv/internal/ts_psi.h"

/**
 * @brief Build a simple PMT section used by the TS helper tests.
 * @return Newly allocated PMT section byte array.
 */
static GByteArray *
build_simple_pmt_section(void)
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

  TsDescriptor *metadata = g_new0(TsDescriptor, 1);
  metadata->tag = 0x26;
  metadata->payload = g_byte_array_new();
  {
    const guint8 payload[] = {0xFF, 0xFF, 'M', 'I', 'S', 'B', 0xFF, 'K', 'L', 'V', 'A', 0x01, 0x00};
    g_byte_array_append(metadata->payload, payload, sizeof(payload));
  }
  g_ptr_array_add(klv->descriptors, metadata);

  g_ptr_array_add(pmt.streams, klv);

  GByteArray *section = ts_build_pmt_section(&pmt);
  ts_pmt_info_clear(&pmt);
  return section;
}

/**
 * @brief Build a minimal PAT section pointing to the PMT PID under test.
 * @param pmt_pid PMT PID to advertise.
 * @param pat Output PAT section bytes.
 */
static void
build_pat_section(guint16 pmt_pid, guint8 pat[16])
{
  const guint8 prefix[] = {
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
  };
  memcpy(pat, prefix, sizeof(prefix));
  pat[10] = 0xE0 | ((pmt_pid >> 8) & 0x1F);
  pat[11] = pmt_pid & 0xFF;

  guint32 crc = ts_crc32_mpeg2(pat, 12);
  pat[12] = (crc >> 24) & 0xFF;
  pat[13] = (crc >> 16) & 0xFF;
  pat[14] = (crc >> 8) & 0xFF;
  pat[15] = crc & 0xFF;
}

/**
 * @brief Wrap a PSI section into a single TS packet.
 * @param section PSI section payload.
 * @param section_len Number of bytes in @p section.
 * @param pid Packet PID.
 * @param packet Output 188-byte TS packet buffer.
 */
static void
wrap_section_in_ts_packet(const guint8 *section, gsize section_len, guint16 pid, guint8 packet[188])
{
  memset(packet, 0xFF, 188);
  packet[0] = 0x47;
  packet[1] = 0x40 | ((pid >> 8) & 0x1F);
  packet[2] = pid & 0xFF;
  packet[3] = 0x10;
  packet[4] = 0x00;
  fail_unless(section_len + 5 <= 188, "section does not fit in one TS packet");
  memcpy(packet + 5, section, section_len);
}

/**
 * @brief Verify the CRC-32/MPEG-2 helper against a known vector.
 */
GST_START_TEST(test_ts_crc32_known_vector)
{
  const guint8 data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  fail_unless_equals_int((gint)ts_crc32_mpeg2(data, sizeof(data)), 0x0376E6E7);
}
GST_END_TEST

/**
 * @brief Verify PAT parsing for PMT PID discovery.
 */
GST_START_TEST(test_ts_psi_parse_pat_section)
{
  guint8 pat[16] = {0};
  guint16 pmt_pid = 0;

  build_pat_section(0x0042, pat);
  fail_unless(ts_parse_pat_section(pat, sizeof(pat), &pmt_pid));
  fail_unless_equals_int(pmt_pid, 0x0042);
}
GST_END_TEST

/**
 * @brief Verify PMT section build / parse round-trip behaviour.
 */
GST_START_TEST(test_ts_psi_build_and_parse_pmt)
{
  GByteArray *section = build_simple_pmt_section();
  fail_unless(section != NULL);

  TsPmtInfo parsed;
  ts_pmt_info_init(&parsed);
  fail_unless(ts_parse_pmt_section(section->data, section->len, &parsed));
  fail_unless_equals_int(parsed.program_number, 1);
  fail_unless_equals_int((gint)parsed.streams->len, 2);

  TsStreamInfo *video = g_ptr_array_index(parsed.streams, 0);
  TsStreamInfo *klv = g_ptr_array_index(parsed.streams, 1);
  fail_unless(video != NULL);
  fail_unless(klv != NULL);
  fail_unless_equals_int(video->stream_type, 0x1B);
  fail_unless_equals_int(klv->stream_type, 0x15);
  fail_unless_equals_int(klv->pid, 0x0042);
  fail_unless_equals_int((gint)klv->descriptors->len, 2);

  TsDescriptor *registration = g_ptr_array_index(klv->descriptors, 0);
  TsDescriptor *metadata = g_ptr_array_index(klv->descriptors, 1);
  fail_unless_equals_int(registration->tag, 0x05);
  fail_unless_equals_int(metadata->tag, 0x26);

  ts_pmt_info_clear(&parsed);
  g_byte_array_unref(section);
}
GST_END_TEST

/**
 * @brief Verify PSI section extraction from a packetized PMT.
 */
GST_START_TEST(test_ts_psi_extract_section_from_packet)
{
  GByteArray *section = build_simple_pmt_section();
  guint8 packet[188];
  const guint8 *out_section = NULL;
  gsize out_len = 0;

  fail_unless(section != NULL);
  wrap_section_in_ts_packet(section->data, section->len, 0x0042, packet);

  fail_unless(ts_extract_section_from_packet(packet, sizeof(packet), 0x02, &out_section, &out_len));
  fail_unless_equals_int((gint)out_len, (gint)section->len);
  fail_unless(memcmp(out_section, section->data, section->len) == 0,
              "extracted section does not match original");

  g_byte_array_unref(section);
}
GST_END_TEST

/**
 * @brief Build the `ts-psi` suite definition.
 * @return Populated Check suite.
 */
static Suite *
ts_psi_suite(void)
{
  Suite *s = suite_create("ts-psi");
  TCase *tc = tcase_create("general");

  tcase_add_test(tc, test_ts_crc32_known_vector);
  tcase_add_test(tc, test_ts_psi_parse_pat_section);
  tcase_add_test(tc, test_ts_psi_build_and_parse_pmt);
  tcase_add_test(tc, test_ts_psi_extract_section_from_packet);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(ts_psi)
