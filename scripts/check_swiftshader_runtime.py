# Copyright 2021 Google LLC.
# Copyright 2021 The Khronos Group Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Requirements to run the script:
# - Python3 (apt-get install -y python3.x)
# - GO      (apt-get install -y golang-go)
# - cmake   (version 3.13 or later)
# - ninja   (apt-get install -y ninja-build)
# - git     (sudo apt-get install -y git)

# GO dependencies needed:
# - crypto/openpgp (go get -u golang.org/x/crypto/openpgp...)

import os
import json
import tempfile
import subprocess
import sys

from argparse import ArgumentParser
from shutil import which, copyfile
from pathlib import Path
from datetime import datetime

# Check for correct python version (python3) before doing anything.
if sys.version_info.major < 3:
        raise RuntimeError("Python version needs to be 3 or greater.")

AP = ArgumentParser()
AP.add_argument(
    "-d",
    "--directory",
    metavar="DIRECTORY",
    type=str,
    help="Path to directory that will be used as root for cloning and file saving.",
    default=str(Path(tempfile.gettempdir()) / "deqp-swiftshader")
)
AP.add_argument(
    "-u",
    "--url",
    metavar="URL",
    type=str,
    help="URL of SwiftShader Git repository.",
    default="https://swiftshader.googlesource.com/SwiftShader",
)
AP.add_argument(
    "-l",
    "--vlayer_url",
    metavar="VURL",
    type=str,
    help="URL of Validation Layers Git repository.",
    default="https://github.com/KhronosGroup/Vulkan-ValidationLayers.git",
)
AP.add_argument(
    "-b",
    "--sws_build_type",
    metavar="SWS_BUILD_TYPE",
    type=str,
    help="SwiftShader build type.",
    choices=["debug", "release"],
    default="debug",
)
AP.add_argument(
    "-q",
    "--deqp_vk",
    metavar="DEQP_VK",
    type=str,
    help="Path to deqp-vk binary.",
)
AP.add_argument(
    "-v",
    "--vk_gl_cts",
    metavar="VK_GL_CTS",
    type=str,
    help="Path to vk-gl-cts source directory.",
)
AP.add_argument(
    "-w",
    "--vk_gl_cts_build",
    metavar="VK_GL_CTS_BUILD",
    type=str,
    help="Path to vk-gl-cts build directory.",
    default=str(Path(tempfile.gettempdir()) / "deqp-swiftshader" / "vk-gl-cts-build"),
)
AP.add_argument(
    "-t",
    "--vk_gl_cts_build_type",
    metavar="VK_GL_CTS_BUILD_TYPE",
    type=str,
    help="vk-gl-cts build type.",
    choices=["debug", "release"],
    default="debug",
)
AP.add_argument(
    "-r",
    "--recipe",
    metavar="RECIPE",
    type=str,
    help="Recipes to only run parts of script.",
    choices=["run-deqp", "check-comparison"],
    default="run-deqp",
)
AP.add_argument(
    "-f",
    "--files",
    nargs=2,
    metavar=("NEWER_FILE_PATH", "OLDER_FILE_PATH"),
    type=str,
    help="Compare two different run results.",
)
AP.add_argument(
    "-a",
    "--validation",
    metavar="VALIDATION",
    type=str,
    help="Enable vulkan validation layers.",
    choices=["true", "false"],
    default="false",
)
AP.add_argument(
    "-o",
    "--result_output",
    metavar="OUTPUT",
    type=str,
    help="Filename of the regres results.",
    default=str("result_" + str(datetime.now().strftime('%m_%d_%Y_%H_%M_%S')) + ".json"),
)

ARGS = AP.parse_args()

# Check that we have everything needed to run the script when using recipe run-deqp.
if ARGS.recipe == "run-deqp":
    if which("go") is None:
        raise RuntimeError("go not found. (apt-get install -y golang-go)")
    if which("cmake") is None:
        raise RuntimeError("CMake not found. (version 3.13 or later needed)")
    if which("ninja") is None:
        raise RuntimeError("Ninja not found. (apt-get install -y ninja-build)")
    if which("git") is None:
        raise RuntimeError("Git not found. (apt-get install -y git)")
    if ARGS.vk_gl_cts is None:
        raise RuntimeError("vk-gl-cts source directory must be provided. Use --help for more info.")

