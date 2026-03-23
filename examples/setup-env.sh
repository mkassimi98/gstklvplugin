#!/bin/bash
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

# setup-env.sh - Common environment setup for GStreamer KLV plugin examples
# This script should be sourced, not executed: source ./setup-env.sh

# Check if script is being sourced
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "ERROR: This script must be sourced, not executed directly"
    echo "Usage: source ./setup-env.sh"
    exit 1
fi

# Get the directory where this script is located
# When sourced, BASH_SOURCE[0] contains the script's path (absolute or relative)
SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_PATH")" && pwd)"

# Calculate relative path to build directory
# examples/ is at: /repo/examples/setup-env.sh
# repo root is at: /repo
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${REPO_ROOT}/build"

# Add build directory to plugin path
export GST_PLUGIN_PATH="${BUILD_DIR}:${GST_PLUGIN_PATH}"

# Verify plugin is available
if ! gst-inspect-1.0 klvmetaenc &>/dev/null; then
    echo "ERROR: klvmetaenc plugin not found in $BUILD_DIR"
    echo "Please build the plugin first:"
    echo "  cd $REPO_ROOT && mkdir -p build && cd build && cmake .. && make"
    return 1
fi

export GST_PLUGIN_PATH
echo "GStreamer KLV plugin configured: $BUILD_DIR"
