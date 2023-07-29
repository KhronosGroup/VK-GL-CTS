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
import subprocess
import sys

TEXT_FILE_EXTENSION = [
    ".bat",
    ".c",
    ".cfg",
    ".cmake",
    ".cpp",
    ".css",
    ".h",
    ".hh",
    ".hpp",
    ".html",
    ".inl",
    ".java",
    ".js",
    ".m",
    ".mk",
    ".mm",
    ".py",
    ".rule",
    ".sh",
    ".test",
    ".txt",
    ".xml",
    ".xsl",
    ]

BINARY_FILE_EXTENSION = [
    ".bin",
    ".png",
    ".pkm",
    ".xcf",
    ".nspv",
    ".h264",
    ".h265",
    ".mp4",
    ".diff"
    ]

def isTextFile (filePath):
    # Special case for a preprocessor test file that uses a non-ascii/utf8 encoding
    if filePath.endswith("preprocessor.test"):
        return False
    # Special case for clang-format which is a baked binary from clang
    if filePath.endswith("clang-format"):
        return False

    ext = os.path.splitext(filePath)[1]
    if ext in TEXT_FILE_EXTENSION:
        return True
    if ext in BINARY_FILE_EXTENSION:
        return False

    # Analyze file contents, zero byte is the marker for a binary file
    f = open(filePath, "rb")

    TEST_LIMIT = 1024
    nullFound = False
    numBytesTested = 0

    byte = f.read(1)
    while byte and numBytesTested < TEST_LIMIT:
        if byte == "\0":
            nullFound = True
            break

        byte = f.read(1)
        numBytesTested += 1

    f.close()
    return not nullFound

def getProjectPath ():
    # File system hierarchy is fixed
    scriptDir = os.path.dirname(os.path.abspath(__file__))
    projectDir = os.path.normpath(os.path.join(scriptDir, "../.."))
    return projectDir

def git (*args):
    process = subprocess.Popen(['git'] + list(args), cwd=getProjectPath(), stdout=subprocess.PIPE)
    output = process.communicate()[0]
    if process.returncode != 0:
        raise Exception("Failed to execute '%s', got %d" % (str(args), process.returncode))
    return output

def getAbsolutePathPathFromProjectRelativePath (projectRelativePath):
    return os.path.normpath(os.path.join(getProjectPath(), projectRelativePath))

def getChangedFiles ():
    # Added, Copied, Moved, Renamed
    output = git('diff', '--cached', '--name-only', '-z', '--diff-filter=ACMR')
    if not output:
        return []
    relativePaths = output.decode().split('\0')[:-1] # remove trailing ''
    return [getAbsolutePathPathFromProjectRelativePath(path) for path in relativePaths]

def getFilesChangedSince (commit):
    # Get all the files changed since a given commit
    output = git('diff', '--name-only', '-U0', '-z', '--no-color', '--no-relative', '--diff-filter=ACMR', commit)
    relativePaths = output.decode().split('\0')[:-1] # remove trailing ''
    return [getAbsolutePathPathFromProjectRelativePath(path) for path in relativePaths]

def getFilesCurrentlyDirty ():
    # Get all the files currently dirty and uncommitted
    return getFilesChangedSize('HEAD')

def getFilesModifiedSinceLastCommit ():
    # Try to get only the modified files.  In a shallow clone with depth 1,
    # HEAD^ doesn't exist, so we have no choice but to return all the files.
    try:
        return getFilesChangedSize('HEAD^')
    except:
        return getAllProjectFiles()

def getAllProjectFiles ():
    output = git('ls-files', '--cached', '-z').decode()
    relativePaths = output.split('\0')[:-1] # remove trailing ''
    return [getAbsolutePathPathFromProjectRelativePath(path) for path in relativePaths]

def runCommand (command):
    process = runCommandAsync(command)
    waitAsyncCommand(process, command)

def runCommandAsync (command):
    try:
        return subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError as e:
        raise RuntimeError('Failed to run command "%s": %s' % (' '.join(command), e.strerror))

def waitAsyncCommand (process, command):
    (out, err) = process.communicate()
    if process.returncode == 0:
        return out
    else:
        print('Failed to run command "%s": %s' % (' '.join(command), err))
        sys.exit(process.returncode)
