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
import urllib2
import hashlib

import registry

BASE_URL = ""

class RegistrySource:
	def __init__(self, filename, revision, checksum):
		self.filename	= filename
		self.revision	= revision
		self.checksum	= checksum

	def __hash__(self):
		return hash((self.filename, self.revision, self.checksum))

	def __eq__(self, other):
		return (self.filename, self.revision, self.checksum) == (other.filename, other.revision, other.checksum)

	def getFilename (self):
		return self.filename

	def getCacheFilename (self):
		return "r%d-%s" % (self.revision, self.filename)

	def getChecksum (self):
		return self.checksum

	def getRevision (self):
		return self.revision

	def getSourceUrl (self):
		return "https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/%s?r=%d" % (self.filename, self.revision)

def computeChecksum (data):
	return hashlib.sha256(data).hexdigest()

def fetchUrl (url):
	req		= urllib2.urlopen(url)
	data	= req.read()
	return data

def fetchFile (dstPath, url, checksum):
	def writeFile (filename, data):
		f = open(filename, 'wb')
		f.write(data)
		f.close()

	if not os.path.exists(os.path.dirname(dstPath)):
		os.makedirs(os.path.dirname(dstPath))

	print "Fetching %s" % url
	data		= fetchUrl(url)
	gotChecksum	= computeChecksum(data)

	if checksum != gotChecksum:
		raise Exception("Checksum mismatch, exepected %s, got %s" % (checksum, gotChecksum))

	writeFile(dstPath, data)

def checkFile (filename, checksum):
	def readFile (filename):
		f = open(filename, 'rb')
		data = f.read()
		f.close()
		return data

	if os.path.exists(filename):
		return computeChecksum(readFile(filename)) == checksum
	else:
		return False

g_registryCache = {}

def getRegistry (source):
	global g_registryCache

	if source in g_registryCache:
		return g_registryCache[source]

	cacheDir	= os.path.join(os.path.dirname(__file__), "cache")
	cachePath	= os.path.join(cacheDir, source.getCacheFilename())

	if not checkFile(cachePath, source.checksum):
		fetchFile(cachePath, source.getSourceUrl(), source.getChecksum())

	parsedReg	= registry.parse(cachePath)

	g_registryCache[source] = parsedReg

	return parsedReg
