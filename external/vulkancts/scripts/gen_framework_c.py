# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2015 Google Inc.
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

import os
import sys
import argparse
import re

scriptPath = os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *
from khr_util.format import writeInlFile

VULKAN_HEADERS_INCLUDE_DIR      = os.path.join(os.path.dirname(__file__), "..", "..", "vulkan-docs", "src", "include")
VULKAN_H = { "" : [    os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codecs_common.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h264std.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h264std_decode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h264std_encode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h265std.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h265std_decode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_h265std_encode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_av1std.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_av1std_decode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_av1std_encode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_vp9std.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vk_video", "vulkan_video_codec_vp9std_decode.h"),
                                os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "vulkan", "vulkan_core.h") ],
                        "SC" : [ os.path.join(os.path.dirname(__file__), "src", "vulkan_sc_core.h") ] }
DEFAULT_OUTPUT_DIR = { "" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkan"),
                        "SC" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkansc") }

INL_HEADER = """\
/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 * This file was generated by /scripts/gen_framework_c.py
 */\

"""

TYPE_SUBSTITUTIONS = [
    ("uint8_t", "uint8_t"),
    ("uint16_t", "uint16_t"),
    ("uint32_t", "uint32_t"),
    ("uint64_t", "uint64_t"),
    ("int8_t", "int8_t"),
    ("int16_t", "int16_t"),
    ("int32_t", "int32_t"),
    ("int64_t", "int64_t"),
    ("bool32_t", "uint32_t"),
    ("size_t", "uintptr_t"),
]

def readFile (filename):
    with open(filename, 'rt') as f:
        return f.read()

def writeVulkanCHeader (src, filename):
    def gen ():
        dst = re.sub(r'(#include "[^\s,\n}]+")', '', src)

        for old_type, new_type in TYPE_SUBSTITUTIONS:
            dst = dst.replace(old_type, new_type)
        yield dst
    writeInlFile(filename, INL_HEADER, gen())

def parseCmdLineArgs():
    parser = argparse.ArgumentParser(description = "Generate Vulkan INL files",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-a",
                        "--api",
                        dest="api",
                        default="",
                        help="Choose between Vulkan and Vulkan SC")
    parser.add_argument("-o",
                        "--outdir",
                        dest="outdir",
                        default="",
                        help="Choose output directory")
    parser.add_argument("-v", "--verbose",
                        dest="verbose",
                        action="store_true",
                        help="Enable verbose logging")
    return parser.parse_args()

def getApiName (args):
    if len(args)<2:
        return ''
    return args[1]

if __name__ == "__main__":
    args = parseCmdLineArgs()
    initializeLogger(args.verbose)

    outputPath = DEFAULT_OUTPUT_DIR[args.api]
    # if argument was specified it is interpreted as a path to which .inl file will be written
    if args.outdir != '':
        outputPath = args.outdir
    src = ""

    # Generate vulkan headers from vk.xml
    currentDir = os.getcwd()
    pythonExecutable = sys.executable or "python"
    os.chdir(os.path.join(VULKAN_HEADERS_INCLUDE_DIR, "..", "xml"))
    vkTargets = ["vulkan_core.h"]
    for target in vkTargets:
        execute([pythonExecutable, "../scripts/genvk.py", "-o", "../include/vulkan", target])

    videoDir = "../include/vk_video"
    if (not os.path.isdir(videoDir)):
        os.mkdir(videoDir)

    videoTargets = [
        'vulkan_video_codecs_common.h',
        'vulkan_video_codec_h264std.h',
        'vulkan_video_codec_h264std_decode.h',
        'vulkan_video_codec_h264std_encode.h',
        'vulkan_video_codec_h265std.h',
        'vulkan_video_codec_h265std_decode.h',
        'vulkan_video_codec_h265std_encode.h',
        'vulkan_video_codec_av1std.h',
        'vulkan_video_codec_av1std_decode.h',
        'vulkan_video_codec_av1std_encode.h',
        'vulkan_video_codec_vp9std.h',
        'vulkan_video_codec_vp9std_decode.h',
    ]
    for target in videoTargets:
        execute([pythonExecutable, "../scripts/genvk.py", "-registry", "video.xml", "-o", videoDir, target])

    os.chdir(currentDir)

    for file in VULKAN_H[args.api]:
        src += readFile(file)

    writeVulkanCHeader                (src, os.path.join(outputPath, "vkVulkan_c.inl"))
