# -*- coding: utf-8 -*-

# -------------------------------------------------------------------------
# fetch_video_decode_samples.py
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

"""Fetch video decode sample data from the Khronos GCS bucket."""

from fetch_video_common import SourceFile, main

# Source: Khronos Google Cloud Storage
PACKAGES = [
    SourceFile(
        "avc/4k_26_ibp_main.h264",
        "1b6c2fa6ea7cb8fac8064036d8729f668e913ea7cf3860009924789b8edf042f"),
    SourceFile(
        "avc/clip-a.h264",
        "b119d5d667eb7791613d0e39f375b61f0fa41baa5ee1f216a678e0483c69333c"),
    SourceFile(
        "avc/clip-c.h264",
        "fab2665bfffaac4a60c13cf9d082fae6a2042a3926b91889cb06d8dd54d26388"),
    SourceFile(
        "hevc/hevc-big_buck_bunny_2160p.h265",
        "ddb1e4f0b0b6be037a81b9c88842b729d3780813ff67f90145305be422becc43"),
    SourceFile(
        "hevc/clip-d.h265",
        "d7db8cb323e10c5e30667b141554e88bab3bacf6bc878b544866867b76c3d35a"),
    SourceFile(
        "hevc/hevc-itu-slist-a.h265",
        "be5532289143b068bfe451243b750b8162017c0569046c680931987469f47095"),
    SourceFile(
        "hevc/hevc-itu-slist-b.h265",
        "d37c0a144fc67a10195b2134a1eccc268d8b37add53e91ddb74f26b690c053ec"),
    SourceFile(
        "hevc/LTRPSPS_A_Qualcomm_1.bit",
        "8f8e50fa408b7e96e76e6936b380fc34c208271efb8c283b15a42cb1f8c78ae7"),
    SourceFile(
        "av1/av1-176x144-main-basic-8.ivf",
        "798b52f372a95b4da01334ba97c651e8c2e21e887180f6ad401cf9d4e3a15aa7"),
    SourceFile(
        "av1/av1-176x144-main-basic-10.ivf",
        "1c6bcfdeddd173ddb25074d2433e8cb84628692fb64a2d09d69e394b2b618ce2"),
    SourceFile(
        "av1/av1-176x144-main-cdef-10.ivf",
        "cc2e76d084ab86d53bbc6f93477346c3f97dcfc97b8b328972657ef53a230b6c"),
    SourceFile(
        "av1/av1-176x144-main-forward-key-frame-10.ivf",
        "1c6bcfdeddd173ddb25074d2433e8cb84628692fb64a2d09d69e394b2b618ce2"),
    SourceFile(
        "av1/av1-176x144-main-globalmotion-8.ivf",
        "8670d6bfb76915abb94aa6e3c3650541c1405e6745b244460dd4256cab65701f"),
    SourceFile(
        "av1/av1-176x144-main-loop-filter-10.ivf",
        "2203334129266193a648ba8ef9fe3a46be6647303826f205cb6403b9c88dd07f"),
    SourceFile(
        "av1/av1-176x144-main-lossless-10.ivf",
        "73f45315b2f45026e734fa6a918a482c12bda0a2e771602a99ccee2855720996"),
    SourceFile(
        "av1/av1-176x144-main-orderhint-10.ivf",
        "e54037e32462abbd7e985c97ae60e04e4a976a4b0d2807c076b3cb7a46ffd52e"),
    SourceFile(
        "av1/av1-1920x1080-intrabc-extreme-dv-8.ivf",
        "0252716fe7a7c7f1ebb1eb23920a6737f45c10dfba942304bd339ae42b5c576c"),
    SourceFile(
        "av1/av1-1920x1080-main-superres-8.ivf",
        "af25538cbae7f31af0f25065ddc01ea908d08389c63ecb51886c3f69223fde23"),
    SourceFile(
        "av1/av1-352x288-main-allintra-8.ivf",
        "5fcd265fd9f9bdd0d3179340b4c4532f1422ca5e5d97741c7481b84cb5dc122f"),
    SourceFile(
        "av1/av1-352x288-main-cdfupdate-8.ivf",
        "14a3dbf537b6bf15efc003182d9916d61438c93624a8bd26e6e3ae7eaf33ea82"),
    SourceFile(
        "av1/av1-352x288-main-filmgrain-8.ivf",
        "ba3edd82a58414f009e1c821b947ec8de4e0420f33803ca7bfea39af6aeea155"),
    SourceFile(
        "av1/av1-640x360-main-svc-L1T2-8.ivf",
        "eade696ab60415d476e1d2a489ec2cccd24d5eaaadfa4559c6488b7932274c5e"),
    SourceFile(
        "av1/av1-argon_test1019.obu",
        "c11d5f0dcebb5def9d24752bca1594d3182e19661d3bf4b53473b2da935cd7d8"),
    SourceFile(
        "av1/av1-argon_test787.obu",
        "3c2df56e296cf387efb0474d7c588d68eca55592e15d4731bb85e2122059d367"),
    SourceFile(
        "av1/av1-frames_refs_short_signaling_golden.ivf",
        "d9136e7e427a1ec1423823f1d8cee37ed0aa354d16bb6607feb5413436810918"),
    SourceFile(
        "av1/av1-argon_test9354_2.obu",
        "23cc663c961262f4417adcd8f25ef6c92ba47f76bdfb3cafe6f1e78c27a03211"),
    SourceFile(
        "av1/av1-sizeup-fluster.ivf",
        "24b9554460aea54b24aff4adaf154c887fcc14ed114436bc87e8c29d8f4e7113"),
    # GStreamer generated
    SourceFile(
        "vp9/vp9-176x144-key-10.ivf",
        "c0da2ebe0c2787e5a4ffb23dc3dee31d57c11786e0cf22f1a15196d4428bbdd7"),
    # Source to confirm
    SourceFile(
        "vp9/vp9-512x288-basic-10.ivf",
        "28652251b44be3549fcb827c9d0bb1ddcdc203f11792b8c9cd9b8b6c3b0b88ca"),
    # vp90-2-17-show-existing-frame.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x288-show-existing-frames.ivf",
        "af01c1466fe5759bad4fee45cf6444bfb6baa027d98d7334a9ef76aea6d33cce"),
    # vp90-2-11-size-351x287.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-351x287-basic.ivf",
        "26865fd70915a801a4dc1bc9db71ac33097e6992f2f9119b86b8f3b4f3dfc841"),
    # vp90-2-08-tile_1x2.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-1920x1080-tile-1x2.ivf",
        "6d7b071fbae0604b098a09bdea66ab4b0d5e9c273efd52d82be6b3bd4827603f"),
    # vp90-2-08-tile_1x4.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-1920x1080-tile-1x4.ivf",
        "7ebe0ecb21fa7a53264a5ab6b29325520261b80039be279ffc7e8c49b1ada539"),
    # vp90-2-08-tile_1x8.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-3980x2160-tile-1x8.ivf",
        "c2340e53813734331247f860f928eaef3316e08362a163c74aa1efa3fcc4de3b"),
    # vp90-2-08-tile_4x4.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-1920x1080-tile-4x4.ivf",
        "aac08edced94e6c5858d48bcaea839679c701133963959e182e1684ea44cfb21"),
    # vp90-2-05-resize.ivf
    SourceFile(
        "vp9/vp9-resize.ivf",
        "84a345546e47e37de87d2959347bf731f9f440f9f2ab32bede3f0e416100c585"),
    # vp90-2-09-lf_deltas.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x240-lf.ivf",
        "808e5734610d2f58472d7a3dc2e703e83fbf0e4ab513c2f13dfce902ee41d0b7"),
    # Generated with GStreamer videotestsrc
    SourceFile(
        "vp9/vp9-320x240-10bits.ivf",
        "0ba93502fa08ace58c84d02c7bc8664ca862cb4ad5cda7642d116956fe53d137"),
    # vp90-2-16-intra-only.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x288-intra-only.ivf",
        "abe164efe985fcc342af82866e1a7998348563c75d95fec3e96e59bd871f2fd5"),
    # vp90-2-15-segkey_adpq.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-852x480-segkey.ivf",
        "39e9f10473f358fe25bc901157be6fcbd1a62c1bd1b0eefde05de201fd4f5ea7"),
    # vp90-2-00-quantizer-00.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x288-quant-00.ivf",
        "0ee879e57f9f705757847c7cf62ea1672c99d7f8b6e5f6764d5b5f25847022bc"),
    # vp90-2-00-quantizer-26.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x288-quant-26.ivf",
        "8950c0d42d34f9d767d25382a30e08198df8ca18ba8ad9481c66c28e7e6da335"),
    # vp90-2-00-quantizer-59.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-352x288-quant-59.ivf",
        "a063b444112d81ca6d3163756bac01e341c79c607dbf52ad7169a2b71b17a460"),
    # vp90-2-14-resize-fp-tiles-1-2.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-resize-1-2.ivf",
        "eb73c5e6a3fe0aa22bda18ed5fbf4c57baa29681ace12aa1a2739b89ab2536bd"),
    # vp90-2-22-svc_1280x720_1.webm container converted with GStreamer
    SourceFile(
        "vp9/vp9-1280x720-svc.ivf",
        "8a8dc19ce33eaeb48a1b800fb775a45b358cf8a779ea97face108e2e6ea958f2"),
]


if __name__ == "__main__":
    main(PACKAGES, "Fetch video decode sample data")
