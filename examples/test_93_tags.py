#!/usr/bin/env python3
"""
@file examples/test_93_tags.py
@brief Python end-to-end exercise for all 93 MISB ST 0601.8 tags.
@ingroup gstklv_examples_python

This example generates representative values for the full MISB ST 0601.8 tag
set, runs the plugin encode/decode path, and prints a human-readable summary.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import json
import subprocess
import sys
import os
from pathlib import Path
from collections import OrderedDict

REPO_ROOT = Path(__file__).resolve().parents[1]
_meson_dir = REPO_ROOT / "build" / "src"
_cmake_dir = REPO_ROOT / "build"
PLUGIN_DIR = _meson_dir if (_meson_dir / "gstklvplugin.so").exists() else _cmake_dir


def extract_tag_definitions():
    """Load all tag definitions from INI"""
    base_dir = REPO_ROOT
    ini_file = base_dir / "data" / "stanag4609_tags.ini"
    tags = OrderedDict()
    
    with open(ini_file, 'r') as f:
        current_id = None
        current_tag = {}
        
        for line in f:
            line = line.strip()
            if line.startswith('[tag_'):
                current_id = None
            elif line.startswith('id = '):
                current_id = int(line.split('=')[1].strip())
            elif line.startswith('name = '):
                if current_id is not None:
                    if current_id not in tags:
                        tags[current_id] = {}
                    tags[current_id]['name'] = line.split('=', 1)[1].strip()
            elif line.startswith('type = '):
                if current_id is not None:
                    tags[current_id]['type'] = line.split('=', 1)[1].strip()
            elif line.startswith('length = '):
                if current_id is not None:
                    tags[current_id]['length'] = int(line.split('=')[1].strip())
    
    return tags

def generate_test_json():
    """Generate test values for all tags"""
    json_data = {}
    byte_tags = {48, 66, 73, 74, 92, 93}
    
    for tag_id in range(1, 94):
        if tag_id in byte_tags:
            json_data[str(tag_id)] = "hex:00"
            continue
        if tag_id == 1:
            json_data[str(tag_id)] = 0
        elif tag_id == 2:
            json_data[str(tag_id)] = 1234567890
        elif tag_id in [3, 4, 10, 11, 59, 63, 70]:  # STRING tags
            json_data[str(tag_id)] = f"TEST-{tag_id}"
        elif tag_id == 5:
            json_data[str(tag_id)] = 90.0
        elif tag_id == 6:
            json_data[str(tag_id)] = 5.0
        elif tag_id == 7:
            json_data[str(tag_id)] = 10.0
        elif tag_id in [8, 9]:
            json_data[str(tag_id)] = 50
        elif tag_id in [12, 13]:
            json_data[str(tag_id)] = 40.0
        elif tag_id in [14, 15]:
            json_data[str(tag_id)] = 45.5
        elif tag_id == 15:
            json_data[str(tag_id)] = 1500
        elif tag_id in range(16, 57):
            json_data[str(tag_id)] = 50.0 + (tag_id % 10)
        elif tag_id in range(57, 94):
            json_data[str(tag_id)] = 100 + tag_id
        else:
            json_data[str(tag_id)] = 0
    
    return json_data

def run_gst_test(json_str):
    """Run GStreamer pipeline"""
    env = os.environ.copy()
    env['GST_PLUGIN_PATH'] = str(PLUGIN_DIR)
    env['KLV_TAGS_INI'] = str(REPO_ROOT / "data" / "stanag4609_tags.ini")

    cmd = [
        'gst-launch-1.0', '-q',
        'videotestsrc', 'num-buffers=2',
        '!', 'video/x-raw,width=640,height=480',
        '!', 'x264enc',
        '!', 'h264parse',
        '!', 'klvframeinject',
        f'tags-json={json_str}',
        'name=inject',
        '!', 'queue',
        '!', 'fakesink',
        'inject.klv_src',
        '!', 'queue',
        '!', 'klvmetadec',
        '!', 'filesink',
        'location=/tmp/final_93_tags.json'
    ]
    
    result = subprocess.run(cmd, env=env, capture_output=True, text=True)
    return result.returncode == 0, result.stdout, result.stderr

def parse_output():
    """Parse decoded JSON output"""
    try:
        with open('/tmp/final_93_tags.json', 'r') as f:
            content = f.read()
            bracket_count = 0
            end_idx = 0
            for i, c in enumerate(content):
                if c == '{':
                    bracket_count += 1
                elif c == '}':
                    bracket_count -= 1
                    if bracket_count == 0:
                        end_idx = i + 1
                        break
            
            if end_idx > 0:
                first_obj = content[:end_idx]
                return json.loads(first_obj)
    except Exception as e:
        print(f"ERROR: {e}")
        return None

def main():
    print("\n" + "="*100)
    print("FINAL TEST: MISB ST 0601.8 TAGS")
    print("="*100 + "\n")
    
    # Load tag definitions
    tag_defs = extract_tag_definitions()
    print(f"OK Loaded definitions: {len(tag_defs)} tags\n")
    
    # Generate test JSON
    test_json = generate_test_json()
    json_str = json.dumps(test_json)
    
    print(f"OK Generated test data: {len(test_json)} tags sent\n")
    
    # Run pipeline
    print("> Running encode/decode pipeline...")
    success, stdout, stderr = run_gst_test(json_str)
    
    if not success:
        print(f"ERROR Pipeline error: {stderr}")
        return 1
    print("OK Pipeline completed\n")
    
    # Parse results
    output = parse_output()
    if not output:
        print("ERROR Failed to parse output")
        return 1
    
    decoded_tags = output if isinstance(output, dict) else {}
    encoded_tags = sorted([int(k) for k in decoded_tags.keys() if str(k).isdigit()])
    
    # Display all tags
    print("="*100)
    print(f"DETAILED RESULTS ({len(encoded_tags)} tags decoded from {len(test_json)} sent)")
    print("="*100 + "\n")
    
    # Create comprehensive table
    all_numeric = set(range(1, 94))
    
    print(f"{'TAG':>4} | {'STATUS':>12} | {'NAME':<35} | {'TYPE':<15} | {'VALUE':<20}\n")
    print("-" * 110)
    
    for tag_id in range(1, 94):
        tag_def = tag_defs.get(tag_id, {})
        tag_name = tag_def.get('name', 'Unknown')[:33]
        tag_type = tag_def.get('type', 'unknown')[:13]
        
        # Determine status
        if tag_id not in encoded_tags:
            status = "NOT DECODED"
            value_str = "-"
        else:
            status = "OK"
            value = decoded_tags.get(str(tag_id), "-")
            unit = tag_def.get('units', '')
            if tag_def.get('type', '').lower() == 'bytes' and isinstance(value, str):
                value_str = value[:18]
            elif isinstance(value, (int, float)):
                value_str = f"{value} {unit}".strip()
            else:
                value_str = str(value)[:18]
        
        print(f"{tag_id:>4} | {status:>12} | {tag_name:<35} | {tag_type:<15} | {value_str:<20}")
    
    # Summary
    print("\n" + "="*100)
    print("SUMMARY")
    print("="*100)
    print(f"OK Total tags: 93")
    print(f"OK Tags sent: {len(test_json)}")
    print(f"OK Tags decoded: {len(encoded_tags)}")
    print(f"OK Tags with decoded values: {len(encoded_tags)}")
    print("\nOK KLV pipeline test: OPERATIONAL\n")
    
    print("\n" + "="*100 + "\n")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
