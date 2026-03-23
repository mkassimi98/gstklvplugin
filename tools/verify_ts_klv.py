#!/usr/bin/env python3
"""
@file tools/verify_ts_klv.py
@brief Verify MPEG-TS signalling for KLV metadata streams.
@ingroup gstklv_tools

This utility inspects PAT/PMT data inside a transport stream and reports the
KLV signalling used by the metadata PID.

Checks:
- PMT KLV signaling (`stream_type 0x15`, or current `0x06 + KLVA`)
- registration_descriptor (0x05) format identifier (e.g. KLVA)
- metadata_descriptor (0x26) presence

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
from dataclasses import dataclass
from pathlib import Path


TS_PACKET_SIZE = 188


@dataclass
class StreamInfo:
    pid: int
    stream_type: int
    descriptors: list


def iter_ts_packets(data: bytes):
    for i in range(0, len(data) - TS_PACKET_SIZE + 1, TS_PACKET_SIZE):
        pkt = data[i : i + TS_PACKET_SIZE]
        if pkt[0] != 0x47:
            continue
        yield pkt


def parse_descriptors(data: bytes):
    descriptors = []
    i = 0
    while i + 1 < len(data):
        tag = data[i]
        length = data[i + 1]
        payload = data[i + 2 : i + 2 + length]
        descriptors.append((tag, payload))
        i += 2 + length
    return descriptors


def parse_pat(section: bytes):
    if not section or section[0] != 0x00:
        return None
    section_length = ((section[1] & 0x0F) << 8) | section[2]
    end = 3 + section_length - 4
    pos = 8
    pmt_pid = None
    while pos + 4 <= end:
        program_number = (section[pos] << 8) | section[pos + 1]
        pid = ((section[pos + 2] & 0x1F) << 8) | section[pos + 3]
        if program_number != 0:
            pmt_pid = pid
            break
        pos += 4
    return pmt_pid


def parse_pmt(section: bytes):
    if not section or section[0] != 0x02:
        return []
    section_length = ((section[1] & 0x0F) << 8) | section[2]
    end = 3 + section_length - 4
    if end > len(section):
        end = len(section)
    program_info_length = ((section[10] & 0x0F) << 8) | section[11]
    pos = 12 + program_info_length
    streams = []
    while pos + 5 <= end:
        stream_type = section[pos]
        pid = ((section[pos + 1] & 0x1F) << 8) | section[pos + 2]
        es_info_length = ((section[pos + 3] & 0x0F) << 8) | section[pos + 4]
        desc_start = pos + 5
        desc_end = desc_start + es_info_length
        desc_bytes = section[desc_start:desc_end]
        descriptors = parse_descriptors(desc_bytes)
        streams.append(StreamInfo(pid=pid, stream_type=stream_type, descriptors=descriptors))
        pos = desc_end
    return streams


def extract_sections(data: bytes):
    sections = {}
    buffers = {}

    for pkt in iter_ts_packets(data):
        payload_unit_start = (pkt[1] & 0x40) != 0
        pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
        afc = (pkt[3] >> 4) & 0x03
        offset = 4

        if afc in (2, 3):
            if offset >= TS_PACKET_SIZE:
                continue
            afl = pkt[offset]
            offset += 1 + afl

        if afc not in (1, 3) or offset >= TS_PACKET_SIZE:
            continue

        payload = pkt[offset:]
        if not payload:
            continue

        if payload_unit_start:
            pointer = payload[0]
            payload = payload[1 + pointer :]
            buffers[pid] = bytearray()

        buf = buffers.setdefault(pid, bytearray())
        buf.extend(payload)

        if len(buf) >= 3:
            section_length = ((buf[1] & 0x0F) << 8) | buf[2]
            total_len = 3 + section_length
            if len(buf) >= total_len:
                sections.setdefault(pid, []).append(bytes(buf[:total_len]))
                buffers[pid] = buf[total_len:]

    return sections


def describe_descriptor(tag, payload):
    if tag == 0x05 and len(payload) >= 4:
        ident = payload[:4].decode("ascii", errors="replace")
        return f"registration_descriptor: {ident}"
    if tag == 0x26:
        return "metadata_descriptor"
    return f"descriptor 0x{tag:02X} ({len(payload)} bytes)"


def registration_identifier(stream: StreamInfo):
    for tag, payload in stream.descriptors:
        if tag == 0x05 and len(payload) >= 4:
            return payload[:4].decode("ascii", errors="replace")
    return None


def is_klv_stream(stream: StreamInfo):
    ident = registration_identifier(stream)
    return stream.stream_type == 0x15 or (stream.stream_type == 0x06 and ident == "KLVA")


def main():
    parser = argparse.ArgumentParser(description="Verify TS KLV metadata mapping")
    parser.add_argument("ts_path", help="Path to MPEG-TS file")
    parser.add_argument("--list-all", action="store_true", help="List all PMT streams")
    args = parser.parse_args()

    data = Path(args.ts_path).read_bytes()
    sections = extract_sections(data)

    pmt_pid = None
    if 0 in sections:
        for sec in sections[0]:
            pmt_pid = parse_pat(sec)
            if pmt_pid is not None:
                break

    if pmt_pid is None:
        print("ERROR PAT/PMT not found")
        return 1

    pmt_sections = sections.get(pmt_pid, [])
    if not pmt_sections:
        print(f"ERROR PMT PID 0x{pmt_pid:04X} not found")
        return 1

    streams = parse_pmt(pmt_sections[-1])

    if not streams:
        print("ERROR No streams found in PMT")
        return 1

    print(f"PMT PID: 0x{pmt_pid:04X}")
    if args.list_all:
        for s in streams:
            print(f"  PID 0x{s.pid:04X} stream_type 0x{s.stream_type:02X}")
            for tag, payload in s.descriptors:
                print(f"    - {describe_descriptor(tag, payload)}")

    klv_streams = [s for s in streams if is_klv_stream(s)]
    if not klv_streams:
        print("ERROR No KLV metadata streams found (expected 0x15 or 0x06 + KLVA)")
        return 2

    print("KLV metadata streams:")
    for s in klv_streams:
        reg_desc = [d for d in s.descriptors if d[0] == 0x05]
        meta_desc = [d for d in s.descriptors if d[0] == 0x26]
        ident = registration_identifier(s)
        if s.stream_type == 0x15:
            mode = "metadata PES"
        else:
            mode = "private PES + KLVA"
        print(f"  PID 0x{s.pid:04X} stream_type 0x{s.stream_type:02X} ({mode})")
        if ident:
            print(f"    registration: {ident}")
        if meta_desc:
            print("    metadata_descriptor: present")
        else:
            print("    metadata_descriptor: not found")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
