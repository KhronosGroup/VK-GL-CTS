# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2018 The Android Open Source Project
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
import sys
import argparse

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts"))

import khr_util.format
import khr_util.registry_cache
from collections import defaultdict

# At the moment we don't create vkApiExtensionDependencyInfo.inl for Vulkan SC due to vk.xml not being public.
# All other code modifications are here, but script is not being called from check_build_sanity.py
VK_SOURCE = { ''        : khr_util.registry_cache.RegistrySource(
                                                "https://github.com/KhronosGroup/Vulkan-Docs.git",
                                                "xml/vk.xml",
                                                "cee0f4b12acde766e64d0d038b03458c74bb67f1",
                                                "eb31286278b1ecf55ae817198a4238f82ea8fe028aa0631e2c1b09747f10ebb4"),
                                    'SC'    : khr_util.registry_cache.RegistrySource(
                                                "https://github.com/KhronosGroup/Vulkan-Docs.git",
                                                "xml/vk.xml",
                                                "cee0f4b12acde766e64d0d038b03458c74bb67f1",
                                                "eb31286278b1ecf55ae817198a4238f82ea8fe028aa0631e2c1b09747f10ebb4") }
VK_INL_FILE = { ''        : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkan", "vkApiExtensionDependencyInfo.inl"),
                                    'SC'    : os.path.join(os.path.dirname(__file__), "..", "framework", "vulkan", "generated", "vulkansc", "vkApiExtensionDependencyInfo.inl") }
VK_INL_HEADER = { ''        : khr_util.format.genInlHeader("Khronos Vulkan API description (vk.xml)", VK_SOURCE[''].getRevision()),
                                    'SC'    : khr_util.format.genInlHeader("Khronos Vulkan API description (vk.xml)", VK_SOURCE[''].getRevision()) }

def VK_MAKE_API_VERSION(api, major, minor, patch):
    return ( ((api) << 29) | ((major) << 22) | ((minor) << 12) | (patch))

VK_EXT_NOT_PROMOTED = 0xFFFFFFFF
VK_EXT_TYPE_INSTANCE = 0
VK_EXT_TYPE_DEVICE = 1
VK_EXT_DEP_INSTANCE = 'instanceExtensionDependencies'
VK_EXT_DEP_DEVICE = 'deviceExtensionDependencies'
VK_EXT_API_VERSIONS = 'releasedApiVersions'
VK_EXT_CORE_VERSIONS = 'extensionRequiredCoreVersion'
VK_XML_EXT_DEPS = 'requires'
VK_XML_EXT_NAME = 'name'
VK_XML_EXT_PROMO = 'promotedto'
VK_XML_EXT_REQUIRES_CORE = 'requiresCore'
VK_XML_EXT_SUPPORTED = 'supported'
VK_XML_EXT_SUPPORTED_VULKAN = 'vulkan'
VK_XML_EXT_API = 'api'
VK_XML_EXT_TYPE = 'type'
VK_XML_EXT_TYPE_DEVICE = 'device'
VK_XML_EXT_TYPE_INSTANCE = 'instance'

apiVariantNames = {    'VK' : 0,
                                    'VKSC' : 1 }

def writeInlFile(api, filename, lines):
    khr_util.format.writeInlFile(filename, VK_INL_HEADER[api], lines)

def genExtDepArray(extDepsName, extDepsDict):
    yield 'static const std::tuple<uint32_t, uint32_t, uint32_t, const char*, const char*>\t{}[]\t='.format(extDepsName)
    yield '{'
    for ( apiName, major, minor, ext, extDeps ) in extDepsDict:
        for dep in extDeps:
            yield '\tstd::make_tuple({}, {}, {}, "{}", "{}"),'.format(apiVariantNames[apiName], major, minor, ext, dep)
    yield '};'

def genApiVersions(name, apiVersions):
    yield 'static const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>\t{}[]\t='.format(name)
    yield '{'
    for ( version, apiName, major, minor ) in apiVersions:
        yield '\tstd::make_tuple({}, {}, {}, {}),'.format(version, apiVariantNames[apiName], major, minor)
    yield '};'

def genRequiredCoreVersions(name, coreVersionsDict):
    yield 'static const std::tuple<uint32_t, uint32_t, const char*>\t{}[]\t ='.format(name)
    yield '{'
    extNames = sorted(coreVersionsDict.keys())
    for extName in extNames:
        (major, minor) = coreVersionsDict[extName]
        yield '\tstd::make_tuple({}, {}, "{}"),'.format(major, minor, extName)
    yield '};'

def genExtDepInl(api, dependenciesAndVersions):
    allExtDepsDict, apiVersions, allExtCoreVersions = dependenciesAndVersions
    apiVersions.reverse()
    lines = []

    lines.extend(genExtDepArray(VK_EXT_DEP_INSTANCE, allExtDepsDict[VK_EXT_TYPE_INSTANCE]))
    lines.extend(genExtDepArray(VK_EXT_DEP_DEVICE, allExtDepsDict[VK_EXT_TYPE_DEVICE]))
    lines.extend(genApiVersions(VK_EXT_API_VERSIONS, apiVersions))
    lines.extend(genRequiredCoreVersions(VK_EXT_CORE_VERSIONS, allExtCoreVersions))

    writeInlFile(api, VK_INL_FILE[api], lines)

