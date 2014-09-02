# -*- coding: utf-8 -*-

import os
import sys
from fnmatch import fnmatch
from optparse import OptionParser

HEADER_PATTERNS				= ["*.hpp", "*.h"]
INDENTED_INCLUDE_PREFIX		= "#\tinclude "
IFNDEF_PREFIX				= "#ifndef "

def getIncludeGuardName (headerFile):
	return '_' + os.path.basename(headerFile).upper().replace('.', '_')

def getRedundantIncludeGuardErrors (fileName):
	f		= open(fileName, 'rb')
	errors	= []

	lineNumber = 1
	prevLine = None
	for line in f:
		if line.startswith(INDENTED_INCLUDE_PREFIX):
			if prevLine is not None and prevLine.startswith(IFNDEF_PREFIX):
				ifndefName		= prevLine[len(IFNDEF_PREFIX):-1]			# \note -1 to take out the newline.
				includeName		= line[len(INDENTED_INCLUDE_PREFIX)+1:-2]	# \note +1 to take out the beginning quote, -2 to take out the newline and the ending quote.
				if getIncludeGuardName(includeName) != ifndefName:
					errors.append("Invalid redundant include guard around line %d:" % lineNumber)
					errors.append("guard is %s but included file is %s" % (ifndefName, includeName))

		prevLine = line
		lineNumber += 1

	f.close()
	return errors

def isHeader (filename):
	for pattern in HEADER_PATTERNS:
		if fnmatch(filename, pattern):
			return True
	return False

def getFileList (path):
	allFiles = []
	if os.path.isfile(path):
		if isHeader(path):
			allFiles.append(path)
	else:
		for root, dirs, files in os.walk(path):
			for file in files:
				if isHeader(file):
					allFiles.append(os.path.join(root, file))
	return allFiles

if __name__ == "__main__":
	parser = OptionParser()
	parser.add_option("-q", "--quiet", action="store_true", dest="quiet", default=False, help="only print files with errors")

	(options, args)	= parser.parse_args()
	quiet			= options.quiet
	files			= []
	invalidFiles	= []

	for dir in args:
		files += getFileList(os.path.normpath(dir))

	print "Checking..."
	for file in files:
		if not quiet:
			print "  %s" % file

		errors = getRedundantIncludeGuardErrors(file)
		if errors:
			if quiet:
				print "  %s" % file
			for err in errors:
				print "    %s" % err
			invalidFiles.append(file)

	print ""
	if len(invalidFiles) > 0:
		print "Found %d files with invalid redundant include guards:" % len(invalidFiles)

		for file in invalidFiles:
			print "  %s" % file

		sys.exit(-1)
	else:
		print "All files have valid redundant include guards."
