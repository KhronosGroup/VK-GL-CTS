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

if len(sys.argv) != 4:
	print("Usage: yuv2rgb-cv.py yuvFile width height")
	sys.exit(1)

yuv_filename = sys.argv[1]
width = int(sys.argv[2])
height = int(sys.argv[3])

def checksum(bs: bytes) -> str:
	md5 = hashlib.md5()
	md5.update(bs)
	return md5.hexdigest()

file_size = os.path.getsize(yuv_filename)

# This assumes the YCrCb format is YV12, as produced by the Fraunhofer
# reference decoders.
frame_size = int(width * height * 1.5)
n_frames = file_size // frame_size

with open(yuv_filename, 'rb') as f:
	print("Computing checksums for ", n_frames, " frames")
	for frame in range(n_frames):
		print(checksum(f.read(frame_size)))
