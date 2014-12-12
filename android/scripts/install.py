# -*- coding: utf-8 -*-

import sys
import os
import time
import string

import common

def install (extraArgs = [], printPrefix=""):
	print printPrefix + "Removing old dEQP Package...\n",
	common.execArgsInDirectory([common.ADB_BIN] + extraArgs + [
			'uninstall',
			'com.drawelements.deqp'
		], common.ANDROID_DIR)
	print printPrefix + "Remove complete\n",

	print printPrefix + "Installing dEQP Package...\n",
	common.execArgsInDirectory([common.ADB_BIN] + extraArgs + [
			'install',
			'-r',
			'package/bin/dEQP-debug.apk'
		], common.ANDROID_DIR)
	print printPrefix + "Install complete\n",

def installToDevice (device, printPrefix=""):
	print printPrefix + "Installing to %s (%s)...\n" % (device.serial, device.model),
	install(['-s', device.serial], printPrefix)

def installToAllDevices (doParallel):
	devices = common.getDevices(common.ADB_BIN)
	padLen = max([len(device.model) for device in devices])+1
	if doParallel:
		common.parallelApply(installToDevice, [(device, ("(%s):%s" % (device.model, ' ' * (padLen - len(device.model))))) for device in devices]);
	else:
		common.serialApply(installToDevice, [(device, ) for device in devices]);

if __name__ == "__main__":
	if len(sys.argv) > 1:
		if sys.argv[1] == '-a':
			doParallel = '-p' in sys.argv[1:]
			installToAllDevices(doParallel)
		else:
			install(sys.argv[1:])
	else:
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
