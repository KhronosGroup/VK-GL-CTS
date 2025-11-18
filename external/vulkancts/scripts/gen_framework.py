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
import ast
import logging
from lxml import etree
from dataclasses import dataclass
from typing import Any

scriptPath = os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts")
sys.path.insert(0, scriptPath)

from ctsbuild.common import *
from khr_util.format import indentLines, combineLines

VULKAN_XML_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "vulkan-docs", "src", "xml")
SCRIPTS_SRC_DIR = os.path.join(os.path.dirname(__file__), "src")
DEFAULT_OUTPUT_DIR = { "" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkan"),
                       "SC" : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkansc") }

vulkanObjectPath = os.path.join(VULKAN_XML_DIR, "..", "scripts")
sys.path.insert(0, vulkanObjectPath)

from reg import Registry
from base_generator import BaseGenerator, BaseGeneratorOptions, SetTargetApiName, SetOutputDirectory, SetMergedApiNames, OutputGenerator
from vulkan_object import Struct, Member, Enum, EnumField, Extension

# list of KHR and EXT extensions that are tested by CTS and that were not promoted to core
# (core extensions are implicitly in that list because if they are core we know that tests
# for them must be present in CTS - so we do not need to list them here)
EXTENSIONS_TESTED_BY_CTS = """
VK_EXT_attachment_feedback_loop_dynamic_state
VK_EXT_attachment_feedback_loop_layout
VK_EXT_border_color_swizzle
VK_EXT_buffer_device_address
VK_EXT_color_write_enable
VK_EXT_conditional_rendering
VK_EXT_conservative_rasterization
VK_EXT_custom_border_color
VK_EXT_depth_bias_control
VK_EXT_depth_clamp_control
VK_EXT_depth_clamp_zero_one
VK_EXT_depth_clip_control
VK_EXT_depth_clip_enable
VK_EXT_descriptor_buffer
VK_EXT_device_address_binding_report
VK_EXT_device_fault
VK_EXT_device_generated_commands
VK_EXT_device_memory_report
VK_EXT_dynamic_rendering_unused_attachments
VK_EXT_extended_dynamic_state3
VK_EXT_fragment_density_map
VK_EXT_fragment_density_map2
VK_EXT_fragment_density_map_offset
VK_EXT_fragment_shader_interlock
VK_EXT_frame_boundary
VK_EXT_global_priority_query
VK_EXT_graphics_pipeline_library
VK_EXT_image_2d_view_of_3d
VK_EXT_image_compression_control
VK_EXT_image_compression_control_swapchain
VK_EXT_image_view_min_lod
VK_EXT_index_type_uint8
VK_EXT_legacy_dithering
VK_EXT_legacy_vertex_attributes
VK_EXT_line_rasterization
VK_EXT_memory_decompression
VK_EXT_memory_priority
VK_EXT_mesh_shader
VK_EXT_multi_draw
VK_EXT_multisampled_render_to_single_sampled
VK_EXT_mutable_descriptor_type
VK_EXT_nested_command_buffer
VK_EXT_non_seamless_cube_map
VK_EXT_opacity_micromap
VK_EXT_pageable_device_local_memory
VK_EXT_pipeline_library_group_handles
VK_EXT_present_mode_fifo_latest_ready
VK_EXT_primitive_topology_list_restart
VK_EXT_primitives_generated_query
VK_EXT_provoking_vertex
VK_EXT_rgba10x6_formats
VK_EXT_robustness2
VK_EXT_shader_64bit_indexing
VK_EXT_shader_atomic_float
VK_EXT_shader_atomic_float2
VK_EXT_shader_float8
VK_EXT_shader_image_atomic_int64
VK_EXT_shader_module_identifier
VK_EXT_shader_object
VK_EXT_shader_tile_image
VK_EXT_subpass_merge_feedback
VK_EXT_swapchain_maintenance1
VK_EXT_transform_feedback
VK_EXT_uniform_buffer_unsized_array
VK_EXT_vertex_attribute_divisor
VK_EXT_vertex_input_dynamic_state
VK_EXT_ycbcr_image_arrays
VK_EXT_zero_initialize_device_memory
VK_KHR_acceleration_structure
VK_KHR_android_surface
VK_KHR_calibrated_timestamps
VK_KHR_compute_shader_derivatives
VK_KHR_cooperative_matrix
VK_KHR_copy_memory_indirect
VK_KHR_deferred_host_operations
VK_KHR_depth_clamp_zero_one
VK_KHR_display
VK_KHR_display_swapchain
VK_KHR_external_fence_fd
VK_KHR_external_fence_win32
VK_KHR_external_memory_fd
VK_KHR_external_memory_win32
VK_KHR_external_semaphore_fd
VK_KHR_external_semaphore_win32
VK_KHR_fragment_shader_barycentric
VK_KHR_fragment_shading_rate
VK_KHR_get_display_properties2
VK_KHR_get_surface_capabilities2
VK_KHR_incremental_present
VK_KHR_maintenance7
VK_KHR_maintenance8
VK_KHR_maintenance9
VK_KHR_maintenance10
VK_KHR_mir_surface
VK_KHR_object_refresh
VK_KHR_performance_query
VK_KHR_pipeline_binary
VK_KHR_pipeline_executable_properties
VK_KHR_pipeline_library
VK_KHR_portability_enumeration
VK_KHR_portability_subset
VK_KHR_present_id
VK_KHR_present_id2
VK_KHR_present_mode_fifo_latest_ready
VK_KHR_present_wait
VK_KHR_present_wait2
VK_KHR_ray_query
VK_KHR_ray_tracing_maintenance1
VK_KHR_ray_tracing_pipeline
VK_KHR_ray_tracing_position_fetch
VK_KHR_robustness2
VK_KHR_shader_bfloat16
VK_KHR_shader_clock
VK_KHR_shader_fma
VK_KHR_shader_maximal_reconvergence
VK_KHR_shader_quad_control
VK_KHR_shader_relaxed_extended_instruction
VK_KHR_shader_subgroup_uniform_control_flow
VK_KHR_shader_untyped_pointers
VK_KHR_shared_presentable_image
VK_KHR_surface
VK_KHR_surface_maintenance1
VK_KHR_surface_protected_capabilities
VK_KHR_swapchain
VK_KHR_swapchain_maintenance1
VK_KHR_swapchain_mutable_format
VK_KHR_unified_image_layouts
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
VK_KHR_video_maintenance1
VK_KHR_video_maintenance2
VK_KHR_video_queue
VK_KHR_wayland_surface
VK_KHR_win32_keyed_mutex
VK_KHR_win32_surface
VK_KHR_workgroup_memory_explicit_layout
VK_KHR_xcb_surface
VK_KHR_xlib_surface
""".splitlines()

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

    # VK_EXT_directfb_surface
    (["IDirectFB"], ["IDirectFB"], "int"),
    (["IDirectFBSurface"], ["IDirectFBSurface"], "int"),
]

PLATFORM_TYPE_NAMESPACE = "pt"

TYPE_SUBSTITUTIONS = [
    # Platform-specific
    ("DWORD", "uint32_t"),
    ("HANDLE*", PLATFORM_TYPE_NAMESPACE + "::" + "Win32Handle*"),
]

EXTENSION_POSTFIXES_STANDARD = ["KHR", "EXT"]
EXTENSION_POSTFIXES_VENDOR = ["AMD", "ARM", "NV", 'INTEL', "NVX", "KHX", "NN", "MVK", "FUCHSIA", 'QCOM', "GGP", "QNX", "ANDROID", 'VALVE', 'HUAWEI', 'IMG']
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

def transformSingleDependsConditionToCpp(depPart, vk, checkVersionString, checkExtensionString, extension, depends):
    ret = None
    if 'VK_VERSION' in depPart:
        # when dependency is vulkan version then replace it with proper condition
        ret = checkVersionString % (depPart[-3], depPart[-1])
    else:
        # when dependency is extension check if it was promoted
        for dExt in vk.extensions.values():
            if depPart == dExt.name:
                depExtVector = 'vDEP' if dExt.device else 'vIEP'
                isSupportedCheck = checkExtensionString % (depExtVector, depPart)
                ret = isSupportedCheck
                # This check is just heuristics. In theory we should check if the promotion is actually checked properly
                # in the dependency
                if dExt.promotedTo is not None and dExt.promotedTo not in depends:
                     p = dExt.promotedTo
                     # check if dependency was promoted to vulkan version or other extension
                     if 'VK_VERSION' in p:
                         ret = f'({checkVersionString % (p[-3], p[-1])} || {isSupportedCheck})'
                     else:
                         ret = f'({checkExtensionString % (depExtVector, depPart)} || {isSupportedCheck})'
        if ret is None:
            ret = "false /* UNSUPPORTED CONDITION: " + depPart + "*/"
        if ret is None:
            assert False, f"{depPart} not found: {extension} : {depends}"
    return ret

def transformDependsToCondition(depends, vk, checkVersionString, checkExtensionString, extension):
    tree = parseDependsEpression(depends)
    condition = generateCppDependencyAST(tree, vk, checkVersionString, checkExtensionString, extension, depends)
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

def generateCppDependencyAST(node, vk, checkVersionString, checkExtensionString, extension, depends):
    if isinstance(node, ast.BoolOp):
        parts = [
            generateCppDependencyAST(v, vk, checkVersionString, checkExtensionString, extension, depends)
            for v in node.values
        ]
        op = "&&" if isinstance(node.op, ast.And) else "||"
        # Parenthesize each part, then join with the operator, and wrap the whole
        joined = f" {op} ".join(f"{p}" for p in parts)
        return f"({joined})"

    elif isinstance(node, ast.Name):
        return transformSingleDependsConditionToCpp(
            node.id, vk, checkVersionString, checkExtensionString, extension, depends
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

# helper function that checks if type should be replaced with other type
def substituteType(fullType, baseType):
    for src, dst in TYPE_SUBSTITUTIONS:
        if baseType == src:
            return fullType.replace(baseType, dst)
    for platformType, substitute, _ in PLATFORM_TYPES:
        basePlatformType = platformType[-2] if '*' in platformType else platformType[0]
        if baseType == basePlatformType:
            valueToReplace = ' '.join(platformType).replace(' *', '*')
            return fullType.replace(valueToReplace, PLATFORM_TYPE_NAMESPACE + '::' + substitute[0])
    return fullType

def prefixName (prefix, name):
    name = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name[2:])
    name = re.sub(r'([a-zA-Z])([0-9])', r'\1_\2', name)
    name = name.upper()
    return prefix + name

def readFile (filename):
    with open(filename, 'rt') as f:
        return f.read()

def getInterfaceName (functionName):
    assert functionName[:2] == "vk"
    return functionName[2].lower() + functionName[3:]

def getFunctionTypeName (functionName):
    assert functionName[:2] == "vk"
    return functionName[2:] + "Func"

def argListToStr (args):
    def argumentToString(arg):
        result = substituteType(arg.fullType, arg.type) + ' ' + arg.name
        for size in arg.fixedSizeArray:
            result += f"[{size}]"
        return result

    return ", ".join(argumentToString(arg) for arg in args)

def getFunctionType(command):
    if command.device:
        return 'Device'
    # some functios that are marked as instance functions in vulkan_object
    # need to be interpreted as platform functions for CTS
    platformFunctions = [
        'vkCreateInstance',
        'vkGetInstanceProcAddr',
        'vkEnumerateInstanceVersion',
        'vkEnumerateInstanceLayerProperties',
        'vkEnumerateInstanceExtensionProperties',
        'vkGetExternalComputeQueueDataNV',
    ]
    if command.name in platformFunctions:
        return 'Platform'
    return 'Instance'


def camelToSnake(name):
    name = re.sub('([a-z])([23])D([A-Z])', r'\1_\2d\3', name)
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()

class HandleTypeGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        def getHandleName (name):
            return prefixName("HANDLE_TYPE_", name)

        def genHandles ():
            sorted_handles = sorted(self.vk.handles.values(), key=lambda item: item.name)
            it = iter(sorted_handles)
            yield f"\t{getHandleName(next(it).name)}\t= 0,"
            for h in it:
                yield f"\t{getHandleName(h.name)},"
            for h in sorted_handles:
                for a in h.aliases or []:
                    yield f"\t{getHandleName(a)}\t= {getHandleName(h.name)},"
            it = reversed(sorted_handles)
            yield f"\tHANDLE_TYPE_LAST\t= {getHandleName(next(it).name)} + 1\n}};"

        self.write(INL_HEADER + "\nenum HandleType\n{")
        self.write(combineLines(indentLines(genHandles())))

class BasicTypesGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        def gen ():
            # we need registry object in this generator, we cant operate on vulkan_object alone
            assert(self.registry)
            yield "// Defines"
            for line in self.genDefinesSrc("" if self.targetApiName == "vulkan" else "SC"):
                yield line
            yield "\n"

            yield "// Handles"
            # <vulkan_object_issue_workaround>
            # remove VkPrivateDataSlot handle
            if self.targetApiName == "vulkansc" and 'VkPrivateDataSlot' in self.vk.handles:
                self.vk.handles.pop('VkPrivateDataSlot')
                self.vk.commands.pop('vkCreatePrivateDataSlot')
                self.vk.commands.pop('vkDestroyPrivateDataSlot')
                self.vk.commands.pop('vkGetPrivateData')
                self.vk.commands.pop('vkSetPrivateData')
            # </vulkan_object_issue_workaround>

            for line in self.genHandlesSrc():
                yield line
            yield "\n"

            if self.targetApiName == "vulkansc":
                st = self.vk.enums['VkStructureType']
                # append VkStructureType field required by vulkan_json_data.hpp
                st.fields.append(EnumField(name = "VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV",
                                         aliases=[],
                                         protect=None,
                                         negative=False,
                                         value = 1000314008,
                                         valueStr = "1000314008",
                                         extensions=[]))
                # append VkStructureType field required by cts for SC
                st.fields.append(EnumField(name = "VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO",
                                         aliases=[],
                                         protect=None,
                                         negative=False,
                                         value = 16,
                                         valueStr = "16",
                                         extensions=[]))

            # <vulkan_object_issue_workaround>
            # add missing VK_STD_VIDEO_AV1_COLOR_PRIMARIES_BT_UNSPECIFIED alias
            if self.vk.videoStd.enums:
                av1ColorPrimitives = self.vk.videoStd.enums['StdVideoAV1ColorPrimaries']
                missingAlias = 'STD_VIDEO_AV1_COLOR_PRIMARIES_BT_UNSPECIFIED'
                for field in av1ColorPrimitives.fields:
                    if field.name == "STD_VIDEO_AV1_COLOR_PRIMARIES_UNSPECIFIED" and missingAlias not in field.aliases:
                        field.aliases.append(missingAlias)
                        break
            # <vulkan_object_issue_workaround>

            # append enums directly from video.xml
            all_enums = list(self.vk.enums.values())
            all_enums.extend(self.vk.videoStd.enums.values())
            all_enums = sorted(all_enums, key=lambda item: item.name)

            yield "// Enums"
            for enum in all_enums:
                # skip empty enums only for vulkan
                # vulkan_json_data.hpp and vulkan_json_parser.hpp in SC need empty enums
                if len(enum.fields) == 0 and self.targetApiName == "vulkan":
                    continue
                for line in self.genEnumSrc(enum):
                    yield line

            yield "// Enum aliases"
            for enum in all_enums:
                # skip empty enums only for vulkan
                # vulkan_json_data.hpp and vulkan_json_parser.hpp in SC need empty enums
                if len(enum.fields) == 0 and self.targetApiName == "vulkan":
                    continue
                for a in enum.aliases or []:
                    yield f"typedef {enum.name} {a};"

            yield "// Bitmasks"
            for bitmask in self.vk.bitmasks.values():
                genBitfield = self.genBitfield32Src if bitmask.bitWidth == 32 else self.genBitfield64Src
                for line in genBitfield(bitmask):
                    yield line
                yield f"typedef uint{bitmask.bitWidth}_t {bitmask.flagName};\n"
                for a in bitmask.aliases or []:
                    yield f"typedef {bitmask.name} {a};\n"

            yield "\n"
            yield "// Flags"
            sorted_flags = sorted(self.vk.flags.values(), key=lambda item: item.name)
            for f in sorted_flags:
                yield f"typedef uint{f.bitWidth}_t {f.name};\n"
                for a in f.aliases or []:
                    yield f"typedef {f.name} {a};\n"

            yield "\n"
            for line in indentLines(["VK_DEFINE_PLATFORM_TYPE(%s,\t%s)" % (s[0], c) for n, s, c in PLATFORM_TYPES]):
                yield line
            yield "\n"

            yield "// Extensions"
            sorted_extensions = sorted(self.vk.extensions.values(), key=lambda item: item.name)
            for e in sorted_extensions:
                yield f'#define {e.nameString} "{e.name}"'
                #yield f'#define {e.specVersion} 1'
            # <vulkan_object_issue_workaround>
            # there is no values for *_SPEC_VERSION
            yield f'#define VK_KHR_VULKAN_MEMORY_MODEL_SPEC_VERSION 3'
            # </vulkan_object_issue_workaround>

            # print video defines
            video_defines = sorted(self.vk.videoStd.constants.values(), key=lambda item: item.name)
            for vd in video_defines:
                yield f'#define {vd.name} {vd.valueStr}'
            for c in self.vk.videoCodecs.keys():
                if c == 'Decode' or c == 'Encode':
                    continue
                name = c.replace(' ', '_').replace('.', '').lower()
                nameUp = name.upper()
                yield f'#define VK_STD_VULKAN_VIDEO_CODEC_{nameUp}_EXTENSION_NAME "VK_STD_vulkan_video_codec_{name}"'
                yield f'#define VK_STD_VULKAN_VIDEO_CODEC_{nameUp}_SPEC_VERSION VK_MAKE_VIDEO_STD_VERSION(1, 0, 0)'

        self.write(INL_HEADER)
        for l in gen():
            self.write(l)

    def getEnumValuePrefixAndPostfix (self, enum):
        prefix = enum.name[0]
        for i in range(1, len(enum.name)):
            if enum.name[i].isupper() and not enum.name[i-1].isupper():
                prefix += "_"
            prefix += enum.name[i].upper()
        for p in EXTENSION_POSTFIXES:
            if prefix.endswith(p):
                return prefix[:-len(p)-1], '_'+p
        return prefix, ''

    def parseInt (self, valueStr):
        return int(valueStr, 16 if ("0x" in valueStr) else 10)

    def areValuesLinear (self, enum):
        curIndex = 0
        for enumerator in enum.fields:
            intValue = self.parseInt(enumerator.valueStr)
            if intValue != curIndex:
                return False
            curIndex += 1
        return True

    def genEnumSrc (self, enum):
        yield "enum %s" % enum.name
        yield "{"
        lines = []
        fields = sorted(enum.fields, key=lambda item: item.value)
        for ed in fields:
            if ed.valueStr is not None:
                lines.append(f"\t{ed.name}\t= {ed.valueStr},")
        for ed in fields:
            for alias in ed.aliases:
                lines.append(f"\t{alias}\t= {ed.name},")

        # add *_LAST item when enum is linear
        prefix, postfix = self.getEnumValuePrefixAndPostfix(enum)
        if self.areValuesLinear(enum):
            lines.append(f"\t{prefix}{postfix}_LAST,")

        # add _MAX_ENUM item with the ext postifix at the end
        lines.append(f"\t{prefix}_MAX_ENUM{postfix}\t= 0x7FFFFFFF")

        for line in indentLines(lines):
            yield line

        yield "};"

    def genBitfield32Src (self, bitfield):
        lines = []
        for ev in bitfield.flags:
            lines.append(f"\t{ev.name}\t= {ev.valueStr},")
            for a in ev.aliases or []:
                lines.append(f"\t{a}\t= {ev.valueStr},")
        # add _MAX_ENUM item
        prefix, postfix = self.getEnumValuePrefixAndPostfix(bitfield)
        lines.append(f"\t{prefix}_MAX_ENUM{postfix}\t= 0x7FFFFFFF")
        yield f"enum {bitfield.name}"
        yield "{"
        for line in indentLines(lines):
            yield line
        yield "};"

    def genBitfield64Src (self, bitfield64):
        yield f"typedef uint64_t {bitfield64.name};"
        lines = []
        for ev in bitfield64.flags:
            n = bitfield64.name
            v = ev.valueStr
            lines.append(f"static const {n} {ev.name}\t= {v};")
            for a in ev.aliases or []:
                lines.append(f"static const {n} {a}\t= {v};")
        # write indented lines
        for line in indentLines(lines):
            yield line
        yield "\n"

    def genDefinesSrc (self, apiName):
        def genLines ():
            apiVariant = 1 if apiName == "SC" else 0
            yield f"#define VK_API_VERSION_1_0\t(static_cast<uint32_t>\t(VK_MAKE_API_VERSION(0, 1, 0, 0)))"
            for v in self.vk.versions.values():
                major, minor = v.name[-3:].split('_')
                yield f"#define {v.nameApi}\t(static_cast<uint32_t>\t(VK_MAKE_API_VERSION(0, {major}, {minor}, 0)))"
            # add VK_API_MAX_FRAMEWORK_VERSION
            maxApiVersion = list(self.vk.versions.keys())[-1][-3:]
            # <vulkan_object_issue_workaround>
            # missing VK_SC_API_VERSION_1_0
            yield "#define VKSC_API_VERSION_1_0\t(static_cast<uint32_t>\t(VK_MAKE_API_VERSION(1, 1, 0, 0)))"
            maxApiVersion = '1_0' if apiVariant else maxApiVersion
            # </vulkan_object_issue_workaround>
            sortedConstants = sorted(self.vk.constants.values(), key=lambda t: t.name)
            for c in sortedConstants:
                defineType = DEFINITIONS.get(c.name, c.type)
                yield f"#define {c.name}\t(static_cast<{c.type}>\t({c.valueStr}))"
            logging.debug("Found max framework version for API '%s': %s" % (self.targetApiName, maxApiVersion))
            yield f"#define VK{apiName}_API_MAX_FRAMEWORK_VERSION\tVK{apiName}_API_VERSION_{maxApiVersion}"
        for line in indentLines(genLines()):
            yield line

    def genHandlesSrc (self):
        def genLines (handles):
            sorted_handles = sorted(handles, key=lambda item: item.name)
            for h in sorted_handles:
                define = "VK_DEFINE_HANDLE" if h.dispatchable else "VK_DEFINE_NON_DISPATCHABLE_HANDLE"
                handleType    = h.type
                line = f"{define}\t({{}},\tHANDLE{handleType[9:]});"
                yield line.format(h.name)
                for a in h.aliases or []:
                    yield line.format(a)

        for line in indentLines(genLines(self.vk.handles.values())):
            yield line

class StructTypesGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    # function that returns definition of structure member
    def memberAsString (self, member):
        result = substituteType(member.fullType, member.type) + '\t' + member.name
        for size in member.fixedSizeArray:
            result += f"[{size}]"
        if member.bitFieldWidth:
            result += " : " + str(member.bitFieldWidth)
        return result

    # function that prints single structure definition
    def genCompositeTypeSrc (self, type):
        structLines = "%s %s\n{\n" % ("union" if type.union else "struct", type.name)
        for line in indentLines(['\t'+self.memberAsString(m)+';' for m in type.members]):
            structLines += line + '\n'
        return structLines + "};\n"

    # function that prints all structure definitions and alias typedefs
    def genVulkanStructs(self):
        all_structs = list(self.vk.structs.values())
        all_structs.extend(self.vk.videoStd.structs.values())
        all_structs = sorted(all_structs, key=lambda s: s.name)
        # structures in xml are not ordered in a correct way for C++
        # we need to save structures that are used in other structures first
        allStructureNamesList = [s.name for s in all_structs]
        savedStructureNamesList = []
        delayedStructureObjectsList = []

        # helper function that checks if all structure members were already saved
        def canStructBeSaved(compositeObject):
            for m in compositeObject.members:
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
        for ct in all_structs:
            # check if one of delayed structures can be saved
            delayedButSaved = []
            for dct in delayedStructureObjectsList:
                if lastDelayedComposite != dct and canStructBeSaved(dct):
                    yield self.genCompositeTypeSrc(dct)
                    delayedButSaved.append(dct)
            lastDelayedComposite = None
            for dsct in delayedButSaved:
                savedStructureNamesList.append(dsct.name)
                delayedStructureObjectsList.remove(dsct)
            # check if current structure can be saved
            if canStructBeSaved(ct):
                yield self.genCompositeTypeSrc(ct)
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
                    yield self.genCompositeTypeSrc(dct)
                    savedStructureNamesList.append(dct.name)
                    delayedStructureObjectsList.remove(dct)
                    break

        # add VkShaderModuleCreateInfo structure, it is not part of SC but it is needed for vkscserver
        if self.targetApiName == 'vulkansc':
            yield 'struct VkShaderModuleCreateInfo'
            yield '{'
            yield '    VkStructureType sType;'
            yield '    const void *pNext;'
            yield '    uint32_t flags;'
            yield '    size_t codeSize;'
            yield '    const uint32_t *pCode;'
            yield '};\n'

        # write all alias typedefs
        for ct in all_structs:
            sorted_aliases = sorted(ct.aliases)
            for alias in sorted_aliases:
                yield "typedef %s %s;" % (ct.name, alias)
                yield "\n"

    def generate(self):
        self.write(INL_HEADER)
        # declare vulkan structures
        for l in self.genVulkanStructs():
            self.write(l)

class InterfaceDeclarationGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        def genProtos ():
            postfix = "" if 'Concrete' in self.filename else " = 0"
            selectedFunctions = []
            for fun in self.vk.commands.values():
                if getFunctionType(fun) in self.filename:
                    if fun.alias and fun.alias in selectedFunctions:
                        continue
                    selectedFunctions.append(fun.name)
            selectedFunctions = sorted(selectedFunctions)
            for funName in selectedFunctions:
                fun = self.vk.commands[funName]
                yield "virtual %s\t%s\t(%s) const%s;" % (fun.returnType, getInterfaceName(funName), argListToStr(fun.params), postfix)
        self.write(INL_HEADER)
        for l in indentLines(genProtos()):
            self.write(l)

class FunctionPointerTypesGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        def genTypes ():
            pattern = "typedef VKAPI_ATTR {}\t(VKAPI_CALL* {})\t({});"
            sorted_functions = sorted(self.vk.commands.values(), key=lambda f: f.name)
            for function in sorted_functions:
                argList = argListToStr(function.params)
                yield pattern.format(function.returnType, getFunctionTypeName(function.name), argList)
                if function.alias:
                    yield pattern.format(function.returnType, getFunctionTypeName(function.alias), argList)
        self.write(INL_HEADER)
        for l in indentLines(genTypes()):
            self.write(l)

class FunctionPointersGenerator(BaseGenerator):
    def __init__(self, params):
        BaseGenerator.__init__(self)
        self.savedFunctions = []

    def prepareEntry (self, functionName):
        interfaceName = getInterfaceName(functionName)
        functionTypeName = getFunctionTypeName(functionName)
        self.savedFunctions.append(functionName)
        return f"{functionTypeName}\t{interfaceName};"

    def functionsYielder (self):
        generateForInstance = "Instance" in self.filename
        generateForDevice = "Device" in self.filename
        sortedFunctions = sorted(self.vk.commands.values(), key=lambda f: f.name)
        processedFunctions = []
        for function in sortedFunctions:
            if getFunctionType(function) not in self.filename:
                continue
            processedFunctions.append(function.name)
            if function.name not in self.savedFunctions:
                if generateForDevice and function.alias and function.alias in processedFunctions:
                    continue
                yield self.prepareEntry(function.name)
            if function.alias and generateForInstance and function.alias not in self.savedFunctions:
                yield self.prepareEntry(function.alias)

    def generate(self):
        self.write(INL_HEADER)
        for l in indentLines(self.functionsYielder()):
            self.write(l)

class InitFunctionPointersGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)
        # dictionary containing function names as keys
        # and their initialization code as single string value
        self.resultDict = {}

    def getFunctionSetup(self, prefix, interfaceName, functionTypeName, functionName):
        return f"{prefix}m_vk.{interfaceName} = ({functionTypeName}) GET_PROC_ADDR(\"{functionName}\");"

    def makeInitFunctionPointers (self):
        isSC = int(self.targetApiName == 'vulkansc')
        generateForInstance = "Instance" in self.filename
        generateForDevice = "Device" in self.filename
        for function in self.vk.commands.values():
            if getFunctionType(function) not in self.filename or function.name == 'vkGetInstanceProcAddr':
                continue
            condition = ''
            if not isSC and generateForDevice and function.version:
                version = function.version.nameApi[-3:].replace('_', ', ')
                condition = f"if (usedApiVersion >= VK_MAKE_API_VERSION(0, {version}, 0))\n    "
            funName = function.name
            interfaceName = getInterfaceName(funName)
            aliasName = function.alias
            # check if function is already in result dictionary
            if interfaceName not in self.resultDict and not aliasName:
                self.resultDict[interfaceName] = self.getFunctionSetup(condition, interfaceName, getFunctionTypeName(funName), funName)
            # if command has an alias, add proper entry to the result dictionary
            if not isSC and aliasName:
                alaisInterfaceName = getInterfaceName(aliasName)
                self.resultDict[alaisInterfaceName] += f"\nif (!m_vk.{alaisInterfaceName})\n"
                self.resultDict[alaisInterfaceName] += self.getFunctionSetup("    ", alaisInterfaceName, getFunctionTypeName(aliasName), funName)
                if generateForInstance and function.params[0].type == "VkPhysicalDevice":
                    self.resultDict[alaisInterfaceName] += '\n' + self.getFunctionSetup("", interfaceName, getFunctionTypeName(aliasName), funName)
        # sort the result dictionary by function name
        self.resultDict = dict(sorted(self.resultDict.items()))

    def generate(self):
        self.makeInitFunctionPointers()
        self.write(INL_HEADER)
        for v in self.resultDict.values():
            self.write(v)

# List pre filled manually with commands forbidden for computation only implementations
computeOnlyForbiddenCommands = [
    "destroyRenderPass",
    "createRenderPass2",
    "createRenderPass",
    "createGraphicsPipelines"
]
computeOnlyRestrictedCommands = {
    "createComputePipelines"  : "\t\tfor (uint32_t i=0; i<createInfoCount; ++i)\n\t\t\tif ((pCreateInfos[i].stage.stage & VK_SHADER_STAGE_ALL_GRAPHICS) != 0) THROW_NOT_SUPPORTED_COMPUTE_ONLY();",
    "createBuffer"            : "\t\tif ((pCreateInfo->usage & ( VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT )) !=0) THROW_NOT_SUPPORTED_COMPUTE_ONLY();",
}

class FuncPtrInterfaceImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def makeFuncPtrInterfaceImpl (self):
        processedClassName = "Instance"
        processedClassName = "Device" if "Device" in self.filename else processedClassName
        processedClassName = "Platform" if "Platform" in self.filename else processedClassName

        sortedFunctions = []
        for function in self.vk.commands.values():
            if processedClassName != getFunctionType(function):
                continue
            name = function.alias if function.alias and function.alias in self.vk.commands else function.name
            if name not in sortedFunctions:
                sortedFunctions.append(name)
        sortedFunctions = sorted(sortedFunctions)

        processedFunctions = []
        for name in sortedFunctions:
            function = self.vk.commands[name]
            functionInterfaceName = getInterfaceName(name)
            processedFunctions.append(name)
            yield ""
            yield "%s %sDriver::%s (%s) const" % (function.returnType, processedClassName, functionInterfaceName, argListToStr(function.params))
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
            funParams = ", ".join(a.name for a in function.params)
            callTemplate = f"{tab}{funReturn}m_vk.{{}}({funParams});"
            # Special case for all instance functions that operate on VkPhysicalDevice
            if processedClassName == "Instance" and function.params[0].type == "VkPhysicalDevice" and function.version:
                aliasName = name + 'KHR'
                if aliasName not in self.vk.commands:
                    aliasName = name + 'EXT'
                    if aliasName not in self.vk.commands:
                        aliasName = None
                if aliasName:
                    callTemplate = f"{tab}{callTemplate}"
                    yield "    vk::VkPhysicalDeviceProperties props;"
                    yield "    m_vk.getPhysicalDeviceProperties(physicalDevice, &props);"
                    yield f"    if (props.apiVersion >= {function.version.nameApi})"
                    yield callTemplate.format(functionInterfaceName)
                    yield "    else"
                    yield callTemplate.format(getInterfaceName(aliasName))
                    yield "}\n"
                    continue
            yield callTemplate.format(functionInterfaceName)
            yield "}\n"

    def generate(self):
        # populate compute only forbidden commands
        for fun in self.vk.commands.values():
            if "VK_QUEUE_GRAPHICS_BIT" in fun.queues and not ("VK_QUEUE_COMPUTE_BIT" in fun.queues):
                # remove the 'vk' prefix and change the first character of the remaining string to lowercase
                commandName = fun.name[2:3].lower() + fun.name[3:]
                computeOnlyForbiddenCommands.append(commandName)

                # if the command has an alias, also add it
                if fun.alias:
                    alias_name_without_vk = fun.alias[2:3].lower() + fun.alias[3:]
                    computeOnlyForbiddenCommands.append(alias_name_without_vk)

        self.write(INL_HEADER)
        for l in self.makeFuncPtrInterfaceImpl():
            self.write(l)

class FuncPtrInterfaceSCImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)
        self.normFuncs = {
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
        self.statFuncs = {
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
            #"" : "surfaceRequestCount",
            #"" : "swapchainRequestCount",
            #"" : "displayModeRequestCount"
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
        self.statReturns = {
            "VkResult"            : "return VK_SUCCESS;",
            "VkDeviceAddress"    : "return 0u;",
            "uint64_t"            : "return 0u;",
        }

    def makeFuncPtrInterfaceStatisticsImpl(self):
        for function in self.vk.commands.values():
            if not function.device:
                continue
            ifaceName = getInterfaceName(function.name)
            yield ""
            yield "%s DeviceDriverSC::%s (%s) const" % (function.returnType, ifaceName, argListToStr(function.params))
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
            if ( ifaceName in self.normFuncs ) or ( ifaceName in self.statFuncs ):
                yield "\tstd::lock_guard<std::mutex> lock(functionMutex);"
            if ifaceName != "getDeviceProcAddr" :
                yield "\tif (m_normalMode)"
            if ifaceName in self.normFuncs :
                yield "%s" % ( self.normFuncs[ifaceName] )
            else:
                yield "\t\t%sm_vk.%s(%s);" % ("return " if function.returnType != "void" else "", ifaceName, ", ".join(a.name for a in function.params))
            if ifaceName in self.statFuncs :
                yield "\telse"
                yield "%s" % ( self.statFuncs[ifaceName] )
            elif ifaceName[:3] == "cmd" :
                yield "\telse"
                yield "\t\tincreaseCommandBufferSize(commandBuffer, 0u);"
            if function.returnType in self.statReturns:
                yield "\t%s" % ( self.statReturns[function.returnType] )
            yield "}\n"

    def generate(self):
        self.write(INL_HEADER)
        for l in self.makeFuncPtrInterfaceStatisticsImpl():
            self.write(l)

class StrUtilProtoGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def makeStrUtilProto(self):
        sorted_enums = sorted(self.vk.enums.values(), key=lambda en: en.name)
        sorted_bitmasks = sorted(self.vk.bitmasks.values(), key=lambda en: en.name)
        sorted_structs = sorted(self.vk.structs.values(), key=lambda en: en.name)
        for line in indentLines([f"const char*\tget{e.name[2:]}Name\t({e.name} value);" for e in sorted_enums]):
            yield line
        # save video enums
        for e in self.vk.videoStd.enums.keys():
            yield f"const char*\tget{e[2:]}Name\t({e} value);"
        yield "\n"
        for line in indentLines([f"inline tcu::Format::Enum<{e.name}>\tget{e.name[2:]}Str\t({e.name} value)\t{{ return tcu::Format::Enum<{e.name}>(get{e.name[2:]}Name, value);\t}}" for e in sorted_enums]):
            yield line
        yield "\n"
        for line in indentLines([f"inline std::ostream&\toperator<<\t(std::ostream& s, {e.name} value)\t{{ return s << get{e.name[2:]}Str(value);\t}}" for e in sorted_enums]):
            yield line
        yield "\n"
        for line in indentLines([f"tcu::Format::Bitfield<{b.bitWidth}>\tget{b.flagName[2:]}Str\t({b.flagName} value);" for b in sorted_bitmasks]):
            yield line
        yield "\n"
        for line in indentLines([f"std::ostream&\toperator<<\t(std::ostream& s, const {s.name}& value);" for s in sorted_structs]):
            yield line

    def generate(self):
        self.write(INL_HEADER)
        for l in self.makeStrUtilProto():
            self.write(l)

class StrUtilImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def makeStrUtilImpl(self):
        sorted_handles = sorted(self.vk.handles.keys())
        for line in indentLines([f"template<> const char*\tgetTypeName<{h}>\t(void) {{ return \"{h}\";\t}}" for h in sorted_handles]):
            yield line

        yield "\n"
        yield "namespace %s" % PLATFORM_TYPE_NAMESPACE
        yield "{"
        for line in indentLines("std::ostream& operator<< (std::ostream& s, %s\tv) { return s << tcu::toHex(v.internal); }" % ''.join(s) for n, s, c in PLATFORM_TYPES):
            yield line
        yield "}"

        all_enums = list(self.vk.enums.values())
        all_enums.extend(self.vk.videoStd.enums.values())

        all_enums = sorted(all_enums, key=lambda en: en.name)
        for enum in all_enums:
            yield "\n"
            yield "const char* get%sName (%s value)" % (enum.name[2:], enum.name)
            yield "{"
            yield "\tswitch (value)"
            yield "\t{"
            enumValues = []
            sorted_fields = sorted(enum.fields, key=lambda en: en.name)
            for e in sorted_fields:
                enumValues.append(f"\t\tcase {e.name}:\treturn \"{e.name}\";")
            enumValues.append("\t\tdefault:\treturn nullptr;")
            for line in indentLines(enumValues):
                yield line
            yield "\t}"
            yield "}"

        sorted_bitmasks = sorted(self.vk.bitmasks.values(), key=lambda item: item.name)
        for bitmask in sorted_bitmasks:
            yield "\n"
            yield f"tcu::Format::Bitfield<{bitmask.bitWidth}> get{bitmask.flagName[2:]}Str ({bitmask.flagName} value)"
            yield "{"
            yield "\tstatic const tcu::Format::BitDesc s_desc[] ="
            yield "\t{"
            if len(bitmask.flags) == 0:
                # some bitfields in SC have no items
                yield f"\t\ttcu::Format::BitDesc(0, \"0\")"
            else:
                sorted_flags = sorted(bitmask.flags, key=lambda en: en.name)
                for line in indentLines([f"\t\ttcu::Format::BitDesc({b.name},\t\"{b.name}\")," for b in sorted_flags]):
                    yield line
            yield "\t};"
            yield f"\treturn tcu::Format::Bitfield<{bitmask.bitWidth}>(value, DE_ARRAY_BEGIN(s_desc), DE_ARRAY_END(s_desc));"
            yield "}"

        bitfieldTypeNames = set([bitmask.flagName for bitmask in sorted_bitmasks])

        yield "\n"
        sorted_structs = sorted(self.vk.structs.values(), key=lambda item: item.name)
        for type in sorted_structs:
            yield ""
            yield "std::ostream& operator<< (std::ostream& s, const %s& value)" % type.name
            yield "{"
            yield "\ts << \"%s = {\\n\";" % type.name
            for member in type.members:
                memberName = member.name
                valFmt = None
                newLine = ""
                if member.type in bitfieldTypeNames:
                    operator = '*' if member.pointer else ''
                    valFmt = "get%sStr(%svalue.%s)" % (member.type[2:], operator, member.name)
                elif member.type == "char" and member.fullType.count('*') == 1:
                    valFmt = "getCharPtrStr(value.%s)" % member.name
                elif member.type == PLATFORM_TYPE_NAMESPACE + "::Win32LPCWSTR":
                    valFmt = "getWStr(value.%s)" % member.name
                elif len(member.fixedSizeArray) == 1:
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
                elif len(member.fixedSizeArray) > 1:
                    yield f"\ts << \"\\t{member.name} = \" << '\\n';"
                    dim = 0
                    index = ''
                    dimensionCount = len(member.fixedSizeArray)
                    while dim < dimensionCount-1:
                        yield f"\tfor(uint32_t i{dim} = 0 ; i{dim} < {member.fixedSizeArray[dim]} ; ++i{dim})"
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
            yield "}\n"

    def generate(self):
        self.write(INL_HEADER)
        for l in self.makeStrUtilImpl():
            self.write(l)

class ObjTypeImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def makeObjTypeImpl (self):
        yield "namespace vk"
        yield "{"

        yield "template<typename T> VkObjectType getObjectType    (void);"

        for line in indentLines(["template<> inline VkObjectType\tgetObjectType<%s>\t(void) { return %s;\t}" % (handle.name, prefixName("VK_OBJECT_TYPE_", handle.name)) for handle in self.vk.handles.values()]):
            yield line

        yield "}"

    def generate(self):
        self.write(INL_HEADER)
        for l in self.makeObjTypeImpl():
            self.write(l)

class RefUtilGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)
        self.savedFunctions = []

    class ConstructorFunction:
        def __init__ (self, type, name, objectType, ifaceArgs, params):
            self.type = type
            self.name = name
            self.objectType = objectType
            self.ifaceArgs = ifaceArgs
            self.params = params

    def getConstructorFunctions (self):
        funcs = []
        sortedFunctions = sorted(self.vk.commands.values(), key=lambda f: f.name)
        for function in sortedFunctions:
            if (function.name[:8] == "vkCreate" or function.name == "vkAllocateMemory") and not "createInfoCount" in [a.name for a in function.params]:
                if function.name in ["vkCreatePipelineBinariesKHR", "vkCreateDisplayModeKHR"]:
                    continue # No way to delete display modes (bug?)

                if function.name in self.savedFunctions or function.alias in self.savedFunctions:
                    continue
                self.savedFunctions.append(function.name)

                functionType = getFunctionType(function)
                ifaceArgs = "const PlatformInterface& vkp, VkInstance instance, " if function.name == "vkCreateDevice" else ""
                ifaceArgs += f"const {functionType}Interface& vk"

                objectType = function.params[-1].type
                allocatorArg = function.params[-2]
                assert (allocatorArg.type == "VkAllocationCallbacks" and \
                        "const" in allocatorArg.fullType and \
                        allocatorArg.pointer == 1)

                arguments = function.params[:-1]
                funcs.append(self.ConstructorFunction(functionType, getInterfaceName(function.name), objectType, ifaceArgs, arguments))
        return funcs

    def makeRefUtilProto (self):
        functions = self.getConstructorFunctions()
        for line in indentLines(["Move<%s>\t%s\t(%s, %s = nullptr);" % (f.objectType, f.name, f.ifaceArgs, argListToStr(f.params)) for f in functions]):
            yield line

    def makeRefUtilImpl (self):
        functions = self.getConstructorFunctions()
        yield "namespace refdetails"
        yield "{"
        yield ""

        savedDeleters = []
        sortedFunctions = sorted(self.vk.commands.values(), key=lambda f: f.name)
        for function in sortedFunctions:
            if not function.device:
                continue
            if (function.name[:9] == "vkDestroy" or function.name == "vkFreeMemory") and not function.name == "vkDestroyDevice":
                if function.name in savedDeleters or (function.alias and function.alias in savedDeleters):
                    continue
                savedDeleters.append(function.name)
                if function.alias:
                    savedDeleters.append(function.alias)
                objectType = function.params[-2].type
                yield "template<>"
                yield "void Deleter<%s>::operator() (%s obj) const" % (objectType, objectType)
                yield "{"
                yield "\tm_deviceIface->%s(m_device, obj, m_allocator);" % (getInterfaceName(function.name))
                yield "}\n"

        yield "} // refdetails"
        yield "\n"

        sortedFunctions = sorted(functions, key=lambda f: f.name)
        for function in sortedFunctions:
            deleterArgsString = ''
            if function.name == "createDevice":
                # createDevice requires two additional parameters to setup VkDevice deleter
                deleterArgsString = "vkp, instance, object, " +  function.params[-1].name
            else:
                dtor = function.type.lower().replace('platform', 'object')
                deleterArgsString = "vk, %s, %s" % (dtor, function.params[-1].name)

            yield "Move<%s> %s (%s, %s)" % (function.objectType, function.name, function.ifaceArgs, argListToStr(function.params))
            yield "{"
            yield "\t%s object = VK_NULL_HANDLE;" % function.objectType
            yield "\tVK_CHECK(vk.%s(%s));" % (function.name, ", ".join([a.name for a in function.params] + ["&object"]))
            yield "\treturn Move<%s>(check<%s>(object), Deleter<%s>(%s));" % (function.objectType, function.objectType, function.objectType, deleterArgsString)
            yield "}\n"

    def generate(self):
        generatePrototypes = False if 'Impl' in self.filename else True
        makeRefUtil = self.makeRefUtilProto if generatePrototypes else self.makeRefUtilImpl
        self.write(INL_HEADER)
        for l in makeRefUtil():
            self.write(l)

class GetStructureTypeImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def gen (self):
        sorted_structs = sorted(self.vk.structs.values(), key=lambda s: s.name)
        for cType in sorted_structs:
            if cType.members[0].name == "sType" and cType.name != "VkBaseOutStructure" and cType.name != "VkBaseInStructure":
                yield "template<> VkStructureType getStructureType<%s> (void)" % cType.name
                yield "{"
                yield "\treturn %s;" % cType.sType
                yield "}\n"

    def generate(self):
        self.write(INL_HEADER)
        for l in self.gen():
            self.write(l)

class NullDriverImplGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def genNullDriverImpl (self):
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

        specialFuncs = [f for f in self.vk.commands.values() if f.name in specialFuncNames]
        createFuncs = [f for f in self.vk.commands.values() if (f.name[:8] == "vkCreate" or f.name == "vkAllocateMemory") and not f in specialFuncs]
        destroyFuncs = [f for f in self.vk.commands.values() if (f.name[:9] == "vkDestroy" or f.name == "vkFreeMemory") and not f in specialFuncs]
        dummyFuncs = [f for f in self.vk.commands.values() if f not in specialFuncs + createFuncs + destroyFuncs]

        def getHandle (name):
            for handle in self.vk.handles.values():
                if handle.name == name:
                    return handle
            raise Exception("No such handle: %s" % name)

        for function in sorted(createFuncs, key=lambda f: f.name):
            objectType = function.params[-1].type
            argsStr = ", ".join([a.name for a in function.params[:-1]])

            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.params))
            yield "{"
            yield "\tDE_UNREF(%s);" % function.params[-2].name

            if function.params[-1].length != None:
                yield "\tVK_NULL_RETURN((allocateNonDispHandleArray<%s, %s>(%s, %s)));" % (objectType[2:], objectType, argsStr, function.params[-1].name)
            else:
                if function.name == "vkCreatePipelineBinariesKHR":
                    yield "\tDE_UNREF(device);"
                    yield "\tDE_UNREF(pCreateInfo);"
                    yield "\tDE_UNREF(pAllocator);"
                    yield "\tDE_UNREF(pBinaries);"
                    yield "\treturn VK_SUCCESS;"
                    yield "}\n"
                    continue
                allocateMethod = 'allocateHandle' if getHandle(objectType).dispatchable else 'allocateNonDispHandle'
                yield "\tVK_NULL_RETURN((*%s = %s<%s, %s>(%s)));" % (function.params[-1].name, allocateMethod, objectType[2:], objectType, argsStr)
            yield "}\n"

        for function in sorted(destroyFuncs, key=lambda f: f.name):
            objectArg = function.params[-2]
            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.params))
            yield "{"
            for arg in function.params[:-2]:
                yield "\tDE_UNREF(%s);" % arg.name
            freeMethod = 'freeHandle' if getHandle(objectArg.type).dispatchable else 'freeNonDispHandle'
            yield "\t%s<%s, %s>(%s, %s);" % (freeMethod, objectArg.type[2:], objectArg.type, objectArg.name, function.params[-1].name)
            yield "}\n"

        for function in sorted(dummyFuncs, key=lambda f: f.name):
            yield "VKAPI_ATTR %s VKAPI_CALL %s (%s)" % (function.returnType, getInterfaceName(function.name), argListToStr(function.params))
            yield "{"
            for arg in function.params:
                yield "\tDE_UNREF(%s);" % arg.name
            if function.returnType != "void":
                yield "\treturn VK_SUCCESS;"
            yield "}\n"

        platformEntries = []
        instanceEntries = []
        deviceEntries = []
        for f in self.vk.commands.values():
            functionType = getFunctionType(f)
            if functionType == 'Platform':
                platformEntries.append(f.name)
                continue
            if functionType == 'Device':
                if f.alias is None or f.alias not in deviceEntries:
                    deviceEntries.append(f.name)
                continue
            if f.alias is None or f.alias not in instanceEntries:
                instanceEntries.append(f.name)

        def genFuncEntryTable (libraryName, functionEntries):
            entries =[]
            pattern = "\tVK_NULL_FUNC_ENTRY(%s,\t%s),"
            sorted_functions = sorted(functionEntries)
            for fName in sorted_functions:
                entries.append(pattern % (fName, getInterfaceName(fName)))
            yield f"static const tcu::StaticFunctionLibrary::Entry {libraryName}[] ="
            yield "{"
            for line in indentLines(entries):
                yield line
            yield "};\n"

        # Func tables
        for line in genFuncEntryTable("s_platformFunctions", platformEntries):
            yield line

        for line in genFuncEntryTable("s_instanceFunctions", instanceEntries):
            yield line

        for line in genFuncEntryTable("s_deviceFunctions", deviceEntries):
            yield line

    def generate(self):
        self.write(INL_HEADER)
        for l in self.genNullDriverImpl():
            self.write(l)

class TypeUtilGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)
        # Structs filled by API queries are not often used in test code
        self.QUERY_RESULT_TYPES = set([
                "VkPhysicalDeviceFeatures",
                "VkPhysicalDeviceLimits",
                "VkFormatProperties",
                "VkImageFormatProperties",
                "VkPhysicalDeviceSparseProperties",
                "VkQueueFamilyProperties",
                "VkMemoryType",
                "VkMemoryHeap",
                "VkClusterAccelerationStructureGeometryIndexAndGeometryFlagsNV",
                "VkClusterAccelerationStructureBuildTriangleClusterInfoNV",
            ])

    def isSimpleStruct (self, type):
        def hasArrayMember (type):
            return any(len(member.fixedSizeArray) for member in type.members)

        def hasCompositeMember (type):
            for member in type.members:
                if member.pointer == False:
                    match = [c for c in self.vk.structs.values() if member.type == c.name]
                    if len(match) > 0:
                        return True
            return False

        return type.members[0].type != "VkStructureType" and \
        not type.name in self.QUERY_RESULT_TYPES and \
        not hasArrayMember(type) and \
        not hasCompositeMember(type)

    def gen (self):
        sorted_structs = sorted(self.vk.structs.values(), key=lambda s: s.name)
        for type in sorted_structs:
            if not self.isSimpleStruct(type):
                continue

            name = type.name[2:] if type.name[:2].lower() == "vk" else type.name

            yield "inline %s make%s (%s)" % (type.name, name, argListToStr(type.members))
            yield "{"
            yield "\t%s res;" % type.name
            for line in indentLines(["\tres.%s\t= %s;" % (m.name, m.name) for m in type.members]):
                yield line
            yield "\treturn res;"
            yield "}\n"

    def generate(self):
        self.write(INL_HEADER)
        for l in self.gen():
            self.write(l)

class DriverIdsGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        self.write(INL_HEADER)
        self.write("static const struct\n"
                   "{\n"
                   "\tstd::string driver;\n"
                   "\tuint32_t id;\n"
                   "} driverIds [] =\n"
                   "{")
        driverIdEnum = self.vk.enums['VkDriverId']
        for enumerator in driverIdEnum.fields:
            self.write(f"\t{{\"{enumerator.name}\", {enumerator.value}}},")
        for enumerator in driverIdEnum.fields:
            if len(enumerator.aliases) > 0:
                self.write(f"\t{{\"{enumerator.aliases[0]}\", {enumerator.value}}},\t// {enumerator.name}")
        self.write("\t{\"VK_DRIVER_ID_MAX_ENUM\", 0x7FFFFFFF}")
        self.write("};")

class SupportedExtensionsGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)
        self.partiallyPromotedExtensions = [
            'VK_EXT_extended_dynamic_state',
            'VK_EXT_extended_dynamic_state2',
            'VK_EXT_texel_buffer_alignment',
            'VK_EXT_ycbcr_2plane_444_formats',
            'VK_EXT_4444_formats'
        ]

    def writeExtensionsForVersions(self, map):
        for version in map:
            self.write("    if (coreVersion >= " + str(version) + ")")
            self.write("    {")
            for extension in sorted(map[version], key=lambda e: e.name):
                self.write('        dst.push_back("' + extension.name + '");')
            self.write("    }")
        if not map:
            self.write("    DE_UNREF(coreVersion);")
            self.write("    DE_UNREF(dst);")

    def generate(self):
        isSC = self.targetApiName == 'vulkansc'
        instanceMap = {}
        deviceMap = {}

        for ext in self.vk.extensions.values():
            if ext.promotedTo is None or "VK_VERSION" not in ext.promotedTo:
                continue
            # skip partialy promoted extensions
            if ext.name in self.partiallyPromotedExtensions:
                continue
            major = int(ext.promotedTo[-3])
            minor = int(ext.promotedTo[-1])
            currVersion = "VK_API_VERSION_" + ext.promotedTo[-3:]
            # VulkanSC is based on Vulkan 1.2. Any Vulkan version greater than 1.2 should be excluded
            if isSC and major==1 and minor>2:
                continue
            if ext.instance:
                list = instanceMap.get(currVersion)
                instanceMap[currVersion] = list + [ext] if list else [ext]
            else:
                list = deviceMap.get(currVersion)
                deviceMap[currVersion] = list + [ext] if list else [ext]

        self.write(INL_HEADER)
        self.write("")
        self.write("\nvoid getCoreDeviceExtensionsImpl (uint32_t coreVersion, ::std::vector<const char*>&%s)\n{" % (" dst" if len(deviceMap) != 0 or isSC else ""))
        self.writeExtensionsForVersions(deviceMap)
        self.write("}\n\nvoid getCoreInstanceExtensionsImpl (uint32_t coreVersion, ::std::vector<const char*>&%s)\n{" % (" dst" if len(instanceMap) != 0 or isSC else ""))
        self.writeExtensionsForVersions(instanceMap)
        self.write("}\n")

class ExtensionFunctionsGenerator(BaseGenerator):
    def __init__(self, params):
        BaseGenerator.__init__(self)
        self.rawVkXml = params

    def writeExtensionNameArrays (self):
        yield '::std::string instanceExtensionNames[] =\n{'
        for ext in self.vk.extensions.values():
            if ext.instance:
                yield f'\t"{ext.name}",'
        yield '};\n'
        yield '::std::string deviceExtensionNames[] =\n{'
        for ext in self.vk.extensions.values():
            if ext.device:
                yield f'\t"{ext.name}",'
        yield '};'

    def writeExtensionFunctions (self, functionType):
        dg_list = []    # Device groups functions need special casing, as Vulkan 1.0 keeps them in VK_KHR_device_groups whereas 1.1 moved them into VK_KHR_swapchain
        if functionType == 'Instance':
            yield 'void getInstanceExtensionFunctions (uint32_t apiVersion, const std::vector<std::string> vIEP, const std::vector<std::string> vDEP, const std::string extName, ::std::vector<const char*>& functions)\n{'
            yield '\t(void)vIEP;\n\t(void)vDEP;'
            dg_list = ["vkGetPhysicalDevicePresentRectanglesKHR"]
        elif functionType == 'Device':
            yield 'void getDeviceExtensionFunctions (uint32_t apiVersion, const std::vector<std::string> vIEP, const std::vector<std::string> vDEP, const std::string extName, ::std::vector<const char*>& functions)\n{'
            yield '\t(void)vIEP;\n\t(void)vDEP;'
            dg_list = ["vkGetDeviceGroupPresentCapabilitiesKHR", "vkGetDeviceGroupSurfacePresentModesKHR", "vkAcquireNextImage2KHR"]

        # <vulkan_object_issue_workaround>
        # there is no information in vulkan_object about 'require depends' for extensions
        resultData = {}
        for rootChild in self.rawVkXml.getroot():
            if rootChild.tag != 'extensions':
                continue
            for extensionNode in rootChild:
                extensionName = extensionNode.get('name')
                if extensionName not in self.vk.extensions:
                    continue
                for requireItem in extensionNode.findall('require'):
                    parsedRequirements = []
                    depends = requireItem.get("depends")
                    funcNames = []
                    for individualRequirement in requireItem:
                        if individualRequirement.tag != "command":
                            continue
                        commandName = individualRequirement.get("name")
                        if commandName not in self.vk.commands:
                            continue
                        if getFunctionType(self.vk.commands[commandName]) != functionType:
                            continue
                        funcNames.append(commandName)
                    if extensionName not in resultData:
                        resultData[extensionName] = [(depends, funcNames)]
                    else:
                        resultData[extensionName].append((depends, funcNames))
        resultData = dict(sorted(resultData.items()))
        # </vulkan_object_issue_workaround>

        for extensionName, requirementList in resultData.items():
            yield f'\tif (extName == "{extensionName}")'
            yield '\t{'
            for depends, functionList in requirementList:
                if len(functionList) == 0:
                    continue
                condition = None
                indent = '\t\t'
                if depends is not None:
                    try:
                        condition = transformDependsToCondition(depends, self.vk, 'checkVersion(%s, %s, apiVersion)', 'extensionIsSupported(%s, "%s")', extensionName)
                    except Exception as e:
                        if self.targetApiName != 'vulkansc':
                            raise e
                    yield '\t\t// Dependencies: %s' % depends
                    yield '\t\tif (%s) {' % condition
                    indent = '\t\t\t'
                for funcName in functionList:
                    if funcName in dg_list:
                        yield '%sif(apiVersion >= VK_API_VERSION_1_1) functions.push_back("%s");' % (indent, funcName)
                    else:
                        yield '%sfunctions.push_back("%s");' % (indent, funcName)
                if depends is not None:
                    yield '\t\t}'
                if extensionName == "VK_KHR_device_group":
                    for dg_func in dg_list:
                        yield '\t\tif(apiVersion < VK_API_VERSION_1_1) functions.push_back("%s");' % dg_func
            yield '\t\treturn;'
            yield '\t}'

        yield '\tDE_FATAL("Extension name not found");'
        yield '}'

    def genHelperFunctions(self):
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
        if self.targetApiName != 'vulkansc':
            yield 'bool extensionIsSupported(const std::vector<std::string> extNames, const std::string& ext)'
            yield '{'
            yield '\tfor (const std::string& supportedExtension : extNames)'
            yield '\t{'
            yield '\t\tif (supportedExtension == ext) return true;'
            yield '\t}'
            yield '\treturn false;'
            yield '}\n'

    def generate(self):
        self.write(INL_HEADER)
        for line in self.genHelperFunctions():
            self.write(line)
        for line in self.writeExtensionFunctions('Instance'):
            self.write(line)
        self.write('')
        for line in self.writeExtensionFunctions('Device'):
            self.write(line)
        self.write('')
        for line in self.writeExtensionNameArrays():
            self.write(line)

class CoreFunctionalitiesGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        functionNamesPerApiVersionDict = {}
        for f in self.vk.commands.values():
            name = f.name
            # skip extension commands
            if name[-1].isupper():
                continue

            apiVersion = 'VK_API_VERSION_1_0'
            if f.version is not None:
                apiVersion = f.version.nameApi
                # skip optional promotions like functions from VK_EXT_host_image_copy
                if f.version.nameApi == 'VK_API_VERSION_1_4':
                    if 'vkCopy' in name or name == 'vkTransitionImageLayout':
                        continue

            # add function to dictionary
            if apiVersion in functionNamesPerApiVersionDict:
                if name not in functionNamesPerApiVersionDict[apiVersion]:
                    functionNamesPerApiVersionDict[apiVersion].append(name)
            else:
                functionNamesPerApiVersionDict[apiVersion] = [name]

        lines = [
        '\nenum FunctionOrigin', '{'] + [line for line in indentLines([
        '\tFUNCTIONORIGIN_PLATFORM\t= 0,',
        '\tFUNCTIONORIGIN_INSTANCE,',
        '\tFUNCTIONORIGIN_DEVICE'])] + [
        "};\n",
        "typedef ::std::pair<const char*, FunctionOrigin> FunctionInfo;",
        "typedef ::std::vector<FunctionInfo> FunctionInfosList;",
        "typedef ::std::map<uint32_t, FunctionInfosList> ApisMap;\n",
        "void initApisMap (ApisMap& apis)",
        "{",
        "    apis.clear();"] + [
        "    apis.insert(::std::pair<uint32_t, FunctionInfosList>(" + v + ", FunctionInfosList()));" for v in functionNamesPerApiVersionDict] + [
        "\n"]

        functionLines = []
        for apiVersion in functionNamesPerApiVersionDict:
            lines += [f'\tapis[{apiVersion}] = {{']
            # iterate over names of functions added with api
            for functionName in functionNamesPerApiVersionDict[apiVersion]:
                f = self.vk.commands[functionName]
                functionType = getFunctionType(f)
                # add line corresponding to this function
                functionLines.append(f'\t\t{{"{functionName}",\tFUNCTIONORIGIN_{functionType.upper()}}},')
            # indent all functions of specified api and add them to main list
            lines = lines + [line for line in indentLines(functionLines)] + ["\t};"]

        # write all lines to file
        self.write(INL_HEADER)
        for l in lines:
            self.write(l)
        self.write("}")

