# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
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
#-------------------------------------------------------------------------

import os
import sys
import shutil
import tarfile
import zipfile
import hashlib
import argparse
import subprocess
import ssl
import stat
import platform
import logging

scriptPath = os.path.join(os.path.dirname(__file__), "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *

EXTERNAL_DIR = os.path.realpath(os.path.normpath(os.path.dirname(__file__)))

SYSTEM_NAME = platform.system()

def computeChecksum (data):
    return hashlib.sha256(data).hexdigest()

def onReadonlyRemoveError (func, path, exc_info):
    os.chmod(path, stat.S_IWRITE)
    os.unlink(path)

class Source:
    def __init__(self, baseDir, extractDir):
        self.baseDir = baseDir
        self.extractDir = extractDir

    def clean (self):
        fullDstPath = os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir)
        # Remove read-only first
        readonlydir = os.path.join(fullDstPath, ".git")
        if os.path.exists(readonlydir):
            logging.debug("Deleting " + readonlydir)
            shutil.rmtree(readonlydir, onerror = onReadonlyRemoveError)
        if os.path.exists(fullDstPath):
            logging.debug("Deleting " + fullDstPath)
            shutil.rmtree(fullDstPath, ignore_errors=False)

class SourceFile (Source):
    def __init__(self, url, filename, checksum, baseDir, extractDir = "src"):
        Source.__init__(self, baseDir, extractDir)
        self.url = url
        self.filename = filename
        self.checksum = checksum

    def clean (self):
        filename = os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir, self.filename)
        try:
            os.remove(filename)
        except OSError:
            pass

    def update (self, cmdProtocol = None, force = False):
        if not self.isFileUpToDate():
            self.clean()
            self.fetchAndVerifyFile()

    def isFileUpToDate (self):
        file = os.path.join(EXTERNAL_DIR, pkg.baseDir, pkg.extractDir, pkg.filename)
        if os.path.exists(file):
            data = readBinaryFile(file)
            return computeChecksum(data) == self.checksum
        else:
            return False

    def connectToUrl (self, url):
        result = None

        if sys.version_info < (3, 0):
            from urllib2 import urlopen
        else:
            from urllib.request import urlopen

        if args.insecure:
            print("Ignoring certificate checks")
            ssl_context = ssl._create_unverified_context()
            result = urlopen(url, context=ssl_context)
        else:
            result = urlopen(url)

        return result

    def fetchAndVerifyFile (self):
        print("Fetching %s" % self.url)

        req = self.connectToUrl(self.url)
        data = req.read()
        checksum = computeChecksum(data)
        dstPath = os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir, self.filename)

        if checksum != self.checksum:
            raise Exception("Checksum mismatch for %s, expected %s, got %s" % (self.filename, self.checksum, checksum))

        if not os.path.exists(os.path.dirname(dstPath)):
            os.mkdir(os.path.dirname(dstPath))

        writeBinaryFile(dstPath, data)
        print("Downloaded: " + dstPath)


