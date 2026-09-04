/**
 * @file examples/udp-pipelines/cpp/udp_sender_95tags.cpp
 * @brief C++ UDP sender that streams H.264 with MISB ST 0601.8 KLV tags.
 *
 * @ingroup gstklv_examples_cpp
 *
 * @details
 * This example generates a complete MISB ST 0601.8 tag set, injects it
 * as KLV metadata, and streams the result via UDP/MPEG-TS. Tags are loaded
 * from `data/stanag4609_tags.ini`. Numeric tags are generated within their
 * declared ranges, string tags get a `DEMO-<tag>` placeholder, and byte
 * tags emit deterministic `hex:` payloads.
 *
 * @section gstklv_udp_sender_pipeline Pipeline Topology
 * The sender pipeline is built as a single GStreamer launch string:
 *
 * @dot
 * digraph sender_pipeline {
 *   rankdir=LR;
 *   node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
 *   videotestsrc -> videoconvert -> capsfilter -> x264enc -> h264parse -> klvframeinject;
 *   klvframeinject -> queue_video -> mpegtsmux -> tspmtrewrite -> udpsink;
 *   klvframeinject -> queue_klv -> klv_caps -> mpegtsmux;
 *   klv_caps [label="meta/x-klv\nstream-type=21"];
 * }
 * @enddot
 *
 * @note `tspmtrewrite` is configured to emit tsdemux-friendly KLVA metadata
 * signaling in the PMT while preserving STANAG 4609 interoperability.
 */

#include <gst/gst.h>
#include <glib.h>

#include "gstklv/internal/klv_tag_defs.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct Options
{
  std::string host = "127.0.0.1";
  int port = 5000;
  int fps = 2;
  int bitrate_kbps = 2000;
  int count = 0;
  std::string tags_ini;
  std::string plugin_path;
  unsigned int seed = 0;
  bool seed_provided = false;
  guint metadata_app_format = 0xFFFF;
  std::string metadata_app_identifier = "MISB";
  guint metadata_format = 0xFF;
  std::string metadata_format_identifier = "KLVA";
  guint metadata_service_id = 0x01;
  guint metadata_flags = 0x00;
};

std::string
default_source_dir()
{
#ifdef GSTKLVPLUGIN_SOURCE_DIR
  return std::string(GSTKLVPLUGIN_SOURCE_DIR);
#else
  return std::string(".");
#endif
}

std::string
join_path(const std::string &base, const std::string &rel)
{
  if (base.empty())
    return rel;
  if (base.back() == '/')
    return base + rel;
  return base + "/" + rel;
}

bool
parse_hex_uint(const char *text, guint *out)
{
  if (!text || !out)
    return false;
  char *end = nullptr;
  unsigned long v = std::strtoul(text, &end, 0);
  if (!end || *end != '\0')
    return false;
  *out = static_cast<guint>(v);
  return true;
}

bool
mpegtsmux_supports_enable_custom_mappings()
{
  GstElement *mux = gst_element_factory_make("mpegtsmux", nullptr);
  if (!mux)
    return false;
  GParamSpec *prop =
    g_object_class_find_property(G_OBJECT_GET_CLASS(mux), "enable-custom-mappings");
  gst_object_unref(mux);
  return prop != nullptr;
}

bool
has_property(GstElement *element, const char *name)
{
  return element && name &&
         g_object_class_find_property(G_OBJECT_GET_CLASS(element), name) != nullptr;
}

std::string
hex_bytes_for_tag(int tag_id)
{
  guint8 b0 = static_cast<guint8>(tag_id & 0xFF);
  guint8 b1 = static_cast<guint8>((tag_id * 3) & 0xFF);
  guint8 b2 = static_cast<guint8>((tag_id * 7) & 0xFF);
  guint8 b3 = static_cast<guint8>((tag_id * 11) & 0xFF);
  std::ostringstream oss;
  oss << "hex:";
  oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b0) << std::setw(2)
      << static_cast<int>(b1) << std::setw(2) << static_cast<int>(b2) << std::setw(2)
      << static_cast<int>(b3);
  return oss.str();
}

std::string
json_escape(const std::string &in)
{
  std::ostringstream oss;
  for (char c : in) {
    switch (c) {
      case '\\': oss << "\\\\"; break;
      case '"': oss << "\\\""; break;
      case '\n': oss << "\\n"; break;
      case '\r': oss << "\\r"; break;
      case '\t': oss << "\\t"; break;
      default: oss << c; break;
    }
  }
  return oss.str();
}

