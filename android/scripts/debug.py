# -*- coding: utf-8 -*-

import sys
import os
import time
import string
import shutil
import subprocess
import signal
import argparse

import common

def getADBProgramPID(program):
	adbCmd	= common.shellquote(common.ADB_BIN)
	pid		= -1

	process = subprocess.Popen("%s shell ps" % adbCmd, shell=True, stdout=subprocess.PIPE)

	firstLine = True
	for line in process.stdout.readlines():
		if firstLine:
			firstLine = False
			continue

		fields = string.split(line)
		fields = filter(lambda x: len(x) > 0, fields)

		if len(fields) < 9:
			continue

		if fields[8] == program:
			assert pid == -1
			pid = int(fields[1])

	process.wait()

	if process.returncode != 0:
		print("adb shell ps returned %s" % str(process.returncode))
		pid = -1

	return pid

def debug(
	adbCmd,
	deqpCmdLine,
	targetGDBPort,
	hostGDBPort,
	jdbPort,
	jdbCmd,
	gdbCmd,
	buildDir,
	deviceLibs,
	breakpoints
	):

	programPid			= -1
	gdbServerProcess	= None
	gdbProcess			= None
	jdbProcess			= None
	curDir				= os.getcwd()
	debugDir			= os.path.join(common.ANDROID_DIR, "debug")

	if os.path.exists(debugDir):
		shutil.rmtree(debugDir)

	os.makedirs(debugDir)
	os.chdir(debugDir)

	try:
		# Start execution
		print("Starting intent...")
		common.execute("%s shell am start -W -D -n com.drawelements.deqp/android.app.NativeActivity -e cmdLine \"unused %s\"" % (adbCmd, deqpCmdLine.replace("\"", "\\\"")))
		print("Intent started")

		# Kill existing gdbservers
		print("Check and kill existing gdbserver")
		gdbPid = getADBProgramPID("lib/gdbserver")
		if gdbPid != -1:
			print("Found gdbserver with PID %i" % gdbPid)
			common.execute("%s shell run-as com.drawelements.deqp kill -9 %i" % (adbCmd, gdbPid))
			print("Killed gdbserver")
		else:
			print("Couldn't find existing gdbserver")

		programPid = getADBProgramPID("com.drawelements.deqp:testercore")

		print("Find process PID")
		if programPid == -1:
			common.die("Couldn't get PID of testercore")
		print("Process running with PID %i" % programPid)

		# Start gdbserver
		print("Start gdbserver for PID %i redirect stdout to gdbserver-stdout.txt" % programPid)
		gdbServerProcess = subprocess.Popen("%s shell run-as com.drawelements.deqp lib/gdbserver localhost:%i --attach %i" % (adbCmd, targetGDBPort, programPid), shell=True, stdin=subprocess.PIPE, stdout=open("gdbserver-stdout.txt", "wb"), stderr=open("gdbserver-stderr.txt", "wb"))
		print("gdbserver started")

		time.sleep(1)

		gdbServerProcess.poll()

		if gdbServerProcess.returncode != None:
			common.die("gdbserver returned unexpectly with return code %i see gdbserver-stdout.txt for more info" % gdbServerProcess.returncode)

		# Setup port forwarding
		print("Forwarding local port to gdbserver port")
		common.execute("%s forward tcp:%i tcp:%i" % (adbCmd, hostGDBPort, targetGDBPort))

		# Pull some data files for debugger
		print("Pull /system/bin/app_process from device")
		common.execute("%s pull /system/bin/app_process" % adbCmd)

		print("Pull /system/bin/linker from device")
		common.execute("%s pull /system/bin/linker" % adbCmd)

		for lib in deviceLibs:
			print("Pull library %s from device" % lib)
			common.execute("%s pull %s" % (adbCmd, lib))

		print("Copy %s from build dir" % common.NATIVE_LIB_NAME)
		shutil.copyfile(os.path.join(buildDir, common.NATIVE_LIB_NAME), common.NATIVE_LIB_NAME)

		# Forward local port for jdb
		print("Forward local port to jdb port")
		common.execute("%s forward tcp:%i jdwp:%i" % (adbCmd, jdbPort, programPid))

		# Connect JDB
		print("Start jdb process redirectd stdout to jdb-stdout.txt")
		jdbProcess = subprocess.Popen("%s -connect com.sun.jdi.SocketAttach:hostname=localhost,port=%i -sourcepath ../package" % (jdbCmd, jdbPort), shell=True, stdin=subprocess.PIPE, stdout=open("jdb-stdout.txt", "wb"), stderr=open("jdb-stderr.txt", "wb"))
		print("Started jdb process")

		# Write gdb.setup
		print("Write gdb.setup")
		gdbSetup = open("gdb.setup", "wb")
		gdbSetup.write("file app_process\n")
		gdbSetup.write("set solib-search-path .\n")
		gdbSetup.write("target remote :%i\n" % hostGDBPort)
		gdbSetup.write("set breakpoint pending on\n")

		for breakpoint in breakpoints:
			print("Set breakpoint at %s" % breakpoint)
			gdbSetup.write("break %s\n" % breakpoint)

		gdbSetup.write("set breakpoint pending off\n")
		gdbSetup.close()

		print("Start gdb")
		gdbProcess = subprocess.Popen("%s -x gdb.setup" % common.shellquote(gdbCmd), shell=True)

		gdbProcess.wait()

		print("gdb returned with %i" % gdbProcess.returncode)
		gdbProcess=None

		print("Close jdb process with 'quit'")
		jdbProcess.stdin.write("quit\n")
		jdbProcess.wait()
		print("JDB returned %s" % str(jdbProcess.returncode))
		jdbProcess=None

		print("Kill gdbserver process")
		gdbServerProcess.kill()
		gdbServerProcess=None
		print("Killed gdbserver process")

		print("Kill program %i" % programPid)
		common.execute("%s shell run-as com.drawelements.deqp -9 %i" % (adbCmd, programPid))
		print("Killed program")

	finally:
		if jdbProcess and jdbProcess.returncode == None:
			print("Kill jdb")
			jdbProcess.kill()
		elif jdbProcess:
			print("JDB returned %i" % jdbProcess.returncode)

		if gdbProcess and gdbProcess.returncode == None:
			print("Kill gdb")
			gdbProcess.kill()
		elif gdbProcess:
			print("GDB returned %i" % gdbProcess.returncode)

		if gdbServerProcess and gdbServerProcess.returncode == None:
			print("Kill gdbserver")
			gdbServerProcess.kill()
		elif gdbServerProcess:
			print("GDB server returned %i" % gdbServerProcess.returncode)

		print("Kill program %i" % programPid)
		common.execute("%s shell run-as com.drawelements.deqp kill -9 %i" % (adbCmd, programPid))
		print("Killed program")

		os.chdir(curDir)

