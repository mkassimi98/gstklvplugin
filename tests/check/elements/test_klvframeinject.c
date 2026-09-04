/**
 * @file tests/check/elements/test_klvframeinject.c
 * @brief gst-check coverage for the klvframeinject element.
 * @ingroup gstklv_tests
 * @author Mouhsine Kassimi Farhaoui
 * @par Mail
 * mouhsine98@gmail.com
 *
 * This suite validates the multi-pad contract of `klvframeinject`:
 *
 * - factory creation, pads, and default properties
 * - video passthrough on `video_src`
 * - per-frame KLV emission on `klv_src`
 * - timestamp alignment across both output branches
 * - fallback metadata generation from element properties
 */

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <math.h>
#include <string.h>

#include "gstklv/internal/klv_json.h"

/**
 * @brief Small pipeline fixture used to exercise both output branches.
 */
typedef struct
{
  GstElement *pipeline;
  GstElement *inject;
  GstAppSrc *src;
  GstAppSink *video_sink;
  GstAppSink *klv_sink;
} FrameInjectPipeline;

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
 * @brief Create a minimal in-process pipeline around `klvframeinject`.
 * @return Initialised pipeline fixture with named elements resolved.
 */
static FrameInjectPipeline
frameinject_pipeline_new(void)
{
  GError *error = NULL;
  FrameInjectPipeline ctx = {0};
  const gchar *launch =
    "appsrc name=src is-live=false format=time do-timestamp=false "
    "caps=\"video/x-h264,stream-format=(string)byte-stream,alignment=(string)au\" "
    "! klvframeinject name=inject "
    "inject.video_src ! queue ! appsink name=video_sink sync=false async=false emit-signals=false "
    "inject.klv_src ! queue ! appsink name=klv_sink sync=false async=false emit-signals=false "
    "caps=\"meta/x-klv,parsed=(boolean)true\"";

  ctx.pipeline = gst_parse_launch(launch, &error);
  fail_unless(
    error == NULL, "failed to create test pipeline: %s", error ? error->message : "unknown error");
  fail_unless(ctx.pipeline != NULL);

  ctx.inject = gst_bin_get_by_name(GST_BIN(ctx.pipeline), "inject");
  ctx.src = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(ctx.pipeline), "src"));
  ctx.video_sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(ctx.pipeline), "video_sink"));
  ctx.klv_sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(ctx.pipeline), "klv_sink"));

  fail_unless(ctx.inject != NULL);
  fail_unless(ctx.src != NULL);
  fail_unless(ctx.video_sink != NULL);
  fail_unless(ctx.klv_sink != NULL);

  gst_app_sink_set_wait_on_eos(ctx.video_sink, FALSE);
  gst_app_sink_set_wait_on_eos(ctx.klv_sink, FALSE);

  fail_unless(gst_element_set_state(ctx.pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
              "pipeline failed to go to PLAYING");
  return ctx;
}

/**
 * @brief Tear down the pipeline fixture created by frameinject_pipeline_new().
 * @param ctx Pipeline fixture to release.
 */
