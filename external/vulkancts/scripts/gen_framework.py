# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# Vulkan CTS
# ----------
#
# Copyright (c) 2015 Google Inc.
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

import os
import re
import sys
import glob
import json
import argparse
import datetime
import collections
import ast
import logging
from lxml import etree

scriptPath = os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *
from khr_util.format import indentLines, writeInlFile

VULKAN_XML_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "vulkan-docs", "src", "xml")
SCRIPTS_SRC_DIR = os.path.join(os.path.dirname(__file__), "src")
DEFAULT_OUTPUT_DIR = { "" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkan"),
                       "SC" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkansc") }

EXTENSIONS_TO_READ_FROM_XML_NOT_JSON = """
VK_EXT_conservative_rasterization
VK_EXT_custom_border_color
VK_EXT_extended_dynamic_state3
VK_EXT_mesh_shader
VK_EXT_shader_atomic_float
VK_EXT_shader_atomic_float2
VK_EXT_shader_image_atomic_int64
VK_KHR_8bit_storage
VK_KHR_16bit_storage
# VK_KHR_acceleration_structure
VK_KHR_android_surface
VK_KHR_bind_memory2
# VK_KHR_buffer_device_address
VK_KHR_calibrated_timestamps
VK_KHR_compute_shader_derivatives
VK_KHR_cooperative_matrix
VK_KHR_copy_commands2
VK_KHR_create_renderpass2
VK_KHR_dedicated_allocation
VK_KHR_deferred_host_operations
VK_KHR_depth_clamp_zero_one
VK_KHR_depth_stencil_resolve
VK_KHR_descriptor_update_template
VK_KHR_device_group
VK_KHR_device_group_creation
VK_KHR_display
VK_KHR_display_swapchain
VK_KHR_draw_indirect_count
VK_KHR_driver_properties
# VK_KHR_dynamic_rendering
# VK_KHR_dynamic_rendering_local_read
VK_KHR_external_fence
VK_KHR_external_fence_capabilities
VK_KHR_external_fence_fd
VK_KHR_external_fence_win32
VK_KHR_external_memory
VK_KHR_external_memory_capabilities
VK_KHR_external_memory_fd
VK_KHR_external_memory_win32
VK_KHR_external_semaphore
VK_KHR_external_semaphore_capabilities
VK_KHR_external_semaphore_fd
VK_KHR_external_semaphore_win32
VK_KHR_format_feature_flags2
# VK_KHR_fragment_shader_barycentric
# VK_KHR_fragment_shading_rate
VK_KHR_get_display_properties2
VK_KHR_get_memory_requirements2
VK_KHR_get_physical_device_properties2
VK_KHR_get_surface_capabilities2
# VK_KHR_global_priority
VK_KHR_image_format_list
VK_KHR_imageless_framebuffer
VK_KHR_incremental_present
VK_KHR_index_type_uint8
VK_KHR_line_rasterization
VK_KHR_load_store_op_none
VK_KHR_maintenance1
VK_KHR_maintenance2
VK_KHR_maintenance3
VK_KHR_maintenance4
VK_KHR_maintenance5
VK_KHR_maintenance6
VK_KHR_maintenance7
VK_KHR_maintenance8
VK_KHR_maintenance9
VK_KHR_map_memory2
VK_KHR_mir_surface
VK_KHR_multiview
VK_KHR_object_refresh
VK_KHR_performance_query
VK_KHR_pipeline_executable_properties
VK_KHR_pipeline_library
VK_KHR_portability_enumeration
VK_KHR_portability_subset
VK_KHR_present_id
VK_KHR_present_mode_fifo_latest_ready
VK_KHR_present_wait
VK_KHR_push_descriptor
VK_KHR_ray_query
VK_KHR_ray_tracing_maintenance1
VK_KHR_ray_tracing_pipeline
VK_KHR_ray_tracing_position_fetch
VK_KHR_relaxed_block_layout
VK_KHR_robustness2
VK_KHR_sampler_mirror_clamp_to_edge
VK_KHR_sampler_ycbcr_conversion
VK_KHR_separate_depth_stencil_layouts
VK_KHR_shader_atomic_int64
VK_KHR_shader_bfloat16
VK_EXT_shader_float8
VK_KHR_shader_clock
VK_KHR_shader_draw_parameters
VK_KHR_shader_expect_assume
VK_KHR_shader_float16_int8
VK_KHR_shader_float_controls
VK_KHR_shader_float_controls2
VK_KHR_shader_integer_dot_product
VK_KHR_shader_maximal_reconvergence
VK_KHR_shader_non_semantic_info
VK_KHR_shader_quad_control
VK_KHR_shader_relaxed_extended_instruction
VK_KHR_shader_subgroup_extended_types
VK_KHR_shader_subgroup_rotate
VK_KHR_shader_subgroup_uniform_control_flow
VK_KHR_shader_terminate_invocation
VK_KHR_shared_presentable_image
VK_KHR_spirv_1_4
VK_KHR_storage_buffer_storage_class
VK_KHR_surface
VK_KHR_surface_protected_capabilities
VK_KHR_surface_maintenance1
VK_KHR_swapchain
VK_KHR_swapchain_mutable_format
VK_KHR_synchronization2
VK_KHR_timeline_semaphore
VK_KHR_unified_image_layouts
VK_KHR_uniform_buffer_standard_layout
VK_KHR_variable_pointers
VK_KHR_vertex_attribute_divisor
VK_KHR_video_decode_av1
VK_KHR_video_decode_h264
VK_KHR_video_decode_h265
VK_KHR_video_decode_queue
VK_KHR_video_decode_vp9
VK_KHR_video_encode_av1
VK_KHR_video_encode_h264
VK_KHR_video_encode_h265
VK_KHR_video_encode_intra_refresh
VK_KHR_video_encode_quantization_map
VK_KHR_video_encode_queue
# VK_KHR_video_maintenance1
VK_KHR_video_maintenance2
VK_KHR_video_queue
VK_KHR_vulkan_memory_model
VK_KHR_wayland_surface
VK_KHR_win32_keyed_mutex
VK_KHR_win32_surface
# VK_KHR_workgroup_memory_explicit_layout
VK_KHR_xcb_surface
VK_KHR_xlib_surface
VK_KHR_zero_initialize_workgroup_memory
VK_NV_shader_atomic_float16_vector
""".splitlines()

EXTENSIONS_TO_READ_FROM_XML_NOT_JSON = [s for s in EXTENSIONS_TO_READ_FROM_XML_NOT_JSON if not s.startswith('#')]

INL_HEADER = """\
/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 * This file was generated by /scripts/gen_framework.py
 */\

"""

DEFINITIONS = {
    "VK_MAX_PHYSICAL_DEVICE_NAME_SIZE": "size_t",
    "VK_MAX_EXTENSION_NAME_SIZE": "size_t",
    "VK_MAX_DRIVER_NAME_SIZE": "size_t",
    "VK_MAX_DRIVER_INFO_SIZE": "size_t",
    "VK_UUID_SIZE": "size_t",
    "VK_LUID_SIZE": "size_t",
    "VK_MAX_MEMORY_TYPES": "size_t",
    "VK_MAX_MEMORY_HEAPS": "size_t",
    "VK_MAX_DESCRIPTION_SIZE": "size_t",
    "VK_MAX_DEVICE_GROUP_SIZE": "size_t",
    "VK_ATTACHMENT_UNUSED": "uint32_t",
    "VK_SUBPASS_EXTERNAL": "uint32_t",
    "VK_QUEUE_FAMILY_IGNORED": "uint32_t",
    "VK_QUEUE_FAMILY_EXTERNAL": "uint32_t",
    "VK_REMAINING_MIP_LEVELS": "uint32_t",
    "VK_REMAINING_ARRAY_LAYERS": "uint32_t",
    "VK_WHOLE_SIZE": "vk::VkDeviceSize",
    "VK_TRUE": "vk::VkBool32",
    "VK_FALSE": "vk::VkBool32",
}

PLATFORM_TYPES = [
    # VK_KHR_xlib_surface
    (["Display","*"], ["XlibDisplayPtr"], "void*"),
    (["Window"], ["XlibWindow"], "uintptr_t",),
    (["VisualID"], ["XlibVisualID"], "uint32_t"),

    # VK_KHR_xcb_surface
    (["xcb_connection_t", "*"], ["XcbConnectionPtr"], "void*"),
    (["xcb_window_t"], ["XcbWindow"], "uintptr_t"),
    (["xcb_visualid_t"], ["XcbVisualid"], "uint32_t"),

    # VK_KHR_wayland_surface
    (["struct", "wl_display","*"], ["WaylandDisplayPtr"], "void*"),
    (["struct", "wl_surface", "*"], ["WaylandSurfacePtr"], "void*"),

    # VK_KHR_mir_surface
    (["MirConnection", "*"], ["MirConnectionPtr"], "void*"),
    (["MirSurface", "*"], ["MirSurfacePtr"], "void*"),

    # VK_KHR_android_surface
    (["ANativeWindow", "*"], ["AndroidNativeWindowPtr"], "void*"),

    # VK_KHR_win32_surface
    (["HINSTANCE"], ["Win32InstanceHandle"], "void*"),
    (["HWND"], ["Win32WindowHandle"], "void*"),
    (["HANDLE"], ["Win32Handle"], "void*"),
    (["const", "SECURITY_ATTRIBUTES", "*"], ["Win32SecurityAttributesPtr"], "const void*"),
    (["AHardwareBuffer", "*"], ["AndroidHardwareBufferPtr"], "void*"),
    (["HMONITOR"], ["Win32MonitorHandle"], "void*"),
    (["LPCWSTR"], ["Win32LPCWSTR"], "const void*"),

    # VK_EXT_acquire_xlib_display
    (["RROutput"], ["RROutput"], "void*"),

    (["zx_handle_t"], ["zx_handle_t"], "uint32_t"),
    (["GgpFrameToken"], ["GgpFrameToken"], "int32_t"),
    (["GgpStreamDescriptor"], ["GgpStreamDescriptor"], "int32_t"),
    (["CAMetalLayer"], ["CAMetalLayer"], "void*"),
    (["struct", "_screen_context", "*"], ["QNXScreenContextPtr"], "void*"),
    (["struct", "_screen_window", "*"], ["QNXScreenWindowPtr"], "void*"),

    # VK_EXT_metal_objects
    (["MTLDevice_id"], ["MTLDevice_id"], "void*"),
    (["MTLCommandQueue_id"], ["MTLCommandQueue_id"], "void*"),
    (["MTLBuffer_id"], ["MTLBuffer_id"], "void*"),
    (["MTLTexture_id"], ["MTLTexture_id"], "void*"),
    (["IOSurfaceRef"], ["IOSurfaceRef"], "void*"),
    (["MTLSharedEvent_id"], ["MTLSharedEvent_id"], "void*"),

    # VK_NV_external_sci_sync
    (["NvSciBufObj"], ["NvSciBufObj"], "int"),
    (["NvSciSyncObj"], ["NvSciSyncObj"], "int"),
    (["NvSciSyncFence"], ["NvSciSyncFence"], "int"),
    (["NvSciBufAttrList"], ["NvSciBufAttrList"], "int"),
    (["NvSciSyncAttrList"], ["NvSciSyncAttrList"], "int"),

    # VK_OHOS_surface
    (["OHNativeWindow"], ["OHNativeWindow"], "void*"),
]

PLATFORM_TYPE_NAMESPACE = "pt"

TYPE_SUBSTITUTIONS = [
    # Platform-specific
    ("DWORD", "uint32_t"),
    ("HANDLE*", PLATFORM_TYPE_NAMESPACE + "::" + "Win32Handle*"),
]

EXTENSION_POSTFIXES_STANDARD = ["KHR", "EXT"]
EXTENSION_POSTFIXES_VENDOR = ["AMD", "ARM", "NV", 'INTEL', "NVX", "KHX", "NN", "MVK", "FUCHSIA", 'QCOM', "GGP", "QNX", "ANDROID", 'VALVE', 'HUAWEI']
EXTENSION_POSTFIXES = EXTENSION_POSTFIXES_STANDARD + EXTENSION_POSTFIXES_VENDOR

def printObjectAttributes(obj, indent=0):
    indent_str = '    ' * indent
    if isinstance(obj, dict):
        for key, value in obj.items():
            print(f"{indent_str}{key}:")
            printObjectAttributes(value, indent + 1)
    elif isinstance(obj, list):
        for i, item in enumerate(obj):
            print(f"{indent_str}[{i}]:")
            printObjectAttributes(item, indent + 1)
    elif hasattr(obj, '__dict__'):  # Check if the object has a __dict__ attribute
        for key, value in obj.__dict__.items():
            print(f"{indent_str}{key}:")
            printObjectAttributes(value, indent + 1)
    else:
        print(f"{indent_str}{repr(obj)}")

def printAttributesToFile(obj, file, indent=0):
    try:
        json_str = json.dumps(obj, indent=4)
        file.write(json_str)
    except TypeError:
        # If serialization fails, fall back to custom printing and write to the file
        indent_str = '    ' * indent
        file.write(f"{indent_str}Object could not be serialized to JSON\n")
        if isinstance(obj, dict):
            for key, value in obj.items():
                file.write(f"{indent_str}{key}:\n")
                printAttributesToFile(value, file, indent + 1)
        elif isinstance(obj, list):
            for i, item in enumerate(obj):
                file.write(f"{indent_str}[{i}]:\n")
                printAttributesToFile(item, file, indent + 1)
        elif hasattr(obj, '__dict__'):
            for key, value in obj.__dict__.items():
                file.write(f"{indent_str}{key}:\n")
                printAttributesToFile(value, file, indent + 1)
        else:
            file.write(f"{indent_str}{repr(obj)}\n")

def transformSingleDependsConditionToCpp(depPart, api, checkVersionString, checkExtensionString, extension, depends):
    ret = None
    if 'VK_VERSION' in depPart:
        # when dependency is vulkan version then replace it with proper condition
        ret = checkVersionString % (depPart[-3], depPart[-1])
    else:
        # when dependency is extension check if it was promoted
        for dExt in api.extensions:
            if depPart == dExt.name:
                depExtVector = 'vDEP' if dExt.type == 'device' else 'vIEP'
                isSupportedCheck = checkExtensionString % (depExtVector, depPart)
                ret = isSupportedCheck
                # This check is just heuristics. In theory we should check if the promotion is actually checked properly
                # in the dependency
                if dExt.promotedto is not None and dExt.promotedto not in depends:
                     p = dExt.promotedto
                     # check if dependency was promoted to vulkan version or other extension
                     if 'VK_VERSION' in p:
                         ret = f'({checkVersionString % (p[-3], p[-1])} || {isSupportedCheck})'
                     else:
                         ret = f'({checkExtensionString % (depExtVector, depPart)} || {isSupportedCheck})'
        # for SC when extension was not found try checking also not supported
        # extensions and see if this extension is part of core
        if ret is None and api.apiName == "vulkansc":
            for dExt in api.notSupportedExtensions:
                if depPart == dExt.name:
                    p = dExt.promotedto
                    if p is None:
                        break
                    if int(p[-1]) > 2:
                        break
                    ret = "true"
        if ret is None:
            ret = "false /* UNSUPPORTED CONDITION: " + depPart + "*/"
        if ret is None:
            assert False, f"{depPart} not found: {extension} : {depends}"
    return ret

def transformDependsToCondition(depends, api, checkVersionString, checkExtensionString, extension):
    tree = parseDependsEpression(depends)
    condition = generateCppDependencyAST(tree, api, checkVersionString, checkExtensionString, extension, depends)
    return condition

# Converts the dependencies expression into an Abstract Syntax Tree that uses boolean operators
def parseDependsEpression(string):
    try:
        # Parse the input string into an abstract syntax tree (AST)
        tree = ast.parse(string.replace('+', ' and ').replace(',', ' or ').replace('::', '__'), mode='eval')
        expression = tree.body
        return expression
    except SyntaxError as e:
        print(f"Syntax error in the input string: {e} \"" + string + "\"")
        sys.exit(-1)

def generateCppDependencyAST(node, api, checkVersionString, checkExtensionString, extension, depends):
    if isinstance(node, ast.BoolOp):
        parts = [
            generateCppDependencyAST(v, api, checkVersionString, checkExtensionString, extension, depends)
            for v in node.values
        ]
        op = "&&" if isinstance(node.op, ast.And) else "||"
        # Parenthesize each part, then join with the operator, and wrap the whole
        joined = f" {op} ".join(f"{p}" for p in parts)
        return f"({joined})"

    elif isinstance(node, ast.Name):
        return transformSingleDependsConditionToCpp(
            node.id, api, checkVersionString, checkExtensionString, extension, depends
        )

    elif isinstance(node, ast.Constant):
        return node.value

    else:
        raise NotImplementedError(f"Unsupported AST node: {node!r}")

# Checks the dependencies AST against the passed extensions
def checkDependencyAST(node, extensions):
    if isinstance(node, ast.BoolOp):
        assert(len(node.values) >= 2)
        value = checkDependencyAST(node.values.pop(), extensions)
        while node.values:
            nextValue = checkDependencyAST(node.values.pop(), extensions)
            if isinstance(node.op, ast.And):
                value = value and nextValue
            if isinstance(node.op, ast.Or):
                value = value or nextValue
        return value
    elif isinstance(node, ast.Name):
        if '_VERSION_' in node.id:
            return True
        for ext in extensions:
            if node.id == ext.name:
                return True
        return False
    elif isinstance(node, ast.Constant):
        return node.value

# helper function that check if dependency is in list of extension
def isDependencyMet(dependsExpression, extensionList):
    if dependsExpression is None:
        return True
    tree = parseDependsEpression(dependsExpression)
    # check if requirement dependencies are meet; if not then struct/function is not used
    ret = checkDependencyAST(tree, extensionList)
    return ret

def substituteType(object): # both CompositeMember and FunctionArgument can be passed to this function
    for src, dst in TYPE_SUBSTITUTIONS:
        object.type = object.type.replace(src, dst)
    for platformType, substitute, _ in PLATFORM_TYPES:
        platformTypeName = platformType[0]
        platformTypeName = platformType[-2] if "*" in platformType else platformType[0]
        if object.type == platformTypeName:
            object.type = PLATFORM_TYPE_NAMESPACE + '::' + substitute[0]
            object.qualifiers = None if 'struct' in platformType else object.qualifiers
            object.qualifiers = None if 'const' in platformType else object.qualifiers
            if "*" in platformType:
                object.pointer = "*" if object.pointer == "**" else None

class Define:
    def __init__ (self, name, aType, alias, value):
        self.name = name
        self.type = aType
        self.alias = alias
        self.value = value

class Handle:
    def __init__ (self, name, aType, alias, parent, objtypeenum):
        self.name = name
        self.type = aType
        self.alias = alias
        self.parent = parent
        self.objtypeenum = objtypeenum

class Bitmask:
    def __init__ (self, name, aType, requires, bitvalues):
        self.name = name
        self.type = aType
        self.alias = None           # initialy None but may be filled while parsing next tag
        self.requires = requires
        self.bitvalues = bitvalues

class Enumerator:
    def __init__ (self, name, value, bitpos):
        self.name = name
        self.aliasList = []         # list of strings
        self.value = value          # some enums specify value and some bitpos
        self.bitpos = bitpos
        self.extension = None       # name of extension that added this enumerator

class Enum:
    def __init__ (self, name):
        self.name = name
        self.alias = None           # name of enum alias or None
        self.type = None            # enum or bitmask
        self.bitwidth = "32"
        self.enumeratorList = []    # list of Enumerator objects

    def areValuesLinear (self):
        if self.type == 'bitmask':
            return False
        curIndex = 0
        for enumerator in self.enumeratorList:
            intValue = parseInt(enumerator.value)
            if intValue != curIndex:
                return False
            curIndex += 1
        return True

class CompositeMember:
    def __init__ (self, name, aType, pointer, qualifiers, arraySizeList, optional, limittype, values, fieldWidth):
        self.name = name
        self.type = aType                   # member type
        self.pointer = pointer              # None, '*' or '**'
        self.qualifiers = qualifiers        # 'const' or 'struct' or None
        self.arraySizeList = arraySizeList  # can contain digits or enums
        self.optional = optional
        self.limittype = limittype
        self.values = values                # allowed member values
        self.fieldWidth = fieldWidth        # ':' followed by number of bits

        # check if type should be swaped
        substituteType(self)

class Composite:
    def __init__ (self, name, category, allowduplicate, structextends, returnedonly, members):
        self.name = name
        self.category = category            # is it struct or union
        self.aliasList = []                 # most composite types have single alias but there are cases like VkPhysicalDeviceVariablePointersFeatures that have 3
        self.allowduplicate = allowduplicate
        self.structextends = structextends
        self.returnedonly = returnedonly
        self.members = members              # list of CompositeMember objects
        self.notSupportedAlias = None       # alias used in not supported api e.g. VkPhysicalDeviceLineRasterizationFeaturesKHR is alias available only in vulkan but not in SC

class FunctionArgument:
    def __init__ (self, name, qualifiers, aType, pointer = None, secondPointerIsConst = False, arraySize = None, len = None):
        self.name = name
        self.qualifiers = qualifiers
        self.type = aType
        self.pointer = pointer            # None, '*' or '**'
        self.secondPointerIsConst = secondPointerIsConst
        self.arraySize = arraySize
        self.len = len

        # check if type should be swaped
        substituteType(self)

class Function:
    TYPE_PLATFORM = 0 # Not bound to anything
    TYPE_INSTANCE = 1 # Bound to VkInstance
    TYPE_DEVICE = 2   # Bound to VkDevice

    def __init__ (self, name, returnType = None, arguments = None):
        self.name = name
        self.aliasList = []
        self.queuesList = []
        self.returnType = returnType
        self.arguments = arguments                # list of FunctionArgument objects
        self.functionType = Function.TYPE_PLATFORM

        # Determine function type based on first argument but use TYPE_PLATFORM for vkGetInstanceProcAddr
        if self.name == "vkGetInstanceProcAddr":
            return
        assert len(self.arguments) > 0
        firstArgType = self.arguments[0].type
        if firstArgType in ["VkInstance", "VkPhysicalDevice"]:
            self.functionType = Function.TYPE_INSTANCE
        elif firstArgType in ["VkDevice", "VkCommandBuffer", "VkQueue"]:
            self.functionType = Function.TYPE_DEVICE

    def getType (self):
        return self.functionType

class FeatureEnumerator:
    def __init__ (self, name, extends, offset, extnumber):
        self.name = name
        self.extends = extends
        self.offset = offset
        self.extnumber = extnumber

class FeatureRequirement:
    def __init__ (self, operation, comment, enumList, typeList, commandList, featureList):
        self.operation = operation      # "require" or "remove"; "deprecate" should be filtered out
        self.comment = comment
        self.enumList = enumList        # list of FeatureEnumerator objects
        self.typeList = typeList        # list of strings, each representing required structure name
        self.commandList = commandList  # list of strings, each representing required function name
        self.features = featureList        # list of ExtensionFeature objects

class Feature:
    def __init__ (self, api, name, number, requirementsList):
        self.api = api
        self.name = name
        self.number = number
        self.requirementsList = requirementsList  # list of FeatureRequirement objects

class ExtensionEnumerator:
    def __init__ (self, name, extends, alias, value, extnumber, offset, bitpos, vdir, comment):
        self.name = name
        self.extends = extends
        self.alias = alias
        self.value = value
        self.extnumber = extnumber
        self.offset = offset
        self.bitpos = bitpos
        self.dir = vdir
        self.comment = comment  # note: comment is used to mark not promoted features for partially promoted extensions

class ExtensionCommand:
    def __init__ (self, name, comment):
        self.name = name
        self.comment = comment

class ExtensionType:
    def __init__ (self, name, comment):
        self.name = name
        self.comment = comment

class ExtensionFeature:
    def __init__ (self, name, struct):
        self.name = name
        self.struct = struct

class ExtensionRequirements:
    def __init__ (self, depends, extendedEnums, newCommands, newTypes, featureList):
        self.depends = depends                      # None when requirement apply to all implementations of extension or string with dependencies
                                                    # string with extension name when requirements apply to implementations that also support given extension
        self.extendedEnums = extendedEnums          # list of ExtensionEnumerator objects
        self.newCommands = newCommands              # list of ExtensionCommand objects
        self.newTypes = newTypes                    # list of ExtensionType objects
        self.features = featureList                 # list of ExtensionFeature objects

class Extension:
    def __init__ (self, name, number, type, depends, platform, promotedto, partiallyPromoted, requirementsList, supportedList):
        self.name = name                            # extension name
        self.number = number                        # extension version
        self.type = type                            # extension type - "device" or "instance"
        self.depends = depends                      # string containig grammar for required core vulkan version and/or other extensions
        self.platform = platform                    # None, "win32", "ios", "android" etc.
        self.promotedto = promotedto                # vulkan version, other extension or None; eg. VK_KHR_global_priority was promoted to vk1.4
        self.promotedFrom = None                    # extension list from which this one was promoted; eg. VK_KHR_global_priority was promoted
                                                    #   from VK_EXT_global_priority and VK_EXT_global_priority_query
        self.partiallyPromoted = partiallyPromoted  # when True then some of requirements were not promoted
        self.requirementsList = requirementsList    # list of ExtensionRequirements objects
        self.supported = supportedList              # list of supported APIs

class API:
    def __init__ (self, apiName):
        self.apiName = apiName           # string "vulkan" or "vulkansc"
        self.basetypes = {}              # dictionary, e.g. one of keys is VkFlags and its value is uint32_t
        self.defines = []
        self.handles = []                # list of Handle objects
        self.bitmasks = []               # list of Bitmask objects
        self.enums = []                  # list of Enum objects - each contains individual enum definition (including extension enums)
        self.compositeTypes = []         # list of Composite objects - each contains individual structure/union definition (including extension structures)
        self.tempCompositeTypesAliases = []  # list of Composite Aliases to be added to the compositeTypes at the end of the parsing
        self.functions = []              # list of Function objects - each contains individual command definition (including extension functions)
        self.features = []               # list of Feature objects
        self.extensions = []             # list of Extension objects - each contains individual, supported extension definition
        self.notSupportedExtensions = [] # list of Extension objects - it contains NOT supported extensions; this is filled and needed only for SC
        self.basicCTypes = []            # list of basic C types e.g. 'void', 'int8_t'
        self.tempEnumAliasesList = []        # list of aliases for enums that could not be added because enum is defined later than its alias; this is needed for SC

        # read all files from extensions directory
        additionalExtensionData = {}
        for fileName in glob.glob(os.path.join(SCRIPTS_SRC_DIR, "extensions", "*.json")):
            if "schema.json" in fileName:
                continue
            extensionName = os.path.basename(fileName)[:-5]
            if extensionName in EXTENSIONS_TO_READ_FROM_XML_NOT_JSON:
                continue
            fileContent = readFile(fileName)
            try:
                additionalExtensionData[extensionName] = json.loads(fileContent)
                with open(fileName, 'w') as file:
                    file.write(json.dumps(additionalExtensionData[extensionName], indent=4))
            except ValueError as err:
                print("Error in %s: %s" % (os.path.basename(fileName), str(err)))
                sys.exit(-1)
        self.additionalExtensionData = sorted(additionalExtensionData.items(), key=lambda e: e[0])

    def addEnumerator(self, targetEnum, name, value, offset, extnumber, bitpos, dir = None):
        # calculate enumerator value if offset attribute is present
        if value is None and offset is not None:
            value = 1000000000 + (int(extnumber) - 1) * 1000 + int(offset)
            # check if value should be negative
            value = -value if dir == "-" else value
            # convert to string so that type matches the type in which values
            # are stored for enums that were read from enums xml section
            value = str(value)
        # add new enumerator
        targetEnum.enumeratorList.append(Enumerator(name, value, bitpos))

    def addAliasToEnumerator (self, targetEnum, name, alias):
        assert(alias is not None)
        for e in reversed(targetEnum.enumeratorList):
            if alias == e.name or alias in e.aliasList:
                # make sure same alias is not already on the list; this handles special case like
                # VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR alais which is defined in three places
                if name not in e.aliasList:
                    e.aliasList.append(name)
                return True
        return False

    def readEnum (self, enumsNode):
        enumName = enumsNode.get("name")
        # special case for vulkan hardcoded constants that are specified as enum in vk.xml
        if enumName == "API Constants":
            for enumItem in enumsNode:
                self.defines.append(Define(
                    enumItem.get("name"),
                    enumItem.get("type"),
                    enumItem.get("alias"),
                    enumItem.get("value")
                ))
            return
        # initial enum definition is read while processing types section;
        # we need to find this enum definition and add data to it
        enumDefinition = [enumDef for enumDef in self.enums if enumName == enumDef.name][0]
        # add type and bitwidth to enum definition
        enumDefinition.type = enumsNode.get("type")
        enumDefinition.bitwidth = enumsNode.get("bitwidth")
        if enumDefinition.bitwidth is None:
            enumDefinition.bitwidth = "32"
        # add components to enum definition
        for enumeratorItem in enumsNode:
            # skip comment tags
            if enumeratorItem.tag != "enum":
                continue
            # when api attribute is present it limits param to specific api
            supportedApi = enumeratorItem.get("api")
            if supportedApi != None and supportedApi != self.apiName:
                continue
            name = enumeratorItem.get("name")
            alias = enumeratorItem.get("alias")
            if alias is None:
                self.addEnumerator(
                    enumDefinition,
                    name,
                    enumeratorItem.get("value"),
                    enumeratorItem.get("offset"),
                    enumeratorItem.get("extnumber"),
                    enumeratorItem.get("bitpos"),
                    enumeratorItem.get("dir"))
            else:
                self.addAliasToEnumerator(enumDefinition, name, alias)

    def readCommand (self, commandNode):
        # when api attribute is present it limits command just to specified api
        dedicatedForApi = commandNode.get("api")
        if dedicatedForApi is not None and dedicatedForApi != self.apiName:
            # we dont need to parse this command
            return
        # check if this is alias
        alias = commandNode.get("alias")
        # if node is alias then use the fact that alias definition follows aliased structure
        if alias is not None:
            # aliased command has usually been added recently, so we iterate in reverse order
            found = False
            for f in reversed(self.functions):
                found = (f.name == alias)
                if found:
                    f.aliasList.append(commandNode.get("name"))
                    break
            assert found
            # go to next node
            return
        # memorize all parameters
        functionParams = []
        queuesList = []

        protoNode = None
        for paramNode in commandNode:
            # memorize prototype node
            if paramNode.tag == "proto":
                protoNode = paramNode
                continue
            # skip implicitexternsyncparams
            if paramNode.tag != "param":
                continue
            # when api attribute is present it limits param to specific api
            supportedApi = paramNode.get("api")
            if supportedApi != None and supportedApi != self.apiName:
                continue
            nameNode = paramNode.find("name")
            typeNode = paramNode.find("type")
            starCount = typeNode.tail.count('*')
            lenAttr = paramNode.get("len")
            functionParams.append(FunctionArgument(
                nameNode.text,
                paramNode.text,
                paramNode.find("type").text,
                '*' * starCount if starCount > 0 else None,
                'const' in typeNode.tail,
                nameNode.tail,
                lenAttr
            ))

        queuesAttr = commandNode.get("queues")
        if queuesAttr:
            queuesList = queuesAttr.split(",")

        # memorize whole function
        func = Function(
            protoNode.find("name").text,
            protoNode.find("type").text,
            functionParams,
        )

        func.queuesList = queuesList
        self.functions.append(func)

    def readExtension (self, extensionNode):
        # check to which list this extension should be added
        supportedList = extensionNode.get("supported")
        isExtensionSupported = self.apiName in supportedList.split(',')
        promotedto = extensionNode.get("promotedto")
        # read extension definition to proper list
        extensionName = extensionNode.get("name")
        extensionNumber = extensionNode.get("number")
        if promotedto is not None and 'VK_VERSION' in promotedto:
            p = promotedto
            major = int(p[-3])
            minor = int(p[-1])
            if self.apiName == "vulkansc" and "vulkan" in supportedList and major == 1 and minor <= 2 and extensionName in EXTENSIONS_TO_READ_FROM_XML_NOT_JSON:
                isExtensionSupported = True
        targetExtensionList = self.extensions if isExtensionSupported else self.notSupportedExtensions
        partiallyPromoted = False
        # before reading extension data first read extension
        # requirements by iterating over all require tags
        requirementsList = []
        for requireItem in extensionNode.findall('require'):
            extendedEnums = []
            newCommands = []
            newTypes = []
            featureList = []
            # iterate over all children in current require tag
            # and add them to proper list
            for individualRequirement in requireItem:
                requirementName = individualRequirement.get("name")
                requirementComment = individualRequirement.get("comment")
                # check if this requirement was not promoted and mark
                # this extension as not fully promoted
                if requirementComment is not None and "Not promoted to" in requirementComment:
                    partiallyPromoted = True
                # check if this requirement describes enum, command or type
                if individualRequirement.tag == "enum":
                    # when api attribute is present it limits enumerator to specific api
                    supportedApi = individualRequirement.get("api")
                    if supportedApi != None and supportedApi != self.apiName:
                        continue
                    extendedEnumName = individualRequirement.get("extends")
                    extendedEnums.append(ExtensionEnumerator(
                        requirementName,
                        extendedEnumName,
                        individualRequirement.get("alias"),
                        individualRequirement.get("value"),
                        individualRequirement.get("extnumber"),
                        individualRequirement.get("offset"),
                        individualRequirement.get("bitpos"),
                        individualRequirement.get("dir"),
                        requirementComment))
                elif individualRequirement.tag == "command":
                    newCommands.append(ExtensionCommand(requirementName, requirementComment))
                elif individualRequirement.tag == "type":
                    newTypes.append(ExtensionType(requirementName, requirementComment))
                elif individualRequirement.tag == "feature":
                    featureList.append(ExtensionFeature(requirementName, individualRequirement.get("struct")))
                elif individualRequirement.tag == "comment" and "not promoted to" in individualRequirement.text:
                    # partial promotion of VK_EXT_ycbcr_2plane_444_formats and VK_EXT_4444_formats
                    # is marked with comment tag in first require section
                    partiallyPromoted = True
            # construct requirement object and add it to the list
            requirementsList.append(ExtensionRequirements(
                requireItem.get("depends"),  # dependencies that can include "and/or" grammar
                extendedEnums,               # extendedEnums
                newCommands,                 # newCommands
                newTypes,                    # newTypes
                featureList                     # features
            ))

        supportedList = extensionNode.get("supported").split(",")
        # add extension definition to proper api object
        targetExtensionList.append(Extension(
            extensionName,                   # name
            extensionNumber,                 # number
            extensionNode.get("type"),       # type
            extensionNode.get("depends"),    # depends
            extensionNode.get("platform"),   # platform
            extensionNode.get("promotedto"), # promotedto
            partiallyPromoted,               # partiallyPromoted
            requirementsList,                # requirementsList
            supportedList                    # supportedList
        ))

    def readFeature (self, featureNode):
        # api attribute limits feature just to specified api
        supportedApis = featureNode.get("api")
        isFeatureSupportedByApi = self.apiName in supportedApis.split(',')
        requirementsList = []
        for requirementGroup in featureNode:
            enumList = []
            typeList = []
            featureList = []
            commandList = []
            # skip any feature blocks that are tagged deprecate - they are deprecating things from other
            # features, and if add them here - lots of other handling gets confused
            if requirementGroup.tag == "deprecate":
                continue
            for requirement in requirementGroup:
                requirementName = requirement.get("name")
                if requirement.tag == "enum":
                    extendedEnumName = requirement.get("extends")
                    offset = requirement.get("offset")
                    extnumber = requirement.get("extnumber")
                    enumList.append(FeatureEnumerator(requirementName, extendedEnumName, offset, extnumber))
                    if isFeatureSupportedByApi and extendedEnumName is not None:
                        # find extended enum in api.enums list
                        for e in self.enums:
                            if extendedEnumName == e.name:
                                # read enumerator and add it to enum
                                alias = requirement.get("alias")
                                if alias is None:
                                    self.addEnumerator(
                                        e,
                                        requirementName,
                                        requirement.get("value"),
                                        offset,
                                        extnumber,
                                        requirement.get("bitpos"),
                                        requirement.get("dir"))
                                elif not self.addAliasToEnumerator(e, requirementName, alias):
                                    self.tempEnumAliasesList.append((e, requirementName, alias))
                                break
                elif requirement.tag == "type":
                    typeList.append(requirementName)
                elif requirement.tag == "feature":
                    featureList.append(ExtensionFeature(requirementName, requirement.get("struct")))
                elif requirement.tag == "command":
                    commandList.append(requirementName)
            requirementsList.append(FeatureRequirement(
                requirementGroup.tag,
                requirementGroup.get("comment"),
                enumList,
                typeList,
                commandList,
                featureList
            ))
        self.features.append(Feature(
            supportedApis,
            featureNode.get("name"),
            featureNode.get("number"),
            requirementsList
        ))

    def readType (self, typeNode):
        name = typeNode.get("name")
        alias = typeNode.get("alias")
        category = typeNode.get("category")
        if category == "enum":
            if alias is None:
                self.enums.append(Enum(name))
            else:
                for e in reversed(self.enums):
                    if alias == e.name:
                        e.alias = name
                        break
        elif category == "handle":
            type = None
            if alias is None:
                name = typeNode.find("name").text
                type = typeNode.find("type").text
                self.handles.append(Handle(
                    name,
                    type,
                    alias,
                    typeNode.get("parent"),
                    typeNode.get("objtypeenum"),
                ))
            else:
                for h in reversed(self.handles):
                    if alias == h.name:
                        h.alias = name
                        break
        elif category == "basetype":
            # processing only those basetypes that have type child
            type = typeNode.find("type")
            if type is not None:
                self.basetypes[typeNode.find("name").text] = type.text
        elif category == "bitmask":
            # when api attribute is present it limits bitmask to specific api
            supportedApi = typeNode.get("api")
            if supportedApi != None and supportedApi != self.apiName:
                # go to next node
                return
            # if node is alias then use the fact that alias definition follows aliased bitmasks;
            # in majoriti of cases it follows directly aliased bitmasks but in some cases there
            # is a unrelated bitmasks definition in between - to handle this traverse in reverse order
            if alias is not None:
                for bm in reversed(self.bitmasks):
                    if alias == bm.name:
                        bm.alias = name
                        break
            else:
                self.bitmasks.append(Bitmask(
                    typeNode.find("name").text,
                    typeNode.find("type").text,
                    typeNode.get("requires"),
                    typeNode.get("bitvalues")
                ))
        elif category in ["struct", "union"]:
            # if node is alias then use the fact that alias definition follows aliased structure;
            # in majoriti of cases it follows directly aliased structure but in some cases there
            # is a unrelated structure definition in between - to handle this traverse in reverse order
            if alias is not None:
                if alias is not None:
                    self.tempCompositeTypesAliases.append([name, alias])
                    # go to next node
                    return
                # go to next node
                return
            # read structure members
            structMembers = []
            for memberNode in typeNode:
                if memberNode.tag != "member":
                    continue
                # when api attribute is present it limits bitmask to specific api
                supportedApi = memberNode.get("api")
                if supportedApi != None and supportedApi != self.apiName:
                    # go to next member
                    continue
                # handle enum nodes that can be used for array dimensions
                arraySizeList = []
                for node in memberNode:
                    if node.tag == "enum":
                        arraySizeList.append(node.text)
                        # check if there are array dimension that are not enums
                        if '[' in node.tail and len(node.tail) > 2:
                            arraySizeList += node.tail.replace(']', ' ').replace('[', ' ').split()
                # handle additional text after name tag; it can represent array
                # size like in VkPipelineFragmentShadingRateEnumStateCreateInfoNV
                # or number of bits like in VkAccelerationStructureInstanceKHR
                nameNode = memberNode.find("name")
                nameTail = nameNode.tail
                fieldWidth = None
                if nameTail:
                    if ':' in nameTail:
                        fieldWidth = nameTail.replace(':', '').replace(' ', '')
                    elif '[' in nameTail and ']' in nameTail:
                        nameTail = nameTail.replace(']', ' ').replace('[', ' ')
                        arraySizeList = nameTail.split() + arraySizeList
                # handle additional text after type tag; it can represent pointers like *pNext
                memberTypeNode = memberNode.find("type")
                pointer = memberTypeNode.tail.strip() if memberTypeNode.tail is not None else None
                structMembers.append(CompositeMember(
                    nameNode.text,               # name
                    memberTypeNode.text,         # type
                    pointer,                     # pointer
                    memberNode.text,             # qualifiers
                    arraySizeList,               # arraySizeList
                    memberNode.get("optional"),  # optional
                    memberNode.get("limittype"), # limittype
                    memberNode.get("values"),    # values
                    fieldWidth                   # fieldWidth
                ))
            # create structure definition
            self.compositeTypes.append(Composite(
                name,
                category,
                typeNode.get("allowduplicate"),
                typeNode.get("structextends"),
                typeNode.get("returnedonly"),
                structMembers
            ))
        elif category == "define":
            nNode = typeNode.find("name")
            tNode = typeNode.find("type")
            if nNode == None or tNode == None:
                return
            requires = typeNode.get("requires")
            name = nNode.text
            if "API_VERSION_" in name or requires == "VK_MAKE_VIDEO_STD_VERSION":
                value = tNode.tail
                value = tNode.text + value[:value.find(')')+1]
                value = value.replace('VKSC_API_VARIANT', '1')
                self.defines.append(Define(
                    name,
                    "uint32_t",
                    None,
                    value
                ))
        else:
            requires = typeNode.get("requires")
            if requires == 'vk_platform':
                self.basicCTypes.append(name)

    def build (self, rawVkXml):
        # iterate over all *.xml root children
        for rootChild in rawVkXml.getroot():

            # each enum is defined in separate enums node directly under root node
            if rootChild.tag == "enums":
                self.readEnum(rootChild)

            # read function definitions
            if rootChild.tag == "commands":
                commandsNode = rootChild
                for commandItem in commandsNode:
                    self.readCommand(commandItem)

            # read vulkan versions
            if rootChild.tag == "feature":
                self.readFeature(rootChild)

            # read extensions
            if rootChild.tag == "extensions":
                extensionsNode = rootChild
                for extensionItem in extensionsNode:
                    self.readExtension(extensionItem)

            # "types" is a first child of root so it's optimal to check for it
            # last and don't repeat this check for all other iterations
            if rootChild.tag == "types":
                typesNode = rootChild
                for typeItem in typesNode:
                    self.readType(typeItem)

    def postProcess (self):

        if self.apiName == "vulkansc":
            # temporary workaround for extensions that are marked only for vulkan api in xml while
            # they are need by vulkan_json_data.hpp and vulkan_json_parser.hpp in vulkansc
            workAroundList = [
                    "VK_NV_device_diagnostic_checkpoints",
                    "VK_KHR_format_feature_flags2",
                    "VK_EXT_vertex_attribute_divisor",
                    "VK_EXT_global_priority",
                    "VK_EXT_calibrated_timestamps",
            ]
            for extName in workAroundList:
                extData = [e for e in self.notSupportedExtensions if e.name == extName]
                if len(extData):
                    extData = extData[0]
                    self.extensions.append(extData)
                    self.notSupportedExtensions.remove(extData)
            # temporary workaround for enums needed by vulkan_json_parser.hpp that are marked only for vulkan api in xml
            scFeatures = [f for f in self.features if f.api == "vulkansc"][0]
            for enum in self.enums:
                if enum.name == "VkPipelineLayoutCreateFlagBits":
                    self.addEnumerator(enum, "VK_PIPELINE_LAYOUT_CREATE_RESERVED_0_BIT_AMD", None, None, None, 0, None)
                    self.addEnumerator(enum, "VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT", None, None, None, 1, None)
                    scFeatures.requirementsList[0].typeList.append(enum.name)
                    break
        # add new enumerators that were added by extensions to api.enums
        # we have to do it at the end for SC because some enums are dependent from extensions/api versions
        # and those dependencies can be checked only after all extensions were read
        for ext in self.extensions:
            logging.debug("Considering extension %s for API %s" % (ext.name, apiName))
            for requirement in ext.requirementsList:
                # check if this requirement is supported by current implementation
                isRequirementSupported = isDependencyMet(requirement.depends, self.extensions)
                # add enumerator to proper enum from api.enums
                if isRequirementSupported:
                    for enumerator in requirement.extendedEnums:
                        if enumerator.extends is None:
                            continue
                        # find enum in api.enums
                        matchedEnums = [enum for enum in self.enums if enumerator.extends == enum.name or enumerator.extends == enum.alias]
                        if len(matchedEnums) == 0:
                            logging.error("Could not find enum %s extends %s in %s " % (enumerator.name, enumerator.extends, self.apiName))
                        matchedEnum = matchedEnums[0]
                        # add enumerator only when it is not already in enum
                        if len([e for e in matchedEnum.enumeratorList if e.name == enumerator.name]) == 0:
                            if enumerator.alias == None:
                                logging.debug("Adding enum value %s for extension %s in API %s" % (enumerator.name, ext.name, apiName))
                                self.addEnumerator(
                                        matchedEnum,
                                        enumerator.name,
                                        enumerator.value,
                                        enumerator.offset,
                                        enumerator.extnumber if enumerator.extnumber else ext.number,
                                        enumerator.bitpos,
                                        enumerator.dir)
                            elif not self.addAliasToEnumerator(matchedEnum, enumerator.name, enumerator.alias):
                                # we might not be able to add alias as we might be missing what we are aliasing
                                # this will happen when aliased enum is added later then definition of alias
                                logging.debug("Adding alias %s to enum %s for extension %s in API %s" % (enumerator.name, matchedEnum.name, ext.name, apiName))
                                self.tempEnumAliasesList.append((matchedEnum, enumerator.name, enumerator.alias))
                else:
                    logging.warning("Skipping requirement in extension %s because dependencies are not met: %s" % (ext.name, requirement.depends))

        # add aliases to enumerators that were defined after alias definition
        for enum, name, alias in self.tempEnumAliasesList:
            if not self.addAliasToEnumerator(enum, name, alias):
                # if enumerator that should be aliased was not found then try to insert it without alias
                # (this happens for vulkansc as in xml enumerator might be defined in extension that is
                # not supported by sc or in normal vulkan version)
                def tryToFindEnumValueInNotSupportedExtensions(searchedName):
                    for nsExt in self.notSupportedExtensions:
                        for r in nsExt.requirementsList:
                            for enumerator in r.extendedEnums:
                                if enumerator.name == searchedName:
                                    self.addEnumerator(
                                        enum,
                                        name,
                                        enumerator.value,
                                        enumerator.offset,
                                        enumerator.extnumber if enumerator.extnumber else ext.number,
                                        enumerator.bitpos,
                                        enumerator.dir)
                                    # there is still 1 case where alias that is not part of SC needs to be added for SC
                                    self.addAliasToEnumerator(enum, alias, name)
                                    return True
                    return False
                def tryToFindEnumValueInNotSupportedVersions(searchedName):
                    for f in self.features:
                        # check only not supported features
                        if self.apiName in f.api.split(','):
                            continue
                        for r in f.requirementsList:
                            for enumerator in r.enumList:
                                if enumerator.name == searchedName:
                                    assert enumerator.extnumber is not None
                                    self.addEnumerator(
                                        enum,
                                        name,
                                        None,
                                        enumerator.offset,
                                        enumerator.extnumber,
                                        None,
                                        None)
                                    self.addAliasToEnumerator(enum, alias, name)
                                    return True
                    return False
                # using functions for fast stack unwinding
                if tryToFindEnumValueInNotSupportedExtensions(alias):
                    continue
                if tryToFindEnumValueInNotSupportedVersions(alias):
                    continue
        self.tempEnumAliasesList = None

        for alias in self.tempCompositeTypesAliases:
            for ct in reversed(self.compositeTypes):
                if alias[1] == ct.name or alias in ct.aliasList:
                    ct.aliasList.append(alias[0])
                    break

        if self.apiName == "vulkan":
            def removeExtensionFromApi(extName, structureNameList, commandNameList):
                extObjectList = [e for e in api.extensions if e.name == extName]
                if len(extObjectList) > 0:
                    api.extensions.remove(extObjectList[0])
                structObjectList = [ct for ct in api.compositeTypes if ct.name in structureNameList]
                for s in structObjectList:
                    api.compositeTypes.remove(s)
                commandObjectList = [f for f in api.functions if f.name in commandNameList]
                for f in commandObjectList:
                    api.functions.remove(f)

            # remove structures and commands added by VK_EXT_directfb_surface extension
            removeExtensionFromApi("VK_EXT_directfb_surface",
                                   ["VkDirectFBSurfaceCreateFlagsEXT", "VkDirectFBSurfaceCreateInfoEXT"],
                                   ["vkCreateDirectFBSurfaceEXT", "vkGetPhysicalDeviceDirectFBPresentationSupportEXT"])

            # remove structures and commands added by disabled VK_ANDROID_native_buffer extension;
            # disabled extensions aren't read but their structures and commands will be in types and commands sections in vk.xml
            removeExtensionFromApi("VK_ANDROID_native_buffer",
                                   ["VkNativeBufferANDROID", "VkSwapchainImageCreateInfoANDROID",
                                    "VkPhysicalDevicePresentationPropertiesANDROID", "VkNativeBufferUsage2ANDROID",
                                    "VkSwapchainImageUsageFlagBitsANDROID", "VkSwapchainImageUsageFlagsANDROID"],
                                   ["vkGetSwapchainGrallocUsageANDROID", "vkAcquireImageANDROID",
                                    "vkQueueSignalReleaseImageANDROID", "vkGetSwapchainGrallocUsage2ANDROID"])

            # remove empty enums e.g. VkQueryPoolCreateFlagBits, VkDeviceCreateFlagBits
            enumsToRemove = [enum for enum in self.enums if len(enum.enumeratorList) == 0]
            for er in enumsToRemove:
                self.enums.remove(er)

            # add alias for VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR (in vk.xml for this struct alias is defined before struct
            # where in all other cases it is defined after structure definition)
            barycentricFeaturesStruct = [c for c in api.compositeTypes if c.name == 'VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR'][0]
            barycentricFeaturesStruct.aliasList.append('VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV')

        elif self.apiName == "vulkansc":
            # remove commands that are marked with <remove> tag in SC feature specification;
            # e.g. there is no vkCreateShaderModule in SC
            functionsToRemove = []
            scFeatures = [f for f in self.features if f.api == "vulkansc"][0]
            for featureRequirement in scFeatures.requirementsList:
                if featureRequirement.operation == "remove":
                    # find function in the list of all functions
                    for fun in self.functions:
                        if fun.name in featureRequirement.commandList:
                            functionsToRemove.append(fun)
            for fun in functionsToRemove:
                self.functions.remove(fun)

            # sc is based on vk1.2 so we need to check features of vk1.3+
            # and rename functions and structures that were promoted in
            # those versions to their previous names (aliases)
            renamedStructuresDict = {}
            for feature in self.features:
                # skip vk versions smaller than 1.3
                if int(feature.number[-1]) < 3:
                    continue
                # iterate over all requirements and enums/commands/structs added in them
                for featureRequirement in feature.requirementsList:
                    renamedFunctionsList = []
                    # find promotedFun in list of all functions
                    for fun in self.functions:
                        if fun.name not in featureRequirement.commandList:
                            continue
                        # replace function name with its last alias
                        fun.name = fun.aliasList[-1]
                        fun.aliasList = fun.aliasList[:-1]
                        # memorize renamed functions
                        renamedFunctionsList.append(fun)
                    # skip renaming enums and structures for extensions that are available for SC
                    if featureRequirement.comment is not None:
                        matchedExtension = re.search(r'Promoted from (\w+) ', featureRequirement.comment, re.IGNORECASE)
                        if matchedExtension is not None:
                            promotedExtensionName = matchedExtension.group(1)
                            extensionList = [e for e in self.extensions if e.name == promotedExtensionName]
                            if len(extensionList) > 0:
                                continue
                    for promotedEnumerator in featureRequirement.enumList:
                        # iterate over all enums and find one that was extended
                        for enum in self.enums:
                            logging.debug("Considering enum %s for API %s" % (enum.name, api.apiName))
                            if enum.name != promotedEnumerator.extends:
                                continue
                            enumeratorReplaced = False
                            # find enumerator that should have changed name
                            for enumerator in enum.enumeratorList:
                                if enumerator.name != promotedEnumerator.name or len(enumerator.aliasList) == 0:
                                    continue
                                # replace enumerator name with its first alias
                                enumerator.name = enumerator.aliasList[0]
                                enumerator.aliasList = enumerator.aliasList[1:]
                                # first member of almost all structures is VkStructureType and in xml that member
                                # has defined value - we need to change those values to versions supported by SC
                                if "STRUCTURE_TYPE" in enumerator.name:
                                    for struct in self.compositeTypes:
                                        if struct.members[0].values == promotedEnumerator.name:
                                            struct.members[0].values = enumerator.name
                                            break
                                enumeratorReplaced = True
                                break
                            if enumeratorReplaced:
                                break
                    structsToRemove = []
                    # find promotedStruct in list of all structures
                    for struct in self.compositeTypes:
                        promotedStruct = struct.name
                        if promotedStruct not in featureRequirement.typeList:
                            continue
                        # skip structures without alias
                        if len(struct.aliasList) == 0:
                            # remove VkPhysicalDeviceVulkan13Features/Properties
                            if "VkPhysicalDeviceVulkan" in promotedStruct:
                                structsToRemove.append(struct)
                            continue
                        # replace struct name with its last alias
                        struct.notSupportedAlias = struct.name
                        struct.name = struct.aliasList[-1]
                        struct.aliasList = struct.aliasList[:-1]
                        # memorize all renamed structures
                        renamedStructuresDict[promotedStruct] = struct
                        # check all renamed functions and make sure that argument types are also renamed
                        for renamedFun in renamedFunctionsList:
                            for arg in renamedFun.arguments:
                                if arg.type == promotedStruct:
                                    arg.type = struct.name
                    # remove structures that were marked for removal
                    for st in structsToRemove:
                        self.compositeTypes.remove(st)

            # iterate over all renamed structures and make sure that all their attributes are also renamed
            for newName in renamedStructuresDict:
                for member in renamedStructuresDict[newName].members:
                    if member.type in renamedStructuresDict:
                        member.type = renamedStructuresDict[member.type].name

        # remove enums that are not part of any vulkan version nor extension
        # (SC specific enums are in vk.xml without any attribute identifying that they are SC specific; same for enums for disabled extensions)
        def isEnumUsed(enumName, enumAlias):
            for feature in self.features:
                if self.apiName not in feature.api.split(','):
                    continue
                for requirement in feature.requirementsList:
                    for typeName in requirement.typeList:
                        if (typeName == enumName) or (typeName == enumAlias):
                            return True
            for ext in self.extensions:
                for requirement in ext.requirementsList:
                    for newType in requirement.newTypes:
                        if (newType.name == enumName) or (newType.name == enumAlias):
                            return True
                    for extendedEnum in requirement.extendedEnums:
                        if extendedEnum.extends == enumName:
                            return True
            return False
        # do removal using above function
        enumsToRemove = []
        for enum in self.enums:
            if isEnumUsed(enum.name, enum.alias):
                continue
            enumsToRemove.append(enum)
        for er in enumsToRemove:
            logging.debug("Removing enum %s because not used in API %s" % (er.name, self.apiName))
            self.enums.remove(er)

        # remove structures that are not part of any vulkan version nor extension; SC specific
        # structures are in vk.xml without any attribute identifying that they are SC specific
        def isStructUsed(structNameList):
            for feature in self.features:
                if self.apiName not in feature.api.split(','):
                    continue
                for requirement in feature.requirementsList:
                    for typeName in requirement.typeList:
                        if typeName in structNameList:
                            return True
            for ext in self.extensions:
                for requirement in ext.requirementsList:
                    for newType in requirement.newTypes:
                        if newType.name in structNameList:
                            return isDependencyMet(requirement.depends, self.extensions)
            return False

        structsToRemove = []
        for struct in self.compositeTypes:
            structNameList = [struct.name] + struct.aliasList
            if isStructUsed(structNameList):
                continue
            structsToRemove.append(struct)
        for st in structsToRemove:
            self.compositeTypes.remove(st)

        # remove commands that are not part of any vulkan version nor extension
        # (SC specific commands are in vk.xml without any attribute identifying that they are SC specific)
        def isFunctionUsed(functionNameList):
            for feature in self.features:
                if self.apiName not in feature.api.split(','):
                    continue
                for requirement in feature.requirementsList:
                    for commandName in requirement.commandList:
                        if commandName in functionNameList:
                            return True
            for ext in self.extensions:
                for requirement in ext.requirementsList:
                    for newCommand in requirement.newCommands:
                        if newCommand.name in functionNameList:
                            return isDependencyMet(requirement.depends, self.extensions)
            return False

        functionsToRemove = []
        for fun in self.functions:
            functionNameList = [fun.name] + fun.aliasList
            if isFunctionUsed(functionNameList):
                continue
            functionsToRemove.append(fun)
        for fun in functionsToRemove:
            logging.debug("Removing function %s because not used in API %s" % (fun.name, self.apiName))
            self.functions.remove(fun)

        # remove handles that are not part of any vulkan command or structure
        def isHandleUsed(structList, functionList, handleName):
            for struct in structList:
                for member in struct.members:
                    if handleName in member.type:
                        return True
            for fun in functionList:
                for arg in fun.arguments:
                    if handleName in arg.type:
                        return True
            return False

        handlesToRemove = []
        for h in self.handles:
            if isHandleUsed(self.compositeTypes, self.functions, h.name):
                continue
            handlesToRemove.append(h)
        for h in handlesToRemove:
            logging.debug("Removing unused handle %s from API %s" % (h.name, self.apiName))
            self.handles.remove(h)

        for ext in self.extensions:
            if ext.promotedto is not None and "VK_VERSION" not in ext.promotedto:
                # verify that promotedto extensions are supported by the api
                if not any(x.name == ext.promotedto for x in self.extensions):
                    ext.promotedto = None
                if ext.promotedto is None:
                    continue
                # fill promotedFrom attribute
                for e in self.extensions:
                    if ext.promotedto == e.name:
                        if e.promotedFrom == None:
                            e.promotedFrom = [ext.name]
                        else:
                            e.promotedFrom.append(ext.name)
                        break

        # sort enumerators in enums
        sortLambda = lambda enumerator: int(enumerator.bitpos) if enumerator.value is None else int(enumerator.value, 16 if 'x' in enumerator.value else 10)
        for enum in self.enums:
            # skip enums that have no items or  just one in enumeratorList (e.g. VkQueryPoolCreateFlagBits)
            if len(enum.enumeratorList) < 2:
                continue
            # construct list of enumerators in which value and bitpos are not None
            enumeratorsToSort = [e for e in enum.enumeratorList if e.value != e.bitpos]
            # construct list of enumerators in which value and bitpos are equal to None
            remainingEnumerators = [e for e in enum.enumeratorList if e.value == e.bitpos]
            # construct sorted enumerator list with aliases at the end
            enum.enumeratorList = sorted(enumeratorsToSort, key=sortLambda)
            enum.enumeratorList.extend(remainingEnumerators)

        # Fill in extension data that comes from the XML
        additionalExtensionNames = [item[0] for item in self.additionalExtensionData]
        for ext in self.extensions:
            if ext.name not in EXTENSIONS_TO_READ_FROM_XML_NOT_JSON:
                continue
            if ext.name in additionalExtensionNames:
                logging.error("Extension %s already defined as JSON!" % (ext.name))
            if ext.promotedto is not None and 'VK_VERSION' not in ext.promotedto:
                logging.error("Extension %s is promoted to %s" % (ext.name, ext.promotedto))
                exit(-1)
            mandatoryFeatures = {}
            core = ""
            mandatory_variants = ext.supported
            if ext.promotedto is not None and 'VK_VERSION' in ext.promotedto:
                p = ext.promotedto
                major = int(p[-3])
                minor = int(p[-1])
                core = f'0.{major}.{minor}.0'
                if "vulkan" in mandatory_variants and major == 1 and minor <= 2:
                    mandatory_variants = []
            else:
                if "vulkansc" not in mandatory_variants:
                    mandatory_variants = []
            for requirement in ext.requirementsList:
                featureStructName = None
                featureStruct = None
                for feature in requirement.features:
                    newFeatureStructName = feature.struct
                    if newFeatureStructName not in mandatoryFeatures.keys():
                        mandatoryFeatures[newFeatureStructName] = []
                    if newFeatureStructName != featureStructName:
                        featureStructName = newFeatureStructName
                        featureStruct = {'features': [], 'requirements': [], 'mandatory_variant': []}
                        mandatoryFeatures[featureStructName].append(featureStruct)
                    feature_names = feature.name.split(',')
                    featureStruct['features'].extend(feature_names)
                    featureStruct['requirements'].append(ext.name)
                    if requirement.depends is not None:
                        featureStruct['requirements'].append(requirement.depends)
                    if len(mandatory_variants) > 0:
                        featureStruct["mandatory_variant"] = mandatory_variants

                #
                # for reqtype in requirement.newTypes:
                #     for ct in api.compositeTypes:
                #         if reqtype.name == ct.name or reqtype in ct.aliasList:
                #             featureStructName = ct.name
                #             featureStruct = {'features': [], 'requirements': [], 'mandatory_variant': []}
                #             for m in ct.members:
                #                 if m.name not in ["sType", "pNext"]:
                #                     featureStruct['features'].append(m.name)
                #                     featureStruct['requirements'].append(ext.name)
                #             if requirement.depends is not None:
                #                 featureStruct['requirements'].append(requirement.depends)
                #             if len(mandatory_variants) > 0:
                #                 featureStruct["mandatory_variant"] = mandatory_variants
                #             featureStruct['features'] = list(dict.fromkeys(featureStruct['features']))
                #             featureStruct['requirements'] = list(dict.fromkeys(featureStruct['requirements']))
                #             featureStruct['mandatory_variant'] = list(dict.fromkeys(featureStruct['mandatory_variant']))
                #             if len(featureStruct['mandatory_variant']) == 0:
                #                 featureStruct.pop('mandatory_variant')
                #             if featureStructName not in mandatoryFeatures.keys():
                #                 mandatoryFeatures[featureStructName] = []
                #             mandatoryFeatures[featureStructName].append(featureStruct)
            for featureStructName in mandatoryFeatures.keys():
                for featureStruct in mandatoryFeatures[featureStructName]:
                    featureStruct['features'] = list(dict.fromkeys(featureStruct['features']))
                    featureStruct['requirements'] = list(dict.fromkeys(featureStruct['requirements']))
                    featureStruct['mandatory_variant'] = list(dict.fromkeys(featureStruct['mandatory_variant']))
                    if len(featureStruct['mandatory_variant']) == 0:
                        featureStruct.pop('mandatory_variant')
            data = {}
            if ext.name.startswith("VK_KHR") or ext.name.startswith("VK_EXT"):
                data['register_extension'] = {'type': ext.type, 'core': core}
            if len(mandatoryFeatures) > 0:
                data['mandatory_features'] = mandatoryFeatures

            jsonFilePath = os.path.join(SCRIPTS_SRC_DIR, "extensions", ext.name + ".json")
            with open(jsonFilePath, 'w') as file:
                printAttributesToFile(data, file, indent=4)
                logging.debug("File written to " + jsonFilePath)
            api.additionalExtensionData.append((ext.name, data))

        # Here we do the API version requirements
        for apiFeature in self.features:
            if apiFeature.name not in EXTENSIONS_TO_READ_FROM_XML_NOT_JSON:
                continue
            if apiFeature.name in additionalExtensionNames:
                logging.error("API feature %s already defined as JSON!" % (ext.name))
            mandatoryFeatures = {}
            for requirement in apiFeature.requirementsList:
                featureStructName = None
                featureStruct = None
                for feature in requirement.features:
                    newFeatureStructName = feature.struct
                    for ct in self.compositeTypes:
                        if newFeatureStructName in ct.aliasList:
                            newFeatureStructName = ct.name
                    if newFeatureStructName not in mandatoryFeatures.keys():
                        mandatoryFeatures[newFeatureStructName] = []
                    if newFeatureStructName != featureStructName:
                        featureStructName = newFeatureStructName
                        featureStruct = {'features': [], 'requirements': []}
                        mandatoryFeatures[featureStructName].append(featureStruct)
                    featureStruct['features'].append(feature.name)
                    # if feature.name == "vulkanMemoryModel":
                    #     logging.debug("feature %s %s in %s" % (feature.name, featureStructName, apiFeature.name))
                    #     exit(-1)
                    dep = apiFeature.name
                    if 'VK_VERSION' in dep:
                        major = int(dep[-3])
                        minor = int(dep[-1])
                        featureStruct['requirements'].append(f"ApiVersion(0, {major}, {minor}, 0)")
                    else:
                        logging.error("requirement not valid in %s" % (apiFeature.name))
                        exit(-1)
                if featureStructName is not None:
                    featureStruct['features'] = list(dict.fromkeys(featureStruct['features']))
                    featureStruct['requirements'] = list(dict.fromkeys(featureStruct['requirements']))
            data = {'mandatory_features': mandatoryFeatures}
            jsonFilePath = os.path.join(SCRIPTS_SRC_DIR, "extensions", apiFeature.name + ".json")
            with open(jsonFilePath, 'w') as file:
                printAttributesToFile(data, file, indent=4)
                logging.debug("File written to " + jsonFilePath)
            api.additionalExtensionData.append((apiFeature.name, data))

        self.additionalExtensionData = sorted(self.additionalExtensionData, key=lambda e: e[0])

        for ext in self.extensions:
            if not ext.name.startswith("VK_KHR"):
                continue
            jsonFilePath = os.path.join(SCRIPTS_SRC_DIR, "extensions", ext.name + ".json")
            if os.path.isfile(jsonFilePath):
                logging.info("Extension %s has json %s", ext.name, jsonFilePath)
            else:
                logging.error("Extension %s is missing JSON!", ext.name)

def prefixName (prefix, name):
    name = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name[2:])
    name = re.sub(r'([a-zA-Z])([0-9])', r'\1_\2', name)
    name = name.upper()
    return prefix + name

