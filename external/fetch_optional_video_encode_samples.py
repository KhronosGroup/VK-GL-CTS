# -*- coding: utf-8 -*-

# -------------------------------------------------------------------------
# fetch_optional_video_encode_samples.py
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
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
# -------------------------------------------------------------------------

"""Fetch optional YUV video sample data from the Khronos GCS bucket."""

from fetch_video_common import SourceFile, main

# Source: Khronos Google Cloud Storage
# Contact: Stephane Cerveau, scerveau@igalia.com
PACKAGES = [
    SourceFile(
        "yuv/FT_128x128_420_8le.yuv",
        "2cfb9a828ef77425b73afd5816f4678debd81591ecf0b0c400a2ec8e8e6d68dd",
        "yuv/128x128_420_8le.yuv"),
    SourceFile(
        "yuv/FT_128x128_420_10le.yuv",
        "ee302fab5f3c3bd20793896aceb0f2455bfde837cee45f528a5420a2af8f7908",
        "yuv/128x128_420_10le.yuv"),
    SourceFile(
        "yuv/FT_176x144_420_8le.yuv",
        "2ccacb83a3694631d17eea69e69206621d3a9fdbc4d2c88788dcdf78915f8bff",
        "yuv/176x144_420_8le.yuv"),
    SourceFile(
        "yuv/FT_176x144_420_10le.yuv",
        "266d6af3bb87f9a5f0d8759554dd6b6bbbef87da234d227e8d5ce8c4c64948db",
        "yuv/176x144_420_10le.yuv"),
    SourceFile(
        "yuv/FT_352x288_420_8le.yuv",
        "1d522ff68b7bfa603650ead8ce2c597f42e37eabcc3ae1ee47d11ee2f1b2a7f2",
        "yuv/352x288_420_8le.yuv"),
    SourceFile(
        "yuv/FT_352x288_420_10le.yuv",
        "cdf694b21ebf4dd9d1d3e2321dd4a85f0a4a9cf238146f23db1cdeceaf59af0c",
        "yuv/352x288_420_10le.yuv"),
    SourceFile(
        "yuv/FT_720x480_420_8le.yuv",
        "0a53b9fc8739399f2cdfa73c5dc08357d21fc7a5d874c4454c35a2a618ed2425",
        "yuv/720x480_420_8le.yuv"),
    SourceFile(
        "yuv/FT_720x480_420_10le.yuv",
        "eccf4eb80a37e70153f6de5c852f572c67a77e1c649f9b4f9bed2d12d926c0a8",
        "yuv/720x480_420_10le.yuv"),
    SourceFile(
        "yuv/FT_1920x1080_420_8le.yuv",
        "b02e215436c336e610cae4eb35c758534552d059776ec0571fd4703143b95b52",
        "yuv/1920x1080_420_8le.yuv"),
    SourceFile(
        "yuv/FT_1920x1080_420_10le.yuv",
        "7a17b7d6b3aed86c8ab8979f388dedc89d03b7c55e46a7d6144e9a863a1a2d72",
        "yuv/1920x1080_420_10le.yuv"),
    SourceFile(
        "yuv/FT_3840x2160_420_8le.yuv",
        "d4315bb07f07d86e6815fd2ae7a03d057e4c625ab9841d0a3988eaa366c45453",
        "yuv/3840x2160_420_8le.yuv"),
    SourceFile(
        "yuv/FT_3840x2160_420_10le.yuv",
        "935266f1c0f59ca716161dc618341ca9f0a8572594592ebb852dafc22a691cc2",
        "yuv/3840x2160_420_10le.yuv"),
    SourceFile(
        "yuv/FT_7680x4320_420_8le.yuv",
        "8d12097d0ad9d8eb89a626b394df6c84213efe3f6c29823d345a564a1e8ec762",
        "yuv/7680x4320_420_8le.yuv"),
    SourceFile(
        "yuv/FT_7680x4320_420_10le.yuv",
        "4fefdcaaaa2cc144e2d2ab4794abc2ccdb9f9f346ee41d6377779c745f67feeb",
        "yuv/7680x4320_420_10le.yuv"),
]


if __name__ == "__main__":
    main(PACKAGES, "Fetch optional video encode sample data")
