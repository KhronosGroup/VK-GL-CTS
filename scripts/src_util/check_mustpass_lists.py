# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2026 The Khronos Group Inc.
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
from common import getProjectPath

MUSTPASS_ROOTS = [
    os.path.join("android", "cts", "main"),
    os.path.join("external", "vulkancts", "mustpass", "main"),
    ]

# Every file in a group directory must be listed in the index next to it,
# otherwise the mustpass generator no longer writes it and nothing runs it
def checkUnlistedFiles (root):
    error = False
    for name in sorted(os.listdir(root)):
        groupDir = os.path.join(root, name)
        if name == "src" or not os.path.isdir(groupDir):
            continue

        indexPath = groupDir + ".txt"
        listed = set()
        if os.path.isfile(indexPath):
            with open(indexPath) as f:
                listed = set(f.read().split())

        for dirPath, dirNames, fileNames in os.walk(groupDir):
            for fileName in fileNames:
                filePath = os.path.join(dirPath, fileName)
                if os.path.relpath(filePath, root).replace(os.sep, "/") not in listed:
                    print(f"{filePath} is not listed in {indexPath}")
                    error = True

    return not error

def checkMustpassLists ():
    error = False
    for root in MUSTPASS_ROOTS:
        if not checkUnlistedFiles(os.path.join(getProjectPath(), root)):
            error = True

    return not error

if __name__ == "__main__":
    if not checkMustpassLists():
        print("One or more checks failed")
        sys.exit(1)

    print("All checks passed")
