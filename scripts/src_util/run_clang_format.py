#!/usr/bin/env python3

# Clang Format Helper
# -------------------
#
# Copyright (c) 2023 The Khronos Group Inc.
# Copyright (c) 2023 Google LLC
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
# This script retrieves the list of files to format from git and feeds it to
# clang-format.  It requires the following environment variable:
#
#     CTS_CLANG_PATH=/path/to/clang-format
#
# which should point to clang-format from llvm 16.0.4
#
# Usage: python3 run-clang-format.py
#
# -v: verbose output

import argparse
import multiprocessing
import os
import re
import sys
import time

from common import getFilesModifiedSinceLastCommit, runCommandAsync, waitAsyncCommand

def getClangFormat():
    clang_format = os.environ.get('CTS_CLANG_FORMAT')
    if clang_format is not None:
        return clang_format

    print('CTS_CLANG_FORMAT is not defined.  Set this environment variable to path to clang_format')
    if not sys.platform.startswith('linux'):
        sys.exit(1)

    print('Falling back to scripts/src_util/clang-format')
    clang_format = os.path.join(os.path.dirname(__file__), "clang-format")
    return clang_format

def runClangFormat(files, thread_count, verbose):
    clang_format = getClangFormat()

    processes = []

    total = len(files)
    so_far = 0

    last_report_time = time.time()

    for file in files:
        so_far = so_far + 1
        if verbose:
            print('Formatting {}'.format(file))
        elif last_report_time + 1 < time.time():
            print('\rFormatting {}/{}'.format(so_far, total), end = '')
            last_report_time = last_report_time + 1

        # Make sure a maximum of thread_count processes are in flight
        if len(processes) > thread_count:
            waitAsyncCommand(*processes[0])
            processes = processes[1:]

        command = [clang_format, file, '-i']
        processes.append((runCommandAsync(command), command))

    for process in processes:
        waitAsyncCommand(*process)

    print('\rFormatted {}                        '.format(total))

def runClangFormatOnModifiedFiles(verbose):
    files = getFilesModifiedSinceLastCommit()

    # Only format files that can be formatted by clang-format.
    pattern = (r'.*\.(cpp|hpp|c|h|m|mm|hh|inc|js|java|json)')
    files = [file for file in files if re.match('^%s$' % pattern, file, re.IGNORECASE)]
    files = [f for f in files if 'vulkancts/scripts/src' not in f.replace('\\', '/')]
    files = [f for f in files if not re.match('.*external/openglcts/modules/runner/.*Mustpass.*\.hpp', f.replace('\\', '/'))]

    thread_count = min(8, multiprocessing.cpu_count())
    runClangFormat(files, thread_count, verbose)
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("-v", action="store_true", default=False, help="verbose")
    args = parser.parse_args()
    runClangFormatOnModifiedFiles(args.v)
