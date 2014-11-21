# -*- coding: utf-8 -*-

from common import getEGLRegistry, getInterface, getDefaultInterface

import str_util
import call_log_wrapper
import proc_address_tests

def gen ():
	registry	= getEGLRegistry()
	iface		= getDefaultInterface()
	noExtIface	= getInterface(registry, 'egl', '1.4')

	str_util.gen(iface)
	call_log_wrapper.gen(noExtIface)
	proc_address_tests.gen()
