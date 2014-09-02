# -*- coding: utf-8 -*-

import sys
import os
import time
import string

import common

def launch (extraArgs = ""):
	curDir = os.getcwd()

	try:
		os.chdir(common.ANDROID_DIR)

		adbCmd = common.shellquote(common.ADB_BIN)
		if len(extraArgs) > 0:
			adbCmd += " %s" % extraArgs

		print "Launching dEQP ExecService..."
		common.execute("%s forward tcp:50016 tcp:50016" % adbCmd)
		common.execute("%s shell setprop log.tag.dEQP DEBUG" % adbCmd)
		common.execute("%s shell am start -n com.drawelements.deqp/.execserver.ServiceStarter" % adbCmd)
		print "ExecService launched on device"
	finally:
		# Restore working dir
		os.chdir(curDir)
	
if __name__ == "__main__":
	if len(sys.argv) > 1:
		launch(string.join(sys.argv[1:], " "))
	else:
		launch()
