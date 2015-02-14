# -*- coding: utf-8 -*-

import os
import re
import sys
import time
import fnmatch

SRC_FILE_PATTERNS		= [ "*.c", "*.h", "*.cpp", "*.hpp", "*.inl", "*.java", "*.aidl", "*.py" ]
COPYRIGHT_PATTERN		= r'Copyright \(C\) ([0-9]{4})(-[0-9]{4})? drawElements Ltd.'
COPYRIGHT_REPLACEMENT	= r'Copyright (C) \1-' + time.strftime("%Y") + r' drawElements Ltd.'

def isSrcFile (filename):
	for pattern in SRC_FILE_PATTERNS:
		if fnmatch.fnmatch(filename, pattern):
			return True
	return False

def findSrcFiles (dir):
	srcFiles = []
	for root, dirs, files in os.walk(dir):
		for file in files:
			if isSrcFile(file):
				srcFiles.append(os.path.join(root, file))
	return srcFiles

def processFile (filename):
	print filename
	file = open(filename, "rb")
	data = file.read()
	file.close()
	data = re.sub(COPYRIGHT_PATTERN, COPYRIGHT_REPLACEMENT, data)
	file = open(filename, "wb")
	file.write(data)
	file.close()

def processDir (dir):
	srcFiles = findSrcFiles(dir)
	for file in srcFiles:
		processFile(file)

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print sys.argv[0] + ": [directory]"
	else:
		processDir(sys.argv[1])