class DeviceFeatures2Generator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):

        isSC = int(self.targetApiName == 'vulkansc')

        def isStructValid(struct):
            # structure should extend VkPhysicalDeviceFeatures2
            if not struct.extends or 'VkPhysicalDeviceFeatures2' not in struct.extends:
                return False
            # structure should be added by KHR or EXT extension
            if any([postfix in struct.name for postfix in EXTENSION_POSTFIXES_VENDOR]):
                return False
            return True

        # find structures that should be tested
        structures = [c for c in self.vk.structs.values() if isStructValid(c)]
        structures = sorted(structures, key=lambda item: item.name)

        # list of partially promoted extensions that are not marked in vk.xml as partially promoted in extension definition
        # note: for VK_EXT_host_image_copy there is a comment in require section for vk1.4
        partiallyPromotedExtensions = ['VK_EXT_pipeline_protected_access', 'VK_EXT_host_image_copy']

        # construct file content
        self.write(INL_HEADER)

        # individual test functions
        for structure in structures:
            partiallyPromoted = False
            structureName = structure.name
            flagName = 'is' + structure.name[16:]
            instanceName = 'd' + structure.name[11:]

            # generate conditin that will be used to check if feature structure is available
            condition = ''
            for e in structure.extensions:
                partiallyPromoted = True if e in partiallyPromotedExtensions else partiallyPromoted
                condition += f' checkExtension(properties, "{e}") ||'
            if structure.version and not partiallyPromoted:
                versionStr = structure.version.nameApi[-3:].replace('_', ', ')
                condition += f' context.contextSupports(vk::ApiVersion({isSC}, {versionStr}, 0))'
            elif condition == '':
                condition = ' true'
            else:
                condition = condition[:-3] # remove ' ||' from condition end

            nameSpacing = ' ' * int((len(structureName) - len("const bool")) + 1)

            self.write("tcu::TestStatus testPhysicalDeviceFeature" + instanceName[len('device'):]+" (Context& context)")
            self.write('{\n'
                       '    const VkPhysicalDevice        physicalDevice = context.getPhysicalDevice();\n'
                       '    const CustomInstance          instance(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));\n'
                       '    const InstanceDriver&         vki(instance.getDriver());\n'
                       '    const int                     count = 2u;\n'
                       '    TestLog&                      log = context.getTestContext().getLog();\n'
                       '    VkPhysicalDeviceFeatures2     extFeatures;\n'
                       '    vector<VkExtensionProperties> properties = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);\n')
            self.write(f'    {structureName} {instanceName}[count];')
            self.write(f'    const bool{nameSpacing}is{structureName[16:]} ={condition};\n')
            self.write('    if (!' + flagName + ')')
            self.write('        return tcu::TestStatus::pass("Querying not supported");\n')
            self.write('    for (int ndx = 0; ndx < count; ++ndx)\n    {')
            self.write('        deMemset(&' + instanceName + '[ndx], 0xFF * ndx, sizeof(' + structureName + '));')
            self.write('        ' + instanceName + '[ndx].sType = ' + structure.sType + ';')
            self.write('        ' + instanceName + '[ndx].pNext = nullptr;\n')
            self.write(
                    '        deMemset(&extFeatures.features, 0xcd, sizeof(extFeatures.features));\n'
                    '        extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;\n'
                    '        extFeatures.pNext = &' + instanceName + '[ndx];\n\n'
                    '        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);')
            self.write('    }\n')
            # construct log section
            self.write('    log << TestLog::Message << ' + instanceName + '[0] << TestLog::EndMessage;\n')
            # compare each member
            self.write('    if (')
            memberCount = len(structure.members)
            for index, m in enumerate(structure.members[2:]):
                postfix = ')' if index == memberCount-3 else ' ||'
                self.write('        ' + instanceName + '[0].' + m.name + ' != ' + instanceName + '[1].' + m.name + postfix)
            if memberCount == 0:
                self.write('    false)')
            self.write('    {\n        TCU_FAIL("Mismatch between ' + structureName + '");\n    }')

            self.write('    return tcu::TestStatus::pass("Querying succeeded");')
            self.write("}\n")

        promotedTests = []
        for version in self.vk.versions.values():
            if version.name == 'VK_VERSION_1_0':
                continue
            versionStrA = version.name[-3:]
            versionStrB = versionStrA.replace('_', '.')
            versionStrA = versionStrA.replace('_', ', ')
            promotedStructs = [struct for struct in structures if struct.version and struct.version.name == version.name and re.search(r'Vulkan(SC)?\d\d', struct.name) == None]
            if not promotedStructs:
                continue
            promotedStructs = sorted(promotedStructs, key=lambda item: item.name)

            testName = "createDeviceWithPromoted" + version.name[-3:].replace('_', '') + "Structures"
            promotedTests.append(testName)
            self.write("tcu::TestStatus " + testName + " (Context& context)")
            self.write("{")
            self.write(
            f'    if (!context.contextSupports(vk::ApiVersion({isSC}, {versionStrA}, 0)))\n'
            f'        TCU_THROW(NotSupportedError, "Vulkan {versionStrB} is not supported");')
            self.write('\n'
            '    const PlatformInterface&        platformInterface = context.getPlatformInterface();\n'
            '    const CustomInstance            instance            (createCustomInstanceFromContext(context));\n'
            '    const InstanceDriver&            instanceDriver        (instance.getDriver());\n'
            '    const VkPhysicalDevice            physicalDevice = chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());\n'
            '    const uint32_t                    queueFamilyIndex = 0;\n'
            '    const uint32_t                    queueCount = 1;\n'
            '    const uint32_t                    queueIndex = 0;\n'
            '    const float                        queuePriority = 1.0f;\n\n'
            '    const vector<VkQueueFamilyProperties> queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);\n\n'
            '    const VkDeviceQueueCreateInfo    deviceQueueCreateInfo =\n'
            '    {\n'
            '        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,\n'
            '        nullptr,\n'
            '        (VkDeviceQueueCreateFlags)0u,\n'
            '        queueFamilyIndex, //queueFamilyIndex;\n'
            '        queueCount, //queueCount;\n'
            '        &queuePriority, //pQueuePriorities;\n'
            '    };\n')
            lastFeature = None
            for struct in promotedStructs:
                instanceName = 'd' + struct.name[11:]
                pNext = f'&{lastFeature}' if lastFeature else ""
                self.write(f'\t{struct.name} {instanceName} = initVulkanStructure({pNext});')
                lastFeature = instanceName
            self.write("\tVkPhysicalDeviceFeatures2 extFeatures = initVulkanStructure(&" + lastFeature + ");")
            self.write('\n'
            '    instanceDriver.getPhysicalDeviceFeatures2 (physicalDevice, &extFeatures);\n\n'
            '    const VkDeviceCreateInfo        deviceCreateInfo =\n'
            '    {\n'
            '        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;\n'
            '        &extFeatures, //pNext;\n'
            '        (VkDeviceCreateFlags)0u,\n'
            '        1, //queueRecordCount;\n'
            '        &deviceQueueCreateInfo, //pRequestedQueues;\n'
            '        0, //layerCount;\n'
            '        nullptr, //ppEnabledLayerNames;\n'
            '        0, //extensionCount;\n'
            '        nullptr, //ppEnabledExtensionNames;\n'
            '        nullptr, //pEnabledFeatures;\n'
            '    };\n\n'
            '    const Unique<VkDevice>            device            (createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));\n'
            '    const DeviceDriver                deviceDriver    (platformInterface, instance, device.get(), context.getUsedApiVersion(), context.getTestContext().getCommandLine());\n'
            '    const VkQueue                    queue = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);\n\n'
            '    VK_CHECK(deviceDriver.queueWaitIdle(queue));\n\n'
            '    return tcu::TestStatus::pass("Pass");\n'
            '}\n')

        # write function to create all tests
        self.write("void addSeparateFeatureTests (tcu::TestCaseGroup* testGroup)\n{")
        for s in structures:
            instanceName = 'd' + s.name[11:]
            self.write('\taddFunctionCase(testGroup, "' + camelToSnake(instanceName[len('device'):]) + '", testPhysicalDeviceFeature' + instanceName[len('device'):] + ');')
        for x in promotedTests:
            self.write('\taddFunctionCase(testGroup, "' + camelToSnake(x) + '", ' + x + ');')
        self.write('}\n')

class FeaturesOrPropertiesGenericGenerator(BaseGenerator):
    def __init__(self, params):
        BaseGenerator.__init__(self)
        self.structList = params
        self.structGroup = 'Features'
        self.blobList = []

    def fillSelectedStructList(self):
        structPattern = fr'VkPhysicalDevice(\w+){self.structGroup}'
        blobPattern = fr'VkPhysicalDeviceVulkan(SC)?([1-9][0-9]){self.structGroup}'
        basePattern = f'VkPhysicalDevice{self.structGroup}2'
        for struct in self.vk.structs.values():
            if re.search(structPattern, struct.name, re.IGNORECASE):
                # check if struct extends VkPhysicalDeviceFeatures(/Properties)2
                if struct.extends is None or basePattern not in struct.extends:
                    continue
                # check if structure is blob ex) VkPhysicalDeviceVulkan11Features
                if re.search(blobPattern, struct.name, re.IGNORECASE):
                    self.blobList.append(struct)
                else:
                    self.structList.append(struct)
        # sort structures by name
        sortByName = lambda s: s.name
        self.structList.sort(key=sortByName)
        self.blobList.sort(key=sortByName)

    def constructPromotionCheckerFunString(self, structGroupSingular, structList):
        # construct function that will return previous extension that provided same feature/property struct
        mapItems = ""
        for s in structList:
            if len(s.extensions) < 2:
                continue
            extA = self.vk.extensions[s.extensions[0]]
            extB = self.vk.extensions[s.extensions[1]]
            # swap names if they are in wrong order
            if extA.promotedTo == extB.name:
                extA, extB = extB, extA
            mapItems += f"\t\t{{ \"{extA.name}\", \"{extB.name}\" }},\n"
        return (f"const std::string getPrevious{structGroupSingular}ExtName (const std::string &name)\n{{\n"
                 "\tconst std::map<std::string, std::string> previousExtensionsMap {\n"
                f"{mapItems}"
                 "\n\t};\n\n\tauto it = previousExtensionsMap.find(name);\n"
                 "\tif(it == previousExtensionsMap.end())\n"
                 "\t\treturn {};\n"
                 "\treturn it->second;\n}")

    def generate(self):
        self.structGroup = self.structGroup if self.structGroup in self.filename else 'Properties'

        # fill structs list - it will be shared with few other generators
        self.fillSelectedStructList()

        # create helper class storing all member names
        # and structures that were combined into blobs
        class BlobData:
            def __init__(self, memberNames):
                self.memberNames = memberNames
                self.componentStructs = []
        blobDataDict = {}
        for bs in self.blobList:
            allMembers = [bm.name for bm in bs.members]
            blobDataDict[bs.name] = BlobData(allMembers[2:])

        structGroupLen = len(self.structGroup)
        structGroupLow = self.structGroup.lower()
        structGroupUp  = self.structGroup.upper()
        structGroupSingular = ['Feature', 'Property'][self.structGroup == 'Properties']

        initFromBlobDefinitions = []
        emptyInitDefinitions = []

        # iterate over all feature/property structures and try to identify if they are part of blob
        for struct in self.structList:
            # skip sType and pNext and grab unique struct attribute
            attributeName = struct.members[2].name
            # check if there is a blob that has attribute with same name
            blobVersion = None
            for blobName, blobData in blobDataDict.items():
                if attributeName in blobData.memberNames:
                    blobData.componentStructs.append(struct)
                     # get two numbers betwean 'Vulkan' and 'Features'/'Properties'
                    blobVersion = blobName[-structGroupLen-2:-structGroupLen]
                    # skip blobs that are not supported in VulkanSC
                    if self.targetApiName == 'vulkansc' and int(blobVersion) > 12:
                        blobVersion = None
                    break
            # reuse this loop to generate code
            if blobVersion:
                memberCopying = ""
                for member in struct.members[2:]:
                    if len(member.fixedSizeArray) == 0:
                        # handle special case
                        if struct.name == "VkPhysicalDeviceSubgroupProperties" and "subgroup" not in member.name :
                            blobMemberName = "subgroup" + member.name[0].capitalize() + member.name[1:]
                            memberCopying += "\t{0}Type.{1} = allBlobs.vk{2}.{3};\n".format(structGroupLow, member.name, blobVersion, blobMemberName)
                        # end handling special case
                        else:
                            memberCopying += "\t{0}Type.{1} = allBlobs.vk{2}.{1};\n".format(structGroupLow, member.name, blobVersion)
                    else:
                        memberCopying += "\tmemcpy({0}Type.{1}, allBlobs.vk{2}.{1}, sizeof({3}) * {4});\n".format(structGroupLow, member.name, blobVersion, member.type, member.fixedSizeArray[0])
                wholeFunction = \
                    "template<> void init{0}FromBlob<{1}>({1}& {2}Type, const All{3}Blobs& allBlobs)\n" \
                    "{{\n" \
                    "{4}" \
                    "}}".format(structGroupSingular, struct.name, structGroupLow, self.structGroup, memberCopying)
                initFromBlobDefinitions.append(wholeFunction)
            else:
                emptyFunction = "template<> void init{0}FromBlob<{1}>({1}&, const All{2}Blobs&) {{}}"
                emptyInitDefinitions.append(emptyFunction.format(structGroupSingular, struct.name, self.structGroup))

        descDefinitions = []
        structWrappers = []
        for struct in self.structList:
            nameString = f"DECL_CORE_{structGroupUp}_NAME"
            if struct.extensions:
                extName = struct.extensions[0]
                # part of code below contains workaround for bug in ShaderObject
                # where extensions list sometimes has Extension objects in it
                # instead of strings with extension name
                ext = self.vk.extensions[extName] if isinstance(extName, str) else extName
                if len(struct.extensions) > 1:
                    extName = struct.extensions[1]
                    ext2 = self.vk.extensions[extName] if isinstance(extName, str) else extName
                    if ext.promotedTo == ext2.name:
                        ext = ext2
                nameString = ext.nameString
            descDefinitions.append(f"template<> {structGroupSingular}Desc make{structGroupSingular}Desc<{struct.name}>(void) " \
                                   f"{{ return {structGroupSingular}Desc{{{struct.sType}, {nameString}}}; }}")
            pnext = next((m for m in struct.members if m.name == "pNext"), None)
            constStr = "// Contains const pNext " if pnext and getattr(pnext, "const", False) else ""
            structWrappers.append(f"\t{constStr}{{ create{structGroupSingular}StructWrapper<{struct.name}>, {nameString} }},")

        blobChecker = f"uint32_t getBlob{self.structGroup}Version (VkStructureType sType)\n{{\n" \
                      "\tconst std::map<VkStructureType, uint32_t> sTypeBlobMap\n\t{\n"
        blobCheckerMap = "static const std::map<VkStructureType, uint32_t> sTypeBlobMap\n" \
                         "{\n"
        for blobName, blobData in blobDataDict.items():
            blobCheckerMap += f'\t// {blobName}\n'
            for bcs in blobData.componentStructs:
                if bcs.version is None:
                    continue
                tabs = "\t" * int((88 - len(bcs.sType)) / 4)
                blobCheckerMap += f'\t{{ {bcs.sType},{tabs}{bcs.version.nameApi} }},\n'
        blobCheckerMap += "};\n\n"
        blobChecker = f"uint32_t getBlob{self.structGroup}Version (VkStructureType sType)\n{{\n" \
                       "\tauto it = sTypeBlobMap.find(sType);\n" \
                       "\tif(it == sTypeBlobMap.end())\n" \
                       "\t\treturn 0;\n" \
                       "\treturn it->second;\n" \
                       "}\n"
        blobExpander = f"std::set<VkStructureType> getVersionBlob{structGroupSingular}List (uint32_t version)\n{{\n" \
                        "\tstd::set<VkStructureType> features;\n" \
                        "\tfor (const std::pair<const VkStructureType, uint32_t> &item : sTypeBlobMap)\n" \
                        "\t{\n" \
                        "\t\tif (item.second == version)\n" \
                        "\t\t\tfeatures.insert(item.first);\n" \
                        "\t}\n" \
                        "\treturn features;\n" \
                        "}\n"
        stream = [
        f'#include "vkDevice{self.structGroup}.hpp"\n',
        '#include <set>\n',
        'namespace vk\n{']
        stream.append(f'\n#define DECL_CORE_{structGroupUp}_NAME "core_{structGroupLow}"\n')
        stream.extend(initFromBlobDefinitions)
        stream.append('\n// generic template is not enough for some compilers')
        stream.extend(emptyInitDefinitions)
        stream.append('')
        stream.extend(descDefinitions)
        stream.append('')
        stream.append(f'static const {structGroupSingular}StructCreationData {structGroupSingular.lower()}StructCreationArray[]\n{{')
        stream.extend(structWrappers)
        stream.append('};\n')
        stream.append(self.constructPromotionCheckerFunString(structGroupSingular, self.structList))
        stream.append('')
        stream.append(blobCheckerMap)
        stream.append(blobChecker)
        stream.append(blobExpander)
        stream.append('} // vk')
        self.write(combineLines(stream, INL_HEADER))