PACKAGES = [
# Source: Khronos Google Cloud Storage
# Contact: Stephane Cerveau, scerveau@igalia.com
# License: CC BY-NC 3.0
# https://ultravideo.fi/dataset.html
# ShakeNDry
# https://ultravideo.fi/video/ShakeNDry_3840x2160_120fps_420_8bit_HEVC_RAW.hevc
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/128x128_420_8le.yuv",
        "128x128_420_8le.yuv",
        "f0395cfa7003953246a5efcd24110217cfae8eff75e2d7487df75e64f083da8b",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/128x128_420_10le.yuv",
        "128x128_420_10le.yuv",
        "b48b0d714a3b09413284810df262a3dd67e5b646ad766e1990909f4926323cfa",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/176x144_420_8le.yuv",
        "176x144_420_8le.yuv",
        "251e4d9dcf092bdcb629078f4b1d02ed0664f270e175098c023669503e1bf3bf",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/176x144_420_10le.yuv",
        "176x144_420_10le.yuv",
        "ac1126da4de31110e9f23b4e2644b1dfdeb56fd2d9f57910f3de38e31dad3c52",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/352x288_420_8le.yuv",
        "352x288_420_8le.yuv",
        "9e19c6e728a065c0a518c59a1bc78928afcb165ec6123102c57c5a99bdbcd46a",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/352x288_420_10le.yuv",
        "352x288_420_10le.yuv",
        "cd04579c7115e33d01a874f4167a99ba73e165eb5086b5ffa66a8b018e13efb0",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/720x480_420_8le.yuv",
        "720x480_420_8le.yuv",
        "8868cdd209695ae8156009d55ae54143403d84d3526ca48c434865ac1f4c99ae",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/720x480_420_10le.yuv",
        "720x480_420_10le.yuv",
        "d008cea45a07efcd694929526faf0b100ceb80ffd74a933bd14676467d44fe07",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/1920x1080_420_8le.yuv",
        "1920x1080_420_8le.yuv",
        "9cdc96522bd15977445459aa144d019029e65e76c1893c11f89666645e8afc1e",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/1920x1080_420_10le.yuv",
        "1920x1080_420_10le.yuv",
        "9dcc10ded82276a3f80162c5eadb1215111fd8482ce5a4ddce88dfa7015ece4e",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/3840x2160_420_8le.yuv",
        "3840x2160_420_8le.yuv",
        "0e7e4b10bbdbee7588e4ad0a65ddf78195836d59c68c486855391f4ce9b3193b",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/3840x2160_420_10le.yuv",
        "3840x2160_420_10le.yuv",
        "64bc23169107266e17aee4e771c5b56e1caef8cc74992663c20a32ae128bab9c",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/7680x4320_420_8le.yuv",
        "7680x4320_420_8le.yuv",
        "0a7799afe89ad34aeb62b293c4c310a9a15c34961e9203a8499027ad775929b0",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/7680x4320_420_10le.yuv",
        "7680x4320_420_10le.yuv",
        "efedcfd080cd884c3e01b7f85dfeaf9e136ff8131c332b830d0607a35787ba08",
        "vulkancts/data/vulkan/video/yuv",
        ""),
]

def parseArgs ():
    versionsForInsecure = ((2,7,9), (3,4,3))
    versionsForInsecureStr = ' or '.join(('.'.join(str(x) for x in v)) for v in versionsForInsecure)

    parser = argparse.ArgumentParser(description = "Fetch external sources")
    parser.add_argument('--clean', dest='clean', action='store_true', default=False,
                        help='Remove sources instead of fetching')
    parser.add_argument('--insecure', dest='insecure', action='store_true', default=False,
                        help="Disable certificate check for external sources."
                        " Minimum python version required " + versionsForInsecureStr)
    parser.add_argument('--protocol', dest='protocol', default='https', choices=['ssh', 'https'],
                        help="Select protocol to checkout git repositories.")
    parser.add_argument('--force', dest='force', action='store_true', default=False,
                        help="Pass --force to git fetch and checkout commands")
    parser.add_argument("-v", "--verbose",
                        dest="verbose",
                        action="store_true",
                        help="Enable verbose logging")
    args = parser.parse_args()

    if args.insecure:
        for versionItem in versionsForInsecure:
            if (sys.version_info.major == versionItem[0]):
                if sys.version_info < versionItem:
                    parser.error("For --insecure minimum required python version is " +
                                versionsForInsecureStr)
                break;

    return args

def run(*popenargs, **kwargs):
    process = subprocess.Popen(*popenargs, **kwargs)

    try:
        stdout, stderr = process.communicate(None)
    except KeyboardInterrupt:
        # Terminate the process, wait and propagate.
        process.terminate()
        process.wait()
        raise
    except:
        # With any other exception, we _kill_ the process and propagate.
        process.kill()
        process.wait()
        raise
    else:
        # Everything good, fetch the retcode and raise exception if needed.
        retcode = process.poll()
        if retcode:
            raise subprocess.CalledProcessError(retcode, process.args, output=stdout, stderr=stderr)

if __name__ == "__main__":
    args = parseArgs()
    initializeLogger(args.verbose)

    try:
        for pkg in PACKAGES:
            if args.clean:
                pkg.clean()
            else:
                pkg.update(args.protocol, args.force)
    except KeyboardInterrupt:
        sys.exit("") # Returns 1.
