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
import shutil
import subprocess
import signal
import argparse

import common

def getADBProgramPID (adbCmd, program, serial):
	pid		= -1

	process = subprocess.Popen([adbCmd]
								+ (["-s", serial] if serial != None else [])
								+ ["shell", "ps"], stdout=subprocess.PIPE)

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

def debug (
	adbCmd,
	deqpCmdLine,
	targetGDBPort,
	hostGDBPort,
	jdbPort,
	jdbCmd,
	gdbCmd,
	buildDir,
	deviceLibs,
	breakpoints,
	serial,
	deviceGdbCmd,
	appProcessName,
	linkerName
	):

	programPid			= -1
	gdbServerProcess	= None
	gdbProcess			= None
	jdbProcess			= None
	curDir				= os.getcwd()
	debugDir			= os.path.join(common.ANDROID_DIR, "debug")
	serialArg			= "-s " + serial if serial != None else ""

	if os.path.exists(debugDir):
		shutil.rmtree(debugDir)

	os.makedirs(debugDir)
	os.chdir(debugDir)
	try:
		# Start execution
		print("Starting intent...")
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["shell", "am", "start", "-W", "-D", "-n", "com.drawelements.deqp/android.app.NativeActivity", "-e", "cmdLine", "\"\"unused " + deqpCmdLine  + "\"\""])
		print("Intent started")

		# Kill existing gdbservers
		print("Check and kill existing gdbserver")
		gdbPid = getADBProgramPID(adbCmd, "gdbserver", serial)
		if gdbPid != -1:
			print("Found gdbserver with PID %i" % gdbPid)
			common.execArgs([adbCmd]
							+ (["-s", serial] if serial != None else [])
							+ ["shell", "run-as", "com.drawelements.deqp", "kill", "-9", str(gdbPid)])
			print("Killed gdbserver")
		else:
			print("Couldn't find existing gdbserver")

		programPid = getADBProgramPID(adbCmd, "com.drawelements.deqp:testercore", serial)

		print("Find process PID")
		if programPid == -1:
			common.die("Couldn't get PID of testercore")
		print("Process running with PID %i" % programPid)

		# Start gdbserver
		print("Start gdbserver for PID %i redirect stdout to gdbserver-stdout.txt" % programPid)
		gdbServerProcess = subprocess.Popen([adbCmd]
											+ (["-s", serial] if serial != None else [])
											+ ["shell", "run-as", "com.drawelements.deqp", deviceGdbCmd, "localhost:" + str(targetGDBPort), "--attach", str(programPid)], stdin=subprocess.PIPE, stdout=open("gdbserver-stdout.txt", "wb"), stderr=open("gdbserver-stderr.txt", "wb"))
		print("gdbserver started")

		time.sleep(1)

		gdbServerProcess.poll()

		if gdbServerProcess.returncode != None:
			common.die("gdbserver returned unexpectly with return code %i see gdbserver-stdout.txt for more info" % gdbServerProcess.returncode)

		# Setup port forwarding
		print("Forwarding local port to gdbserver port")
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["forward", "tcp:" + str(hostGDBPort), "tcp:" + str(targetGDBPort)])

		# Pull some data files for debugger
		print("Pull /system/bin/%s from device" % appProcessName)
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["pull", "/system/bin/" + str(appProcessName)])

		print("Pull /system/bin/%s from device" % linkerName)
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["pull", "/system/bin/" + str(linkerName)])

		for lib in deviceLibs:
			print("Pull library %s from device" % lib)
			try:
				common.execArgs([adbCmd]
								+ (["-s", serial] if serial != None else [])
								+ ["pull", lib])
			except Exception as e:
				print("Failed to pull library '%s'. Error: %s" % (lib, str(e)))

		print("Copy %s from build dir" % common.NATIVE_LIB_NAME)
		shutil.copyfile(os.path.join(buildDir, common.NATIVE_LIB_NAME), common.NATIVE_LIB_NAME)

		# Forward local port for jdb
		print("Forward local port to jdb port")
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["forward", "tcp:" + str(jdbPort), "jdwp:" + str(programPid)])

		# Connect JDB
		print("Start jdb process redirectd stdout to jdb-stdout.txt")
		jdbProcess = subprocess.Popen([jdbCmd, "-connect", "com.sun.jdi.SocketAttach:hostname=localhost,port=" + str(jdbPort), "-sourcepath", "../package"], stdin=subprocess.PIPE, stdout=open("jdb-stdout.txt", "wb"), stderr=open("jdb-stderr.txt", "wb"))
		print("Started jdb process")

		# Write gdb.setup
		print("Write gdb.setup")
		gdbSetup = open("gdb.setup", "wb")
		gdbSetup.write("file %s\n" % appProcessName)
		gdbSetup.write("set solib-search-path .\n")
		gdbSetup.write("target remote :%i\n" % hostGDBPort)
		gdbSetup.write("set breakpoint pending on\n")

		for breakpoint in breakpoints:
			print("Set breakpoint at %s" % breakpoint)
			gdbSetup.write("break %s\n" % breakpoint)

		gdbSetup.write("set breakpoint pending off\n")
		gdbSetup.close()

		print("Start gdb")
		gdbProcess = subprocess.Popen(common.shellquote(gdbCmd) + " -x gdb.setup", shell=True)

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
		common.execArgs([adbCmd]
						+ (["-s", serial] if serial != None else [])
						+ ["shell", "run-as", "com.drawelements.deqp", "kill", "-9", str(programPid)])
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

		if programPid != -1:
			print("Kill program %i" % programPid)
			common.execArgs([adbCmd]
							+ (["-s", serial] if serial != None else [])
							+ ["shell", "run-as", "com.drawelements.deqp", "kill", "-9", str(programPid)])
			print("Killed program")

		os.chdir(curDir)

