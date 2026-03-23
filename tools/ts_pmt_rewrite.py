#!/usr/bin/env python3
"""
@file tools/ts_pmt_rewrite.py
@brief Rewrite MPEG-TS PMT entries for KLV metadata signalling.
@ingroup gstklv_tools

This utility post-processes transport streams and rewrites PMT entries to mark
KLV streams as metadata, optionally injecting a metadata descriptor.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


TS_PACKET_SIZE = 188
CRC32_POLY = 0x04C11DB7


@dataclass
class StreamInfo:
    stream_type: int
    pid: int
    descriptors: List[Tuple[int, bytes]]


@dataclass
class PMTInfo:
    program_number: int
    version_byte: int
    section_number: int
    last_section_number: int
    pcr_pid: int
    program_info: bytes
    streams: List[StreamInfo]


def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= (b & 0xFF) << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) & 0xFFFFFFFF) ^ CRC32_POLY
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc & 0xFFFFFFFF


def iter_ts_packets(data: bytes):
    for i in range(0, len(data) - TS_PACKET_SIZE + 1, TS_PACKET_SIZE):
        pkt = bytearray(data[i : i + TS_PACKET_SIZE])
        if pkt[0] != 0x47:
            continue
        yield i, pkt


def parse_descriptors(data: bytes) -> List[Tuple[int, bytes]]:
    descriptors = []
    i = 0
    while i + 1 < len(data):
        tag = data[i]
        length = data[i + 1]
        payload = data[i + 2 : i + 2 + length]
        descriptors.append((tag, payload))
        i += 2 + length
    return descriptors


def build_descriptor_bytes(descriptors: List[Tuple[int, bytes]]) -> bytes:
    out = bytearray()
    for tag, payload in descriptors:
        if len(payload) > 255:
            raise ValueError(f"Descriptor 0x{tag:02X} too long: {len(payload)}")
        out.append(tag)
        out.append(len(payload))
        out.extend(payload)
    return bytes(out)


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


def parse_pmt(section: bytes) -> PMTInfo:
    if not section or section[0] != 0x02:
        raise ValueError("Not a PMT section")
    section_length = ((section[1] & 0x0F) << 8) | section[2]
    end = 3 + section_length - 4
    if end > len(section):
        end = len(section)

    program_number = (section[3] << 8) | section[4]
    version_byte = section[5]
    section_number = section[6]
    last_section_number = section[7]
    pcr_pid = ((section[8] & 0x1F) << 8) | section[9]
    program_info_length = ((section[10] & 0x0F) << 8) | section[11]
    program_info = section[12 : 12 + program_info_length]

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
        streams.append(StreamInfo(stream_type=stream_type, pid=pid, descriptors=descriptors))
        pos = desc_end

    return PMTInfo(
        program_number=program_number,
        version_byte=version_byte,
        section_number=section_number,
        last_section_number=last_section_number,
        pcr_pid=pcr_pid,
        program_info=program_info,
        streams=streams,
    )


def build_metadata_descriptor(
    app_format: int,
    app_identifier: bytes,
    fmt: int,
    fmt_identifier: bytes,
    service_id: int,
    flags: int,
) -> bytes:
    payload = bytearray()
    payload.extend(app_format.to_bytes(2, "big"))
    if app_format == 0xFFFF:
        if len(app_identifier) != 4:
            raise ValueError("metadata_application_format_identifier must be 4 bytes")
        payload.extend(app_identifier)
    payload.append(fmt & 0xFF)
    if fmt == 0xFF:
        if len(fmt_identifier) != 4:
            raise ValueError("metadata_format_identifier must be 4 bytes")
        payload.extend(fmt_identifier)
    payload.append(service_id & 0xFF)
    payload.append(flags & 0xFF)
    return bytes(payload)


def build_pmt_section(pmt: PMTInfo) -> bytes:
    body = bytearray()
    body.extend(pmt.program_number.to_bytes(2, "big"))
    body.append(pmt.version_byte)
    body.append(pmt.section_number)
    body.append(pmt.last_section_number)
    body.extend(((0xE0 | ((pmt.pcr_pid >> 8) & 0x1F)), pmt.pcr_pid & 0xFF))
    program_info_length = len(pmt.program_info)
    body.extend(((0xF0 | ((program_info_length >> 8) & 0x0F)), program_info_length & 0xFF))
    body.extend(pmt.program_info)

    for s in pmt.streams:
        desc_bytes = build_descriptor_bytes(s.descriptors)
        es_info_length = len(desc_bytes)
        body.append(s.stream_type & 0xFF)
        body.extend(((0xE0 | ((s.pid >> 8) & 0x1F)), s.pid & 0xFF))
        body.extend(((0xF0 | ((es_info_length >> 8) & 0x0F)), es_info_length & 0xFF))
        body.extend(desc_bytes)

    section_length = len(body) + 4
    if section_length > 0x0FFF:
        raise ValueError("PMT section too long")

    header = bytearray()
    header.append(0x02)
    header.append(0xB0 | ((section_length >> 8) & 0x0F))
    header.append(section_length & 0xFF)

    section = bytes(header + body)
    crc = crc32_mpeg2(section)
    section += crc.to_bytes(4, "big")
    return section


def has_registration_klva(descriptors: List[Tuple[int, bytes]]) -> bool:
    for tag, payload in descriptors:
        if tag == 0x05 and len(payload) >= 4 and payload[:4] == b"KLVA":
            return True
    return False


def has_metadata_descriptor(descriptors: List[Tuple[int, bytes]]) -> bool:
    return any(tag == 0x26 for tag, _ in descriptors)


def rewrite_pmt(
    pmt: PMTInfo,
    target_pids: List[int],
    match_registration: bool,
    metadata_payload: bytes,
) -> Tuple[PMTInfo, int]:
    changed = 0
    new_streams = []
    for s in pmt.streams:
        use = False
        if target_pids and s.pid in target_pids:
            use = True
        if match_registration and has_registration_klva(s.descriptors):
            use = True

        if use:
            new_desc = list(s.descriptors)
            if not has_metadata_descriptor(new_desc):
                reg_index = next(
                    (i for i, (tag, payload) in enumerate(new_desc) if tag == 0x05 and payload[:4] == b"KLVA"),
                    None,
                )
                entry = (0x26, metadata_payload)
                if reg_index is None:
                    new_desc.append(entry)
                else:
                    new_desc.insert(reg_index + 1, entry)
            new_streams.append(StreamInfo(stream_type=0x15, pid=s.pid, descriptors=new_desc))
            if s.stream_type != 0x15:
                changed += 1
        else:
            new_streams.append(s)

    return PMTInfo(
        program_number=pmt.program_number,
        version_byte=pmt.version_byte,
        section_number=pmt.section_number,
        last_section_number=pmt.last_section_number,
        pcr_pid=pmt.pcr_pid,
        program_info=pmt.program_info,
        streams=new_streams,
    ), changed


def extract_sections(data: bytes):
    sections = {}
    buffers = {}

    for _, pkt in iter_ts_packets(data):
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

        payload = bytes(pkt[offset:])
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


def rewrite_ts_packets(data: bytes, pmt_pid: int, new_section: bytes) -> bytes:
    out = bytearray(data)
    payload_only_capacity = TS_PACKET_SIZE - 4
    section_with_pointer = len(new_section) + 1

    if section_with_pointer <= payload_only_capacity:
        for i, pkt in iter_ts_packets(out):
            pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
            if pid != pmt_pid:
                continue

            pkt[1] |= 0x40
            pkt[3] = (pkt[3] & 0xCF) | 0x10
            payload = b"\x00" + new_section
            if len(payload) < payload_only_capacity:
                payload += b"\xFF" * (payload_only_capacity - len(payload))
            pkt[4:] = payload[:payload_only_capacity]
            out[i : i + TS_PACKET_SIZE] = pkt

        return bytes(out)

    remaining = new_section
    for i, pkt in iter_ts_packets(out):
        pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
        if pid != pmt_pid:
            continue

        pkt[3] = (pkt[3] & 0xCF) | 0x30
        pkt[4] = 0x00
        offset = 5
        payload_len = TS_PACKET_SIZE - offset
        if payload_len <= 0:
            continue

        if remaining is new_section:
            pkt[1] |= 0x40
            pointer_field = b"\x00"
            chunk_len = min(payload_len - 1, len(remaining))
            chunk = remaining[:chunk_len]
            remaining = remaining[chunk_len:]
            new_payload = pointer_field + chunk
        else:
            pkt[1] &= 0xBF
            chunk_len = min(payload_len, len(remaining))
            chunk = remaining[:chunk_len]
            remaining = remaining[chunk_len:]
            new_payload = chunk

        if len(new_payload) < payload_len:
            new_payload += b"\xFF" * (payload_len - len(new_payload))
        pkt[offset:] = new_payload
        out[i : i + TS_PACKET_SIZE] = pkt

        if not remaining:
            return bytes(out)

    raise RuntimeError("PMT section did not fit into available packets")


def parse_hex_payload(value: str) -> bytes:
    if value.startswith("hex:"):
        value = value[4:]
    value = value.strip()
    if not value:
        return b""
    return bytes.fromhex(value)


def parse_fourcc(value: str) -> bytes:
    raw = value.encode("ascii", errors="strict")
    if len(raw) != 4:
        raise ValueError("Identifier must be exactly 4 ASCII bytes")
    return raw


def main():
    parser = argparse.ArgumentParser(description="Rewrite PMT metadata stream_type and descriptors")
    parser.add_argument("ts_in", help="Input TS path")
    parser.add_argument("ts_out", help="Output TS path")
    parser.add_argument(
        "--pid",
        action="append",
        type=lambda v: int(v, 0),
        help="PID to mark as metadata (repeatable, hex or decimal)",
    )
    parser.add_argument(
        "--match-registration",
        action="store_true",
        help="Match streams with registration_descriptor KLVA",
    )
    parser.add_argument(
        "--metadata-descriptor-hex",
        help="Override metadata_descriptor payload as hex (e.g. hex:FFFF4D495342FF4B4C56410100)",
    )
    parser.add_argument("--metadata-app-format", type=lambda v: int(v, 0), default=0xFFFF)
    parser.add_argument("--metadata-app-identifier", default="MISB")
    parser.add_argument("--metadata-format", type=lambda v: int(v, 0), default=0xFF)
    parser.add_argument("--metadata-format-identifier", default="KLVA")
    parser.add_argument("--metadata-service-id", type=lambda v: int(v, 0), default=0x01)
    parser.add_argument("--metadata-flags", type=lambda v: int(v, 0), default=0x00)
    args = parser.parse_args()

    data = Path(args.ts_in).read_bytes()
    sections = extract_sections(data)

    pmt_pid = None
    if 0 in sections:
        for sec in sections[0]:
            pmt_pid = parse_pat(sec)
            if pmt_pid is not None:
                break

    if pmt_pid is None:
        raise SystemExit("ERROR PAT/PMT not found")

    pmt_sections = sections.get(pmt_pid, [])
    if not pmt_sections:
        raise SystemExit(f"ERROR PMT PID 0x{pmt_pid:04X} not found")

    pmt = parse_pmt(pmt_sections[0])

    if args.metadata_descriptor_hex:
        metadata_payload = parse_hex_payload(args.metadata_descriptor_hex)
    else:
        metadata_payload = build_metadata_descriptor(
            app_format=args.metadata_app_format,
            app_identifier=parse_fourcc(args.metadata_app_identifier),
            fmt=args.metadata_format,
            fmt_identifier=parse_fourcc(args.metadata_format_identifier),
            service_id=args.metadata_service_id,
            flags=args.metadata_flags,
        )

    target_pids = args.pid or []
    match_registration = args.match_registration or not target_pids

    new_pmt, changed = rewrite_pmt(
        pmt,
        target_pids=target_pids,
        match_registration=match_registration,
        metadata_payload=metadata_payload,
    )

    new_section = build_pmt_section(new_pmt)
    out = rewrite_ts_packets(data, pmt_pid, new_section)
    Path(args.ts_out).write_bytes(out)

    print(f"PMT PID: 0x{pmt_pid:04X}")
    print(f"Streams updated: {changed}")
    print(f"Output: {args.ts_out}")


if __name__ == "__main__":
    raise SystemExit(main())
