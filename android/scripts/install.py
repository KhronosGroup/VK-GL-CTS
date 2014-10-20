# -*- coding: utf-8 -*-

import sys
import os
import time
import string

import common

def install (extraArgs = []):
	curDir = os.getcwd()
	try:
		os.chdir(common.ANDROID_DIR)

		print "Removing old dEQP Package..."
		common.execArgs([common.ADB_BIN] + extraArgs + [
				'uninstall',
				'com.drawelements.deqp'
			])
		print ""

		print "Installing dEQP Package..."
		common.execArgs([common.ADB_BIN] + extraArgs + [
				'install',
				'-r',
				'package/bin/dEQP-debug.apk'
			])
		print ""

	finally:
		# Restore working dir
		os.chdir(curDir)

def installToDevice (device):
	print "Installing to %s (%s)..." % (device.serial, device.model)
	install(['-s', device.serial])

def installToAllDevices ():
	devices = common.getDevices(common.ADB_BIN)
	for device in devices:
		installToDevice(device)

if __name__ == "__main__":
	if len(sys.argv) > 1:
		if sys.argv[1] == '-a':
			installToAllDevices()
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