struct TagGenerator
{
  std::mt19937 rng;
  double lat = 40.7128;
  double lon = -74.0060;
  double alt = 500.0;
  double heading = 0.0;
  double pitch = 0.0;
  double roll = 0.0;
  double airspeed = 150.0;
  double north_vel = 5.0;
  double east_vel = -5.0;
  double up_vel = 0.5;
  guint64 event_start_us = 0;

  explicit TagGenerator(unsigned int seed)
  {
    if (seed == 0) {
      seed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    rng.seed(seed);
    std::uniform_real_distribution<double> d(-0.1, 0.1);
    lat += d(rng);
    lon += d(rng);
    alt += std::uniform_real_distribution<double>(-50, 50)(rng);
    heading = std::uniform_real_distribution<double>(0, 360)(rng);
    pitch = std::uniform_real_distribution<double>(-20, 20)(rng);
    roll = std::uniform_real_distribution<double>(-45, 45)(rng);
    airspeed = std::uniform_real_distribution<double>(100, 300)(rng);
    north_vel = std::uniform_real_distribution<double>(-50, 50)(rng);
    east_vel = std::uniform_real_distribution<double>(-50, 50)(rng);
    up_vel = std::uniform_real_distribution<double>(-10, 10)(rng);
    event_start_us = static_cast<guint64>(g_get_real_time());
  }

  void
  update()
  {
    lat += std::uniform_real_distribution<double>(-0.0001, 0.0001)(rng);
    lon += std::uniform_real_distribution<double>(-0.0001, 0.0001)(rng);
    alt += std::uniform_real_distribution<double>(-2, 2)(rng);
    heading =
      std::fmod(heading + std::uniform_real_distribution<double>(-5, 5)(rng) + 360.0, 360.0);
    pitch =
      std::max(-30.0, std::min(30.0, pitch + std::uniform_real_distribution<double>(-2, 2)(rng)));
    roll =
      std::max(-45.0, std::min(45.0, roll + std::uniform_real_distribution<double>(-2, 2)(rng)));
    airspeed = std::max(
      50.0, std::min(400.0, airspeed + std::uniform_real_distribution<double>(-10, 10)(rng)));
    north_vel += std::uniform_real_distribution<double>(-5, 5)(rng);
    east_vel += std::uniform_real_distribution<double>(-5, 5)(rng);
    up_vel += std::uniform_real_distribution<double>(-2, 2)(rng);
  }

  double
  random_in_range(double min, double max)
  {
    if (max <= min)
      return min;
    return std::uniform_real_distribution<double>(min, max)(rng);
  }

  double
  value_for_tag(int tag_id, const KLVTagDef *td)
  {
    if (tag_id == 2)
      return static_cast<double>(g_get_real_time());
    if (tag_id == 72)
      return static_cast<double>(event_start_us);
    if (tag_id == 13)
      return lat;
    if (tag_id == 14)
      return lon;
    if (tag_id == 15)
      return alt;
    if (tag_id == 5)
      return heading;
    if (tag_id == 6)
      return pitch;
    if (tag_id == 7)
      return roll;
    if (tag_id == 8)
      return airspeed;
    if (tag_id == 9)
      return airspeed * 0.95;
    if (tag_id == 51)
      return up_vel;
    if (tag_id == 79)
      return north_vel;
    if (tag_id == 80)
      return east_vel;
    /* Tags 82-89 (Corner Lat/Lon Point 1-4, Full) and 90-93 (Platform
     * Pitch/Roll/Angle-of-Attack/Sideslip, Full) fall through to the
     * generic registry-driven random_in_range() below, which already
     * respects each tag's declared engineering range. Tag 81 (Image
     * Horizon Pixel Pack) is a "bytes" type and is handled separately
     * before this function is ever called. */

    if (td && td->has_range)
      return random_in_range(td->range_min, td->range_max);
    return random_in_range(0.0, 1.0);
  }
};

std::vector<int>
sorted_tag_ids(GHashTable *defs)
{
  std::vector<int> ids;
  GHashTableIter iter;
  gpointer key = nullptr;
  g_hash_table_iter_init(&iter, defs);
  while (g_hash_table_iter_next(&iter, &key, nullptr))
    ids.push_back(GPOINTER_TO_INT(key));
  std::sort(ids.begin(), ids.end());
  return ids;
}

struct GeneratedTag
{
  int tag_id = 0;
  bool is_raw = false;
  std::string raw;
  double value = 0.0;
};

std::string
build_tags_json(GHashTable *defs,
                TagGenerator &gen,
                int *out_count,
                std::vector<GeneratedTag> *out_tags)
{
  std::ostringstream oss;
  oss << "{";
  bool first = true;
  int count = 0;
  for (int tag_id : sorted_tag_ids(defs)) {
    if (tag_id <= 0 || tag_id > 95 || tag_id == 1)
      continue;
    KLVTagDef *td = (KLVTagDef *)g_hash_table_lookup(defs, GINT_TO_POINTER(tag_id));
    if (!td || !td->type)
      continue;

    std::string type(td->type);
    bool is_string = type.rfind("String", 0) == 0 || type.rfind("string", 0) == 0;
    bool is_bytes = type.rfind("bytes", 0) == 0;

    if (!first)
      oss << ",";
    first = false;
    count++;

    oss << "\"" << tag_id << "\":";

    if (is_bytes) {
      std::string hex = hex_bytes_for_tag(tag_id);
      oss << "\"" << hex << "\"";
      if (out_tags) {
        GeneratedTag gt;
        gt.tag_id = tag_id;
        gt.is_raw = true;
        gt.raw = hex;
        out_tags->push_back(std::move(gt));
      }
      continue;
    }

    if (is_string) {
      std::ostringstream label;
      label << "DEMO-" << tag_id;
      oss << "\"" << json_escape(label.str()) << "\"";
      if (out_tags) {
        GeneratedTag gt;
        gt.tag_id = tag_id;
        gt.is_raw = true;
        gt.raw = label.str();
        out_tags->push_back(std::move(gt));
      }
      continue;
    }

    double val = gen.value_for_tag(tag_id, td);
    if (tag_id == 2 || tag_id == 72) {
      guint64 uval = static_cast<guint64>(llround(val));
      oss << static_cast<unsigned long long>(uval);
    }
    else {
      oss << std::setprecision(15) << val;
    }
    if (out_tags) {
      GeneratedTag gt;
      gt.tag_id = tag_id;
      gt.is_raw = false;
      gt.value = val;
      out_tags->push_back(std::move(gt));
    }
  }
  oss << "}";
  if (out_count)
    *out_count = count;
  return oss.str();
}

std::string
tag_units(const KLVTagDef *td)
{
  if (!td || !td->unit || td->unit[0] == '\0')
    return std::string();
  return std::string(" ") + td->unit;
}

void
print_frame_tags(int frame, int tag_count, const std::vector<GeneratedTag> &tags, GHashTable *defs)
{
  std::cout << "\n" << std::string(80, '-') << "\n";
  std::cout << "[FRAME " << std::setw(4) << std::setfill('0') << frame << "] Sent " << tag_count
            << " tags\n";
  std::cout << std::string(80, '-') << "\n";

  for (const auto &tag : tags) {
    KLVTagDef *td =
      defs ? (KLVTagDef *)g_hash_table_lookup(defs, GINT_TO_POINTER(tag.tag_id)) : nullptr;
    std::string name = td && td->name ? td->name : ("Tag " + std::to_string(tag.tag_id));
    std::string units = tag_units(td);

    if (tag.is_raw) {
      std::cout << "  " << std::setw(2) << std::setfill('0') << tag.tag_id << " " << name << ": "
                << tag.raw << units << "\n";
    }
    else {
      std::cout << "  " << std::setw(2) << std::setfill('0') << tag.tag_id << " " << name << ": "
                << std::setprecision(15) << tag.value << units << "\n";
    }
  }
  std::cout << std::string(80, '-') << "\n";
}

gboolean
on_bus_message(GstBus *, GstMessage *msg, gpointer user_data)
{
  GMainLoop *loop = static_cast<GMainLoop *>(user_data);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
      GError *err = nullptr;
      gchar *dbg = nullptr;
      gst_message_parse_error(msg, &err, &dbg);
      std::cerr << "ERROR: " << (err ? err->message : "unknown") << std::endl;
      if (dbg)
        std::cerr << "DEBUG: " << dbg << std::endl;
      g_clear_error(&err);
      g_free(dbg);
      g_main_loop_quit(loop);
      break;
    }
    case GST_MESSAGE_EOS: g_main_loop_quit(loop); break;
    default: break;
  }
  return TRUE;
}

