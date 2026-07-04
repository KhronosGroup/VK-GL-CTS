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
import string
import argparse
import tempfile
import shutil
import fnmatch
import subprocess

scriptPath = os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *
from ctsbuild.config import *
from ctsbuild.build import *

class Module:
    def __init__ (self, name, dirName, binName):
        self.name = name
        self.dirName = dirName
        self.binName = binName

VULKAN_MODULE = Module("dEQP-VK", "../external/vulkancts/modules/vulkan", "deqp-vk")
DEFAULT_BUILD_DIR = os.path.join(tempfile.gettempdir(), "spirv-binaries", "{targetName}-{buildType}")
DEFAULT_TARGET = "null"
DEFAULT_DST_DIR = os.path.join(DEQP_DIR, "external", "vulkancts", "data", "vulkan", "prebuilt")

def getBuildConfig (buildPathPtrn, targetName, buildType):
    buildPath = buildPathPtrn.format(
        targetName = targetName,
        buildType = buildType)

    return BuildConfig(buildPath, buildType, ["-DDEQP_TARGET=%s" % targetName])

def cleanDstDir (dstPath):
    binFiles = [f for f in os.listdir(dstPath) if os.path.isfile(os.path.join(dstPath, f)) and fnmatch.fnmatch(f, "*.spv")]

    for binFile in binFiles:
        print("Removing %s" % os.path.join(dstPath, binFile))
        os.remove(os.path.join(dstPath, binFile))

def execBuildPrograms (buildCfg, generator, module, dstPath, vulkanVersion, parallelExecutors, fractions):
    fullDstPath = os.path.realpath(dstPath)
    workDir = os.path.join(buildCfg.getBuildDir(), "modules", module.dirName)

    pushWorkingDir(workDir)

    try:
        binPath = generator.getBinaryPath(buildCfg.getBuildType(), os.path.join(".", "vk-build-programs"))

        if parallelExecutors <= 1 and fractions <= 1:
            execute([binPath, "--validate-spv", "--dst-path", fullDstPath, "--target-vulkan-version", vulkanVersion])
            return

        # Run the work as `fractions` disjoint --deqp-fraction pieces, with
        # at most `parallelExecutors` vk-build-programs subprocesses live at
        # once (a worker pool). Each fraction writes its own dst dir;
        # outputs are NOT merged. Suitable for sanity-check use where the
        # registry is discarded.
        running = {}   # pid -> (fractionIdx, Popen)
        nextFraction = 0
        failed = []

        def launch(k):
            fractionDst = os.path.join(fullDstPath, "fraction-%d" % k)
            if not os.path.exists(fractionDst):
                os.makedirs(fractionDst)
            cmd = [binPath, "--validate-spv",
                   "--dst-path", fractionDst,
                   "--target-vulkan-version", vulkanVersion,
                   "--fraction", "%d,%d" % (k, fractions),
                   "--log-filename", os.path.join(fractionDst, "TestResults-%d-of-%d.qpa" % (k, fractions))]
            p = subprocess.Popen(cmd)
            running[p.pid] = (k, p)

        # Prime the pool.
        while nextFraction < fractions and len(running) < parallelExecutors:
            launch(nextFraction)
            nextFraction += 1

        # Refill as fractions complete.
        while running:
            pid, status = os.wait()
            k, p = running.pop(pid)
            # os.wait() has already reaped the child, so calling p.wait() here
            # would hit ChildProcessError internally and silently report 0,
            # hiding fraction failures (e.g. shaders that fail to build). Decode
            # the real exit code from the status returned by os.wait() instead.
            rc = os.waitstatus_to_exitcode(status)
            p.returncode = rc  # keep the Popen object consistent / silence its destructor
            if rc != 0:
                failed.append((k, rc))
            if nextFraction < fractions:
                launch(nextFraction)
                nextFraction += 1

        if failed:
            raise Exception("vk-build-programs fractions failed: %s" % failed)
    finally:
        popWorkingDir()

def parseArgs ():
    parser = argparse.ArgumentParser(description = "Build SPIR-V programs",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-b",
                        "--build-dir",
                        dest="buildDir",
                        default=DEFAULT_BUILD_DIR,
                        help="Temporary build directory")
    parser.add_argument("-t",
                        "--build-type",
                        dest="buildType",
                        default="Debug",
                        help="Build type")
    parser.add_argument("-c",
                        "--deqp-target",
                        dest="targetName",
                        default=DEFAULT_TARGET,
                        help="dEQP build target")
    parser.add_argument("-d",
                        "--dst-path",
                        dest="dstPath",
                        default=DEFAULT_DST_DIR,
                        help="Destination path")
    parser.add_argument("-u",
                        "--target-vulkan-version",
                        dest="vulkanVersion",
                        default="1.2",
                        choices=["1.0", "1.1", "1.2"],
                        help="Target Vulkan version")
    parser.add_argument("-v", "--verbose",
                        dest="verbose",
                        action="store_true",
                        help="Enable verbose logging")
    parser.add_argument("-p", "--parallel-executors",
                        dest="parallelExecutors",
                        type=int,
                        default=1,
                        help="Maximum vk-build-programs subprocesses to run concurrently (sanity-check use only; outputs not merged)")
    parser.add_argument("-f", "--fractions",
                        dest="fractions",
                        type=int,
                        default=0,
                        help="Number of fractions to divide the work into; defaults to --parallel-executors")
    args = parser.parse_args()
    if args.fractions <= 0:
        args.fractions = args.parallelExecutors
    if args.fractions < args.parallelExecutors:
        args.fractions = args.parallelExecutors
    return args

if __name__ == "__main__":
    args = parseArgs()
    initializeLogger(args.verbose)

    generator = ANY_GENERATOR
    buildCfg = getBuildConfig(args.buildDir, args.targetName, args.buildType)
    module = VULKAN_MODULE

    build(buildCfg, generator, ["vk-build-programs"])

    if not os.path.exists(args.dstPath):
        os.makedirs(args.dstPath)

    execBuildPrograms(buildCfg, generator, module, args.dstPath, args.vulkanVersion, args.parallelExecutors, args.fractions)
