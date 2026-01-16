# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
#
# Copyright (C) 2023 The Khronos Group Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------
#
# Generate MD5 checksums from video frames using ffmpeg.
#
# This script uses ffmpeg's framemd5 muxer to calculate MD5 checksums
# for each frame. This provides an alternative to the Fraunhofer reference
# decoder approach used in frame_checksums.py.
#
# Usage:
#   python frame_checksums_ffmpeg.py <video_file>
#
# Example:
#   python frame_checksums_ffmpeg.py clip-a.h264
#
# Requirements:
#   - ffmpeg must be installed and available in PATH
#
# The script outputs MD5 checksums in C array format, matching the style
# used in vktVideoClipInfo.cpp (3 checksums per line).
#

import sys
import subprocess
import os

def print_help():
    help_text = """Usage: frame_checksums_ffmpeg.py [OPTIONS] videoFile

Generate MD5 checksums from video frames using ffmpeg in C array format.

Arguments:
  videoFile            Video file to process (H.264, H.265, etc.)

Options:
  -a NAME              C array name (default: frameChecksums)
  -n NUM               Process only first NUM frames
  -h, --help           Show this help message and exit

Requirements:
  - ffmpeg must be installed and available in PATH

Examples:
  python frame_checksums_ffmpeg.py clip-a.h264
"""
    print(help_text)
    sys.exit(0)

# Parse command line arguments
video_filename = None
array_name = "frameChecksums"
frame_limit = None

i = 1
while i < len(sys.argv):
    if sys.argv[i] in ('-h', '--help'):
        print_help()
    elif sys.argv[i] == '-n' and i + 1 < len(sys.argv):
        frame_limit = int(sys.argv[i + 1])
        i += 2
    elif sys.argv[i] == '-a' and i + 1 < len(sys.argv):
        array_name = sys.argv[i + 1]
        i += 2
    elif video_filename is None:
        video_filename = sys.argv[i]
        i += 1
    else:
        i += 1

if video_filename is None:
    print("Usage: frame_checksums_ffmpeg.py [OPTIONS] videoFile")
    print("Use -h or --help for more information")
    sys.exit(1)

if not os.path.exists(video_filename):
    print(f"Error: File '{video_filename}' not found", file=sys.stderr)
    sys.exit(1)

try:
    # Use ffmpeg to generate frame MD5 checksums
    # The -f framemd5 output format generates MD5 for each frame
    # Output to stdout (-) so we can parse it directly
    cmd = [
        'ffmpeg',
        '-i', video_filename,
    ]

    # Add frame limit if specified
    if frame_limit is not None:
        cmd.extend(['-vframes', str(frame_limit)])

    cmd.extend(['-f', 'framemd5', '-'])

    result = subprocess.run(cmd, capture_output=True, text=True, check=True)

    # Parse the framemd5 output
    # Format is: stream#, frame#, pts, duration, size, md5
    # Example: 0,          0,          0,        1,  38016, 6b5e29f8f5f4e3d4f8c9e7b8a4d6c3f2
    md5_hashes = []
    for line in result.stdout.split('\n'):
        line = line.strip()
        # Skip comments and empty lines
        if not line or line.startswith('#'):
            continue

        # Parse the line and extract the MD5 (last field)
        parts = line.split(',')
        if len(parts) >= 6:
            md5_hash = parts[-1].strip()
            md5_hashes.append(md5_hash)

    frame_count = len(md5_hashes)
    print(f"Computing checksums for {frame_count} frames", file=sys.stderr)

    # Output in C array format (3 hashes per line)
    print(f"static const char *{array_name}[{frame_count}] = {{")

    for i in range(0, frame_count, 3):
        # Get up to 3 hashes for this line
        line_hashes = md5_hashes[i:i+3]
        formatted_hashes = ', '.join(f'"{h}"' for h in line_hashes)

        # Add comma after line unless it's the last line
        line_end = ',' if i + 3 < frame_count else ''
        print(f"    {formatted_hashes}{line_end}")

    print("};")
    print(f"", file=sys.stderr)

except subprocess.CalledProcessError as e:
    print(f"Error running ffmpeg: {e}", file=sys.stderr)
    if e.stderr:
        print(f"ffmpeg stderr: {e.stderr}", file=sys.stderr)
    sys.exit(1)
except FileNotFoundError:
    print("Error: ffmpeg not found. Please install ffmpeg.", file=sys.stderr)
    sys.exit(1)
except Exception as e:
    print(f"Error processing video: {e}", file=sys.stderr)
    sys.exit(1)
