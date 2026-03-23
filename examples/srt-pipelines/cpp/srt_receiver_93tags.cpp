/**
 * @file examples/srt-pipelines/cpp/srt_receiver_93tags.cpp
 * @brief C++ SRT receiver that decodes MISB ST 0601.8 KLV tags.
 *
 * @ingroup gstklv_examples_cpp
 *
 * @details
 * This example listens for an SRT MPEG-TS stream, demuxes video and KLV,
 * decodes KLV JSON, and prints tag values with names and units sourced
 * from `data/stanag4609_tags.ini`.
 *
 * @section gstklv_receiver_pipeline Pipeline Topology
 * @dot
 * digraph receiver_pipeline {
 *   rankdir=LR;
 *   node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
 *   srtsrc -> tsdemux;
 *   tsdemux -> queue_video -> h264parse -> avdec_h264 -> videoconvert -> videosink;
 *   tsdemux -> queue_klv -> klvmetadec -> klv_fakesink;
 *   klv_fakesink [label="fakesink\n(signal-handoffs=TRUE)"];
 * }
 * @enddot
 *
 * @section gstklv_receiver_flow Execution Flow
 * 1. Load tag definitions from the INI registry.
 * 2. Build the pipeline graph and link dynamic pads from `tsdemux`.
 * 3. `klvmetadec` emits JSON on a fakesink handoff.
 * 4. JSON is parsed and printed with names and units.
 *
 * @note Use `--video-sink` to force a specific sink (e.g. `glimagesink`).
 */

#include <gst/gst.h>
#include <glib.h>