class Device:
	def __init__ (self, libraries=[], nativeBuildDir=None, hostGdbBins=None, deviceGdbCmd=None, appProcessName=None, linkerName=None):
		self.libraries = libraries
		self.nativeBuildDir = nativeBuildDir
		self.hostGdbBins = hostGdbBins
		self.deviceGdbCmd = deviceGdbCmd
		self.appProcessName = appProcessName
		self.linkerName = linkerName

	def getBuildDir (self):
		return self.nativeBuildDir

	def getGdbCommand (self, platform):
		return self.hostGdbBins[platform]

	def getDeviceGdbCommand (self):
		return self.deviceGdbCmd

	def getLibs (self):
		return self.libraries

	def getLinkerName (self):
		return self.linkerName

	def getAppProcessName (self):
		return self.appProcessName

if __name__ == "__main__":
	parser = argparse.ArgumentParser()

	devices = {
		"nexus-4" : Device(
			nativeBuildDir = "../native/debug-13-armeabi-v7a",
			deviceGdbCmd = "lib/gdbserver",
			hostGdbBins = {
				"linux" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")),
				"windows" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb"))
			},
			appProcessName = "app_process",
			linkerName = "linker",
			libraries = [
				"/system/lib/libgenlock.so",
				"/system/lib/libmemalloc.so",
				"/system/lib/libqdutils.so",
				"/system/lib/libsc-a3xx.so"
			]),
		"nexus-6" : Device(
			nativeBuildDir = "../native/debug-13-armeabi-v7a",
			deviceGdbCmd = "lib/gdbserver",
			hostGdbBins = {
				"linux" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")),
				"windows" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb"))
			},
			appProcessName = "app_process",
			linkerName = "linker",
			libraries = [
				"/system/lib/libutils.so",
				"/system/lib/libstdc++.so",
				"/system/lib/libm.so",
				"/system/lib/liblog.so",
				"/system/lib/libhardware.so",
				"/system/lib/libbinder.so",
				"/system/lib/libcutils.so",
				"/system/lib/libc++.so",
				"/system/lib/libLLVM.so",
				"/system/lib/libbcinfo.so",
				"/system/lib/libunwind.so",
				"/system/lib/libz.so",
				"/system/lib/libpng.so",
				"/system/lib/libcommon_time_client.so",
				"/system/lib/libstlport.so",
				"/system/lib/libui.so",
				"/system/lib/libsync.so",
				"/system/lib/libgui.so",
				"/system/lib/libft2.so",
				"/system/lib/libbcc.so",
				"/system/lib/libGLESv2.so",
				"/system/lib/libGLESv1_CM.so",
				"/system/lib/libEGL.so",
				"/system/lib/libunwind-ptrace.so",
				"/system/lib/libgccdemangle.so",
				"/system/lib/libcrypto.so",
				"/system/lib/libicuuc.so",
				"/system/lib/libicui18n.so",
				"/system/lib/libjpeg.so",
				"/system/lib/libexpat.so",
				"/system/lib/libpcre.so",
				"/system/lib/libharfbuzz_ng.so",
				"/system/lib/libstagefright_foundation.so",
				"/system/lib/libsonivox.so",
				"/system/lib/libnbaio.so",
				"/system/lib/libcamera_client.so",
				"/system/lib/libaudioutils.so",
				"/system/lib/libinput.so",
				"/system/lib/libhardware_legacy.so",
				"/system/lib/libcamera_metadata.so",
				"/system/lib/libgabi++.so",
				"/system/lib/libskia.so",
				"/system/lib/libRScpp.so",
				"/system/lib/libRS.so",
				"/system/lib/libwpa_client.so",
				"/system/lib/libnetutils.so",
				"/system/lib/libspeexresampler.so",
				"/system/lib/libGLES_trace.so",
				"/system/lib/libbacktrace.so",
				"/system/lib/libusbhost.so",
				"/system/lib/libssl.so",
				"/system/lib/libsqlite.so",
				"/system/lib/libsoundtrigger.so",
				"/system/lib/libselinux.so",
				"/system/lib/libprocessgroup.so",
				"/system/lib/libpdfium.so",
				"/system/lib/libnetd_client.so",
				"/system/lib/libnativehelper.so",
				"/system/lib/libnativebridge.so",
				"/system/lib/libminikin.so",
				"/system/lib/libmemtrack.so",
				"/system/lib/libmedia.so",
				"/system/lib/libinputflinger.so",
				"/system/lib/libimg_utils.so",
				"/system/lib/libhwui.so",
				"/system/lib/libandroidfw.so",
				"/system/lib/libETC1.so",
				"/system/lib/libandroid_runtime.so",
				"/system/lib/libsigchain.so",
				"/system/lib/libbacktrace_libc++.so",
				"/system/lib/libart.so",
				"/system/lib/libjavacore.so",
				"/system/lib/libvorbisidec.so",
				"/system/lib/libstagefright_yuv.so",
				"/system/lib/libstagefright_omx.so",
				"/system/lib/libstagefright_enc_common.so",
				"/system/lib/libstagefright_avc_common.so",
				"/system/lib/libpowermanager.so",
				"/system/lib/libopus.so",
				"/system/lib/libdrmframework.so",
				"/system/lib/libstagefright_amrnb_common.so",
				"/system/lib/libstagefright.so",
				"/system/lib/libmtp.so",
				"/system/lib/libjhead.so",
				"/system/lib/libexif.so",
				"/system/lib/libmedia_jni.so",
				"/system/lib/libsoundpool.so",
				"/system/lib/libaudioeffect_jni.so",
				"/system/lib/librs_jni.so",
				"/system/lib/libjavacrypto.so",
				"/system/lib/libqservice.so",
				"/system/lib/libqdutils.so",
				"/system/lib/libqdMetaData.so",
				"/system/lib/libmemalloc.so",
				"/system/lib/libandroid.so",
				"/system/lib/libcompiler_rt.so",
				"/system/lib/libjnigraphics.so",
				"/system/lib/libwebviewchromium_loader.so",

				"/system/lib/hw/gralloc.msm8084.so",
				"/system/lib/hw/memtrack.msm8084.so",

				"/vendor/lib/libgsl.so",
				"/vendor/lib/libadreno_utils.so",
				"/vendor/lib/egl/libEGL_adreno.so",
				"/vendor/lib/egl/libGLESv1_CM_adreno.so",
				"/vendor/lib/egl/libGLESv2_adreno.so",
				"/vendor/lib/egl/eglSubDriverAndroid.so",
				"/vendor/lib/libllvm-glnext.so",
			]),
		"nexus-7" : Device(
			nativeBuildDir = "../native/debug-13-armeabi-v7a",
			deviceGdbCmd = "lib/gdbserver",
			hostGdbBins = {
				"linux" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")),
				"windows" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb"))
			},
			appProcessName = "app_process",
			linkerName = "linker",
			libraries = [
				"/system/lib/libm.so",
				"/system/lib/libqdutils.so",
				"/system/lib/libmemalloc.so",
				"/system/lib/hw/gralloc.msm8960.so",
				"/system/lib/libstdc++.so",
				"/system/lib/liblog.so.",
				"/system/lib/libstdc++.so",
				"/system/lib/liblog.so",
				"/system/lib/libsigchain.so",
				"/system/lib/libcutils.so",
				"/system/lib/libstlport.so",
				"/system/lib/libgccdemangle.so",
				"/system/lib/libunwind.so",
				"/system/lib/libunwind-ptrace.so",
				"/system/lib/libbacktrace.so",
				"/system/lib/libutils.so",
				"/system/lib/libGLES_trace.so",
				"/system/lib/libEGL.so",
				"/system/lib/libETC1.so",
				"/system/lib/libGLESv1_CM.so",
				"/system/lib/libGLESv2.so",
				"/system/lib/libbinder.so",
				"/system/lib/libz.so",
				"/system/lib/libandroidfw.so",
				"/system/lib/libspeexresampler.so",
				"/system/lib/libaudioutils.so",
				"/system/lib/libcamera_metadata.so",
				"/system/lib/libsync.so",
				"/system/lib/libhardware.so",
				"/system/lib/libui.so",
				"/vendor/lib/egl/eglsubAndroid.so",
				"/vendor/lib/libsc-a3xx.so",
				"/system/lib/libgui.so",
				"/system/lib/libcamera_client.so",
				"/system/lib/libcrypto.so",
				"/system/lib/libexpat.so",
				"/system/lib/libnetutils.so",
				"/system/lib/libwpa_client.so",
				"/system/lib/libhardware_legacy.so",
				"/system/lib/libgabi++.so",
				"/system/lib/libicuuc.so",
				"/system/lib/libicui18n.so",
				"/system/lib/libharfbuzz_ng.so",
				"/system/lib/libc++.so",
				"/system/lib/libLLVM.so",
				"/system/lib/libbcinfo.so",
				"/system/lib/libbcc.so",
				"/system/lib/libpng.so",
				"/system/lib/libft2.so",
				"/system/lib/libRS.so",
				"/system/lib/libRScpp.so",
				"/system/lib/libjpeg.so",
				"/system/lib/libskia.so",
				"/system/lib/libhwui.so",
				"/system/lib/libimg_utils.so",
				"/system/lib/libinput.so",
				"/system/lib/libinputflinger.so",
				"/system/lib/libcommon_time_client.so",
				"/system/lib/libnbaio.so",
				"/system/lib/libsonivox.so",
				"/system/lib/libstagefright_foundation.so",
				"/system/lib/libmedia.so",
				"/system/lib/libmemtrack.so",
				"/system/lib/libminikin.so",
				"/system/lib/libnativebridge.so",
				"/system/lib/libnativehelper.so",
				"/system/lib/libnetd_client.so",
				"/system/lib/libpdfium.so",
				"/system/lib/libprocessgroup.so",
				"/system/lib/libselinux.so",
				"/system/lib/libsoundtrigger.so",
				"/system/lib/libsqlite.so",
				"/system/lib/libssl.so",
				"/system/lib/libusbhost.so",
				"/system/lib/libandroid_runtime.so",
				"/system/lib/libbacktrace_libc++.so",
				"/system/lib/libart.so",
				"/system/lib/libjavacore.so",
				"/system/lib/libexif.so",
				"/system/lib/libjhead.so",
				"/system/lib/libmtp.so",
				"/system/lib/libdrmframework.so",
				"/system/lib/libopus.so",
				"/system/lib/libpowermanager.so",
				"/system/lib/libstagefright_avc_common.so",
				"/system/lib/libstagefright_enc_common.so",
				"/system/lib/libstagefright_omx.so",
				"/system/lib/libstagefright_yuv.so",
				"/system/lib/libvorbisidec.so",
				"/system/lib/libstagefright.so",
				"/system/lib/libstagefright_amrnb_common.so",
				"/system/lib/libmedia_jni.so",
				"/system/lib/libsoundpool.so",
				"/system/lib/libaudioeffect_jni.so",
				"/system/lib/librs_jni.so",
				"/system/lib/libjavacrypto.so",
				"/system/lib/libandroid.so",
				"/system/lib/libcompiler_rt.so",
				"/system/lib/libjnigraphics.so",
				"/system/lib/libwebviewchromium_loader.so",

				"/system/lib/hw/memtrack.msm8960.so",

				"/vendor/lib/libgsl.so",
				"/vendor/lib/libadreno_utils.so",
				"/vendor/lib/egl/libEGL_adreno.so",
				"/vendor/lib/egl/libGLESv1_CM_adreno.so",
				"/vendor/lib/egl/libGLESv2_adreno.so",
			]),
		"nexus-10" : Device(
			nativeBuildDir = "../native/debug-13-armeabi-v7a",
			deviceGdbCmd = "lib/gdbserver",
			hostGdbBins = {
				"linux" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")),
				"windows" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb"))
			},
			appProcessName = "app_process",
			linkerName = "linker",
			libraries = [
				"/system/lib/libutils.so",
				"/system/lib/libstdc++.so",
				"/system/lib/libm.so",
				"/system/lib/liblog.so",
				"/system/lib/libhardware.so",
				"/system/lib/libbinder.so",
				"/system/lib/libcutils.so",
				"/system/lib/libc++.so",
				"/system/lib/libLLVM.so",
				"/system/lib/libbcinfo.so",
				"/system/lib/libunwind.so",
				"/system/lib/libz.so",
				"/system/lib/libpng.so",
				"/system/lib/libcommon_time_client.so",
				"/system/lib/libstlport.so",
				"/system/lib/libui.so",
				"/system/lib/libsync.so",
				"/system/lib/libgui.so",
				"/system/lib/libft2.so",
				"/system/lib/libbcc.so",
				"/system/lib/libGLESv2.so",
				"/system/lib/libGLESv1_CM.so",
				"/system/lib/libEGL.so",
				"/system/lib/libunwind-ptrace.so",
				"/system/lib/libgccdemangle.so",
				"/system/lib/libcrypto.so",
				"/system/lib/libicuuc.so",
				"/system/lib/libicui18n.so",
				"/system/lib/libjpeg.so",
				"/system/lib/libexpat.so",
				"/system/lib/libpcre.so",
				"/system/lib/libharfbuzz_ng.so",
				"/system/lib/libstagefright_foundation.so",
				"/system/lib/libsonivox.so",
				"/system/lib/libnbaio.so",
				"/system/lib/libcamera_client.so",
				"/system/lib/libaudioutils.so",
				"/system/lib/libinput.so",
				"/system/lib/libhardware_legacy.so",
				"/system/lib/libcamera_metadata.so",
				"/system/lib/libgabi++.so",
				"/system/lib/libskia.so",
				"/system/lib/libRScpp.so",
				"/system/lib/libRS.so",
				"/system/lib/libwpa_client.so",
				"/system/lib/libnetutils.so",
				"/system/lib/libspeexresampler.so",
				"/system/lib/libGLES_trace.so",
				"/system/lib/libbacktrace.so",
				"/system/lib/libusbhost.so",
				"/system/lib/libssl.so",
				"/system/lib/libsqlite.so",
				"/system/lib/libsoundtrigger.so",
				"/system/lib/libselinux.so",
				"/system/lib/libprocessgroup.so",
				"/system/lib/libpdfium.so",
				"/system/lib/libnetd_client.so",
				"/system/lib/libnativehelper.so",
				"/system/lib/libnativebridge.so",
				"/system/lib/libminikin.so",
				"/system/lib/libmemtrack.so",
				"/system/lib/libmedia.so",
				"/system/lib/libinputflinger.so",
				"/system/lib/libimg_utils.so",
				"/system/lib/libhwui.so",
				"/system/lib/libandroidfw.so",
				"/system/lib/libETC1.so",
				"/system/lib/libandroid_runtime.so",
				"/system/lib/libsigchain.so",
				"/system/lib/libbacktrace_libc++.so",
				"/system/lib/libart.so",
				"/system/lib/libjavacore.so",
				"/system/lib/libvorbisidec.so",
				"/system/lib/libstagefright_yuv.so",
				"/system/lib/libstagefright_omx.so",
				"/system/lib/libstagefright_enc_common.so",
				"/system/lib/libstagefright_avc_common.so",
				"/system/lib/libpowermanager.so",
				"/system/lib/libopus.so",
				"/system/lib/libdrmframework.so",
				"/system/lib/libstagefright_amrnb_common.so",
				"/system/lib/libstagefright.so",
				"/system/lib/libmtp.so",
				"/system/lib/libjhead.so",
				"/system/lib/libexif.so",
				"/system/lib/libmedia_jni.so",
				"/system/lib/libsoundpool.so",
				"/system/lib/libaudioeffect_jni.so",
				"/system/lib/librs_jni.so",
				"/system/lib/libjavacrypto.so",
				"/system/lib/libandroid.so",
				"/system/lib/libcompiler_rt.so",
				"/system/lib/libjnigraphics.so",
				"/system/lib/libwebviewchromium_loader.so",
				"/system/lib/libion.so",
				"/vendor/lib/hw/gralloc.exynos5.so",
				"/vendor/lib/egl/libGLES_mali.so",
			]),
		"default" : Device(
			nativeBuildDir = "../native/debug-13-armeabi-v7a",
			deviceGdbCmd = "lib/gdbserver",
			hostGdbBins = {
				"linux" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86/bin/arm-linux-androideabi-gdb")),
				"windows" : common.shellquote(os.path.join(common.ANDROID_NDK_PATH, "toolchains/arm-linux-androideabi-4.8/prebuilt/windows/bin/arm-linux-androideabi-gdb"))
			},
			appProcessName = "app_process",
			linkerName = "linker",
			libraries = [
			]),
	}

	parser.add_argument('--adb',				dest='adbCmd',			default=common.ADB_BIN, help="Path to adb command. Use absolute paths.")
	parser.add_argument('--deqp-commandline',	dest='deqpCmdLine',		default="--deqp-log-filename=/sdcard/TestLog.qpa", help="Command line arguments passed to dEQP test binary.")
	parser.add_argument('--gdb',				dest='gdbCmd',			default=None, help="gdb command used by script. Use absolute paths")
	parser.add_argument('--device-gdb',			dest='deviceGdbCmd',	default=None, help="gdb command used by script on device.")
	parser.add_argument('--target-gdb-port',	dest='targetGDBPort',	default=60001, type=int, help="Port used by gdbserver on target.")
	parser.add_argument('--host-gdb-port',		dest='hostGDBPort',		default=60002, type=int, help="Host port that is forwarded to device gdbserver port.")
	parser.add_argument('--jdb',				dest='jdbCmd',			default="jdb", help="Path to jdb command. Use absolute paths.")
	parser.add_argument('--jdb-port',			dest='jdbPort',			default=60003, type=int, help="Host port used to forward jdb commands to device.")
	parser.add_argument('--build-dir',			dest='buildDir',		default=None, help="Path to dEQP native build directory.")
	parser.add_argument('--device-libs',		dest='deviceLibs',		default=[], nargs='+', help="List of libraries that should be pulled from device for debugging.")
	parser.add_argument('--breakpoints',		dest='breakpoints',		default=["tcu::App::App"], nargs='+', help="List of breakpoints that are set by gdb.")
	parser.add_argument('--app-process-name',	dest='appProcessName',	default=None, help="Name of the app_process binary.")
	parser.add_argument('--linker-name',		dest='linkerName',		default=None, help="Name of the linker binary.")
	parser.add_argument('--device',				dest='device',			default="default", choices=devices, help="Pull default libraries for this device.")
	parser.add_argument('--serial','-s',		dest='serial',			default=None, help="-s Argument for adb.")

	args = parser.parse_args()
	device = devices[args.device]

	if args.deviceGdbCmd == None:
		args.deviceGdbCmd = device.getDeviceGdbCommand()

	if args.buildDir == None:
		args.buildDir = device.getBuildDir()

	if args.gdbCmd == None:
		args.gdbCmd = device.getGdbCommand(common.getPlatform())

	if args.linkerName == None:
		args.linkerName = device.getLinkerName()

	if args.appProcessName == None:
		args.appProcessName = device.getAppProcessName()

	debug(adbCmd=os.path.normpath(args.adbCmd),
		  gdbCmd=os.path.normpath(args.gdbCmd),
		  targetGDBPort=args.targetGDBPort,
		  hostGDBPort=args.hostGDBPort,
		  jdbCmd=os.path.normpath(args.jdbCmd),
		  jdbPort=args.jdbPort,
		  deqpCmdLine=args.deqpCmdLine,
		  buildDir=args.buildDir,
		  deviceLibs=["/system/lib/libc.so", "/system/lib/libdl.so"] + args.deviceLibs + device.getLibs(),
		  breakpoints=args.breakpoints,
		  serial=args.serial,
		  deviceGdbCmd=args.deviceGdbCmd,
		  appProcessName=args.appProcessName,
		  linkerName=args.linkerName)