def parseInt (value):
    return int(value, 16 if ("0x" in value) else 10)

def readFile (filename):
    with open(filename, 'rt') as f:
        return f.read()

def getInterfaceName (functionName):
    assert functionName[:2] == "vk"
    return functionName[2].lower() + functionName[3:]

def getFunctionTypeName (functionName):
    assert functionName[:2] == "vk"
    return functionName[2:] + "Func"

def endsWith (str, postfix):
    return str[-len(postfix):] == postfix

def writeHandleType (api, filename):

    def getHandleName (name):
        return prefixName("HANDLE_TYPE_", name)

    def genHandles ():
        yield "\t%s\t= 0," % getHandleName(api.handles[0].name)
        for h in api.handles[1:]:
            yield "\t%s," % getHandleName(h.name)
        for h in api.handles:
            if h.alias is not None:
                yield "\t%s\t= %s," % (getHandleName(h.alias), getHandleName(h.name))
        yield "\tHANDLE_TYPE_LAST\t= %s + 1" % (getHandleName(api.handles[-1].name))

    def genHandlesBlock ():
        yield "enum HandleType"
        yield "{"

        for line in indentLines(genHandles()):
            yield line

        yield "};"
        yield ""

    writeInlFile(filename, INL_HEADER, genHandlesBlock())