PARENT_DIR = Path(ARGS.directory).resolve()

SWS_SRC_DIR = PARENT_DIR / "SwiftShader"
SWS_BUILD_DIR = SWS_SRC_DIR / "build"
SWIFTSHADER_URL = ARGS.url

LAYERS_PARENT_DIR = Path(ARGS.directory).resolve()
LAYERS_SRC_DIR = LAYERS_PARENT_DIR / "Vulkan_Validation_Layers"
LAYERS_URL = ARGS.vlayer_url
LAYERS_BUILD_DIR = LAYERS_SRC_DIR / "build"

LINUX_SWS_ICD_DIR = SWS_BUILD_DIR / "Linux"
REGRES_DIR = SWS_SRC_DIR / "tests" / "regres"
RESULT_DIR = PARENT_DIR / "regres_results"
COMP_RESULTS_DIR = PARENT_DIR / "comparison_results"

VK_GL_CTS_ROOT_DIR = Path(ARGS.vk_gl_cts)
VK_GL_CTS_BUILD_DIR = Path(ARGS.vk_gl_cts_build)
MUSTPASS_LIST = VK_GL_CTS_ROOT_DIR / "external" / "vulkancts" / "mustpass" / "master" / "vk-default.txt"
if ARGS.deqp_vk is None:
    DEQP_VK_BINARY = VK_GL_CTS_BUILD_DIR / "external" / "vulkancts" / "modules" / "vulkan" / "deqp-vk"
else:
    DEQP_VK_BINARY = str(ARGS.deqp_vk)

new_pass = []
new_fail = []
new_crash = []
new_notsupported = []
has_been_removed = []
status_change = []
compatibility_warning = []
quality_warning = []
internal_errors = []
waivers = []

class Result:
    def __init__(self, filename):
        self.filename = filename
        self.f = open(filename)
        # Skip the first four lines and check that the file order has not been changed.
        tmp = ""
        for i in range(4):
            tmp = tmp + self.f.readline()
        if "Tests" not in tmp:
            raise RuntimeError("Skipped four lines, no starting line found. Has the file order changed?")

    # Reads one test item from the file.
    def readResult(self):
        while True:
            tmp = ""
            while "}" not in tmp:
                tmp = tmp + self.f.readline()
            if "Test" in tmp:
                tmp = tmp[tmp.find("{") : tmp.find("}") + 1]
                return json.loads(tmp)
            else:
                return None

    # Search for a test name. Returns the test data if found and otherwise False.
    def searchTest(self, test):
        line = self.f.readline()
        while line:
            if line.find(test) != -1:
                # Found the test.
                while "}" not in line:
                    line = line + self.f.readline()

                line = line[line.find("{") : line.find("}") + 1]
                return json.loads(line)
            line = self.f.readline()

# Run deqp-vk with regres.
def runDeqp(deqp_path, testlist_path):
    deqpVkParam = "--deqp-vk=" + deqp_path
    validationLayerParam = "--validation=" + ARGS.validation
    testListParam = "--test-list=" + testlist_path
    run(["./run_testlist.sh", deqpVkParam, validationLayerParam, testListParam], working_dir=REGRES_DIR)

# Run commands.
def run(command: str, working_dir: str = Path.cwd()) -> None:
    """Run command using subprocess.run()"""
    subprocess.run(command, cwd=working_dir, check=True)

# Set VK_ICD_FILENAMES
def setVkIcdFilenames():
    os.environ["VK_ICD_FILENAMES"] = str(LINUX_SWS_ICD_DIR / "vk_swiftshader_icd.json")
    print(f"VK_ICD_FILENAMES = {os.getenv('VK_ICD_FILENAMES')}")

