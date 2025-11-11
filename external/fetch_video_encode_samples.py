# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
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

    def update (self):
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
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/176x144_30_i420.yuv",
        "176x144_30_i420.yuv",
        "235d1727dc8b4d3766fcbb52a8bfc5a87bcb050d3d7a15785bd30fa4a473c8e4",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/352x288_15_i420.yuv",
        "352x288_15_i420.yuv",
        "6e0e1a026717237f9546dfbd29d5e2ebbad0a993cdab38921bb43291a464ccd4",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/720x480_420_8le.yuv",
        "720x480_420_8le.yuv",
        "8868cdd209695ae8156009d55ae54143403d84d3526ca48c434865ac1f4c99ae",
        "vulkancts/data/vulkan/video/yuv",
        ""),
    SourceFile(
        "https://storage.googleapis.com/vulkan-video-samples/yuv/1920x1080_420_8le.yuv",
        "1920x1080_420_8le.yuv",
        "9cdc96522bd15977445459aa144d019029e65e76c1893c11f89666645e8afc1e",
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
                pkg.update()
    except KeyboardInterrupt:
        sys.exit("") # Returns 1.