def stripExtensionSuffix(extensionName):
    for p in EXTENSION_POSTFIXES:
        if extensionName.endswith(p):
            s = extensionName[:-len(p)]
            return s, '_'+p
    return extensionName, None

def getEnumValuePrefixAndPostfix (enum):
    prefix = enum.name[0]
    for i in range(1, len(enum.name)):
        if enum.name[i].isupper() and not enum.name[i-1].isupper():
            prefix += "_"
        prefix += enum.name[i].upper()
    for p in EXTENSION_POSTFIXES:
        if prefix.endswith(p):
            return prefix[:-len(p)-1], '_'+p
    return prefix, ''

def genEnumSrc (enum):
    yield "enum %s" % enum.name
    yield "{"
    lines = []
    for ed in enum.enumeratorList:
        if ed.value is not None:
            lines.append(f"\t{ed.name}\t= {ed.value},")
    for ed in enum.enumeratorList:
        for alias in ed.aliasList:
            lines.append(f"\t{alias}\t= {ed.name},")

    # add *_LAST item when enum is linear
    prefix, postfix = getEnumValuePrefixAndPostfix(enum)
    if enum.areValuesLinear():
        lines.append(f"\t{prefix}{postfix}_LAST,")

    # add _MAX_ENUM item with the ext postifix at the end
    lines.append(f"\t{prefix}_MAX_ENUM{postfix}\t= 0x7FFFFFFF")

    for line in indentLines(lines):
        yield line

    yield "};"

def genBitfieldSrc (bitfield):
    lines = []
    for ev in bitfield.enumeratorList:
        # bitfields may use mix of bitpos and values
        if ev.bitpos is not None:
            value = pow(2, int(ev.bitpos))
            lines.append(f"\t{ev.name}\t= {value:#010x},")
        if ev.value is not None:
            lines.append(f"\t{ev.name}\t= {ev.value},")
    for ev in bitfield.enumeratorList:
        for alias in ev.aliasList:
            lines.append(f"\t{alias}\t= {ev.name},")
    # add _MAX_ENUM item
    prefix, postfix = getEnumValuePrefixAndPostfix(bitfield)
    lines.append(f"\t{prefix}_MAX_ENUM{postfix}\t= 0x7FFFFFFF")
    yield f"enum {bitfield.name}"
    yield "{"
    for line in indentLines(lines):
        yield line
    yield "};"

def genBitfield64Src (bitfield64):
    def generateEntry(lines, bitfieldName, entryName, bitpos, value):
        if entryName is None:
            return
        # bitfields may use mix of bitpos and values
        if ev.bitpos is not None:
            v = pow(2, int(bitpos))
            lines.append(f"static const {bitfieldName} {entryName}\t= {v:#010x}ULL;")
        if value is not None:
            lines.append(f"static const {bitfieldName} {entryName}\t= {value}ULL;")

    yield f"typedef uint64_t {bitfield64.name};"
    lines = []
    for ev in bitfield64.enumeratorList:
        generateEntry(lines, bitfield64.name, ev.name,  ev.bitpos, ev.value)
        for alias in ev.aliasList:
            generateEntry(lines, bitfield64.name, alias, ev.bitpos, ev.value)
    # write indented lines
    for line in indentLines(lines):
        yield line
    yield ""

def genDefinesSrc (apiName, defines):
    def genLines (defines):
        for d in defines:
            if d.alias is not None:
                continue
            defineType = DEFINITIONS.get(d.name, d.type)
            yield f"#define {d.name}\t(static_cast<{defineType}>\t({d.value}))"
    for line in indentLines(genLines(defines)):
        yield line
    # add VK_API_MAX_FRAMEWORK_VERSION
    major, minor = 1, 0
    # In vk.xml, vulkan features (1.1, 1.2, 1.3) are marked as vulkan,vulkansc
    api_feature_name = "vulkan,vulkansc" if api.apiName == "vulkan" else api.apiName
    sorted_features = reversed(sorted(api.features, key=lambda feature: feature.number))
    for feature in sorted_features:
        if feature.api == api_feature_name:
            major, minor = feature.number.split('.')
            break
    logging.debug("Found max framework version for API '%s': %s.%s" % (api.apiName, major, minor))
    yield f"#define VK{apiName}_API_MAX_FRAMEWORK_VERSION\tVK{apiName}_API_VERSION_{major}_{minor}"

def genHandlesSrc (handles):
    def genLines (handles):
        for h in handles:
            handleType = h.type
            handleObjtype = h.objtypeenum
            if h.alias is not None:
                # search for aliased handle
                for searchedHandle in handles:
                    if h.alias == searchedHandle.name:
                        handleType = searchedHandle.type
                        handleObjtype = searchedHandle.objtypeenum
                        break
            yield f"{handleType}\t({h.name},\tHANDLE{handleObjtype[9:]});"
    for line in indentLines(genLines(handles)):
        yield line

def genHandlesSrc (handles):
    def genLines (handles):
        for h in handles:
            handleType    = h.type
            handleObjtype = h.objtypeenum
            line = f"{handleType}\t({{}},\tHANDLE{handleObjtype[9:]});"
            yield line.format(h.name)
            if h.alias is not None:
                yield line.format(h.alias)

    for line in indentLines(genLines(handles)):
        yield line

def writeBasicTypes (api, filename):

    def gen ():
        yield "// Defines"
        for line in genDefinesSrc("SC" if api.apiName == "vulkansc" else "", api.defines):
            yield line
        yield ""

        yield "// Handles"
        for line in genHandlesSrc(api.handles):
            yield line
        yield ""

        yield "// Enums"
        for enum in api.enums:
            # skip empty enums only for vulkan
            # vulkan_json_data.hpp and vulkan_json_parser.hpp in SC need many empty enums
            if len(enum.enumeratorList) == 0 and api.apiName == "vulkan":
                continue
            if enum.type == "bitmask":
                if enum.bitwidth == "32":
                    for line in genBitfieldSrc(enum):
                        yield line
                else:
                    for line in genBitfield64Src(enum):
                        yield line
            else:
                for line in genEnumSrc(enum):
                    yield line
            if enum.alias is not None:
                yield f"typedef {enum.name} {enum.alias};"
            yield ""

        yield "// Bitmasks"
        for bitmask in api.bitmasks:
            plainType = api.basetypes[bitmask.type]
            yield f"typedef {plainType} {bitmask.name};\n"
            if bitmask.alias:
                yield f"typedef {bitmask.name} {bitmask.alias};\n"

        yield ""
        for line in indentLines(["VK_DEFINE_PLATFORM_TYPE(%s,\t%s)" % (s[0], c) for n, s, c in PLATFORM_TYPES]):
            yield line
        yield ""

        yield "// Extensions"
        for ext in api.extensions:
            firstRequirementEnums = ext.requirementsList[0].extendedEnums
            for e in firstRequirementEnums:
                if e.extends is None and e.value is not None:
                    yield "#define " + e.name + " " + e.value

    writeInlFile(filename, INL_HEADER, gen())

def writeCompositeTypes (api, filename):
    # function that returns definition of structure member
    def memberAsString (member):
        result = ''
        if member.qualifiers:
            result += member.qualifiers
        result += member.type
        if member.pointer:
            result += member.pointer
        result += '\t' + member.name
        for size in member.arraySizeList:
            result += f"[{size}]"
        if member.fieldWidth:
            result += f":{member.fieldWidth}"
        return result

    # function that prints single structure definition
    def genCompositeTypeSrc (type):
        structLines = "%s %s\n{\n" % (type.category, type.name)
        for line in indentLines(['\t'+memberAsString(m)+';' for m in type.members]):
            structLines += line + '\n'
        return structLines + "};\n"

    # function that prints all structure definitions and alias typedefs
    def gen ():
        # structures in xml are not ordered in a correct way for C++
        # we need to save structures that are used in other structures first
        allStructureNamesList = [s.name for s in api.compositeTypes]
        commonTypesList = api.basicCTypes + ['VkStructureType']
        savedStructureNamesList = []
        delayedStructureObjectsList = []

        # helper function that checks if all structure members were already saved
        def canStructBeSaved(compositeObject):
            for m in compositeObject.members:
                # check first commonTypesList to speed up the algorithm
                if m.type in commonTypesList:
                    continue
                # make sure that member is not of same type as compositeObject
                # (this hadles cases like VkBaseOutStructure)
                if m.type == compositeObject.name:
                    continue
                # if member is of compositeType that was not saved we cant save it now
                if m.type in allStructureNamesList and m.type not in savedStructureNamesList:
                    return False
            return True

        # iterate over all composite types
        lastDelayedComposite = None
        for ct in api.compositeTypes:
            # check if one of delayed structures can be saved
            delayedButSaved = []
            for dct in delayedStructureObjectsList:
                if lastDelayedComposite != dct and canStructBeSaved(dct):
                    yield genCompositeTypeSrc(dct)
                    delayedButSaved.append(dct)
            lastDelayedComposite = None
            for dsct in delayedButSaved:
                savedStructureNamesList.append(dsct.name)
                delayedStructureObjectsList.remove(dsct)
            # check if current structure can be saved
            if canStructBeSaved(ct):
                yield genCompositeTypeSrc(ct)
                savedStructureNamesList.append(ct.name)
            else:
                delayedStructureObjectsList.append(ct)
                # memorize structure that was delayed in last iteration to
                # avoid calling for it canStructBeSaved in next iteration
                lastDelayedComposite = ct
        # save remaining delayed composite types (~4 video related structures)
        while len(delayedStructureObjectsList) > 0:
            for dct in delayedStructureObjectsList:
                if canStructBeSaved(dct):
                    yield genCompositeTypeSrc(dct)
                    savedStructureNamesList.append(dct.name)
                    delayedStructureObjectsList.remove(dct)
                    break
        # write all alias typedefs
        for ct in api.compositeTypes:
            for alias in ct.aliasList:
                yield "typedef %s %s;" % (ct.name, alias)
                yield ""

    writeInlFile(filename, INL_HEADER, gen())

def argListToStr (args):
    def argumentToString(arg):
        # args can be instance of FunctionArgument or CompositeMember
        # but CompositeMember has no arraySize atrribute nor secondPointerIsConst
        workingOnFunctionArgument = True if hasattr(arg, 'arraySize') else False
        result = ''
        if arg.qualifiers:
            result += arg.qualifiers
        result += arg.type
        if arg.pointer:
            if workingOnFunctionArgument and arg.secondPointerIsConst:
                result += '* const*'
            else:
                result += arg.pointer
        result += ' ' + arg.name
        if workingOnFunctionArgument:
            if arg.arraySize:
                result += arg.arraySize
        return result
    return ", ".join(argumentToString(arg) for arg in args)

def writeInterfaceDecl (api, filename, functionTypes, concrete):
    def genProtos ():
        postfix = "" if concrete else " = 0"
        for function in api.functions:
            if not function.getType() in functionTypes:
                continue
            yield "virtual %s\t%s\t(%s) const%s;" % (function.returnType, getInterfaceName(function.name), argListToStr(function.arguments), postfix)

    writeInlFile(filename, INL_HEADER, indentLines(genProtos()))

def writeFunctionPtrTypes (api, filename):
    def genTypes ():
        pattern = "typedef VKAPI_ATTR {}\t(VKAPI_CALL* {})\t({});"
        for function in api.functions:
            argList = argListToStr(function.arguments)
            yield pattern.format(function.returnType, getFunctionTypeName(function.name), argList)
            for alias in function.aliasList:
                yield pattern.format(function.returnType, getFunctionTypeName(alias), argList)

    writeInlFile(filename, INL_HEADER, indentLines(genTypes()))

def writeFunctionPointers (api, filename, functionTypes):
    def FunctionsYielder ():
        for function in api.functions:
            if function.getType() in functionTypes:
                interfaceName = getInterfaceName(function.name)
                functionTypeName = getFunctionTypeName(function.name)
                yield f"{functionTypeName}\t{interfaceName};"
                if function.getType() == Function.TYPE_INSTANCE:
                    for alias in function.aliasList:
                        interfaceName = getInterfaceName(alias)
                        functionTypeName = getFunctionTypeName(alias)
                        yield f"{functionTypeName}\t{interfaceName};"

    writeInlFile(filename, INL_HEADER, indentLines(FunctionsYielder()))

def getPromotedFunctions (api):
    apiNum = 0 if api.apiName == "vulkan" else 1
    promotedFunctions = collections.defaultdict(lambda: list())
    for feature in api.features:
        versionSplit = feature.name.split('_')
        apiMajor = int(versionSplit[-2])
        apiMinor = int(versionSplit[-1])
        apiPrefix = '_'.join(versionSplit[:-2])
        if apiNum == 0 and apiPrefix != 'VK_VERSION':
            continue
        if apiNum == 1 and apiPrefix == 'VK_VERSION':
            # Map of "standard" Vulkan versions to VulkanSC version.
            stdToSCMap = {
                (1, 0): (1, 0),
                (1, 1): (1, 0),
                (1, 2): (1, 0),
            }
            mapKey = (apiMajor, apiMinor)
            if mapKey not in stdToSCMap:
                continue
            (apiMajor, apiMinor) = stdToSCMap[mapKey]
        apituple = (apiNum, apiMajor, apiMinor)
        for featureRequirement in feature.requirementsList:
            for promotedFun in featureRequirement.commandList:
                promotedFunctions[promotedFun].append(apituple)
    return promotedFunctions

def writeInitFunctionPointers (api, filename, functionTypes, cond = None):
    promotedFunctions = getPromotedFunctions(api) if Function.TYPE_DEVICE in functionTypes else None
    def makeInitFunctionPointers ():
        for function in api.functions:
            if function.getType() in functionTypes and (cond == None or cond(function)):
                condition = ''
                if function.getType() == Function.TYPE_DEVICE:
                    versionCheck = ''
                    if function.name in promotedFunctions:
                        for versionTuple in promotedFunctions[function.name]:
                            if len(versionCheck) > 0:
                                versionCheck += ' || '
                            versionCheck = 'usedApiVersion >= VK_MAKE_API_VERSION(%s, %s, %s, 0)' % versionTuple
                    if len(versionCheck) > 0:
                        condition = f"if ({versionCheck})\n    "
                interfaceName = getInterfaceName(function.name)
                functionTypeName = getFunctionTypeName(function.name)
                yield f"{condition}m_vk.{interfaceName} = ({functionTypeName}) GET_PROC_ADDR(\"{function.name}\");"
                for alias in function.aliasList:
                    yield f"if (!m_vk.{interfaceName})"
                    yield f"    m_vk.{interfaceName} = ({functionTypeName}) GET_PROC_ADDR(\"{alias}\");"
                    if function.getType() == Function.TYPE_INSTANCE and function.arguments[0].type == "VkPhysicalDevice":
                        interfaceName = getInterfaceName(alias)
                        functionTypeName = getFunctionTypeName(alias)
                        yield f"m_vk.{interfaceName} = ({functionTypeName}) GET_PROC_ADDR(\"{alias}\");"

    lines = makeInitFunctionPointers()
    writeInlFile(filename, INL_HEADER, lines)

# List pre filled manually with commands forbidden for computation only implementations
computeOnlyForbiddenCommands = [
    "destroyRenderPass",
    "createRenderPass2",
    "createRenderPass",
    "createGraphicsPipelines"
]

computeOnlyRestrictedCommands = {
    "createComputePipelines"    : "\t\tfor (uint32_t i=0; i<createInfoCount; ++i)\n\t\t\tif ((pCreateInfos[i].stage.stage & VK_SHADER_STAGE_ALL_GRAPHICS) != 0) THROW_NOT_SUPPORTED_COMPUTE_ONLY();",
    "createBuffer"                : "\t\tif ((pCreateInfo->usage & ( VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT )) !=0) THROW_NOT_SUPPORTED_COMPUTE_ONLY();",
}