class FeaturesOrPropertiesMethodsGenerator(BaseGenerator):
    def __init__(self, params):
        BaseGenerator.__init__(self)
        self.featureStructs, self.pattern = params

    def generate(self):
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
            "MemoryDecompression",
            "LinearColorAttachment",
            "OpticalFlow",
            "RayTracingInvocationReorder",
            "DisplacementMicromap"]
        stream = []
        for fop in self.featureStructs:
            # remove VkPhysicalDevice prefix from structure name
            nameSubStr = fop.name[16:]
            # remove extension type in some cases
            if nameSubStr[-3:] == "KHR":
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
            pnext = next((m for m in fop.members if m.name == "pNext"), None)
            constStr = "// Contains const pNext " if pnext and getattr(pnext, "const", False) else ""
            stream.append(constStr + self.pattern.format(fop.name, nameSubStr))
        self.write(combineLines(indentLines(stream), INL_HEADER))

class DeviceFeatureTestGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        coreFeaturesPattern = re.compile("^VkPhysicalDeviceVulkan([1-9][0-9])Features[0-9]*$")
        featureItems = []
        testFunctions = []

        # iterate over all feature structures
        allFeaturesPattern = re.compile(r"^VkPhysicalDevice\w+Features[1-9]*")

        sortedStructs = sorted(self.vk.structs.values(), key=lambda s: s.name)
        for struct in sortedStructs:
            # skip structures that are not feature structures
            if not allFeaturesPattern.match(struct.name):
                continue
            # skip sType and pNext and just grab third and next attributes
            structureMembers = struct.members[2:]

            items = []
            for member in structureMembers:
                items.append("        FEATURE_ITEM ({0}, {1}),".format(struct.name, member.name))

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
            additionalParams = ( 'memReservationStatMax, isSubProcess' if self.targetApiName == 'vulkansc' else 'isSubProcess' )
            additionalDefs = ( '    VkDeviceObjectReservationCreateInfo memReservationStatMax = context.getResourceInterface()->getStatMax();' if self.targetApiName == 'vulkansc' else '')
            featureItems.append(testBlock.format(struct.name, "\n".join(items), len(items), ("nullptr" if coreFeaturesPattern.match(struct.name) else "&extensionNames"), struct.name[len('VkPhysicalDevice'):], additionalParams, additionalDefs))

            testFunctions.append("createDeviceWithUnsupportedFeaturesTest" + struct.name[len('VkPhysicalDevice'):])

        stream = ['']
        stream.extend(featureItems)
        stream.append("""
void addSeparateUnsupportedFeatureTests (tcu::TestCaseGroup* testGroup)
{
""")
        for x in testFunctions:
            stream.append('\taddFunctionCase(testGroup, "' + camelToSnake(x[len('createDeviceWithUnsupportedFeaturesTest'):]) + '", ' + x + ');')
        stream.append('}\n')
        self.write(combineLines(stream, INL_HEADER))

class MandatoryFeaturesGenerator(BaseGenerator):
    @dataclass
    class StructData:
        names: list[str]
        extensions: list[str]
        structObject: None

    def __init__(self, _):
        BaseGenerator.__init__(self)
        self.usedFeatureStructs = []
        self.uniqueFeatureStructs = []

    def generate(self):

        if self.targetApiName == 'vulkan':
            # check if any of he extensions in EXTENSIONS_TESTED_BY_CTS
            # could be removed from the list because they are part of core
            for extName in EXTENSIONS_TESTED_BY_CTS:
                if extName in self.vk.extensions:
                    ext = self.vk.extensions[extName]
                    if ext.promotedTo and 'VK_VERSION' in ext.promotedTo:
                        print(f'  {extName} is in core, it can be removed from EXTENSIONS_TESTED_BY_CTS list')
            # check if there is no duplicates in EXTENSIONS_TESTED_BY_CTS
            if len(EXTENSIONS_TESTED_BY_CTS) != len(set(EXTENSIONS_TESTED_BY_CTS)):
                print(f'  Remove duplicates from EXTENSIONS_TESTED_BY_CTS list')

        # iterate over all extensions and vulkan versions to
        # find all feature structures that have mandatory fields
        self.addFeatureStructs(self.vk.extensions.values())
        if self.targetApiName == 'vulkan':
            self.addFeatureStructs(self.vk.versions.values())

        # sort structures by name
        self.usedFeatureStructs = sorted(self.usedFeatureStructs)

        # extract needed data for each structure name
        structs = self.vk.structs
        for structName in self.usedFeatureStructs:
            # try to find structure object in vk.structs
            s = structs[structName] if structName in structs else None
            if s is None:
                # try to check if it was promoted to core, KHR or EXT
                structBareName = re.sub(r'[A-Z]+$', '', structName)
                for postfix in ['', 'KHR', 'EXT']:
                    searchedName = structBareName + postfix
                    if searchedName in structs:
                        s = structs[searchedName]
                        break
            if s is None:
                # handle promotions with changed names
                if structName == 'VkPhysicalDeviceVariablePointerFeaturesKHR':
                    s = structs['VkPhysicalDeviceVariablePointersFeatures']
                if structName == 'VkPhysicalDeviceExternalSciBufFeaturesNV':
                    s = structs['VkPhysicalDeviceExternalMemorySciBufFeaturesNV']
                assert s, f'Error: {structName} was not found, more logic to find it should be added'
            # check if struct is not already in uniqueFeatureStructs list
            structProcessedPreviously = False
            for ufs in self.uniqueFeatureStructs:
                structProcessedPreviously = (ufs.names[0] == s.name)
                if structProcessedPreviously:
                    break
            # if struct was already processed, skip it
            if structProcessedPreviously:
                continue
            names = [s.name] + s.aliases
            self.uniqueFeatureStructs.append(self.StructData(names, s.extensions, s))

        self.write(INL_HEADER)
        self.write('bool canUseFeaturesStruct (const vector<VkExtensionProperties>& deviceExtensions, uint32_t usedApiVersion,\n'
                   '\t\t\t\tconst char* extension, const char* extensionPromotedFrom = nullptr)\n'
                   '{\n'
                   '\tif (isCoreDeviceExtension(usedApiVersion, extension))\n'
                   '\t\treturn true;\n'
                   '\tif (isExtensionStructSupported(deviceExtensions, RequiredExtension(extension)))\n'
                   '\t\treturn true;\n'
                   '\treturn extensionPromotedFrom && isExtensionStructSupported(deviceExtensions, RequiredExtension(extensionPromotedFrom));\n'
                   '}\n'
                   '\n'
                   'void checkBasicMandatoryFeatures(const vkt::Context& context, std::vector<std::string>& failMesages)\n{\n'
                   '\tif (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))\n'
                   '\t\tTCU_THROW(NotSupportedError, "Extension VK_KHR_get_physical_device_properties2 is not present");\n'
                   '\n'
                   '\tVkPhysicalDevice\t\t\t\t\tphysicalDevice\t\t= context.getPhysicalDevice();\n'
                   '\tconst InstanceInterface&\t\t\tvki\t\t\t\t\t= context.getInstanceInterface();\n'
                   '\tconst vector<VkExtensionProperties>\tdeviceExtensions\t= enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);\n'
                   '\tconst uint32_t\t\t\t\t\t\tusedApiVersion\t\t= context.getUsedApiVersion();\n'
                   '\n'
                   '\tvk::VkPhysicalDeviceFeatures2 coreFeatures = initVulkanStructure();\n'
                   '\tconst auto addFeatures = makeStructChainAdder(&coreFeatures);\n')

        # add feature structs to chain if required extension(/vk version) is supported
        for fs in self.uniqueFeatureStructs:

            names = ', '.join(fs.names)
            extensions = ', '.join(fs.extensions)
            self.write(f'\t// {names} for ext [{extensions}]')
            typeName = fs.names[0]
            varName = self.getVarNameFromType(typeName)

            condition = ''
            if len(fs.extensions) > 0:
                assert len(fs.extensions) < 3
                extensionParams = '"' + '", "'.join(fs.extensions) + '"'
                condition = f'canUseFeaturesStruct(deviceExtensions, usedApiVersion, {extensionParams})'
            else:
                # use different condition for blob feature structures
                version = fs.structObject.version.name[-3:].replace('_', ', ')
                condition = f'context.contextSupports(vk::ApiVersion(0, {version}, 0))'

            self.write(f'\tvk::{typeName} {varName} = initVulkanStructure();\n'
                       f'\tif ({condition})\n'
                       f'\t\taddFeatures(&{varName});\n')

        self.write('\tcontext.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);\n')

        skippedExtensions = []
        for ext in self.vk.extensions.values():
            # KHR and EXT extensions that are not core need to be cross check with
            # EXTENSIONS_TESTED_BY_CTS, we can check mandatory features only
            # for extensions that are tested in the CTS
            if ext.featureRequirement and ('KHR' in ext.name or 'EXT' in ext.name) and \
               (not ext.promotedTo or 'VK_VERSION' not in ext.promotedTo) and \
               (ext.name not in EXTENSIONS_TESTED_BY_CTS):
               skippedExtensions.append(ext.name)
               continue
            # generate check
            for fr in ext.featureRequirement:
                self.checkRequirement(ext.name, fr, f'isExtensionStructSupported(deviceExtensions, RequiredExtension("{ext.name}"))')

        if skippedExtensions:
            print("  Not testing mandatory features for extensions that aren't on EXTENSIONS_TESTED_BY_CTS list:")
            for extName in skippedExtensions:
                print(f'    {extName}')
            print("  If any of the above extensions should be tested, add them to the list.")

        if self.targetApiName == 'vulkan':
            for ver in self.vk.versions.values():
                for fr in ver.featureRequirement:
                    verStr = ver.name[-3:].replace('_', ', ')
                    self.checkRequirement(ver.name, fr, f'context.contextSupports(vk::ApiVersion(0, {verStr}, 0))')

        self.write('}\n')

    def addFeatureStructs(self, objectList):
        # generic code that handles list of Extension or Version objects
        for obj in objectList:
            for fr in obj.featureRequirement:
                self.addSingleFeatureStruct(fr.struct)
                # check if dependency is also a feature structure
                if fr.depends is not None:
                    separatedChecks = fr.depends.replace('+', ',').replace('(', '').replace(')', '').split(',')
                    for definedCheck in separatedChecks:
                        if '::' in definedCheck:
                            structName, _ = definedCheck.split('::')
                            self.addSingleFeatureStruct(structName)

    def addSingleFeatureStruct(self, structName):
        # skip VkPhysicalDeviceFeatures as it is always present
        if structName == 'VkPhysicalDeviceFeatures':
            return
        # skip structures that are already in usedFeatureStructs
        if structName in self.usedFeatureStructs:
            return
        self.usedFeatureStructs.append(structName)

    def checkRequirement(self, objectName, fr, initialCondition):
        # objectName is either extension name or vulkan version name
        varName = self.getVarNameFromType(fr.struct)
        structCondition = initialCondition
        fieldCondition = ''
        fieldMessage = ''
        # check if multiple fields are specified
        if ',' in fr.field:
            fieldSet = fr.field.split(',')
            fieldCondition = '(' + ') && ('.join([f'{varName}.{fs} == VK_FALSE' for fs in fieldSet]) + ')'
            fieldMessage = ' or '.join(fieldSet)
        else:
            fieldCondition = f'{varName}.{fr.field} == VK_FALSE'
            fieldMessage = fr.field
        # small number of feature bits are mandatory but under additional conditions
        if fr.depends is not None:
            #print (f'{objectName} - {fr.struct}::{fr.field} depends: {fr.depends}')
            # we convert dependency string from specification to C++ code
            additionalCondition = fr.depends.replace(',', ' || ').replace('+', ' && ')
            # each individula part of dependency is converted separately
            separatedChecks = fr.depends.replace('+', ',').replace('(', '').replace(')', '').split(',')
            for definedCheck in separatedChecks:
                cppCheck = ''
                if definedCheck.startswith('VK_VERSION'):
                    # handle vulkan version
                    version = definedCheck[-3:].replace('_', ', ')
                    cppCheck = f'context.contextSupports(vk::ApiVersion(0, {version}, 0))'
                elif definedCheck.startswith('VK_'):
                    # handle extension
                    cppCheck = f'isExtensionStructSupported(deviceExtensions, RequiredExtension("{definedCheck}"))'
                elif '::' in definedCheck:
                    # handle other structure field
                    # note: assuming structure is part of chain, if not then logic that
                    # checks this before calling getPhysicalDeviceFeatures2 should be added
                    structName, fieldName = definedCheck.split('::')
                    structVarName = self.getVarNameFromType(structName)
                    cppCheck = f'{structVarName}.{fieldName}'
                else:
                    assert False, f'Error: unknown dependency {definedCheck} for {objectName}'
                # replace defined check with cpp check so that we perserve potential parentheses
                additionalCondition = additionalCondition.replace(definedCheck, cppCheck)
            # add additional conditions to structCondition
            if ',' in fr.depends:
                additionalCondition = '(' + additionalCondition + ')'
            structCondition += ' && ' + additionalCondition
        # write code that checks if mandatory feature is supported
        self.write(f'\t// {fr.struct}\n'
                    f'\tif ( {structCondition} )\n'
                    '\t{\n'
                    f'\t\tif ( {fieldCondition} )\n'
                    f'\t\t\tfailMesages.push_back("{fieldMessage}");\n'
                    '\t}\n')

    def getVarNameFromType(self, structName):
        if structName == 'VkPhysicalDeviceFeatures':
            return 'coreFeatures.features'
        # dependencies in spec may use older structure names, in generated code
        # we generate variable name always from most recent structure name
        for fs in self.uniqueFeatureStructs:
            if structName in fs.names:
                s = fs.names[0]
                return s[2].lower() + s[3:]
        assert False, f'Error: {structName} not found in uniqueFeatureStructs'

class ExtensionListGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        limitToDevice = 'Device' in self.filename
        listName = 'Device' if limitToDevice else 'Instance'
        extensionList = []
        for extension in self.vk.extensions.values():
            # make sure extension name starts with VK_KHR
            if not extension.name.startswith('VK_KHR'):
                continue
            # make sure extension has proper type - device or instance
            if limitToDevice == extension.device:
                extensionList.append(extension.name)
        extensionList.sort()
        # write list of all found extensions
        self.write(INL_HEADER)
        self.write(f'static const char* s_allowed{listName}KhrExtensions[] =\n{{')
        for n in extensionList:
            self.write('\t"' + n + '",')
        self.write('};\n')

class ApiExtensionDependencyInfoGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def genHelperFunctions(self):
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

    def genExtDepArray(self, extType):
        extensionList = []
        maxExtLength = 0
        searchForDeviceExt = False
        extVector = 'vIEP'
        othVector = 'vDEP'
        if extType == 'device':
            searchForDeviceExt = True
            extVector, othVector = othVector, extVector        # swap
        # iterate over all extension that are of specified type and that have requirements
        self.sortedExtensions = sorted(self.vk.extensions.values(), key=lambda item: item.name)
        for ext in self.sortedExtensions:
            if searchForDeviceExt:
                if not ext.device:
                    continue
            elif not ext.instance:
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
            if ext.promotedTo is not None and 'VK_VERSION' in ext.promotedTo:
                p = ext.promotedTo
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
            finalCondition = transformDependsToCondition(ext.depends, self.vk, 'isCompatible(%s, %s, v)', 'isSupported(%s, "%s")', ext.name)
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

    def genApiVersions(self):
        yield 'static const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>\treleasedApiVersions[]\t='
        yield '{'
        for v in reversed(self.vk.versions.values()):
            apiVariant = '0' if self.targetApiName == 'vulkan' else '1'
            major, minor = v.name[-3:].split('_')
            version = (int(apiVariant) << 29) | (int(major) << 22) | (int(minor) << 12)
            yield '\tstd::make_tuple({}, {}, {}, {}),'.format(version, apiVariant, major, minor)
        yield '\tstd::make_tuple(4194304, 0, 1, 0)'
        yield '};'

    def parseExtensionDependencies(self, extDeps, ext):
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
            major, minor, requiredVerFound = self.parseExtensionDependencies(extDeps[groupLength:], ext)
        return major, minor, requiredVerFound

    def genRequiredCoreVersions(self):
        yield 'static const std::tuple<uint32_t, uint32_t, const char*>\textensionRequiredCoreVersion[]\t ='
        yield '{'
        versionPattern = "[A-Z]+_VERSION_([0-9]+)_([0-9]+)"
        for ext in self.sortedExtensions:
            # skip video extensions
            if 'vulkan_video_' in ext.name:
                continue
            major, minor = 1, 0
            if ext.depends is not None:
                major, minor, requiredVerFound = self.parseExtensionDependencies(ext.depends, ext)
                if not requiredVerFound:
                    # find all extensions that are dependencies of this one
                    matches = re.findall(r"VK_\w+", ext.depends, re.M)
                    for m in matches:
                        for de in self.sortedExtensions:
                            if de.name == m:
                                if de.depends is not None:
                                    # check if the dependency states explicitly the required vulkan version and pick the higher one
                                    newMajor, newMinor, requiredVerFound = self.parseExtensionDependencies(de.depends, de)
                                    if requiredVerFound:
                                        if newMajor > major:
                                            major, minor = newMajor, newMinor
                                        elif newMajor == major and newMinor > minor:
                                            minor = newMinor
                                break
            yield '\tstd::make_tuple({}, {}, "{}"),'.format(major, minor, ext.name)
        yield '};'

    def generate(self):
        self.write(INL_HEADER)
        for l in self.genHelperFunctions():
            self.write(l)
        for l in self.genExtDepArray('instance'):
            self.write(l)
        for l in self.genExtDepArray('device'):
            self.write(l)
        for l in self.genApiVersions():
            self.write(l)
        for l in self.genRequiredCoreVersions():
            self.write(l)

class EntryPointValidationGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
        # keys are instance extension names and value is list of device-level functions
        instExtDeviceFunDict = {}
        # iterate over all extensions and find instance extensions
        for ext in self.vk.extensions.values():
            if not ext.instance:
                continue
            # find device functions added by instance extension
            for extCommand in ext.commands:
                if not extCommand.device:
                    continue
                if ext.name not in instExtDeviceFunDict:
                    instExtDeviceFunDict[ext.name] = []
                instExtDeviceFunDict[ext.name].append(extCommand.name)
        # write data to file
        self.write(INL_HEADER)
        self.write('std::map<std::string, std::vector<std::string> > instExtDeviceFun\n{')
        for extName in instExtDeviceFunDict:
            self.write(f'\t{{ "{extName}",\n\t\t{{')
            for fun in instExtDeviceFunDict[extName]:
                self.write(f'\t\t\t"{fun}",')
            self.write('\t\t}\n\t},')
        self.write('};')

class GetDeviceProcAddrGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate(self):
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
        self.write(INL_HEADER)
        self.write('#include "tcuCommandLine.hpp"\n'
                   '#include "vktTestCase.hpp"\n'
                   '#include "vkPlatform.hpp"\n'
                   '#include "vkDeviceUtil.hpp"\n'
                   '#include "vkQueryUtil.hpp"\n'
                   '#include "vktCustomInstancesDevices.hpp"\n'
                   '#include "vktTestCase.hpp"\n'
                   '#include "vktTestCaseUtil.hpp"\n'
                   '\nnamespace vkt\n{\n\n'
                   'using namespace vk;\n\n')
        self.write(testBlockStart)
        sortedExtensions = sorted(self.vk.extensions.values(), key=lambda item: item.name)
        for e in sortedExtensions:
            if len(e.commands) == 0:
                continue
            self.write('\n\t\t// "' + e.name)
            sortedCommands = sorted(e.commands, key=lambda item: item.name)
            for c in sortedCommands:
                self.write('\t\t"' + c.name + '",')
        self.write(testBlockEnd)

        # function to create tests
        self.write("void addGetDeviceProcAddrTests (tcu::TestCaseGroup* testGroup)\n{")
        self.write('\taddFunctionCase(testGroup, "non_enabled", testGetDeviceProcAddr);')
        self.write('}\n')
        self.write('}\n')

class ProfileTestsGenerator(BaseGenerator):
    def __init__(self, jsonFilesList):
        BaseGenerator.__init__(self)
        self.jsonFilesList = jsonFilesList

    # helper function; workaround for lack of information in json about limit type
    def getLimitMacro(self, propName, propComponent):
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
    def constructStruct(self, structName, structInitNamesList, structInitList):
        # skip structures that already are in the chain
        if structName in structInitNamesList:
            return
        structInitNamesList.append(structName)
        # construct structure instance and connect it to chain
        parentStruct = "" if (len(structInitNamesList) == 3) else "&vk" + structInitNamesList[-2]
        structInitList.append(f"\tVkPhysicalDevice{structName} vk{structName} = initVulkanStructure({parentStruct});")

    # helper function handling strings representing property limit checks
    def addPropertyEntries(self, structName, propName, propLimit, propertyTableItems):
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
                limitMacro = self.getLimitMacro(name, i)
                limitValue = "true" if limit == True else limit
                if limitValue == False:
                     limitValue = "false"
                limitValue = limitValue[i] if limitComponentCount > 1 else limitValue
                propertyTableItems += [f"PN({combinedStructName}.{name}{componentAccess}), {limitMacro}({limitValue})"]

    def generate (self):
        vkpdLen = len("VkPhysicalDevice")
        profilesList = []
        stream = []

        for jsonFile in self.jsonFilesList:
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
                                self.constructStruct(structName, featureStructInitNamesList, featureStructInitList)
                                for feature in featureStructList[featureStruct]:
                                    featureTableItems.append(f"vk{structName}, {feature}")
                                featureTableItems.append("\n")
                        if "properties" in capabilityDefinition:
                            propertyStructList = capabilityDefinition["properties"]
                            propertyTableItems.append(f"\t\t// {capabilityName}");
                            for propertyStruct in propertyStructList:
                                structName = propertyStruct[vkpdLen:]
                                self.constructStruct(structName, propertyStructInitNamesList, propertyStructInitList)
                                for propName, propLimit in propertyStructList[propertyStruct].items():
                                    self.addPropertyEntries("vk" + structName, propName, propLimit, propertyTableItems)
                                propertyTableItems.append("\n")
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
        self.write(INL_HEADER)
        for l in stream:
            self.write(l)
        self.write("static const std::vector<ProfileEntry> profileEntries {")
        for l in profilesList:
            self.write(l)
        self.write("};")

class FormatListsGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generate (self):

        self.write(INL_HEADER)
        self.write('// note: using inline C++17 feature instead of extern\n')

        bitClassesDict = {}
        for f in self.vk.formats.values():
            if f.className.endswith("-bit"):
                bitClassesDict[int(f.className.split('-')[0])] = f.className

        for bitValue, bitClass in bitClassesDict.items():
            arraySubName = bitClass.replace('-b','B')
            def compatibleFormatsCheckFun(f):
                # skip vendor extension formats
                if self.isPartOfVendorExtension(f.name):
                    return False
                if f.className == bitClass:
                    return True
                # add selected compressed formats to 64-bit+ formats
                if bitValue >= 64:
                    return f.compressed is not None and f.blockSize == (bitValue / 8)
                return False
            self.writeList(f'compatibleFormats{arraySubName}', compatibleFormatsCheckFun)

        for intClass in ['SINT', 'UINT']:
            arraySubName = intClass.replace('NT','nt')
            def intCompatibleFormatsCheckFun(f):
                # find int formats (but don't include depth/stencil formats)
                if intClass in f.name and not f.className.startswith('D') and not f.className.startswith('S'):
                    return not self.isPartOfVendorExtension(f.name)
                return False
            self.writeList(f'compatibleFormats{arraySubName}s', intCompatibleFormatsCheckFun)

        floatVariants = ['UNORM', 'SNORM', 'USCALED', 'SSCALED', 'SFLOAT', 'UFLOAT']
        def compatibleFormatsFloatsCheckFun(f):
            if any(sub in f.name for sub in floatVariants):
                if f.compressed is None and not f.className.startswith('D'):
                    if f.chroma:
                        # accept only one chroma format to match what was in the list before generation
                        return f.className == '64-bit R10G10B10A10'
                    # skip vendor extension formats
                    return not self.isPartOfVendorExtension(f.name)
            return False
        self.writeList(f'compatibleFormatsFloats', compatibleFormatsFloatsCheckFun)

        def compressedFormatsFloatsCheckFun(f):
            if f.compressed is not None and any(sub in f.name for sub in floatVariants):
                # skip formats added by VK_EXT_texture_compression_astc_hdr to
                # avoid adding thousends of tests that were not there before
                # generation of format lists
                if 'ASTC' in f.name and 'SFLOAT' in f.name:
                    return False
                # skip vendor extension formats
                return not self.isPartOfVendorExtension(f.name)
            return False
        self.writeList(f'compressedFormatsFloats', compressedFormatsFloatsCheckFun)

        compatibleFormatsSrgbCheckFun = lambda f: f.compressed is None and 'SRGB' in f.name
        self.writeList(f'compatibleFormatsSrgb', compatibleFormatsSrgbCheckFun)

        def compressedFormatsSrgbCheckFun(f):
            return not self.isPartOfVendorExtension(f.name) and f.compressed is not None and 'SRGB' in f.name
        self.writeList(f'compressedFormatsSrgb', compressedFormatsSrgbCheckFun)

        stencilFormatsCheckFun = lambda f: 'S8' in f.className
        self.writeList(f'stencilFormats', stencilFormatsCheckFun)

        depthFormatsCheckFun = lambda f: f.className.startswith('D')
        self.writeList(f'depthFormats', depthFormatsCheckFun)

        depthFormatsCheckFun = lambda f: f.className.startswith('D') and not 'S' in f.className
        self.writeList(f'depthOnlyFormats', depthFormatsCheckFun)

        depthAndStencilFormatsCheckFun = lambda f: f.className.startswith('D') or f.className.startswith('S')
        self.writeList(f'depthAndStencilFormats', depthAndStencilFormatsCheckFun)

        ycbcrFormatsCheckFun = lambda f: not self.isPartOfVendorExtension(f.name) and f.chroma
        self.writeList(f'ycbcrFormats', ycbcrFormatsCheckFun)

        ycbcrCompatibileFormatsCheckFun = lambda f: ('X6_UNORM' in f.name or 'X4_UNORM' in f.name) and len(f.components) < 4
        self.writeList(f'ycbcrCompatibileFormats', ycbcrCompatibileFormatsCheckFun)

        disjointPlanesCheckFun = lambda f: not self.isPartOfVendorExtension(f.name) and 'plane' in f.className
        self.writeList(f'disjointPlanesFormats', disjointPlanesCheckFun)

        xChromaSubsampledCheckFun = lambda f: not self.isPartOfVendorExtension(f.name) and '422_UNORM' in f.name
        self.writeList(f'xChromaSubsampledFormats', xChromaSubsampledCheckFun)

        xyChromaSubsampledCheckFun = lambda f: not self.isPartOfVendorExtension(f.name) and '420_UNORM' in f.name
        self.writeList(f'xyChromaSubsampledFormats', xyChromaSubsampledCheckFun)

        allFormatsCheckFun = lambda f: not self.isPartOfVendorExtension(f.name)
        self.writeList(f'allFormats', allFormatsCheckFun)

        def nonPlanarFormatsCheckFun(f):
            # skip vendor extension formats
            if self.isPartOfVendorExtension(f.name):
                return False
            return 'plane' not in f.className
        self.writeList(f'nonPlanarFormats', nonPlanarFormatsCheckFun)

        def planarFormatsCheckFun(f):
            # skip vendor extension formats
            if self.isPartOfVendorExtension(f.name):
                return False
            return 'plane' in f.className
        self.writeList(f'planarFormats', planarFormatsCheckFun)

        # helper function used in generation of few folowing lists
        def isCommonlySkippedFormat(f):
            return self.isPartOfVendorExtension(f.name) or \
                    'plane' in f.className or f.chroma

        def basicColorCheckFun(f):
            if isCommonlySkippedFormat(f):
                return False
            return "-bit" in f.className
        self.writeList(f'basicColorFormats', basicColorCheckFun)

        def basicUnsignedFloatFormatsCheckFun(f):
           return not self.isPartOfVendorExtension(f.name) and\
                ('UNORM' in f.name or 'UFLOAT' in f.name) and\
                not f.compressed and not f.className.startswith('D') and\
                'E5B9G9R9' not in f.name
        self.writeList(f'basicUnsignedFloatFormats', basicUnsignedFloatFormatsCheckFun)

        def bufferViewAccessCheckFun(f):
            if 'UFLOAT' in f.name or 'SRGB' in f.name:
                return False
            if isCommonlySkippedFormat(f):
                return False
            if "-bit" in f.className:
                # accept only up to 64-bit formats because bigger formats are not supported by vkImageUtil in cts framework
                bitCount = int(f.className.split('-')[0])
                if bitCount > 64:
                    return False
                return True
            return False
        self.writeList(f'bufferViewAccessFormats', bufferViewAccessCheckFun)

        def pipelineImageCheckFun(f):
            if isCommonlySkippedFormat(f):
                return False
            if f.className.startswith('D') or f.className.startswith('S'):
                return False
            if f.className.startswith('BC'):
                return False
            if f.className.startswith('ASTC') and 'SFLOAT' in f.name:
                return False
            if '64' in f.name:
                return False
            return True
        self.writeList(f'pipelineImageFormats', pipelineImageCheckFun)

    def writeList(self, listName, checkCallback):
        listOfFormatsNotSupportedBySC = [
            'VK_FORMAT_A1B5G5R5_UNORM_PACK16',
            'VK_FORMAT_A8_UNORM',

            'VK_FORMAT_R16G16_SFIXED5_NV',
            'VK_FORMAT_R10X6_UINT_PACK16_ARM',
            'VK_FORMAT_R10X6G10X6_UINT_2PACK16_ARM',
            'VK_FORMAT_R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM',
            'VK_FORMAT_R12X4_UINT_PACK16_ARM',
            'VK_FORMAT_R12X4G12X4_UINT_2PACK16_ARM',
            'VK_FORMAT_R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM',
            'VK_FORMAT_R14X2_UINT_PACK16_ARM',
            'VK_FORMAT_R14X2G14X2_UINT_2PACK16_ARM',
            'VK_FORMAT_R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM',
            'VK_FORMAT_R14X2_UNORM_PACK16_ARM',
            'VK_FORMAT_R14X2G14X2_UNORM_2PACK16_ARM',
            'VK_FORMAT_R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM',
            'VK_FORMAT_G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM',
            'VK_FORMAT_G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM',
            'VK_FORMAT_R8_BOOL_ARM',

            # removed from Vulkan SC test set: VK_IMG_format_pvrtc extension does not exist in Vulkan SC
            'VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG',
            'VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG',
            'VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG',
            'VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG',
            'VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG',
            'VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG',
            'VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG',
            'VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG',
            'VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT',
            'VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT',
            'VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT',
            'VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT',
            'VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT',
            'VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT',
        ]

        formats = []
        for f in self.vk.formats.values():
            if self.targetApiName == "vulkansc" and f.name in listOfFormatsNotSupportedBySC:
                continue
            if checkCallback(f):
                formats.append(f.name)

        self.write(f'inline const std::vector<VkFormat> {listName}\n{{')
        for formatName in formats:
            self.write('\t' + formatName + ',')
        self.write('};\n')

    def isPartOfVendorExtension(self, name):
        return any(name.endswith(postfix) for postfix in EXTENSION_POSTFIXES_VENDOR)