# Choose the category/status to write results to.
def writeToStatus(test):
    if test['Status'] == "PASS":
        new_pass.append(test['Test'])
    elif test['Status'] == "FAIL":
        new_fail.append(test['Test'])
    elif test['Status'] == "NOT_SUPPORTED" or test['Status'] == "UNSUPPORTED":
        new_notsupported.append(test['Test'])
    elif test['Status'] == "CRASH":
        new_crash.append(test['Test'])
    elif test['Status'] == "COMPATIBILITY_WARNING":
        compatibility_warning.append(test['Test'])
    elif test['Status'] == "QUALITY_WARNING":
        quality_warning.append(test['Test'])
    elif test['Status'] == "INTERNAL_ERROR":
        internal_errors.append(test['Test'])
    elif test['Status'] == "WAIVER":
        waivers.append(test['Test'])
    else:
        raise RuntimeError(f"Expected PASS, FAIL, NOT_SUPPORTED, UNSUPPORTED, CRASH, COMPATIBILITY_WARNING, " +
                           f"QUALITY_WARNING, INTERNAL_ERROR or WAIVER as status, " +
                           f"got {test['Status']}. Is there an unhandled status case?")

# Compare two result.json files for regression.
def compareRuns(new_result, old_result):
    print(f"Comparing files: {old_result} and {new_result}")

    r0 = Result(new_result)
    r1 = Result(old_result)

    t0 = r0.readResult()
    t1 = r1.readResult()

    done = False

    while not done:
        # Old result file has ended, continue with new.
        if t1 == None and t0 != None:
            advance1 = False
            writeToStatus(t0)
        # New result file has ended, continue with old.
        elif t0 == None and t1 != None:
            advance0 = False
            has_been_removed.append(t1['Test'])
        # Both files have ended, stop iteration.
        elif t1 == None and t0 == None:
            done = True
        # By default advance both files.
        else:
            advance0 = True
            advance1 = True

            if t0['Test'] == t1['Test']:
                # The normal case where both files are in sync. Just check if the status matches.
                if t0['Status'] != t1['Status']:
                    status_change.append(f"{t0['Test']}, new status: {t0['Status']}, old status: {t1['Status']}")
                    print(f"Status changed: {t0['Test']} {t0['Status']} vs {t1['Status']}")
            else:
                # Create temporary objects for searching through the whole file.
                tmp0 = Result(r0.filename)
                tmp1 = Result(r1.filename)

                # Search the mismatching test cases from the opposite file.
                s0 = tmp0.searchTest(t1['Test'])
                s1 = tmp1.searchTest(t0['Test'])

                # Old test not in new results
                if not s0:
                    print(f"Missing old test {t1['Test']} from new file: {r0.filename}\n")
                    has_been_removed.append(t1['Test'])
                    # Don't advance this file since we already read a test case other than the missing one.
                    advance0 = False

                # New test not in old results
                if not s1:
                    print(f"Missing new test {t0['Test']} from old file: {r1.filename}\n")
                    writeToStatus(t0)
                    # Don't advance this file since we already read a test case other than the missing one.
                    advance1 = False

                if s0 and s1:
                    # This should never happen because the test cases are in alphabetical order.
                    # Print an error and bail out.
                    raise RuntimeError(f"Tests in different locations: {t0['Test']}\n")

            if not advance0 and not advance1:
                # An exotic case where both tests are missing from the other file.
                # Need to skip both.
                advance0 = True
                advance1 = True

        if advance0:
            t0 = r0.readResult()
        if advance1:
            t1 = r1.readResult()

    result_file = str(COMP_RESULTS_DIR / "comparison_results_") + str(datetime.now().strftime('%m_%d_%Y_%H_%M_%S')) + ".txt"
    print(f"Writing to file {result_file}")
    COMP_RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    with open(result_file, "w") as log_file:
        log_file.write("New passes:\n")
        for line in new_pass:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("New fails:\n")
        for line in new_fail:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("New crashes:\n")
        for line in new_crash:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("New not_supported:\n")
        for line in new_notsupported:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Tests removed:\n")
        for line in has_been_removed:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Status changes:\n")
        for line in status_change:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Compatibility warnings:\n")
        for line in compatibility_warning:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Quality warnings:\n")
        for line in quality_warning:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Internal errors:\n")
        for line in internal_errors:
            log_file.write(line + "\n")
        log_file.write("\n")

        log_file.write("Waiver:\n")
        for line in waivers:
            log_file.write(line + "\n")

    print(f"Comparison done. Results have been written to: {COMP_RESULTS_DIR}")

