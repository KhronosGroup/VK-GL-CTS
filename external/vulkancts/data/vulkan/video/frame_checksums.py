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
# Generate checksums from a sequence of YV12 YUV frames as produced by
# the Fraunhofer reference decoders.
#
#  The reference checksums were generated using the Fluster testing
#  framework (git@github.com:fluendo/fluster.git @
#  fd2a3aec596ba5437b4c3364111938531a4b5f92) to build the Fraunhofer
#  reference decoders,
#
#  ~/src/fluster $ make h26[45]_reference_decoder
#
#  Then, to generate references for H.264 test content,
#
#  $ cd ~/src/vk-gl-cts/external/vulkancts/data/vulkan/video/
#  $ ~/src/fluster/contrib/JM/bin/umake/gcc-12.2/x86_64/release/ldecod -s -i clip-a.h264 -o clip-a.out
#  $ python frame_checksums.py  clip-a.out  176 144 ; rm clip-a.out
#
#  To generate the H.265 YUV files, change the reference decoder line above to,
#
#  $ ~/src/fluster/contrib/HM/bin/umake/gcc-12.2/x86_64/release/TAppDecoder -b jellyfish-250-mbps-4k-uhd-GOB-IPB13.h265 -o jellyfish265.out
#
#  The h264_resolution_change example was specially handled, since
#  it's actually a non-conformant bitstream according to the reference
#  decoders. There are two streams of different resolution
#  concatenated together in the test vector. They were manually cut
#  out and each independent stream was passed through the above
#  process to generate the checksums.
#

import os
import sys
import hashlib

def print_help():
    help_text = """Usage: frame_checksums.py [OPTIONS] yuvFile width height

Generate MD5 checksums from YUV frames in C array format.

Arguments:
  yuvFile              YUV file to process (YV12 format)
  width                Frame width in pixels
  height               Frame height in pixels

Options:
  -a NAME              C array name (default: frameChecksums)
  -n NUM               Process only first NUM frames
  -h, --help           Show this help message and exit

Examples:
  python frame_checksums.py clip-a.out 176 144
  python frame_checksums.py -n 30 -a clipAChecksums clip-a.out 176 144
"""
    print(help_text)
    sys.exit(0)

# Parse command line arguments
yuv_filename = None
width = None
height = None
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
    elif yuv_filename is None:
        yuv_filename = sys.argv[i]
        i += 1
    elif width is None:
        width = int(sys.argv[i])
        i += 1
    elif height is None:
        height = int(sys.argv[i])
        i += 1
    else:
        i += 1

if yuv_filename is None or width is None or height is None:
    print("Usage: frame_checksums.py [OPTIONS] yuvFile width height")
    print("Use -h or --help for more information")
    sys.exit(1)

def checksum(bs: bytes) -> str:
    md5 = hashlib.md5()
    md5.update(bs)
    return md5.hexdigest()

file_size = os.path.getsize(yuv_filename)

# This assumes the YCrCb format is YV12, as produced by the Fraunhofer
# reference decoders.
frame_size = int(width * height * 1.5)
n_frames = file_size // frame_size

# Apply frame limit if specified
if frame_limit is not None:
    n_frames = min(n_frames, frame_limit)

md5_hashes = []
with open(yuv_filename, 'rb') as f:
    print("Computing checksums for ", n_frames, " frames", file=sys.stderr)
    for frame in range(n_frames):
        md5_hashes.append(checksum(f.read(frame_size)))

# Output in C array format (3 hashes per line)
print(f"static const char *{array_name}[{n_frames}] = {{")

for i in range(0, n_frames, 3):
    # Get up to 3 hashes for this line
    line_hashes = md5_hashes[i:i+3]
    formatted_hashes = ', '.join(f'"{h}"' for h in line_hashes)

    # Add comma after line unless it's the last line
    line_end = ',' if i + 3 < n_frames else ''
    print(f"    {formatted_hashes}{line_end}")

print("};")
print("", file=sys.stderr)
