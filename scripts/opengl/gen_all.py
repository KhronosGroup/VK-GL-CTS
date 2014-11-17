# -*- coding: utf-8 -*-

from src_util import getGLRegistry, getHybridInterface
from gen_call_log_wrapper import genCallLogWrapper
from gen_enums import genEnums
from gen_es31_wrapper import genES31WrapperFuncs
from gen_es_direct_init import genESDirectInit
from gen_es_static_library import genESStaticLibrary
from gen_ext_init import genExtInit
from gen_func_init import genFuncInit
from gen_func_ptrs import genFunctionPointers
from gen_null_render_context import genNullRenderContext
from gen_str_util import genStrUtil
from gen_wrapper import genWrapper
from gen_query_util import genQueryUtil

def genAll ():
	registry = getGLRegistry()
	iface = getHybridInterface()
	genCallLogWrapper(iface)
	genEnums(iface)
	genES31WrapperFuncs(registry)
	genESDirectInit(registry)
	genESStaticLibrary(registry)
	genExtInit(registry, iface)
	genFuncInit(registry)
	genFunctionPointers(iface)
	genNullRenderContext(iface)
	genStrUtil(iface)
	genWrapper(iface)
	genQueryUtil(iface)

if __name__ == "__main__":
	genAll()