class extInfo:
    def __init__(self):
        self.type = VK_EXT_TYPE_INSTANCE
        self.core = VK_MAKE_API_VERSION(0, 1, 0, 0)
        self.coreMajorMinor = (1, 0)
        self.promo = VK_EXT_NOT_PROMOTED
        self.deps = []

def genExtDepsOnApiVersion(ext, extInfoDict, apiVersion):
    deps = []

    for dep in extInfoDict[ext].deps:
        if apiVersion >= extInfoDict[dep].promo:
            continue

        deps.append(dep)

    return deps

def genExtDeps(extensionsAndVersions):
    extInfoDict, apiVersionID = extensionsAndVersions

    allExtDepsDict = defaultdict(list)
    apiVersions = []

    for (apiName,major,minor) in apiVersionID:
        apiVersions.append((VK_MAKE_API_VERSION(apiVariantNames[apiName], major, minor, 0),apiName,major,minor))
    apiVersions.sort(key=lambda x: x[0])

    for ext, info in extInfoDict.items():
        if info.deps == None:
            continue

        for (version,apiName,major,minor) in apiVersions:
            if info.core <= version:
                extDeps = genExtDepsOnApiVersion(ext, extInfoDict, version)
                if extDeps == None:
                    continue
                allExtDepsDict[info.type].append( ( apiName, major, minor, ext, extDeps ) )

    for key, value in allExtDepsDict.items():
        value.sort(key=lambda x: x[3])

    allExtCoreVersions = {}
    for (ext, info) in extInfoDict.items():
        allExtCoreVersions[ext] = info.coreMajorMinor

    return allExtDepsDict, apiVersions, allExtCoreVersions


def getExtInfoDict(vkRegistry):
    extInfoDict = {}
    apiVersionID = []

    for feature in vkRegistry.features:
        if feature.attrib[VK_XML_EXT_API] != VK_XML_EXT_SUPPORTED_VULKAN:
            continue
        featureName = feature.attrib[VK_XML_EXT_NAME].split('_')
        if len(featureName)!=4 or featureName[0] not in apiVariantNames or featureName[1] != 'VERSION' :
            continue
        apiVersionID.append( ( featureName[0], int(featureName[2]), int(featureName[3])) )

    apiVersionsByName = {}
    apiVersionsByNumber = {}
    apiMajorMinorByNumber = {}
    for (apiName, major,minor) in apiVersionID:
        majorDotMinor = '{}.{}'.format(major,minor)
        apiVersionsByName['{}_VERSION_{}_{}'.format(apiName,major,minor)] = VK_MAKE_API_VERSION(apiVariantNames[apiName], major, minor, 0);
        apiVersionsByNumber[majorDotMinor] = VK_MAKE_API_VERSION(apiVariantNames[apiName], major, minor, 0);
        apiMajorMinorByNumber[majorDotMinor] = (major, minor)

    for ext in vkRegistry.extensions:
        if ext.attrib[VK_XML_EXT_SUPPORTED] != VK_XML_EXT_SUPPORTED_VULKAN:
            continue

        name = ext.attrib[VK_XML_EXT_NAME]
        extInfoDict[name] = extInfo()
        if ext.attrib[VK_XML_EXT_TYPE] == VK_XML_EXT_TYPE_DEVICE:
            extInfoDict[name].type = VK_EXT_TYPE_DEVICE
        if VK_XML_EXT_REQUIRES_CORE in ext.attrib and ext.attrib[VK_XML_EXT_REQUIRES_CORE] in apiVersionsByNumber:
            extInfoDict[name].core = apiVersionsByNumber[ext.attrib[VK_XML_EXT_REQUIRES_CORE]]
            extInfoDict[name].coreMajorMinor = apiMajorMinorByNumber[ext.attrib[VK_XML_EXT_REQUIRES_CORE]]
        if VK_XML_EXT_PROMO in ext.attrib and ext.attrib[VK_XML_EXT_PROMO] in apiVersionsByName :
            extInfoDict[name].promo = apiVersionsByName[ext.attrib[VK_XML_EXT_PROMO]]
        if VK_XML_EXT_DEPS in ext.attrib:
            extInfoDict[name].deps = ext.attrib[VK_XML_EXT_DEPS].split(',')

    return extInfoDict, apiVersionID

def getVKRegistry(api):
    return khr_util.registry_cache.getRegistry(VK_SOURCE[api])

def parseCmdLineArgs():
    parser = argparse.ArgumentParser(description = "Generate Vulkan INL files",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-a",
                        "--api",
                        dest="api",
                        default="",
                        help="Choose between Vulkan and Vulkan SC")
    return parser.parse_args()

if __name__ == '__main__':
    args = parseCmdLineArgs()
    genExtDepInl(args.api, genExtDeps(getExtInfoDict(getVKRegistry(args.api))))
