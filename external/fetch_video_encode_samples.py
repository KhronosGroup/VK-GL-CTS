# -*- coding: utf-8 -*-

# -------------------------------------------------------------------------
# fetch_video_encode_samples.py
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

"""Fetch video encode sample data from the Khronos GCS bucket."""

from fetch_video_common import SourceFile, main

# Source: Khronos Google Cloud Storage
PACKAGES = [
    SourceFile(
        "yuv/176x144_30_i420.yuv",
        "235d1727dc8b4d3766fcbb52a8bfc5a87bcb050d3d7a15785bd30fa4a473c8e4"),
    SourceFile(
        "yuv/352x288_15_i420.yuv",
        "6e0e1a026717237f9546dfbd29d5e2ebbad0a993cdab38921bb43291a464ccd4"),
    SourceFile(
        "yuv/FT_720x480_420_8le.yuv",
        "0a53b9fc8739399f2cdfa73c5dc08357d21fc7a5d874c4454c35a2a618ed2425",
        "yuv/720x480_420_8le.yuv"),
    SourceFile(
        "yuv/FT_1920x1080_420_8le.yuv",
        "b02e215436c336e610cae4eb35c758534552d059776ec0571fd4703143b95b52",
        "yuv/1920x1080_420_8le.yuv"),
]


if __name__ == "__main__":
    main(PACKAGES, "Fetch video encode sample data")
