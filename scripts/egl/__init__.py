# -*- coding: utf-8 -*-

from common import getEGLRegistry, getInterface, getDefaultInterface, VERSION

import str_util
import call_log_wrapper
import proc_address_tests
import enums
import func_ptrs
import library

def gen ():
	registry	= getEGLRegistry()
	iface		= getDefaultInterface()
	noExtIface	= getInterface(registry, 'egl', VERSION)

	str_util.gen(iface)
	call_log_wrapper.gen(noExtIface)
	proc_address_tests.gen()
	enums.gen(iface)
	func_ptrs.gen(iface)
	library.gen(registry)