if __name__ == "__main__":
	parser = argparse.ArgumentParser()

	defaultDeviceLibs = {
		"nexus-4" : [
			"/system/lib/libgenlock.so",
			"/system/lib/libmemalloc.so",
			"/system/lib/libqdutils.so",
			"/system/lib/libsc-a3xx.so"
		]
	}

	defaultDevices = []

	for device in defaultDeviceLibs:
		defaultDevices += [device]

	parser.add_argument('--adb',				dest='adbCmd',			default=common.shellquote(common.ADB_BIN), help="Path to adb command. Use absolute paths.")
	parser.add_argument('--deqp-commandline',	dest='deqpCmdLine',		default="--deqp-log-filename=/sdcard/TestLog.qpa", help="Command line arguments passed to dEQP test binary.")

	if common.getPlatform() == "linux":
		parser.add_argument('--gdb',				dest='gdbCmd',			default=common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")), help="gdb command used by script. Use absolute paths")
	else:
		parser.add_argument('--gdb',				dest='gdbCmd',			default=common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb")), help="gdb command used by script. Use absolute paths")

	parser.add_argument('--target-gdb-port',	dest='targetGDBPort',	default=60001, type=int, help="Port used by gdbserver on target.")
	parser.add_argument('--host-gdb-port',		dest='hostGDBPort',		default=60002, type=int, help="Host port that is forwarded to device gdbserver port.")
	parser.add_argument('--jdb',				dest='jdbCmd',			default="jdb", help="Path to jdb command. Use absolute paths.")
	parser.add_argument('--jdb-port',			dest='jdbPort',			default=60003, type=int, help="Host port used to forward jdb commands to device.")
	parser.add_argument('--build-dir',			dest='buildDir',		default="../../../deqp-build-android-9-armeabi-v7a-debug", help="Path to dEQP native build directory.")
	parser.add_argument('--device-libs',		dest='deviceLibs',		default=[], nargs='+', help="List of libraries that should be pulled from device for debugging.")
	parser.add_argument('--breakpoints',		dest='breakpoints',		default=["tcu::App::App"], nargs='+', help="List of breakpoints that are set by gdb.")
	parser.add_argument('--device',				dest='device',			default=None, choices=defaultDevices, help="Pull default libraries for this device.")

	args = parser.parse_args()

	debug(adbCmd=os.path.normpath(args.adbCmd),
	      gdbCmd=os.path.normpath(args.gdbCmd),
	      targetGDBPort=args.targetGDBPort,
	      hostGDBPort=args.hostGDBPort,
          jdbCmd=os.path.normpath(args.jdbCmd),
	      jdbPort=args.jdbPort,
	      deqpCmdLine=args.deqpCmdLine,
	      buildDir=args.buildDir,
	      deviceLibs=["/system/lib/libc.so", "/system/lib/libdl.so"] + args.deviceLibs + (defaultDeviceLibs[args.device] if args.device else []),
	      breakpoints=args.breakpoints)
