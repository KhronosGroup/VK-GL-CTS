# Script for checking which projects have unsubmitted modifications in them.
#
# Usage:
# - recommended to add a alias/bat/sh for a shorter command
# - running without parameters will check any existing known dE projects.
# - can give projects names on command line, if only wish to check a sub-set
#   e.g., git-check.py delibs deqp

import os
import sys

COMMANDS	= ["pull", "push", "check"]
ALL_REPOS	= ["delibs", "deqp", "movies", "domeni", "demisc"]

# Defaults.
command = "check"
repos	= ALL_REPOS

# Parse command line.
numArgs = len(sys.argv)
if (numArgs == 1):
	pass 
else:
	if (sys.argv[1] in COMMANDS):
		command = sys.argv[1]
		if (numArgs > 2):
			repos = sys.argv[2:]
	else:
		repos = sys.argv[1:]

def findRepo(x):
	for repo in ALL_REPOS:
		if repo.startswith(x):
			return repo
	print "%s not a valid repository directory" % x
	sys.exit(1)

repoDirs = [findRepo(x) for x in repos]

# Find git base repo directory.
oldDir		= os.getcwd()
baseDir 	= os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), "../.."))
foundAny	= False

# Execute the command.
print "## Executing '%s' on repos: %s" % (command.upper(), ", ".join(repoDirs))
print ""

for gitDir in repoDirs:
	subDir = os.path.join(baseDir, gitDir)
	if os.path.exists(subDir):
		foundAny = True
		print "***** Check directory '%s' *****" % subDir
		os.chdir(subDir)
		if command == "check":
			os.system("git status")
			os.system("git push --dry-run")
		elif command == "push":
			os.system("git push")
		elif command == "pull":
			os.system("git pull")
		else:
			assert False
		print ""

if not foundAny:
	print "No subdirs found -- tried %s" % repoDirs
	print "Searching in '%s'" % baseDir

os.chdir(oldDir)