struct SenderState
{
  GstElement *pipeline = nullptr;
  GstElement *inject = nullptr;
  GMainLoop *loop = nullptr;
  GHashTable *defs = nullptr;
  TagGenerator *gen = nullptr;
  int sent = 0;
  int max_count = 0;
};

gboolean
on_tick(gpointer user_data)
{
  SenderState *state = static_cast<SenderState *>(user_data);
  if (!state || !state->inject || !state->defs || !state->gen)
    return G_SOURCE_REMOVE;

  state->gen->update();
  int tag_count = 0;
  std::vector<GeneratedTag> tags;
  tags.reserve(96);
  std::string json = build_tags_json(state->defs, *state->gen, &tag_count, &tags);
  g_object_set(state->inject, "tags-json", json.c_str(), nullptr);

  state->sent++;
  print_frame_tags(state->sent, tag_count, tags, state->defs);

  if (state->max_count > 0 && state->sent >= state->max_count) {
    gst_element_send_event(state->pipeline, gst_event_new_eos());
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

} // namespace

int
main(int argc, char **argv)
{
  Options opt;
  gchar *host_opt = nullptr;
  gchar *tags_ini_opt = nullptr;
  gchar *plugin_path_opt = nullptr;
  gchar *metadata_app_identifier_opt = nullptr;
  gchar *metadata_format_identifier_opt = nullptr;
  gchar *metadata_app_format_str = nullptr;
  gchar *metadata_format_str = nullptr;
  gchar *metadata_service_id_str = nullptr;
  gchar *metadata_flags_str = nullptr;

  GOptionEntry entries[] = {
    {"host",
     0,
     0,
     G_OPTION_ARG_STRING,
     &host_opt,
     (gchar *)"Destination host",
     (gchar *)"127.0.0.1"},
    {"port", 0, 0, G_OPTION_ARG_INT, &opt.port, (gchar *)"Destination port", (gchar *)"5000"},
    {"fps", 0, 0, G_OPTION_ARG_INT, &opt.fps, (gchar *)"Frames per second", (gchar *)"2"},
    {"bitrate",
     0,
     0,
     G_OPTION_ARG_INT,
     &opt.bitrate_kbps,
     (gchar *)"Video bitrate (kbps)",
     (gchar *)"2000"},
    {"count",
     0,
     0,
     G_OPTION_ARG_INT,
     &opt.count,
     (gchar *)"Frame count (0 = infinite)",
     (gchar *)"0"},
    {"tags-ini",
     0,
     0,
     G_OPTION_ARG_STRING,
     &tags_ini_opt,
     (gchar *)"Path to stanag4609_tags.ini",
     nullptr},
    {"plugin-path",
     0,
     0,
     G_OPTION_ARG_STRING,
     &plugin_path_opt,
     (gchar *)"GST_PLUGIN_PATH override",
     nullptr},
    {"seed", 0, 0, G_OPTION_ARG_INT, &opt.seed, (gchar *)"Random seed", nullptr},
    {"metadata-app-format",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_app_format_str,
     (gchar *)"metadata_application_format (hex)",
     (gchar *)"0xFFFF"},
    {"metadata-app-identifier",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_app_identifier_opt,
     (gchar *)"metadata_application_format_identifier",
     (gchar *)"MISB"},
    {"metadata-format",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_format_str,
     (gchar *)"metadata_format (hex)",
     (gchar *)"0xFF"},
    {"metadata-format-identifier",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_format_identifier_opt,
     (gchar *)"metadata_format_identifier",
     (gchar *)"KLVA"},
    {"metadata-service-id",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_service_id_str,
     (gchar *)"metadata_service_id (hex)",
     (gchar *)"0x01"},
    {"metadata-flags",
     0,
     0,
     G_OPTION_ARG_STRING,
     &metadata_flags_str,
     (gchar *)"decoder_config_flags (hex)",
     (gchar *)"0x00"},
    {nullptr}};

  GOptionContext *ctx = g_option_context_new(" - C++ UDP sender with MISB ST 0601.8 tags");
  g_option_context_add_main_entries(ctx, entries, nullptr);
  g_option_context_set_ignore_unknown_options(ctx, TRUE);

  GError *error = nullptr;
  if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
    std::cerr << "ERROR: " << (error ? error->message : "invalid arguments") << std::endl;
    g_clear_error(&error);
    g_option_context_free(ctx);
    return 1;
  }
  g_option_context_free(ctx);

  auto free_meta_strings = [&]() {
    g_free(metadata_app_format_str);
    g_free(metadata_format_str);
    g_free(metadata_service_id_str);
    g_free(metadata_flags_str);
  };

  if (host_opt) {
    opt.host = host_opt;
    g_free(host_opt);
  }
  if (tags_ini_opt) {
    opt.tags_ini = tags_ini_opt;
    g_free(tags_ini_opt);
  }
  if (plugin_path_opt) {
    opt.plugin_path = plugin_path_opt;
    g_free(plugin_path_opt);
  }
  if (metadata_app_identifier_opt) {
    opt.metadata_app_identifier = metadata_app_identifier_opt;
    g_free(metadata_app_identifier_opt);
  }
  if (metadata_format_identifier_opt) {
    opt.metadata_format_identifier = metadata_format_identifier_opt;
    g_free(metadata_format_identifier_opt);
  }

  if (metadata_app_format_str &&
      !parse_hex_uint(metadata_app_format_str, &opt.metadata_app_format)) {
    std::cerr << "ERROR Invalid metadata-app-format: " << metadata_app_format_str << std::endl;
    free_meta_strings();
    return 1;
  }
  if (metadata_format_str && !parse_hex_uint(metadata_format_str, &opt.metadata_format)) {
    std::cerr << "ERROR Invalid metadata-format: " << metadata_format_str << std::endl;
    free_meta_strings();
    return 1;
  }
  if (metadata_service_id_str &&
      !parse_hex_uint(metadata_service_id_str, &opt.metadata_service_id)) {
    std::cerr << "ERROR Invalid metadata-service-id: " << metadata_service_id_str << std::endl;
    free_meta_strings();
    return 1;
  }
  if (metadata_flags_str && !parse_hex_uint(metadata_flags_str, &opt.metadata_flags)) {
    std::cerr << "ERROR Invalid metadata-flags: " << metadata_flags_str << std::endl;
    free_meta_strings();
    return 1;
  }

  if (opt.seed != 0)
    opt.seed_provided = true;

  std::string source_dir = default_source_dir();
  if (opt.tags_ini.empty())
    opt.tags_ini = join_path(source_dir, "data/stanag4609_tags.ini");
  if (opt.plugin_path.empty())
    opt.plugin_path = join_path(source_dir, "build");

  if (!g_getenv("GST_PLUGIN_PATH"))
    g_setenv("GST_PLUGIN_PATH", opt.plugin_path.c_str(), TRUE);
  if (!g_getenv("KLV_TAGS_INI"))
    g_setenv("KLV_TAGS_INI", opt.tags_ini.c_str(), TRUE);

  gst_init(&argc, &argv);
  bool supports_custom_mappings = mpegtsmux_supports_enable_custom_mappings();

  if (!g_file_test(opt.tags_ini.c_str(), G_FILE_TEST_EXISTS)) {
    std::cerr << "ERROR Tags INI not found: " << opt.tags_ini << std::endl;
    return 1;
  }

  GHashTable *defs = klv_load_tag_defs_from_ini(opt.tags_ini.c_str());
  if (!defs || g_hash_table_size(defs) == 0) {
    std::cerr << "ERROR Failed to load tag definitions" << std::endl;
    if (defs)
      g_hash_table_destroy(defs);
    return 1;
  }

  std::cout << "\n" << std::string(100, '=') << "\n";
  std::cout << "UDP SENDER (C++) - MISB ST 0601.8 KLV TAGS\n";
  std::cout << std::string(100, '=') << "\n\n";
  std::cout << "> Configuration:\n";
  std::cout << "  Destination Host: " << opt.host << "\n";
  std::cout << "  Port: " << opt.port << "\n";
  std::cout << "  Framerate: " << opt.fps << " fps\n";
  std::cout << "  Bitrate: " << opt.bitrate_kbps << " kbps\n";
  std::cout << "  Plugin path: " << opt.plugin_path << "\n";
  std::cout << "  Tags: " << opt.tags_ini << "\n";
  std::cout << "  mpegtsmux custom mappings: "
            << (supports_custom_mappings ? "enabled" : "disabled (not supported by this GST)")
            << "\n";
  std::cout << "  UDP: " << opt.host << ":" << opt.port << "\n\n";

  const int gop = std::max(2, opt.fps * 2);
  std::ostringstream pipeline_str;
  pipeline_str << "videotestsrc is-live=true ! videoconvert ! "
               << "video/x-raw,width=640,height=480,framerate=" << opt.fps << "/1 ! "
               << "x264enc name=enc bitrate=" << opt.bitrate_kbps
               << " speed-preset=ultrafast tune=zerolatency ! " << "h264parse name=vparse ! "
               << "video/x-h264,stream-format=byte-stream,alignment=au ! "
               << "klvframeinject name=inject " << "inject.video_src ! queue ! mpegtsmux name=mux"
               << (supports_custom_mappings ? " enable-custom-mappings=true" : "") << " ! "
               << "tspmtrewrite name=pmtrw ! " << "udpsink host=" << opt.host
               << " port=" << opt.port << " sync=false async=false "
               << "inject.klv_src ! queue ! "
                  "meta/x-klv,parsed=true,stream-format=klv,stream-type=(int)21 ! mux.";

  GstElement *pipeline = gst_parse_launch(pipeline_str.str().c_str(), &error);
  if (!pipeline || error) {
    std::cerr << "ERROR Failed to create pipeline: " << (error ? error->message : "unknown")
              << std::endl;
    g_clear_error(&error);
    if (pipeline)
      gst_object_unref(pipeline);
    g_hash_table_destroy(defs);
    free_meta_strings();
    return 1;
  }

  GstElement *inject = gst_bin_get_by_name(GST_BIN(pipeline), "inject");
  GstElement *pmtrw = gst_bin_get_by_name(GST_BIN(pipeline), "pmtrw");
  GstElement *mux = gst_bin_get_by_name(GST_BIN(pipeline), "mux");
  GstElement *enc = gst_bin_get_by_name(GST_BIN(pipeline), "enc");
  GstElement *vparse = gst_bin_get_by_name(GST_BIN(pipeline), "vparse");
  if (!inject || !pmtrw || !mux || !enc || !vparse) {
    std::cerr << "ERROR Failed to get elements from pipeline" << std::endl;
    if (inject)
      gst_object_unref(inject);
    if (pmtrw)
      gst_object_unref(pmtrw);
    if (mux)
      gst_object_unref(mux);
    if (enc)
      gst_object_unref(enc);
    if (vparse)
      gst_object_unref(vparse);
    gst_object_unref(pipeline);
    g_hash_table_destroy(defs);
    free_meta_strings();
    return 1;
  }

  if (has_property(mux, "alignment"))
    g_object_set(mux, "alignment", 7, nullptr);
  if (has_property(enc, "key-int-max"))
    g_object_set(enc, "key-int-max", gop, nullptr);
  if (has_property(enc, "bframes"))
    g_object_set(enc, "bframes", 0u, nullptr);
  if (has_property(enc, "option-string")) {
    std::ostringstream enc_opts;
    enc_opts << "keyint=" << gop << ":min-keyint=" << gop << ":scenecut=0:repeat-headers=1:aud=1";
    g_object_set(enc, "option-string", enc_opts.str().c_str(), nullptr);
  }
  if (has_property(enc, "aud"))
    g_object_set(enc, "aud", TRUE, nullptr);
  if (has_property(vparse, "config-interval"))
    g_object_set(vparse, "config-interval", -1, nullptr);

  g_object_set(pmtrw,
               "metadata-app-format",
               opt.metadata_app_format,
               "metadata-app-identifier",
               opt.metadata_app_identifier.c_str(),
               "metadata-format",
               opt.metadata_format,
               "metadata-format-identifier",
               opt.metadata_format_identifier.c_str(),
               "metadata-service-id",
               opt.metadata_service_id,
               "metadata-flags",
               opt.metadata_flags,
               nullptr);

  TagGenerator gen(opt.seed_provided ? opt.seed : 0);
  SenderState state;
  state.pipeline = pipeline;
  state.inject = inject;
  state.loop = g_main_loop_new(nullptr, FALSE);
  state.defs = defs;
  state.gen = &gen;
  state.sent = 0;
  state.max_count = opt.count;

  int first_count = 0;
  std::string first_json = build_tags_json(defs, gen, &first_count, nullptr);
  g_object_set(inject, "tags-json", first_json.c_str(), nullptr);

  GstBus *bus = gst_element_get_bus(pipeline);
  gst_bus_add_watch(bus, on_bus_message, state.loop);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  guint interval_ms = opt.fps > 0 ? static_cast<guint>(1000 / opt.fps) : 500;
  g_timeout_add(interval_ms, on_tick, &state);

  g_main_loop_run(state.loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_object_unref(inject);
  g_object_unref(pmtrw);
  g_object_unref(mux);
  g_object_unref(enc);
  g_object_unref(vparse);
  gst_object_unref(pipeline);
  g_main_loop_unref(state.loop);
  g_hash_table_destroy(defs);
  free_meta_strings();
  return 0;
}
