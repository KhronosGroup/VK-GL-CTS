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
import argparse

import common

def install (extraArgs = [], printPrefix=""):
	print printPrefix + "Removing old dEQP Package...\n",
	common.execArgsInDirectory([common.ADB_BIN] + extraArgs + [
			'uninstall',
			'com.drawelements.deqp'
		], common.ANDROID_DIR, printPrefix)
	print printPrefix + "Remove complete\n",

	print printPrefix + "Installing dEQP Package...\n",
	common.execArgsInDirectory([common.ADB_BIN] + extraArgs + [
			'install',
			'-r',
			'package/bin/dEQP-debug.apk'
		], common.ANDROID_DIR, printPrefix)
	print printPrefix + "Install complete\n",

def installToDevice (device, printPrefix=""):
	if len(printPrefix) == 0:
		print "Installing to %s (%s)...\n" % (device.serial, device.model),
	else:
		print printPrefix + "Installing to %s\n" % device.serial,

	install(['-s', device.serial], printPrefix)

def installToDevices (devices, doParallel):
	padLen = max([len(device.model) for device in devices])+1
	if doParallel:
		common.parallelApply(installToDevice, [(device, ("(%s):%s" % (device.model, ' ' * (padLen - len(device.model))))) for device in devices]);
	else:
		common.serialApply(installToDevice, [(device, ) for device in devices]);

def installToAllDevices (doParallel):
	devices = common.getDevices(common.ADB_BIN)
	installToDevices(devices, doParallel)

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument('-p', '--parallel', dest='doParallel', action="store_true", help="Install package in parallel.")
	parser.add_argument('-s', '--serial', dest='serial', type=str, nargs='+', help="Install package to device with serial number.")
	parser.add_argument('-a', '--all', dest='all', action="store_true", help="Install to all devices.")

	args = parser.parse_args()

	if args.all:
		installToAllDevices(args.doParallel)
	else:
		if args.serial == None:
			devices = common.getDevices(common.ADB_BIN)
			if len(devices) == 0:
				common.die('No devices connected')
			elif len(devices) == 1:
				installToDevice(devices[0])
			else:
				print "More than one device connected:"
				for i in range(0, len(devices)):
					print "%3d: %16s %s" % ((i+1), devices[i].serial, devices[i].model)

				deviceNdx = int(raw_input("Choose device (1-%d): " % len(devices)))
				installToDevice(devices[deviceNdx-1])
		else:
			devices = common.getDevices(common.ADB_BIN)

			devices = [dev for dev in devices if dev.serial in args.serial]
			devSerials = [dev.serial for dev in devices]
			notFounds = [serial for serial in args.serial if not serial in devSerials]

			for notFound in notFounds:
				print("Couldn't find device matching serial '%s'" % notFound)

			installToDevices(devices, args.doParallel)
