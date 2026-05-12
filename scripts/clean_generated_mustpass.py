#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2026 The Khronos Group Inc.
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

"""Remove generated mustpass output files, keeping hand-maintained inputs.

The mustpass generators
(external/vulkancts/scripts/build_mustpass.py and
scripts/build_android_mustpass.py) write into two trees that mix
hand-maintained inputs with generated outputs.  This helper wipes only
the generated outputs so the next regen produces a fresh tree; any file
that the regen does NOT recreate becomes a `git status` deletion the
maintainer can commit (matching the workflow used for CL 20200 on main).

Hand-maintained paths kept (relative to the repo root):

  external/vulkancts/mustpass/main/src/
  external/vulkancts/mustpass/main/waivers.xml
  android/cts/main/src/

Files outside the two listed roots (e.g. android/cts/Android.bp,
android/cts/AndroidTest.xml, android/cts/runner/) are not touched.
"""

import os
import sys
import argparse
import subprocess

DEQP_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))

ROOTS = [
    ("external/vulkancts/mustpass/main", {"src", "waivers.xml"}),
    ("android/cts/main",                 {"src"}),
]


def listTrackedFiles (root):
    """Return tracked files under `root`, repo-relative POSIX paths."""
    out = subprocess.check_output(
        ["git", "-C", DEQP_DIR, "ls-files", "--", root],
        text=True)
    return [line for line in out.splitlines() if line]


def isUnderKeep (relPath, root, keep):
    """True if relPath sits under one of the keep entries beneath `root`."""
    # relPath is relative to repo root; e.g. "external/.../main/src/main.txt".
    inside = os.path.relpath(relPath, root).split(os.sep)
    return inside[0] in keep


def cleanRoot (root, keep, dryRun):
    absRoot = os.path.join(DEQP_DIR, root)
    if not os.path.isdir(absRoot):
        print("skip: %s does not exist" % root)
        return 0

    removed = 0
    for rel in listTrackedFiles(root):
        if isUnderKeep(rel, root, keep):
            continue
        absPath = os.path.join(DEQP_DIR, rel)
        if dryRun:
            print("would remove: %s" % rel)
            removed += 1
            continue
        os.remove(absPath)
        print("removed: %s" % rel)
        removed += 1
    return removed


def main ():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry-run", action="store_true",
                        help="List paths that would be removed; do not delete.")
    args = parser.parse_args()

    total = 0
    for root, keep in ROOTS:
        total += cleanRoot(root, keep, args.dry_run)
    verb = "would remove" if args.dry_run else "removed"
    print("%s %d tracked file(s) across %d root(s); directory structure left intact." %
          (verb, total, len(ROOTS)))


if __name__ == "__main__":
    main()
