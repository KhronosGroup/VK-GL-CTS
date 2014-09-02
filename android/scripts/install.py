# -*- coding: utf-8 -*-

import sys
import os
import time
import string

import common

def install (extraArgs = ""):
	curDir = os.getcwd()
	try:
		os.chdir(common.ANDROID_DIR)

		adbCmd = common.shellquote(common.ADB_BIN)
		if len(extraArgs) > 0:
			adbCmd += " %s" % extraArgs

		print "Removing old dEQP Package..."
		common.execute("%s uninstall com.drawelements.deqp" % adbCmd)
		print ""

		print "Installing dEQP Package..."
		common.execute("%s install -r package/bin/dEQP-debug.apk" % adbCmd)
		print ""

	finally:
		# Restore working dir
		os.chdir(curDir)
		
if __name__ == "__main__":
	if len(sys.argv) > 1:
		install(string.join(sys.argv[1:], " "))
	else:
		install()