class ConformanceVersionsGenerator(BaseGenerator):
    def __init__(self, _):
        BaseGenerator.__init__(self)

    def generateFromCache(self, cacheVkObjectData, genOpts):
        # on Jenkins, Git operations are not executed, resulting in empty file generation;
        # to resolve this issue, we override the generateFromCache method instead of generate
        # and open the file for writing only when the data is valid

        logging.debug("Preparing to generate " + genOpts.filename)
        # get list of all vulkan/vulkansc tags from git
        remote_urls = os.popen("git remote -v").read().split('\n')
        remote_url = None
        url_regexp = r'\bgerrit\.khronos\.org\b.*\bvk-gl-cts\b'
        for line in remote_urls:
            if re.search(url_regexp, line, re.IGNORECASE) is not None:
                remote_url = line.split()[1]
                break
        listOfTags = os.popen("git ls-remote -t %s" % (remote_url)).read()
        pattern = rf"{self.targetApiName}-cts-(\d+).(\d+).(\d+).(\d+)"
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

        OutputGenerator.beginFile(self, genOpts)
        self.write(INL_HEADER)
        self.write(combineLines(stream))
        OutputGenerator.endFile(self)

# <vulkan_object_issue_workaround>
# some functions and structures for Vulkan SC use names from regular Vulkan e.g.
# vkCmdBindVertexBuffers2 is provided instead of non promoted vkCmdBindVertexBuffers2EXT
def postProcess(vk):
    khrCommands = [
        # VK_KHR_copy_commands2
        'vkCmdBlitImage2',
        'vkCmdCopyBuffer2',
        'vkCmdCopyBufferToImage2',
        'vkCmdCopyImage2',
        'vkCmdCopyImageToBuffer2',
        'vkCmdResolveImage2',
        # VK_KHR_synchronization2
        'vkCmdPipelineBarrier2',
        'vkCmdResetEvent2',
        'vkCmdSetEvent2',
        'vkCmdWaitEvents2',
        'vkCmdWriteTimestamp2',
        'vkQueueSubmit2',
    ]
    extCommands = [
        # VK_EXT_extended_dynamic_state
        'vkCmdBindVertexBuffers2',
        'vkCmdSetCullMode',
        'vkCmdSetDepthBoundsTestEnable',
        'vkCmdSetDepthCompareOp',
        'vkCmdSetDepthTestEnable',
        'vkCmdSetDepthWriteEnable',
        'vkCmdSetFrontFace',
        'vkCmdSetPrimitiveTopology',
        'vkCmdSetScissorWithCount',
        'vkCmdSetStencilOp',
        'vkCmdSetStencilTestEnable',
        'vkCmdSetViewportWithCount',
        # VK_EXT_extended_dynamic_state2
        'vkCmdSetDepthBiasEnable',
        'vkCmdSetLogicOp',
        'vkCmdSetPatchControlPoints',
        'vkCmdSetPrimitiveRestartEnable',
        'vkCmdSetRasterizerDiscardEnable',
        # VK_EXT_line_rasterization
        'vkCmdSetLineStipple',
    ]
    # rename commands that shoud have EXT or KHR postfix for SC
    def renameCommands(commandList, postfix):
        for commandName in commandList:
            if commandName in vk.commands:
                newName = commandName + postfix
                cObj = vk.commands.pop(commandName)
                cObj.name = newName
                vk.commands[newName] = cObj
    renameCommands(khrCommands, 'KHR')
    renameCommands(extCommands, 'EXT')
    # remove incorrect commands
    incorrectCommands = [
        'vkGetDeviceImageSparseMemoryRequirements',
    ]
    for ic in incorrectCommands:
        if ic in vk.commands:
            vk.commands.pop(ic)
    # add aliases for structures with incorrect names
    khrStructs = [
        # VK_KHR_global_priority
        'VkQueueGlobalPriority',
        # VK_KHR_vertex_attribute_divisor
        'VkVertexInputBindingDivisorDescription',
        'VkPhysicalDeviceVertexAttributeDivisorFeatures'
        'VkPhysicalDeviceVertexAttributeDivisorProperties'
        'VkPipelineVertexInputDivisorStateCreateInfo'
    ]
    for s in vk.structs.values():
        if s.name in khrStructs:
            s.alias = s.name + 'KHR'
# </vulkan_object_issue_workaround>

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
    isSC = (args.api=='SC')

    # if argument was specified it is interpreted as a path to which .inl files will be written
    SetOutputDirectory(DEFAULT_OUTPUT_DIR[args.api] if args.outdir == '' else args.outdir)
    SetTargetApiName('vulkansc' if isSC else 'vulkan')
    SetMergedApiNames(None)

    # parameters used by some of generators
    featuresForDDDefsPattern   = "const {0}&\tget{1}\t(void) const {{ return m_deviceFeatures.getFeatureType<{0}>();\t}}"
    propertiesForDDDefsPattern = "const {0}&\tget{1}\t(void) const {{ return m_deviceProperties.getPropertyType<{0}>();\t}}"
    contextDeclPattern         = "const vk::{0}&\tget{1}\t(void) const;"
    contextDefsPattern         = "const vk::{0}&\tContext::get{1}\t(void) const {{ return m_device->get{1}();\t}}"

    # objects shared betwean some generators
    rawVkXml = etree.parse(os.path.join(VULKAN_XML_DIR, "vk.xml"))
    vkObject = None
    featureStructs = []
    propertyStructs = []

    # array of generators
    @dataclass
    class GenData:
        filename: str
        generatorType: BaseGenerator
        params: (Any | None) = None
    generatorList = [

        GenData('vkBasicTypes.inl',                           BasicTypesGenerator),
        GenData('vkHandleType.inl',                           HandleTypeGenerator),
        GenData('vkStructTypes.inl',                          StructTypesGenerator),

        GenData('vkDeviceFeatures.inl',                       FeaturesOrPropertiesGenericGenerator, (featureStructs)),
        GenData('vkDeviceFeaturesForDefaultDeviceDefs.inl',   FeaturesOrPropertiesMethodsGenerator, (featureStructs, featuresForDDDefsPattern)),
        GenData('vkDeviceFeaturesForContextDecl.inl',         FeaturesOrPropertiesMethodsGenerator, (featureStructs, contextDeclPattern)),
        GenData('vkDeviceFeaturesForContextDefs.inl',         FeaturesOrPropertiesMethodsGenerator, (featureStructs, contextDefsPattern)),
        GenData('vkDeviceFeatureTest.inl',                    DeviceFeatureTestGenerator),
        GenData("vkDeviceFeatures2.inl",                      DeviceFeatures2Generator),

        GenData('vkDeviceProperties.inl',                     FeaturesOrPropertiesGenericGenerator, (propertyStructs)),
        GenData('vkDevicePropertiesForDefaultDeviceDefs.inl', FeaturesOrPropertiesMethodsGenerator, (propertyStructs, propertiesForDDDefsPattern)),
        GenData('vkDevicePropertiesForContextDecl.inl',       FeaturesOrPropertiesMethodsGenerator, (propertyStructs, contextDeclPattern)),
        GenData('vkDevicePropertiesForContextDefs.inl',       FeaturesOrPropertiesMethodsGenerator, (propertyStructs, contextDefsPattern)),

        GenData('vkVirtualPlatformInterface.inl',             InterfaceDeclarationGenerator),
        GenData('vkVirtualInstanceInterface.inl',             InterfaceDeclarationGenerator),
        GenData('vkVirtualDeviceInterface.inl',               InterfaceDeclarationGenerator),
        GenData('vkConcretePlatformInterface.inl',            InterfaceDeclarationGenerator),
        GenData('vkConcreteInstanceInterface.inl',            InterfaceDeclarationGenerator),
        GenData('vkConcreteDeviceInterface.inl',              InterfaceDeclarationGenerator),

        GenData('vkFunctionPointerTypes.inl',                 FunctionPointerTypesGenerator),
        GenData('vkPlatformFunctionPointers.inl',             FunctionPointersGenerator),
        GenData('vkInstanceFunctionPointers.inl',             FunctionPointersGenerator),
        GenData('vkDeviceFunctionPointers.inl',               FunctionPointersGenerator),

        GenData('vkInitPlatformFunctionPointers.inl',         InitFunctionPointersGenerator),
        GenData('vkInitInstanceFunctionPointers.inl',         InitFunctionPointersGenerator),
        GenData('vkInitDeviceFunctionPointers.inl',           InitFunctionPointersGenerator),

        GenData('vkPlatformDriverImpl.inl',                   FuncPtrInterfaceImplGenerator),
        GenData('vkInstanceDriverImpl.inl',                   FuncPtrInterfaceImplGenerator),
        GenData('vkDeviceDriverImpl.inl',                     FuncPtrInterfaceImplGenerator),

        GenData('vkStrUtil.inl',                              StrUtilProtoGenerator),
        GenData('vkStrUtilImpl.inl',                          StrUtilImplGenerator),

        GenData('vkRefUtil.inl',                              RefUtilGenerator),
        GenData('vkRefUtilImpl.inl',                          RefUtilGenerator),

        GenData('vkGetStructureTypeImpl.inl',                 GetStructureTypeImplGenerator),
        GenData('vkTypeUtil.inl',                             TypeUtilGenerator),
        GenData('vkNullDriverImpl.inl',                       NullDriverImplGenerator),
        GenData('vkSupportedExtensions.inl',                  SupportedExtensionsGenerator),
        GenData('vkCoreFunctionalities.inl',                  CoreFunctionalitiesGenerator),
        GenData('vkExtensionFunctions.inl',                   ExtensionFunctionsGenerator, (rawVkXml)),
        GenData('vkMandatoryFeatures.inl',                    MandatoryFeaturesGenerator),
        GenData('vkInstanceExtensions.inl',                   ExtensionListGenerator),
        GenData('vkDeviceExtensions.inl',                     ExtensionListGenerator),
        GenData('vkKnownDriverIds.inl',                       DriverIdsGenerator),
        GenData('vkObjTypeImpl.inl',                          ObjTypeImplGenerator),
        GenData('vkApiExtensionDependencyInfo.inl',           ApiExtensionDependencyInfoGenerator),
        GenData('vkEntryPointValidation.inl',                 EntryPointValidationGenerator),
        GenData('vkGetDeviceProcAddr.inl',                    GetDeviceProcAddrGenerator),
        GenData('vkFormatLists.inl',                          FormatListsGenerator),
        GenData('vkKnownConformanceVersions.inl',             ConformanceVersionsGenerator),

        # NOTE: when new generators are added then they should also be added to the
        # vk-gl-cts\external\vulkancts\framework\vulkan\CMakeLists.txt outputs list
    ]

    # append api-specific generators
    if isSC:
        generatorList.append(GenData('vkDeviceDriverSCImpl.inl', FuncPtrInterfaceSCImplGenerator, None))
    else:
        profileList = [os.path.join(VULKAN_XML_DIR, "profiles", "VP_KHR_roadmap.json")]
        #profileList += [os.path.join(VULKAN_XML_DIR, "profiles", "VP_ANDROID_baseline_2022.json"]
        generatorList.append(GenData('vkProfileTests.inl', ProfileTestsGenerator, (profileList)))

    for i, generatorData in enumerate(generatorList):
        gen = generatorData.generatorType(generatorData.params)
        print('[' + (' ' * (i<9)) + f'{i+1}/{len(generatorList)}] Generating {generatorData.filename}')

        # execute generator; first generator creates vulkan_object, remaining generators reuse it
        if vkObject is None:
            bgo = BaseGeneratorOptions(generatorData.filename, videoXmlPath = os.path.abspath(os.path.join(VULKAN_XML_DIR, "video.xml")))
            reg = Registry(gen, bgo)
            reg.loadElementTree(rawVkXml)
            reg.apiGen()
            # memorize vulkan object
            vkObject = gen.vk

            # <vulkan_object_issue_workaround>
            if isSC:
                postProcess(vkObject)
            # </vulkan_object_issue_workaround>
        else:
            # reuse vulkan object
            reg = Registry(gen, BaseGeneratorOptions(generatorData.filename))
            reg.gen.generateFromCache(vkObject, reg.genOpts)