def writeFuncPtrInterfaceImpl (api, filename, functionTypes, className):

    # populate compute only forbidden commands
    for fun in api.functions:
        if "graphics" in fun.queuesList and "compute" not in fun.queuesList:
            # remove the 'vk' prefix and change the first character of the remaining string to lowercase
            commandName = fun.name[2:3].lower() + fun.name[3:]
            computeOnlyForbiddenCommands.append(commandName)

            # if the command has an alias, also add it
            for alias_name in fun.aliasList:
                alias_name_without_vk = alias_name[2:3].lower() + alias_name[3:]
                computeOnlyForbiddenCommands.append(alias_name_without_vk)

    def makeFuncPtrInterfaceImpl ():
        for function in api.functions:
            functionInterfaceName = getInterfaceName(function.name)
            if function.getType() in functionTypes:
                yield ""
                yield "%s %s::%s (%s) const" % (function.returnType, className, functionInterfaceName, argListToStr(function.arguments))
                yield "{"
                # Check for compute only forbidden commands
                if functionInterfaceName in computeOnlyForbiddenCommands:
                    yield "    if( m_computeOnlyMode ) THROW_NOT_SUPPORTED_COMPUTE_ONLY();"
                # Check for compute only restricted commands
                if functionInterfaceName in computeOnlyRestrictedCommands:
                    yield "\tif( m_computeOnlyMode )"
                    yield "\t{"
                    yield computeOnlyRestrictedCommands[functionInterfaceName]
                    yield "\t}"
                # Special case for vkEnumerateInstanceVersion
                if function.name == "vkEnumerateInstanceVersion":
                    yield "    if (m_vk.enumerateInstanceVersion)"
                    yield "        return m_vk.enumerateInstanceVersion(pApiVersion);"
                    yield ""
                    yield "    *pApiVersion = VK_API_VERSION_1_0;"
                    yield "    return VK_SUCCESS;"
                    yield "}"
                    continue
                # Simplify code by preparing string template needed in few code branches
                tab = ' ' * 4
                funReturn = "" if function.returnType == "void" else "return "
                funParams = ", ".join(a.name for a in function.arguments)
                callTemplate = f"{tab}{funReturn}m_vk.{{}}({funParams});"
                # Special case for all instance functions that operate on VkPhysicalDevice
                if function.getType() == Function.TYPE_INSTANCE and function.arguments[0].type == "VkPhysicalDevice":
                    # Helper function that checks if entry point was promoted to core
                    def isInCore(allFunAliases):
                        for feature in api.features:
                            if api.apiName not in feature.api.split(','):
                                continue
                            for r in feature.requirementsList:
                                for n in allFunAliases:
                                    if n in r.commandList:
                                        return (True, feature.number)
                        return (False, "1.0")
                    (inCore, coreNumber) = isInCore([function.name] + function.aliasList)
                    if inCore and "1.0" not in coreNumber:
                        callTemplate = f"{tab}{callTemplate}"
                        yield "    vk::VkPhysicalDeviceProperties props;"
                        yield "    m_vk.getPhysicalDeviceProperties(physicalDevice, &props);"
                        yield f"    if (props.apiVersion >= VK_API_VERSION_{coreNumber.replace('.', '_')})"
                        yield callTemplate.format(functionInterfaceName)
                        yield "    else"
                        yield callTemplate.format(getInterfaceName(function.aliasList[0]))
                        yield "}"
                        continue
                yield callTemplate.format(functionInterfaceName)
                yield "}"
    writeInlFile(filename, INL_HEADER, makeFuncPtrInterfaceImpl())

def writeFuncPtrInterfaceSCImpl (api, filename, functionTypes, className):
    normFuncs = {
        "createGraphicsPipelines"        : "\t\treturn createGraphicsPipelinesHandlerNorm(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);",
        "createComputePipelines"        : "\t\treturn createComputePipelinesHandlerNorm(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);",
        "createSampler"                    : "\t\treturn createSamplerHandlerNorm(device, pCreateInfo, pAllocator, pSampler);",
        "createSamplerYcbcrConversion"    : "\t\treturn createSamplerYcbcrConversionHandlerNorm(device, pCreateInfo, pAllocator, pYcbcrConversion);",
        "createDescriptorSetLayout"        : "\t\treturn createDescriptorSetLayoutHandlerNorm(device, pCreateInfo, pAllocator, pSetLayout);",
        "createPipelineLayout"            : "\t\treturn createPipelineLayoutHandlerNorm(device, pCreateInfo, pAllocator, pPipelineLayout);",
        "createRenderPass"                : "\t\treturn createRenderPassHandlerNorm(device, pCreateInfo, pAllocator, pRenderPass);",
        "createRenderPass2"                : "\t\treturn createRenderPass2HandlerNorm(device, pCreateInfo, pAllocator, pRenderPass);",
        "createCommandPool"                : "\t\treturn createCommandPoolHandlerNorm(device, pCreateInfo, pAllocator, pCommandPool);",
        "resetCommandPool"                : "\t\treturn resetCommandPoolHandlerNorm(device, commandPool, flags);",
        "createFramebuffer"                : "\t\treturn createFramebufferHandlerNorm(device, pCreateInfo, pAllocator, pFramebuffer);",
    }
    statFuncs = {
        "destroyDevice"                    : "\t\tdestroyDeviceHandler(device, pAllocator);",
        "createDescriptorSetLayout"        : "\t\tcreateDescriptorSetLayoutHandlerStat(device, pCreateInfo, pAllocator, pSetLayout);",
        "destroyDescriptorSetLayout"    : "\t\tdestroyDescriptorSetLayoutHandler(device, descriptorSetLayout, pAllocator);",
        "createImageView"                : "\t\tcreateImageViewHandler(device, pCreateInfo, pAllocator, pView);",
        "destroyImageView"                : "\t\tdestroyImageViewHandler(device, imageView, pAllocator);",
        "createSemaphore"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(semaphoreRequestCount,1);\n\t\t*pSemaphore = m_resourceInterface->incResourceCounter<VkSemaphore>();\n\t}",
        "destroySemaphore"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(semaphore,semaphoreRequestCount,1);\n\t}",
        "createFence"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(fenceRequestCount,1);\n\t\t*pFence = m_resourceInterface->incResourceCounter<VkFence>();\n\t}",
        "destroyFence"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(fence,fenceRequestCount,1);\n\t}",
        "allocateMemory"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(deviceMemoryRequestCount,1);\n\t\t*pMemory = m_resourceInterface->incResourceCounter<VkDeviceMemory>();\n\t}",
        "createBuffer"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(bufferRequestCount,1);\n\t\t*pBuffer = m_resourceInterface->incResourceCounter<VkBuffer>();\n\t}",
        "destroyBuffer"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(buffer,bufferRequestCount,1);\n\t}",
        "createImage"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(imageRequestCount,1);\n\t\t*pImage = m_resourceInterface->incResourceCounter<VkImage>();\n\t}",
        "destroyImage"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(image,imageRequestCount,1);\n\t}",
        "createEvent"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(eventRequestCount,1);\n\t\t*pEvent = m_resourceInterface->incResourceCounter<VkEvent>();\n\t}",
        "destroyEvent"                    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(event,eventRequestCount,1);\n\t}",
        "createQueryPool"                : "\t\tcreateQueryPoolHandler(device, pCreateInfo, pAllocator, pQueryPool);",
        "createBufferView"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(bufferViewRequestCount,1);\n\t\t*pView = m_resourceInterface->incResourceCounter<VkBufferView>();\n\t}",
        "destroyBufferView"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(bufferView,bufferViewRequestCount,1);\n\t}",
        "createPipelineLayout"            : "\t\tcreatePipelineLayoutHandlerStat(device, pCreateInfo, pAllocator, pPipelineLayout);",
        "destroyPipelineLayout"            : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(pipelineLayout,pipelineLayoutRequestCount,1);\n\t}",
        "createRenderPass"                : "\t\tcreateRenderPassHandlerStat(device, pCreateInfo, pAllocator, pRenderPass);",
        "createRenderPass2"                : "\t\tcreateRenderPass2HandlerStat(device, pCreateInfo, pAllocator, pRenderPass);",
        "destroyRenderPass"                : "\t\tdestroyRenderPassHandler(device, renderPass, pAllocator);",
        "createGraphicsPipelines"        : "\t\tcreateGraphicsPipelinesHandlerStat(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);",
        "createComputePipelines"        : "\t\tcreateComputePipelinesHandlerStat(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);",
        "destroyPipeline"                : "\t\tdestroyPipelineHandler(device, pipeline, pAllocator);",
        "createSampler"                    : "\t\tcreateSamplerHandlerStat(device, pCreateInfo, pAllocator, pSampler);",
        "destroySampler"                : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(sampler,samplerRequestCount,1);\n\t}",
        "createDescriptorPool"            : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(descriptorPoolRequestCount,1);\n\t\t*pDescriptorPool = m_resourceInterface->incResourceCounter<VkDescriptorPool>();\n\t}",
        "resetDescriptorPool"            : "\t\tresetDescriptorPoolHandlerStat(device, descriptorPool, flags);",
        "allocateDescriptorSets"        : "\t\tallocateDescriptorSetsHandlerStat(device, pAllocateInfo, pDescriptorSets);",
        "freeDescriptorSets"            : "\t\tfreeDescriptorSetsHandlerStat(device, descriptorPool, descriptorSetCount, pDescriptorSets);",
        "createFramebuffer"                : "\t\tcreateFramebufferHandlerStat(device, pCreateInfo, pAllocator, pFramebuffer);",
        "destroyFramebuffer"            : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(framebuffer,framebufferRequestCount,1);\n\t}",
        "createCommandPool"                : "\t\tcreateCommandPoolHandlerStat(device, pCreateInfo, pAllocator, pCommandPool);",
        "resetCommandPool"                : "\t\tresetCommandPoolHandlerStat(device, commandPool, flags);",
        "allocateCommandBuffers"        : "\t\tallocateCommandBuffersHandler(device, pAllocateInfo, pCommandBuffers);",
        "freeCommandBuffers"            : "\t\tfreeCommandBuffersHandler(device, commandPool, commandBufferCount, pCommandBuffers);",
        "createSamplerYcbcrConversion"    : "\t\tcreateSamplerYcbcrConversionHandlerStat(device, pCreateInfo, pAllocator, pYcbcrConversion);",
        "destroySamplerYcbcrConversion"    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(ycbcrConversion,samplerYcbcrConversionRequestCount,1);\n\t}",
        "getDescriptorSetLayoutSupport"    : "\t\tgetDescriptorSetLayoutSupportHandler(device, pCreateInfo, pSupport);",
#        "" : "surfaceRequestCount",
#        "" : "swapchainRequestCount",
#        "" : "displayModeRequestCount"
        "mapMemory"                        : "\t{\n\t\tDDSTAT_LOCK();\n\t\tif(m_falseMemory.size() < (static_cast<std::size_t>(offset+size)))\n\t\t\tm_falseMemory.resize(static_cast<std::size_t>(offset+size));\n\t\t*ppData = (void*)m_falseMemory.data();\n\t}",
        "getBufferMemoryRequirements"    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tpMemoryRequirements->size = 1048576U;\n\t\tpMemoryRequirements->alignment = 1U;\n\t\tpMemoryRequirements->memoryTypeBits = ~0U;\n\t}",
        "getImageMemoryRequirements"    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tpMemoryRequirements->size = 1048576U;\n\t\tpMemoryRequirements->alignment = 1U;\n\t\tpMemoryRequirements->memoryTypeBits = ~0U;\n\t}",
        "getBufferMemoryRequirements2"    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tpMemoryRequirements->memoryRequirements.size = 1048576U;\n\t\tpMemoryRequirements->memoryRequirements.alignment = 1U;\n\t\tpMemoryRequirements->memoryRequirements.memoryTypeBits = ~0U;\n\t}",
        "getImageMemoryRequirements2"    : "\t{\n\t\tDDSTAT_LOCK();\n\t\tpMemoryRequirements->memoryRequirements.size = 1048576U;\n\t\tpMemoryRequirements->memoryRequirements.alignment = 1U;\n\t\tpMemoryRequirements->memoryRequirements.memoryTypeBits = ~0U;\n\t}",
        "getImageSubresourceLayout"        : "\t{\n\t\tDDSTAT_LOCK();\n\t\tpLayout->offset = 0U;\n\t\tpLayout->size = 1048576U;\n\t\tpLayout->rowPitch = 0U;\n\t\tpLayout->arrayPitch = 0U;\n\t\tpLayout->depthPitch = 0U;\n\t}",
        "createPipelineCache"            : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_CREATE(pipelineCacheRequestCount,1);\n\t\t*pPipelineCache = m_resourceInterface->incResourceCounter<VkPipelineCache>();\n\t}",
        "destroyPipelineCache"            : "\t{\n\t\tDDSTAT_LOCK();\n\t\tDDSTAT_HANDLE_DESTROY_IF(pipelineCache,pipelineCacheRequestCount,1);\n\t}",
        "cmdUpdateBuffer"                : "\t\tincreaseCommandBufferSize(commandBuffer, dataSize);",
        "getDeviceQueue"                : "\t\tm_vk.getDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);",
    }

    statReturns = {
        "VkResult"            : "return VK_SUCCESS;",
        "VkDeviceAddress"    : "return 0u;",
        "uint64_t"            : "return 0u;",
    }

    def makeFuncPtrInterfaceStatisticsImpl ():
        for function in api.functions:
            if function.getType() in functionTypes:
                ifaceName = getInterfaceName(function.name)
                yield ""
                yield "%s %s::%s (%s) const" % (function.returnType, className, ifaceName, argListToStr(function.arguments))
                yield "{"
                # Check for compute only forbidden commands
                if ifaceName in computeOnlyForbiddenCommands:
                    yield "\tif( m_computeOnlyMode ) THROW_NOT_SUPPORTED_COMPUTE_ONLY();"
                # Check for compute only restricted commands
                if ifaceName in computeOnlyRestrictedCommands:
                    yield "\tif( m_computeOnlyMode )"
                    yield "\t{"
                    yield computeOnlyRestrictedCommands[ifaceName]
                    yield "\t}"
                if ( ifaceName in normFuncs ) or ( ifaceName in statFuncs ):
                    yield "\tstd::lock_guard<std::mutex> lock(functionMutex);"
                if ifaceName != "getDeviceProcAddr" :
                    yield "\tif (m_normalMode)"
                if ifaceName in normFuncs :
                    yield "%s" % ( normFuncs[ifaceName] )
                else:
                    yield "\t\t%sm_vk.%s(%s);" % ("return " if function.returnType != "void" else "", ifaceName, ", ".join(a.name for a in function.arguments))
                if ifaceName in statFuncs :
                    yield "\telse"
                    yield "%s" % ( statFuncs[ifaceName] )
                elif ifaceName[:3] == "cmd" :
                    yield "\telse"
                    yield "\t\tincreaseCommandBufferSize(commandBuffer, 0u);"
                if function.returnType in statReturns:
                    yield "\t%s" % ( statReturns[function.returnType] )
                yield "}"

    writeInlFile(filename, INL_HEADER, makeFuncPtrInterfaceStatisticsImpl())

def writeStrUtilProto (api, filename):
    def makeStrUtilProto ():
        for line in indentLines(["const char*\tget%sName\t(%s value);" % (enum.name[2:], enum.name) for enum in api.enums if enum.type == "enum"]):
            yield line
        yield ""
        for line in indentLines(["inline tcu::Format::Enum<%s>\tget%sStr\t(%s value)\t{ return tcu::Format::Enum<%s>(get%sName, value);\t}" % (e.name, e.name[2:], e.name, e.name, e.name[2:]) for e in api.enums if e.type == "enum"]):
            yield line
        yield ""
        for line in indentLines(["inline std::ostream&\toperator<<\t(std::ostream& s, %s value)\t{ return s << get%sStr(value);\t}" % (e.name, e.name[2:]) for e in api.enums if e.type == "enum"]):
            yield line
        yield ""
        for line in indentLines(["tcu::Format::Bitfield<%s>\tget%sStr\t(%s value);" % (("64" if b.type == "VkFlags64" else "32"), b.name[2:], b.name) for b in api.bitmasks]):
            yield line
        yield ""
        for line in indentLines(["std::ostream&\toperator<<\t(std::ostream& s, const %s& value);" % (s.name) for s in api.compositeTypes]):
            yield line

    writeInlFile(filename, INL_HEADER, makeStrUtilProto())

def writeStrUtilImpl (api, filename):
    def makeStrUtilImpl ():
        for line in indentLines(["template<> const char*\tgetTypeName<%s>\t(void) { return \"%s\";\t}" % (handle.name, handle.name) for handle in api.handles]):
            yield line

        yield ""
        yield "namespace %s" % PLATFORM_TYPE_NAMESPACE
        yield "{"

        for line in indentLines("std::ostream& operator<< (std::ostream& s, %s\tv) { return s << tcu::toHex(v.internal); }" % ''.join(s) for n, s, c in PLATFORM_TYPES):
            yield line

        yield "}"

        savedBitmasks = []
        for enum in api.enums:
            if enum.type == "enum":
                yield ""
                yield "const char* get%sName (%s value)" % (enum.name[2:], enum.name)
                yield "{"
                yield "\tswitch (value)"
                yield "\t{"
                enumValues = []
                lastValue = 0x7FFFFFFF
                for e in enum.enumeratorList:
                    enumValues.append(f"\t\tcase {e.name}:\treturn \"{e.name}\";")
                enumValues.append("\t\tdefault:\treturn nullptr;")
                for line in indentLines(enumValues):
                    yield line
                yield "\t}"
                yield "}"
            elif enum.type == "bitmask":
                # find bitfield that uses those bitmasks
                foundBitmask = None
                for bitmask in api.bitmasks:
                    if bitmask.requires == enum.name or bitmask.bitvalues == enum.name:
                        foundBitmask = bitmask
                        break
                if foundBitmask == None:
                    continue
                savedBitmasks.append(foundBitmask.name)
                bitSize = "64" if foundBitmask.type == "VkFlags64" else "32"
                yield ""
                yield f"tcu::Format::Bitfield<{bitSize}> get{bitmask.name[2:]}Str ({bitmask.name} value)"
                yield "{"
                yield "\tstatic const tcu::Format::BitDesc s_desc[] ="
                yield "\t{"
                if len(enum.enumeratorList) == 0:
                    # some bitfields in SC have no items
                    yield f"\t\ttcu::Format::BitDesc(0, \"0\")"
                else:
                    for line in indentLines([f"\t\ttcu::Format::BitDesc({e.name},\t\"{e.name}\")," for e in enum.enumeratorList]):
                        yield line
                yield "\t};"
                yield f"\treturn tcu::Format::Bitfield<{bitSize}>(value, DE_ARRAY_BEGIN(s_desc), DE_ARRAY_END(s_desc));"
                yield "}"

        for bitmask in api.bitmasks:
            if bitmask.name not in savedBitmasks:
                bitSize = "64" if bitmask.type == "VkFlags64" else "32"
                yield ""
                yield f"tcu::Format::Bitfield<{bitSize}> get{bitmask.name[2:]}Str ({bitmask.name} value)"
                yield "{"
                yield f"\treturn tcu::Format::Bitfield<{bitSize}>(value, nullptr, nullptr);"
                yield "}"

        bitfieldTypeNames = set([bitmask.name for bitmask in api.bitmasks])

        for type in api.compositeTypes:
            yield ""
            yield "std::ostream& operator<< (std::ostream& s, const %s& value)" % type.name
            yield "{"
            yield "\ts << \"%s = {\\n\";" % type.name
            for member in type.members:
                memberName = member.name
                valFmt = None
                newLine = ""
                if member.type in bitfieldTypeNames:
                    operator = '*' if member.pointer == '*' else ''
                    valFmt = "get%sStr(%svalue.%s)" % (member.type[2:], operator, member.name)
                elif member.type == "char" and member.pointer == '*':
                    valFmt = "getCharPtrStr(value.%s)" % member.name
                elif member.type == PLATFORM_TYPE_NAMESPACE + "::Win32LPCWSTR":
                    valFmt = "getWStr(value.%s)" % member.name
                elif len(member.arraySizeList) == 1:
                    if member.name in ["extensionName", "deviceName", "layerName", "description"]:
                        valFmt = "(const char*)value.%s" % member.name
                    elif member.type == 'char' or member.type == 'uint8_t':
                        newLine = "'\\n' << "
                        valFmt = "tcu::formatArray(tcu::Format::HexIterator<%s>(DE_ARRAY_BEGIN(value.%s)), tcu::Format::HexIterator<%s>(DE_ARRAY_END(value.%s)))" % (member.type, member.name, member.type, member.name)
                    else:
                        if member.name == "memoryTypes" or member.name == "memoryHeaps":
                            endIter = "DE_ARRAY_BEGIN(value.%s) + value.%sCount" % (member.name, member.name[:-1])
                        else:
                            endIter = "DE_ARRAY_END(value.%s)" % member.name
                        newLine = "'\\n' << "
                        valFmt = "tcu::formatArray(DE_ARRAY_BEGIN(value.%s), %s)" % (member.name, endIter)
                    memberName = member.name
                elif len(member.arraySizeList) > 1:
                    yield f"\ts << \"\\t{member.name} = \" << '\\n';"
                    dim = 0
                    index = ''
                    dimensionCount = len(member.arraySizeList)
                    while dim < dimensionCount-1:
                        yield f"\tfor(uint32_t i{dim} = 0 ; i{dim} < {member.arraySizeList[dim]} ; ++i{dim})"
                        index += f"[i{dim}]"
                        dim +=1
                    yield f"\t\ts << tcu::formatArray(DE_ARRAY_BEGIN(value.{member.name}{index}), DE_ARRAY_END(value.{member.name}{index})) << '\\n';"
                    # move to next member
                    continue
                else:
                    valFmt = "value.%s" % member.name
                yield ("\ts << \"\\t%s = \" << " % memberName) + newLine + valFmt + " << '\\n';"
            yield "\ts << '}';"
            yield "\treturn s;"
            yield "}"
    writeInlFile(filename, INL_HEADER, makeStrUtilImpl())

def writeObjTypeImpl (api, filename):
    def makeObjTypeImpl ():

        yield "namespace vk"
        yield "{"

        yield "template<typename T> VkObjectType getObjectType    (void);"

        for line in indentLines(["template<> inline VkObjectType\tgetObjectType<%s>\t(void) { return %s;\t}" % (handle.name, prefixName("VK_OBJECT_TYPE_", handle.name)) for handle in api.handles]):
            yield line

        yield "}"

    writeInlFile(filename, INL_HEADER, makeObjTypeImpl())

class ConstructorFunction:
    def __init__ (self, type, name, objectType, ifaceArgs, arguments):
        self.type = type
        self.name = name
        self.objectType = objectType
        self.ifaceArgs = ifaceArgs
        self.arguments = arguments

def getConstructorFunctions (api):
    funcs = []

    ifacesDict = {
        Function.TYPE_PLATFORM: [FunctionArgument("vk", "const ", "PlatformInterface&")],
        Function.TYPE_INSTANCE: [FunctionArgument("vk", "const ", "InstanceInterface&")],
        Function.TYPE_DEVICE: [FunctionArgument("vk", "const ", "DeviceInterface&")]}

    for function in api.functions:
        if (function.name[:8] == "vkCreate" or function.name == "vkAllocateMemory") and not "createInfoCount" in [a.name for a in function.arguments]:
            if function.name in ["vkCreatePipelineBinariesKHR", "vkCreateDisplayModeKHR"]:
                continue # No way to delete display modes (bug?)

            ifaceArgs = []
            if function.name == "vkCreateDevice":
                ifaceArgs = [FunctionArgument("vkp", "const ", "PlatformInterface&"),
                             FunctionArgument("instance", "", "VkInstance")]
            ifaceArgs.extend(ifacesDict[function.getType()])

            allocatorArg = function.arguments[-2]
            assert (allocatorArg.type == "VkAllocationCallbacks" and \
                    "const" in allocatorArg.qualifiers and \
                    allocatorArg.pointer == "*")

            objectType = function.arguments[-1].type
            arguments = function.arguments[:-1]
            funcs.append(ConstructorFunction(function.getType(), getInterfaceName(function.name), objectType, ifaceArgs, arguments))
    return funcs

def addVersionDefines(versionSpectrum):
    output = ["#define " + ver.getDefineName() + " " + ver.getInHex() for ver in versionSpectrum if not ver.isStandardVersion()]
    return output

def writeRefUtilProto (api, filename):
    functions = getConstructorFunctions(api)

    def makeRefUtilProto ():
        unindented = []
        for line in indentLines(["Move<%s>\t%s\t(%s = nullptr);" % (function.objectType, function.name, argListToStr(function.ifaceArgs + function.arguments)) for function in functions]):
            yield line

    writeInlFile(filename, INL_HEADER, makeRefUtilProto())

def writeRefUtilImpl (api, filename):
    functions = getConstructorFunctions(api)

    def makeRefUtilImpl ():
        yield "namespace refdetails"
        yield "{"
        yield ""

        for function in api.functions:
            if function.getType() == Function.TYPE_DEVICE \
            and (function.name[:9] == "vkDestroy" or function.name == "vkFreeMemory") \
            and not function.name == "vkDestroyDevice":
                objectType = function.arguments[-2].type
                yield "template<>"
                yield "void Deleter<%s>::operator() (%s obj) const" % (objectType, objectType)
                yield "{"
                yield "\tm_deviceIface->%s(m_device, obj, m_allocator);" % (getInterfaceName(function.name))
                yield "}"
                yield ""

        yield "} // refdetails"
        yield ""

        dtorDict = {
            Function.TYPE_PLATFORM: "object",
            Function.TYPE_INSTANCE: "instance",
            Function.TYPE_DEVICE: "device"
        }

        for function in functions:
            deleterArgsString = ''
            if function.name == "createDevice":
                # createDevice requires two additional parameters to setup VkDevice deleter
                deleterArgsString = "vkp, instance, object, " +  function.arguments[-1].name
            else:
                deleterArgsString = "vk, %s, %s" % (dtorDict[function.type], function.arguments[-1].name)

            yield "Move<%s> %s (%s)" % (function.objectType, function.name, argListToStr(function.ifaceArgs + function.arguments))
            yield "{"
            yield "\t%s object = VK_NULL_HANDLE;" % function.objectType
            yield "\tVK_CHECK(vk.%s(%s));" % (function.name, ", ".join([a.name for a in function.arguments] + ["&object"]))
            yield "\treturn Move<%s>(check<%s>(object), Deleter<%s>(%s));" % (function.objectType, function.objectType, function.objectType, deleterArgsString)
            yield "}"
            yield ""

    writeInlFile(filename, INL_HEADER, makeRefUtilImpl())

def writeStructTraitsImpl (api, filename):
    def gen ():
        for cType in api.compositeTypes:
            if cType.category == "struct" and cType.members[0].name == "sType" and cType.name != "VkBaseOutStructure" and cType.name != "VkBaseInStructure":
                yield "template<> VkStructureType getStructureType<%s> (void)" % cType.name
                yield "{"
                yield "\treturn %s;" % cType.members[0].values
                yield "}"
                yield ""

    writeInlFile(filename, INL_HEADER, gen())

