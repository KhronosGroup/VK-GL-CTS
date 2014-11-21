# -*- coding: utf-8 -*-

from itertools import chain

INL_HEADER_TMPL = """\
/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from {registryName} revision {revision}.
 */\
"""

def genInlHeader (registryName, revision):
	return INL_HEADER_TMPL.format(
		registryName	= registryName,
		revision		= str(revision))

def genInlHeaderForSource (registrySource):
	return genInlHeaderForSource(registrySource.getFilename(), registrySource.getRevision())

def nextMod (val, mod):
	if val % mod == 0:
		return val + mod
	else:
		return int(val/mod)*mod + mod

def indentLines (lines):
	tabSize = 4

	# Split into columns
	lineColumns = [line.split("\t") for line in lines if line is not None]
	if len(lineColumns) == 0:
		return

	numColumns = max(len(line) for line in lineColumns)

	# Figure out max length per column
	columnLengths = [nextMod(max(len(line[ndx]) for line in lineColumns if len(line) > ndx), tabSize) for ndx in range(numColumns)]

	for line in lineColumns:
		indented = []
		for columnNdx, col in enumerate(line[:-1]):
			colLen	= len(col)
			while colLen < columnLengths[columnNdx]:
				col		+= "\t"
				colLen	 = nextMod(colLen, tabSize)
			indented.append(col)

		# Append last col
		indented.append(line[-1])
		yield "".join(indented)

def writeLines (filename, lines):
	with open(filename, 'wb') as f:
		for line in lines:
			if line is not None:
				f.write(line)
				f.write('\n')
	print filename

def writeInlFile (filename, header, source):
	writeLines(filename, chain([header], source))
