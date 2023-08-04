# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
# Copyright 2023 The Khronos Group Inc.
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
from argparse import ArgumentParser
from common import getChangedFiles, getAllProjectFiles

def checkFileSizeLimit (file, limitBytes):
    fileSize = os.path.getsize(file)
    if fileSize <= limitBytes:
        return True
    else:
        print(f"File size {fileSize} bytes exceeds the limit of {limitBytes} bytes for {file}")
        return False

def checkFilesSizeLimit (files, limitBytes):
    error = False
    for file in files:
        if not checkFileSizeLimit(file, limitBytes):
            error = True

    return not error

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-l", "--limit", required=True, type=int, help="Maximum file size allowed in MB")
    args = parser.parse_args()

    files = getAllProjectFiles()
    error = not checkFilesSizeLimit(files, args.limit * 1024 * 1024)

    if error:
        print("One or more checks failed")
        sys.exit(1)

    print("All checks passed")
