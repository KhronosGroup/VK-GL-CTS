# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2016 Google Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and/or associated documentation files (the
# "Materials"), to deal in the Materials without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Materials, and to
# permit persons to whom the Materials are furnished to do so, subject to
# the following conditions:
#
# The above copyright notice(s) and this permission notice shall be
# included in all copies or substantial portions of the Materials.
#
# THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
#
#-------------------------------------------------------------------------

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from build.common import DEQP_DIR
from build.config import ANY_GENERATOR
from build_caselists import Module, getModuleByName, getBuildConfig, DEFAULT_BUILD_DIR, DEFAULT_TARGET
from mustpass import Project, Package, Mustpass, Configuration, include, exclude, genMustpassLists

COPYRIGHT_DECLARATION = """
     Permission is hereby granted, free of charge, to any person obtaining a
     copy of this software and/or associated documentation files (the
     "Materials"), to deal in the Materials without restriction, including
     without limitation the rights to use, copy, modify, merge, publish,
     distribute, sublicense, and/or sell copies of the Materials, and to
     permit persons to whom the Materials are furnished to do so, subject to
     the following conditions:

     The above copyright notice(s) and this permission notice shall be
     included in all copies or substantial portions of the Materials.

     THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
     EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
     MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
     TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
     MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
     """

MUSTPASS_PATH		= os.path.join(DEQP_DIR, "external", "vulkancts", "mustpass")
PROJECT				= Project(path = MUSTPASS_PATH, copyright = COPYRIGHT_DECLARATION)
VULKAN_MODULE		= getModuleByName("dEQP-VK")
BUILD_CONFIG		= getBuildConfig(DEFAULT_BUILD_DIR, DEFAULT_TARGET, "Debug")

# 1.0.0

VULKAN_1_0_0_PKG	= Package(module = VULKAN_MODULE, configurations = [
		# Master
		Configuration(name		= "default",
					  filters	= [include("master.txt"),
					  			   exclude("test-issues.txt"),
					  			   exclude("excluded-tests.txt")]),
	])

MUSTPASS_LISTS		= [
		Mustpass(project = PROJECT, version = "1.0.0",		packages = [VULKAN_1_0_0_PKG])
	]

if __name__ == "__main__":
	genMustpassLists(MUSTPASS_LISTS, ANY_GENERATOR, BUILD_CONFIG)