def writeNullDriverImpl (api, filename):
    def genNullDriverImpl ():
        specialFuncNames = [
                "vkCreateGraphicsPipelines",
                "vkCreateComputePipelines",
                "vkCreateRayTracingPipelinesNV",
                "vkCreateRayTracingPipelinesKHR",
                "vkGetInstanceProcAddr",
                "vkGetDeviceProcAddr",
                "vkEnumeratePhysicalDevices",
                "vkEnumerateInstanceExtensionProperties",
                "vkEnumerateDeviceExtensionProperties",
                "vkGetPhysicalDeviceFeatures",
                "vkGetPhysicalDeviceFeatures2KHR",
                "vkGetPhysicalDeviceProperties",
                "vkGetPhysicalDeviceProperties2KHR",
                "vkGetPhysicalDeviceQueueFamilyProperties",
                "vkGetPhysicalDeviceMemoryProperties",
                "vkGetPhysicalDeviceFormatProperties",
                "vkGetPhysicalDeviceImageFormatProperties",
                "vkGetDeviceQueue",
                "vkGetBufferMemoryRequirements",
                "vkGetBufferMemoryRequirements2KHR",
                "vkGetImageMemoryRequirements",
                "vkGetImageMemoryRequirements2KHR",
                "vkAllocateMemory",
                "vkMapMemory",
                "vkUnmapMemory",
                "vkAllocateDescriptorSets",
                "vkFreeDescriptorSets",
                "vkResetDescriptorPool",
                "vkAllocateCommandBuffers",
                "vkFreeCommandBuffers",
                "vkCreateDisplayModeKHR",
                "vkCreateSharedSwapchainsKHR",
                "vkGetPhysicalDeviceExternalBufferPropertiesKHR",
                "vkGetPhysicalDeviceImageFormatProperties2KHR",
                "vkGetMemoryAndroidHardwareBufferANDROID",
                "vkCreateShadersEXT",
            ]

        specialFuncs = [f for f in api.functions if f.name in specialFuncNames]
        createFuncs = [f for f in api.functions if (f.name[:8] == "vkCreate" or f.name == "vkAllocateMemory") and not f in specialFuncs]
        destroyFuncs = [f for f in api.functions if (f.name[:9] == "vkDestroy" or f.name == "vkFreeMemory") and not f in specialFuncs]
        dummyFuncs = [f for f in api.functions if f not in specialFuncs + createFuncs + destroyFuncs]

        def getHandle (name):
            for handle in api.handles:
                if handle.name == name:
                    return handle
            raise Exception("No such handle: %s" % name)

        for function in createFuncs:
            objectType = function.arguments[-1].type
            argsStr = ", ".join([a.name for a in function.arguments[:-1]])

            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.arguments))
            yield "{"
            yield "\tDE_UNREF(%s);" % function.arguments[-2].name

            if function.arguments[-1].len != None:
                yield "\tVK_NULL_RETURN((allocateNonDispHandleArray<%s, %s>(%s, %s)));" % (objectType[2:], objectType, argsStr, function.arguments[-1].name)
            else:
                if function.name == "vkCreatePipelineBinariesKHR":
                    yield "\tDE_UNREF(device);"
                    yield "\tDE_UNREF(pCreateInfo);"
                    yield "\tDE_UNREF(pAllocator);"
                    yield "\tDE_UNREF(pBinaries);"
                    yield "\treturn VK_SUCCESS;"
                elif getHandle(objectType).type == "VK_DEFINE_NON_DISPATCHABLE_HANDLE":
                    yield "\tVK_NULL_RETURN((*%s = allocateNonDispHandle<%s, %s>(%s)));" % (function.arguments[-1].name, objectType[2:], objectType, argsStr)
                else:
                    yield "\tVK_NULL_RETURN((*%s = allocateHandle<%s, %s>(%s)));" % (function.arguments[-1].name, objectType[2:], objectType, argsStr)
            yield "}"
            yield ""

        for function in destroyFuncs:
            objectArg = function.arguments[-2]

            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.arguments))
            yield "{"
            for arg in function.arguments[:-2]:
                yield "\tDE_UNREF(%s);" % arg.name

            if getHandle(objectArg.type).type == 'VK_DEFINE_NON_DISPATCHABLE_HANDLE':
                yield "\tfreeNonDispHandle<%s, %s>(%s, %s);" % (objectArg.type[2:], objectArg.type, objectArg.name, function.arguments[-1].name)
            else:
                yield "\tfreeHandle<%s, %s>(%s, %s);" % (objectArg.type[2:], objectArg.type, objectArg.name, function.arguments[-1].name)

            yield "}"
            yield ""

        for function in dummyFuncs:
            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.arguments))
            yield "{"
            for arg in function.arguments:
                yield "\tDE_UNREF(%s);" % arg.name
            if function.returnType != "void":
                yield "\treturn VK_SUCCESS;"
            yield "}"
            yield ""

        def genFuncEntryTable (type, name):

            entries = []
            pattern = "\tVK_NULL_FUNC_ENTRY(%s,\t%s),"
            for f in api.functions:
                if f.getType() != type:
                    continue
                entries.append(pattern % (f.name, getInterfaceName(f.name)))

            yield "static const tcu::StaticFunctionLibrary::Entry %s[] =" % name
            yield "{"

            for line in indentLines(entries):
                yield line
            yield "};"
            yield ""

        # Func tables
        for line in genFuncEntryTable(Function.TYPE_PLATFORM, "s_platformFunctions"):
            yield line

        for line in genFuncEntryTable(Function.TYPE_INSTANCE, "s_instanceFunctions"):
            yield line

        for line in genFuncEntryTable(Function.TYPE_DEVICE, "s_deviceFunctions"):
            yield line

    writeInlFile(filename, INL_HEADER, genNullDriverImpl())

def writeTypeUtil (api, filename):
    # Structs filled by API queries are not often used in test code
    QUERY_RESULT_TYPES = set([
            "VkPhysicalDeviceFeatures",
            "VkPhysicalDeviceLimits",
            "VkFormatProperties",
            "VkImageFormatProperties",
            "VkPhysicalDeviceSparseProperties",
            "VkQueueFamilyProperties",
            "VkMemoryType",
            "VkMemoryHeap",
            "StdVideoH264SpsVuiFlags",
            "StdVideoH264SpsFlags",
            "StdVideoH264PpsFlags",
            "StdVideoDecodeH264PictureInfoFlags",
            "StdVideoDecodeH264ReferenceInfoFlags",
            "StdVideoEncodeH264SliceHeaderFlags",
            "StdVideoEncodeH264PictureInfoFlags",
            "StdVideoEncodeH264ReferenceInfoFlags",
            "StdVideoEncodeH264ReferenceInfoFlags",
            "StdVideoH265HrdFlags",
            "StdVideoH265VpsFlags",
            "StdVideoH265SpsVuiFlags",
            "StdVideoH265SpsFlags",
            "StdVideoH265PpsFlags",
            "StdVideoDecodeH265PictureInfoFlags",
            "StdVideoDecodeH265ReferenceInfoFlags",
            "StdVideoEncodeH265PictureInfoFlags",
            "StdVideoEncodeH265ReferenceInfoFlags",
            "StdVideoEncodeH265SliceSegmentHeaderFlags",
            "StdVideoH265ProfileTierLevelFlags",
            "StdVideoH265ShortTermRefPicSetFlags",
            "StdVideoEncodeH264ReferenceListsInfoFlags",
            "StdVideoEncodeH265ReferenceListsInfoFlags",
            "StdVideoEncodeAV1OperatingPointInfoFlags",
            "StdVideoEncodeAV1PictureInfoFlags",
            "StdVideoEncodeAV1ReferenceInfoFlags",
            "StdVideoVP9ColorConfigFlags",
            "StdVideoVP9LoopFilterFlags",
            "StdVideoVP9SegmentationFlags",
            "StdVideoDecodeVP9PictureInfoFlags",
            "VkClusterAccelerationStructureGeometryIndexAndGeometryFlagsNV",
            "VkClusterAccelerationStructureBuildTriangleClusterInfoNV",
        ])

    def isSimpleStruct (type):
        def hasArrayMember (type):
            for member in type.members:
                if len(member.arraySizeList) > 0:
                    return True
            return False

        def hasBitField (type):
            for member in type.members:
                if member.fieldWidth:
                    return True
            return False

        def hasCompositeMember (type):
            for member in type.members:
                if member.pointer is not None and '*' not in member.pointer:
                    match = [c for c in api.compositeTypes if member.type == c.name]
                    if len(match) > 0:
                        return True
            return False

        return type.category == "struct" and \
        type.members[0].type != "VkStructureType" and \
        not type.name in QUERY_RESULT_TYPES and \
        not hasArrayMember(type) and \
        not hasCompositeMember(type) and \
        not hasBitField(type)

    def gen ():
        for type in api.compositeTypes:
            if not isSimpleStruct(type):
                continue

            if "StdVideoAV1" in type.name or "StdVideoDecodeAV1" in type.name:
                continue

            name = type.name[2:] if type.name[:2].lower() == "vk" else type.name

            yield ""
            yield "inline %s make%s (%s)" % (type.name, name, argListToStr(type.members))
            yield "{"
            yield "\t%s res;" % type.name
            for line in indentLines(["\tres.%s\t= %s;" % (m.name, m.name) for m in type.members]):
                yield line
            yield "\treturn res;"
            yield "}"

    writeInlFile(filename, INL_HEADER, gen())

def writeDriverIds(api, filename):
    driverIdsString = []
    driverIdsString.append("static const struct\n"
                     "{\n"
                     "\tstd::string driver;\n"
                     "\tuint32_t id;\n"
                     "} driverIds [] =\n"
                     "{")
    driverItems = dict()
    driverIdEnum = [enum for enum in api.enums if enum.name == 'VkDriverId'][0]
    for enumerator in driverIdEnum.enumeratorList:
        driverIdsString.append(f"\t{{\"{enumerator.name}\", {enumerator.value}}},")
        driverItems[enumerator.name] = enumerator.value
    for enumerator in driverIdEnum.enumeratorList:
        if len(enumerator.aliasList) > 0:
            driverIdsString.append(f"\t{{\"{enumerator.aliasList[0]}\", {enumerator.value}}},\t// {enumerator.name}")
    driverIdsString.append("\t{\"VK_DRIVER_ID_MAX_ENUM\", 0x7FFFFFFF}")
    driverIdsString.append("};")

    writeInlFile(filename, INL_HEADER, driverIdsString)

def writeSupportedExtensions(api, filename):

    def writeExtensionsForVersions(map):
        result = []
        for version in map:
            result.append("    if (coreVersion >= " + str(version) + ")")
            result.append("    {")
            for extension in sorted(map[version], key=lambda e: e.name):
                result.append('        dst.push_back("' + extension.name + '");')
            result.append("    }")

        if not map:
            result.append("    DE_UNREF(coreVersion);")
            result.append("    DE_UNREF(dst);")

        return result

    isSC = api.apiName == 'vulkansc'
    instanceMap = {}
    deviceMap = {}

    allExtensionNames = {e.name for e in api.extensions}
    for ext in api.extensions:
        if ext.promotedto is None or "VK_VERSION" not in ext.promotedto:
            continue
        # skip partialy promoted extensions
        if ext.partiallyPromoted is True:
            continue
        major = int(ext.promotedto[-3])
        minor = int(ext.promotedto[-1])
        currVersion = "VK_API_VERSION_" + ext.promotedto[-3:]
        # VulkanSC is based on Vulkan 1.2. Any Vulkan version greater than 1.2 should be excluded
        if isSC and major==1 and minor>2:
            continue
        if ext.type == 'instance':
            list = instanceMap.get(currVersion)
            instanceMap[currVersion] = list + [ext] if list else [ext]
        else:
            list = deviceMap.get(currVersion)
            deviceMap[currVersion] = list + [ext] if list else [ext]

    # add list of extensions missing in Vulkan SC specification
    if isSC:
        for extensionName, data in api.additionalExtensionData:
            if extensionName in allExtensionNames:
                logging.debug("Skipping additional extension " + extensionName + " because already added")
                continue
            # make sure that this extension was registered
            if 'register_extension' not in data.keys():
                logging.debug("Skipping unregistered extension " + extensionName)
                continue
            # save array containing 'device' or 'instance' string followed by the optional vulkan version in which this extension is core;
            # note that register_extension section is also required for partially promoted extensions like VK_EXT_extended_dynamic_state2
            # but those extensions should not fill 'core' tag
            match = re.match(r"(\d).(\d).(\d).(\d)", data['register_extension']['core'])
            if match is None:
                logging.debug("Skipping extension that is not matching core " + extensionName)
                continue
            major = int(match.group(2))
            minor = int(match.group(3))
            if major==1 and minor>2:
                continue
            currVersion = f"VK_API_VERSION_{major}_{minor}"
            ext = Extension(extensionName, 0, 0, 0, 0, 0, 0, 0, 0)
            if data['register_extension']['type'] == 'instance':
                list = instanceMap.get(currVersion)
                instanceMap[currVersion] = list + [ext] if list else [ext]
            else:
                list = deviceMap.get(currVersion)
                deviceMap[currVersion] = list + [ext] if list else [ext]

    lines = [
    "",
    "void getCoreDeviceExtensionsImpl (uint32_t coreVersion, ::std::vector<const char*>&%s)" % (" dst" if len(deviceMap) != 0 or isSC else ""),
    "{"] + writeExtensionsForVersions(deviceMap) + [
    "}",
    "",
    "void getCoreInstanceExtensionsImpl (uint32_t coreVersion, ::std::vector<const char*>&%s)" % (" dst" if len(instanceMap) != 0 or isSC else ""),
    "{"] + writeExtensionsForVersions(instanceMap) + [
    "}",
    ""]
    writeInlFile(filename, INL_HEADER, lines)


def writeExtensionFunctions (api, filename):

    def writeExtensionNameArrays ():
        instanceExtensionNames = [f"\t\"{ext.name}\"," for ext in api.extensions if ext.type == "instance"]
        deviceExtensionNames = [f"\t\"{ext.name}\"," for ext in api.extensions if ext.type == "device"]
        yield '::std::string instanceExtensionNames[] =\n{'
        for instanceExtName in instanceExtensionNames:
            yield instanceExtName
        yield '};\n'
        yield '::std::string deviceExtensionNames[] =\n{'
        for deviceExtName in deviceExtensionNames:
            yield deviceExtName
        yield '};'

    def writeExtensionFunctions (functionType):
        isFirstWrite = True
        dg_list = []    # Device groups functions need special casing, as Vulkan 1.0 keeps them in VK_KHR_device_groups whereas 1.1 moved them into VK_KHR_swapchain
        if functionType == Function.TYPE_INSTANCE:
            yield 'void getInstanceExtensionFunctions (uint32_t apiVersion, const std::vector<std::string> vIEP, const std::vector<std::string> vDEP, const std::string extName, ::std::vector<const char*>& functions)\n{'
            yield '\t(void)vIEP;\n(void)vDEP;'
            dg_list = ["vkGetPhysicalDevicePresentRectanglesKHR"]
        elif functionType == Function.TYPE_DEVICE:
            yield 'void getDeviceExtensionFunctions (uint32_t apiVersion, const std::vector<std::string> vIEP, const std::vector<std::string> vDEP, const std::string extName, ::std::vector<const char*>& functions)\n{'
            yield '\t(void)vIEP;\n(void)vDEP;'
            dg_list = ["vkGetDeviceGroupPresentCapabilitiesKHR", "vkGetDeviceGroupSurfacePresentModesKHR", "vkAcquireNextImage2KHR"]
        for ext in api.extensions:
            parsedRequirements = []
            for requirement in ext.requirementsList:
                funcNames = []
                for requiredCommand in requirement.newCommands:
                    commandName = requiredCommand.name
                    # find function that has specified name
                    func = None
                    funcList = [f for f in api.functions if f.name == commandName]
                    # if name was not found check if this is alias
                    if len(funcList) == 0:
                        for f in api.functions:
                            for aliasName in f.aliasList:
                                if aliasName == commandName:
                                    func = f
                                    break
                            if func:
                                break
                    else:
                        func = funcList[0]
                    if func == None:
                        if api.apiName == "vulkansc":
                            continue
                        # something went wrong, for "vulkan" func should always be found
                        logging.error("%s in %s not valid" % (commandName, ext.name))
                        assert(False)
                    if func.getType() == functionType:
                        funcNames.append(commandName)
                condition = None
                if requirement.depends is not None:
                    try:
                        condition = transformDependsToCondition(requirement.depends, api, 'checkVersion(%s, %s, apiVersion)', 'extensionIsSupported(%s, "%s")', ext.name)
                    except Exception as e:
                        if api.apiName != "vulkansc":
                            raise e
                parsedRequirements.append((requirement.depends, condition, funcNames))
            if ext.name:
                yield '\tif (extName == "%s")' % ext.name
                yield '\t{'
                for depends, condition, funcNames in parsedRequirements:
                    if len(funcNames) == 0:
                        continue
                    indent = '\t\t'
                    if depends is not None:
                        yield '\t\t// Dependencies: %s' % depends
                        yield '\t\tif (%s) {' % condition
                        indent = '\t\t\t'
                    for funcName in funcNames:
                        if funcName in dg_list:
                            yield '%sif(apiVersion >= VK_API_VERSION_1_1) functions.push_back("%s");' % (indent, funcName)
                        else:
                            yield '%sfunctions.push_back("%s");' % (indent, funcName)
                    if depends is not None:
                        yield '\t\t}'
                if ext.name == "VK_KHR_device_group":
                    for dg_func in dg_list:
                        yield '\t\tif(apiVersion < VK_API_VERSION_1_1) functions.push_back("%s");' % dg_func
                yield '\t\treturn;'
                yield '\t}'
                isFirstWrite = False
        if not isFirstWrite:
            yield '\tDE_FATAL("Extension name not found");'
            yield '}'

    def genHelperFunctions():
        yield 'bool checkVersion(uint32_t major, uint32_t minor, const uint32_t testedApiVersion)'
        yield '{'
        yield '\tuint32_t testedMajor = VK_API_VERSION_MAJOR(testedApiVersion);'
        yield '\tuint32_t testedMinor = VK_API_VERSION_MINOR(testedApiVersion);'
        yield '\t// return true when tested api version is greater'
        yield '\t// or equal to version represented by two uints'
        yield '\tif (major == testedMajor)'
        yield '\t\treturn minor <= testedMinor;'
        yield '\treturn major < testedMajor;'
        yield '}\n'
        yield 'bool extensionIsSupported(const std::vector<std::string> extNames, const std::string& ext)'
        yield '{'
        yield '\tfor (const std::string& supportedExtension : extNames)'
        yield '\t{'
        yield '\t\tif (supportedExtension == ext) return true;'
        yield '\t}'
        yield '\treturn false;'
        yield '}\n'

    lines = ['']
    lines.extend(genHelperFunctions())
    for line in writeExtensionFunctions(Function.TYPE_INSTANCE):
        lines += [line]
    lines += ['']
    for line in writeExtensionFunctions(Function.TYPE_DEVICE):
        lines += [line]
    lines += ['']
    for line in writeExtensionNameArrays():
        lines += [line]

    writeInlFile(filename, INL_HEADER, lines)

def writeCoreFunctionalities(api, filename):
    functionOriginValues = ["FUNCTIONORIGIN_PLATFORM", "FUNCTIONORIGIN_INSTANCE", "FUNCTIONORIGIN_DEVICE"]

    functionNamesPerApiVersionDict = {}
    for feature in api.features:
        if api.apiName not in feature.api.split(','):
            continue
        apiVersion = "VK_API_VERSION_" + feature.number.replace('.', '_')
        functionNamesPerApiVersionDict[apiVersion] = []
        for r in feature.requirementsList:
            # skip optional promotions like for VK_EXT_host_image_copy
            if float(feature.number) > 1.35 and r.comment is not None and 'Promoted from ' not in r.comment:
                continue
            functionNamesPerApiVersionDict[apiVersion].extend(r.commandList)

    lines = [
    "",
    'enum FunctionOrigin', '{'] + [line for line in indentLines([
    '\t' + functionOriginValues[0] + '\t= 0,',
    '\t' + functionOriginValues[1] + ',',
    '\t' + functionOriginValues[2]])] + [
    "};",
    "",
    "typedef ::std::pair<const char*, FunctionOrigin> FunctionInfo;",
    "typedef ::std::vector<FunctionInfo> FunctionInfosList;",
    "typedef ::std::map<uint32_t, FunctionInfosList> ApisMap;",
    "",
    "void initApisMap (ApisMap& apis)",
    "{",
    "    apis.clear();"] + [
    "    apis.insert(::std::pair<uint32_t, FunctionInfosList>(" + v + ", FunctionInfosList()));" for v in functionNamesPerApiVersionDict] + [
    ""]

    apiVersions = []
    functionLines = []
    for apiVersion in functionNamesPerApiVersionDict:
        lines += [f'\tapis[{apiVersion}] = {{']
        # iterate over names of functions added with api
        for functionName in functionNamesPerApiVersionDict[apiVersion]:
            # search for data of this function in all functions list
            functionData = None
            for f in api.functions:
                if functionName == f.name or functionName in f.aliasList:
                    functionData = f
                    break
            if functionData == None:
                if api.apiName == "vulkansc":
                    continue
                # something went wrong, for "vulkan" functionData should always be found
                assert(False)
            # add line corresponding to this function
            functionLines.append('\t\t{"' + functionName + '",\t' + functionOriginValues[functionData.getType()] + '},')
        # indent all functions of specified api and add them to main list
        lines = lines + [line for line in indentLines(functionLines)] + ["\t};"]

    lines = lines + ["}"]
    writeInlFile(filename, INL_HEADER, lines)

def camelToSnake(name):
    name = re.sub('([a-z])([23])D([A-Z])', r'\1_\2d\3', name)
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()

def writeDeviceFeatures2(api, filename):

    def structInAPI(compositeObject):
        for c in api.compositeTypes:
            if c.name == compositeObject.name:
                return True
        return False

    # helper class used to encapsulate all data needed during generation
    class StructureDetail:
        def __init__ (self, compositeObject):
            self.nameList = [compositeObject.name] + compositeObject.aliasList
            self.sType = compositeObject.members[0].values
            self.instanceName = 'd' + compositeObject.name[11:]
            self.flagName = 'is' + compositeObject.name[16:]
            self.extension = None
            self.promotedFromEextension = None
            self.api = None
            self.major = None
            self.minor = None
            structureMembers = compositeObject.members[2:]
            self.members = [m.name for m in structureMembers]

    # helper extension class used in algorith below
    class StructureFoundContinueToNextOne(Exception):
        pass

    # find structures that extend VkPhysicalDeviceFeatures2
    structures = [c for c in api.compositeTypes if c.structextends is not None and 'VkPhysicalDeviceFeatures2' in c.structextends]
    # remove structures that were added by extensions other than KHR and EXT
    testedStructures = []
    for s in structures:
        if all([postfix not in s.name for postfix in EXTENSION_POSTFIXES_VENDOR]):
            testedStructures.append(s)

    existingStructures = list(filter(structInAPI, testedStructures)) # remove features not found in API ( important for Vulkan SC )
    testedStructureDetail = [StructureDetail(struct) for struct in existingStructures]
    # list of partially promoted extensions that are not marked in vk.xml as partially promoted in extension definition
    # note: for VK_EXT_host_image_copy there is a comment in require section for vk1.4
    partiallyPromotedExtensions = ['VK_EXT_pipeline_protected_access', 'VK_EXT_host_image_copy']
    # iterate over all searched structures and find extensions that enabled them
    for structureDetail in testedStructureDetail:
        try:
            # iterate over all extensions
            for extension in api.extensions:
                for requirement in extension.requirementsList:
                    for extensionStructure in requirement.newTypes:
                        if extensionStructure.name in structureDetail.nameList:
                            structureDetail.extension = extension.name
                            if extension.promotedto is not None and extension.partiallyPromoted is False and extension.name not in partiallyPromotedExtensions:
                                # check if extension was promoted to vulkan version or other extension;
                                if 'VK_VERSION' in extension.promotedto:
                                    versionSplit = extension.promotedto.split('_')
                                    structureDetail.api = 0 if api.apiName == "vulkan" else 1
                                    structureDetail.major = versionSplit[-2]
                                    structureDetail.minor = versionSplit[-1]
                                else:
                                    structureDetail.extension = extension.promotedto
                                    structureDetail.promotedFromEextension = extension.name
                            raise StructureFoundContinueToNextOne
        except StructureFoundContinueToNextOne:
            continue
    structureDetailToRemove = []
    for structureDetail in testedStructureDetail:
        if structureDetail.major is not None or structureDetail.extension in partiallyPromotedExtensions:
            continue
        # if structure was not added with extension then check if
        # it was added directly with one of vulkan versions
        structureName = structureDetail.nameList[0]
        for feature in api.features:
            for requirement in feature.requirementsList:
                if structureName in requirement.typeList:
                    if api.apiName == "vulkansc" and int(feature.number[-1]) > 2:
                        structureDetailToRemove.append(structureDetail)
                    else:
                        versionSplit = feature.name.split('_')
                        structureDetail.api = 0 if api.apiName == "vulkan" else 1
                        structureDetail.major = versionSplit[-2]
                        structureDetail.minor = versionSplit[-1]
                    break
            if structureDetail.major is not None:
                break
    # remove structures that should not be tested for given api version
    for sd in structureDetailToRemove:
        testedStructureDetail.remove(sd)
    # generate file content
    structureDefinitions = []
    featureEnabledFlags = []
    clearStructures = []
    structureChain = []
    logStructures = []
    verifyStructures = []
    for index, structureDetail in enumerate(testedStructureDetail):
        structureName = structureDetail.nameList[0]
        # create two instances of each structure
        structureDefinitions.append(structureName + ' ' + structureDetail.instanceName + '[count];')
        # create flags that check if proper extension or vulkan version is available
        condition = ''
        extension = structureDetail.extension
        promotedFromEextension = structureDetail.promotedFromEextension
        major = structureDetail.major
        if extension is not None:
            condition = ' checkExtension(properties, "' + extension + '")'
            if promotedFromEextension is not None:
                condition += ' || checkExtension(properties, "' + promotedFromEextension + '")'
        if major is not None:
            condition = ' ' if condition == '' else condition + ' || '
            condition += 'context.contextSupports(vk::ApiVersion(' + str(structureDetail.api) + ', ' + str(major) + ', ' + str(structureDetail.minor) + ', 0))'
        if condition == '':
            condition = ' true'
        condition += ';'
        nameSpacing = ' ' * int((len(structureName) - len("const bool")) + 1)
        featureEnabledFlags.append('const bool' + nameSpacing + structureDetail.flagName + ' =' + condition)
        # clear memory of each structure
        clearStructures.append('    deMemset(&' + structureDetail.instanceName + '[ndx], 0xFF * ndx, sizeof(' + structureName + '));')
        # construct structure chain
        nextInstanceName = 'nullptr';
        if index < len(testedStructureDetail)-1:
            nextInstanceName = '&' + testedStructureDetail[index+1].instanceName + '[ndx]'
        structureChain.append([
            '        ' + structureDetail.instanceName + '[ndx].sType = ' + structureDetail.sType + ';',
            '        ' + structureDetail.instanceName + '[ndx].pNext = nullptr;'])
        # construct log section
        logStructures.append([
            '    log << TestLog::Message << ' + structureDetail.instanceName + '[0] << TestLog::EndMessage;'
            ])
        # construct verification section
        verifyStructure = []
        verifyStructure.append('    if (')
        for index, m in enumerate(structureDetail.members):
            postfix = ')' if index == len(structureDetail.members)-1 else ' ||'
            verifyStructure.append('        ' + structureDetail.instanceName + '[0].' + m + ' != ' + structureDetail.instanceName + '[1].' + m + postfix)
        if len(structureDetail.members) == 0:
            verifyStructure.append('    false)')
        verifyStructure.append('    {\n        TCU_FAIL("Mismatch between ' + structureName + '");\n    }')
        verifyStructures.append(verifyStructure)

    # construct file content
    stream = []

    # individual test functions
    for n, x in enumerate(testedStructureDetail):
        stream.append("tcu::TestStatus testPhysicalDeviceFeature" + x.instanceName[len('device'):]+" (Context& context)")
        stream.append("""{
    const VkPhysicalDevice        physicalDevice = context.getPhysicalDevice();
    const CustomInstance          instance(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
    const InstanceDriver&         vki(instance.getDriver());
    const int                     count = 2u;
    TestLog&                      log = context.getTestContext().getLog();
    VkPhysicalDeviceFeatures2     extFeatures;
    vector<VkExtensionProperties> properties = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
""")
        stream.append("    "+structureDefinitions[n])
        stream.append("    "+featureEnabledFlags[n])
        stream.append('')
        stream.append('    if (!' + x.flagName + ')')
        stream.append('        return tcu::TestStatus::pass("Querying not supported");')
        stream.append('')
        stream.append('    for (int ndx = 0; ndx < count; ++ndx)\n    {')
        stream.append("    " + clearStructures[n])
        stream.extend(structureChain[n])
        stream.append('')
        stream.append(
                '        deMemset(&extFeatures.features, 0xcd, sizeof(extFeatures.features));\n'
                '        extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;\n'
                '        extFeatures.pNext = &' + testedStructureDetail[n].instanceName + '[ndx];\n\n'
                '        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);')
        stream.append('    }\n')
        stream.extend(logStructures[n])
        stream.append('')
        stream.extend(verifyStructures[n])
        stream.append('    return tcu::TestStatus::pass("Querying succeeded");')
        stream.append("}\n")

    allApiVersions = [f.number for f in api.features]
    promotedTests = []
    if api.apiName == "vulkan":
        for feature in api.features:
            if api.apiName not in feature.api.split(','):
                continue
            major = feature.number[0]
            minor = feature.number[-1]
            promotedFeatures = []
            if feature.name == 'VK_VERSION_1_0':
                continue
            for requirement in feature.requirementsList:
                for type in requirement.typeList:
                    matchedStructType = re.search(r'VkPhysicalDevice(\w+)Features', type, re.IGNORECASE)
                    matchedCoreStructType = re.search(r'VkPhysicalDeviceVulkan(\d+)Features', type, re.IGNORECASE)
                    if matchedStructType and not matchedCoreStructType:
                        promotedFeatures.append(type)
            if promotedFeatures:
                testName = "createDeviceWithPromoted" + feature.number.replace('.', '') + "Structures"
                promotedTests.append(testName)
                stream.append("tcu::TestStatus " + testName + " (Context& context)")
                stream.append("{")
                stream.append(
                '    if (!context.contextSupports(vk::ApiVersion(0, ' + major + ', ' + minor + ', 0)))\n'
                '        TCU_THROW(NotSupportedError, "Vulkan ' + major + '.' + minor + ' is not supported");')
                stream.append("""
    const PlatformInterface&        platformInterface = context.getPlatformInterface();
    const CustomInstance            instance            (createCustomInstanceFromContext(context));
    const InstanceDriver&            instanceDriver        (instance.getDriver());
    const VkPhysicalDevice            physicalDevice = chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t                    queueFamilyIndex = 0;
    const uint32_t                    queueCount = 1;
    const uint32_t                    queueIndex = 0;
    const float                        queuePriority = 1.0f;

    const vector<VkQueueFamilyProperties> queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    const VkDeviceQueueCreateInfo    deviceQueueCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (VkDeviceQueueCreateFlags)0u,
        queueFamilyIndex, //queueFamilyIndex;
        queueCount, //queueCount;
        &queuePriority, //pQueuePriorities;
    };
""")
                lastFeature = ''
                usedFeatures = []
                for feature in promotedFeatures:
                    for struct in testedStructureDetail:
                        if (struct.instanceName in usedFeatures):
                            continue
                        if feature in struct.nameList:
                            if lastFeature:
                                stream.append("\t" + feature + " " + struct.instanceName + " = initVulkanStructure(&" + lastFeature + ");")
                            else:
                                stream.append("\t" + feature + " " + struct.instanceName + " = initVulkanStructure();")
                            lastFeature = struct.instanceName
                            usedFeatures.append(struct.instanceName)
                            break
                stream.append("\tVkPhysicalDeviceFeatures2 extFeatures = initVulkanStructure(&" + lastFeature + ");")
                stream.append("""
    instanceDriver.getPhysicalDeviceFeatures2 (physicalDevice, &extFeatures);

    const VkDeviceCreateInfo        deviceCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
        &extFeatures, //pNext;
        (VkDeviceCreateFlags)0u,
        1, //queueRecordCount;
        &deviceQueueCreateInfo, //pRequestedQueues;
        0, //layerCount;
        nullptr, //ppEnabledLayerNames;
        0, //extensionCount;
        nullptr, //ppEnabledExtensionNames;
        nullptr, //pEnabledFeatures;
    };

    const Unique<VkDevice>            device            (createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
    const DeviceDriver                deviceDriver    (platformInterface, instance, device.get(), context.getUsedApiVersion(), context.getTestContext().getCommandLine());
    const VkQueue                    queue = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);

    VK_CHECK(deviceDriver.queueWaitIdle(queue));

    return tcu::TestStatus::pass("Pass");
}
""")

    # function to create tests
    stream.append("void addSeparateFeatureTests (tcu::TestCaseGroup* testGroup)\n{")
    for x in testedStructureDetail:
        stream.append('\taddFunctionCase(testGroup, "' + camelToSnake(x.instanceName[len('device'):]) + '", testPhysicalDeviceFeature' + x.instanceName[len('device'):] + ');')
    for x in promotedTests:
        stream.append('\taddFunctionCase(testGroup, "' + camelToSnake(x) + '", ' + x + ');')
    stream.append('}\n')

    # write out
    writeInlFile(filename, INL_HEADER, stream)