#include "gstklv/internal/klv_json.h"
#include "gstklv/internal/klv_tag_defs.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct Options
{
  /**
   * @brief Command line configuration for the receiver.
   *
   * @par Important options
   * - `host`: Host to connect.
   * - `port`: SRT port.
   * - `headless`: Use fakesink for video output.
   * - `video_sink`: Override the video sink element.
   * - `print_all`: Print all tags (otherwise a short subset).
   * - `output_path`: Optional JSON log file.
   */
  std::string host = "127.0.0.1";
  int port = 5000;
  gboolean headless = FALSE;
  gboolean print_all = TRUE;
  std::string video_sink;
  bool video_sink_forced = false;
  std::string output_path = "-";
  std::string tags_ini;
  std::string plugin_path;
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
has_property(GstElement *element, const char *name)
{
  return element && name &&
         g_object_class_find_property(G_OBJECT_GET_CLASS(element), name) != nullptr;
}

GstElement *
make_video_sink(const Options &opt)
{
  if (opt.headless) {
    return gst_element_factory_make("fakesink", "videosink");
  }

  if (opt.video_sink_forced && !opt.video_sink.empty()) {
    return gst_element_factory_make(opt.video_sink.c_str(), "videosink");
  }

  const char *preferred[] = {"xvimagesink", "ximagesink", "autovideosink"};
  for (const char *factory : preferred) {
    GstElement *sink = gst_element_factory_make(factory, "videosink");
    if (sink)
      return sink;
  }

  return nullptr;
}

std::string
element_factory_name(GstElement *element)
{
  if (!element)
    return {};
  GstElementFactory *factory = gst_element_get_factory(element);
  if (!factory)
    return {};
  const gchar *name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
  return name ? std::string(name) : std::string();
}

void
free_tags(KLVJsonTag *tags, gint count)
{
  for (gint i = 0; i < count; i++) {
    if (tags[i].is_raw && tags[i].raw) {
      g_free(tags[i].raw);
      tags[i].raw = nullptr;
    }
    if (tags[i].number_text) {
      g_free(tags[i].number_text);
      tags[i].number_text = nullptr;
    }
  }
}

std::string
bytes_preview(const guint8 *data, gsize len)
{
  std::ostringstream oss;
  oss << "bytes[" << len << "]: ";
  gsize preview = std::min<gsize>(len, 16);
  for (gsize i = 0; i < preview; i++) {
    oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
  }
  if (len > preview)
    oss << "...";
  return oss.str();
}

bool
is_string_tag(const KLVTagDef *td)
{
  if (!td || !td->type)
    return false;
  std::string type(td->type);
  return type.rfind("String", 0) == 0 || type.rfind("string", 0) == 0;
}

bool
is_bytes_tag(const KLVTagDef *td)
{
  if (!td || !td->type)
    return false;
  std::string type(td->type);
  return type.rfind("bytes", 0) == 0;
}

struct ReceiverState
{
  /// Tag registry for names and units.
  GHashTable *defs = nullptr;
  /// Frame counter for formatted output.
  int frame = 0;
  bool print_all = true;
  /// Optional JSON log file.
  std::ofstream output;
};

void
on_handoff(GstElement *, GstBuffer *buffer, GstPad *, gpointer user_data)
{
  ReceiverState *state = static_cast<ReceiverState *>(user_data);
  if (!state)
    return;

  GstMapInfo map;
  if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    return;

  state->frame++;
  std::string text(reinterpret_cast<char *>(map.data), map.size);
  gst_buffer_unmap(buffer, &map);

  if (state->output.is_open()) {
    state->output << text << "\n";
    state->output.flush();
  }

  KLVJsonTag tags[128];
  gint count = klv_json_parse_flat(text.c_str(), text.size(), tags, 128);
  if (count < 0) {
    std::cerr << "[FRAME " << std::setw(4) << std::setfill('0') << state->frame
              << "] Failed to parse JSON\n";
    return;
  }

  std::cout << "\n" << std::string(80, '=') << "\n";
  std::cout << "[FRAME " << std::setw(4) << std::setfill('0') << state->frame
            << "] Decoded tags: " << count << "\n";
  std::cout << std::string(80, '-') << "\n";

  std::vector<KLVJsonTag *> sorted;
  sorted.reserve(count);
  for (int i = 0; i < count; i++)
    sorted.push_back(&tags[i]);
  std::sort(sorted.begin(), sorted.end(), [](const KLVJsonTag *a, const KLVJsonTag *b) {
    return a->tag_id < b->tag_id;
  });

  for (const auto *tag : sorted) {
    int tag_id = tag->tag_id;
    KLVTagDef *td = state->defs
                      ? (KLVTagDef *)g_hash_table_lookup(state->defs, GINT_TO_POINTER(tag_id))
                      : nullptr;
    std::string name = td && td->name ? td->name : ("Tag " + std::to_string(tag_id));
    std::string units = (td && td->unit && td->unit[0]) ? std::string(" ") + td->unit : "";

    if (!state->print_all) {
      if (!(tag_id == 2 || tag_id == 5 || tag_id == 13 || tag_id == 14 || tag_id == 15))
        continue;
    }

    if (tag->is_raw) {
      if (is_string_tag(td)) {
        std::string value(reinterpret_cast<char *>(tag->raw), tag->raw_len);
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << value << units << "\n";
      }
      else if (is_bytes_tag(td)) {
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << bytes_preview(tag->raw, tag->raw_len) << "\n";
      }
      else {
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << bytes_preview(tag->raw, tag->raw_len) << "\n";
      }
    }
    else if ((tag_id == 2 || tag_id == 72) && tag->number_text) {
      std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                << tag->number_text << units << "\n";
    }
    else {
      std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                << std::setprecision(15) << tag->value << units << "\n";
    }
  }

  free_tags(tags, count);
}

void
on_pad_added(GstElement *demux, GstPad *pad, gpointer user_data)
{
  (void)demux;
  GstElement **queues = static_cast<GstElement **>(user_data);
  GstElement *queue_video = queues[0];
  GstElement *queue_klv = queues[1];

  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (!caps)
    caps = gst_pad_query_caps(pad, nullptr);
  if (!caps)
    return;

  const GstStructure *s = gst_caps_get_structure(caps, 0);
  const gchar *name = gst_structure_get_name(s);

  if (g_str_has_prefix(name, "video/")) {
    GstPad *sink = gst_element_get_static_pad(queue_video, "sink");
    if (sink && !gst_pad_is_linked(sink)) {
      GstPadLinkReturn ret = gst_pad_link(pad, sink);
      if (ret == GST_PAD_LINK_OK) {
        std::cout << "  Linked video pad: " << name << "\n";
      }
      else {
        std::cerr << "  WARNING Failed to link video pad (" << name
                  << "): " << gst_pad_link_get_name(ret) << "\n";
      }
    }
    if (sink)
      gst_object_unref(sink);
  }
  else if (g_str_has_prefix(name, "meta/")) {
    GstPad *sink = gst_element_get_static_pad(queue_klv, "sink");
    if (sink && !gst_pad_is_linked(sink)) {
      GstPadLinkReturn ret = gst_pad_link(pad, sink);
      if (ret == GST_PAD_LINK_OK) {
        std::cout << "  Linked KLV pad: " << name << "\n";
      }
      else {
        std::cerr << "  WARNING Failed to link KLV pad (" << name
                  << "): " << gst_pad_link_get_name(ret) << "\n";
      }
    }
    if (sink)
      gst_object_unref(sink);
  }

  gst_caps_unref(caps);
}

gboolean
on_bus_message(GstBus *bus, GstMessage *msg, gpointer user_data)
{
  (void)bus;
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

} // namespace

int
main(int argc, char **argv)
{
  /**
   * @brief Entry point for the C++ SRT receiver.
   *
   * Initializes GStreamer, builds the pipeline, and starts the main loop.
   * KLV tags are printed on each buffer handoff from the KLV fakesink.
   */
  Options opt;
  gchar *host_opt = nullptr;
  gchar *output_opt = nullptr;
  gchar *tags_ini_opt = nullptr;
  gchar *plugin_path_opt = nullptr;
  gchar *video_sink_opt = nullptr;

  GOptionEntry entries[] = {
    {"host",
     0,
     0,
     G_OPTION_ARG_STRING,
     &host_opt,
     (gchar *)"Host to connect",
     (gchar *)"127.0.0.1"},
    {"port", 0, 0, G_OPTION_ARG_INT, &opt.port, (gchar *)"Port to connect", (gchar *)"5000"},
    {"headless",
     0,
     0,
     G_OPTION_ARG_NONE,
     &opt.headless,
     (gchar *)"Use fakesink for video",
     nullptr},
    {"print-all", 0, 0, G_OPTION_ARG_NONE, &opt.print_all, (gchar *)"Print all tags", nullptr},
    {"video-sink",
     0,
     0,
     G_OPTION_ARG_STRING,
     &video_sink_opt,
     (gchar *)"Video sink element (default autovideosink)",
     nullptr},
    {"output",
     0,
     0,
     G_OPTION_ARG_STRING,
     &output_opt,
     (gchar *)"Write JSON output to file",
     (gchar *)"-"},
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
    {nullptr}};

  GOptionContext *ctx = g_option_context_new(" - C++ SRT receiver with MISB ST 0601.8 tags");
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

  if (host_opt) {
    opt.host = host_opt;
    g_free(host_opt);
  }
  if (output_opt) {
    opt.output_path = output_opt;
    g_free(output_opt);
  }
  if (tags_ini_opt) {
    opt.tags_ini = tags_ini_opt;
    g_free(tags_ini_opt);
  }
  if (plugin_path_opt) {
    opt.plugin_path = plugin_path_opt;
    g_free(plugin_path_opt);
  }
  if (video_sink_opt) {
    opt.video_sink = video_sink_opt;
    opt.video_sink_forced = true;
    g_free(video_sink_opt);
  }

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
  std::cout << "SRT RECEIVER (C++) - MISB ST 0601.8 KLV TAGS\n";
  std::cout << std::string(100, '=') << "\n\n";
  std::cout << "> Configuration:\n";
  std::cout << "  Host: " << opt.host << "\n";
  std::cout << "  Port: " << opt.port << "\n";
  std::cout << "  Plugin path: " << opt.plugin_path << "\n";
  std::cout << "  Tags: " << opt.tags_ini << "\n";
  std::cout << "  Output: " << opt.output_path << "\n";
  if (opt.headless) {
    std::cout << "  Video: headless (fakesink)\n";
  }
  std::cout << "\nNote: klvmetadec emits JSON; 2/5/13/14/15 are scaled to physical units.\n\n";

  GstElement *pipeline = gst_pipeline_new("srt-receiver");
  GstElement *srtsrc = gst_element_factory_make("srtsrc", "src");
  GstElement *demux = gst_element_factory_make("tsdemux", "demux");
  GstElement *queue_video = gst_element_factory_make("queue", "queue_video");
  GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse");
  GstElement *decoder = gst_element_factory_make("avdec_h264", "decoder");
  GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
  GstElement *videosink = make_video_sink(opt);
  GstElement *queue_klv = gst_element_factory_make("queue", "queue_klv");
  GstElement *klvmetadec = gst_element_factory_make("klvmetadec", "klvmetadec");
  GstElement *klvsink = gst_element_factory_make("fakesink", "klvsink");

  if (!pipeline || !srtsrc || !demux || !queue_video || !h264parse || !decoder || !videoconvert ||
      !videosink || !queue_klv || !klvmetadec || !klvsink) {
    std::cerr << "ERROR Failed to create elements" << std::endl;
    if (!videosink && !opt.headless) {
      std::cerr << "  Video sink could not be created. Try --video-sink=xvimagesink or "
                   "--video-sink=ximagesink."
                << std::endl;
    }
    if (pipeline)
      gst_object_unref(pipeline);
    g_hash_table_destroy(defs);
    return 1;
  }

  std::string uri = "srt://" + opt.host + ":" + std::to_string(opt.port) + "?mode=caller";
  g_object_set(srtsrc, "uri", uri.c_str(), "blocksize", 1316u, "latency", 125, nullptr);
  if (has_property(h264parse, "config-interval")) {
    g_object_set(h264parse, "config-interval", -1, nullptr);
  }
  if (has_property(videosink, "async")) {
    g_object_set(videosink, "async", FALSE, nullptr);
  }
  // Keep the displayed frame and the KLV handoff on the same clocked timeline.
  g_object_set(videosink, "sync", TRUE, nullptr);

  if (opt.headless) {
    std::cout << "  Video sink: fakesink\n";
  }
  else {
    std::string sink_name = element_factory_name(videosink);
    std::cout << "  Video sink: " << (sink_name.empty() ? "unknown" : sink_name) << "\n";
  }

  gst_bin_add_many(GST_BIN(pipeline),
                   srtsrc,
                   demux,
                   queue_video,
                   h264parse,
                   decoder,
                   videoconvert,
                   videosink,
                   queue_klv,
                   klvmetadec,
                   klvsink,
                   nullptr);

  gst_element_link(srtsrc, demux);
  gst_element_link(queue_video, h264parse);
  gst_element_link(h264parse, decoder);
  gst_element_link(decoder, videoconvert);
  gst_element_link(videoconvert, videosink);
  gst_element_link(queue_klv, klvmetadec);
  gst_element_link(klvmetadec, klvsink);

  GstElement *queues[2] = {queue_video, queue_klv};
  g_signal_connect(demux, "pad-added", G_CALLBACK(on_pad_added), queues);

  ReceiverState state;
  state.defs = defs;
  state.print_all = opt.print_all;
  if (opt.output_path != "-" && !opt.output_path.empty()) {
    state.output.open(opt.output_path, std::ios::out | std::ios::app);
  }

  g_object_set(klvsink, "signal-handoffs", TRUE, "sync", TRUE, nullptr);
  g_signal_connect(klvsink, "handoff", G_CALLBACK(on_handoff), &state);

  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
  GstBus *bus = gst_element_get_bus(pipeline);
  gst_bus_add_watch(bus, on_bus_message, loop);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  if (state.output.is_open())
    state.output.close();
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);
  g_hash_table_destroy(defs);
  return 0;
}
