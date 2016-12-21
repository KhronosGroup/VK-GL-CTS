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
import urllib2
import hashlib
import argparse
import subprocess

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "scripts"))

from build.common import *

EXTERNAL_DIR	= os.path.realpath(os.path.normpath(os.path.dirname(__file__)))

def computeChecksum (data):
	return hashlib.sha256(data).hexdigest()

class Source:
	def __init__(self, baseDir, extractDir):
		self.baseDir		= baseDir
		self.extractDir		= extractDir

	def clean (self):
		fullDstPath = os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir)
		if os.path.exists(fullDstPath):
			shutil.rmtree(fullDstPath, ignore_errors=False)

class SourcePackage (Source):
	def __init__(self, url, filename, checksum, baseDir, extractDir = "src", postExtract=None):
		Source.__init__(self, baseDir, extractDir)
		self.url			= url
		self.filename		= filename
		self.checksum		= checksum
		self.archiveDir		= "packages"
		self.postExtract	= postExtract

	def clean (self):
		Source.clean(self)
		self.removeArchives()

	def update (self):
		if not self.isArchiveUpToDate():
			self.fetchAndVerifyArchive()

		# \note No way to verify that extracted contents match archive, re-extract
		Source.clean(self)
		self.extract()

	def removeArchives (self):
		archiveDir = os.path.join(EXTERNAL_DIR, pkg.baseDir, pkg.archiveDir)
		if os.path.exists(archiveDir):
			shutil.rmtree(archiveDir, ignore_errors=False)

	def isArchiveUpToDate (self):
		archiveFile = os.path.join(EXTERNAL_DIR, pkg.baseDir, pkg.archiveDir, pkg.filename)
		if os.path.exists(archiveFile):
			return computeChecksum(readFile(archiveFile)) == self.checksum
		else:
			return False

	def fetchAndVerifyArchive (self):
		print "Fetching %s" % self.url

		req			= urllib2.urlopen(self.url)
		data		= req.read()
		checksum	= computeChecksum(data)
		dstPath		= os.path.join(EXTERNAL_DIR, self.baseDir, self.archiveDir, self.filename)

		if checksum != self.checksum:
			raise Exception("Checksum mismatch for %s, exepected %s, got %s" % (self.filename, self.checksum, checksum))

		if not os.path.exists(os.path.dirname(dstPath)):
			os.mkdir(os.path.dirname(dstPath))

		writeFile(dstPath, data)

	def extract (self):
		print "Extracting %s to %s/%s" % (self.filename, self.baseDir, self.extractDir)

		srcPath	= os.path.join(EXTERNAL_DIR, self.baseDir, self.archiveDir, self.filename)
		tmpPath	= os.path.join(EXTERNAL_DIR, ".extract-tmp-%s" % self.baseDir)
		dstPath	= os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir)
		archive	= tarfile.open(srcPath)

		if os.path.exists(tmpPath):
			shutil.rmtree(tmpPath, ignore_errors=False)

		os.mkdir(tmpPath)

		archive.extractall(tmpPath)
		archive.close()

		extractedEntries = os.listdir(tmpPath)
		if len(extractedEntries) != 1 or not os.path.isdir(os.path.join(tmpPath, extractedEntries[0])):
			raise Exception("%s doesn't contain single top-level directory" % self.filename)

		topLevelPath = os.path.join(tmpPath, extractedEntries[0])

		if not os.path.exists(dstPath):
			os.mkdir(dstPath)

		for entry in os.listdir(topLevelPath):
			if os.path.exists(os.path.join(dstPath, entry)):
				raise Exception("%s exists already" % entry)

			shutil.move(os.path.join(topLevelPath, entry), dstPath)

		shutil.rmtree(tmpPath, ignore_errors=True)

		if self.postExtract != None:
			self.postExtract(dstPath)

class GitRepo (Source):
	def __init__(self, url, revision, baseDir, extractDir = "src"):
		Source.__init__(self, baseDir, extractDir)
		self.url		= url
		self.revision	= revision

	def update (self):
		fullDstPath = os.path.join(EXTERNAL_DIR, self.baseDir, self.extractDir)

		if not os.path.exists(fullDstPath):
			execute(["git", "clone", "--no-checkout", self.url, fullDstPath])

		pushWorkingDir(fullDstPath)
		try:
			execute(["git", "fetch", self.url, "+refs/heads/*:refs/remotes/origin/*"])
			execute(["git", "checkout", self.revision])
		finally:
			popWorkingDir()

def postExtractLibpng (path):
	shutil.copy(os.path.join(path, "scripts", "pnglibconf.h.prebuilt"),
				os.path.join(path, "pnglibconf.h"))

PACKAGES = [
	SourcePackage(
		"http://zlib.net/zlib-1.2.8.tar.gz",
		"zlib-1.2.8.tar.gz",
		"36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d",
		"zlib"),
	SourcePackage(
		"http://prdownloads.sourceforge.net/libpng/libpng-1.6.17.tar.gz",
		"libpng-1.6.17.tar.gz",
		"a18233c99e1dc59a256180e6871d9305a42e91b3f98799b3ceb98e87e9ec5e31",
		"libpng",
		postExtract = postExtractLibpng),
	GitRepo(
		"https://github.com/KhronosGroup/SPIRV-Tools.git",
		"f7e63786a919040cb2e0e572d960a0650f2c2881",
		"spirv-tools"),
	GitRepo(
		"https://github.com/KhronosGroup/glslang.git",
		"d02dc5d05ad1f63db8d37fda9928a4d59e3c132d",
		"glslang"),
]

def parseArgs ():
	parser = argparse.ArgumentParser(description = "Fetch external sources")
	parser.add_argument('--clean', dest='clean', action='store_true', default=False,
						help='Remove sources instead of fetching')
	return parser.parse_args()

if __name__ == "__main__":
	args = parseArgs()

	for pkg in PACKAGES:
		if args.clean:
			pkg.clean()
		else:
			pkg.update()