class FeaturesOrPropertiesDefs:
    def __init__ (self, structureType, structureTypeName):
        # string with most important part from structure type e.g. 'LINE_RASTERIZATION'
        # (for VkPhysicalDeviceLineRasterizationFeaturesEXT) without prefix nor postfix
        self.structureType = structureType
        # string containing version, this is needed to handle corner case like e.g.
        # VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD where 2 is after PROPERTIES
        self.verSuffix = ''
        self.extSuffix = ''                          # string with extension type e.g. '_EXT' for VkPhysicalDeviceLineRasterizationFeaturesEXT
        self.structureTypeName = structureTypeName   # full structure name e.g. 'VkPhysicalDeviceCustomBorderColorFeaturesEXT'
        self.extensionName = None                    # name of extension that added this structure eg. 'VK_EXT_depth_clip_enable'
        self.previousExtensionName = None            # None or name of previous extension from which this one was promoted
        self.nameString = None                       # e.g. 'VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME'
        self.versionString = '0'                     # e.g. 'VK_EXT_SHADER_ATOMIC_FLOAT_SPEC_VERSION'
        self.compositeType = None                    # None or pointer to composite type
    def __iter__(self):
        return iter((self.structureType, self.verSuffix, self.extSuffix, self.structureTypeName, self.extensionName, self.nameString, self.versionString))

def generateDeviceFeaturesOrPropertiesDefs(api, FeaturesOrProperties):
    assert(FeaturesOrProperties in ['Features', 'Properties'])
    defs = []
    foundStructureEnums = []
    structureEnumPattern = fr'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_(\w+)_{FeaturesOrProperties.upper()}(\w+)'
    structureEnumPatternNotExtension = structureEnumPattern[:-5] + '$'
    structureTypePattern = fr'VkPhysicalDevice(\w+){FeaturesOrProperties}(\w+)'
    structureTypePatternNotExtension = structureTypePattern[:-5] + '$'
    structureTypeToSkipPattern = fr'VkPhysicalDeviceVulkan(SC)*\d\d{FeaturesOrProperties}'
    structureExtendsPattern = f'VkPhysicalDevice{FeaturesOrProperties}2'
    # iterate over all extensions to find extension that adds enum value matching pattern;
    # this will always be in first requirement section
    for ext in api.extensions:
        # skip extensions that were promoted to other extensions (not vk version)
        if ext.promotedto is not None and "VK_VERSION" not in ext.promotedto:
            continue
        allExtendedEnums = ext.requirementsList[0].extendedEnums
        for extendedEnum in allExtendedEnums:
            matchedStructEnum = re.search(structureEnumPattern, extendedEnum.name, re.IGNORECASE)
            if matchedStructEnum:
                # find feature/property structure type name
                structureTypeName = ""
                for stRequirement in ext.requirementsList[0].newTypes:
                    stName = stRequirement.name
                    matchedStructType = re.search(structureTypePattern, stName, re.IGNORECASE)
                    if matchedStructType:
                        structureTypeName = stName
                        break
                # iterate over all composite types to check if structureTypeName is not alias
                # this handles case where extension was promoted and with it feature/property structure
                structureType = None
                for ct in api.compositeTypes:
                    if structureTypeName == ct.name:
                        structureType = ct
                        break
                    elif structureTypeName in ct.aliasList:
                        structureType = ct
                        structureTypeName = structureType.name
                        break
                # use data in structextends to skip structures that should not be passed to vkGetPhysicalDeviceProperties(/Features)2 function
                if structureType is None or structureType.structextends is None or structureExtendsPattern not in structureType.structextends:
                    continue
                # meke sure that structure was not added earlier - this handles special
                # cases like VkPhysicalDeviceIDPropertiesKHR added by 3 extensions
                if len([d for d in defs if d.structureTypeName == structureTypeName]) > 0:
                    continue
                foundStructureEnums.append(matchedStructEnum.group(1))
                fop = FeaturesOrPropertiesDefs(matchedStructEnum.group(1), structureTypeName)
                fop.extensionName         = ext.name
                fop.previousExtensionName = None if ext.promotedFrom is None else ext.promotedFrom[0]
                fop.nameString            = allExtendedEnums[1].name
                fop.versionString         = allExtendedEnums[0].name
                fop.compositeType         = structureType
                defs.append(fop)
                # there are cases like VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD
                # where 2 is after PROPERTIES - to handle this we need to split suffix to two parts
                fop.extSuffix = matchedStructEnum.group(2)
                suffixStart = fop.extSuffix.rfind('_')
                if suffixStart > 0:
                    fop.verSuffix = fop.extSuffix[:suffixStart]
                    fop.extSuffix = fop.extSuffix[suffixStart:]
                # accept single feature/property structure per extension - this also handles cases
                # like VK_KHR_variable_pointers which specify feature structure and its alias
                break

    # iterate over all structures to find Feature/Property structures that were not added with extension
    # but with vulkan version; to do that we need to skip extension part from pattern
    for ct in api.compositeTypes:
        matchedStructType = re.search(structureTypePatternNotExtension, ct.name, re.IGNORECASE)
        if matchedStructType:
            if ct.members[0].name != "sType":
                continue
            if ct.structextends is None or structureExtendsPattern not in ct.structextends:
                continue
            matchedStructEnum = re.search(structureEnumPatternNotExtension, ct.members[0].values, re.IGNORECASE)
            if (matchedStructEnum.group(1) not in foundStructureEnums) and (re.match(structureTypeToSkipPattern, ct.name) == None):
                defs.append(FeaturesOrPropertiesDefs(matchedStructEnum.group(1), ct.name))
    return defs

def constructPromotionCheckerFunString(defs, FeatureOrProperty):
    # construct function that will return previous extension that provided same feature struct
    assert(FeatureOrProperty in ['Feature', 'Property'])
    l = ',\n'.join([f"\t\t{{ \"{d.extensionName}\", \"{d.previousExtensionName}\" }}" for d in defs if d.previousExtensionName != None])
    return (f"const std::string getPrevious{FeatureOrProperty}ExtName (const std::string &name)\n{{\n"
             "\tconst std::map<std::string, std::string> previousExtensionsMap {\n"
            f"{l}"
             "\n\t};\n\n\tauto it = previousExtensionsMap.find(name);\n"
             "\tif(it == previousExtensionsMap.end())\n"
             "\t\treturn {};\n"
             "\treturn it->second;\n}")

def writeFeaturesVariant(dfDefs, filename):
    types = []

    extendVariant = \
        "template<class Variant, class... Types> struct extend_variant;\n" \
        "template<class... X, class... Types>\n" \
        "    struct extend_variant<std::variant<X...>, Types...> {\n" \
        "        typedef std::variant<X..., Types...> type;\n" \
        "};"

    variantIndex = \
        "template <class X, class Variant> struct variant_index;\n" \
        "template <class X, class... Types> struct variant_index<X, std::variant<X, Types...>>\n" \
        "    : std::integral_constant<std::size_t, 0> { };\n" \
        "template <class X, class Y, class... Types> struct variant_index<X, std::variant<Y, Types...>>\n" \
        "    : std::integral_constant<std::size_t, 1 + variant_index<X, std::variant<Types...>>::value> { };\n" \
        "template <class X> struct variant_index<X, std::variant<>> : std::integral_constant<std::size_t, 0> { };\n" \
        "template <typename X, typename Variant>\n" \
        "    constexpr std::size_t variant_index_v = variant_index<X, Variant>::value;"

    feature2sType = []
    f2st0 = "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceFeatures> = VK_STRUCTURE_TYPE_MAX_ENUM;"
    f2st1 = "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceFeatures2> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;\n" \
            "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan11Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;"
    f2st2 = "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan12Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;"
    f2st3 = "#if defined(VK_API_VERSION_1_3) && !defined(CTS_USES_VULKANSC)\n" \
            "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan13Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;\n" \
            "#endif"
    f2st4 = "#if defined(VK_API_VERSION_1_4) && !defined(CTS_USES_VULKANSC)\n" \
            "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkan14Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;\n" \
            "#endif"
    f2stsc10 = "#ifdef CTS_USES_VULKANSC\n" \
               "template <> inline constexpr VkStructureType feature2sType<VkFaultCallbackInfo> = VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO;\n" \
               "template <> inline constexpr VkStructureType feature2sType<VkPhysicalDeviceVulkanSC10Features> = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES;\n" \
               "template <> inline constexpr VkStructureType feature2sType<VkDeviceObjectReservationCreateInfo> = VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO;\n" \
               "#endif"

    feature2sType.append("template <class> VkStructureType feature2sType;")
    feature2sType.append(f2st0)
    feature2sType.append(f2st1)
    feature2sType.append(f2st2)
    feature2sType.append(f2st3)
    feature2sType.append(f2st4)
    feature2sType.append(f2stsc10)

    getFeatureSType = \
        "template <class X> VkStructureType getFeatureSType() {\n" \
        "if constexpr (variant_index_v<X, ImplementedFeaturesVariant> < std::variant_size_v<ImplementedFeaturesVariant>)\n" \
        "    return vk::makeFeatureDesc<X>().sType;\n" \
        "else\n" \
        "    return feature2sType<X>;\n" \
        "}"

    hasPnextOfVoidPtr = \
        "template<class, class = void>\n" \
        "    struct hasPnextOfVoidPtr : std::false_type {};\n" \
        "template<class X>\n" \
        "    struct hasPnextOfVoidPtr<X, std::void_t<decltype(std::declval<X>().pNext)>>\n" \
        "        : std::integral_constant<bool,\n" \
        "              std::is_same<decltype(std::declval<X>().pNext), void*>::value ||\n" \
        "              std::is_same<decltype(std::declval<X>().pNext), const void*>::value> {};"

    implementedVariantBegin = "typedef std::variant<"
    for idx, (sType, sVerSuffix, sExtSuffix, extStruct, _, extNameDef, specVersionDef) in enumerate(dfDefs):
        types.append("    {0}{1}".format((", " if idx else ""), extStruct))
    implementedVariantEnd = "> ImplementedFeaturesVariant;"
    v10 = "    VkPhysicalDeviceFeatures"
    v11 = "#ifdef VK_API_VERSION_1_1\n" \
          "    , VkPhysicalDeviceFeatures2\n" \
          "    , VkPhysicalDeviceVulkan11Features\n" \
          "#endif"
    v12 = "#ifdef VK_API_VERSION_1_2\n" \
          "    , VkPhysicalDeviceVulkan12Features\n" \
          "#endif"
    v13 = "#if defined(VK_API_VERSION_1_3) && !defined(CTS_USES_VULKANSC)\n" \
          "    , VkPhysicalDeviceVulkan13Features\n" \
          "#endif"
    v14 = "#if defined(VK_API_VERSION_1_4) && !defined(CTS_USES_VULKANSC)\n" \
          "    , VkPhysicalDeviceVulkan14Features\n" \
          "#endif"
    vsc10 = "#ifdef CTS_USES_VULKANSC\n" \
            "    , VkFaultCallbackInfo\n" \
            "    , VkPhysicalDeviceVulkanSC10Features\n" \
            "    , VkDeviceObjectReservationCreateInfo\n" \
            "#endif"
    fullVariant = []
    fullVariant.append("typedef typename extend_variant<ImplementedFeaturesVariant,")
    fullVariant.append(v10)
    fullVariant.append(v11)
    fullVariant.append(v12)
    fullVariant.append(v13)
    fullVariant.append(v14)
    fullVariant.append(vsc10)
    fullVariant.append(">::type FullFeaturesVariant;")
    stream = []
    stream.append(extendVariant)
    stream.append(variantIndex)
    stream.append(implementedVariantBegin)
    stream.extend(types)
    stream.append(implementedVariantEnd)
    stream.extend(fullVariant)
    stream.extend(feature2sType)
    stream.append(getFeatureSType)
    stream.append(hasPnextOfVoidPtr)
    writeInlFile(filename, INL_HEADER, stream)

def writeDeviceFeatures(api, dfDefs, filename):
    # find VkPhysicalDeviceVulkan[1-9][0-9]Features blob structurs
    # and construct dictionary with all of their attributes
    blobMembers = {}
    blobStructs = {}
    blobPattern = re.compile("^VkPhysicalDeviceVulkan([1-9][0-9])Features[0-9]*$")
    for structureType in api.compositeTypes:
        match = blobPattern.match(structureType.name)
        if match:
            allMembers = [member.name for member in structureType.members]
            vkVersion = match.group(1)
            blobMembers[vkVersion] = allMembers[2:]
            blobStructs[vkVersion] = set()
    initFromBlobDefinitions = []
    emptyInitDefinitions = []
    # iterate over all feature structures
    allFeaturesPattern = re.compile(r"^VkPhysicalDevice\w+Features[1-9]*")
    nonExtFeaturesPattern = re.compile(r"^VkPhysicalDevice\w+Features[1-9]*$")
    for structureType in api.compositeTypes:
        # skip structures that are not feature structures
        if not allFeaturesPattern.match(structureType.name):
            continue
        # skip structures that were previously identified as blobs
        if blobPattern.match(structureType.name):
            continue
        # skip sType and pNext and just grab third and next attributes
        structureMembers = structureType.members[2:]
        notPartOfBlob = True
        if nonExtFeaturesPattern.match(structureType.name):
            # check if this member is part of any of the blobs
            for blobName, blobMemberList in blobMembers.items():
                # if just one member is not part of this blob go to the next blob
                # (we asume that all members are part of blob - no need to check all)
                if structureMembers[0].name not in blobMemberList:
                    continue
                # add another feature structure name to this blob
                blobStructs[blobName].add(structureType)
                # add specialization for this feature structure
                memberCopying = ""
                for member in structureMembers:
                    memberCopying += "\tfeatureType.{0} = allFeaturesBlobs.vk{1}.{0};\n".format(member.name, blobName)
                wholeFunction = \
                    "template<> void initFeatureFromBlob<{0}>({0}& featureType, const AllFeaturesBlobs& allFeaturesBlobs)\n" \
                    "{{\n" \
                    "{1}" \
                    "}}".format(structureType.name, memberCopying)
                initFromBlobDefinitions.append(wholeFunction)
                notPartOfBlob = False
                # assuming that all members are part of blob, goto next
                break
        # add empty template definition as on Fedora there are issue with
        # linking using just generic template - all specializations are needed
        if notPartOfBlob:
            emptyFunction = "template<> void initFeatureFromBlob<{0}>({0}&, const AllFeaturesBlobs&) {{}}"
            emptyInitDefinitions.append(emptyFunction.format(structureType.name))
    extensionDefines = []
    makeFeatureDescDefinitions = []
    featureStructWrappers = []
    for idx, (sType, sVerSuffix, sExtSuffix, extStruct, _, extNameDef, specVersionDef) in enumerate(dfDefs):
        extensionNameDefinition = extNameDef
        if not extensionNameDefinition:
            extensionNameDefinition = 'DECL{0}_{1}_EXTENSION_NAME'.format((sExtSuffix if sExtSuffix else ''), sType)
            extensionDefines.append(f'#define {extensionNameDefinition} "core_feature"')
        # construct makeFeatureDesc template function definitions
        sTypeName = "VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_{0}_FEATURES{1}".format(sType, sVerSuffix + sExtSuffix)
        makeFeatureDescDefinitions.append("template<> FeatureDesc makeFeatureDesc<{0}>(void) " \
                                          "{{ return FeatureDesc{{{1}, {2}, {3}}}; }}".format(extStruct, sTypeName, extensionNameDefinition, specVersionDef))
        # construct CreateFeatureStruct wrapper block
        featureStructWrappers.append("\t{{ createFeatureStructWrapper<{0}>, {1}, {2} }},".format(extStruct, extensionNameDefinition, specVersionDef))
    # construct function that will check for which vk version structure sType is part of blob
    blobChecker = "uint32_t getBlobFeaturesVersion (VkStructureType sType)\n{\n" \
                  "\tconst std::map<VkStructureType, uint32_t> sTypeBlobMap {\n"
    blobCheckerMap = "static const std::map<VkStructureType, uint32_t> sTypeBlobMap\n" \
                     "{\n"
    # iterate over blobs with list of structures
    for blobName in sorted(blobStructs.keys()):
        blobCheckerMap += "\t// Vulkan{0}\n".format(blobName)
        # iterate over all feature structures in current blob
        structuresList = list(blobStructs[blobName])
        structuresList = sorted(structuresList, key=lambda s: s.name)
        for structType in structuresList:
            # find definition of this structure in dfDefs
            structDef = None
            allNamesToCheck = [structType.name]
            if len(structType.aliasList) > 0:
                allNamesToCheck.extend(structType.aliasList)
            for structName in allNamesToCheck:
                structDefList = [s for s in dfDefs if s.structureTypeName == structName]
                if len(structDefList) > 0:
                    structDef = structDefList[0]
                    break
            sType = structDef.structureType
            sSuffix = structDef.verSuffix + structDef.extSuffix
            sTypeName = "VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_{0}_FEATURES{1}".format(sType, sSuffix)
            tabs = "\t" * int((88 - len(sTypeName)) / 4)
            blobCheckerMap += "\t{{ {0},{1}VK_API_VERSION_{2}_{3} }},\n".format(sTypeName, tabs, blobName[0], blobName[1])
    blobCheckerMap += "};\n"

    blobChecker = "uint32_t getBlobFeaturesVersion (VkStructureType sType)\n{\n" \
                   "\tauto it = sTypeBlobMap.find(sType);\n" \
                   "\tif(it == sTypeBlobMap.end())\n" \
                   "\t\treturn 0;\n" \
                   "\treturn it->second;\n" \
                   "}\n"
    blobExpander = "std::set<VkStructureType> getVersionBlobFeatureList (uint32_t version)\n" \
                   "{\n" \
                   "\tstd::set<VkStructureType> features;\n" \
                   "\tfor (const std::pair<const VkStructureType, uint32_t> &item : sTypeBlobMap)\n" \
                   "\t{\n" \
                   "\t\tif (item.second == version)\n" \
                   "\t\t\tfeatures.insert(item.first);\n" \
                   "\t}\n" \
                   "\treturn features;\n" \
                   "}\n"

    # combine all definition lists
    stream = [
    '#include "vkDeviceFeatures.hpp"\n',
    "#include <set>\n",
    'namespace vk\n{']
    stream.extend(extensionDefines)
    stream.append('')
    stream.extend(initFromBlobDefinitions)
    stream.append('\n// generic template is not enough for some compilers')
    stream.extend(emptyInitDefinitions)
    stream.append('')
    stream.extend(makeFeatureDescDefinitions)
    stream.append('')
    stream.append('static const FeatureStructCreationData featureStructCreationArray[]\n{')
    stream.extend(featureStructWrappers)
    stream.append('};\n')
    stream.append(constructPromotionCheckerFunString(dfDefs, "Feature"))
    stream.append('')
    stream.append(blobCheckerMap)
    stream.append(blobChecker)
    stream.append(blobExpander)
    stream.append('} // vk\n')
    writeInlFile(filename, INL_HEADER, stream)

def writeDeviceFeatureTest(api, filename):

    coreFeaturesPattern = re.compile("^VkPhysicalDeviceVulkan([1-9][0-9])Features[0-9]*$")
    featureItems = []
    testFunctions = []
    # iterate over all feature structures
    allFeaturesPattern = re.compile(r"^VkPhysicalDevice\w+Features[1-9]*")
    for structureType in api.compositeTypes:
        # skip structures that are not feature structures
        if not allFeaturesPattern.match(structureType.name):
            continue
        # skip sType and pNext and just grab third and next attributes
        structureMembers = structureType.members[2:]

        items = []
        for member in structureMembers:
            items.append("        FEATURE_ITEM ({0}, {1}),".format(structureType.name, member.name))

        testBlock = """
tcu::TestStatus createDeviceWithUnsupportedFeaturesTest{4} (Context& context)
{{
    const PlatformInterface&                vkp = context.getPlatformInterface();
    tcu::TestLog&                            log = context.getTestContext().getLog();
    tcu::ResultCollector                    resultCollector            (log);
    const CustomInstance                    instance                (createCustomInstanceWithExtensions(context, context.getInstanceExtensions(), nullptr, true));
    const InstanceDriver&                    instanceDriver            (instance.getDriver());
    const VkPhysicalDevice                    physicalDevice = chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t                            queueFamilyIndex = 0;
    const uint32_t                            queueCount = 1;
    const float                                queuePriority = 1.0f;
    const DeviceFeatures                    deviceFeaturesAll        (context.getInstanceInterface(), context.getUsedApiVersion(), physicalDevice, context.getInstanceExtensions(), context.getDeviceExtensions(), true);
    const VkPhysicalDeviceFeatures2            deviceFeatures2 = deviceFeaturesAll.getCoreFeatures2();
    int                                        numErrors = 0;
    const tcu::CommandLine&                    commandLine = context.getTestContext().getCommandLine();
    bool                                    isSubProcess = context.getTestContext().getCommandLine().isSubProcess();
{6}

    VkPhysicalDeviceFeatures emptyDeviceFeatures;
    deMemset(&emptyDeviceFeatures, 0, sizeof(emptyDeviceFeatures));

    // Only non-core extensions will be used when creating the device.
    const auto& extensionNames = context.getDeviceCreationExtensions();
    DE_UNREF(extensionNames); // In some cases this is not used.

    if (const void* featuresStruct = findStructureInChain(const_cast<const void*>(deviceFeatures2.pNext), getStructureType<{0}>()))
    {{
        static const Feature features[] =
        {{
{1}
        }};
        auto* supportedFeatures = reinterpret_cast<const {0}*>(featuresStruct);
        checkFeatures(vkp, instance, instanceDriver, physicalDevice, {2}, features, supportedFeatures, queueFamilyIndex, queueCount, queuePriority, numErrors, resultCollector, {3}, emptyDeviceFeatures, {5}, context.getUsedApiVersion(), commandLine);
    }}

    if (numErrors > 0)
        return tcu::TestStatus(resultCollector.getResult(), "Enabling unsupported features didn't return VK_ERROR_FEATURE_NOT_PRESENT.");

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}}
"""
        additionalParams = ( 'memReservationStatMax, isSubProcess' if api.apiName == 'vulkansc' else 'isSubProcess' )
        additionalDefs = ( '    VkDeviceObjectReservationCreateInfo memReservationStatMax = context.getResourceInterface()->getStatMax();' if apiName == 'vulkansc' else '')
        featureItems.append(testBlock.format(structureType.name, "\n".join(items), len(items), ("nullptr" if coreFeaturesPattern.match(structureType.name) else "&extensionNames"), structureType.name[len('VkPhysicalDevice'):], additionalParams, additionalDefs))

        testFunctions.append("createDeviceWithUnsupportedFeaturesTest" + structureType.name[len('VkPhysicalDevice'):])

    stream = ['']
    stream.extend(featureItems)
    stream.append("""
void addSeparateUnsupportedFeatureTests (tcu::TestCaseGroup* testGroup)
{
""")
    for x in testFunctions:
        stream.append('\taddFunctionCase(testGroup, "' + camelToSnake(x[len('createDeviceWithUnsupportedFeaturesTest'):]) + '", ' + x + ');')
    stream.append('}\n')

    writeInlFile(filename, INL_HEADER, stream)

