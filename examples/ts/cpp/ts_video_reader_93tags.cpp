/**
 * @file examples/ts/cpp/ts_video_reader_93tags.cpp
 * @brief C++ MPEG-TS reader that decodes MISB ST 0601.8 KLV tags.
 *
 * @ingroup gstklv_examples_cpp
 *
 * @details
 * This example reads a recorded `.ts` file, plays back the H.264 video,
 * decodes KLV JSON with `klvmetadec`, and prints tag values with names and
 * units sourced from `data/stanag4609_tags.ini`.
 *
 * @section gstklv_ts_reader_pipeline Pipeline Topology
 * @dot
 * digraph ts_reader_pipeline {
 *   rankdir=LR;
 *   node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
 *   filesrc -> tsdemux;
 *   tsdemux -> queue_video -> h264parse -> avdec_h264 -> videoconvert -> videosink;
 *   tsdemux -> queue_klv -> klvraw_sink;
 *   klvraw_sink [label="fakesink\n(signal-handoffs=TRUE)"];
 *   appsrc -> klvmetadec -> klvjson_sink;
 *   klvjson_sink [label="fakesink\n(signal-handoffs=TRUE)"];
 * }
 * @enddot
 */

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <glib.h>

#include "gstklv/internal/klv_json.h"
#include "gstklv/internal/klv_tag_defs.h"
#include "gstklv/internal/ts_psi.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct Options
{
  /**
   * @brief Command line configuration for the TS reader.
   *
   * @par Key options
   * - `input`: Path to the `.ts` file.
   * - `video_sink`: GStreamer video sink element.
   * - `video_decoder`: H.264 decoder element.
   * - `print_all`: Print all tags or a short subset.
   * - `pace`: PTS-based pacing for KLV output.
   */
  std::string input;
  gboolean headless = FALSE;
  gboolean print_all = TRUE;
  std::string video_sink = "autovideosink";
  std::string video_decoder = "avdec_h264";
  std::string output_path = "-";
  bool pace = true;
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

void
free_tags(KLVJsonTag *tags, gint count)
{
  for (gint i = 0; i < count; i++) {
    if (tags[i].is_raw && tags[i].raw) {
      g_free(tags[i].raw);
      tags[i].raw = nullptr;
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

struct ReaderState
{
  /// Tag registry for names and units.
  GHashTable *defs = nullptr;
  /// Frame counter for formatted output.
  int frame = 0;
  /// Print all tags or a short subset.
  bool print_all = true;
  /// Optional JSON output file.
  std::ofstream output;
  /// appsrc feeding klvmetadec.
  GstElement *appsrc = nullptr;
  /// PTS for the next assembled KLV.
  GstClockTime pending_pts = GST_CLOCK_TIME_NONE;
  /// Accumulator for KLV bytes across buffers.
  std::vector<guint8> klv_buffer;
  /// Worker stop flag.
  bool stop = false;
  /// First PTS observed.
  GstClockTime first_pts = GST_CLOCK_TIME_NONE;
  /// Origin initialized flag.
  bool have_origin = false;
  /// Queue mutex.
  std::mutex mutex;
  /// Queue condition variable.
  std::condition_variable cv;
  /// Queue of (PTS, KLV payload).
  std::deque<std::pair<GstClockTime, std::vector<guint8>>> queue;
};

static const guint8 kKlvUl[16] = {
  0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00};
static const guint8 kKlvUlSuffix[11] = {
  0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00};

struct PesPayload
{
  bool has_pts = false;
  guint64 pts = 0;
  std::vector<guint8> payload;
};

bool
wait_for_pipeline_clock(GstElement *pipeline,
                        GstClock **clock_out,
                        GstClockTime *base_time_out,
                        guint timeout_ms = 5000)
{
  if (clock_out)
    *clock_out = nullptr;
  if (base_time_out)
    *base_time_out = GST_CLOCK_TIME_NONE;
  if (!pipeline)
    return false;

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    GstClock *clock = gst_element_get_clock(pipeline);
    GstClockTime base_time = gst_element_get_base_time(pipeline);
    if (clock && GST_CLOCK_TIME_IS_VALID(base_time) && base_time != 0) {
      if (clock_out)
        *clock_out = clock;
      if (base_time_out)
        *base_time_out = base_time;
      return true;
    }
    if (clock)
      gst_object_unref(clock);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  GstClock *clock = gst_element_get_clock(pipeline);
  GstClockTime base_time = gst_element_get_base_time(pipeline);
  if (clock_out)
    *clock_out = clock;
  else if (clock)
    gst_object_unref(clock);
  if (base_time_out)
    *base_time_out = base_time;
  return clock && GST_CLOCK_TIME_IS_VALID(base_time) && base_time != 0;
}

bool
has_klva_descriptor(const GPtrArray *descriptors)
{
  if (!descriptors)
    return false;
  for (guint i = 0; i < descriptors->len; i++) {
    auto *desc =
      static_cast<TsDescriptor *>(g_ptr_array_index(const_cast<GPtrArray *>(descriptors), i));
    if (!desc || desc->tag != 0x05 || !desc->payload || desc->payload->len < 4)
      continue;
    if (std::memcmp(desc->payload->data, "KLVA", 4) == 0)
      return true;
  }
  return false;
}

bool
find_klv_pid(const std::string &path, guint16 *out_pid)
{
  gchar *contents = nullptr;
  gsize len = 0;
  if (!g_file_get_contents(path.c_str(), &contents, &len, nullptr))
    return false;

  const auto *data = reinterpret_cast<const guint8 *>(contents);
  guint16 pmt_pid = 0;
  bool have_pat = false;
  for (gsize off = 0; off + 188 <= len; off += 188) {
    const guint8 *pkt = data + off;
    if (pkt[0] != 0x47)
      continue;
    guint16 pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
    if (pid != 0x0000)
      continue;
    const guint8 *section = nullptr;
    gsize section_len = 0;
    if (ts_extract_section_from_packet(pkt, 188, 0x00, &section, &section_len) &&
        ts_parse_pat_section(section, section_len, &pmt_pid)) {
      have_pat = true;
      break;
    }
  }

  if (!have_pat) {
    g_free(contents);
    return false;
  }

  bool found = false;
  guint16 fallback_pid = 0;
  bool have_fallback = false;
  for (gsize off = 0; off + 188 <= len; off += 188) {
    const guint8 *pkt = data + off;
    if (pkt[0] != 0x47)
      continue;
    guint16 pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
    if (pid != pmt_pid)
      continue;
    const guint8 *section = nullptr;
    gsize section_len = 0;
    if (!ts_extract_section_from_packet(pkt, 188, 0x02, &section, &section_len))
      continue;
    TsPmtInfo pmt;
    ts_pmt_info_init(&pmt);
    if (!ts_parse_pmt_section(section, section_len, &pmt)) {
      ts_pmt_info_clear(&pmt);
      continue;
    }
    for (guint i = 0; i < pmt.streams->len; i++) {
      auto *stream = static_cast<TsStreamInfo *>(g_ptr_array_index(pmt.streams, i));
      if (!stream)
        continue;
      if (stream->stream_type == 0x15) {
        *out_pid = stream->pid;
        found = true;
        break;
      }
      if (stream->stream_type == 0x06) {
        if (has_klva_descriptor(stream->descriptors)) {
          *out_pid = stream->pid;
          found = true;
          break;
        }
        if (!have_fallback) {
          fallback_pid = stream->pid;
          have_fallback = true;
        }
      }
    }
    ts_pmt_info_clear(&pmt);
    if (found)
      break;
  }

  if (!found && have_fallback) {
    *out_pid = fallback_pid;
    found = true;
  }

  g_free(contents);
  return found;
}

guint64
parse_pts(const guint8 *pts_bytes)
{
  guint64 pts = ((pts_bytes[0] >> 1) & 0x07) << 30;
  pts |= (static_cast<guint64>(((pts_bytes[1] << 8) | pts_bytes[2]) >> 1) << 15);
  pts |= static_cast<guint64>(((pts_bytes[3] << 8) | pts_bytes[4]) >> 1);
  return pts;
}

bool
extract_pes_payload(const std::vector<guint8> &pes, PesPayload *out)
{
  if (!out || pes.size() < 9)
    return false;
  if (!(pes[0] == 0x00 && pes[1] == 0x00 && pes[2] == 0x01))
    return false;

  guint8 flags = pes[7];
  guint8 header_len = pes[8];
  gsize payload_start = 9 + header_len;
  if (payload_start > pes.size())
    return false;

  out->has_pts = false;
  out->pts = 0;
  if ((flags & 0x80) && pes.size() >= 14) {
    out->has_pts = true;
    out->pts = parse_pts(pes.data() + 9);
  }
  out->payload.assign(pes.begin() + payload_start, pes.end());
  return !out->payload.empty();
}

std::vector<PesPayload>
load_klv_pes_payloads(const std::string &path, guint16 pid)
{
  std::vector<PesPayload> out;
  gchar *contents = nullptr;
  gsize len = 0;
  if (!g_file_get_contents(path.c_str(), &contents, &len, nullptr))
    return out;

  const auto *data = reinterpret_cast<const guint8 *>(contents);
  std::vector<guint8> current;
  for (gsize off = 0; off + 188 <= len; off += 188) {
    const guint8 *pkt = data + off;
    if (pkt[0] != 0x47)
      continue;

    bool payload_unit_start = (pkt[1] & 0x40) != 0;
    guint16 packet_pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
    if (packet_pid != pid)
      continue;

    guint8 afc = (pkt[3] >> 4) & 0x03;
    gsize offset = 4;
    if (afc == 2 || afc == 3) {
      if (offset >= 188)
        continue;
      guint8 afl = pkt[offset];
      offset += 1 + afl;
    }
    if ((afc != 1 && afc != 3) || offset >= 188)
      continue;

    const guint8 *payload = pkt + offset;
    gsize payload_len = 188 - offset;
    if (payload_unit_start && !current.empty()) {
      PesPayload item;
      if (extract_pes_payload(current, &item))
        out.push_back(std::move(item));
      current.clear();
    }
    current.insert(current.end(), payload, payload + payload_len);
  }

  if (!current.empty()) {
    PesPayload item;
    if (extract_pes_payload(current, &item))
      out.push_back(std::move(item));
  }

  g_free(contents);
  return out;
}

std::vector<guint8>
normalize_klv_payload(const std::vector<guint8> &payload)
{
  if (payload.size() >= sizeof(kKlvUl) &&
      std::memcmp(payload.data(), kKlvUl, sizeof(kKlvUl)) == 0) {
    return payload;
  }
  if (payload.size() >= sizeof(kKlvUlSuffix) &&
      std::memcmp(payload.data(), kKlvUlSuffix, sizeof(kKlvUlSuffix)) == 0) {
    std::vector<guint8> fixed;
    fixed.insert(fixed.end(), std::begin(kKlvUl), std::begin(kKlvUl) + 5);
    fixed.insert(fixed.end(), payload.begin(), payload.end());
    return fixed;
  }
  return payload;
}

struct PadLinkData
{
  /// Video queue for tsdemux video pad.
  GstElement *queue_video;
  /// KLV queue for tsdemux metadata pad.
  GstElement *queue_klv;
};

/**
 * @brief Link dynamic tsdemux pads to video/KLV queues.
 */
void
on_demux_pad_added(GstElement *, GstPad *pad, gpointer user_data)
{
  auto *pipes = static_cast<PadLinkData *>(user_data);
  GstElement *queue_video = pipes ? pipes->queue_video : nullptr;
  GstElement *queue_klv = pipes ? pipes->queue_klv : nullptr;
  if (!queue_video || !queue_klv)
    return;

  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (!caps)
    caps = gst_pad_query_caps(pad, nullptr);
  if (!caps)
    return;
  GstStructure *s = gst_caps_get_structure(caps, 0);
  const gchar *name = gst_structure_get_name(s);
  if (g_str_has_prefix(name, "video/")) {
    GstPad *sink_pad = gst_element_get_static_pad(queue_video, "sink");
    if (!gst_pad_is_linked(sink_pad)) {
      if (gst_pad_link(pad, sink_pad) == GST_PAD_LINK_OK)
        std::cout << "  Linked video pad: " << name << "\n";
    }
    gst_object_unref(sink_pad);
  }
  else if (g_str_has_prefix(name, "meta/")) {
    GstPad *sink_pad = gst_element_get_static_pad(queue_klv, "sink");
    if (!gst_pad_is_linked(sink_pad)) {
      if (gst_pad_link(pad, sink_pad) == GST_PAD_LINK_OK)
        std::cout << "  Linked KLV pad: " << name << "\n";
    }
    gst_object_unref(sink_pad);
  }
  gst_caps_unref(caps);
}

/**
 * @brief Link decodebin's dynamic pad to videoconvert.
 */
void
on_decoder_pad_added(GstElement *, GstPad *pad, gpointer user_data)
{
  GstElement *convert = static_cast<GstElement *>(user_data);
  if (!convert)
    return;
  GstPad *sink_pad = gst_element_get_static_pad(convert, "sink");
  if (!gst_pad_is_linked(sink_pad))
    gst_pad_link(pad, sink_pad);
  gst_object_unref(sink_pad);
}

/**
 * @brief Handle JSON output from klvmetadec and print tags.
 */
void
on_klv_json_handoff(GstElement *, GstBuffer *buffer, GstPad *, gpointer user_data)
{
  ReaderState *state = static_cast<ReaderState *>(user_data);
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
    if (!state->print_all) {
      if (!(tag_id == 2 || tag_id == 5 || tag_id == 13 || tag_id == 14 || tag_id == 15))
        continue;
    }

    KLVTagDef *td = state->defs
                      ? (KLVTagDef *)g_hash_table_lookup(state->defs, GINT_TO_POINTER(tag_id))
                      : nullptr;
    std::string name = td && td->name ? td->name : ("Tag " + std::to_string(tag_id));
    std::string units = (td && td->unit && td->unit[0]) ? std::string(" ") + td->unit : "";

    if (tag->is_raw) {
      if (is_string_tag(td)) {
        std::string value(reinterpret_cast<char *>(tag->raw), tag->raw_len);
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << value << units << "\n";
      }
      else if (is_bytes_tag(td)) {
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << bytes_preview(tag->raw, tag->raw_len) << units << "\n";
      }
      else {
        std::string raw(reinterpret_cast<char *>(tag->raw), tag->raw_len);
        std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                  << raw << units << "\n";
      }
    }
    else {
      std::cout << "  " << std::setw(2) << std::setfill('0') << tag_id << " " << name << ": "
                << std::setprecision(15) << tag->value << units << "\n";
    }
  }

  std::cout << std::string(80, '=') << "\n";
  free_tags(tags, count);
}

/**
 * @brief Consume raw KLV buffers and assemble complete KLV packets.
 *
 * The function restores the missing UL prefix if `tsdemux` trimmed it,
 * then parses the BER length to push complete KLV packets into the queue.
 */
void
on_klv_raw_handoff(GstElement *, GstBuffer *buffer, GstPad *, gpointer user_data)
{
  ReaderState *state = static_cast<ReaderState *>(user_data);
  if (!state || !state->appsrc)
    return;

  GstMapInfo map;
  if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    return;

  const guint8 *data = map.data;
  gsize size = map.size;

  std::vector<guint8> chunk;
  chunk.reserve(size + 5);
  if (size >= sizeof(kKlvUl) && std::memcmp(data, kKlvUl, sizeof(kKlvUl)) == 0) {
    chunk.insert(chunk.end(), data, data + size);
  }
  else if (size >= (sizeof(kKlvUl) - 5) && std::memcmp(data, kKlvUl + 5, sizeof(kKlvUl) - 5) == 0) {
    chunk.insert(chunk.end(), kKlvUl, kKlvUl + 5);
    chunk.insert(chunk.end(), data, data + size);
  }
  else if (size >= sizeof(kKlvUlSuffix) &&
           std::memcmp(data, kKlvUlSuffix, sizeof(kKlvUlSuffix)) == 0) {
    chunk.insert(chunk.end(), kKlvUl, kKlvUl + 5);
    chunk.insert(chunk.end(), data, data + size);
  }
  else {
    chunk.insert(chunk.end(), data, data + size);
  }

  gst_buffer_unmap(buffer, &map);

  if (state->klv_buffer.empty()) {
    state->pending_pts = GST_BUFFER_PTS(buffer);
  }

  state->klv_buffer.insert(state->klv_buffer.end(), chunk.begin(), chunk.end());

  auto find_ul = [&](const std::vector<guint8> &buf) -> size_t {
    if (buf.size() < sizeof(kKlvUl))
      return std::string::npos;
    auto it = std::search(buf.begin(), buf.end(), std::begin(kKlvUl), std::end(kKlvUl));
    if (it != buf.end())
      return static_cast<size_t>(std::distance(buf.begin(), it));
    return std::string::npos;
  };

  while (true) {
    size_t ul_pos = find_ul(state->klv_buffer);
    if (ul_pos == std::string::npos) {
      if (state->klv_buffer.size() > sizeof(kKlvUl)) {
        state->klv_buffer.erase(state->klv_buffer.begin(),
                                state->klv_buffer.end() - sizeof(kKlvUl));
      }
      break;
    }

    if (ul_pos > 0) {
      state->klv_buffer.erase(state->klv_buffer.begin(), state->klv_buffer.begin() + ul_pos);
      state->pending_pts = GST_BUFFER_PTS(buffer);
    }

    if (state->klv_buffer.size() < sizeof(kKlvUl) + 1)
      break;

    size_t len_index = sizeof(kKlvUl);
    guint8 len_byte = state->klv_buffer[len_index];
    size_t len_len = 0;
    size_t klv_len = 0;
    if ((len_byte & 0x80) == 0) {
      len_len = 1;
      klv_len = len_byte;
    }
    else {
      len_len = len_byte & 0x7F;
      if (state->klv_buffer.size() < sizeof(kKlvUl) + 1 + len_len)
        break;
      klv_len = 0;
      for (size_t i = 0; i < len_len; i++) {
        klv_len = (klv_len << 8) | state->klv_buffer[len_index + 1 + i];
      }
      len_len = 1 + len_len;
    }

    size_t total_len = sizeof(kKlvUl) + len_len + klv_len;
    if (state->klv_buffer.size() < total_len)
      break;

    std::vector<guint8> frame(state->klv_buffer.begin(), state->klv_buffer.begin() + total_len);
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->queue.emplace_back(state->pending_pts, std::move(frame));
    }
    state->cv.notify_one();

    state->klv_buffer.erase(state->klv_buffer.begin(), state->klv_buffer.begin() + total_len);
  }
}

} // namespace

int
main(int argc, char **argv)
{
  Options opt;
  gchar *input_opt = nullptr;
  gchar *tags_ini_opt = nullptr;
  gchar *plugin_path_opt = nullptr;
  gchar *video_sink_opt = nullptr;
  gchar *video_decoder_opt = nullptr;
  gchar *output_opt = nullptr;
  gboolean print_summary = FALSE;
  gboolean no_pace = FALSE;

  GOptionEntry entries[] = {
    {"input", 0, 0, G_OPTION_ARG_STRING, &input_opt, (gchar *)"Input .ts file", nullptr},
    {"headless", 0, 0, G_OPTION_ARG_NONE, &opt.headless, (gchar *)"Disable video output", nullptr},
    {"video-sink",
     0,
     0,
     G_OPTION_ARG_STRING,
     &video_sink_opt,
     (gchar *)"Video sink element",
     (gchar *)"autovideosink"},
    {"video-decoder",
     0,
     0,
     G_OPTION_ARG_STRING,
     &video_decoder_opt,
     (gchar *)"Video decoder element",
     (gchar *)"avdec_h264"},
    {"print-all", 0, 0, G_OPTION_ARG_NONE, &opt.print_all, (gchar *)"Print all tags", nullptr},
    {"print-summary",
     0,
     0,
     G_OPTION_ARG_NONE,
     &print_summary,
     (gchar *)"Only print tags 13/14/15",
     nullptr},
    {"no-pace",
     0,
     0,
     G_OPTION_ARG_NONE,
     &no_pace,
     (gchar *)"Disable pacing (dump KLV fast)",
     nullptr},
    {"output",
     0,
     0,
     G_OPTION_ARG_STRING,
     &output_opt,
     (gchar *)"Append raw JSON to file",
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

  GOptionContext *ctx = g_option_context_new(" - C++ TS reader with MISB ST 0601.8 tags");
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

  if (input_opt) {
    opt.input = input_opt;
    g_free(input_opt);
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
    g_free(video_sink_opt);
  }
  if (video_decoder_opt) {
    opt.video_decoder = video_decoder_opt;
    g_free(video_decoder_opt);
  }
  if (output_opt) {
    opt.output_path = output_opt;
    g_free(output_opt);
  }
  if (print_summary)
    opt.print_all = false;
  if (no_pace)
    opt.pace = false;

  if (opt.input.empty()) {
    std::cerr << "ERROR Input .ts file is required (use --input)" << std::endl;
    return 1;
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
  if (!g_file_test(opt.input.c_str(), G_FILE_TEST_EXISTS)) {
    std::cerr << "ERROR Input TS not found: " << opt.input << std::endl;
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
  std::cout << "TS VIDEO READER (C++) - MISB ST 0601.8 KLV TAGS\n";
  std::cout << std::string(100, '=') << "\n\n";
  std::cout << "> Configuration:\n";
  std::cout << "  Input: " << opt.input << "\n";
  std::cout << "  Plugin path: " << opt.plugin_path << "\n";
  std::cout << "  Tags: " << opt.tags_ini << "\n";
  std::cout << "  Output: " << opt.output_path << "\n";
  if (opt.headless) {
    std::cout << "  Video: headless (fakesink)\n";
  }
  else {
    std::cout << "  Video sink: " << opt.video_sink << "\n";
    std::cout << "  Video decoder: " << opt.video_decoder << "\n";
  }
  std::cout << "\nNote: klvmetadec emits JSON; 2/5/13/14/15 are scaled to physical units.\n\n";

  GstElement *pipeline = gst_pipeline_new("ts-video-reader");
  GstElement *filesrc = gst_element_factory_make("filesrc", "src");
  GstElement *demux = gst_element_factory_make("tsdemux", "demux");
  GstElement *queue_video = gst_element_factory_make("queue", "queue_video");
  GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse");
  GstElement *decoder = gst_element_factory_make(opt.video_decoder.c_str(), "decoder");
  bool decoder_is_decodebin = false;
  if (!decoder) {
    decoder = gst_element_factory_make("decodebin", "decoder");
    decoder_is_decodebin = true;
    std::cerr << "WARNING Requested decoder not found, falling back to decodebin\n";
  }
  GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
  GstElement *videosink =
    gst_element_factory_make(opt.headless ? "fakesink" : opt.video_sink.c_str(), "videosink");
  GstElement *queue_klv = gst_element_factory_make("queue", "queue_klv");
  GstElement *klvraw = gst_element_factory_make("fakesink", "klvraw");

  if (!filesrc || !demux || !queue_video || !h264parse || !decoder || !videoconvert || !videosink ||
      !queue_klv || !klvraw) {
    std::cerr << "ERROR Failed to create elements for video pipeline" << std::endl;
    if (pipeline)
      gst_object_unref(pipeline);
    g_hash_table_destroy(defs);
    return 1;
  }

  g_object_set(filesrc, "location", opt.input.c_str(), nullptr);
  g_object_set(klvraw, "signal-handoffs", FALSE, "sync", FALSE, nullptr);
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(videosink), "sync")) {
    g_object_set(videosink, "sync", opt.pace ? TRUE : FALSE, nullptr);
  }

  gst_bin_add_many(GST_BIN(pipeline),
                   filesrc,
                   demux,
                   queue_video,
                   h264parse,
                   decoder,
                   videoconvert,
                   videosink,
                   queue_klv,
                   klvraw,
                   nullptr);
  gst_element_link(filesrc, demux);
  gst_element_link(queue_video, h264parse);
  if (!decoder_is_decodebin) {
    gst_element_link(h264parse, decoder);
    gst_element_link(decoder, videoconvert);
    gst_element_link(videoconvert, videosink);
  }
  else {
    gst_element_link(h264parse, decoder);
  }
  gst_element_link(queue_klv, klvraw);

  if (decoder_is_decodebin) {
    g_signal_connect(decoder, "pad-added", G_CALLBACK(on_decoder_pad_added), videoconvert);
    gst_element_link(videoconvert, videosink);
  }

  PadLinkData pad_link_data{queue_video, queue_klv};
  g_signal_connect(demux, "pad-added", G_CALLBACK(on_demux_pad_added), &pad_link_data);

  GstElement *klv_pipeline = gst_pipeline_new("klv-decode-pipeline");
  GstElement *appsrc = gst_element_factory_make("appsrc", "klvsrc");
  GstElement *klvmetadec = gst_element_factory_make("klvmetadec", "klvmetadec");
  GstElement *klvjson = gst_element_factory_make("fakesink", "klvjson");
  if (!klv_pipeline || !appsrc || !klvmetadec || !klvjson) {
    std::cerr << "ERROR Failed to create KLV decode pipeline" << std::endl;
    gst_object_unref(pipeline);
    g_hash_table_destroy(defs);
    return 1;
  }

  GstCaps *klv_caps = gst_caps_from_string("meta/x-klv");
  g_object_set(appsrc, "caps", klv_caps, "format", GST_FORMAT_TIME, "is-live", FALSE, nullptr);
  gst_caps_unref(klv_caps);
  g_object_set(klvjson, "signal-handoffs", TRUE, "sync", FALSE, nullptr);

  gst_bin_add_many(GST_BIN(klv_pipeline), appsrc, klvmetadec, klvjson, nullptr);
  gst_element_link(appsrc, klvmetadec);
  gst_element_link(klvmetadec, klvjson);

  ReaderState state;
  state.defs = defs;
  state.print_all = opt.print_all;
  state.appsrc = appsrc;
  if (opt.output_path != "-")
    state.output.open(opt.output_path);

  g_signal_connect(klvjson, "handoff", G_CALLBACK(on_klv_json_handoff), &state);

  guint16 klv_pid = 0;
  bool have_klv_pid = find_klv_pid(opt.input, &klv_pid);
  if (!have_klv_pid) {
    std::cerr << "WARNING No KLV PID found in PMT; KLV output will be empty.\n";
  }

  GstBus *bus = gst_element_get_bus(pipeline);
  GstBus *klv_bus = gst_element_get_bus(klv_pipeline);
  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);

  struct BusContext
  {
    GMainLoop *loop;
    bool video_eos = false;
    bool klv_eos = false;
    bool error = false;
  } bus_ctx{loop};

  gst_bus_add_watch(
    bus,
    [](GstBus *, GstMessage *msg, gpointer user_data) -> gboolean {
      auto *ctx = static_cast<BusContext *>(user_data);
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
          ctx->error = true;
          g_main_loop_quit(ctx->loop);
          break;
        }
        case GST_MESSAGE_EOS:
          ctx->video_eos = true;
          if (ctx->klv_eos)
            g_main_loop_quit(ctx->loop);
          break;
        default: break;
      }
      return TRUE;
    },
    &bus_ctx);

  gst_bus_add_watch(
    klv_bus,
    [](GstBus *, GstMessage *msg, gpointer user_data) -> gboolean {
      auto *ctx = static_cast<BusContext *>(user_data);
      if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err = nullptr;
        gchar *dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        std::cerr << "ERROR (KLV): " << (err ? err->message : "unknown") << std::endl;
        if (dbg)
          std::cerr << "DEBUG: " << dbg << std::endl;
        g_clear_error(&err);
        g_free(dbg);
        ctx->error = true;
        g_main_loop_quit(ctx->loop);
      }
      else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        ctx->klv_eos = true;
        if (ctx->video_eos)
          g_main_loop_quit(ctx->loop);
      }
      return TRUE;
    },
    &bus_ctx);

  gst_object_unref(bus);
  gst_object_unref(klv_bus);

  gst_element_set_state(klv_pipeline, GST_STATE_PLAYING);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  std::thread klv_feeder([&state, &opt, &have_klv_pid, klv_pid, appsrc, pipeline]() {
    if (!have_klv_pid) {
      gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
      return;
    }

    auto items = load_klv_pes_payloads(opt.input, klv_pid);
    GstClock *clock = nullptr;
    GstClockTime base_time = GST_CLOCK_TIME_NONE;
    if (opt.pace && !wait_for_pipeline_clock(pipeline, &clock, &base_time)) {
      std::cerr << "WARNING Video pipeline clock unavailable; falling back to local pacing.\n";
    }

    auto fallback_start = std::chrono::steady_clock::now();
    bool have_origin = false;
    guint64 first_pts = 0;

    for (const auto &item : items) {
      {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.stop)
          break;
      }

      GstClockTime pts_ns = GST_CLOCK_TIME_NONE;
      if (item.has_pts) {
        if (!have_origin) {
          first_pts = item.pts;
          have_origin = true;
        }
        pts_ns = static_cast<GstClockTime>((item.pts - first_pts) * (1000000000.0 / 90000.0));

        if (opt.pace) {
          if (clock && GST_CLOCK_TIME_IS_VALID(base_time) && base_time != 0) {
            GstClockTime target = base_time + pts_ns;
            while (true) {
              {
                std::lock_guard<std::mutex> lock(state.mutex);
                if (state.stop)
                  break;
              }
              GstClockTime now = gst_clock_get_time(clock);
              if (now >= target)
                break;
              GstClockTime remaining = target - now;
              auto sleep_for =
                std::chrono::nanoseconds(std::min<GstClockTime>(remaining, 50 * GST_MSECOND));
              std::this_thread::sleep_for(sleep_for);
            }
          }
          else {
            auto target = fallback_start + std::chrono::nanoseconds(pts_ns);
            std::this_thread::sleep_until(target);
          }
        }
      }

      auto fixed = normalize_klv_payload(item.payload);
      GstBuffer *out = gst_buffer_new_allocate(nullptr, fixed.size(), nullptr);
      gst_buffer_fill(out, 0, fixed.data(), fixed.size());
      if (pts_ns != GST_CLOCK_TIME_NONE) {
        GST_BUFFER_PTS(out) = pts_ns;
        GST_BUFFER_DTS(out) = pts_ns;
      }
      GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(appsrc), out);
      if (flow != GST_FLOW_OK) {
        std::cerr << "WARNING KLV appsrc push failed: " << gst_flow_get_name(flow) << "\n";
        break;
      }
    }

    if (clock)
      gst_object_unref(clock);
    gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
  });

  g_main_loop_run(loop);

  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.stop = true;
  }
  if (klv_feeder.joinable())
    klv_feeder.join();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_element_set_state(klv_pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_object_unref(klv_pipeline);
  g_main_loop_unref(loop);

  if (state.output.is_open())
    state.output.close();
  g_hash_table_destroy(defs);

  std::cout << "\nOK Playback complete (" << state.frame << " frames)\n";
  return 0;
}
