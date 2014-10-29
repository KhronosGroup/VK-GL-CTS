# -*- coding: utf-8 -*-

import os
import sys
import shutil
import tarfile
import urllib2
import hashlib
import argparse

EXTERNAL_DIR	= os.path.realpath(os.path.normpath(os.path.dirname(__file__)))

class SourcePackage:
	def __init__(self, url, filename, checksum, dstDir, postExtract=None):
		self.url			= url
		self.filename		= filename
		self.checksum		= checksum
		self.dstDir			= dstDir
		self.postExtract	= postExtract

def computeChecksum (data):
	return hashlib.sha256(data).hexdigest()

def clean (pkg):
	srcPath = os.path.join(EXTERNAL_DIR, pkg.dstDir)

	for entry in os.listdir(srcPath):
		if entry == "CMakeLists.txt":
			continue

		fullPath = os.path.join(srcPath, entry)

		if os.path.isfile(fullPath):
			os.unlink(fullPath)
		elif os.path.isdir(fullPath):
			shutil.rmtree(fullPath, ignore_errors=False)

def fetch (pkg):
	print "Fetching %s" % pkg.url

	req			= urllib2.urlopen(pkg.url)
	data		= req.read()
	checksum	= computeChecksum(data)
	dstPath		= os.path.join(EXTERNAL_DIR, pkg.filename)

	if checksum != pkg.checksum:
		raise Exception("Checksum mismatch for %s, exepected %s, got %s" % (pkg.filename, pkg.checksum, checksum))

	out = open(dstPath, 'wb')
	out.write(data)
	out.close()

def extract (pkg):
	print "Extracting %s to %s" % (pkg.filename, pkg.dstDir)

	srcPath	= os.path.join(EXTERNAL_DIR, pkg.filename)
	tmpPath	= os.path.join(EXTERNAL_DIR, ".extract-tmp-%s" % pkg.dstDir)
	dstPath	= os.path.join(EXTERNAL_DIR, pkg.dstDir)
	archive	= tarfile.open(srcPath)

	if os.path.exists(tmpPath):
		shutil.rmtree(tmpPath, ignore_errors=False)

	os.mkdir(tmpPath)

	archive.extractall(tmpPath)
	archive.close()

	extractedEntries = os.listdir(tmpPath)
	if len(extractedEntries) != 1 or not os.path.isdir(os.path.join(tmpPath, extractedEntries[0])):
		raise Exception("%s doesn't contain single top-level directory") % pkg.filename

	topLevelPath = os.path.join(tmpPath, extractedEntries[0])

	for entry in os.listdir(topLevelPath):
		if os.path.exists(os.path.join(dstPath, entry)):
			print "  skipping %s" % entry
			continue

		shutil.move(os.path.join(topLevelPath, entry), dstPath)

	shutil.rmtree(tmpPath, ignore_errors=True)

	if pkg.postExtract != None:
		pkg.postExtract(dstPath)

def postExtractLibpng (path):
	shutil.copy(os.path.join(path, "scripts", "pnglibconf.h.prebuilt"),
				os.path.join(path, "pnglibconf.h"))

PACKAGES = [
	SourcePackage("http://zlib.net/zlib-1.2.8.tar.gz",
				  "zlib-1.2.8.tar.gz",
				  "36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d",
				  "zlib"),
	SourcePackage("http://www.imagemagick.org/download/delegates/libpng-1.6.14.tar.gz",
				  "libpng-1.6.14.tar.gz",
				  "e6cab38f051bfc66929e766c1e67eba6fafac9e0f463ee3bbeb4ac16f173fe8e",
				  "libpng",
				  postExtract = postExtractLibpng),
]

def parseArgs ():
	parser = argparse.ArgumentParser(description = "Fetch external sources")
	parser.add_argument('--clean-only', dest='cleanOnly', action='store_true', default=False,
						help='Clean only, do not fetch/extract')
	parser.add_argument('--keep-archive', dest='keepArchive', action='store_true', default=False,
						help='Keep archive after extracting')
	return parser.parse_args()

if __name__ == "__main__":
	args = parseArgs()

	for pkg in PACKAGES:
		clean(pkg)

		if args.cleanOnly:
			continue

		fetch(pkg)
		extract(pkg)

		if not args.keepArchive:
			os.unlink(os.path.join(EXTERNAL_DIR, pkg.filename))