def writeDeviceProperties(api, dpDefs, filename):
    # find VkPhysicalDeviceVulkan[1-9][0-9]Features blob structurs
    # and construct dictionary with all of their attributes
    blobMembers = {}
    blobStructs = {}
    blobPattern = re.compile("^VkPhysicalDeviceVulkan([1-9][0-9])Properties[0-9]*$")
    for structureType in api.compositeTypes:
        match = blobPattern.match(structureType.name)
        if match:
            allMembers = [member.name for member in structureType.members]
            vkVersion = match.group(1)
            blobMembers[vkVersion] = allMembers[2:]
            blobStructs[vkVersion] = set()
    initFromBlobDefinitions = []
    emptyInitDefinitions = []
    # iterate over all property structures
    allPropertiesPattern = re.compile(r"^VkPhysicalDevice\w+Properties[1-9]*")
    nonExtPropertiesPattern = re.compile(r"^VkPhysicalDevice\w+Properties[1-9]*$")
    for structureType in api.compositeTypes:
        # skip structures that are not property structures
        if not allPropertiesPattern.match(structureType.name):
            continue
        # skip structures that were previously identified as blobs
        if blobPattern.match(structureType.name):
            continue
        # skip sType and pNext and just grab third and next attributes
        structureMembers = structureType.members[2:]
        notPartOfBlob = True
        if nonExtPropertiesPattern.match(structureType.name):
            # check if this member is part of any of the blobs
            for blobName, blobMemberList in blobMembers.items():
                # if just one member is not part of this blob go to the next blob
                # (we asume that all members are part of blob - no need to check all)
                if structureMembers[0].name not in blobMemberList:
                    continue
                # add another property structure name to this blob
                blobStructs[blobName].add(structureType)
                # add specialization for this property structure
                memberCopying = ""
                for member in structureMembers:
                    if len(member.arraySizeList) == 0:
                        # handle special case
                        if structureType.name == "VkPhysicalDeviceSubgroupProperties" and "subgroup" not in member.name :
                            blobMemberName = "subgroup" + member.name[0].capitalize() + member.name[1:]
                            memberCopying += "\tpropertyType.{0} = allPropertiesBlobs.vk{1}.{2};\n".format(member.name, blobName, blobMemberName)
                        # end handling special case
                        else:
                            memberCopying += "\tpropertyType.{0} = allPropertiesBlobs.vk{1}.{0};\n".format(member.name, blobName)
                    else:
                        memberCopying += "\tmemcpy(propertyType.{0}, allPropertiesBlobs.vk{1}.{0}, sizeof({2}) * {3});\n".format(member.name, blobName, member.type, member.arraySizeList[0])
                wholeFunction = \
                    "template<> void initPropertyFromBlob<{0}>({0}& propertyType, const AllPropertiesBlobs& allPropertiesBlobs)\n" \
                    "{{\n" \
                    "{1}" \
                    "}}".format(structureType.name, memberCopying)
                initFromBlobDefinitions.append(wholeFunction)
                notPartOfBlob = False
                # assuming that all members are part of blob, goto next
                break
        # add empty template definition as on Fedora there are issue with
        # linking using just generic template - all specializations are needed
        if notPartOfBlob:
            emptyFunction = "template<> void initPropertyFromBlob<{0}>({0}&, const AllPropertiesBlobs&) {{}}"
            emptyInitDefinitions.append(emptyFunction.format(structureType.name))
    extensionDefines = []
    makePropertyDescDefinitions = []
    propertyStructWrappers = []
    for idx, (sType, sVerSuffix, sExtSuffix, extStruct, _, extNameDef, specVersionDef) in enumerate(dpDefs):
        extensionNameDefinition = extNameDef
        if not extensionNameDefinition:
            extensionNameDefinition = 'DECL{0}_{1}_EXTENSION_NAME'.format((sExtSuffix if sExtSuffix else ''), sType)
            extensionDefines.append(f'#define {extensionNameDefinition} "core_property"')
        # construct makePropertyDesc template function definitions
        sTypeName = "VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_{0}_PROPERTIES{1}".format(sType, sVerSuffix + sExtSuffix)
        makePropertyDescDefinitions.append("template<> PropertyDesc makePropertyDesc<{0}>(void) " \
                                           "{{ return PropertyDesc{{{1}, {2}, {3}}}; }}".format(extStruct, sTypeName, extensionNameDefinition, specVersionDef))
        # construct CreateProperty struct wrapper block
        propertyStructWrappers.append("\t{{ createPropertyStructWrapper<{0}>, {1}, {2} }},".format(extStruct, extensionNameDefinition, specVersionDef))
    # construct method that will check if structure sType is part of blob
    blobChecker = "uint32_t getBlobPropertiesVersion (VkStructureType sType)\n{\n" \
                  "\tconst std::map<VkStructureType, uint32_t> sTypeBlobMap\n" \
                  "\t{\n"
    # iterate over blobs with list of structures
    for blobName in sorted(blobStructs.keys()):
        blobChecker += "\t\t// Vulkan{0}\n".format(blobName)
        # iterate over all feature structures in current blob
        structuresList = list(blobStructs[blobName])
        structuresList = sorted(structuresList, key=lambda s: s.name)
        for structType in structuresList:
            # find definition of this structure in dpDefs
            structName = structType.name
            structDef = None
            foundDefs = [s for s in dpDefs if s.structureTypeName == structName]
            if len(foundDefs) > 0:
                structDef = foundDefs[0]
            else:
                for alias in structType.aliasList:
                    foundDefs = [s for s in dpDefs if s.structureTypeName == alias]
                    if len(foundDefs) > 0:
                        structDef = foundDefs[0]
                        break
            sType = structDef.structureType
            sSuffix = structDef.verSuffix + structDef.extSuffix
            sTypeName = "VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_{0}_PROPERTIES{1}".format(sType, sSuffix)
            tabs = "\t" * int((80 - len(sTypeName)) / 4)
            blobChecker += "\t\t{{ {0},{1}VK_API_VERSION_{2}_{3} }},\n".format(sTypeName, tabs, blobName[0], blobName[1])
    blobChecker += "\t};\n\n" \
                   "\tauto it = sTypeBlobMap.find(sType);\n" \
                   "\tif(it == sTypeBlobMap.end())\n" \
                   "\t\treturn 0;\n" \
                   "\treturn it->second;\n" \
                   "}\n"
    # combine all definition lists
    stream = [
    '#include "vkDeviceProperties.hpp"\n',
    'namespace vk\n{']
    stream.extend(extensionDefines)
    stream.append('')
    stream.extend(initFromBlobDefinitions)
    stream.append('\n// generic template is not enough for some compilers')
    stream.extend(emptyInitDefinitions)
    stream.append('')
    stream.extend(makePropertyDescDefinitions)
    stream.append('\n')
    stream.append('static const PropertyStructCreationData propertyStructCreationArray[] =\n{')
    stream.extend(propertyStructWrappers)
    stream.append('};\n')
    stream.append(constructPromotionCheckerFunString(dpDefs, "Property"))
    stream.append('')
    stream.append(blobChecker)
    stream.append('} // vk')
    writeInlFile(filename, INL_HEADER, stream)

def genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename):
    UNSUFFIXED_STRUCTURES = [
        "CornerSampledImage",
        "ShaderSMBuiltins",
        "ShadingRateImage",
        "RayTracing",
        "RepresentativeFragmentTest",
        "ComputeShaderDerivatives",
        "MeshShader",
        "ShaderImageFootprint",
        "ExclusiveScissor",
        "DedicatedAllocationImageAliasing",
        "CoverageReductionMode",
        "DeviceGeneratedCommands",
        "InheritedViewportScissor",
        "PresentBarrier",
        "DiagnosticsConfig",
        "FragmentShadingRateEnums",
        "RayTracingMotionBlur",
        "ExternalMemoryRDMA",
        "CopyMemoryIndirect",
        "MemoryDecompression",
        "LinearColorAttachment",
        "OpticalFlow",
        "RayTracingInvocationReorder",
        "DisplacementMicromap"]
    stream = []
    for fop in dfDefs:
        # remove VkPhysicalDevice prefix from structure name
        nameSubStr = fop.structureTypeName[16:]
        # remove extension type in some cases
        if nameSubStr[-3:] == "KHR":
            nameSubStr = nameSubStr[:-3]
        elif fop.compositeType and fop.compositeType.notSupportedAlias:
            # remove KHR also for extensions that were promoted in Vulkan but
            # not in VulkanSC this reduces number of ifdefs for SC in CTS code
            nameSubStr = nameSubStr[:-3]
        elif nameSubStr[-2:] == "NV":
            suffix = nameSubStr[-2:]
            nameSubStr = nameSubStr[:-2]
            if nameSubStr[-8:] == "Features":
                infix = nameSubStr[-8:]
                nameSubStr = nameSubStr[:-8]
            elif nameSubStr[-10:] == "Properties":
                infix = nameSubStr[-10:]
                nameSubStr = nameSubStr[:-10]
            if (nameSubStr in UNSUFFIXED_STRUCTURES):
                suffix = ""
            nameSubStr = nameSubStr + infix + suffix
        stream.append(pattern.format(fop.structureTypeName, nameSubStr))
    writeInlFile(filename, INL_HEADER, indentLines(stream))

def writeDeviceFeaturesDefaultDeviceDefs(dfDefs, filename):
    pattern = "const {0}&\tget{1}\t(void) const {{ return m_deviceFeatures.getFeatureType<{0}>();\t}}"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)

def writeDeviceFeaturesContextDecl(dfDefs, filename):
    pattern = "const vk::{0}&\tget{1}\t(void) const;"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)

def writeDeviceFeaturesContextDefs(dfDefs, filename):
    pattern = "const vk::{0}&\tContext::get{1}\t(void) const {{ return m_device->get{1}();\t}}"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)

def writeDevicePropertiesDefaultDeviceDefs(dfDefs, filename):
    pattern = "const {0}&\tget{1}\t(void) const {{ return m_deviceProperties.getPropertyType<{0}>();\t}}"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)

def writeDevicePropertiesContextDecl(dfDefs, filename):
    pattern = "const vk::{0}&\tget{1}\t(void) const;"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)

def writeDevicePropertiesContextDefs(dfDefs, filename):
    pattern = "const vk::{0}&\tContext::get{1}\t(void) const {{ return m_device->get{1}();\t}}"
    genericDeviceFeaturesOrPropertiesWriter(dfDefs, pattern, filename)


def findMandatoryFeaturesDuplicatedStructs(apiStructs):
    # Sanitize
    # apiStructs: {structName: [ext1, ext2, ...]}
    groups = collections.defaultdict(list)

    for struct, exts in apiStructs:          # <- no .items()
        key = tuple(sorted(set(exts)))       # normalize
        groups[key].append(struct)

    dupes = {k: v for k, v in groups.items() if len(v) > 1}

    for exts, structs in dupes.items():
        logging.error("Structs %s share extensions %s",
                      ", ".join(structs), ", ".join(exts))


def findStructNameList(structName, api):
    for compType in api.compositeTypes:
        nameList = [compType.name] + compType.aliasList
        if structName in nameList:
            return nameList
    return []


def getVarNameFromType(s):
    return s[2].lower() + s[3:]


def writeMandatoryFeatures(api, filename):

    def structInAPI(name):
        for c in api.compositeTypes:
            if c.name == name:
                return True
            for alias in c.aliasList:
                if alias == name:
                    return True
        return False

    def getMainAliasForStruct(name):
        for ct in api.compositeTypes:
            if name in ct.aliasList:
                return ct.name
        return name

    def isSupportedByCurrentApi(variants):
        if len(variants) == 0:
            return True
        for variant in variants:
            if variant.lower() == api.apiName.lower():
                return True
        return False

    stream = []

    dictStructs = {}
    dictData = []
    extData = []
    usedFeatureStructs = {}
    debugStruct = "sicalDeviceMultiviewFeat"
    for _, data in api.additionalExtensionData:
        if 'mandatory_features' in data.keys():
            # sort to have same results for py2 and py3
            listStructFeatures = sorted(data['mandatory_features'].items(), key=lambda tup: tup[0])
            for structure, featuresList in listStructFeatures:
                for featureData in featuresList:
                    # allow for featureless VKSC only extensions
                    if not 'features' in featureData.keys() or 'requirements' not in featureData.keys():
                        continue
                    requirements = featureData['requirements']

                    mandatory_variant = ''
                    try:
                        mandatory_variant = featureData['mandatory_variant']
                    except KeyError:
                        mandatory_variant = ''

                    dictData.append( [ structure, featureData['features'], requirements, mandatory_variant] )

                    if structure == 'VkPhysicalDeviceFeatures':
                        continue

                    nameList = findStructNameList(structure, api)
                    if len(nameList) == 0:
                        continue
                    referenceStructName = nameList[0]
                    usedFeatureStructs[referenceStructName] = set()

                    # if structure is not in dict construct name of variable and add is as a first item
                    if (referenceStructName not in dictStructs):
                        dictStructs[referenceStructName] = ([referenceStructName[2:3].lower() + referenceStructName[3:]], mandatory_variant)
                    # add first requirement if it is unique
                    if requirements and (requirements[0] not in dictStructs[referenceStructName][0]):
                        dictStructs[referenceStructName][0].append(requirements[0])

                    if requirements:
                        for req in requirements:
                            if '.' in req:
                                req = req.split('.')[0]
                                reqStruct = 'Vk' + req[0].upper() + req[1:]
                                usedFeatureStructs[reqStruct] = set()

        if 'mandatory_extensions' in data:
            mandatoryExtensions = []
            for mandatoryExt in data['mandatory_extensions']:
                if 'extension' in mandatoryExt:
                    extName = mandatoryExt.pop('extension')
                    mandatoryExtensions.append((extName, mandatoryExt))

            for extension, extensionData in mandatoryExtensions:
                # requirements are actually mandatory.
                if 'requirements' not in extensionData:
                    continue

                requirements = extensionData['requirements']
                mandatory_variant = '' if 'mandatory_variant' not in extensionData else extensionData['mandatory_variant']
                extData.append((extension, requirements, mandatory_variant))

                for req in requirements:
                    if '.' in req:
                        req = req.split('.')[0]
                        reqStruct = 'Vk' + req[0].upper() + req[1:]
                        if stripExtensionSuffix(reqStruct) in usedFeatureStructs:
                            logging.error("")
                        usedFeatureStructs[reqStruct] = set()

    stream.extend(['bool canUseFeaturesStruct (const vector<VkExtensionProperties>& deviceExtensions, uint32_t usedApiVersion,',
                   '\t\t\t\tconst char* extension, const char* extensionPromotedFrom = nullptr)',
                   '{',
                   '\tif (isCoreDeviceExtension(usedApiVersion, extension))',
                   '\t\treturn true;',
                   '\tif (isExtensionStructSupported(deviceExtensions, RequiredExtension(extension)))',
                   '\t\treturn true;',
                   '\treturn extensionPromotedFrom && isExtensionStructSupported(deviceExtensions, RequiredExtension(extensionPromotedFrom));',
                   '}',
                   '',
                   'bool checkBasicMandatoryFeatures(const vkt::Context& context)\n{',
                   '\tif (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))',
                   '\t\tTCU_THROW(NotSupportedError, "Extension VK_KHR_get_physical_device_properties2 is not present");',
                   '',
                   '\tVkPhysicalDevice\t\t\t\t\tphysicalDevice\t\t= context.getPhysicalDevice();',
                   '\tconst InstanceInterface&\t\t\tvki\t\t\t\t\t= context.getInstanceInterface();',
                   '\tconst vector<VkExtensionProperties>\tdeviceExtensions\t= enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);',
                   '\tconst uint32_t\t\t\t\t\t\tusedApiVersion\t\t= context.getUsedApiVersion();',
                   '',
                   '\ttcu::TestLog& log = context.getTestContext().getLog();',
                   '\tvk::VkPhysicalDeviceFeatures2 coreFeatures;',
                   '\tdeMemset(&coreFeatures, 0, sizeof(coreFeatures));',
                   '\tcoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;',
                   '\tvoid** nextPtr = &coreFeatures.pNext;',
                   ''])

    # Find the extensions that added the required feature structs.
    class StructFoundContinue(Exception):
        pass

    for usedStruct in usedFeatureStructs:
        nameList = findStructNameList(usedStruct, api)
        try:
            if usedStruct in nameList:
                # Found the official name list for the struct.
                for extension in api.extensions:
                    for requirement in extension.requirementsList:
                        for extensionStructure in requirement.newTypes:
                            if extensionStructure.name in nameList:
                                # Found extension for the struct.
                                ufs = usedFeatureStructs[usedStruct]
                                if extension.promotedto and 'VK_VERSION' not in extension.promotedto:
                                    ufs.add(extension.promotedto)
                                ufs.add(extension.name)
                                if extension.promotedFrom:
                                    ufs.add(extension.promotedFrom[0])
                                raise StructFoundContinue
        except StructFoundContinue:
            continue

    structList = sorted(usedFeatureStructs.items(), key=lambda tup: tup[0])  # sort to have same results for py2 and py3
    apiStructs = list(filter(lambda x: structInAPI(x[0]), structList))  # remove items not defined in current API

    findMandatoryFeaturesDuplicatedStructs(apiStructs)

    for structName, extensions in apiStructs:
        extensions = sorted(extensions)
        mandatoryVariantList = []
        if structName in dictStructs:
            mandatoryVariantList = dictStructs[structName][1]
        nameList = findStructNameList(structName, api)
        stream.append('\t// ' + ','.join(nameList) + ' for ext [' + ', '.join(extensions) + '] in APIs [' + ', '.join(mandatoryVariantList) + ']\n')
        newStructName = nameList[0]
        newVar = getVarNameFromType(newStructName)

        if not isSupportedByCurrentApi(mandatoryVariantList):
            continue

        stream.extend(['\tvk::' + structName + ' ' + newVar + ';',
                    '\tdeMemset(&' + newVar + ', 0, sizeof(' + newVar + '));',
                    ''])
        if len(extensions) > 0:
            assert len(extensions) < 3
            extensionParams = [f'"{e}"' for e in extensions]
            extensionParams = ', '.join(extensionParams)
            stream.append(f'\tif (canUseFeaturesStruct(deviceExtensions, usedApiVersion, {extensionParams}))')
        elif api.apiName == "vulkan" and structName in dictStructs:
            #reqs = v[0][1:]
            reqs = dictStructs[structName][0][1:]
            cond = 'if ( '
            i = 0
            for req in reqs:
                if i > 0:
                    cond = cond + ' || '
                if (req.startswith("ApiVersion")):
                    cond = cond + 'context.contextSupports(vk::' + req + ')'
                    i += 1
            cond = cond + ' )'
            stream.append('\t' + cond)

        stream.extend(['\t{',
                       '\t\t' + newVar + '.sType = getStructureType<' + structName + '>();',
                       '\t\t*nextPtr = &' + newVar + ';',
                       '\t\tnextPtr  = &' + newVar + '.pNext;',
                       '\t}'])

        stream.append('')

    stream.extend(['\tcontext.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);',
                   '\tbool result = true;',
                   ''])

    for v in dictData:
        stream.append('\t// ' + v[0] + ' in APIs [' + ', '.join(v[3]) + ']')
        if not structInAPI(v[0]): # remove items not defined in current API ( important for Vulkan SC )
            continue
        if not isSupportedByCurrentApi(v[3]):
            continue
        structType = v[0]
        structName = 'coreFeatures.features'
        if v[0] != 'VkPhysicalDeviceFeatures' :
            structName = v[0] #dictStructs[v[0]][0][0]
            nameList = findStructNameList(structName, api)
            #structName = getMainAliasForStruct(structName)
            structName = nameList[0]
            structName = getVarNameFromType(structName)
        if len(v[2]) > 0 :
            condition = 'if ( '
            for i, req in enumerate(v[2]) :
                if (req.startswith("ApiVersion")):
                    condition = condition + 'context.contextSupports(vk::' + req + ')'
                elif '.' in req:
                    condition = condition + req
                else:
                    condition = condition + 'isExtensionStructSupported(deviceExtensions, RequiredExtension("' + req + '"))'
                if i+1 < len(v[2]) :
                    condition = condition + ' && '
            condition = condition + ' )'
            stream.append('\t' + condition)
        stream.append('\t{')
        # Don't need to support an AND case since that would just be another line in the .txt
        if len(v[1]) == 1:
            stream.append('\t\tif ( ' + structName + '.' + v[1][0] + ' == VK_FALSE )')
        else:
            condition = 'if ( '
            for i, feature in enumerate(v[1]):
                if i != 0:
                    condition = condition + ' && '
                    # Here we do the "or"
                features2 = feature.split(',')
                condition2 = ""
                for i2, feature2 in enumerate(features2):
                    if i2 != 0:
                        condition2 = condition2 + ' || '
                    condition2 = condition2 + structName + '.' + feature2 + ' == VK_FALSE'
                condition = condition + '( ' + condition2 + ' )'
            condition = condition + ' )'
            stream.append('\t\t' + condition)
        featureSet = " or ".join(v[1])
        stream.extend(['\t\t{',
                       '\t\t\tlog << tcu::TestLog::Message << "Mandatory feature ' + featureSet + ' not supported" << tcu::TestLog::EndMessage;',
                       '\t\t\tresult = false;',
                       '\t\t}'])
        stream.append('\t}')
        stream.extend([''])

    last_extension = None
    for extension, requirements, mandatory_variant in extData:
        if last_extension != extension:
            stream.append('\t// ' + extension + '\n')
            last_extension = extension
        if not isSupportedByCurrentApi(mandatory_variant):
            continue
        if len(requirements) > 0 :
            condition = 'if ( '
            for i, req in enumerate(requirements) :
                if (req.startswith("ApiVersion")):
                    condition = condition + 'context.contextSupports(vk::' + req + ')'
                elif '.' in req:
                    condition = condition + req
                else:
                    condition = condition + 'isExtensionStructSupported(deviceExtensions, RequiredExtension("' + req + '"))'
                if i+1 < len(requirements) :
                    condition = condition + ' && '
            condition = condition + ' )'
            stream.append('\t' + condition)
        stream.append('\t{')
        stream.extend(['\t\tif (!(isExtensionStructSupported(deviceExtensions, RequiredExtension("' + extension + '")) || isCoreDeviceExtension(usedApiVersion, "' + extension + '")))',
                       '\t\t{',
                       '\t\t\tlog << tcu::TestLog::Message << "Mandatory extension ' + extension + ' not supported" << tcu::TestLog::EndMessage;',
                       '\t\t\tresult = false;',
                       '\t\t}',
                       '\t}'])
        stream.append('')

    stream.append('\treturn result;')
    stream.append('}\n')

    writeInlFile(filename, INL_HEADER, stream)

def writeExtensionList(api, filename, extensionType):
    extensionList = []
    for extensionName, data in api.additionalExtensionData:
        # make sure extension name starts with VK_KHR
        if not extensionName.startswith('VK_KHR'):
            continue
        # make sure that this extension was registered
        if 'register_extension' not in data.keys():
            continue
        # skip extensions that are not supported in Vulkan SC
        if api.apiName == 'vulkansc':
            if any(ext.name == extensionName for ext in api.notSupportedExtensions):
                continue
        # make sure extension is intended for the vulkan variant
        is_sc_only = False

        if api.apiName != 'vulkansc':
            if 'mandatory_features' in data.keys():
                for structure, listStruct in data['mandatory_features'].items():
                    for featureData in listStruct:
                        mandatory_variant = ''
                        try:
                            mandatory_variant = featureData['mandatory_variant']
                        except KeyError:
                            mandatory_variant = ''
                        # VKSC only
                        if 'vulkansc' in mandatory_variant and len(mandatory_variant) == 1:
                            is_sc_only = True
        if is_sc_only:
            continue

        # make sure extension has proper type
        if extensionType == data['register_extension']['type']:
            extensionList.append(extensionName)
    extensionList.sort()
    # write list of all found extensions
    stream = []
    stream.append('static const char* s_allowed{0}KhrExtensions[] =\n{{'.format(extensionType.title()))
    for n in extensionList:
        stream.append('\t"' + n + '",')
    stream.append('};\n')
    writeInlFile(filename, INL_HEADER, stream)

def writeApiExtensionDependencyInfo(api, filename):

    def genHelperFunctions():
        yield 'using namespace tcu;'
        yield 'using ExtPropVect = std::vector<vk::VkExtensionProperties>;'
        yield 'using IsSupportedFun = bool (*)(const tcu::UVec2&, const ExtPropVect&, const ExtPropVect&);'
        yield 'using DependencyCheckVect = std::vector<std::pair<const char*, IsSupportedFun> >;\n'
        yield 'bool isCompatible(uint32_t major, uint32_t minor, const tcu::UVec2& testedApiVersion)'
        yield '{'
        yield '\t// return true when tested api version is greater'
        yield '\t// or equal to version represented by two uints'
        yield '\tif (major == testedApiVersion.x())'
        yield '\t\treturn minor <= testedApiVersion.y();'
        yield '\treturn major < testedApiVersion.x();'
        yield '}\n'
        yield 'bool isSupported(const ExtPropVect& extensions, const char* ext)'
        yield '{'
        yield '\treturn isExtensionStructSupported(extensions, vk::RequiredExtension(ext));'
        yield '}\n'

    def genExtDepArray(extType):
        extensionList = []
        maxExtLength = 0
        extVector = 'vIEP'
        othVector = 'vDEP'
        if extType == 'device':
            extVector, othVector = othVector, extVector        # swap
        # iterate over all extension that are of specified type and that have requirements
        for ext in api.extensions:
            if ext.type != extType:
                continue
            if ext.depends is None:
                continue
            # memorize extension name and dependencies for future vector generation
            extensionList.append(ext.name)
            # memorize max extension name and dependency length
            maxExtLength = max(maxExtLength, len(ext.name))
            # generate check function for this extension
            yield f'bool check_{ext.name}(const tcu::UVec2& v, const ExtPropVect& vIEP, const ExtPropVect& vDEP)'
            yield '{'
            # check if extension was promoted; for SC we need to check vulkan version as sc10 is based on vk12
            if ext.promotedto is not None and 'VK_VERSION' in ext.promotedto:
                p = ext.promotedto
                yield f'\tif (isCompatible({p[-3]}, {p[-1]}, v))'
                yield '\t\treturn true;\n'
            else:
                yield '\tDE_UNREF(v);'
            # there is a high chance that other vector won't be used
            yield f'\tDE_UNREF({othVector});'
            # check if extension is supported
            yield f'\n\tif (!isSupported({extVector}, "{ext.name}"))'
            yield '\t\treturn true;\n'
            # replace dependent extensions/versions with proper conditions
            finalCondition = transformDependsToCondition(ext.depends, api, 'isCompatible(%s, %s, v)', 'isSupported(%s, "%s")', ext.name)
            yield f'\t// depends attribute in xml: {ext.depends}'
            yield f'\treturn {finalCondition};'
            yield '}\n'
        # save list of all device/instance extensions
        yield 'static const DependencyCheckVect {}ExtensionDependencies'.format(extType)
        yield '{'
        for ext in extensionList:
            extTabCount = (maxExtLength - len(ext)) / 4
            eTabs = '\t'*int(round(extTabCount+1.49))
            yield f'\tstd::make_pair("{ext}",{eTabs}&check_{ext}),'
        yield '};\n'

    def genApiVersions():
        yield 'static const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>\treleasedApiVersions[]\t='
        yield '{'
        for f in reversed(api.features):
            apis = f.api.split(',')
            if not api.apiName in apis:
                continue;
            apiVariant = '0' if api.apiName == 'vulkan' else '1'
            major, minor = f.number.split('.')
            version = (int(apiVariant) << 29) | (int(major) << 22) | (int(minor) << 12)
            yield '\tstd::make_tuple({}, {}, {}, {}),'.format(version, apiVariant, major, minor)
        yield '};'

    def parseExtensionDependencies(extDeps, ext):
        major, minor = 1, 0
        requiredVerFound = False;
        # return in case nothing more left to be processed
        if extDeps is None or extDeps == "":
            return major, minor, requiredVerFound
        ungrpPartLen = 0
        versionPattern = "[A-Z]+_VERSION_([0-9]+)_([0-9]+)"
        ungroupedPattern = r"^.*?\(+|^.*?$"
        # look for non-grouped part, it may include the required vulkan version
        ungroupPart = re.search(ungroupedPattern, extDeps)
        if ungroupPart is not None and ungroupPart[0].replace(r"(", "") != "":
            ungrpPartLen = len(ungroupPart[0].replace(r"(", ""))
            # is specific version explicitly requested?
            match = re.search(versionPattern, ungroupPart[0])
            if match is not None:
                if len(match[0]) != len(extDeps):
                    # there is more than just a version; check if it's accompanied by AND operator(s)
                    ext_pattern = r".*\+*"+versionPattern+r"\++.*|.*\++"+versionPattern+r"\+*.*"
                    match = re.search(ext_pattern, ungroupPart[0])
                if match is not None:
                    # specific version is explicitly requested
                    major, minor = int(match[1]), int(match[2])
                    return major, minor, True
            # no explicit version is requested, continue parsing the remaining part
            extDeps = extDeps[ungrpPartLen:]
        groupedPattern = r"(.*)\+|(.*)$"
        match = re.search(groupedPattern, extDeps)
        if match is not None and match[0] != "":
            # groups may include the dependency "promoted to" versions accompanied by OR operator
            # but they don't include the extension explicit required version; continue parsing the remaining part
            groupLength = len(match[0])
            major, minor, requiredVerFound = parseExtensionDependencies(extDeps[groupLength:], ext)
        return major, minor, requiredVerFound

    def genRequiredCoreVersions():
        yield 'static const std::tuple<uint32_t, uint32_t, const char*>\textensionRequiredCoreVersion[]\t ='
        yield '{'
        versionPattern = "[A-Z]+_VERSION_([0-9]+)_([0-9]+)"
        for ext in api.extensions:
            # skip video extensions
            if 'vulkan_video_' in ext.name:
                continue
            major, minor = 1, 0
            if ext.depends is not None:
                major, minor, requiredVerFound = parseExtensionDependencies(ext.depends, ext)
                if not requiredVerFound:
                    # find all extensions that are dependencies of this one
                    matches = re.findall(r"VK_\w+", ext.depends, re.M)
                    for m in matches:
                        for de in api.extensions:
                            if de.name == m:
                                if de.depends is not None:
                                    # check if the dependency states explicitly the required vulkan version and pick the higher one
                                    newMajor, newMinor, requiredVerFound = parseExtensionDependencies(de.depends, de)
                                    if requiredVerFound:
                                        if newMajor > major:
                                            major, minor = newMajor, newMinor
                                        elif newMajor == major and newMinor > minor:
                                            minor = newMinor
                                break
            yield '\tstd::make_tuple({}, {}, "{}"),'.format(major, minor, ext.name)
        yield '};'

    stream = []
    stream.extend(genHelperFunctions())
    stream.extend(genExtDepArray('instance'))
    stream.extend(genExtDepArray('device'))
    stream.extend(genApiVersions())
    stream.extend(genRequiredCoreVersions())

    writeInlFile(filename, INL_HEADER, stream)

