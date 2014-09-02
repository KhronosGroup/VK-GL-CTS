# -*- coding: utf-8 -*-

import os
import platform

try:
	import _winreg
except:
	_winreg = None

class BuildConfig:
	def __init__ (self, buildDir, buildType, args):
		self.buildDir		= buildDir
		self.buildType		= buildType
		self.args			= args

	def getBuildDir (self):
		return self.buildDir

	def getBuildType (self):
		return self.buildType

	def getArgs (self):
		return self.args

class CMakeGenerator:
	def __init__ (self, name, isMultiConfig = False):
		self.name			= name
		self.isMultiConfig	= isMultiConfig

	def getName (self):
		return self.name

	def getGenerateArgs (self, buildType):
		args = ['-G', self.name]
		if not self.isMultiConfig:
			args.append('-DCMAKE_BUILD_TYPE=%s' % buildType)
		return args

	def getBuildArgs (self, buildType):
		args = []
		if self.isMultiConfig:
			args += ['--config', buildType]
		return args

	def getBinaryPath (self, buildType, basePath):
		return basePath

class DefaultGenerator(CMakeGenerator):
	def __init__(self):
		CMakeGenerator.__init__("default")

	def getGenerateArgs (self, buildType):
		args = []
		if not self.isMultiConfig:
			args.append('-DCMAKE_BUILD_TYPE=%s' % buildType)
		return args

class VSProjectGenerator(CMakeGenerator):
	ARCH_32BIT	= 0
	ARCH_64BIT	= 1

	def __init__(self, version, arch):
		name = "Visual Studio %d" % version
		if arch == self.ARCH_64BIT:
			name += " Win64"

		CMakeGenerator.__init__(self, name, isMultiConfig = True)
		self.version		= version
		self.arch			= arch

	def getBuildArgs (self, buildType):
		return CMakeGenerator.getBuildArgs(self, buildType) + ['--', '/m']

	def getBinaryPath (self, buildType, basePath):
		return os.path.join(os.path.dirname(basePath), buildType, os.path.basename(basePath) + ".exe")

	@staticmethod
	def getNativeArch ():
		arch = platform.machine().lower()

		if arch == 'x86':
			return VSProjectGenerator.ARCH_32BIT
		elif arch == 'amd64':
			return VSProjectGenerator.ARCH_64BIT
		else:
			raise Exception("Unhandled arch '%s'" % arch)

	@staticmethod
	def registryKeyAvailable (root, arch, name):
		try:
			key = _winreg.OpenKey(root, name, 0, _winreg.KEY_READ | arch)
			_winreg.CloseKey(key)
			return True
		except:
			return False

	def isAvailable (self):
		if _winreg != None:
			nativeArch = VSProjectGenerator.getNativeArch()
			if nativeArch == self.ARCH_32BIT and self.arch == self.ARCH_64BIT:
				return False

			arch = _winreg.KEY_WOW64_32KEY if nativeArch == self.ARCH_64BIT else 0
			keyMap = {
				10:		[(_winreg.HKEY_CLASSES_ROOT, "VisualStudio.DTE.10.0"), (_winreg.HKEY_LOCAL_MACHINE, "Software\\Microsoft\\VCExpress\\10.0")],
				11:		[(_winreg.HKEY_CLASSES_ROOT, "VisualStudio.DTE.11.0"), (_winreg.HKEY_LOCAL_MACHINE, "Software\\Microsoft\\VCExpress\\11.0")],
				12:		[(_winreg.HKEY_CLASSES_ROOT, "VisualStudio.DTE.12.0"), (_winreg.HKEY_LOCAL_MACHINE, "Software\\Microsoft\\VCExpress\\12.0")],
			}

			if not self.version in keyMap:
				raise Exception("Unsupported VS version %d" % self.version)

			keys = keyMap[self.version]
			for root, name in keys:
				if VSProjectGenerator.registryKeyAvailable(root, arch, name):
					return True
			return False
		else:
			return False

	@staticmethod
	def getDefault (arch):
		for version in reversed(range(10, 13)):
			gen = VSProjectGenerator(version, arch)
			if gen.isAvailable():
				return gen
		return None

# Pre-defined generators

MAKEFILE_GENERATOR		= CMakeGenerator("Unix Makefiles")
VS2010_X32_GENERATOR	= VSProjectGenerator(10, VSProjectGenerator.ARCH_32BIT)
VS2010_X64_GENERATOR	= VSProjectGenerator(10, VSProjectGenerator.ARCH_64BIT)
VS2013_X64_GENERATOR	= VSProjectGenerator(12, VSProjectGenerator.ARCH_32BIT)
VS2013_X64_GENERATOR	= VSProjectGenerator(12, VSProjectGenerator.ARCH_64BIT)

ANY_VS_X32_GENERATOR	= VSProjectGenerator.getDefault(VSProjectGenerator.ARCH_32BIT)
ANY_VS_X64_GENERATOR	= VSProjectGenerator.getDefault(VSProjectGenerator.ARCH_64BIT)