# Build VK-GL-CTS
def buildCts():
    VK_GL_CTS_BUILD_DIR.mkdir(parents=True, exist_ok=True)

    FETCH_SOURCES = str(VK_GL_CTS_ROOT_DIR / "external" / "fetch_sources.py")
    run([which("python3"), FETCH_SOURCES], working_dir=VK_GL_CTS_ROOT_DIR)

    # Build VK-GL-CTS
    buildType = "-DCMAKE_BUILD_TYPE=" + ARGS.vk_gl_cts_build_type
    run([which("cmake"), "-GNinja", str(VK_GL_CTS_ROOT_DIR), buildType], working_dir=VK_GL_CTS_BUILD_DIR)
    run([which("ninja"), "deqp-vk"], working_dir=VK_GL_CTS_BUILD_DIR)
    print(f"vk-gl-cts built to: {VK_GL_CTS_BUILD_DIR}")

# Clone and build SwiftShader and Vulkan validation layers.
def cloneSwsAndLayers():
    # Clone SwiftShader or update if it already exists.
    if not SWS_SRC_DIR.exists():
        SWS_SRC_DIR.mkdir(parents=True, exist_ok=True)
        run([which("git"), "clone", SWIFTSHADER_URL, SWS_SRC_DIR])
    else:
        run([which("git"), "pull", "origin"], working_dir=SWS_SRC_DIR)

    # Build SwiftShader.
    run([which("cmake"),
            "-GNinja",
            str(SWS_SRC_DIR),
            "-DSWIFTSHADER_BUILD_EGL:BOOL=OFF",
            "-DSWIFTSHADER_BUILD_GLESv2:BOOL=OFF",
            "-DSWIFTSHADER_BUILD_TESTS:BOOL=OFF",
            "-DINSTALL_GTEST=OFF",
            "-DBUILD_TESTING:BOOL=OFF",
            "-DENABLE_CTEST:BOOL=OFF",
            "-DCMAKE_BUILD_TYPE=" + ARGS.sws_build_type],
            working_dir=SWS_BUILD_DIR)
    run([which("cmake"), "--build", ".", "--target", "vk_swiftshader"], working_dir=SWS_BUILD_DIR)

    # Set Vulkan validation layers if flag is set.
    if ARGS.validation == "true":
        # Clone Vulkan validation layers or update if they already exist.
        if not LAYERS_SRC_DIR.exists():
            LAYERS_SRC_DIR.mkdir(parents=True, exist_ok=True)
            run([which("git"), "clone", LAYERS_URL, LAYERS_SRC_DIR])
        else:
            run([which("git"), "pull", "origin"], working_dir=LAYERS_SRC_DIR)

        # Build and set Vulkan validation layers.
        LAYERS_BUILD_DIR.mkdir(parents=True, exist_ok=True)
        UPDATE_DEPS = str(LAYERS_SRC_DIR / "scripts" / "update_deps.py")
        run([which("python3"), UPDATE_DEPS], working_dir=LAYERS_BUILD_DIR)
        run([which("cmake"),
                "-GNinja",
                "-C",
                "helper.cmake",
                LAYERS_SRC_DIR],
                working_dir=LAYERS_BUILD_DIR)
        run([which("cmake"), "--build", "."], working_dir=LAYERS_BUILD_DIR)
        LAYERS_PATH = str(LAYERS_BUILD_DIR / "layers")
        os.environ["VK_LAYER_PATH"] = LAYERS_PATH
    print(f"Tools cloned and built in: {PARENT_DIR}")

# Run cts with regres and move result files accordingly.
def runCts():
    setVkIcdFilenames()

    # Run cts and copy the resulting file to RESULT_DIR.
    print("Running cts...")
    runDeqp(str(DEQP_VK_BINARY), str(MUSTPASS_LIST))
    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    copyfile(str(REGRES_DIR / "results.json"), str(RESULT_DIR / ARGS.result_output))
    print("Run completed.")
    print(f"Result file copied to: {RESULT_DIR}")
    exit(0)

# Recipe for running cts.
if ARGS.recipe == "run-deqp":
    cloneSwsAndLayers()
    if ARGS.deqp_vk is None:
        buildCts()
    runCts()

# Recipe for only comparing the already existing result files.
if ARGS.recipe == "check-comparison":
    if ARGS.files is None:
        raise RuntimeError("No comparable files provided. Please provide them with flag --files. Use --help for more info.")
    newFile, oldFile = ARGS.files
    compareRuns(str(newFile), str(oldFile))