def writeEntryPointValidation(api, filename):
    # keys are instance extension names and value is list of device-level functions
    instExtDeviceFunDict = {}
    # iterate over all extensions and find instance extensions
    for ext in api.extensions:
        if ext.type == "instance":
            # iterate over all functions instance extension adds
            for requirement in ext.requirementsList:
                for extCommand in requirement.newCommands:
                    # to get a type of command we need to find this command definition in list of all functions
                    for command in api.functions:
                        if extCommand.name == command.name or extCommand.name in command.aliasList:
                            # check if this is device-level entry-point
                            if command.getType() == Function.TYPE_DEVICE:
                                if ext.name not in instExtDeviceFunDict:
                                    instExtDeviceFunDict[ext.name] = []
                                instExtDeviceFunDict[ext.name].append(extCommand.name)
    stream = ['std::map<std::string, std::vector<std::string> > instExtDeviceFun', '{']
    for extName in instExtDeviceFunDict:
        stream.append(f'\t{{ "{extName}",\n\t\t{{')
        for fun in instExtDeviceFunDict[extName]:
            stream.append(f'\t\t\t"{fun}",')
        stream.append('\t\t}\n\t},')
    stream.append('};')
    writeInlFile(filename, INL_HEADER, stream)

def writeGetDeviceProcAddr(api, filename):
    testBlockStart = '''tcu::TestStatus        testGetDeviceProcAddr        (Context& context)
{
    tcu::TestLog&                                log                        (context.getTestContext().getLog());
    const PlatformInterface&                    platformInterface = context.getPlatformInterface();
    const auto                                    validationEnabled = context.getTestContext().getCommandLine().isValidationEnabled();
    const CustomInstance                        instance                (createCustomInstanceFromContext(context));
    const InstanceDriver&                        instanceDriver = instance.getDriver();
    const VkPhysicalDevice                        physicalDevice = chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t                                queueFamilyIndex = 0;
    const uint32_t                                queueCount = 1;
    const float                                    queuePriority = 1.0f;
    const std::vector<VkQueueFamilyProperties>    queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    const VkDeviceQueueCreateInfo            deviceQueueCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //  VkStructureType sType;
        nullptr, //  const void* pNext;
        (VkDeviceQueueCreateFlags)0u, //  VkDeviceQueueCreateFlags flags;
        queueFamilyIndex, //  uint32_t queueFamilyIndex;
        queueCount, //  uint32_t queueCount;
        &queuePriority, //  const float* pQueuePriorities;
    };

    const VkDeviceCreateInfo                deviceCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //  VkStructureType sType;
        nullptr, //  const void* pNext;
        (VkDeviceCreateFlags)0u, //  VkDeviceCreateFlags flags;
        1u, //  uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfo, //  const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u, //  uint32_t enabledLayerCount;
        nullptr, //  const char* const* ppEnabledLayerNames;
        0u, //  uint32_t enabledExtensionCount;
        nullptr, //  const char* const* ppEnabledExtensionNames;
        nullptr, //  const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };
    const Unique<VkDevice>                    device            (createCustomDevice(validationEnabled, platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
    const DeviceDriver                        deviceDriver    (platformInterface, instance, device.get(), context.getUsedApiVersion(), context.getTestContext().getCommandLine());

    const std::vector<std::string> functions{'''
    testBlockEnd = '''    };

    bool fail = false;
    for (const auto& function : functions)
    {
        if (deviceDriver.getDeviceProcAddr(device.get(), function.c_str()) != nullptr)
        {
            fail = true;
            log << tcu::TestLog::Message << "Function " << function << " is not NULL" << tcu::TestLog::EndMessage;
        }
    }
    if (fail)
        return tcu::TestStatus::fail("Fail");
    return tcu::TestStatus::pass("All functions are NULL");
}
'''

    def functions(functionType):
        for ext in api.extensions:
            for requirement in ext.requirementsList:
                for requiredCommand in requirement.newCommands:
                    yield '\t\t"' + requiredCommand.name + '",'
    stream = []
    stream.append('#include "tcuCommandLine.hpp"')
    stream.append('#include "vktTestCase.hpp"')
    stream.append('#include "vkPlatform.hpp"')
    stream.append('#include "vkDeviceUtil.hpp"')
    stream.append('#include "vkQueryUtil.hpp"')
    stream.append('#include "vktCustomInstancesDevices.hpp"')
    stream.append('#include "vktTestCase.hpp"')
    stream.append('#include "vktTestCaseUtil.hpp"')
    stream.append('\nnamespace vkt\n{\n')
    stream.append('using namespace vk;\n')
    stream.append(testBlockStart)
    stream.extend(functions(api))
    stream.append(testBlockEnd)

    # function to create tests
    stream.append("void addGetDeviceProcAddrTests (tcu::TestCaseGroup* testGroup)\n{")
    stream.append('\taddFunctionCase(testGroup, "non_enabled", testGetDeviceProcAddr);')
    stream.append('}\n')
    stream.append('}\n')

    writeInlFile(filename, INL_HEADER, stream)

def writeProfileTests(inlFileName, jsonFilesList):

    # helper function; workaround for lack of information in json about limit type
    def getLimitMacro(propName, propComponent):
        maxUintPropNames = ["bufferImageGranularity", "storageTexelBufferOffsetAlignmentBytes",\
                            "robustUniformBufferAccessSizeAlignment", "shaderWarpsPerSM",\
                            "perViewPositionAllComponents", "minTexelBufferOffsetAlignment",\
                            "minUniformBufferOffsetAlignment"]
        minFloatPropNames = ["maxSamplerLodBias"]
        maxFloatPropNames = ["pointSizeGranularity", "lineWidthGranularity"]
        minDevSizePropNames = ["maxBufferSize"]
        if propName in maxUintPropNames:
            return "LIM_MAX_UINT32"
        elif propName in minFloatPropNames:
            return "LIM_MIN_FLOAT"
        elif propName in maxFloatPropNames:
            return "LIM_MAX_FLOAT"
        elif propName in minDevSizePropNames:
            return "LIM_MIN_DEVSIZE"
        elif propName.endswith("SampleCounts"):
            return "LIM_MIN_BITI32"
        elif not propName.startswith("max") and propName.endswith("Range"):
            return "LIM_MAX_FLOAT" if propComponent == 0 else "LIM_MIN_FLOAT"
        return "LIM_MIN_UINT32"

    # helper function that adds property or feature structures to lists of struct initializers
    def constructStruct(structName, structInitNamesList, structInitList):
        # skip structures that already are in the chain
        if structName in structInitNamesList:
            return
        structInitNamesList.append(structName)
        # construct structure instance and connect it to chain
        parentStruct = "" if (len(structInitNamesList) == 3) else "&vk" + structInitNamesList[-2]
        structInitList.append(f"\tVkPhysicalDevice{structName} vk{structName} = initVulkanStructure({parentStruct});")

    # helper function handling strings representing property limit checks
    def addPropertyEntries(structName, propName, propLimit, propertyTableItems):
        if propName == "driverName":
            return
        propSubItems = [(propName, propLimit)]
        combinedStructName = structName
        # checkk if propLimit is actualy a dictionary this will be the case when propName is "limits";
        # in that case we have to get all sub items and add all of them to propertyTableItems
        if isinstance(propLimit, dict):
            propSubItems = propLimit.items()
            combinedStructName += "." + propName
        # usualy we will add just one item but we need to handle cases where there is more
        for name, limit in propSubItems:
            limitComponentCount = 1
            if isinstance(limit, list):
                limitComponentCount = len(limit)
                # handle special case like storageImageSampleCounts
                if limitComponentCount == 1:
                   limit = limit[0]
            componentAccessFormat = ""
            if limitComponentCount > 1:
                # if limit is list of strings joint them together;
                # e.g. this is the case for subgroupSupportedStages
                if isinstance(limit[0], str):
                    limitComponentCount = 1
                    limit = "|".join(limit)
                else:
                    componentAccessFormat = "[{}]"
            # handle case where limit is represented by more than one value;
            # in that case we will add as many entries to propertyTableItems as there are limiting values
            for i in range(limitComponentCount):
                componentAccess = componentAccessFormat.format(i)
                limitMacro = getLimitMacro(name, i)
                limitValue = "true" if limit == True else limit
                if limitValue == False:
                     limitValue = "false"
                limitValue = limitValue[i] if limitComponentCount > 1 else limitValue
                propertyTableItems += [f"PN({combinedStructName}.{name}{componentAccess}), {limitMacro}({limitValue})"]

    vkpdLen = len("VkPhysicalDevice")
    profilesList = []
    stream = []

    for jsonFile in jsonFilesList:
        jsonContent = readFile(jsonFile)
        profilesDict = json.loads(jsonContent)
        capabilitiesDefinitionsDict = profilesDict["capabilities"]

        for profileName, profileData in reversed(profilesDict["profiles"].items()):
            featureStructInitList = []
            featureStructInitNamesList = ["Features", "Features2"]
            featureTableItems = []
            propertyStructInitList = []
            propertyStructInitNamesList = ["Properties", "Properties2"]
            propertyTableItems = []
            extensionList = []
            formatsList = []
            highestMajor = 1
            highestMinor = 0

            allCapabilities = profileData["capabilities"] + profileData.get("optionals", [])
            for capability in allCapabilities:
                capabilityList = capability if isinstance(capability, list) else [capability]
                for capabilityName in capabilityList:
                    capabilityDefinition = capabilitiesDefinitionsDict[capabilityName]
                    # identify highest required vulkan version
                    match = re.match(r"vulkan(\d)(\d)requirements", capabilityName)
                    if match is not None:
                        major, minor = int(match.group(1)), int (match.group(2))
                        if major*10 + minor > highestMajor * 10 + highestMinor:
                            highestMajor, highestMinor = major, minor
                    if "features" in capabilityDefinition:
                        featureStructList = capabilityDefinition["features"]
                        # skip adding comment for empty requirements
                        if len(featureStructList) == 1 and not list(featureStructList.values())[0]:
                            continue
                        featureTableItems.append(f"\t\t// {capabilityName}");
                        # iterate over required features
                        for featureStruct in featureStructList:
                            structName = featureStruct[vkpdLen:]
                            constructStruct(structName, featureStructInitNamesList, featureStructInitList)
                            for feature in featureStructList[featureStruct]:
                                featureTableItems.append(f"vk{structName}, {feature}")
                            featureTableItems.append("")
                    if "properties" in capabilityDefinition:
                        propertyStructList = capabilityDefinition["properties"]
                        propertyTableItems.append(f"\t\t// {capabilityName}");
                        for propertyStruct in propertyStructList:
                            structName = propertyStruct[vkpdLen:]
                            constructStruct(structName, propertyStructInitNamesList, propertyStructInitList)
                            for propName, propLimit in propertyStructList[propertyStruct].items():
                                addPropertyEntries("vk" + structName, propName, propLimit, propertyTableItems)
                            propertyTableItems.append("")
                    if "extensions" in capabilityDefinition:
                        extensionList = [n for n in capabilityDefinition["extensions"]]
                    if "formats" in capabilityDefinition:
                        formatsList = capabilityDefinition["formats"]

            # remove empty lines at the end
            featureTableItems.pop()
            propertyTableItems.pop()

            # remove "VP_KHR_" from roadmap profile name
            if "VP_KHR_" in profileName:
                profileName = profileName[7:]
            # lower letters for all profile names
            profileName = profileName.lower()

            # template used to get both device features and device properties
            structGetterTemplate = "\n"\
            "\tVkPhysicalDevice{0}2 vk{0}2 = initVulkanStructure(&vk{2});\n"\
            "\tauto& vk{0} = vk{0}2.{1};\n"\
            "\tvki.getPhysicalDevice{0}2(pd, &vk{0}2);\n"

            # construct function that will validate profile
            stream.append(f"tcu::TestStatus validate_{profileName}(Context& context)")

            stream.append("{\n"
            "\tconst VkBool32 checkAlways = true;\n"
            "\tbool oneOrMoreChecksFailed = false;\n"
            "\tauto pd = context.getPhysicalDevice();\n"
            "\tconst auto &vki = context.getInstanceInterface();\n"
            "\tTestLog& log = context.getTestContext().getLog();\n")

            stream.extend(featureStructInitList)
            stream.append(structGetterTemplate.format("Features", "features", featureStructInitNamesList[-1]))
            stream.extend(propertyStructInitList)
            stream.append(structGetterTemplate.format("Properties", "properties", propertyStructInitNamesList[-1]))
            if len(featureTableItems):
                stream.append("\tconst std::vector<FeatureEntry> featureTable {")
                stream.extend(["\t\tROADMAP_FEATURE_ITEM(" + f + ")," if ("," in f) else f for f in featureTableItems])
                stream.append("\t};\n"
                "\tfor (const auto &testedFeature : featureTable)\n"
                "\t{\n"
                "\t    if (!testedFeature.fieldPtr[0])\n"
                "\t    {\n"
                "\t        log << TestLog::Message\n"
                "\t            << \"Feature \" << testedFeature.fieldName << \" is not supported\"\n"
                "\t            << TestLog::EndMessage;\n"
                "\t        oneOrMoreChecksFailed = true;\n"
                "\t    }\n"
                "\t}\n")
            if len(propertyTableItems):
                stream.append("\tconst std::vector<FeatureLimitTableItem> propertyTable {")
                stream.extend(["\t\t{ PN(checkAlways), " + p + " }," if ("," in p) else p for p in propertyTableItems])
                stream.append("\t};\n"
                "\tfor (const auto& testedProperty : propertyTable)\n"
                "\t    oneOrMoreChecksFailed |= !validateLimit(testedProperty, log);\n")
            if len(extensionList):
                stream.append("\tstd::vector<std::string> extensionList {")
                stream.append('\t\t"' + '",\n\t\t"'.join(extensionList) + '"')
                stream.append("\t};\n"
                "\tconst auto deviceExtensions = enumerateDeviceExtensionProperties(vki, pd, nullptr);\n"
                "\tfor (const auto& testedExtension : extensionList)\n"
                "\t{\n"
                "\t    if (isExtensionStructSupported(deviceExtensions, RequiredExtension(testedExtension)) ||\n"
                "\t        context.isInstanceFunctionalitySupported(testedExtension))\n"
                "\t        continue;\n"
                "\t    log << TestLog::Message\n"
                "\t        << testedExtension << \" is not supported\"\n"
                "\t        << TestLog::EndMessage;\n"
                "\t    oneOrMoreChecksFailed = true;\n"
                "\t}")
            if len(formatsList):
                stream.append("\n\tstd::vector<FormatEntry> formatsList {")
                for formatName, formatProperties in formatsList.items():
                    formatProperties = formatProperties["VkFormatProperties"]
                    linearTilingFeatures = formatProperties["linearTilingFeatures"]
                    linearTilingFeatures = "0" if not linearTilingFeatures else linearTilingFeatures
                    optimalTilingFeatures = formatProperties["optimalTilingFeatures"]
                    optimalTilingFeatures = "0" if not optimalTilingFeatures else optimalTilingFeatures
                    bufferFeatures = formatProperties["bufferFeatures"]
                    bufferFeatures = "0" if not bufferFeatures else bufferFeatures
                    stream.append(f"""\t\t{{ {formatName}, "{formatName}",
            {{ {"|".join(linearTilingFeatures)},
              {"|".join(optimalTilingFeatures)},
              {"|".join(bufferFeatures)} }} }},""")
                stream.append("\t};\n"
                "\t\tVkFormatProperties supportedFormatPropertiess;\n"
                "\t\tfor (const auto& [f, fn, fp] : formatsList)\n"
                "\t\t{\n"
                "\t\t    vki.getPhysicalDeviceFormatProperties(pd, f, &supportedFormatPropertiess);\n"
                "\t\t    if (((fp.linearTilingFeatures & supportedFormatPropertiess.linearTilingFeatures) == fp.linearTilingFeatures) &&\n"
                "\t\t        ((fp.optimalTilingFeatures & supportedFormatPropertiess.optimalTilingFeatures) == fp.optimalTilingFeatures) &&\n"
                "\t\t        ((fp.bufferFeatures & supportedFormatPropertiess.bufferFeatures) == fp.bufferFeatures))\n"
                "\t\t        continue;\n"
                "\t\t    log << TestLog::Message\n"
                "\t\t        << \"Required format properties for \" << fn << \" are not supported\"\n"
                "\t\t        << TestLog::EndMessage;\n"
                "\t\t    oneOrMoreChecksFailed = true;\n"
                "\t\t}\n")

            stream.append("\n"
            "\tif (oneOrMoreChecksFailed)\n"
            "\t    TCU_THROW(NotSupportedError, \"Profile not supported\");\n"
            "\treturn tcu::TestStatus::pass(\"Profile supported\");\n}\n")

            profilesList.append(f"\t{{ \"{profileName}\", checkApiVersionSupport<{highestMajor}, {highestMinor}>, validate_{profileName} }},")

    # save list of all callbacks
    stream.append("static const std::vector<ProfileEntry> profileEntries {")
    stream.extend(profilesList)
    stream.append("};")

    writeInlFile(inlFileName, INL_HEADER, stream)

def writeConformanceVersions(api, filename):
    logging.debug("Preparing to generate " + filename)
    # get list of all vulkan/vulkansc tags from git
    remote_urls = os.popen("git remote -v").read().split('\n')
    remote_url = None
    url_regexp = r'\bgerrit\.khronos\.org\b.*\bvk-gl-cts\b'
    for line in remote_urls:
        if re.search(url_regexp, line, re.IGNORECASE) is not None:
            remote_url = line.split()[1]
            break
    listOfTags = os.popen("git ls-remote -t %s" % (remote_url)).read()
    pattern = r"vulkan-cts-(\d+).(\d+).(\d+).(\d+)"
    if args.api == 'SC':
        pattern = r"vulkansc-cts-(\d+).(\d+).(\d+).(\d+)"
    matches = re.findall(pattern, listOfTags, re.M)
    matches = sorted([tuple(map(int, tup)) for tup in matches])
    if len(matches) == 0:
        return

    # read all text files in doc folder and find withdrawn cts versions (branches)
    withdrawnBranches = set()
    today = datetime.date.today()
    docFiles = glob.glob(os.path.join(os.path.dirname(__file__), "..", "doc", "*.txt"))
    for fileName in docFiles:
        if "withdrawal" not in fileName:
            continue
        fileContent = readFile(fileName)
        # get date when releases are withdrawn
        match = re.search(r"(20\d\d)-(\d\d)-(\d\d).+ withdrawn", fileContent, re.IGNORECASE)
        if match is not None:
            # check if announcement refers to date in the past
            if today > datetime.date(int(match[1]), int(match[2]), int(match[3])):
                # get names of withdrawn branches
                branchMatches = re.findall(pattern, fileContent, re.M)
                branchMatches = [tuple(map(int, tup)) for tup in branchMatches]
                for v in branchMatches:
                    withdrawnBranches.add((v[0], v[1], v[2], v[3]))
    # define helper function that will be used to add entries for both vk and sc
    def appendToStream(stream, versionsToAdd, maxWithdrawnVersion):
        addedVersions = set()
        for v in reversed(versionsToAdd):
            # add only unique versions; ignore duplicates (e.g. with "-rc1", "-rc2" postfix);
            # also add versions that are greater than maximal withdrawn version
            if v in addedVersions or v <= maxWithdrawnVersion:
                continue
            addedVersions.add(v)
            stream.append(f'\tmakeConformanceVersion({v[0]}, {v[1]}, {v[2]}, {v[3]}),')
    # save array with versions
    stream = ['static const VkConformanceVersion knownConformanceVersions[]',
              '{']
    appendToStream(stream, matches, tuple('0'*4) if len(withdrawnBranches) == 0 else max(withdrawnBranches))
    stream.append('};')
    writeInlFile(filename, INL_HEADER, stream)

def parseCmdLineArgs():
    parser = argparse.ArgumentParser(description = "Generate Vulkan INL files",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-a",
                        "--api",
                        dest="api",
                        default="",
                        help="Choose between Vulkan and Vulkan SC")
    parser.add_argument("-o",
                        "--outdir",
                        dest="outdir",
                        default="",
                        help="Choose output directory")
    parser.add_argument("-v", "--verbose",
                        dest="verbose",
                        action="store_true",
                        help="Enable verbose logging")
    return parser.parse_args()

if __name__ == "__main__":
    args = parseCmdLineArgs()
    initializeLogger(args.verbose)

    # if argument was specified it is interpreted as a path to which .inl files will be written
    outputPath = DEFAULT_OUTPUT_DIR[args.api] if args.outdir == '' else args.outdir

    vkTree = etree.parse(os.path.join(VULKAN_XML_DIR, "vk.xml"))
    apiName = "vulkansc" if args.api == 'SC' else "vulkan"

    # Read vk.xml and generate vulkan headers from it
    api = API(apiName)
    api.build(vkTree)
    api.postProcess()

    # Read video.xml
    if args.api != 'SC':
        api.build( etree.parse(os.path.join(VULKAN_XML_DIR, "video.xml")) )

    platformFuncs = [Function.TYPE_PLATFORM]
    instanceFuncs = [Function.TYPE_INSTANCE]
    deviceFuncs = [Function.TYPE_DEVICE]

    dfd = generateDeviceFeaturesOrPropertiesDefs(api, 'Features')
    writeDeviceFeatures                         (api, dfd, os.path.join(outputPath, "vkDeviceFeatures.inl"))
    writeFeaturesVariant                        (dfd, os.path.join(outputPath, "vkDeviceFeaturesVariantDecl.inl"))
    writeDeviceFeaturesDefaultDeviceDefs        (dfd, os.path.join(outputPath, "vkDeviceFeaturesForDefaultDeviceDefs.inl"))
    writeDeviceFeaturesContextDecl              (dfd, os.path.join(outputPath, "vkDeviceFeaturesForContextDecl.inl"))
    writeDeviceFeaturesContextDefs              (dfd, os.path.join(outputPath, "vkDeviceFeaturesForContextDefs.inl"))
    writeDeviceFeatureTest                      (api, os.path.join(outputPath, "vkDeviceFeatureTest.inl"))

    dpd = generateDeviceFeaturesOrPropertiesDefs(api, 'Properties')
    writeDeviceProperties                       (api, dpd, os.path.join(outputPath, "vkDeviceProperties.inl"))
    writeDevicePropertiesDefaultDeviceDefs      (dpd, os.path.join(outputPath, "vkDevicePropertiesForDefaultDeviceDefs.inl"))
    writeDevicePropertiesContextDecl            (dpd, os.path.join(outputPath, "vkDevicePropertiesForContextDecl.inl"))
    writeDevicePropertiesContextDefs            (dpd, os.path.join(outputPath, "vkDevicePropertiesForContextDefs.inl"))

    writeHandleType                             (api, os.path.join(outputPath, "vkHandleType.inl"))
    writeBasicTypes                             (api, os.path.join(outputPath, "vkBasicTypes.inl"))
    writeCompositeTypes                         (api, os.path.join(outputPath, "vkStructTypes.inl"))
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkVirtualPlatformInterface.inl"), platformFuncs, False)
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkVirtualInstanceInterface.inl"), instanceFuncs, False)
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkVirtualDeviceInterface.inl"), deviceFuncs, False)
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkConcretePlatformInterface.inl"), platformFuncs, True)
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkConcreteInstanceInterface.inl"), instanceFuncs, True)
    writeInterfaceDecl                          (api, os.path.join(outputPath, "vkConcreteDeviceInterface.inl"), deviceFuncs, True)
    writeFunctionPtrTypes                       (api, os.path.join(outputPath, "vkFunctionPointerTypes.inl"))
    writeFunctionPointers                       (api, os.path.join(outputPath, "vkPlatformFunctionPointers.inl"), platformFuncs)
    writeFunctionPointers                       (api, os.path.join(outputPath, "vkInstanceFunctionPointers.inl"), instanceFuncs)
    writeFunctionPointers                       (api, os.path.join(outputPath, "vkDeviceFunctionPointers.inl"), deviceFuncs)
    writeInitFunctionPointers                   (api, os.path.join(outputPath, "vkInitPlatformFunctionPointers.inl"), platformFuncs, lambda f: f.name != "vkGetInstanceProcAddr")
    writeInitFunctionPointers                   (api, os.path.join(outputPath, "vkInitInstanceFunctionPointers.inl"), instanceFuncs)
    writeInitFunctionPointers                   (api, os.path.join(outputPath, "vkInitDeviceFunctionPointers.inl"), deviceFuncs)
    writeFuncPtrInterfaceImpl                   (api, os.path.join(outputPath, "vkPlatformDriverImpl.inl"), platformFuncs, "PlatformDriver")
    writeFuncPtrInterfaceImpl                   (api, os.path.join(outputPath, "vkInstanceDriverImpl.inl"), instanceFuncs, "InstanceDriver")
    writeFuncPtrInterfaceImpl                   (api, os.path.join(outputPath, "vkDeviceDriverImpl.inl"), deviceFuncs, "DeviceDriver")
    writeStrUtilProto                           (api, os.path.join(outputPath, "vkStrUtil.inl"))
    writeStrUtilImpl                            (api, os.path.join(outputPath, "vkStrUtilImpl.inl"))
    writeRefUtilProto                           (api, os.path.join(outputPath, "vkRefUtil.inl"))
    writeRefUtilImpl                            (api, os.path.join(outputPath, "vkRefUtilImpl.inl"))
    writeStructTraitsImpl                       (api, os.path.join(outputPath, "vkGetStructureTypeImpl.inl"))
    writeNullDriverImpl                         (api, os.path.join(outputPath, "vkNullDriverImpl.inl"))
    writeTypeUtil                               (api, os.path.join(outputPath, "vkTypeUtil.inl"))
    writeSupportedExtensions                    (api, os.path.join(outputPath, "vkSupportedExtensions.inl"))
    writeCoreFunctionalities                    (api, os.path.join(outputPath, "vkCoreFunctionalities.inl"))
    writeExtensionFunctions                     (api, os.path.join(outputPath, "vkExtensionFunctions.inl"))
    writeDeviceFeatures2                        (api, os.path.join(outputPath, "vkDeviceFeatures2.inl"))
    writeMandatoryFeatures                      (api, os.path.join(outputPath, "vkMandatoryFeatures.inl"))
    writeExtensionList                          (api, os.path.join(outputPath, "vkInstanceExtensions.inl"), 'instance')
    writeExtensionList                          (api, os.path.join(outputPath, "vkDeviceExtensions.inl"), 'device')
    writeDriverIds                              (api, os.path.join(outputPath, "vkKnownDriverIds.inl"))
    writeObjTypeImpl                            (api, os.path.join(outputPath, "vkObjTypeImpl.inl"))
    writeApiExtensionDependencyInfo             (api, os.path.join(outputPath, "vkApiExtensionDependencyInfo.inl"))
    writeEntryPointValidation                   (api, os.path.join(outputPath, "vkEntryPointValidation.inl"))
    writeGetDeviceProcAddr                      (api, os.path.join(outputPath, "vkGetDeviceProcAddr.inl"))
    writeConformanceVersions                    (api, os.path.join(outputPath, "vkKnownConformanceVersions.inl"))
    if args.api=='SC':
        writeFuncPtrInterfaceSCImpl(api, os.path.join(outputPath, "vkDeviceDriverSCImpl.inl"), deviceFuncs, "DeviceDriverSC")
    else:
        profileList = [os.path.join(VULKAN_XML_DIR, "profiles", "VP_KHR_roadmap.json")]
        #profileList += [os.path.join(VULKAN_XML_DIR, "profiles", "VP_ANDROID_baseline_2022.json"]
        writeProfileTests(os.path.join(outputPath, "vkProfileTests.inl"), profileList)

    # NOTE: when new files are generated then they should also be added to the
    # vk-gl-cts\external\vulkancts\framework\vulkan\CMakeLists.txt outputs list
