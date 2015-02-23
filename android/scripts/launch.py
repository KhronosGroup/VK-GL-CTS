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
