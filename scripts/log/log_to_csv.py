# -*- coding: utf-8 -*-

import sys
from log_parser import BatchResultParser

def batchResultToCsv (filename):
	parser = BatchResultParser()
	results = parser.parseFile(filename)

	for result in results:
		print "%s,%s" % (result.name, result.statusCode)

if __name__ == "__main__":
	if len(sys.argv) != 2:
		print "%s: [qpa log]" % sys.argv[0]
		sys.exit(-1)

	batchResultToCsv(sys.argv[1])