static void
frameinject_pipeline_free(FrameInjectPipeline *ctx)
{
  if (!ctx)
    return;

  if (ctx->pipeline)
    gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
  if (ctx->src)
    gst_object_unref(ctx->src);
  if (ctx->video_sink)
    gst_object_unref(ctx->video_sink);
  if (ctx->klv_sink)
    gst_object_unref(ctx->klv_sink);
  if (ctx->inject)
    gst_object_unref(ctx->inject);
  if (ctx->pipeline)
    gst_object_unref(ctx->pipeline);

  memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Allocate a deterministic input buffer for the appsrc branch.
 * @param data Payload bytes to copy into the buffer.
 * @param size Number of bytes in @p data.
 * @param pts Presentation timestamp to stamp on the buffer.
 * @param dts Decode timestamp to stamp on the buffer.
 * @return Newly allocated `GstBuffer`.
 */
static GstBuffer *
build_input_buffer(const guint8 *data, gsize size, GstClockTime pts, GstClockTime dts)
{
  GstBuffer *buffer = gst_buffer_new_allocate(NULL, size, NULL);
  gst_buffer_fill(buffer, 0, data, size);
  GST_BUFFER_PTS(buffer) = pts;
  GST_BUFFER_DTS(buffer) = dts;
  return buffer;
}

/**
 * @brief Push a buffer into the test appsrc and wait for a sample on a sink.
 * @param src Source element feeding the pipeline.
 * @param sink Sink from which a sample is expected.
 * @param buffer Input buffer to push.
 * @return Pulled sample from @p sink.
 */
static GstSample *
push_and_pull_sample(GstAppSrc *src, GstAppSink *sink, GstBuffer *buffer)
{
  GstFlowReturn ret = gst_app_src_push_buffer(src, buffer);
  fail_unless(ret == GST_FLOW_OK, "push failed: %s", gst_flow_get_name(ret));

  GstSample *sample = gst_app_sink_try_pull_sample(sink, 2 * GST_SECOND);
  fail_unless(sample != NULL, "timed out waiting for sample");
  return sample;
}

/**
 * @brief Decode a captured KLV buffer back into parsed JSON tags.
 * @param buffer KLV buffer captured from the `klv_src` branch.
 * @param tags Destination array for parsed tags.
 * @param max_tags Capacity of @p tags.
 * @return Number of parsed tags, or a negative value on parse failure.
 */
static gint
decode_klv_to_tags(GstBuffer *buffer, KLVJsonTag *tags, gint max_tags)
{
  GstHarness *dec = gst_harness_new("klvmetadec");
  GstMapInfo map;
  gint count = -1;

  fail_unless(dec != NULL);
  gst_harness_set_src_caps_str(dec, "meta/x-klv, parsed=true");

  GstFlowReturn ret = gst_harness_push(dec, gst_buffer_copy(buffer));
  fail_unless(ret == GST_FLOW_OK, "decoder push failed: %s", gst_flow_get_name(ret));

  GstBuffer *json = gst_harness_pull(dec);
  fail_unless(json != NULL, "decoder produced no JSON output");

  fail_unless(gst_buffer_map(json, &map, GST_MAP_READ));
  count = klv_json_parse_flat((const gchar *)map.data, map.size, tags, max_tags);
  gst_buffer_unmap(json, &map);
  gst_buffer_unref(json);
  gst_harness_teardown(dec);
  return count;
}

/**
 * @brief Verify that the element factory instantiates `klvframeinject`.
 */
GST_START_TEST(test_klvframeinject_create)
{
  GstElement *el = gst_element_factory_make("klvframeinject", NULL);
  fail_unless(el != NULL, "klvframeinject element could not be created");
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify that the injector exposes sink, video, and KLV pads.
 */
GST_START_TEST(test_klvframeinject_pads)
{
  GstElement *el = gst_element_factory_make("klvframeinject", NULL);
  fail_unless(el != NULL);

  GstPad *sink = gst_element_get_static_pad(el, "sink");
  GstPad *video_src = gst_element_get_static_pad(el, "video_src");
  GstPad *klv_src = gst_element_get_static_pad(el, "klv_src");

  fail_unless(sink != NULL, "missing sink pad");
  fail_unless(video_src != NULL, "missing video_src pad");
  fail_unless(klv_src != NULL, "missing klv_src pad");

  gst_object_unref(sink);
  gst_object_unref(video_src);
  gst_object_unref(klv_src);
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify default property values exposed by the injector.
 */
GST_START_TEST(test_klvframeinject_properties)
{
  GstElement *el = gst_element_factory_make("klvframeinject", NULL);
  fail_unless(el != NULL);

  gdouble lat = 0.0, lon = 0.0, heading = 0.0, altitude = 0.0;
  guint64 timestamp = G_MAXUINT64;
  gboolean use_system_time = FALSE;
  gchar *tags_json = NULL;

  g_object_get(el,
               "latitude",
               &lat,
               "longitude",
               &lon,
               "heading",
               &heading,
               "altitude",
               &altitude,
               "timestamp",
               &timestamp,
               "use-system-time",
               &use_system_time,
               "tags-json",
               &tags_json,
               NULL);

  fail_unless(lat >= -90.0 && lat <= 90.0, "latitude out of range");
  fail_unless(lon >= -180.0 && lon <= 180.0, "longitude out of range");
  fail_unless(heading >= 0.0 && heading <= 360.0, "heading out of range");
  fail_unless(altitude == 1000.0, "unexpected default altitude");
  fail_unless(timestamp == 0, "unexpected default timestamp");
  fail_unless(use_system_time == TRUE, "use-system-time should default to TRUE");
  fail_unless(tags_json != NULL && *tags_json == '\0', "tags-json should default to empty string");

  g_free(tags_json);
  gst_object_unref(el);
}
GST_END_TEST

/**
 * @brief Verify video passthrough and KLV emission from the JSON property.
 */
GST_START_TEST(test_klvframeinject_pushes_video_and_json_klv)
{
  const guint8 payload[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};
  FrameInjectPipeline ctx = frameinject_pipeline_new();

  g_object_set(ctx.inject,
               "use-system-time",
               FALSE,
               "tags-json",
               "{\"2\":1774033378153350,\"13\":12.5,\"14\":-3.25}",
               NULL);

  GstSample *video_sample = push_and_pull_sample(
    ctx.src,
    ctx.video_sink,
    build_input_buffer(payload, sizeof(payload), 3 * GST_SECOND, 2 * GST_SECOND));
  GstSample *klv_sample = gst_app_sink_try_pull_sample(ctx.klv_sink, 2 * GST_SECOND);
  fail_unless(klv_sample != NULL, "timed out waiting for KLV sample");

  GstBuffer *video = gst_sample_get_buffer(video_sample);
  GstBuffer *klv = gst_sample_get_buffer(klv_sample);
  GstCaps *klv_caps = gst_sample_get_caps(klv_sample);

  fail_unless(video != NULL);
  fail_unless(klv != NULL);
  fail_unless(GST_BUFFER_PTS(video) == 3 * GST_SECOND);
  fail_unless(GST_BUFFER_DTS(video) == 2 * GST_SECOND);
  fail_unless(GST_BUFFER_PTS(klv) == 3 * GST_SECOND);
  fail_unless(GST_BUFFER_DTS(klv) == 2 * GST_SECOND);

  guint8 out_bytes[sizeof(payload)] = {0};
  gst_buffer_extract(video, 0, out_bytes, sizeof(out_bytes));
  fail_unless(memcmp(out_bytes, payload, sizeof(payload)) == 0,
              "video payload was not forwarded unchanged");

  fail_unless(klv_caps != NULL, "missing caps on klv branch");
  fail_unless(gst_caps_get_size(klv_caps) == 1);
  fail_unless(
    g_strcmp0(gst_structure_get_name(gst_caps_get_structure(klv_caps, 0)), "meta/x-klv") == 0,
    "unexpected klv caps");

  KLVJsonTag tags[8] = {0};
  gint count = decode_klv_to_tags(klv, tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 3);

  KLVJsonTag *tag2 = find_tag(tags, count, 2);
  KLVJsonTag *tag13 = find_tag(tags, count, 13);
  KLVJsonTag *tag14 = find_tag(tags, count, 14);

  fail_unless(tag2 != NULL && tag2->number_text != NULL, "missing Tag 2");
  fail_unless(g_strcmp0(tag2->number_text, "1774033378153350") == 0,
              "unexpected Tag 2 text: %s",
              GST_STR_NULL(tag2->number_text));
  fail_unless(tag13 != NULL && fabs(tag13->value - 12.5) < 0.05, "unexpected Tag 13 value");
  fail_unless(tag14 != NULL && fabs(tag14->value + 3.25) < 0.05, "unexpected Tag 14 value");

  free_json_tags(tags, count);
  gst_sample_unref(video_sample);
  gst_sample_unref(klv_sample);
  frameinject_pipeline_free(&ctx);
}
GST_END_TEST

/**
 * @brief Verify fallback KLV generation from scalar element properties.
 */
GST_START_TEST(test_klvframeinject_falls_back_to_properties)
{
  const guint8 payload[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x20};
  FrameInjectPipeline ctx = frameinject_pipeline_new();

  g_object_set(ctx.inject,
               "use-system-time",
               FALSE,
               "timestamp",
               (guint64)1700000000,
               "latitude",
               42.25,
               "longitude",
               -8.5,
               "heading",
               123.0,
               "altitude",
               1500.0,
               NULL);

  GstSample *video_sample = push_and_pull_sample(
    ctx.src,
    ctx.video_sink,
    build_input_buffer(payload, sizeof(payload), 7 * GST_SECOND, 6 * GST_SECOND));
  GstSample *klv_sample = gst_app_sink_try_pull_sample(ctx.klv_sink, 2 * GST_SECOND);
  fail_unless(klv_sample != NULL, "timed out waiting for KLV sample");

  GstBuffer *klv = gst_sample_get_buffer(klv_sample);
  fail_unless(klv != NULL);
  fail_unless(GST_BUFFER_PTS(klv) == 7 * GST_SECOND);
  fail_unless(GST_BUFFER_DTS(klv) == 6 * GST_SECOND);

  KLVJsonTag tags[8] = {0};
  gint count = decode_klv_to_tags(klv, tags, G_N_ELEMENTS(tags));
  fail_unless(count >= 5, "expected fallback tags, got %d", count);

  KLVJsonTag *tag2 = find_tag(tags, count, 2);
  KLVJsonTag *tag5 = find_tag(tags, count, 5);
  KLVJsonTag *tag13 = find_tag(tags, count, 13);
  KLVJsonTag *tag14 = find_tag(tags, count, 14);
  KLVJsonTag *tag15 = find_tag(tags, count, 15);

  fail_unless(tag2 != NULL && tag2->number_text != NULL, "missing Tag 2");
  fail_unless(g_strcmp0(tag2->number_text, "1700000000") == 0,
              "unexpected fallback Tag 2 text: %s",
              GST_STR_NULL(tag2->number_text));
  fail_unless(tag5 != NULL && fabs(tag5->value - 123.0) < 0.05, "unexpected Tag 5 value");
  fail_unless(tag13 != NULL && fabs(tag13->value - 42.25) < 0.05, "unexpected Tag 13 value");
  fail_unless(tag14 != NULL && fabs(tag14->value + 8.5) < 0.05, "unexpected Tag 14 value");
  fail_unless(tag15 != NULL && fabs(tag15->value - 1500.0) < 0.5, "unexpected Tag 15 value");

  free_json_tags(tags, count);
  gst_sample_unref(video_sample);
  gst_sample_unref(klv_sample);
  frameinject_pipeline_free(&ctx);
}
GST_END_TEST

/**
 * @brief Verify Tag 95 (SAR Motion Imagery Metadata) is not dropped.
 *
 * Regression test for a hard-coded `tag_id > 93` gate that predates the
 * ST 0601.8 registry correction through Tag 95; it silently discarded any
 * JSON tag above 93, including the new Tags 94 and 95.
 */
GST_START_TEST(test_klvframeinject_accepts_tag_95)
{
  const guint8 payload[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x30};
  FrameInjectPipeline ctx = frameinject_pipeline_new();

  g_object_set(ctx.inject,
               "use-system-time",
               FALSE,
               "tags-json",
               "{\"2\":1774033378153350,\"94\":\"hex:0102\",\"95\":\"hex:deadbeef\"}",
               NULL);

  GstSample *video_sample = push_and_pull_sample(
    ctx.src,
    ctx.video_sink,
    build_input_buffer(payload, sizeof(payload), 4 * GST_SECOND, 3 * GST_SECOND));
  GstSample *klv_sample = gst_app_sink_try_pull_sample(ctx.klv_sink, 2 * GST_SECOND);
  fail_unless(klv_sample != NULL, "timed out waiting for KLV sample");

  GstBuffer *klv = gst_sample_get_buffer(klv_sample);
  fail_unless(klv != NULL);

  KLVJsonTag tags[8] = {0};
  gint count = decode_klv_to_tags(klv, tags, G_N_ELEMENTS(tags));
  fail_unless_equals_int(count, 3);

  KLVJsonTag *tag94 = find_tag(tags, count, 94);
  KLVJsonTag *tag95 = find_tag(tags, count, 95);
  fail_unless(tag94 != NULL, "Tag 94 was dropped by klvframeinject");
  fail_unless(tag95 != NULL, "Tag 95 was dropped by klvframeinject");

  free_json_tags(tags, count);
  gst_sample_unref(video_sample);
  gst_sample_unref(klv_sample);
  frameinject_pipeline_free(&ctx);
}
GST_END_TEST

/**
 * @brief Build the `klvframeinject` suite definition.
 * @return Populated Check suite.
 */
static Suite *
klvframeinject_suite(void)
{
  Suite *s = suite_create("klvframeinject");
  TCase *tc = tcase_create("general");

  tcase_add_test(tc, test_klvframeinject_create);
  tcase_add_test(tc, test_klvframeinject_pads);
  tcase_add_test(tc, test_klvframeinject_properties);
  tcase_add_test(tc, test_klvframeinject_pushes_video_and_json_klv);
  tcase_add_test(tc, test_klvframeinject_falls_back_to_properties);
  tcase_add_test(tc, test_klvframeinject_accepts_tag_95);

  suite_add_tcase(s, tc);
  return s;
}

GST_CHECK_MAIN(klvframeinject)
